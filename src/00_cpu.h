/*
 *  Code from:
 *
 * pidstat: Report statistics for Linux tasks
 * (C) 2007-2016 by Sebastien GODARD (sysstat <at> orange.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */

#ifndef __CPU_H_
#define __CPU_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************
 *              Constants
 **************************************************/

/***************************************************
 *              Structures
 **************************************************/

/***************************************************
 *              Prototypes
 **************************************************/

unsigned long total_ram_in_kb(void); /* Total memory in kB */
unsigned long free_ram_in_kb(void); /* Free memory in kB */
int cpu_usage(unsigned int pid, uint64_t *system_time, uint64_t *process_time);
unsigned long proc_vmem_in_kb(unsigned int pid); /* virtual memory in kB */


#ifdef __cplusplus
}
#endif

#endif
