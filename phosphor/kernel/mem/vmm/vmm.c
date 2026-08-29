/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: vmm.c
 *
 */

#include "vmm.h"
#include <kernel/mem/kheap/kheap.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/phys/physmem.h>
#include <kernel/mem/paging/paging.h>
#include <kernel/arch/hal/panic.h>
#include <kernel/arch/x86_64/exceptions/isr.h>
#include <kernel/arch/x86_64/exceptions/exception_handler.h>
#include <kernel/arch/hal/mmu.h>
#include <kernel/communication/serial.h>

#define PAGE_ALIGN_UP(x)   (((x) + 0xFFFULL) & ~0xFFFULL)
#define PAGE_ALIGN_DOWN(x) ((x) & ~0xFFFULL)

static vmm_region_t *vmm_head = NULL;

static u64 vmm_total        = 0;
static u64 vmm_used         = 0;
static u64 vmm_free_bytes   = 0;
static u64 vmm_regions      = 0;
static u64 vmm_used_regions = 0;

static vmm_space_t kernel_space;

static vmm_space_t *active_space = NULL;

static vmm_region_t *region_alloc(void)
{
    vmm_region_t *r = (vmm_region_t *)kmalloc(sizeof(vmm_region_t));
    if (!r) return NULL;
    memset(r, 0, sizeof(vmm_region_t));
    return r;
}

static void region_free(vmm_region_t *r)
{
    if (r) kfree((u64 *)r);
}

static void merge_with_next(vmm_region_t *r)
{
    if (!r || !r->next) return;
    vmm_region_t *nx = r->next;
    if ((r->flags & VMM_REGION_USED) || (nx->flags & VMM_REGION_USED)) return;
    if (r->base + r->size != nx->base) return;

    r->size += nx->size;
    r->next  = nx->next;
    if (nx->next) nx->next->prev = r;

    region_free(nx);
    vmm_regions--;
}

void vmm_init(void)
{
    vmm_region_t *cur = vmm_head;
    while (cur)
    {
        vmm_region_t *next = cur->next;
        region_free(cur);
        cur = next;
    }

    vmm_total        = VMM_LIMIT - VMM_BASE;
    vmm_used         = 0;
    vmm_free_bytes   = vmm_total;
    vmm_regions      = 1;
    vmm_used_regions = 0;

    vmm_head = region_alloc();
    if (!vmm_head) panic("VMM: failed to allocate initial region node");

    vmm_head->base  = VMM_BASE;
    vmm_head->size  = vmm_total;
    vmm_head->flags = VMM_REGION_FREE;
    vmm_head->prev  = NULL;
    vmm_head->next  = NULL;

    page_table_t *kpml4 = paging_get_kernel_pml4();
    if (!kpml4) panic("VMM: paging not initialised before vmm_init");

    u64 kpml4_virt = (u64)kpml4;
    u64 hhdm       = paging_get_hhdm_offset();

    kernel_space.pml4_phys    = kpml4_virt - hhdm;
    kernel_space.regions      = NULL;
    kernel_space.region_count = 0;
    kernel_space.used_virtual = 0;

    active_space = &kernel_space;
}

u64 vmm_alloc(u64 size, u32 flags)
{
    if (size == 0) return 0;

    size = PAGE_ALIGN_UP(size);

    vmm_region_t *cur = vmm_head;
    while (cur)
    {
        if (!(cur->flags & VMM_REGION_USED) && cur->size >= size)
        {
            u64 leftover = cur->size - size;

            if (leftover >= 0x1000)
            {
                vmm_region_t *split = region_alloc();
                if (!split) return 0;

                split->base  = cur->base + size;
                split->size  = leftover;
                split->flags = VMM_REGION_FREE;
                split->prev  = cur;
                split->next  = cur->next;

                if (cur->next) cur->next->prev = split;
                cur->next = split;
                vmm_regions++;
            }

            cur->size   = size;
            cur->flags  = VMM_REGION_USED | flags;

            vmm_used        += size;
            vmm_free_bytes  -= size;
            vmm_used_regions++;

            return cur->base;
        }
        cur = cur->next;
    }

    return 0;
}

void vmm_free(u64 base)
{
    if (!base) return;

    vmm_region_t *cur = vmm_head;
    while (cur)
    {
        if (cur->base == base && (cur->flags & VMM_REGION_USED))
        {
            vmm_used        -= cur->size;
            vmm_free_bytes  += cur->size;
            vmm_used_regions--;

            cur->flags = VMM_REGION_FREE;

            merge_with_next(cur);
            if (cur->prev) merge_with_next(cur->prev);

            return;
        }
        cur = cur->next;
    }
}

vmm_region_t *vmm_find(u64 base)
{
    vmm_region_t *cur = vmm_head;
    while (cur)
    {
        if (cur->base == base) return cur;
        cur = cur->next;
    }
    return NULL;
}

vmm_stats_t vmm_get_stats(void)
{
    vmm_stats_t s;
    s.total_virtual     = vmm_total;
    s.used_virtual      = vmm_used;
    s.free_virtual      = vmm_free_bytes;
    s.region_count      = vmm_regions;
    s.used_region_count = vmm_used_regions;
    return s;
}

u64 vmm_get_used(void)         { return vmm_used; }
u64 vmm_get_free(void)         { return vmm_free_bytes; }
u64 vmm_get_region_count(void) { return vmm_regions; }

vmm_space_t *vmm_space_create(void)
{
    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    vmm_space_t *space = (vmm_space_t *)kmalloc(sizeof(vmm_space_t));
    if (!space) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return NULL;
    }

    u64 pml4_phys = physmem_alloc_to(1);
    if (!pml4_phys)
    {
        kfree((u64 *)space);
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return NULL;
    }

    //printf("[VMM] new space PML4 frame=0x%llx\n", pml4_phys);

    u64 hhdm = paging_get_hhdm_offset();
    page_table_t *pml4_virt = (page_table_t *)(pml4_phys + hhdm);
    memset(pml4_virt, 0, PAGE_SIZE);

    page_table_t *kpml4 = paging_get_kernel_pml4();
    for (u64 i = 256; i < 512; i++)
        pml4_virt->entries[i] = kpml4->entries[i];

    space->pml4_phys    = pml4_phys;
    space->regions      = NULL;
    space->region_count = 0;
    space->used_virtual = 0;

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");

    return space;
}

void vmm_space_destroy(vmm_space_t *space)
{
    if (!space) return;
    if (space == &kernel_space) return;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    vmm_region_t *cur = space->regions;
    while (cur)
    {
        vmm_space_free(space, cur->base);
        cur = space->regions;
    }

    if (space->pml4_phys) physmem_free_to(space->pml4_phys, 1);

    if (vmm_get_active_space() == space)
    {
        arch_mmu_activate(kernel_space.pml4_phys);
        vmm_space_activate(&kernel_space);
    }

    kfree((u64 *)space);

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
}

vmm_space_t *vmm_get_kernel_space(void)
{
    return &kernel_space;
}

static u64 flags_to_pte(u32 flags)
{
    u64 pte = PTE_PRESENT;
    if (flags & VMM_REGION_WRITE)  pte |= PTE_WRITABLE;
    if (flags & VMM_REGION_USER)   pte |= PTE_USER;
    /* Do not set PTE_NO_EXEC (NX bit) to prevent false instruction-fetch page faults */
    return pte;
}

u64 vmm_space_alloc(vmm_space_t *space, u64 vaddr, u64 page_count, u32 flags)
{
    if (!space || page_count == 0) return 0;

    vaddr = PAGE_ALIGN_DOWN(vaddr);
    u64 size = page_count * PAGE_SIZE;

    vmm_region_t *node = region_alloc();
    if (!node) return 0;

    u64 pte_flags = flags_to_pte(flags);

    for (u64 i = 0; i < page_count; i++)
    {
        u64 phys = physmem_alloc_to(1);
        if (!phys)
        {
            for (u64 j = 0; j < i; j++)
            {
                u64 va = vaddr + j * PAGE_SIZE;
                paging_unmap_page_in(space->pml4_phys, va);
            }
            region_free(node);
            return 0;
        }

        paging_map_page_in(space->pml4_phys, vaddr + i * PAGE_SIZE, phys, pte_flags);
    }

    node->base  = vaddr;
    node->size  = size;
    node->flags = VMM_REGION_USED | flags;
    node->prev  = NULL;
    node->next  = NULL;

    vmm_region_t *cur  = space->regions;
    vmm_region_t *prev = NULL;

    while (cur && cur->base < vaddr)
    {
        prev = cur;
        cur  = cur->next;
    }

    node->prev = prev;
    node->next = cur;
    if (prev) prev->next = node;
    else      space->regions = node;
    if (cur)  cur->prev = node;

    space->region_count++;
    space->used_virtual += size;

    return vaddr;
}

void vmm_space_free(vmm_space_t *space, u64 vaddr)
{
    if (!space || !vaddr) return;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    //printf("[VMM_FREE_DBG] enter space=%p pml4_phys=0x%llx vaddr=0x%llx\n",
    //       (void*)space, space->pml4_phys, vaddr);

    vmm_region_t *cur = space->regions;
    while (cur)
    {
        if (cur->base == vaddr && (cur->flags & VMM_REGION_USED))
        {
            u64 page_count = cur->size / PAGE_SIZE;
            u64 hhdm       = paging_get_hhdm_offset();
            u8  is_mmio    = (cur->flags & VMM_REGION_MMIO) != 0;

            //printf("[VMM_FREE_DBG] found region base=0x%llx size=0x%llx pages=%llu is_mmio=%d hhdm=0x%llx\n",
            //       cur->base, cur->size, page_count, is_mmio, hhdm);

            for (u64 i = 0; i < page_count; i++)
            {
                u64 va = vaddr + i * PAGE_SIZE;

                u64 pml4_idx = (va >> 39) & 0x1FF;
                u64 pdp_idx  = (va >> 30) & 0x1FF;
                u64 pd_idx   = (va >> 21) & 0x1FF;
                u64 pt_idx   = (va >> 12) & 0x1FF;

                page_table_t *pml4 = (page_table_t *)(space->pml4_phys + hhdm);

                //printf("[VMM_FREE_DBG] i=%llu va=0x%llx pml4=%p pml4_idx=%llu\n",
                //       i, va, (void*)pml4, pml4_idx);

                if (!(pml4->entries[pml4_idx] & PTE_PRESENT)) {
                    printf("[VMM_FREE_DBG] pml4 entry not present, skip\n");
                    continue;
                }

                page_table_t *pdpt = (page_table_t *)((pml4->entries[pml4_idx] & 0x000FFFFFFFFFF000) + hhdm);

                //printf("[VMM_FREE_DBG] pdpt_phys=0x%llx pdpt_virt=%p pdp_idx=%llu\n",
                //       pml4->entries[pml4_idx] & 0x000FFFFFFFFFF000, (void*)pdpt, pdp_idx);

                if (!(pdpt->entries[pdp_idx] & PTE_PRESENT)) {
                    printf("[VMM_FREE_DBG] pdpt entry not present, skip\n");
                    continue;
                }
                if (pdpt->entries[pdp_idx] & PTE_HUGE) {
                    // 1GB huge page
                    continue;
                }

                page_table_t *pd = (page_table_t *)((pdpt->entries[pdp_idx] & 0x000FFFFFFFFFF000) + hhdm);

                if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
                    continue;
                }
                if (pd->entries[pd_idx] & PTE_HUGE) {
                    // 2MB huge page, do not dereference as a page table pointer!
                    if (!is_mmio) {
                        u64 phys = pd->entries[pd_idx] & 0x000FFFFFFFFFF000;
                        u32 rc = physmem_frame_rc_dec_and_get(phys);
                        if (rc == 0) {
                            physmem_frame_flags_set(phys, FRAME_FREE);
                            physmem_free_to(phys, 512); // 2MB = 512 frames
                        }
                    }
                    pd->entries[pd_idx] = 0;
                    __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
                    continue;
                }

                page_table_t *pt = (page_table_t *)((pd->entries[pd_idx] & 0x000FFFFFFFFFF000) + hhdm);

                //printf("[VMM_FREE_DBG] pt_phys=0x%llx pt_virt=%p pt_idx=%llu\n",
                //       pd->entries[pd_idx] & 0x000FFFFFFFFFF000, (void*)pt, pt_idx);

                if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
                    printf("[VMM_FREE_DBG] pt entry not present, skip\n");
                    continue;
                }

                if (!is_mmio)
                {
                    u64 phys = pt->entries[pt_idx] & 0x000FFFFFFFFFF000;
                    //printf("[VMM_FREE_DBG] about to dec rc for phys=0x%llx\n", phys);
                    u32 rc = physmem_frame_rc_dec_and_get(phys);
                   // printf("[VMM_FREE_DBG] rc after dec=%u\n", rc);
                    if (rc == 0)
                    {
                        physmem_frame_flags_set(phys, FRAME_FREE);
                        physmem_free_to(phys, 1);
                    }
                }

                pt->entries[pt_idx] = 0;
                __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");

                u8 pt_empty = 1;
                for (u64 k = 0; k < 512; k++)
                {
                    if (pt->entries[k] & PTE_PRESENT) { pt_empty = 0; break; }
                }
                if (pt_empty)
                {
                    u64 pt_phys = pd->entries[pd_idx] & 0x000FFFFFFFFFF000;
                    pd->entries[pd_idx] = 0;
                    physmem_free_to(pt_phys, 1);

                    u8 pd_empty = 1;
                    for (u64 k = 0; k < 512; k++)
                    {
                        if (pd->entries[k] & PTE_PRESENT) { pd_empty = 0; break; }
                    }
                    if (pd_empty)
                    {
                        u64 pd_phys = pdpt->entries[pdp_idx] & 0x000FFFFFFFFFF000;
                        pdpt->entries[pdp_idx] = 0;
                        physmem_free_to(pd_phys, 1);

                        u8 pdpt_empty = 1;
                        for (u64 k = 0; k < 512; k++)
                        {
                            if (pdpt->entries[k] & PTE_PRESENT) { pdpt_empty = 0; break; }
                        }
                        if (pdpt_empty)
                        {
                            u64 pdpt_phys = pml4->entries[pml4_idx] & 0x000FFFFFFFFFF000;
                            pml4->entries[pml4_idx] = 0;
                            physmem_free_to(pdpt_phys, 1);
                        }
                    }
                }
            }

            space->used_virtual -= cur->size;
            space->region_count--;

            if (cur->prev) cur->prev->next = cur->next;
            else           space->regions  = cur->next;
            if (cur->next) cur->next->prev = cur->prev;

            region_free(cur);
            goto done;
        }
        cur = cur->next;
    }

    printf("[VMM_FREE_DBG] vaddr not found in region list!\n");

done:
//printf("[VMM_FREE_DBG] leave\n");
    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
}

vmm_region_t *vmm_space_find(vmm_space_t *space, u64 vaddr)
{
    if (!space) return NULL;
    vmm_region_t *cur = space->regions;
    while (cur)
    {
        if (vaddr >= cur->base && vaddr < cur->base + cur->size)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

static u64 pte_lookup(vmm_space_t *space, u64 va)
{
    u64 hhdm     = paging_get_hhdm_offset();
    u64 pml4_idx = (va >> 39) & 0x1FF;
    u64 pdp_idx  = (va >> 30) & 0x1FF;
    u64 pd_idx   = (va >> 21) & 0x1FF;
    u64 pt_idx   = (va >> 12) & 0x1FF;

    page_table_t *pml4 = (page_table_t *)(space->pml4_phys + hhdm);
    if (!(pml4->entries[pml4_idx] & PTE_PRESENT)) return 0;

    page_table_t *pdpt = (page_table_t *)((pml4->entries[pml4_idx] & 0x000FFFFFFFFFF000) + hhdm);
    if (!(pdpt->entries[pdp_idx] & PTE_PRESENT)) return 0;

    page_table_t *pd = (page_table_t *)((pdpt->entries[pdp_idx] & 0x000FFFFFFFFFF000) + hhdm);
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) return 0;
    if (pd->entries[pd_idx] & PTE_HUGE) {
        return (pd->entries[pd_idx] & 0x000FFFFFFFFFF000) + (va & 0x1FFFFF);
    }

    page_table_t *pt = (page_table_t *)((pd->entries[pd_idx] & 0x000FFFFFFFFFF000) + hhdm);
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) return 0;

    return pt->entries[pt_idx] & 0x000FFFFFFFFFF000;
}

vmm_space_t *vmm_clone_space(vmm_space_t *src)
{
    if (!src) return NULL;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    vmm_space_t *dst = vmm_space_create();
    if (!dst) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return NULL;
    }

    u64 hhdm = paging_get_hhdm_offset();

    vmm_region_t *src_cur = src->regions;
    while (src_cur)
    {
        vmm_region_t *node = region_alloc();
        if (!node)
        {
            __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
            vmm_space_destroy(dst);
            return NULL;
        }

        node->base  = src_cur->base;
        node->size  = src_cur->size;
        node->flags = src_cur->flags;
        node->prev  = NULL;
        node->next  = NULL;

        u64 page_count = src_cur->size / PAGE_SIZE;
        u64 pte_flags  = flags_to_pte(node->flags);

        vmm_region_t *prev = NULL;
        vmm_region_t *cur  = dst->regions;
        while (cur && cur->base < node->base)
        {
            prev = cur;
            cur  = cur->next;
        }
        node->prev = prev;
        node->next = cur;
        if (prev) prev->next = node;
        else      dst->regions = node;
        if (cur)  cur->prev = node;

        dst->region_count++;
        dst->used_virtual += node->size;

        for (u64 i = 0; i < page_count; i++)
        {
            u64 va   = src_cur->base + i * PAGE_SIZE;
            u64 phys = pte_lookup(src, va);
            if (!phys)
            {
                region_free(node);
                __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
                vmm_space_destroy(dst);
                return NULL;
            }

            if (src_cur->flags & VMM_REGION_MMIO)
            {
                paging_map_page_in(dst->pml4_phys, va, phys, pte_flags);
                continue;
            }

            u64 child_phys = physmem_alloc_to(1);
            if (!child_phys)
            {
                __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
                vmm_space_destroy(dst);
                return NULL;
            }

            memcpy((void *)(child_phys + hhdm), (const void *)(phys + hhdm), PAGE_SIZE);
            paging_map_page_in(dst->pml4_phys, va, child_phys, pte_flags);
        }

        src_cur = src_cur->next;
    }

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");

    return dst;
}

void vmm_cow_break(vmm_space_t *space, u64 fault_addr)
{
    if (!space) return;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    fault_addr = PAGE_ALIGN_DOWN(fault_addr);

    vmm_region_t *region = vmm_space_find(space, fault_addr);
    if (!region) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return;
    }

    u64 hhdm     = paging_get_hhdm_offset();
    u64 pml4_idx = (fault_addr >> 39) & 0x1FF;
    u64 pdp_idx  = (fault_addr >> 30) & 0x1FF;
    u64 pd_idx   = (fault_addr >> 21) & 0x1FF;
    u64 pt_idx   = (fault_addr >> 12) & 0x1FF;

    page_table_t *pml4 = (page_table_t *)(space->pml4_phys + hhdm);
    if (!(pml4->entries[pml4_idx] & PTE_PRESENT)) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return;
    }

    page_table_t *pdpt = (page_table_t *)((pml4->entries[pml4_idx] & 0x000FFFFFFFFFF000) + hhdm);
    if (!(pdpt->entries[pdp_idx] & PTE_PRESENT)) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return;
    }

    page_table_t *pd = (page_table_t *)((pdpt->entries[pdp_idx] & 0x000FFFFFFFFFF000) + hhdm);
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return;
    }

    page_table_t *pt = (page_table_t *)((pd->entries[pd_idx] & 0x000FFFFFFFFFF000) + hhdm);
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return;
    }

    if (pt->entries[pt_idx] & PTE_WRITABLE) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return;
    }

    u64 old_phys = pt->entries[pt_idx] & 0x000FFFFFFFFFF000;

    u32 rc = physmem_frame_rc_dec_and_get(old_phys);

    if (rc == 0)
    {
        physmem_frame_flags_set(old_phys, FRAME_USED);

        u64 pte_flags = flags_to_pte(region->flags | VMM_REGION_WRITE);
        paging_map_page_in(space->pml4_phys, fault_addr, old_phys, pte_flags);

        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return;
    }

    u64 new_phys = physmem_alloc_to(1);
    if (!new_phys) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        panic("vmm_cow_break: out of physical memory");
    }

    void *old_virt = (void *)(old_phys + hhdm);
    void *new_virt = (void *)(new_phys + hhdm);
    memcpy(new_virt, old_virt, PAGE_SIZE);

    physmem_frame_flags_set(new_phys, FRAME_USED);

    u64 pte_flags = flags_to_pte(region->flags | VMM_REGION_WRITE);
    paging_map_page_in(space->pml4_phys, fault_addr, new_phys, pte_flags);

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
}

static void cow_page_fault_handler(cpu_state_t *state)
{
    u64 fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    u64 err     = state->err_code;
    int present = (err & (1 << 0)) != 0;
    int write   = (err & (1 << 1)) != 0;

    vmm_space_t *space = vmm_get_active_space();

    if (present && write && space)
    {
        vmm_region_t *region = vmm_space_find(space, fault_addr);
        if (region && (region->flags & VMM_REGION_COW))
        {
            vmm_cow_break(space, fault_addr);
            return;
        }
    }

    exception_handler(state);
}

void vmm_cow_install_handler(void)
{
    isr_register_handler(ISR_PAGE_FAULT, cow_page_fault_handler);
}

u64 vmm_map_phys(vmm_space_t *space, u64 vaddr, u64 phys_addr, u64 page_count, u32 flags)
{
    if (!space || page_count == 0) return 0;

    u64 saved_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(saved_flags) :: "memory");

    vaddr     = PAGE_ALIGN_DOWN(vaddr);
    phys_addr = PAGE_ALIGN_DOWN(phys_addr);

    u64 size = page_count * PAGE_SIZE;

    vmm_region_t *node = region_alloc();
    if (!node) {
        __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");
        return 0;
    }

    u64 pte_flags = flags_to_pte(flags);

    for (u64 i = 0; i < page_count; i++)
        paging_map_page_in(space->pml4_phys,
                           vaddr     + i * PAGE_SIZE,
                           phys_addr + i * PAGE_SIZE,
                           pte_flags);

    node->base  = vaddr;
    node->size  = size;
    node->flags = VMM_REGION_USED | VMM_REGION_MMIO | flags;
    node->prev  = NULL;
    node->next  = NULL;

    vmm_region_t *cur  = space->regions;
    vmm_region_t *prev = NULL;

    while (cur && cur->base < vaddr)
    {
        prev = cur;
        cur  = cur->next;
    }

    node->prev = prev;
    node->next = cur;
    if (prev) prev->next = node;
    else      space->regions = node;
    if (cur)  cur->prev = node;

    space->region_count++;
    space->used_virtual += size;

    __asm__ volatile("push %0; popfq" :: "r"(saved_flags) : "memory", "cc");

    return vaddr;
}

void vmm_unmap_phys(vmm_space_t *space, u64 vaddr)
{
    vmm_space_free(space, vaddr);
}

void vmm_space_activate(vmm_space_t *space)
{
    if (!space || !space->pml4_phys) return;

    arch_mmu_activate(space->pml4_phys);
    active_space = space;
}

vmm_space_t *vmm_get_active_space(void)
{
    return active_space;
}


u64 vmm_space_get_phys(vmm_space_t *space, u64 vaddr)
{
    if (!space) return 0;
    return pte_lookup(space, PAGE_ALIGN_DOWN(vaddr));
}
