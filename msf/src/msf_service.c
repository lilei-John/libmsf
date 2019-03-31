/**************************************************************************
*
* Copyright (c) 2017-2018, luotang.me <wypx520@gmail.com>, China.
* All rights reserved.
*
* Distributed under the terms of the GNU General Public License v2.
*
* This software is provided 'as is' with no explicit or implied warranties
* in respect of its properties, including, but not limited to, correctness
* and/or fitness for purpose.
*
**************************************************************************/
#include "msf_process.h"
#include <msf_log.h>

#define MSF_MOD_SVC "SVC"
#define MSF_SVC_LOG(level, ...) \
    log_write(level, MSF_MOD_SVC, __func__, __FILE__, __LINE__, __VA_ARGS__)

s32 svcinst_init(struct svcinst *svc) {

    s32 rc;
    struct file_info f_info;

    rc = file_get_info(svc->svc_lib, &f_info, MSF_FILE_READ);
    if (rc == -1 || f_info.is_directory == 0) {
    }

    svc->svc_handle = plugin_load_dynamic(svc->svc_lib);
    if (!svc->svc_handle) {
        MSF_SVC_LOG(DBG_ERROR, "plugin_load lib: '%s' failed\n", svc->svc_lib);
        return -1;
    }

    svc->svc_cb = plugin_load_symbol(svc->svc_handle, svc->svc_name);
    if (!svc->svc_cb) {
        MSF_SVC_LOG(DBG_ERROR, "plugin_load symbol: '%s' failed\n", svc->svc_name);
        MSF_DLCLOSE(svc->svc_handle);
        return -1;
    } else {
        MSF_SVC_LOG(DBG_ERROR, "plugin_load: '%s' successful\n", svc->svc_lib);
    }

    return 0;
}

