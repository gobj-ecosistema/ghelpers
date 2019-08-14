##############################################
#   CMake
##############################################
project(ghelpers C)
cmake_minimum_required(VERSION 2.8)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_INSTALL_PREFIX /yuneta/development/output)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
  option(SHOWNOTES "Show preprocessor notes" OFF)

  if(CMAKE_COMPILER_IS_GNUCC)
    # GCC specific debug options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3 -ggdb3 -gdwarf-2")
    set(AVOID_VERSION -avoid-version)
  endif(CMAKE_COMPILER_IS_GNUCC)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)
add_definitions(-DHTTP_PARSER_STRICT=0)


IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
  add_definitions(-DNOT_INCLUDE_LIBUNWIND=1)
ELSE(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

include_directories(/yuneta/development/output/include)

##############################################
#   Source
##############################################

set (SRCS
    src/00_base64.c
    src/00_cpu.c
    src/00_http_parser.c
    src/00_replace_string.c
    src/00_sha1.c
    src/00_utf8.c
    src/00_utf8proc.c
    src/01_cnv_hex.c
    src/01_gstrings.c
    src/01_print_error.c
    src/01_reference.c
    src/02_dirs.c
    src/02_dl_list.c
    src/02_json_buffer.c
    src/02_stdout.c
    src/02_time_helper.c
    src/02_urls.c
    src/03_json_config.c
    src/03_output_udp_client.c
    src/03_rotatory.c
    src/10_glogger.c
    src/11_gbmem.c
#     src/11_gbmem2.c
    src/11_growbuffer.c
    src/11_ip_port.c
    src/11_time_helper2.c
    src/11_run_command.c
    src/11_ydaemon.c
    src/12_circular_fifo.c
    src/12_gstrings2.c
    src/12_rc_list.c
    src/12_urls2.c
    src/12_walkdir.c
    src/13_json_helper.c
    src/14_kw.c
    src/20_gbuffer.c
    src/20_stats.c
    src/20_stats_rd.c
    src/20_stats_wr.c
    src/21_json_xml.c
    src/21_termbox.c
    src/30_timeranger.c
    src/31_tr_db.c
    src/31_tr_msg.c
    src/31_tr_queue.c
    src/ghelpers.c
)

set (HDRS
    src/00_ansi_escape_codes.h
    src/00_asn1_snmp.h
    src/00_base64.h
    src/00_cpu.h
    src/00_gtypes.h
    src/00_http_parser.h
    src/00_replace_string.h
    src/00_security.h
    src/00_sha1.h
    src/00_utf8.h
    src/00_utf8proc.h
    src/01_cnv_hex.h
    src/01_gstrings.h
    src/01_print_error.h
    src/01_reference.h
    src/02_dirs.h
    src/02_dl_list.h
    src/02_json_buffer.h
    src/03_json_config.h
    src/12_rc_list.h
    src/02_stdout.h
    src/02_time_helper.h
    src/02_urls.h
    src/03_output_udp_client.h
    src/03_rotatory.h
    src/10_glogger.h
    src/11_gbmem.h
#     src/11_gbmem2.h
    src/11_growbuffer.h
    src/11_ip_port.h
    src/11_time_helper2.h
    src/11_run_command.h
    src/11_ydaemon.h
    src/12_circular_fifo.h
    src/12_gstrings2.h
    src/12_urls2.h
    src/12_walkdir.h
    src/13_json_helper.h
    src/14_kw.h
    src/20_gbuffer.h
    src/20_stats.h
    src/20_stats_rd.h
    src/20_stats_wr.h
    src/21_json_xml.h
    src/21_termbox.h
    src/30_timeranger.h
    src/31_tr_db.h
    src/31_tr_msg.h
    src/31_tr_queue.h

    src/ghelpers.h
)

##############################################
#   Prod Core lib
##############################################
add_library(ghelpers ${SRCS} ${HDRS})

##############################################
#   Debug Core lib
##############################################
add_library(ghelpersd ${SRCS} ${HDRS})
set_target_properties(ghelpersd
    PROPERTIES COMPILE_DEFINITIONS "__FULL_TRACE__=1"
)

##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})

install(
    TARGETS ghelpers ghelpersd
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)
