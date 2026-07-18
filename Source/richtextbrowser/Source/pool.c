/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * pool.c - memory pool helpers
 */

#include "classbase.h"

APTR
poolAlloc(struct ClassBase *cb, struct localData *ld, ULONG size)
{
    (void)cb;
    if (ld == NULL || ld->ld_Pool == NULL)
        return NULL;
    return AllocPooled(ld->ld_Pool, size);
}

APTR
poolAllocClear(struct ClassBase *cb, struct localData *ld, ULONG size)
{
    APTR p;

    p = poolAlloc(cb, ld, size);
    if (p != NULL)
        memset(p, 0, size);
    return p;
}

STRPTR
poolStrDup(struct ClassBase *cb, struct localData *ld,
    CONST_STRPTR src, ULONG len)
{
    STRPTR dst;

    if (src == NULL)
        return NULL;
    if (len == 0)
    {
        while (src[len] != '\0')
            len++;
    }
    dst = (STRPTR)poolAlloc(cb, ld, len + 1);
    if (dst == NULL)
        return NULL;
    CopyMem((APTR)src, (APTR)dst, len);
    dst[len] = '\0';
    return dst;
}
