/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: elf.c
 *
 */

#include "elf.h"

#include <kernel/proc/process.h>
#include <kernel/proc/thread.h>
#include <kernel/proc/scheduler.h>
#include <kernel/mem/vmm/vmm.h>
#include <kernel/mem/meminclude.h>
#include <kernel/mem/paging/paging.h>
#include <kernel/communication/serial.h>
#include <kernel/screen/lib/log.h>
#include <kernel/proc/signal.h>
#include "../../fs/openfile.h"

static inline u64 align_up(u64 v, u64 a)
{
    return (v + a - 1) & ~(a - 1);
}

static inline u64 align_down(u64 v, u64 a)
{
    return v & ~(a - 1);
}

static int elf_check(const u8 *data, u64 size)
{
    if (size < sizeof(elf64_ehdr_t))
        return -1;

    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)data;

    if (eh->e_ident[0] != ELF_MAGIC0 ||
        eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 ||
        eh->e_ident[3] != ELF_MAGIC3)
    {
        printf("[ELF] bad magic\n");
        return -1;
    }

    if (eh->e_ident[4] != ELF_CLASS64)
    {
        printf("[ELF] not ELF64\n");
        return -1;
    }
    if (eh->e_ident[5] != ELF_DATA2LSB)
    {
        printf("[ELF] not little-endian\n");
        return -1;
    }
    if (eh->e_type != ELF_TYPE_EXEC && eh->e_type != ELF_TYPE_DYN)
    {
        printf("[ELF] not executable (e_type=%u)\n", (u32)eh->e_type);
        return -1;
    }
    if (eh->e_machine != ELF_MACH_X86_64)
    {
        printf("[ELF] not x86-64\n");
        return -1;
    }
    if (eh->e_phentsize < sizeof(elf64_phdr_t))
    {
        printf("[ELF] phentsize too small\n");
        return -1;
    }

    u64 ph_end = eh->e_phoff + (u64)eh->e_phnum * eh->e_phentsize;
    if (ph_end > size)
    {
        printf("[ELF] phdrs out of bounds\n");
        return -1;
    }

    return 0;
}

#define USER_STACK_PAGES 16ULL
#define USER_STACK_TOP   0x00007FFFFFFFE000ULL
#define USER_STACK_BASE  (USER_STACK_TOP - USER_STACK_PAGES * 4096ULL)

#define USER_STACK_PAGES_EXEC USER_STACK_PAGES

static int elf_map_segments_and_stack(
    proc_t *p,
    const u8 *data,
    u64 size,
    const elf64_ehdr_t *eh,
    u64 *out_entry
){
    u64 hhdm = paging_get_hhdm_offset();

    for (u16 i = 0; i < eh->e_phnum; i++)
    {
        const elf64_phdr_t *ph = (const elf64_phdr_t *)(data + eh->e_phoff + (u64)i * eh->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_offset + ph->p_filesz > size) return -1;
        if (ph->p_memsz < ph->p_filesz) return -1;

        u64 va_base  = align_down(ph->p_vaddr, 4096);
        u64 va_end   = align_up(ph->p_vaddr + ph->p_memsz, 4096);
        u64 pg_count = (va_end - va_base) / 4096;

        u32 vmm_flags = VMM_REGION_USER | VMM_REGION_READ;
        if (ph->p_flags & PF_W) vmm_flags |= VMM_REGION_WRITE;
        if (ph->p_flags & PF_X) vmm_flags |= VMM_REGION_EXEC;

        u64 mapped = vmm_space_alloc(p->space, va_base, pg_count, vmm_flags);
        if (!mapped) return -1;

        u64 file_src  = (u64)(data + ph->p_offset);
        u64 file_rem  = ph->p_filesz;
        u64 va_cursor = ph->p_vaddr;
        u64 mem_rem   = ph->p_memsz;

        while (mem_rem > 0)
        {
            u64 page_va   = align_down(va_cursor, 4096);
            u64 page_off  = va_cursor - page_va;
            u64 phys      = vmm_space_get_phys(p->space, page_va);
            if (!phys) return -1;

            u8 *dest  = (u8 *)(phys + hhdm + page_off);
            u64 chunk = 4096 - page_off;
            if (chunk > mem_rem) chunk = mem_rem;

            if (file_rem > 0)
            {
                u64 copy = (chunk < file_rem) ? chunk : file_rem;
                memcpy(dest, (const void *)file_src, copy);
                file_src += copy;
                file_rem -= copy;
                dest  += copy;
                chunk -= copy;
            }
            if (chunk > 0) memset(dest, 0, chunk);

            u64 step = 4096 - page_off;
            if (step > mem_rem) step = mem_rem;
            va_cursor += step;
            mem_rem -= step;
        }
    }

    u64 stack = vmm_space_alloc(
        p->space,
        USER_STACK_BASE, USER_STACK_PAGES_EXEC,
        VMM_REGION_USER | VMM_REGION_READ | VMM_REGION_WRITE
    );
    if (!stack) return -1;

    *out_entry = eh->e_entry;
    return 0;
}


#define EXEC_ARGSTR_MAX  256
#define EXEC_ARGS_TOTAL  (16 * 1024)

static int copy_from_user_space(vmm_space_t *space, u64 uaddr, void *kdst, u64 len)
{
    u64 hhdm = paging_get_hhdm_offset();
    u64 va = uaddr;
    u64 remaining = len;
    u8 *dst = (u8 *)kdst;

    while (remaining > 0)
    {
        u64 page_va  = va & ~0xFFFULL;
        u64 page_off = va - page_va;
        u64 phys     = vmm_space_get_phys(space, page_va);

        if (!phys)
            return -1;

        u8 *src = (u8 *)(phys + hhdm + page_off);

        u64 chunk = 4096 - page_off;
        if (chunk > remaining) chunk = remaining;

        memcpy(dst, src, chunk);

        va += chunk;
        dst += chunk;
        remaining -= chunk;
    }

    return 0;
}

int elf_collect_user_argv(
    vmm_space_t *space,
    u64 user_argv,
    char *argv_out[EXEC_ARGV_MAX],
    int *argc_out
)
{
    *argc_out = 0;
    u64 total = 0;

    if (user_argv == 0) return 0;
    if (user_argv > 0x00007FFFFFFFFFFFULL) return -1;

    for (int i = 0; i < EXEC_ARGV_MAX; i++)
    {
        u64 ptr_val;

        if (
            copy_from_user_space(
                space,
                user_argv + (u64)i * 8,
                &ptr_val,
                8
            )
            != 0
        ) {
            elf_free_argv(argv_out, i);
            return -1;
        }

        if (ptr_val == 0)
        {
            *argc_out = i;
            return 0;
        }
        if (ptr_val > 0x00007FFFFFFFFFFFULL)
        {
            elf_free_argv(argv_out, i);
            return -1;
        }

        char tmp[EXEC_ARGSTR_MAX];
        int len = 0;

        for (; len < EXEC_ARGSTR_MAX - 1; len++)
        {
            char c;

            if (
                copy_from_user_space(
                    space,
                    ptr_val + (u64)len,
                    &c,
                    1
                )
                != 0
            ){
                elf_free_argv(argv_out, i);
                return -1;
            }

            tmp[len] = c;

            if (c == '\0') break;
        }

        tmp[len] = '\0';
        total += (u64)len + 1;

        if (total > EXEC_ARGS_TOTAL)
        {
            elf_free_argv(argv_out, i);
            return -1;
        }

        argv_out[i] = (char *)kmalloc((u64)len + 1);

        if (!argv_out[i])
        {
            elf_free_argv(argv_out, i);
            return -1;
        }

        memcpy(
            argv_out[i],
            tmp,
            (u64)len + 1
        );
    }

    elf_free_argv(argv_out, EXEC_ARGV_MAX);
    return -1;
}

void elf_free_argv(char *argv[EXEC_ARGV_MAX], int argc)
{
    for (int i = 0; i < argc; i++)
    {
        if (argv[i]) kfree((u64 *)argv[i]);
    }
}

static u64 setup_initial_stack(
    vmm_space_t *space,
    const char *prog_name,
    char **argv,
    int argc
)
{
    u64 hhdm = paging_get_hhdm_offset();
    u64 top = USER_STACK_TOP - 8;

    char *fallback[1];

    if (argc <= 0)
    {
        fallback[0] = (char *)prog_name;
        argv = fallback;
        argc = 1;
    }

    u64 cursor = top;
    u64 str_addrs[EXEC_ARGV_MAX];

    for (int i = argc - 1; i >= 0; i--)
    {
        u64 slen = 0;

        while (argv[i][slen]) slen++;

        slen++;

        cursor = (cursor - slen) & ~0xFULL;
        str_addrs[i] = cursor;

        u64 va = cursor;
        u64 remaining = slen;
        const char *src = argv[i];

        while (remaining > 0)
        {
            u64 page_va  = va & ~0xFFFULL;
            u64 page_off = va - page_va;
            u64 phys     = vmm_space_get_phys(space, page_va);
            if (!phys) return 0;

            u8 *dest  = (u8 *)(phys + hhdm + page_off);
            u64 chunk = 4096 - page_off;
            if (chunk > remaining) chunk = remaining;

            memcpy(dest, src, chunk);

            va += chunk;
            src += chunk;
            remaining -= chunk;
        }
    }

    u64 nvals = 1 + (u64)argc + 1 + 1 + 2;
    u64 vi = 0;

    u64 *vals = (u64 *)kmalloc(nvals * 8);
    if (!vals) return 0;

    vals[vi++] = (u64)argc;

    for (int i = 0; i < argc; i++) vals[vi++] = str_addrs[i];

    vals[vi++] = 0;
    vals[vi++] = 0;
    vals[vi++] = 0;
    vals[vi++] = 0;

    u64 sp = (cursor - nvals * 8) & ~0xFULL;

    u64 va = sp;
    u64 remaining = nvals * 8;
    u8 *src = (u8 *)vals;

    while (remaining > 0)
    {
        u64 page_va  = va & ~0xFFFULL;
        u64 page_off = va - page_va;
        u64 phys     = vmm_space_get_phys(space, page_va);

        if (!phys)
        {
            kfree((u64*)vals);
            return 0;
        }

        u8 *dest = (u8 *)(phys + hhdm + page_off);

        u64 chunk = 4096 - page_off;
        if (chunk > remaining) chunk = remaining;

        memcpy(dest, src, chunk);

        va += chunk;
        src += chunk;
        remaining -= chunk;
    }

    kfree((u64 *)vals);

    return sp;
}

static void kill_sibling_threads(proc_t *p, thread_t *caller)
{
    thread_t *t = p->threads;
    while (t)
    {
        thread_t *next = t->proc_next;

        if (t != caller && t->state != THREAD_DEAD)
        {
            t->state = THREAD_DEAD;
            if (p->alive_count > 0) p->alive_count--;

            sched_remove(t);
        }

        t = next;
    }
}

int elf_load(const u8 *data, u64 size, const char *name, u64 initial_caps, u64 *out_pid)
{
    if (!data || size == 0)
    {
        printf("[ELF] load '%s': null data or zero size\n", name ? name : "?");
        return -1;
    }

    if (elf_check(data, size) != 0) return -1;

    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)data;
    u64 hhdm = paging_get_hhdm_offset();

    #if DEBUGINFO
        printf(
            "[ELF] loading '%s': type=%u machine=0x%x entry=0x%llx phdrs=%u\n",
            name,
            (u32)eh->e_type,
            (u32)eh->e_machine,
            eh->e_entry,
            (u32)eh->e_phnum
        );
    #endif

    proc_t *p = process_create_user(name, initial_caps);
    proc_t *spawner = process_get_current();
    if (!p)
    {
        printf("[ELF] process_create_user failed\n");
        return -1;
    }
    if (out_pid) *out_pid = p->pid;
    if (spawner)
    {
        memcpy(p->fd_table, spawner->fd_table, sizeof(spawner->fd_table));

        for (int fd = 0; fd < FD_MAX; fd++)
        {
            if (
                p->fd_table[fd].used &&
                p->fd_table[fd].ofd >= 0
            ) openfile_ref(p->fd_table[fd].ofd);
        }
    }

    for (u16 i = 0; i < eh->e_phnum; i++)
    {
        const elf64_phdr_t *ph = (const elf64_phdr_t *)(data + eh->e_phoff + (u64)i * eh->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;

        #if DEBUGINFO
            printf(
                "[ELF] seg %u: vaddr=0x%llx filesz=%llu memsz=%llu flags=%c%c%c\n",
                (u32)i,
                ph->p_vaddr,
                ph->p_filesz,
                ph->p_memsz,
                (ph->p_flags & PF_R) ? 'R' : '-',
                (ph->p_flags & PF_W) ? 'W' : '-',
                (ph->p_flags & PF_X) ? 'X' : '-'
            );
        #endif

        if (ph->p_offset + ph->p_filesz > size)
        {
            printf(
                "[ELF] seg %u out of bounds (off=%llu filesz=%llu total=%llu)\n",
                (u32)i,
                ph->p_offset,
                ph->p_filesz,
                size
            );

            process_destroy(p);
            return -1;
        }

        if (ph->p_memsz < ph->p_filesz)
        {
            printf("[ELF] seg %u memsz < filesz\n", (u32)i);
            process_destroy(p);
            return -1;
        }

        u64 va_base      = align_down(ph->p_vaddr, 4096);
        u64 va_end       = align_up(ph->p_vaddr + ph->p_memsz, 4096);
        u64 pg_count     = (va_end - va_base) / 4096;

        u32 vmm_flags = VMM_REGION_USER | VMM_REGION_READ;

        if (ph->p_flags & PF_W) vmm_flags |= VMM_REGION_WRITE;
        if (ph->p_flags & PF_X) vmm_flags |= VMM_REGION_EXEC;

        u64 mapped = vmm_space_alloc(p->space, va_base, pg_count, vmm_flags);
        if (!mapped)
        {
            printf(
                "[ELF] seg %u vmm_space_alloc failed (va=0x%llx pages=%llu)\n",
                (u32)i,
                va_base,
                pg_count
            );
            process_destroy(p);
            return -1;
        }

        u64 file_src      = (u64)(data + ph->p_offset);
        u64 file_rem      = ph->p_filesz;
        u64 va_cursor     = ph->p_vaddr;
        u64 mem_rem       = ph->p_memsz;

        while (mem_rem > 0)
        {
            u64 page_va      = align_down(va_cursor, 4096);
            u64 page_off     = va_cursor - page_va;
            u64 phys         = vmm_space_get_phys(p->space, page_va);

            if (!phys)
            {
                printf("[ELF] seg %u get_phys failed va=0x%llx\n", (u32)i, page_va);
                process_destroy(p);
                return -1;
            }

            u8 *dest      = (u8 *)(phys + hhdm + page_off);
            u64 chunk     = 4096 - page_off;

            if (chunk > mem_rem) chunk = mem_rem;

            if (file_rem > 0)
            {
                u64 copy =      (chunk < file_rem) ? chunk : file_rem;
                memcpy(dest, (const void *)file_src, copy);
                file_src     += copy;
                file_rem     -= copy;
                dest         += copy;
                chunk        -= copy;
            }

            if (chunk > 0) memset(dest, 0, chunk);

            u64 step = 4096 - page_off;
            if (step > mem_rem) step = mem_rem;
            va_cursor += step;
            mem_rem -= step;
        }

        #if DEBUGINFO
            printf("[ELF] seg %u mapped va=0x%llx pages=%llu\n", (u32)i, va_base, pg_count);
        #endif
    }

    u64 stack = vmm_space_alloc(
        p->space,
        USER_STACK_BASE,
        USER_STACK_PAGES,
        VMM_REGION_USER | VMM_REGION_READ | VMM_REGION_WRITE
    );

    if (!stack)
    {
        printf(
            "[ELF] stack alloc failed (base=0x%llx pages=%llu)\n",
            USER_STACK_BASE,
            USER_STACK_PAGES
        );

        process_destroy(p);
        return -1;
    }

    #if DEBUGINFO
        printf(
            "[ELF] stack mapped: 0x%llx - 0x%llx\n",
            USER_STACK_BASE,
            USER_STACK_TOP
        );
    #endif

    u64 stack_top = setup_initial_stack(
        p->space,
        name,
        NULL,
        0
    );

    if (!stack_top)
    {
        process_destroy(p);
        return -1;
    }

    thread_t *t = thread_create_user(
        p,
        name,
        (thread_entry_t)eh->e_entry,
        NULL,
        stack_top
    );

    if (!t)
    {
        printf("[ELF] thread_create_user failed\n");
        process_destroy(p);
        return -1;
    }

    #if DEBUGINFO
        printf(
            "[ELF] launched '%s' entry=0x%llx stack_top=0x%llx\n",
            name,
            eh->e_entry,
            stack_top
        );

        log("[ELF]", "process scheduled\n", success);
    #endif
    return 0;
}

int elf_exec_replace(
    proc_t *p,
    cpu_state_t *state,
    const u8 *data,
    u64 size,
    const char *name,
    char *argv[EXEC_ARGV_MAX],
    int argc
) {
    if (!p || !state || !data || size == 0) return -1;
    if (elf_check(data, size) != 0) return -1;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)data;

    #if DEBUGINFO
        printf("[ELF] execve replacing '%s' image with '%s'\n", p->name, name ? name : "?");
    #endif

    thread_t *self = thread_get_current();
    kill_sibling_threads(p, self);

    vmm_region_t *cur = p->space->regions;
    while (cur)
    {
        u64 base = cur->base;
        cur = cur->next;
        vmm_space_free(p->space, base);
    }

    u64 entry = 0;
    if (elf_map_segments_and_stack(p, data, size, eh, &entry) != 0)
    {
        #if DEBUGINFO
            printf("[ELF] execve: mapping new image failed, address space is gone\n");
        #endif

        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return -1;
    }

    int i = 0;
    if (name)
    {
        while (name[i] && i < 63) { p->name[i] = name[i]; i++; }
    }
    p->name[i] = '\0';

    if (self)
    {
        int k  = 0;
        while (
            name[k] &&
            k < THREAD_NAME_MAX - 1
        ) {
            self->name[k] = name[k];
            k++;
        }
        self->name[k] = '\0';
    }

    p->heap_break = 0;

    u64 stack_top = setup_initial_stack(p->space, name, argv, argc);
    if (!stack_top)
    {
        #if DEBUGINFO
            printf("[ELF] execve, setup initial stack '%s'\n", name);
        #endif

        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return -1;
    }

    state->rip    = entry;
    state->rsp    = stack_top;
    state->rflags = 0x202;
    state->rax    = 0;
    state->rbx    = 0;
    state->rcx    = 0;
    state->rdx    = 0;
    state->rsi    = 0;
    state->rdi    = 0;
    state->rbp    = 0;
    state->r8     = 0;
    state->r9     = 0;
    state->r10    = 0;
    state->r11    = 0;
    state->r12    = 0;
    state->r13    = 0;
    state->r14    = 0;
    state->r15    = 0;

    signal_on_exec(p);

    #if DEBUGINFO
        log("[ELF]", "execve replaced process image\n", success);
    #endif

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");

    return 0;
}