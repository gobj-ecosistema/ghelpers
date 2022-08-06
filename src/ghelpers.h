/****************************************************************************
 *              GHELPERS.H
 *              Copyright (c) 1996-2022 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "00_ansi_escape_codes.h"
#include "00_asn1_snmp.h"
#include "00_base64.h"
#include "00_cpu.h"
#include "00_gtypes.h"
#include "00_http_parser.h"
#include "00_replace_string.h"
#include "00_sha1.h"
#include "00_utf8.h"
#include "00_utf8proc.h"
#include "01_cnv_hex.h"
#include "01_gstrings.h"
#include "01_print_error.h"
#include "01_reference.h"
#include "02_dirs.h"
#include "02_dl_list.h"
#include "02_json_buffer.h"
#include "02_stdout.h"
#include "02_time_helper.h"
#include "02_urls.h"
#include "03_rotatory.h"
#include "03_json_config.h"
#include "03_output_udp_client.h"
#include "10_glogger.h"
#include "11_gbmem.h"
#include "11_growbuffer.h"
#include "11_ip_port.h"
#include "11_time_helper2.h"
#include "11_run_command.h"
#include "11_ydaemon.h"
#include "12_circular_fifo.h"
#include "12_gstrings2.h"
#include "12_rc_list.h"
#include "12_urls2.h"
#include "12_walkdir.h"
#include "13_json_helper.h"
#include "14_kw.h"
#include "15_webix_helper.h"
#include "20_gbuffer.h"
#include "20_stats.h"
#include "21_json_xml.h"
#include "21_termbox.h"
#include "30_timeranger.h"
#include "31_tr_msg.h"
#include "31_tr_msg2db.h"
#include "31_tr_queue.h"
#include "31_tr_treedb.h"

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Prototypes
 *********************************************************************/
PUBLIC int init_ghelpers_library(const char *process_name);
PUBLIC void end_ghelpers_library(void);

/*********************************************************************
 *      Version
 *********************************************************************/
#define __ghelpers_version__  "5.11.2"  /* XX__yuneta_version__XX */


#ifdef __cplusplus
}
#endif
