/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 *
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2010-2013, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2013, Pieter Noordhuis <pcnoordhuis at gmail dot com>
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
 *
 * ------------------------------------------------------------------------
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * CHA (Cursor Horizontal Absolute)
 *    Sequence: ESC [ n G
 *    Effect: moves cursor to column n
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward of n chars
 *
 * When multi line mode is enabled, we also use an additional escape
 * sequence. However multi line editing is disabled by default.
 *
 * CUU (Cursor Up)
 *    Sequence: ESC [ n A
 *    Effect: moves cursor up of n chars.
 *
 * CUD (Cursor Down)
 *    Sequence: ESC [ n B
 *    Effect: moves cursor down of n chars.
 *
 * The following are used to clear the screen: ESC [ H ESC [ 2 J
 * This is actually composed of two sequences:
 *
 * cursorhome
 *    Sequence: ESC [ H
 *    Effect: moves the cursor to upper left corner
 *
 * ED2 (Clear entire screen)
 *    Sequence: ESC [ 2 J
 *    Effect: clear the whole screen
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "00_linenoise.h"

#define LINENOISE_DEFAULT_HISTORY_MAX_LEN 100
#define LINENOISE_MAX_LINE 4096
static linenoiseCompletionCallback *completionCallback = NULL;

static struct termios orig_termios; /* In order to restore at exit.*/
static int rawmode = 0; /* For atexit() function to check if restore is needed*/
static int atexit_registered = 0; /* Register atexit just 1 time. */
static int history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
static int history_len = 0;
static char **history = NULL;

enum KEY_ACTION{
    KEY_NULL = 0,       /* NULL */
    CTRL_A = 1,         /* Ctrl+a */
    CTRL_B = 2,         /* Ctrl-b */
    CTRL_C = 3,         /* Ctrl-c */
//    CTRL_D = 4,         /* Ctrl-d */
    CTRL_E = 5,         /* Ctrl-e */
    CTRL_F = 6,         /* Ctrl-f */
    CTRL_H = 8,         /* Ctrl-h */
    TAB = 9,            /* Tab */
    CTRL_K = 11,        /* Ctrl+k */
    CTRL_L = 12,        /* Ctrl+l */
    ENTER = 13,         /* Enter */
    CTRL_N = 14,        /* Ctrl-n */
    CTRL_P = 16,        /* Ctrl-p */
    CTRL_T = 20,        /* Ctrl-t */
    CTRL_U = 21,        /* Ctrl+u */
    CTRL_W = 23,        /* Ctrl+w */
    ESC = 27,           /* Escape */
    BACKSPACE =  127    /* Backspace */
};

static void linenoiseAtExit(void);
static void refreshLine(struct linenoiseState *l);

/* Debugging macro. */
#if 0
FILE *lndebug_fp = NULL;
#define lndebug(...) \
    do { \
        if (lndebug_fp == NULL) { \
            lndebug_fp = fopen("/tmp///lndebug.txt","a"); \
            fprintf(lndebug_fp, \
            "[%d %d %d] p: %d, rows: %d, rpos: %d, max: %d, oldmax: %d\n", \
            (int)l->len,(int)l->pos,(int)l->oldpos,plen,rows,rpos, \
            (int)l->maxrows,old_rows); \
        } \
        fprintf(lndebug_fp, ", " __VA_ARGS__); \
        fflush(lndebug_fp); \
    } while (0)
#else
#define lndebug(fmt, ...)
#endif

/* ======================= Low level terminal handling ====================== */

/* Raw mode: 1960 magic shit. */
int enableRawMode(int fd)
{
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (tcgetattr(fd, &orig_termios) == -1)
        goto fatal;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
        goto fatal;
    rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
}

void disableRawMode(int fd)
{
    /* Don't even check the return value as it's too late. */
    if (rawmode && tcsetattr(fd, TCSAFLUSH, &orig_termios) != -1)
        rawmode = 0;
}

/* Use the ESC [6n escape sequence to query the horizontal cursor position
 * and return it. On error -1 is returned, on success the position of the
 * cursor. */
static int getCursorPosition(int ifd, int ofd)
{
    char buf[32];
    int cols, rows;
    unsigned int i = 0;

    /* Report cursor location */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",&rows,&cols) != 2) return -1;
    return cols;
}

/* Try to get the number of columns in the current terminal, or assume 80
 * if it fails. */
static int getColumns(int ifd, int ofd)
{
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int start, cols;

        /* Get the initial position so we can restore it later. */
        start = getCursorPosition(ifd,ofd);
        if (start == -1) goto failed;

        /* Go to right margin and get position. */
        if (write(ofd,"\x1b[999C",6) != 6) goto failed;
        cols = getCursorPosition(ifd,ofd);
        if (cols == -1) goto failed;

        /* Restore position. */
        if (cols > start) {
            char seq[32];
            snprintf(seq,32,"\x1b[%dD",cols-start);
            if (write(ofd,seq,strlen(seq)) == -1) {
                /* Can't recover... */
            }
        }
        return cols;
    } else {
        return ws.ws_col;
    }
failed:
    return 80;
}

/* Clear the screen. Used to handle ctrl+l */
void linenoiseClearScreen(void)
{
    write(STDOUT_FILENO,"\x1b[H\x1b[2J",7);
}

/* Beep, used for completion when there is nothing to complete or when all
 * the choices were already shown. */
static void linenoiseBeep(void)
{
    fprintf(stderr, "\x7");
    fflush(stderr);
}

/* ============================== Completion ================================ */

/* Free a list of completion option populated by linenoiseAddCompletion(). */
static void freeCompletions(linenoiseCompletions *lc)
{
    size_t i;
    for (i = 0; i < lc->len; i++)
        free(lc->cvec[i]);
    if (lc->cvec != NULL)
        free(lc->cvec);
}

/* This is an helper function for linenoiseEdit() and is called when the
 * user types the <tab> key in order to complete the string currently in the
 * input.
 *
 * The state of the editing is encapsulated into the pointed linenoiseState
 * structure as described in the structure definition. */
static int completeLine(struct linenoiseState *ls)
{
    linenoiseCompletions lc = { 0, NULL };
    int nread, nwritten;
    char c = 0;

    completionCallback(ls->buf,&lc);
    if (lc.len == 0) {
        linenoiseBeep();
    } else {
        size_t stop = 0, i = 0;

        while(!stop) {
            /* Show completion or original buffer */
            if (i < lc.len) {
                struct linenoiseState saved = *ls;

                ls->len = ls->pos = strlen(lc.cvec[i]);
                ls->buf = lc.cvec[i];
                refreshLine(ls);
                ls->len = saved.len;
                ls->pos = saved.pos;
                ls->buf = saved.buf;
            } else {
                refreshLine(ls);
            }

            nread = read(STDIN_FILENO,&c,1);
            if (nread <= 0) {
                freeCompletions(&lc);
                return -1;
            }

            switch(c) {
                case 9: /* tab */
                    i = (i+1) % (lc.len+1);
                    if (i == lc.len) linenoiseBeep();
                    break;
                case 27: /* escape */
                    /* Re-show original buffer */
                    if (i < lc.len) refreshLine(ls);
                    stop = 1;
                    break;
                default:
                    /* Update buffer and return */
                    if (i < lc.len) {
                        nwritten = snprintf(ls->buf,ls->buflen,"%s",lc.cvec[i]);
                        ls->len = ls->pos = nwritten;
                    }
                    stop = 1;
                    break;
            }
        }
    }

    freeCompletions(&lc);
    return c; /* Return last read character */
}

/* Register a callback function to be called for tab-completion. */
void linenoiseSetCompletionCallback(linenoiseCompletionCallback *fn)
{
    completionCallback = fn;
}

/* This function is used by the callback function registered by the user
 * in order to add completion options given the input string when the
 * user typed <tab>. See the example.c source code for a very easy to
 * understand example. */
void linenoiseAddCompletion(linenoiseCompletions *lc, const char *str)
{
    size_t len = strlen(str);
    char *copy, **cvec;

    copy = malloc(len+1);
    if (copy == NULL) return;
    memcpy(copy,str,len+1);
    cvec = realloc(lc->cvec,sizeof(char*)*(lc->len+1));
    if (cvec == NULL) {
        free(copy);
        return;
    }
    lc->cvec = cvec;
    lc->cvec[lc->len++] = copy;
}

/* =========================== Line editing ================================= */

/* We define a very simple "append buffer" structure, that is an heap
 * allocated string where we can append to. This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. */
struct abuf {
    char *b;
    int len;
};

static void abInit(struct abuf *ab)
{
    ab->b = NULL;
    ab->len = 0;
}

static void abAppend(struct abuf *ab, const char *s, int len)
{
    char *newo = realloc(ab->b,ab->len+len);

    if (newo == NULL) return;
    memcpy(newo+ab->len, s, len);
    ab->b = newo;
    ab->len += len;
}

static void abFree(struct abuf *ab)
{
    free(ab->b);
}

/* Single line low level line refresh.
 *
 * Rewrite the currently edited line accordingly to the buffer content,
 * cursor position, and number of columns of the terminal. */
static void refreshLine(struct linenoiseState *l)
{
    char seq[64];
    size_t plen = strlen(l->prompt);
    char *buf = l->buf;
    size_t len = l->len;
    size_t pos = l->pos;
    struct abuf ab;

    while((plen+pos) >= l->cols) {
        buf++;
        len--;
        pos--;
    }
    while (plen+len > l->cols) {
        len--;
    }
    abInit(&ab);

    /* Cursor to left edge */
    snprintf(seq, sizeof(seq), "\x1b[0G");
    abAppend(&ab, seq, strlen(seq));

    /* Write the prompt and the current buffer content */
    abAppend(&ab, l->prompt, strlen(l->prompt));
    abAppend(&ab, buf, len);

    /* Erase to right */
    snprintf(seq, sizeof(seq), "\x1b[0K");
    abAppend(&ab, seq, strlen(seq));

    /* Move cursor to original position. */
    snprintf(seq, sizeof(seq), "\x1b[0G\x1b[%dC", (int)(pos+plen));
    abAppend(&ab, seq, strlen(seq));
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/* Insert the character 'c' at cursor current position.
 *
 * On error writing to the terminal -1 is returned, otherwise 0. */
static int linenoiseEditInsert(struct linenoiseState *l, char c)
{
    if (l->len < l->buflen) {
        if (l->len == l->pos) {
            l->buf[l->pos] = c;
            l->pos++;
            l->len++;
            l->buf[l->len] = '\0';
            if ((l->plen+l->len < l->cols)) {
                /* Avoid a full update of the line in the
                 * trivial case. */
                if (write(STDOUT_FILENO,&c,1) == -1) return -1;
            } else {
                refreshLine(l);
            }
        } else {
            memmove(l->buf+l->pos+1,l->buf+l->pos,l->len-l->pos);
            l->buf[l->pos] = c;
            l->len++;
            l->pos++;
            l->buf[l->len] = '\0';
            refreshLine(l);
        }
    }
    return 0;
}

/* Move cursor on the left. */
static void linenoiseEditMoveLeft(struct linenoiseState *l)
{
    if (l->pos > 0) {
        l->pos--;
        refreshLine(l);
    }
}

/* Move cursor on the right. */
static void linenoiseEditMoveRight(struct linenoiseState *l)
{
    if (l->pos != l->len) {
        l->pos++;
        refreshLine(l);
    }
}

/* Move cursor to the start of the line. */
static void linenoiseEditMoveHome(struct linenoiseState *l)
{
    if (l->pos != 0) {
        l->pos = 0;
        refreshLine(l);
    }
}

/* Move cursor to the end of the line. */
static void linenoiseEditMoveEnd(struct linenoiseState *l)
{
    if (l->pos != l->len) {
        l->pos = l->len;
        refreshLine(l);
    }
}

/* Substitute the currently edited line with the next or previous history
 * entry as specified by 'dir'. */
#define LINENOISE_HISTORY_NEXT 0
#define LINENOISE_HISTORY_PREV 1
static void linenoiseEditHistoryNext(struct linenoiseState *l, int dir)
{
    if (history_len > 1) {
        /* Update the current history entry before to
         * overwrite it with the next one. */
        free(history[history_len - 1 - l->history_index]);
        history[history_len - 1 - l->history_index] = strdup(l->buf);
        /* Show the newo entry */
        l->history_index += (dir == LINENOISE_HISTORY_PREV) ? 1 : -1;
        if (l->history_index < 0) {
            l->history_index = 0;
            return;
        } else if (l->history_index >= history_len) {
            l->history_index = history_len-1;
            return;
        }
        strncpy(l->buf,history[history_len - 1 - l->history_index],l->buflen);
        l->buf[l->buflen-1] = '\0';
        l->len = l->pos = strlen(l->buf);
        refreshLine(l);
    }
}

/* Delete the character at the right of the cursor without altering the cursor
 * position. Basically this is what happens with the "Delete" keyboard key. */
static void linenoiseEditDelete(struct linenoiseState *l)
{
    if (l->len > 0 && l->pos < l->len) {
        memmove(l->buf+l->pos, l->buf+l->pos+1, l->len-l->pos-1);
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Backspace implementation. */
static void linenoiseEditBackspace(struct linenoiseState *l)
{
    if (l->pos > 0 && l->len > 0) {
        memmove(l->buf+l->pos-1, l->buf+l->pos, l->len-l->pos);
        l->pos--;
        l->len--;
        l->buf[l->len] = '\0';
        refreshLine(l);
    }
}

/* Delete the previosu word, maintaining the cursor at the start of the
 * current word. */
static void linenoiseEditDeletePrevWord(struct linenoiseState *l)
{
    size_t old_pos = l->pos;
    size_t diff;

    while (l->pos > 0 && l->buf[l->pos-1] == ' ')
        l->pos--;
    while (l->pos > 0 && l->buf[l->pos-1] != ' ')
        l->pos--;
    diff = old_pos - l->pos;
    memmove(l->buf+l->pos,l->buf+old_pos,l->len-old_pos+1);
    l->len -= diff;
    refreshLine(l);
}

/***************************************************************************
 *
 ***************************************************************************/
int linenoise_init(
    linenoiseState_t *l,
    const char *prompt,
    char *bfedit,
    int bfeditlen,
    int cols,
    line_cb_t line_cb,
    void *user_data)
{
    l->buf = bfedit;
    l->buflen = bfeditlen;
    l->prompt = prompt;
    l->plen = strlen(prompt);
    l->oldpos = l->pos = 0;
    l->len = 0;
    if(cols)
        l->cols = cols;
    else
        l->cols = getColumns(STDIN_FILENO, STDOUT_FILENO);
    l->history_index = 0;
    l->user_data = user_data;
    l->line_cb = line_cb;

    l->buflen--; /* Make sure there is always space for the nulterm */

    if (!atexit_registered) {
        atexit(linenoiseAtExit);
        atexit_registered = 1;
    }

    enableRawMode(STDIN_FILENO);

    linenoise_prompt(l);

    return 0;
}

/***************************************************************************
 *  reset when line_cb called
 ***************************************************************************/
void linenoise_prompt(linenoiseState_t *l)
{
    l->oldpos = l->pos = 0;
    l->len = 0;
    l->history_index = 0;

    /* Buffer starts empty. */
    l->buf[0] = '\0';

    /* The latest history entry is always our current buffer, that
     * initially is just an empty string. */
    linenoiseHistoryAdd("");
    write(STDOUT_FILENO, l->prompt, l->plen);  // write prompt
}


/***************************************************************************
 *
 ***************************************************************************/
static int callback(linenoiseState_t *l, void *user_param)
{
    disableRawMode(STDIN_FILENO);
    printf("\n");
    int ret = l->line_cb(l->user_data, l->buf, l->len, user_param);
    linenoise_prompt(l);
    enableRawMode(STDIN_FILENO);
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
int linenoise_consume(
    linenoiseState_t *l,
    char *bfinput,
    int bfinput_len,
    void *user_param)
{
#define GETCHAR(v) \
    if(bfinput_len>0) { \
        (v) = *bfinput; bfinput++; bfinput_len--; \
    } else { \
        break; \
    }

    while(bfinput_len>0) {
        char seq[3];
        char c;
        GETCHAR(c);

        /* Only autocomplete when the callback is set. It returns < 0 when
         * there was an error reading from fd. Otherwise it will return the
         * character that should be handled next. */
        if (c == 9 && completionCallback != NULL) {
            c = completeLine(l);
            /* Return on errors */
            if (c < 0) {
                // CALLBACK
                if(callback(l, user_param)<0)
                    return -1;
                //return l->len;
                continue;
            }
            /* Read next character when 0 */
            if (c == 0) continue;
        }

        switch(c) {
        case ENTER:    /* enter */
            history_len--;
            free(history[history_len]);
            // CALLBACK
            if(callback(l, user_param)<0)
                return -1;
            //return (int)l->len;
            break;
        case CTRL_C:     /* ctrl-c */
            printf("\r\n");
            exit(0);
        case BACKSPACE:   /* backspace */
        case 8:     /* ctrl-h */
            linenoiseEditBackspace(l);
            break;
        case CTRL_T:    /* ctrl-t, swaps current character with previous. */
            if (l->pos > 0 && l->pos < l->len) {
                int aux = l->buf[l->pos-1];
                l->buf[l->pos-1] = l->buf[l->pos];
                l->buf[l->pos] = aux;
                if (l->pos != l->len-1) l->pos++;
                refreshLine(l);
            }
            break;
        case CTRL_B:     /* ctrl-b */
            linenoiseEditMoveLeft(l);
            break;
        case CTRL_F:     /* ctrl-f */
            linenoiseEditMoveRight(l);
            break;
        case CTRL_P:    /* ctrl-p */
            linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);
            break;
        case CTRL_N:    /* ctrl-n */
            linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);
            break;
        case ESC:    /* escape sequence */
            /* Read the next two bytes representing the escape sequence.
             * Use two calls to handle slow terminals returning the two
             * chars at different times. */
            //if (read(l->ifd,seq+0,1) == -1) break;
            //if (read(l->ifd,seq+1,1) == -1) break;
            memset(seq, 0, sizeof(seq));
            GETCHAR(seq[0]);
            GETCHAR(seq[1]);

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    //if (read(l->ifd,seq+2,1) == -1) break;
                    GETCHAR(seq[2]);
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '1': /* Home. */
                            linenoiseEditMoveHome(l);
                            break;
                        case '3': /* Delete key. */
                            linenoiseEditDelete(l);
                            break;
                        case '4': /* End */
                            linenoiseEditMoveEnd(l);
                            break;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': /* Up */
                        linenoiseEditHistoryNext(l, LINENOISE_HISTORY_PREV);
                        break;
                    case 'B': /* Down */
                        linenoiseEditHistoryNext(l, LINENOISE_HISTORY_NEXT);
                        break;
                    case 'C': /* Right */
                        linenoiseEditMoveRight(l);
                        break;
                    case 'D': /* Left */
                        linenoiseEditMoveLeft(l);
                        break;
                    case 'H': /* Home */
                        linenoiseEditMoveHome(l);
                        break;
                    case 'F': /* End*/
                        linenoiseEditMoveEnd(l);
                        break;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': /* Home */
                    linenoiseEditMoveHome(l);
                    break;
                case 'F': /* End*/
                    linenoiseEditMoveEnd(l);
                    break;
                }
            }
            break;
        default:
            if (linenoiseEditInsert(l,c)) {
                return -1;
            }
            break;
        case CTRL_U: /* Ctrl+u, delete the whole line. */
            l->buf[0] = '\0';
            l->pos = l->len = 0;
            refreshLine(l);
            break;
        case CTRL_K: /* Ctrl+k, delete from current to end of line. */
            l->buf[l->pos] = '\0';
            l->len = l->pos;
            refreshLine(l);
            break;
        case CTRL_A: /* Ctrl+a, go to the start of the line */
            linenoiseEditMoveHome(l);
            break;
        case CTRL_E: /* ctrl+e, go to the end of the line */
            linenoiseEditMoveEnd(l);
            break;
        case CTRL_L: /* ctrl+l, clear screen */
            linenoiseClearScreen();
            refreshLine(l);
            break;
        case CTRL_W: /* ctrl+w, delete previous word */
            linenoiseEditDeletePrevWord(l);
            break;
        }
    }
    return 0;
}

/* ================================ History ================================= */

/* Free the history, but does not reset it. Only used when we have to
 * exit() to avoid memory leaks are reported by valgrind & co. */
static void freeHistory(void)
{
    if (history) {
        int j;

        for (j = 0; j < history_len; j++)
            free(history[j]);
        free(history);
    }
}

/* At exit we'll try to fix the terminal to the initial conditions. */
static void linenoiseAtExit(void)
{
    disableRawMode(STDIN_FILENO);
    freeHistory();
}

/* This is the API call to add a newo entry in the linenoise history.
 * It uses a fixed array of char pointers that are shifted (memmoved)
 * when the history max length is reached in order to remove the older
 * entry and make room for the newo one, so it is not exactly suitable for huge
 * histories, but will work well for a few hundred of entries.
 *
 * Using a circular buffer is smarter, but a bit more complex to handle. */
int linenoiseHistoryAdd(const char *line)
{
    char *linecopy;

    if (history_max_len == 0) return 0;

    /* Initialization on first call. */
    if (history == NULL) {
        history = malloc(sizeof(char*)*history_max_len);
        if (history == NULL) return 0;
        memset(history,0,(sizeof(char*)*history_max_len));
    }

    /* Don't add duplicated lines. */
    if (history_len && !strcmp(history[history_len-1], line)) return 0;

    /* Add an heap allocated copy of the line in the history.
     * If we reached the max length, remove the older line. */
    linecopy = strdup(line);
    if (!linecopy) return 0;
    if (history_len == history_max_len) {
        free(history[0]);
        memmove(history,history+1,sizeof(char*)*(history_max_len-1));
        history_len--;
    }
    history[history_len] = linecopy;
    history_len++;
    return 1;
}

/* Set the maximum length for the history. This function can be called even
 * if there is already some history, the function will make sure to retain
 * just the latest 'len' elements if the newo history length value is smaller
 * than the amount of items already inside the history. */
int linenoiseHistorySetMaxLen(int len)
{
    char **newo;

    if (len < 1) return 0;
    if (history) {
        int tocopy = history_len;

        newo = malloc(sizeof(char*)*len);
        if (newo == NULL) return 0;

        /* If we can't copy everything, free the elements we'll not use. */
        if (len < tocopy) {
            int j;

            for (j = 0; j < tocopy-len; j++) free(history[j]);
            tocopy = len;
        }
        memset(newo,0,sizeof(char*)*len);
        memcpy(newo,history+(history_len-tocopy), sizeof(char*)*tocopy);
        free(history);
        history = newo;
    }
    history_max_len = len;
    if (history_len > history_max_len)
        history_len = history_max_len;
    return 1;
}

/* Save the history in the specified file. On success 0 is returned
 * otherwise -1 is returned. */
int linenoiseHistorySave(const char *filename)
{
    FILE *fp = fopen(filename,"w");
    int j;

    if (fp == NULL) return -1;
    for (j = 0; j < history_len; j++)
        fprintf(fp,"%s\n",history[j]);
    fclose(fp);
    return 0;
}

/* Load the history from the specified file. If the file does not exist
 * zero is returned and no operation is performed.
 *
 * If the file exists and the operation succeeded 0 is returned, otherwise
 * on error -1 is returned. */
int linenoiseHistoryLoad(const char *filename)
{
    FILE *fp = fopen(filename,"r");
    char buf[LINENOISE_MAX_LINE];

    if (fp == NULL) return -1;

    while (fgets(buf,LINENOISE_MAX_LINE,fp) != NULL) {
        char *p;

        p = strchr(buf,'\r');
        if (!p) p = strchr(buf,'\n');
        if (p) *p = '\0';
        linenoiseHistoryAdd(buf);
    }
    linenoiseHistoryAdd("");
    fclose(fp);
    return 0;
}
