// apt-get source libmount-dev
#include <base/Logger.h>
#include <base/Sysfs.h>
#include <dirent.h>
#include <errno.h>
#include <libmount/libmount.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

using namespace MSF::BASE;

static struct libmnt_table *mtab, *swaps;
static struct libmnt_cache *mntcache;

static void libmount_cleanup(void) {
  mnt_free_table(mtab);
  mnt_free_table(swaps);
  mnt_free_cache(mntcache);
  mtab = NULL;
  swaps = NULL;
  mntcache = NULL;
}

static int libmount_init(void) {
  mnt_init_debug(0);
  mtab = mnt_new_table();
  swaps = mnt_new_table();
  mntcache = mnt_new_cache();
  if (!mtab || !swaps || !mntcache) {
    libmount_cleanup();
    return -ENOMEM;
  }
  mnt_table_set_cache(mtab, mntcache);
  mnt_table_set_cache(swaps, mntcache);
  mnt_table_parse_mtab(mtab, NULL);
  mnt_table_parse_swaps(swaps, NULL);
  return 0;
}

static int trans_filter(const struct dirent *d) {
  if (!strcmp(".", d->d_name) || !strcmp("..", d->d_name)) return 0;
  return 1;
}

static int subdir_filter(const struct dirent *d) {
  if (!(d->d_type & DT_DIR)) return 0;
  return trans_filter(d);
}

static int is_partition(const char *path) {
  char *devtype;
  int rc = 0;

  devtype = sysfs_get_uevent_devtype(path);
  if (!devtype) return 0;
  if (strcmp(devtype, "partition") == 0) rc = 1;
  free(devtype);
  return rc;
}

static int blockdev_check_mnts(char *syspath) {
  struct libmnt_fs *fs;
  char *devname = NULL;
  char *_devname = NULL;
  int rc = 0;

  devname = sysfs_get_uevent_devname(syspath);
  if (!devname) goto out;

  _devname = (char *)calloc(1, PATH_MAX);
  if (!_devname) goto out;
  snprintf(_devname, PATH_MAX, "/dev/%s", devname);

  fs = mnt_table_find_source(mtab, _devname, MNT_ITER_FORWARD);
  if (fs) {
    rc = 1;
    goto out;
  }
  fs = mnt_table_find_source(swaps, _devname, MNT_ITER_FORWARD);
  if (fs) rc = 1;
out:
  free(devname);
  free(_devname);
  return rc;
}

static int count_device_users(char *syspath);

static int blockdev_get_partitions(char *syspath) {
  struct dirent **parts = NULL;
  int n, i;
  int count = 0;

  n = scandir(syspath, &parts, subdir_filter, alphasort);
  for (i = 0; i < n; i++) {
    char *newpath;

    newpath = (char *)calloc(1, PATH_MAX);
    if (!newpath) continue;
    snprintf(newpath, PATH_MAX, "%s/%s", syspath, parts[i]->d_name);
    free(parts[i]);
    if (is_partition(newpath)) {
      count += count_device_users(newpath);
    }
    free(newpath);
  }
  free(parts);
  return count;
}

static int blockdev_get_holders(char *syspath) {
  char *path = NULL;
  struct dirent **holds = NULL;
  int n, i;
  int count = 0;

  path = (char *)calloc(1, PATH_MAX);
  if (!path) return 0;
  snprintf(path, PATH_MAX, "%s/holders", syspath);

  n = scandir(path, &holds, trans_filter, alphasort);
  for (i = 0; i < n; i++) {
    char *newpath;
    char *rp;

    newpath = (char *)calloc(1, PATH_MAX);
    if (!newpath) continue;
    snprintf(newpath, PATH_MAX, "%s/%s", path, holds[i]->d_name);

    free(holds[i]);
    rp = realpath(newpath, NULL);
    if (rp) count += count_device_users(rp);
    free(newpath);
    free(rp);
  }
  free(path);
  free(holds);
  return count;
}

static int count_device_users(char *syspath) {
  int count = 0;
  count += blockdev_check_mnts(syspath);
  count += blockdev_get_partitions(syspath);
  count += blockdev_get_holders(syspath);
  return count;
};

void device_in_use(void *data, int host_no, int target, int lun) {
  char *syspath = NULL;
  char *devname = NULL;
  int *count = (int *)data;

  // devname = iscsi_sysfs_ge_blockdev_from_lun(host_no, target, lun);
  if (!devname) goto out;
  syspath = (char *)calloc(1, PATH_MAX);
  if (!syspath) goto out;
  snprintf(syspath, PATH_MAX, "/sys/class/block/%s", devname);
  *count += count_device_users(syspath);
out:
  free(syspath);
  free(devname);
}

int session_in_use(int sid) {
  // int host_no = -1, err = 0;
  int count = 0;

  if (libmount_init()) {
    MSF_ERROR << "Failed to initialize libmount, "
                 "not checking for active mounts on session "
              << sid;
    return 0;
  }

  // host_no = iscsi_sysfs_get_host_no_from_sid(sid, &err);
  // if (!err)
  // 	iscsi_sysfs_for_each_device(&count, host_no, sid, device_in_use);

  libmount_cleanup();
  return count;
}
