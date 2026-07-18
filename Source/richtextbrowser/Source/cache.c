/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * cache.c - viewport bitmap cache (page.image / vector.image pattern)
 *
 * Heavy Text()/RectFill work runs into a private BitMap RastPort (no
 * Layer). GM_RENDER only BltBitMapRastPort onto the GIRPort. Matches
 * gfx/page cache.c and gfx/vector cache.c:
 *   AllocBitMap(w, h, 8, flags, NULL)  — never window friend
 *   InitRastPort + BitMap only          — Layer == NULL
 *   compose offscreen, then blit
 */

#include "classbase.h"

void
rtbFreeCache(struct ClassBase *cb, struct localData *ld)
{
    (void)cb;
    if (ld == NULL)
        return;
    if (ld->ld_CachedBM != NULL)
    {
        FreeBitMap(ld->ld_CachedBM);
        ld->ld_CachedBM = NULL;
    }
    ld->ld_CacheW = 0;
    ld->ld_CacheH = 0;
    ld->ld_CacheDirty = TRUE;
}

ULONG
rtbRebuildCache(struct ClassBase *cb, struct localData *ld,
    struct RastPort *friendRP, struct ColorMap *cm)
{
    struct BitMap *bm;
    struct RastPort rp;
    UWORD w;
    UWORD h;
    ULONG depth;

    (void)friendRP;

    if (ld == NULL)
        return 0;

    w = (UWORD)ld->ld_InnerWidth;
    h = (UWORD)ld->ld_InnerHeight;
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;

    if (!ld->ld_CacheDirty &&
        ld->ld_CachedBM != NULL &&
        ld->ld_CacheW == w &&
        ld->ld_CacheH == h &&
        ld->ld_CacheTop == ld->ld_Top)
        return 1;

    rtbFreeCache(cb, ld);

    /*
     * page/vector: friend=NULL, depth 8. No BMF_CLEAR — WaitBlit during
     * clear under OpenWindow layer locks can stall; paint fills the bg.
     */
    depth = 8;
    bm = NULL;
    if (GfxBase != NULL && GfxBase->lib_Version >= 39)
        bm = AllocBitMap((ULONG)w, (ULONG)h, depth, 0L, NULL);
    if (bm == NULL)
        return 0;

    InitRastPort(&rp);
    rp.BitMap = bm;

    /* Document paint in cache space (0,0)..(w,h) — no window Layer. */
    rtbPaintDocument(cb, ld, &rp, NULL, cm, 0, 0);

    ld->ld_CachedBM = bm;
    ld->ld_CacheW = w;
    ld->ld_CacheH = h;
    ld->ld_CacheTop = ld->ld_Top;
    ld->ld_CacheDirty = FALSE;
    return 1;
}

void
rtbBlitCache(struct ClassBase *cb, struct localData *ld,
    struct RastPort *dest)
{
    if (cb == NULL || ld == NULL || dest == NULL || ld->ld_CachedBM == NULL)
        return;
    if (ld->ld_CacheW < 1 || ld->ld_CacheH < 1)
        return;

    BltBitMapRastPort(ld->ld_CachedBM, 0, 0, dest,
        (LONG)ld->ld_InnerLeft, (LONG)ld->ld_InnerTop,
        (LONG)ld->ld_CacheW, (LONG)ld->ld_CacheH, 0xC0);
}

void
rtbRedrawGIRPort(struct ClassBase *cb, Class *cl, Object *o,
    struct GadgetInfo *gi)
{
    struct RastPort *rp;

    (void)cb;
    (void)cl;
    if (o == NULL || gi == NULL)
        return;

    /* TickBox pattern — never RefreshGList / win->RPort from the class. */
    rp = ObtainGIRPort(gi);
    if (rp != NULL)
    {
        DoMethod(o, GM_RENDER, gi, rp, GREDRAW_REDRAW);
        ReleaseGIRPort(rp);
    }
}
