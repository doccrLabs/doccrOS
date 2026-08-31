/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: physmem.c
 * CREDITS: tsaraki
 *
 */

#include <kernel/mem/meminclude.h>

#include <limine/limine.h>
#include <kernel/arch/hal/panic.h>
#include <kernel/communication/serial.h>

extern u8 _kernel_start[];
extern u8 _kernel_end[];

#define BUDDY_MAX_ORDER 20 // 2^20f == 4GB single block
#define FREE_NONE ((u64)-1)
static struct physmem_pageframe *physmem_pageframes = NULL;
static u64 physmem_total = 0;
//static u64 physmem_free  = 0;
//static u64 physmem_used  = 0;
//static u8 *bitmap        = NULL;
//static u64 bitmap_size   = 0;

static u64 physmem_addr_highest = 0;
static u64 hhdm_off = 0;

static u8 *frame_order = NULL;

static u64 free_list_head[BUDDY_MAX_ORDER + 1];
static u64 physmem_free_frames = 0;

static inline u64 *free_slot_ptr(u64 frame)
{
    return (u64 *)(hhdm_off + frame * PAGE_SIZE);
}
static u64 buddy_list_pop(int order)
{
    u64 frame = free_list_head[order];
    if (frame == FREE_NONE) return FREE_NONE;

    free_list_head[order] = *free_slot_ptr(frame);
    return frame;
}
static void buddy_list_push(int order, u64 frame)
{
    if (frame >= physmem_total) panic("physmem, buddy: frame out of range");

    *free_slot_ptr(frame) = free_list_head[order];
    free_list_head[order] = frame;
    frame_order[frame] = (u8)order;
}
static int buddy_list_remove(int order, u64 target)
{
    u64 cur = free_list_head[order];
    u64 prev = FREE_NONE;

    while (cur != FREE_NONE)
    {
        if (cur == target)
        {
            u64 next = *free_slot_ptr(cur);

            if (prev == FREE_NONE) free_list_head[order] = next;
            else *free_slot_ptr(prev) = next;

            return 1;
        }

        prev = cur;
        cur = *free_slot_ptr(cur);
    }

    return 0;
}
static void buddy_free_range(u64 start, u64 len)
{
    u64 frame = start;
    u64 remaining = len;

    while (remaining > 0)
    {
        int order = 0;

        while (order < BUDDY_MAX_ORDER)
        {
            u64 next_size = 1ULL << (order + 1);

            if ((frame % next_size) != 0) break;
            if (next_size > remaining) break; // doesnt fit

            order++;
        }

        buddy_list_push(order, frame);

        u64 sz = 1ULL << order;
        physmem_free_frames += sz;

        frame += sz;
        remaining -= sz;
    }
}
static u64 buddy_alloc_order(int order)
{
    for (int o = order; o <= BUDDY_MAX_ORDER; o++)
    {
        if (free_list_head[o] == FREE_NONE) continue;

        u64 frame = buddy_list_pop(o);

        while (o > order)
        {
            o--;
            u64 buddy = frame + (1ULL << o);
            buddy_list_push(o, buddy);
        }

        frame_order[frame] = (u8)order;
        return frame;
    }

    return FREE_NONE;
}

/// Summary
/// 2025/11/17 tsaraki
/// 2026/08/30 emexos / emexthecat
/// marking all tre memory that hardware is providing
/// and limine mapped
static void physmem_addr_mark(limine_memmap_response_t *mpr) {

    if (!physmem_pageframes) {
        panic("PHYSMEM ADDR MARK PHYSMEM PAGEFRAMES NULL");
        return;
    }

    for (u64 i = 0; i < mpr->entry_count; i++) {
        struct limine_memmap_entry *entry = mpr->entries[i];

        u64 frame_start = entry->base / PAGE_SIZE;
        if (frame_start >= physmem_total) continue;

        u64 frame_end = (entry->base + entry->length) / PAGE_SIZE;

        if (frame_end > physmem_total) frame_end = physmem_total;

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            for (u64 frame = frame_start; frame < frame_end; frame++) {
                //bitmap_clear(frame);
                physmem_pageframes[frame].rc = 0;
                physmem_pageframes[frame].flags = FRAME_FREE;
            }
        } else {
            for (u64 frame = frame_start; frame < frame_end; frame++) {
                //bitmap_set(frame);
                physmem_pageframes[frame].rc = 1;
                physmem_pageframes[frame].flags = FRAME_USED | FRAME_KERNEL;
            }
        }
    }
}

void physmem_addr_mark_used(u64 physmem_addr, u64 count) {

    if (!physmem_pageframes) {
        panic("PHYSMEM ADDR MARK USED PHYSMEM PAGEFRAMES NULL");
        return;
    }

    u64 frame_start = physmem_addr / PAGE_SIZE;
    u64 frame_end = frame_start + count;

    if (frame_end > physmem_total) frame_end = physmem_total;

    for (u64 frame = frame_start; frame < frame_end; frame++) {
        //bitmap_set(frame);

        physmem_pageframes[frame].rc = 1;
        physmem_pageframes[frame].flags = FRAME_USED;
    }
}

void physmem_addr_mark_free(u64 physmem_addr, u64 count) {

    if (!physmem_pageframes) {
        panic("PHYSMEM ADDR MARK FREE PHYSMEM PAGEFRAMES NULL");
        return;
    }

    u64 frame_start = physmem_addr / PAGE_SIZE;
    u64 frame_end = frame_start + count;

    if (frame_end > physmem_total) frame_end = physmem_total;

    for (u64 frame = frame_start; frame < frame_end; frame++) {
        //bitmap_clear(frame);

        physmem_pageframes[frame].rc = 0;
        physmem_pageframes[frame].flags = FRAME_FREE;
    }
}


/// Summary
/// 2025/11/17 tsaraki
/// this is the first addres that the kernel is getting
/// should be used once in physmem_init
void *physmem_addr_get_tracking(
    limine_memmap_response_t *mpr,
    limine_hhdm_response_t *hpr,
    u64 size
) {
    if (!mpr) panic("PHYSMEM ADDR GET LIMINE MEMMAP EQ NULL\n");
    if (!hpr) panic("PHYSMEM ADDR GET LIMINE HHDM EQ NULL\n");

    for (u64 i = 0; i < mpr->entry_count; i++) {
        struct limine_memmap_entry *entry = mpr->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE &&
            entry->length >= size &&
            entry->base >= 0x1000000) {

            return (void *)entry->base;
        }
    }

    for (u64 i = 0; i < mpr->entry_count; i++) {
        struct limine_memmap_entry *entry = mpr->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE &&
            entry->length >= size &&
            entry->base >= 0x100000) {
            return (void *)entry->base;
        }
    }

    for (u64 i = 0; i < mpr->entry_count; i++) {
        struct limine_memmap_entry *entry = mpr->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE &&
            entry->length >= size) {
            return (void *)entry->base;
        }
    }

    /// 2025/11/17 tsaraki
    // panic in physmem_init
    return NULL;
}

/// Summary
/// 2025/11/17/// 2025/11/17 tsaraki
/// helper, used once in physmem_init
u64 used_bytes_to_frame_count(u64 size)  {
    return (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

/// Summary
/// 2025/11/17 tsaraki
/// 2026/08/30 emexos / emexthecat
/// function that setup a physical memory
/// have to be called once in _start() kernel.c
void physmem_init(limine_memmap_response_t *mpr, limine_hhdm_response_t *hpr) {
    physmem_addr_highest = 0;
    hhdm_off = hpr->offset;

    for (u64 i = 0; i < mpr->entry_count; i++)
    {
        struct limine_memmap_entry *entry = mpr->entries[i];

        if (
            entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE
        ){

            u64 end_addr = entry->base + entry->length;

            if (end_addr > physmem_addr_highest)
            {
                physmem_addr_highest = end_addr;
            }
        }
    }

    physmem_total = physmem_addr_highest / PAGE_SIZE;

    u64 pageframes_size = physmem_total * sizeof(physmem_pageframe_t);
    u64 order_arr_size = physmem_total * sizeof(u8);

    void *tracking_phys = physmem_addr_get_tracking(mpr, hpr, pageframes_size + order_arr_size);
    if (!tracking_phys) panic("No usable memory found for physmem tracking structure");

    physmem_pageframes = (physmem_pageframe_t *)((u64)tracking_phys + hpr->offset);
    frame_order = (u8 *)((u64)tracking_phys + hpr->offset + pageframes_size);

    memset(frame_order, 0, order_arr_size);

    for (u64 i = 0; i < physmem_total; i++)
    {
        physmem_pageframes[i].rc = 1;
        physmem_pageframes[i].flags = FRAME_USED | FRAME_KERNEL;
    }

    physmem_addr_mark(mpr);

    for (u64 i = 0; i < mpr->entry_count; i++)
    {
        struct limine_memmap_entry *entry = mpr->entries[i];
        printf(
            "[MEMMAP] base=0x%llx len=0x%llx type=%llu\n",
            entry->base,
            entry->length,
            (u64)entry->type
        );
    }

    printf(
        "[PHYSMEM] kernel_start=%p kernel_end=%p (phys via memmap types above)\n",
        (void*)_kernel_start,
        (void*)_kernel_end
    );

    u64 to_used = used_bytes_to_frame_count(pageframes_size + order_arr_size);
    physmem_addr_mark_used((u64)tracking_phys, to_used);

    physmem_addr_mark_used(0, 256); // 256 * 4KB = 1 MB

    for (int i = 0; i <= BUDDY_MAX_ORDER; i++) free_list_head[i] = FREE_NONE;
    physmem_free_frames = 0;

    u64 frame = 0;
    while (frame < physmem_total)
    {
        if (physmem_pageframes[frame].flags & FRAME_USED)
        {
            frame++;
            continue;
        }

        u64 run_start = frame;
        while (frame < physmem_total && !(physmem_pageframes[frame].flags & FRAME_USED)) frame++;

        buddy_free_range(run_start, frame - run_start);
    }

    printf("[PHYSMEM] buddy allocator ready, %llu frames free\n", physmem_free_frames);
}

/// Summary
/// 2025/11/17 tsaraki
/// 2026/08/30 emexos / emexthecat
/// @Note @Important @Behavior
/// rounds up the count of 2
u64 physmem_alloc_to(u64 count) { //count is len of frames of size 4096
    if (count == 0) return 0;

    int order = 0;
    u64 sz = 1;

    while (sz < count)
    {
        sz <<= 1;
        order++;
    }

    if (order > BUDDY_MAX_ORDER) return 0;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    u64 frame = buddy_alloc_order(order);
    u64 result = 0;

    if (frame != FREE_NONE) {
        result = frame * PAGE_SIZE;
        physmem_addr_mark_used(result, 1ULL << order);
        physmem_free_frames -= (1ULL << order);
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");

    return result;
}

/// Summary
/// 2025/11/17 tsaraki
/// 2026/08/30 emexos / emexthecat
void physmem_free_to(u64 physmem_addr, u64 count) { //count is len of frames of size 4096
    if (count == 0) return;

    u64 frame = physmem_addr / PAGE_SIZE;
    if (frame >= physmem_total) return;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    int order = frame_order[frame];
    u64 blk_size = 1ULL << order;

    physmem_addr_mark_free(physmem_addr, blk_size);
    physmem_free_frames += blk_size;

    while (order < BUDDY_MAX_ORDER)
    {
        u64 buddy = frame ^ (1ULL << order);
        if (buddy >= physmem_total) break;
        if (physmem_pageframes[buddy].flags & FRAME_USED) break;
        if (frame_order[buddy] != order) break;
        if (!buddy_list_remove(order, buddy)) break;

        frame = (frame < buddy) ? frame : buddy;
        order++;
        frame_order[frame] = (u8)order;
    }

    buddy_list_push(order, frame);

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
}

/// Summary
/// 2025/11/17 tsaraki
/// 2026/08/30 emexos / emexthecat
/// helper stat
u64 physmem_free_get(void) {
    return physmem_free_frames;
}

void physmem_frame_rc_inc(u64 phys_addr) {
    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    u64 frame = phys_addr / PAGE_SIZE;
    if (frame < physmem_total) {
        physmem_pageframes[frame].rc++;
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
}

u32 physmem_frame_rc_dec_and_get(u64 phys_addr) {
    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    u64 frame = phys_addr / PAGE_SIZE;
    u32 result = 0;
    if (frame < physmem_total) {
        if (physmem_pageframes[frame].rc > 0)
            physmem_pageframes[frame].rc--;
        result = physmem_pageframes[frame].rc;
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
    return result;
}

u32 physmem_frame_flags_get(u64 phys_addr) {
    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    u64 frame = phys_addr / PAGE_SIZE;
    u32 result = 0;
    if (frame < physmem_total) {
        result = physmem_pageframes[frame].flags;
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
    return result;
}

void physmem_frame_flags_set(u64 phys_addr, u32 flags) {
    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    u64 frame = phys_addr / PAGE_SIZE;
    if (frame < physmem_total) {
        physmem_pageframes[frame].flags = flags;
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
}