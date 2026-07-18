/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * cp_font.c - bullet.library text metrics/draw via glyphengine
 *
 * When an outline face is open (CP_BulletOpen), CP_DrawBulletText stamps
 * glyphs with BltTemplate. Otherwise callers use diskfont CP_DrawText.
 */

#include "classbase.h"
#include "glyphengine.h"

struct CP_BulletFace
{
    struct GlyphRenderCtx bf_Ctx;
    BOOL                  bf_Open;
    UWORD                 bf_PointSize;
};

static struct CP_BulletFace s_DefaultFace;

BOOL
CP_BulletOpen(struct ClassBase *cb, STRPTR otagPath, UWORD pointSize)
{
    if (s_DefaultFace.bf_Open)
        CP_BulletClose(cb);

    if (otagPath == NULL || pointSize == 0)
        return FALSE;

    if (!gleOpenFont(cb, &s_DefaultFace.bf_Ctx, otagPath,
            (LONG)pointSize, 72, 72))
        return FALSE;

    s_DefaultFace.bf_Open = TRUE;
    s_DefaultFace.bf_PointSize = pointSize;
    return TRUE;
}

void
CP_BulletClose(struct ClassBase *cb)
{
    if (!s_DefaultFace.bf_Open)
        return;
    gleCloseFont(cb, &s_DefaultFace.bf_Ctx);
    s_DefaultFace.bf_Open = FALSE;
    s_DefaultFace.bf_PointSize = 0;
}

BOOL
CP_BulletIsOpen(void)
{
    return s_DefaultFace.bf_Open;
}

BOOL
CP_DrawBulletTextFace(struct ClassBase *cb, struct CanvasPlot *cp,
    struct GlyphRenderCtx *ctx, WORD x, WORD y, CONST_STRPTR text, ULONG len,
    WORD fgPen)
{
    ULONG i;
    LONG penX;
    LONG adv;
    ULONG code;

    if (ctx == NULL || ctx->grc_Engine == NULL || cp == NULL ||
        cp->cp_RP == NULL || text == NULL)
        return FALSE;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }

    SetAPen(cp->cp_RP, fgPen);
    SetDrMd(cp->cp_RP, JAM1);

    penX = x;
    for (i = 0; i < len; i++)
    {
        code = (ULONG)(UBYTE)text[i];
        adv = 0;
        if (!gleDrawGlyph(cb, ctx, cp->cp_RP, code,
                penX, (LONG)y, 0, 0x10000L, &adv))
        {
            adv = ctx->grc_EmWidth / 2;
            if (adv < 1)
                adv = 1;
        }
        penX += adv;
    }
    return TRUE;
}

WORD
CP_BulletTextWidthFace(struct ClassBase *cb, struct GlyphRenderCtx *ctx,
    CONST_STRPTR text, ULONG len, WORD avgWidth)
{
    ULONG i;
    LONG penX;
    LONG adv;
    ULONG code;
    struct RastPort rp;
    struct BitMap bm;

    if (ctx == NULL || ctx->grc_Engine == NULL || text == NULL)
        return (WORD)(len * (avgWidth > 0 ? avgWidth : 8));
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }

    memset(&bm, 0, sizeof(bm));
    InitBitMap(&bm, 1, 8, 8);
    bm.Planes[0] = (PLANEPTR)AllocMem(8, MEMF_CHIP | MEMF_CLEAR);
    if (bm.Planes[0] == NULL)
        return (WORD)(len * (avgWidth > 0 ? avgWidth : 8));

    InitRastPort(&rp);
    rp.BitMap = &bm;

    penX = 0;
    for (i = 0; i < len; i++)
    {
        code = (ULONG)(UBYTE)text[i];
        adv = 0;
        if (!gleDrawGlyph(cb, ctx, &rp, code, 0, 8, 0, 0x10000L, &adv))
        {
            adv = ctx->grc_EmWidth / 2;
            if (adv < 1)
                adv = 1;
        }
        penX += adv;
    }

    FreeMem(bm.Planes[0], 8);
    (void)cb;
    return (WORD)penX;
}

BOOL
CP_DrawBulletText(struct ClassBase *cb, struct CanvasPlot *cp,
    WORD x, WORD y, CONST_STRPTR text, ULONG len, WORD fgPen)
{
    if (!s_DefaultFace.bf_Open)
        return FALSE;
    return CP_DrawBulletTextFace(cb, cp, &s_DefaultFace.bf_Ctx,
        x, y, text, len, fgPen);
}

WORD
CP_BulletTextWidth(struct ClassBase *cb, CONST_STRPTR text, ULONG len)
{
    if (!s_DefaultFace.bf_Open)
        return 0;
    return CP_BulletTextWidthFace(cb, &s_DefaultFace.bf_Ctx, text, len,
        (WORD)(s_DefaultFace.bf_PointSize / 2 + 1));
}
