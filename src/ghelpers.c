/****************************************************************************
 *              GHELPERS.C
 *              Copyright (c) 1996-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#ifdef WIN32
    #include <io.h>
    #include <process.h>
#else
    #include <unistd.h>
#endif
#include "ghelpers.h"

PRIVATE BOOL initialized = FALSE;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE char __process_name__[64] = {0};
PRIVATE char __hostname__[64] = {0};
PRIVATE char __username__[64] = {0};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int init_ghelpers_library(const char *process_name)
{
    if(initialized) {
        return -1;
    }

    if(process_name) {
        snprintf(__process_name__, sizeof(__process_name__), "%s", process_name);
    }
    gethostname(__hostname__, sizeof(__hostname__));

#ifdef WIN32
    long unsigned int bufsize = sizeof(__username__);
    GetUserNameA(__username__, &bufsize);
#else
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if(pw) {
        snprintf(__username__, sizeof(__username__), "%s", pw->pw_name);
    }
#endif

    int ret = 0;

    ret += kw_add_binary_type(
        "gbuffer",
        "__gbuffer___",
        (serialize_fn_t)gbuf_serialize,
        (deserialize_fn_t)gbuf_deserialize,
        (incref_fn_t)gbuf_incref,
        (decref_fn_t)gbuf_decref
    );

    ret += rotatory_start_up();
    ret += udpc_start_up(process_name);

    initialized = TRUE;

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int set_process_name2(const char *role, const char *name)
{
    if(role && name) {
        snprintf(__process_name__, sizeof(__process_name__), "%s^%s", role, name);
        log_set_process_name(__process_name__);
    }
    return 0;
}
PUBLIC const char *get_process_name(void)
{
    return __process_name__;
}
PUBLIC const char *get_host_name(void)
{
    return __hostname__;
}
PUBLIC const char *get_user_name(void)
{
    return __username__;
}
PUBLIC int get_pid(void)
{
    return getpid();
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void end_ghelpers_library(void)
{
    if(!initialized) {
        return;
    }

    kw_clean_binary_types();
    log_end();
    gbmem_shutdown();
    rotatory_end();

    initialized = FALSE;
}
