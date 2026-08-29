/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: rc.c
 *
 */

#include "sulfd.h"
#include "rc.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
//#include <sys/wait.h>
#include <stdlib.h>

static char buf[2048];
static char *tp;
static char tb[256]; // token result buffer

#define PREFIX ":: sulfd: "
#define CMD_PREFIX '-'

#define VAR_NAME "var"
#define EXE_NAME "exec"
#define WAI_NAME "wait"
#define IFS_NAME "if"
#define ELS_NAME "ef"
#define EIF_NAME "fi"

#define CURRENT_SKIP (if_top >= 0 && if_stack[if_top])
#define BG ""

static char *tok(void)
{
    int i = 0;

    while (*tp == ' ' || *tp == '\t') tp++;

    if (!*tp || *tp == '\n') return NULL;

    if (*tp == '"')
    {
        tp++;

        while (
            *tp &&
            *tp != '"' &&
            *tp != '\n' &&
            i < 255
        ) {
            tb[i++] = *tp++;
        }

        if (*tp == '"') tp++;
        tb[i] = '\0';
        return tb;
    }

    if (*tp == '=')
    {
        if (*(tp + 1) == '=')
        {
            tb[0] = '=';
            tb[1] = '=';
            tb[2] = '\0';

            tp += 2;

            return tb;
        }

        tb[0] = *tp++;
        tb[1] = '\0';

        return tb;
    }

    if (*tp == '(' || *tp == ')' || *tp == ':')
    {
        tb[0] = *tp++;
        tb[1] = '\0';
        return tb;
    }

    while (
        *tp &&
        *tp != ' ' &&
        *tp != '\t' &&
        *tp != '\n' &&
        *tp != '=' &&
        *tp != '(' &&
        *tp != ')' &&
        *tp != ':' &&
        *tp != '"' &&
        i < 255
    ){
        tb[i++] = *tp++;
    }

    tb[i] = '\0';

    return i ? tb : NULL;
}

static void skipline(void)
{
    while (*tp && *tp != '\n') tp++;
    if (*tp) tp++;
}

static void sulfd_sleep(int ticks)
{
    volatile int i;
    volatile int j;

    for (i = 0; i < ticks; i++)
    {
        for (j = 0; j < 1000000; j++);
    }
}

static int is_direct_path(const char *s)
{
    return s && (s[0] == '/' || s[0] == '.');
}

int sulfd_parse(const char *path, sulfd_t *out)
{
    if (!out) return -1;

    out->var_count = 0;
    out->exec_count = 0;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    int n = (int)read(fd, buf, sizeof(buf) - 1);

    close(fd);

    if (n <= 0) return -1;

    buf[n] = '\0';
    tp = buf;

    while (*tp)
    {
        while (*tp == ' ' || *tp == '\t') tp++;

        if (*tp == '\n')
        {
            tp++;
            continue;
        }

        if (*tp == '/' && *(tp + 1) == '/') //comments
        {
            skipline();
            continue;
        }

        char *kw = tok();

        if (!kw)
        {
            skipline();
            continue;
        }

        // var name=("path")
        if (
            strcmp(kw, VAR_NAME) == 0 &&
            out->var_count < SULFD_MAX_VARS
        )
        {
            sulfd_var_t *v = &out->vars[out->var_count];

            memset(v, 0, sizeof(*v));

            char *name = tok();

            if (!name)
            {
                skipline();
                continue;
            }

            strncpy(v->name, name, sizeof(v->name) - 1);
            v->name[sizeof(v->name) - 1] = '\0';

            char *eq = tok();

            if (!eq || strcmp(eq, "=") != 0)
            {
                skipline();
                continue;
            }

            char *value = tok();

            if (!value)
            {
                skipline();
                continue;
            }

            if (strcmp(value, "(") == 0)
            {
                char *path_value = tok();

                if (path_value)
                {
                    strncpy(
                        v->path,
                        path_value,
                        sizeof(v->path) - 1
                    );

                    v->path[sizeof(v->path) - 1] = '\0';
                }

                v->is_value = 0;

                tok();
            }
            else
            {
                v->value = atoi(value);
                v->is_value = 1;
            }

            out->var_count++;
        }

        //print("text")
        else if (
            strcmp(kw, "print") == 0 &&
            out->exec_count < SULFD_MAX_EXECS
        )
        {
            sulfd_exec_t *e = &out->execs[out->exec_count];

            memset(e, 0, sizeof(*e));

            e->is_print = 1;

            tok(); // (
            char *msg = tok(); // "text"

            if (msg)
            {
                strncpy(e->message, msg, sizeof(e->message) - 1);
                e->message[sizeof(e->message) - 1] = '\0';
            }

            out->exec_count++;
        }

        //elog("text")
        else if (
            strcmp(kw, "elog") == 0 &&
            out->exec_count < SULFD_MAX_EXECS
        )
        {
            sulfd_exec_t *e = &out->execs[out->exec_count];

            memset(e, 0, sizeof(*e));

            e->is_elog = 1;

            tok(); // (
            char *msg = tok(); // "text"

            if (msg)
            {
                strncpy(e->message, msg, sizeof(e->message) - 1);
                e->message[sizeof(e->message) - 1] = '\0';
            }

            out->exec_count++;
        }

        // wait <time>
        else if (
            strcmp(kw, WAI_NAME) == 0 &&
            out->exec_count < SULFD_MAX_EXECS
        )
        {
            sulfd_exec_t *e = &out->execs[out->exec_count];

            memset(e, 0, sizeof(*e));

            e->is_wait = 1;

            char *t = tok();

            if (t) e->wait_time = atoi(t);

            out->exec_count++;
        }

        // exec var_name
        else if (
            strcmp(kw, EXE_NAME) == 0 &&
            out->exec_count < SULFD_MAX_EXECS
        )
        {
            sulfd_exec_t *e = &out->execs[out->exec_count];

            memset(e, 0, sizeof(*e));

            char *next = tok();

            if (!next)
            {
                skipline();
                continue;
            }

            while (next && next[0] == CMD_PREFIX)
            {
                if (strcmp(next + 1, "bg") == 0) e->bg = 1;
                next = tok();
            }

            if (!next)
            {
                skipline();
                continue;
            }

            if (is_direct_path(next))
            {
                strncpy(
                    e->direct_path,
                    next,
                    sizeof(e->direct_path) - 1
                );

                e->direct_path[sizeof(e->direct_path) - 1] = '\0';
            }
            else
            {
                strncpy(
                    e->var_name,
                    next,
                    sizeof(e->var_name) - 1
                );

                e->var_name[sizeof(e->var_name) - 1] = '\0';
            }

            out->exec_count++;
        }

        else if (
            strcmp(kw, IFS_NAME) == 0 &&
            out->exec_count < SULFD_MAX_EXECS
        )
        {
            sulfd_exec_t *e = &out->execs[out->exec_count];

            memset(e, 0, sizeof(*e));

            e->is_if = 1;

            char *var = tok();

            if (var)
            {
                strncpy(
                    e->var_name,
                    var,
                    sizeof(e->var_name) - 1
                );

                e->var_name[
                    sizeof(e->var_name) - 1
                ] = '\0';
            }

            char *op = tok();

            if (op && strcmp(op, "==") == 0)
            {
                char *value = tok();

                if (value) e->wait_time = atoi(value);
            }

            out->exec_count++;
        }

        else if (
            strcmp(kw, ELS_NAME) == 0 &&
            out->exec_count < SULFD_MAX_EXECS
        )
        {
            sulfd_exec_t *e = &out->execs[out->exec_count];

            memset(e, 0, sizeof(*e));

            e->is_else = 1;
            out->exec_count++;
        }

        else if (
            strcmp(kw, EIF_NAME) == 0 &&
            out->exec_count < SULFD_MAX_EXECS
        )
        {
            sulfd_exec_t *e = &out->execs[out->exec_count];

            memset(e, 0, sizeof(*e));

            e->is_endif = 1;
            out->exec_count++;
        }

        skipline();
    }

    return 0;
}

void sulfd_run(sulfd_t *rc)
{
    int if_stack[32];
    int if_top = -1;

    //printf("strt emexrc_run");
    for (int i = 0; i < rc->exec_count; i++)
    {
        sulfd_exec_t *e = &rc->execs[i];

        if (e->is_if)
        {
            int condition = 0;

            for (int j = 0; j < rc->var_count; j++)
            {
                if (
                    strcmp(
                        rc->vars[j].name,
                        e->var_name
                    ) == 0
                ) {
                    if (
                        rc->vars[j].is_value &&
                        rc->vars[j].value == e->wait_time
                    ) {
                        condition = 1;
                    }

                    break;
                }
            }

            if (if_top < 31)
            {
                if_top++;
                if_stack[if_top] = !condition;
            }

            continue;
        }

        if (e->is_else)
        {
            if (if_top >= 0) if_stack[if_top] = !if_stack[if_top];

            continue;
        }

        if (e->is_endif)
        {
            if (if_top >= 0) if_top--;

            continue;
        }

        if (if_top >= 0 && if_stack[if_top]) continue;

        if (e->is_wait)
        {
            //printf(PREFIX "wait %d\n", e->wait_time);
            sulfd_sleep(e->wait_time);
            continue;
        }

        if (e->is_print)
        {
            printf("%s\n", e->message);
            continue;
        }

        if (e->is_elog)
        {
            printf(PREFIX "%s\n", e->message);
            continue;
        }

        const char *path = NULL;

        if (e->direct_path[0] != '\0')
        {
            path = e->direct_path;
        }
        else
        {
            for (int j = 0; j < rc->var_count; j++)
            {
                if (strcmp(rc->vars[j].name, e->var_name) == 0)
                {
                    path = rc->vars[j].path;
                    break;
                }
            }
        }

        if (!path)
        {
            fprintf(
                stderr,
                PREFIX "unknown var '%s'\n",
                e->var_name
            );

            continue;
        }

        printf(
            PREFIX EXE_NAME " %s%s\n",
            path,
            e->bg ? BG : ""
        );

        long pid = spawn(path);

        if (pid < 0)
        {
            fprintf(
                stderr,
                PREFIX "failed to spawn '%s'\n",
                path
            );

            continue;
        }

        //if (!e->bg) waitpid((pid_t)pid, NULL, 0);
    }

}
