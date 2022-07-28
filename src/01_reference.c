/****************************************************************************
 *          REFERENCE.C
 *          Get references
 *
 *          Copyright (c) 2014 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifdef _WIN32
    #include <stdio.h>
    #include <string.h>
    #include <objbase.h>
#else
    #include <uuid/uuid.h>
#endif
#include "01_reference.h"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE uint32_t __short_reference__ = 0;
PRIVATE uint64_t __long_reference__ = 0;


/***************************************************************************
 *      Short age reference
 ***************************************************************************/
uint32_t short_reference(void)
{
    return ++__short_reference__;
}

/***************************************************************************
 *      long age reference
 ***************************************************************************/
uint64_t long_reference(void)
{
    return ++__long_reference__;
}

/***************************************************************************
 *
 ***************************************************************************/
#ifdef _WIN32
int create_uuid(char *bf, int bfsize)
{
    GUID out = {0};
    CoCreateGuid(&out);
    snprintf(bf, bfsize,
         "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
         out.Data1,
         out.Data2,
         out.Data3,
         out.Data4[0],
         out.Data4[1],
         out.Data4[2],
         out.Data4[3],
         out.Data4[4],
         out.Data4[5],
         out.Data4[6],
         out.Data4[7]
     );
    for ( ; *bf; ++bf) *bf = tolower(*bf); // To lower one-liner
    return 0;
}
#else
/* This is the linux friendly implementation, but it could work on other
 * systems that have libuuid available
 */
int create_uuid(char *bf, int bfsize)
{
    if(bfsize < 37) {
        return -1;
    }
    uuid_t out;
    uuid_generate(out);
    uuid_unparse_lower(out, bf);
    return 0;
}
#endif
