/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: unistd.h
 *
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

#define SYS_READ                  0
#define SYS_WRITE                 1
#define SYS_OPEN                  2
#define SYS_CLOSE                 3
#define SYS_IOCTL                16
#define SYS_DUP2                 33
#define SYS_SIGNAL               34
#define SYS_SIGRETURN            35
#define SYS_SET_SIGTRAMP         36
#define SYS_KILL                 62
#define SYS_FORK                 57
#define SYS_EXECVE               59
#define SYS_EXIT                 60
#define SYS_UNAME                63
#define SYS_GETPID               39
#define SYS_YIELD                24
#define SYS_LSEEK                 8
#define SYS_MMAP                  9
#define SYS_MUNMAP               11
#define SYS_BRK                  12
#define SYS_WAITPID              61
#define SYS_GETDENTS             78
#define SYS_FTRUNCATE            77
#define SYS_RENAME               82
#define SYS_MKDIR                83
#define SYS_UNLINK               87
#define SYS_GETUID              102
#define SYS_GETGID              104
#define SYS_REBOOT              169
#define SYS_CLOCK_GETTIME       228
#define SYS_EVENTFD             290
#define SYS_EVENTFD_OPEN        291
#define SYS_IPC_CREATE          292
#define SYS_IPC_OPEN            293
#define SYS_IPC_SEND            294
#define SYS_IPC_RECV            295
#define SYS_IPC_EVENTFD         296
#define SYS_SPAWN               400

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

#define REBOOT_SHUTDOWN 0
#define REBOOT_REBOOT   1

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

long write(int fd, const void *buf, size_t count);
long read(int fd, void *buf, size_t count);
long open(const char *path, int flags);
long close(int fd);
long ioctl(int fd, unsigned long request, void *arg);
long lseek(int fd, long offset, int whence);
long getpid(void);
int dup2(int oldfd, int newfd);
long fork(void);
int execve(const char *path, char *const argv[], char *const envp[]);
int execl(const char *path, const char *arg0, ...);
long spawn(const char *path);
void yield(void);
long getuid(void);
long getgid(void);
long unlink(const char *path);
long getdents(int fd, void *buf, size_t size);
long ftruncate(int fd, long size);
void *brk_call(void *addr);
struct timespec;
int clock_gettime(int clock_id, struct timespec *timespec);

long reboot(int cmd);

void _exit(int code);

#endif
