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
CP_DrawBulletText(struct ClassBase *cb, struct CanvasPlot *cp,
    WORD x, WORD y, CONST_STRPTR text, ULONG len, WORD fgPen)
{
    ULONG i;
    LONG penX;
    LONG adv;
    ULONG code;

    if (!s_DefaultFace.bf_Open || cp == NULL || cp->cp_RP == NULL ||
        text == NULL)
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
        /* Identity rotation: sin=0 cos=1.0 in 16.16 */
        if (!gleDrawGlyph(cb, &s_DefaultFace.bf_Ctx, cp->cp_RP, code,
                penX, (LONG)y, 0, 0x10000L, &adv))
        {
            /* Missing glyph — advance by em estimate */
            adv = s_DefaultFace.bf_Ctx.grc_EmWidth / 2;
            if (adv < 1)
                adv = 1;
        }
        penX += adv;
    }
    return TRUE;
}

WORD
CP_BulletTextWidth(struct ClassBase *cb, CONST_STRPTR text, ULONG len)
{
    ULONG i;
    LONG penX;
    LONG adv;
    ULONG code;
    struct RastPort rp;
    struct BitMap bm;

    if (!s_DefaultFace.bf_Open || text == NULL)
        return 0;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }

    /* Measure without visible blit: use a tiny CHIP bitmap RP */
    memset(&bm, 0, sizeof(bm));
    InitBitMap(&bm, 1, 8, 8);
    bm.Planes[0] = (PLANEPTR)AllocMem(8, MEMF_CHIP | MEMF_CLEAR);
    if (bm.Planes[0] == NULL)
        return (WORD)(len * (s_DefaultFace.bf_PointSize / 2 + 1));

    InitRastPort(&rp);
    rp.BitMap = &bm;

    penX = 0;
    for (i = 0; i < len; i++)
    {
        code = (ULONG)(UBYTE)text[i];
        adv = 0;
        if (!gleDrawGlyph(cb, &s_DefaultFace.bf_Ctx, &rp, code,
                0, 8, 0, 0x10000L, &adv))
        {
            adv = s_DefaultFace.bf_Ctx.grc_EmWidth / 2;
            if (adv < 1)
                adv = 1;
        }
        penX += adv;
    }

    FreeMem(bm.Planes[0], 8);
    (void)cb;
    return (WORD)penX;
}
