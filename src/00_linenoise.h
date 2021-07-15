/* 00_linenoise.h -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * See linenoise.c for more information.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct linenoiseCompletions {
  size_t len;
  char **cvec;
} linenoiseCompletions;

typedef void(linenoiseCompletionCallback)(const char *, linenoiseCompletions *);
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *);
void linenoiseAddCompletion(linenoiseCompletions *, const char *);

int linenoiseHistoryAdd(const char *line);
int linenoiseHistorySetMaxLen(int len);
int linenoiseHistorySave(const char *filename);
int linenoiseHistoryLoad(const char *filename);
void linenoiseClearScreen(void);

typedef int (*line_cb_t)(
    void *user_data,
    char *line,
    int linelength,
    void *user_param
);

typedef struct linenoiseState {
    char *buf;          /* Edited line buffer. */
    size_t buflen;      /* Edited line buffer size. */
    const char *prompt; /* Prompt to display. */
    const char *history_file; /* Prompt to display. */
    size_t plen;        /* Prompt length. */
    size_t pos;         /* Current cursor position. */
    size_t oldpos;      /* Previous refresh cursor position. */
    size_t len;         /* Current edited line length. */
    size_t cols;        /* Number of columns in terminal. */
    int history_index;  /* The history index we are currently editing. */
    line_cb_t line_cb;
    void *user_data;
    char connected;
} linenoiseState_t;


int linenoise_init(
    linenoiseState_t *l,
    const char *prompt,
    char *bfedit,
    int bfeditlen,
    int cols,
    line_cb_t line_cb,
    void *user_data
);
void linenoise_prompt(linenoiseState_t *l);
int linenoise_consume(
    linenoiseState_t *l,
    char *bfinput,
    int bfinput_len,
    void *user_param
);

int enableRawMode(int fd);
void disableRawMode(int fd);

#ifdef __cplusplus
}
#endif
