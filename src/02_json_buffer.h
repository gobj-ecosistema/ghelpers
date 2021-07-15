/* buffer.h -- Auto-growing string buffers
 *
 * Copyright (c) 2013 Ginés Martínez
 *
 * Copyright (c) 2012 BalaBit IT Security Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BALABIT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BALABIT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#pragma once

/*
 *  Dependencies
 */
#include "01_print_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  El código original de lumberlog solo escribe key:string.
 *  Éste código está ampliado a números.
 *  No usa listas sino un array de bufferes,
 *  por lo que las instancias están limitadas a MAX_UL_BUFFER.
 *  No es su finalidad tener miles de instancias.
 *  Users: glogger and print_error
 *  Se podrían realizar test de performance
 *  para incluir trozos del nuevo código.
 */

typedef int hgen_t;
PUBLIC hgen_t json_dict(void); // Return 0 on error.
PUBLIC void json_reset(hgen_t hgen, BOOL indented);
PUBLIC void json_add_string(hgen_t hgen, char *key, char *str);
PUBLIC void json_add_null(hgen_t hgen, char *key);
PUBLIC void json_add_double(hgen_t hgen, char *key, double number);
PUBLIC void json_add_integer(hgen_t hgen, char *key, long long int number);
PUBLIC char * json_get_buf(hgen_t hgen);

//TODO funcion de carga: # dicts y su max mem

#ifdef __cplusplus
}
#endif
