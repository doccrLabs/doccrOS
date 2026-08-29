/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: sys_process.c
 *
 */

#include "sys_process.h"
#include "ptr.h"
#include <kernel/proc/thread.h>
#include <kernel/proc/process.h>
#include <kernel/proc/scheduler.h>
#include <kernel/fs/openfile.h>
#include <kernel/packages/elf/elf.h>
#include <kernel/communication/serial.h>
#include <kernel/mem/phys/physmem.h>
#include <kernel/mem/kheap/kheap.h>

void sys_exit(cpu_state_t *state)
{
    (void)state;
    thread_exit();
}

void sys_yield(cpu_state_t *state)
{
    sched_yield();
    state->rax = 0;
}

void sys_getpid(cpu_state_t *state)
{
    proc_t *p = process_get_current();
    state->rax = p ? p->pid : (u64)-1;
}

void sys_fork(cpu_state_t *state)
{
    //printf("[SYS_FORK] before: physmem_free=%llu kheap_free=%llu\n",
    //       physmem_free_get(), kheap_get_free_size());

    proc_t *child = process_fork(state);

    //printf("[SYS_FORK] after: physmem_free=%llu kheap_free=%llu result=%s\n",
    //       physmem_free_get(), kheap_get_free_size(), child ? "OK" : "FAILED");

    state->rax = child ? child->pid : (u64)-1;
}

void sys_dup2(cpu_state_t *state)
{
    int oldfd = (int)state->rdi;
    int newfd = (int)state->rsi;

    proc_t *p = process_get_current();
    if (!p)
    {
        state->rax = (u64)-1;
        return;
    }

    if (oldfd < 0 || oldfd >= FD_MAX || newfd < 0 || newfd >= FD_MAX)
    {
        state->rax = (u64)-1;
        return;
    }

    if (!p->fd_table[oldfd].used)
    {
        state->rax = (u64)-1;
        return;
    }

    if (oldfd == newfd)
    {
        state->rax = (u64)newfd;
        return;
    }

    if (p->fd_table[newfd].used && p->fd_table[newfd].ofd >= 0) openfile_unref(p->fd_table[newfd].ofd);

    p->fd_table[newfd].used = 1;
    p->fd_table[newfd].ofd = p->fd_table[oldfd].ofd;

    if (p->fd_table[newfd].ofd >= 0) openfile_ref(p->fd_table[newfd].ofd);

    state->rax = (u64)newfd;
}

void sys_execve(cpu_state_t *state)
{
    const char *path =  (const char *)state->rdi;
    u64 user_argv    = state->rsi;

    if (!user_ptr_ok((u64)path))
    {
        state->rax = (u64)-1;
        return;
    }

    char path_buf[VFS_MAX_PATH];
    int i = 0;
    while (path[i]  && i < VFS_MAX_PATH - 1)
    {
        path_buf[i] = path[i];
        i++;
    }
    path_buf[i] = '\0';

    vfs_node_t *node = vfs_find(path_buf);

    if (!node ||
        node->type != VFS_FILE ||
        !node->data ||
        node->size == 0)
    {
        state->rax = (u64)-1;
        return;
    }

    proc_t *p = process_get_current();
    if (!p)
    {
        state->rax = (u64)-1;
        return;
    }

    char name_buf[64];
    {
        const char *base = path_buf;

        for (const char *s = path_buf; *s; s++)
        {
            if (*s == '/')
                base = s + 1;
        }

        int j = 0;
        while (base[j] && j < 63)
        {
            name_buf[j] = base[j];
            j++;
        }
        name_buf[j] = '\0';
    }

    char *argv[EXEC_ARGV_MAX];
    int argc = 0;
    int have_argv = (user_argv != 0);

    if (have_argv)
    {
        if (elf_collect_user_argv(
                p->space,
                user_argv,
                argv,
                &argc
            ) != 0)
        {
            state->rax = (u64)-1;
            return;
        }
    }
    else
    {
        argv[0] = name_buf;
        argc = 1;
    }

    if (elf_exec_replace(
            p,
            state,
            node->data,
            node->size,
            name_buf,
            argv,
            argc
        ) != 0)
    {
        if (have_argv) elf_free_argv(argv, argc);

        #if DEBUGINFO
            printf(
                "[EXECVE] exec of '%s' failed, killing pid=%llu\n",
                path_buf,
                p->pid
            );
        #endif

        thread_t *self = thread_get_current();
        if (self)
        {
            self->state = THREAD_DEAD;

            if (p->alive_count > 0) p->alive_count--;
        }
        process_exit(p, 1);
        sched_yield();

        __asm__ volatile("sti");
        for (;;) __asm__ volatile("hlt");
    }

    if (have_argv) elf_free_argv(argv, argc);
}

void sys_spawn(cpu_state_t *state)
{
    const char *path = (const char *)state->rdi;
    char path_buf[VFS_MAX_PATH];
    int i = 0;

    if (!user_ptr_ok((u64)path))
    {
        state->rax = (u64)-1;
        return;
    }
    while (path[i] && i < VFS_MAX_PATH - 1)
    {
        path_buf[i] = path[i];
        i++;
    }
    path_buf[i] = '\0';

    vfs_node_t *node = vfs_find(path_buf);
    if (!node || node->type != VFS_FILE || !node->data || node->size == 0)
    {
        state->rax = (u64)-1;
        return;
    }

    const char *base = path_buf;
    char name_buf[64];
    int j = 0;

    for (const char *s = path_buf; *s; s++) if (*s == '/') base = s + 1;

    while (base[j] && j < (int)sizeof(name_buf) - 1)
    {
        name_buf[j] = base[j];
        j++;
    }
    name_buf[j] = '\0';

    proc_t *parent = process_get_current();
    u64 caps = parent ? parent->capabilities : 0;
    u64 pid = 0;
    if (elf_load(node->data, node->size, name_buf, caps, &pid) != 0)
    {
        state->rax = (u64)-1;
        return;
    }

    //printf("[SYS_SPAWN] launched '%s' pid=%llu\n", path_buf, pid);
    state->rax = pid;
}

void sys_waitpid(cpu_state_t *state)
{
    i64 target_pid = (i64)state->rdi;
    int *wstatus_ptr = (int *)state->rsi;

    proc_t *caller    = process_get_current();
    if (!caller)
    {
        state->rax    = (u64)-1;
        return;
    }

    int exit_code    = 0;
    int result       = process_waitpid(caller, target_pid, &exit_code);

    if (result != 0)
    {
        state->rax   = (u64)-1; // no dead kids found
        return;
    }


    if (
    	wstatus_ptr &&
     	(u64)wstatus_ptr <= 0x00007FFFFFFFFFFFULL
    )*wstatus_ptr  = (exit_code & 0xFF) << 8;

    state->rax     = (u64)target_pid;
}

//always 0 for now cuz were jst root
void sys_getuid(cpu_state_t *state)
{
    proc_t *p = process_get_current();
    state->rax = p ? (u64)p->uid : 0; // 0 = root, this is fine trust me
}

void sys_getgid(cpu_state_t *state)
{
    proc_t *p = process_get_current();
    state->rax = p ? (u64)p->gid : 0;
}
