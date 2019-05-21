/****************************************************************************
 *              SECURITY.H
 *              Copyright (c) 2014-2015 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/

#ifndef _SECURITY_H
#define _SECURITY_H 1

#ifdef __cplusplus
extern "C"{
#endif

/*
    WARNING: If you include this security file then you MUST define
    a security level at compilation configuration.
    Example: gcc -DSECURITY_LEVEL_AMICABLE -c sample.c
    The philosophy is that security level must be defined statically
    and your project must supply several outputs depending of
    security levels you want.


    Security levels for files
    =========================

    __SECURITY_LAX_LEVEL__          User Group and Other have full rights.
    __SECURITY_AMICABLE_LEVEL__     User and Group full rights. Other partial.
    __SECURITY_TEAM_LEVEL__         User and Group full rights. Other nothing.
    __SECURITY_RESERVED_LEVEL__     User full rights. Group partial . Other nothing.
    __SECURITY_PARANOID_LEVEL__     User full rights. Group and Other nothing.


    Non executable files permissions (RFILE)
    ----------------------------------------
    __SECURITY_LAX_LEVEL__          user: rw-   group: rw-  other: rw-
    __SECURITY_AMICABLE_LEVEL__     user: rw-   group: rw-  other: r--
    __SECURITY_TEAM_LEVEL__         user: rw-   group: rw-  other: ---
    __SECURITY_RESERVED_LEVEL__     user: rw-   group: r-   other: ---
    __SECURITY_PARANOID_LEVEL__     user: rw-   group: ---  other: ---

    Executable files and Directories permissions (XFILE)
    ----------------------------------------------------
    __SECURITY_LAX_LEVEL__          user: rwx   group: rwx  other: rwx  SHARED
    __SECURITY_AMICABLE_LEVEL__     user: rwx   group: rwx  other: r-x  SHARED
    __SECURITY_TEAM_LEVEL__         user: rwx   group: rwx  other: ---  SHARED
    __SECURITY_RESERVED_LEVEL__     user: rwx   group: r-x  other: ---
    __SECURITY_PARANOID_LEVEL__     user: rwx   group: ---  other: ---

    NOTE:   - RFILE_PERMISSION is to create regular files.
            - XFILE_PERMISSION is to create directories and executable files.

*/

#if defined(__SECURITY_LAX_LEVEL__)
    #define RFILE_PERMISSION    0666
    #define XFILE_PERMISSION    02777
#elif defined(__SECURITY_AMICABLE_LEVEL__)
    #define RFILE_PERMISSION    0664
    #define XFILE_PERMISSION    02775
#elif defined(__SECURITY_TEAM_LEVEL__)
    #define RFILE_PERMISSION    0660
    #define XFILE_PERMISSION    02770
#elif defined(__SECURITY_RESERVED_LEVEL__)
    #define RFILE_PERMISSION    0640
    #define XFILE_PERMISSION    0750
#elif defined(__SECURITY_PARANOID_LEVEL__)
    #define RFILE_PERMISSION    0600
    #define XFILE_PERMISSION    0700
#else
    #error Security level NOT defined!
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SECURITY_H */
