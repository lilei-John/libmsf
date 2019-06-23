
#include <msf_os.h>
#include <msf_cpu.h>
#include <msf_network.h>

#define MSF_MOD_OS "OS"
#define MSF_OS_LOG(level, ...) \
    msf_log_write(level, MSF_MOD_OS, MSF_FUNC_FILE_LINE, __VA_ARGS__)

/*
 * 如果能知道CPU cache行的大小,那么就可以有针对性地设置内存的对齐值,
 * 这样可以提高程序的效率.有分配内存池的接口, Nginx会将内存池边界
 * 对齐到 CPU cache行大小32位平台, cacheline_size=32 */
u32 msf_cacheline_size;

struct msf_os os;
struct msf_os *g_os = &os;

s32 msf_set_user(struct process *proc) {

    s8               *group = NULL;
    struct passwd    *pwd = NULL;
    struct group     *grp = NULL;

     if (proc->user != (uid_t) MSF_CONF_UNSET_UINT) {
         MSF_OS_LOG(DBG_DEBUG, "User is duplicate.");
         return 0;
    }

    if (geteuid() != 0) {
        MSF_OS_LOG(DBG_DEBUG,
           "The \"user\" directive makes sense only "
           "if the master process runs "
           "with super-user privileges, ignored.");
        return 0;
    }

    pwd = getpwnam((const s8 *) proc->username);
    if (pwd == NULL) {
        return -1;
    }

   	proc->user = pwd->pw_uid;

    grp = getgrnam(group);
    if (grp == NULL) {      
        return -1;
    }

    proc->group = grp->gr_gid;

    return 0;

}

s32 msf_set_rlimit(struct process *proc) {

    struct rlimit     rlmt;

    if (proc->rlimit_nofile != -1) {
        rlmt.rlim_cur = (rlim_t) proc->rlimit_nofile;
        rlmt.rlim_max = (rlim_t) proc->rlimit_nofile;

        //RLIMIT_NOFILE指定此进程可打开的最大文件描述词大一的值,超出此值,将会产生EMFILE错误。
        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        }
    }

    if (proc->rlimit_core != -1) {
        rlmt.rlim_cur = (rlim_t) proc->rlimit_core;
        rlmt.rlim_max = (rlim_t) proc->rlimit_core;
       //修改工作进程的core文件尺寸的最大值限制(RLIMIT_CORE),用于在不重启主进程的情况下增大该限制
        if (setrlimit(RLIMIT_CORE, &rlmt) == -1) {
        }

    }


    if (geteuid() == 0) {
        if (setgid(proc->group) == -1) {
            /* fatal */
            exit(2);
        }

        if (initgroups(proc->username, proc->group) == -1) {
        }

        if (setuid(proc->user) == -1) {
            /* fatal */
            exit(2);
        }
    }


    process_pin_to_cpu(proc->cpu_affinity);

    /* allow coredump after setuid() in Linux 2.4.x */
    msf_enable_coredump();

    return 0;
}


/**
 * RAM
 */
s32 msf_get_meminfo(struct msf_meminfo *mem) {

    FILE *fp = NULL;
    s8 buff[256];

    fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return -1;
    }

    msf_memzero(buff, sizeof(buff));
    fgets(buff, sizeof(buff), fp); 
    sscanf(buff, "%s %le %s*", mem->name1, &mem->total, mem->name2); 

    msf_memzero(buff, sizeof(buff));
    fgets(buff, sizeof(buff), fp);
    sscanf(buff, "%s %le %s*", mem->name1, &mem->free, mem->name2); 

    mem->used_rate= (1 - mem->total/ mem->total) * 100;

    sfclose(fp);

    return 0;
}

s32 msf_get_hdinfo(struct msf_hdd *hd) {

    FILE *fp = NULL;
    s8 buffer[80],a[80],d[80],e[80],f[80], buf[256];
    double c,b;
    double dev_total = 0, dev_used = 0;

    fp = popen("df", "r");
    if (!fp) {
        return -1;
    }

    fgets(buf, sizeof(buf), fp);
    while (6 == fscanf(fp, "%s %lf %lf %s %s %s", a, &b, &c, d, e, f)) {
        dev_total += b;
        dev_used += c;
    }

    hd->total = dev_total / MB;
    hd->used_rate = dev_used/ dev_total * 100;

    pclose(fp);

    return 0;
}

s32 msf_system_init() {

    u32 n;

    struct rlimit   rlmt;
    struct utsname  u;

    if (uname(&u) == -1) {
        MSF_OS_LOG(DBG_ERROR, "Uname failed, errno(%d).", errno);
        return -1;
    }

    memcpy(g_os->sysname, (u8 *) u.sysname,
          min(sizeof(g_os->sysname), strlen(u.sysname)));
    memcpy(g_os->nodename, (u8 *) u.nodename,
          min(sizeof(g_os->nodename), strlen(u.nodename)));
    memcpy(g_os->release, (u8 *) u.release,
          min(sizeof(g_os->release), strlen(u.release)));
    memcpy(g_os->version, (u8 *) u.version,
          min(sizeof(g_os->version), strlen(u.version)));
    memcpy(g_os->machine, (u8 *) u.machine,
          min(sizeof(g_os->machine), strlen(u.machine)));
    memcpy(g_os->domainname, (u8 *) u.domainname,
          min(sizeof(g_os->domainname), strlen(u.domainname)));

#ifdef WIN32
    SYSTEM_INFO info; 
    GetSystemInfo(&info); 
    g_os->cpuonline = info.dwNumberOfProcessors;
#endif

    /* GNU fuction 
    * getpagesize(); numa_pagesize()
    * get_nprocs_conf();
    * get_nprocs();
    */
    g_os->pagesize = sysconf(_SC_PAGESIZE);
    g_os->pagenum_all = sysconf(_SC_PHYS_PAGES);
    g_os->pagenum_ava = sysconf(_SC_AVPHYS_PAGES);
    g_os->memsize = g_os->pagesize * g_os->pagenum_all / MB;
    g_os->cpuconf = sysconf(_SC_NPROCESSORS_CONF);
    g_os->cpuonline = sysconf(_SC_NPROCESSORS_ONLN);
    g_os->maxfileopen = sysconf(_SC_OPEN_MAX);
    g_os->tickspersec = sysconf(_SC_CLK_TCK);
    g_os->maxhostname = sysconf(_SC_HOST_NAME_MAX);
    g_os->maxloginname = sysconf(_SC_LOGIN_NAME_MAX);

    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        MSF_OS_LOG(DBG_ERROR, "Getrlimit failed, errno(%d).", errno);
        return -1;
    }

    g_os->maxsocket = (s32) rlmt.rlim_cur;

    return 0;
}

/* Numa Info*/
void msf_vnode_init(void) {

    s32 rc;

    g_os->en_numa = numa_available();
    if (g_os->en_numa < 0) {
        MSF_OS_LOG(DBG_ERROR, "Your system does not support NUMA API.");
        return;
    }

    g_os->numacnt = numa_max_node();

    MSF_OS_LOG(DBG_DEBUG, "System numa en(%d).", g_os->en_numa);
    MSF_OS_LOG(DBG_DEBUG, "System numa num(%d).", g_os->numacnt);

    s32 nd;
    char *man = numa_alloc(1000);
    *man = 1;
    if (get_mempolicy(&nd, NULL, 0, man, MPOL_F_NODE|MPOL_F_ADDR) < 0)
        perror("get_mempolicy");
    else
        MSF_OS_LOG(DBG_DEBUG, "my node %d.", nd);

    msf_numa_free(man, 1000);

}

void msf_os_debug(void) {
    
    MSF_OS_LOG(DBG_DEBUG, "OS type:%s.",        g_os->sysname);
    MSF_OS_LOG(DBG_DEBUG, "OS nodename:%s.",    g_os->nodename);
    MSF_OS_LOG(DBG_DEBUG, "OS release:%s.",     g_os->release);
    MSF_OS_LOG(DBG_DEBUG, "OS version:%s.",     g_os->version);
    MSF_OS_LOG(DBG_DEBUG, "OS machine:%s.",     g_os->machine);
    MSF_OS_LOG(DBG_DEBUG, "OS domainname:%s.",  g_os->domainname);
    MSF_OS_LOG(DBG_DEBUG, "Processors configured is :%ld.", g_os->cpuconf);
    MSF_OS_LOG(DBG_DEBUG, "Processors available is :%ld.", g_os->cpuonline);
    MSF_OS_LOG(DBG_DEBUG, "The cacheline size: %ld.", msf_cacheline_size);
    MSF_OS_LOG(DBG_DEBUG, "The pagesize: %ld.", g_os->pagesize);
    MSF_OS_LOG(DBG_DEBUG, "The pages all num: %ld", g_os->pagenum_all);
    MSF_OS_LOG(DBG_DEBUG, "The pages available: %ld.", g_os->pagenum_ava);
    MSF_OS_LOG(DBG_DEBUG, "The memory size: %lld MB.", g_os->memsize);
    MSF_OS_LOG(DBG_DEBUG, "The files max opened: %ld.", g_os->maxfileopen);
    MSF_OS_LOG(DBG_DEBUG, "The socket max opened: %ld.", g_os->maxsocket);
    MSF_OS_LOG(DBG_DEBUG, "The ticks per second: %ld.", g_os->tickspersec);
    MSF_OS_LOG(DBG_DEBUG, "The max len host name: %ld.", g_os->maxhostname);
    MSF_OS_LOG(DBG_DEBUG, "The max len login name: %ld.", g_os->maxloginname);

    MSF_OS_LOG(DBG_DEBUG, "The mem name1: %s.", g_os->meminfo.name1);
    MSF_OS_LOG(DBG_DEBUG, "The mem total: %ld.", g_os->meminfo.total);
    MSF_OS_LOG(DBG_DEBUG, "The mem name2: %s.", g_os->meminfo.name2);
    MSF_OS_LOG(DBG_DEBUG, "The mem free: %ld.", g_os->meminfo.free);
    MSF_OS_LOG(DBG_DEBUG, "The mem used rate: %ld.", g_os->meminfo.used_rate);

    MSF_OS_LOG(DBG_DEBUG, "The hdd total: %ld.", g_os->hdd.total);
    MSF_OS_LOG(DBG_DEBUG, "The hdd used_rate: %ld.", g_os->hdd.used_rate);
}

s32 msf_os_init(void) {

    msf_system_init();

    msf_vnode_init();

    msf_get_meminfo(&g_os->meminfo);

    msf_get_hdinfo(&g_os->hdd);

    msf_os_debug();

    msf_cpuinfo();

    return 0;
}
