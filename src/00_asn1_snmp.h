/****************************************************************************
 *          00_ASN1_SNMP.H
 *
 *  Definitions for Abstract Syntax Notation One, ASN.1
 *  As defined in ISO/IS 8824 and ISO/IS 8825
 *
 *          Copyright (c) 2014-2016 Niyamaka.
 *          All Rights Reserved.
 *
 *  Code inspired and partially copied from asn1 net-nsmp:
 *
 *      Copyright 1988, 1989, 1991, 1992 by Carnegie Mellon University
 *      All Rights Reserved
 *
 *  and
 *      mini-snmpd
 *      Copyright (C) 2008 Robert Ernst <robert.ernst@linux-solutions.at>
 ****************************************************************************/
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern  "C" {
#endif

typedef uint32_t oid_t;

#define OID_LENGTH(x)  (sizeof(x)/sizeof(oid_t))

#define MIN_OID_LEN             2
#define MAX_OID_LEN             128 /* max subid's in an oid */
#define SNMP_MAX_MSG_SIZE       1472 /* ethernet MTU minus IP/UDP header */

#define ASN_COMPATIBLE_TYPES(type1, type2)                          \
    ( (ASN_IS_STRING(type1) && ASN_IS_STRING(type2)) ||             \
      (ASN_IS_NUMBER32(type1) && ASN_IS_BOOLEAN(type2)) ||          \
      (ASN_IS_NUMBER32(type1) && ASN_IS_NUMBER32(type2)) ||         \
      (ASN_IS_NUMBER64(type1) && ASN_IS_NUMBER64(type2)) ||         \
      (ASN_IS_DOUBLE(type1) && ASN_IS_DOUBLE(type2))                \
    )


#define ASN_IS_STRING(type)         \
    ((type) == ASN_OCTET_STR ||     \
    (type) == ASN_IPADDRESS ||      \
    (type) == ASN_BIT_STR ||        \
    (type) == ASN_OPAQUE)

#define ASN_IS_BOOLEAN(type)        \
    ((type) == ASN_BOOLEAN)

#define ASN_IS_NUMBER32(type)       \
    ((type) == ASN_INTEGER ||       \
    (type) == ASN_BOOLEAN  ||       \
    (type) == ASN_COUNTER  ||       \
    (type) == ASN_TIMETICKS ||      \
    (type) == ASN_UNSIGNED)

#define ASN_IS_UNSIGNED32(type)     \
    ((type) == ASN_COUNTER  ||      \
    (type) == ASN_TIMETICKS ||      \
    (type) == ASN_UNSIGNED)

#define ASN_IS_SIGNED32(type)       \
    ((type) == ASN_INTEGER ||       \
    (type) == ASN_BOOLEAN)

#define ASN_IS_NUMBER64(type)       \
    ((type) == ASN_INTEGER64 ||     \
    (type) == ASN_COUNTER64  ||     \
    (type) == ASN_UNSIGNED64)

#define ASN_IS_UNSIGNED64(type)     \
    ((type) == ASN_COUNTER64  ||    \
    (type) == ASN_UNSIGNED64)

#define ASN_IS_SIGNED64(type)       \
    ((type) == ASN_INTEGER64)

#define ASN_IS_DOUBLE(type)         \
    ((type) == ASN_FLOAT ||         \
    (type) == ASN_DOUBLE)

#define ASN_IS_NUMBER(type)         \
    (ASN_IS_NUMBER32(type) ||       \
     ASN_IS_DOUBLE(type) ||         \
     ASN_IS_NUMBER64(type))

#define ASN_IS_REAL_NUMBER(type)    \
    ((type) == ASN_FLOAT ||         \
    (type) == ASN_DOUBLE)

#define ASN_IS_NATURAL_NUMBER(type) \
    (ASN_IS_NUMBER32(type) ||       \
     ASN_IS_NUMBER64(type))

#define ASN_IS_JSON(type)           \
    ((type) == ASN_JSON)

#define ASN_IS_SCHEMA(type)         \
    ((type) == ASN_SCHEMA)

#define ASN_IS_POINTER(type)        \
    ((type) == ASN_POINTER)

#define ASN_IS_DL_LIST(type)        \
    ((type) == ASN_DL_LIST)

#define ASN_IS_ITER(type)           \
    ((type) == ASN_ITER)


/*********************************************************************
 *      ASN.1 Constants
 *********************************************************************/
#define ASN_BOOLEAN             ((uint8_t)0x01)
#define ASN_INTEGER             ((uint8_t)0x02)
#define ASN_BIT_STR             ((uint8_t)0x03)
#define ASN_OCTET_STR           ((uint8_t)0x04)
#define ASN_NULL                ((uint8_t)0x05)
#define ASN_OBJECT_ID           ((uint8_t)0x06)
#define ASN_SEQUENCE            ((uint8_t)0x10)
#define ASN_SET                 ((uint8_t)0x11)

#define ASN_UNIVERSAL           ((uint8_t)0x00)
#define ASN_APPLICATION         ((uint8_t)0x40)
#define ASN_CONTEXT             ((uint8_t)0x80)
#define ASN_PRIVATE             ((uint8_t)0xC0)

#define ASN_PRIMITIVE           ((uint8_t)0x00)
#define ASN_CONSTRUCTOR         ((uint8_t)0x20)

#define ASN_LONG_LEN            (0x80)
#define ASN_EXTENSION_ID        (0x1F)
#define ASN_BIT8                (0x80)

#define IS_CONSTRUCTOR(byte)    ((byte) & ASN_CONSTRUCTOR)
#define IS_EXTENSION_ID(byte)   (((byte) & ASN_EXTENSION_ID) == ASN_EXTENSION_ID)

/*********************************************************************
 *      SNMP Constants
 *********************************************************************/

#define SNMP_VERSION_1     0
#define SNMP_VERSION_2c    1
#define SNMP_VERSION_2u    2    /* not (will never be) supported by this code */
#define SNMP_VERSION_3     3

/*
 * PDU types in SNMPv1, SNMPsec, SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3
 */
#define SNMP_MSG_GET        (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x0) /* a0=160 */
#define SNMP_MSG_GETNEXT    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x1) /* a1=161 */
#define SNMP_MSG_RESPONSE   (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x2) /* a2=162 */
#define SNMP_MSG_SET        (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x3) /* a3=163 */

/*
 * PDU types in SNMPv1 and SNMPsec
 */
#define SNMP_MSG_TRAP       (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x4) /* a4=164 */

/*
 * PDU types in SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3
 */
#define SNMP_MSG_GETBULK    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x5) /* a5=165 */
#define SNMP_MSG_INFORM     (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x6) /* a6=166 */
#define SNMP_MSG_TRAP2      (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x7) /* a7=167 */

/*
 * PDU types in SNMPv2u, SNMPv2*, and SNMPv3
 */
#define SNMP_MSG_REPORT     (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x8) /* a8=168 */

/*
 * Exception values for SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3
 */
#define SNMP_NOSUCHOBJECT    (ASN_CONTEXT | ASN_PRIMITIVE | 0x0) /* 80=128 */
#define SNMP_NOSUCHINSTANCE  (ASN_CONTEXT | ASN_PRIMITIVE | 0x1) /* 81=129 */
#define SNMP_ENDOFMIBVIEW    (ASN_CONTEXT | ASN_PRIMITIVE | 0x2) /* 82=130 */

/*
 * Error codes (the value of the field error-status in PDUs)
 */

/*
 * in SNMPv1, SNMPsec, SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3 PDUs
 */
#define SNMP_ERR_NOERROR                (0)     /* XXX  Used only for PDUs? */
#define SNMP_ERR_TOOBIG                 (1)
#define SNMP_ERR_NOSUCHNAME             (2)
#define SNMP_ERR_BADVALUE               (3)
#define SNMP_ERR_READONLY               (4)
#define SNMP_ERR_GENERR                 (5)

/*
 * in SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3 PDUs
 */
#define SNMP_ERR_NOACCESS               (6)
#define SNMP_ERR_WRONGTYPE              (7)
#define SNMP_ERR_WRONGLENGTH            (8)
#define SNMP_ERR_WRONGENCODING          (9)
#define SNMP_ERR_WRONGVALUE             (10)
#define SNMP_ERR_NOCREATION             (11)
#define SNMP_ERR_INCONSISTENTVALUE      (12)
#define SNMP_ERR_RESOURCEUNAVAILABLE    (13)
#define SNMP_ERR_COMMITFAILED           (14)
#define SNMP_ERR_UNDOFAILED             (15)
#define SNMP_ERR_AUTHORIZATIONERROR     (16)
#define SNMP_ERR_NOTWRISCHEMA            (17)

/*
 * in SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3 PDUs
 */
#define SNMP_ERR_INCONSISTENTNAME       (18)

#define MAX_SNMP_ERR    18

#define SNMP_VALIDATE_ERR(x)  ( (x > MAX_SNMP_ERR) ? \
                                   SNMP_ERR_GENERR : \
                                   (x < SNMP_ERR_NOERROR) ? \
                                      SNMP_ERR_GENERR : \
                                      x )

/*
 * values of the generic-trap field in trap PDUs
 */
#define SNMP_TRAP_COLDSTART             (0)
#define SNMP_TRAP_WARMSTART             (1)
#define SNMP_TRAP_LINKDOWN              (2)
#define SNMP_TRAP_LINKUP                (3)
#define SNMP_TRAP_AUTHFAIL              (4)
#define SNMP_TRAP_EGPNEIGHBORLOSS       (5)
#define SNMP_TRAP_ENTERPRISESPECIFIC    (6)

/*
 * defined types (from the SMI, RFC 1157)
 */
#define ASN_IPADDRESS   (ASN_APPLICATION | 0)
#define ASN_COUNTER     (ASN_APPLICATION | 1)
#define ASN_GAUGE       (ASN_APPLICATION | 2)
#define ASN_UNSIGNED    (ASN_APPLICATION | 2)   /* RFC 1902 - same as GAUGE */
#define ASN_TIMETICKS   (ASN_APPLICATION | 3)
#define ASN_OPAQUE      (ASN_APPLICATION | 4)   /* changed so no conflict with other includes */

/*
 * defined types (from the SMI, RFC 1442)
 */
#define ASN_COUNTER64   (ASN_APPLICATION | 6)

/*
 * defined types from draft-perkins-opaque-01.txt
 */
#define ASN_FLOAT       (ASN_APPLICATION | 8)
#define ASN_DOUBLE      (ASN_APPLICATION | 9)
#define ASN_INTEGER64   (ASN_APPLICATION | 10)
#define ASN_UNSIGNED64  (ASN_APPLICATION | 11)

/*
 *  The basic types are managed by Yuneta.
 *  You don't warry about free them (strings and numbers).
 *  In private types it's different.
 *  In some private cases you are the unique responsable for free them.
 *  My private types
 */
#define ASN_JSON        (ASN_PRIVATE | 1)       // Json, you know. Yuneta will free it.
#define ASN_SCHEMA      (ASN_PRIVATE | 2)       // sdata_desc_t for command parameters. Used only static data.
#define ASN_POINTER     (ASN_PRIVATE | 3)       // It's your responsability manage this.
#define ASN_DL_LIST     (ASN_PRIVATE | 4)       // Supply a free function and Yuneta will free it.
#define ASN_ITER        (ASN_PRIVATE | 5)       // Supply a free function and Yuneta will free it.


#ifdef __cplusplus
}
#endif
