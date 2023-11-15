##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(ghelpers C)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_INSTALL_PREFIX /yuneta/development/output)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)

set(CMAKE_C_STANDARD 99)

if(LINUX OR UNIX OR CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

if(WIN32)
  add_definitions(-DNOT_INCLUDE_LIBUNWIND)
endif()

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

IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
ELSE()
  add_definitions(-DNOT_INCLUDE_LIBUNWIND=1)
ENDIF()

if(WIN32)
  add_definitions(-DNOT_INCLUDE_LIBUNWIND)
endif()

IF(WIN32)
  include_directories(c:/yuneta/development/output/include)
  include_directories(c:/yuneta/development/output/include/regex)
  include_directories(c:/yuneta/development/output/OpenSSL-Win64/include)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
ELSEIF(UNIX)
  include_directories(/yuneta/development/output/include)
ENDIF()

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
    src/15_webix_helper.c
    src/20_gbuffer.c
    src/20_stats.c
    src/20_stats_rd.c
    src/20_stats_wr.c
    src/21_json_xml.c
    src/21_termbox.c
    src/30_timeranger.c
    src/31_tr_msg.c
    src/31_tr_msg2db.c
    src/31_tr_queue.c
    src/31_tr_treedb.c
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
    src/15_webix_helper.h
    src/20_gbuffer.h
    src/20_stats.h
    src/20_stats_rd.h
    src/20_stats_wr.h
    src/21_json_xml.h
    src/21_termbox.h
    src/30_timeranger.h
    src/31_tr_msg.h
    src/31_tr_msg2db.h
    src/31_tr_queue.h
    src/31_tr_treedb.h

    src/ghelpers.h
)

##############################################
#   Core lib
##############################################
OPTION(BUILD_STATIC_LIBS "Build static libraries." ON)
if (MSVC)
  # Turn off Microsofts "security" warnings.
  add_definitions( "/W3 /D_CRT_SECURE_NO_WARNINGS /wd4005 /wd4996 /nologo" )

  if (JANSSON_STATIC_CRT)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
  endif()
endif()

add_library(ghelpers ${SRCS} ${HDRS})

##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})

install(
    TARGETS ghelpers
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)
