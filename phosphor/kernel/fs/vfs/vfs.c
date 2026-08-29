/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: vfs.c
 *
 */

#include "vfs.h"
#include <kernel/mem/meminclude.h>
#include <kernel/screen/lib/string.h>
#include <kernel/screen/lib/print.h>
#include <kernel/communication/serial.h>

static vfs_node_t *vfs_root = NULL;

static vfs_node_t *node_alloc(void)
{
    vfs_node_t *n = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!n) return NULL;

    memset(n, 0, sizeof(vfs_node_t));
    return n;
}

void vfs_init(void)
{
    vfs_root = node_alloc();

    if (!vfs_root)
    {
        log("[VFS]", "could not allocate root node, rip\n");
        return;
    }

    str_copy(vfs_root->name, ROOT);
    vfs_root->type = VFS_DIRECTORY;
    vfs_root->parent = NULL;
    vfs_root->child_count = 0;

    log("[VFS]", "initialised vfs\n");
    //printf("root = %p\n", vfs_root);
}

vfs_node_t *vfs_get_root(void)
{
    return vfs_root;
}

vfs_node_t *vfs_create_node(vfs_node_t *parent, const char *name, vfs_type_t type)
{
    vfs_node_t *existing = vfs_find_child(parent, name);
    vfs_node_t *n = node_alloc();

    if (!parent || !name || name[0] == '\0') return NULL;
    if (parent->type != VFS_DIRECTORY) return NULL;
    if (existing) return existing;
    if (parent->child_count >= VFS_MAX_CHILDREN) return NULL;
    if (!n) return NULL;

    str_copy(n->name, name);

    n->type     = type;
    n->parent      = parent;
    n->child_count = 0;
    n->data     = NULL;
    n->size     = 0;
    n->capacity = 0;
    n->device   = NULL;

    parent->children[parent->child_count++] = n;
/*
    printf(
        "p=%p cc=%d cn=%s\n",
        parent,
        parent->child_count,
        name
    );*/

    return n;
}

vfs_node_t *vfs_find_child(vfs_node_t *dir, const char *name)
{
    if (!dir || !name) return NULL;

    for (int i = 0; i < dir->child_count; i++)
    {
        if (str_equals(dir->children[i]->name, name))
        {
            return dir->children[i];
        }
    }
    return NULL;
}

vfs_node_t *vfs_find(const char *path)
{
    if (!vfs_root || !path) return NULL;
    if (path[0] == '\0' || str_equals(path, "/")) return vfs_root;

    vfs_node_t *cur = vfs_root;
    const char *p = path;
    char token[VFS_NAME_MAX];

    while (*p == '/') p++; // leading slashes are just noise

    while (*p)
    {
        int i = 0;

        while (*p && *p != '/' && i < VFS_NAME_MAX - 1) {
            token[i++] = *p++;
        }
        token[i] = '\0';
        if (i == 0) break;

        vfs_node_t *next = vfs_find_child(cur, token);
        if (!next) return NULL; // dead end, sorry

        cur = next;
        while (*p == '/') p++;
    }

    return cur;
}

vfs_node_t *vfs_mkdir(const char *path)
{
    if (!vfs_root || !path) return NULL;
    if (path[0] == '\0' || str_equals(path, "/")) return vfs_root;

    vfs_node_t *cur = vfs_root;

    const char *p = path;
    char token[VFS_NAME_MAX];

    while (*p == '/') p++;
    while (*p)
    {
        int i = 0;

        while (*p && *p != '/' && i < VFS_NAME_MAX - 1)
        {
            token[i++] = *p++;
        }
        token[i] = '\0';
        if (i == 0) break;

        vfs_node_t *next = vfs_find_child(cur, token);
        if (!next)
        {
            next = vfs_create_node(cur, token, VFS_DIRECTORY); // mkdir -p energy
            if (!next) return NULL;
        } else if (next->type != VFS_DIRECTORY)
        {
            return NULL; // somebody put a file where a folder should be, nope
        }

        cur = next;
        while (*p == '/') p++;
    }

    return cur;
}

void vfs_split_path(const char *path, char *dirpath_out, char *fname_out)
{
    int len = str_len(path);
    int last_slash = -1;

    for (int i = 0; i < len; i++)
    {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash < 0)
    {
        str_copy(dirpath_out, "/");
        str_copy(fname_out, path);
    } else if (last_slash == 0)
    {
        str_copy(dirpath_out, "/");
        str_copy(fname_out, path + 1);
    } else {
        int i = 0;
        for (; i < last_slash && i < VFS_MAX_PATH - 1; i++) dirpath_out[i] = path[i];
        dirpath_out[i] = '\0';
        str_copy(fname_out, path + last_slash + 1);
    }
}

int vfs_remove(const char *path)
{
    vfs_node_t *node = vfs_find(path);
    if (!node || node == vfs_root) return -1; // no deleting the root if no permission

    vfs_node_t *parent = node->parent;
    if (!parent) return -1;

    if (
        node->type == VFS_DIRECTORY &&
        node->child_count > 0
    ){
        return -1;
    }

    int idx = -1;
    for (
        int i = 0;
        i < parent->child_count;
        i++) {
        if (
            parent->children[i] == node
        ) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -1;


    for (
        int i = idx;
        i < parent->child_count - 1;
        i++
        ) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->child_count--;


    if (node->data && !node->borrowed) kfree((u64 *)node->data);
    kfree((u64 *)node);

    return 0;
}

int vfs_rename(const char *oldpath, const char *newpath)
{
    if (!vfs_root || !oldpath || !newpath) return -1;
    if (oldpath[0] == '\0' || newpath[0] == '\0') return -1;
    vfs_node_t *node = vfs_find(oldpath);

    if (!node || node == vfs_root) return -1;

    char old_dirpath[VFS_MAX_PATH];
    char old_name[VFS_NAME_MAX];
    char new_dirpath[VFS_MAX_PATH];
    char new_name[VFS_NAME_MAX];

    vfs_split_path(oldpath, old_dirpath, old_name);
    vfs_split_path(newpath, new_dirpath, new_name);

    if (old_name[0] == '\0' || new_name[0] == '\0') return -1;

    vfs_node_t *old_parent = vfs_find(old_dirpath);
    vfs_node_t *new_parent = vfs_find(new_dirpath);

    if (!old_parent || !new_parent) return -1;
    if (old_parent->type != VFS_DIRECTORY || new_parent->type != VFS_DIRECTORY) return -1;
    if (new_parent->child_count >= VFS_MAX_CHILDREN) return -1;
    if (vfs_find_child(new_parent, new_name)) return -1;

    if (node->type == VFS_DIRECTORY)
    {
        vfs_node_t *check = new_parent;

        while (check)
        {
            if (check == node) return -1;

            check = check->parent;
        }
    }

    int old_index = -1;

    for (int i = 0; i < old_parent->child_count; i++)
    {
        if (old_parent->children[i] == node)
        {
            old_index = i;
            break;
        }
    }

    if (old_index < 0) return -1;

    for (int i = old_index; i < old_parent->child_count - 1; i++)
    {
        old_parent->children[i] = old_parent->children[i + 1];
    }

    old_parent->child_count--;

    str_copy(node->name, new_name);
    node->parent = new_parent;

    new_parent->children[new_parent->child_count++] = node;

    return 0;
}

void vfs_list(vfs_node_t *dir)
{
    if (!dir) return;

    if (dir->type != VFS_DIRECTORY)
    {
        print(dir->name, white());
        print("\n", white());
        return;
    }

    for (int i = 0; i < dir->child_count; i++)
    {
        vfs_node_t *c = dir->children[i];

        if (c->type == VFS_DIRECTORY)
        {
            print(c->name, cyan());
            print("/\n", cyan());
        } else if (c->type == VFS_DEVICE)
        {
            print(c->name, yellow());
            print("\n", yellow());
        } else
        {
            print(c->name, white());
            print("\n", white());
        }
    }
}
