/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * canvasplot.c - immediate-mode plotter (clip, fill, blit, diskfont text)
 *
 * Graphics pragmas expand GfxBase to cb->cb_GfxBase (see classbase.h), so
 * every call site that uses graphics/diskfont must have a local ClassBase *cb.
 * TTEngine draws only through CP_BeginTT / CP_DrawText on this plotter's RP.
 */

#include "classbase.h"

#ifdef TTENGINE_AVAILABLE
#include <libraries/ttengine.h>
#include <clib/ttengine_protos.h>
#include <pragma/ttengine_lib.h>
#endif

void
CP_Init(struct CanvasPlot *cp, struct ClassBase *cb)
{
    if (cp == NULL)
        return;
    memset(cp, 0, sizeof(*cp));
    cp->cp_CB = cb;
    cp->cp_FgPen = 1;
    cp->cp_BgPen = 0;
}

void
CP_SetTarget(struct CanvasPlot *cp, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm)
{
    if (cp == NULL)
        return;
    if (cp->cp_TTActive)
        CP_EndTT(cp);
    cp->cp_RP = rp;
    cp->cp_Window = win;
    cp->cp_ColorMap = cm;
    cp->cp_ClipDepth = 0;
}

void
CP_Done(struct CanvasPlot *cp)
{
    if (cp == NULL)
        return;
    CP_EndTT(cp);
    /*
     * Never CloseFont(cp_Font) — the gadget owns ld_Font and may borrow
     * the same pointer into the plotter (page pattern: one owner).
     */
    cp->cp_Font = NULL;
    cp->cp_RP = NULL;
}

BOOL
CP_PushClip(struct CanvasPlot *cp, WORD minx, WORD miny,
    WORD maxx, WORD maxy)
{
    struct Rectangle *r;
    struct Rectangle *prev;

    if (cp == NULL || cp->cp_RP == NULL)
        return FALSE;
    if (cp->cp_ClipDepth >= CP_CLIP_DEPTH)
        return FALSE;

    r = &cp->cp_ClipStack[cp->cp_ClipDepth];
    r->MinX = minx;
    r->MinY = miny;
    r->MaxX = maxx;
    r->MaxY = maxy;

    if (cp->cp_ClipDepth > 0)
    {
        prev = &cp->cp_ClipStack[cp->cp_ClipDepth - 1];
        if (r->MinX < prev->MinX)
            r->MinX = prev->MinX;
        if (r->MinY < prev->MinY)
            r->MinY = prev->MinY;
        if (r->MaxX > prev->MaxX)
            r->MaxX = prev->MaxX;
        if (r->MaxY > prev->MaxY)
            r->MaxY = prev->MaxY;
    }

    cp->cp_ClipDepth++;
    return TRUE;
}

void
CP_PopClip(struct CanvasPlot *cp)
{
    if (cp == NULL || cp->cp_ClipDepth == 0)
        return;
    cp->cp_ClipDepth--;
}

static BOOL
cp_clip_rect(struct CanvasPlot *cp, WORD *x1, WORD *y1, WORD *x2, WORD *y2)
{
    struct Rectangle *r;

    if (cp->cp_ClipDepth == 0)
        return TRUE;
    r = &cp->cp_ClipStack[cp->cp_ClipDepth - 1];
    if (*x1 < r->MinX)
        *x1 = r->MinX;
    if (*y1 < r->MinY)
        *y1 = r->MinY;
    if (*x2 > r->MaxX)
        *x2 = r->MaxX;
    if (*y2 > r->MaxY)
        *y2 = r->MaxY;
    if (*x1 <= *x2 && *y1 <= *y2)
        return TRUE;
    return FALSE;
}

static BOOL
cp_clip_visible(struct CanvasPlot *cp, WORD x, WORD y, WORD w, WORD h)
{
    struct Rectangle *r;
    WORD x2;
    WORD y2;

    if (cp->cp_ClipDepth == 0)
        return TRUE;
    r = &cp->cp_ClipStack[cp->cp_ClipDepth - 1];
    x2 = (WORD)(x + w - 1);
    y2 = (WORD)(y + h - 1);
    if (x2 < r->MinX || y2 < r->MinY || x > r->MaxX || y > r->MaxY)
        return FALSE;
    return TRUE;
}

void
CP_SetPens(struct CanvasPlot *cp, WORD fg, WORD bg)
{
    struct ClassBase *cb;

    if (cp == NULL)
        return;
    cb = cp->cp_CB;
    cp->cp_FgPen = fg;
    cp->cp_BgPen = bg;
    if (cp->cp_RP != NULL && cb != NULL)
    {
        SetAPen(cp->cp_RP, fg);
        SetBPen(cp->cp_RP, bg);
    }
}

void
CP_FillRect(struct CanvasPlot *cp, WORD x1, WORD y1, WORD x2, WORD y2)
{
    struct ClassBase *cb;

    if (cp == NULL || cp->cp_RP == NULL)
        return;
    cb = cp->cp_CB;
    if (cb == NULL)
        return;
    if (!cp_clip_rect(cp, &x1, &y1, &x2, &y2))
        return;
    SetAPen(cp->cp_RP, cp->cp_FgPen);
    SetDrMd(cp->cp_RP, JAM1);
    RectFill(cp->cp_RP, x1, y1, x2, y2);
}

void
CP_Rect(struct CanvasPlot *cp, WORD x1, WORD y1, WORD x2, WORD y2)
{
    struct ClassBase *cb;

    if (cp == NULL || cp->cp_RP == NULL)
        return;
    cb = cp->cp_CB;
    if (cb == NULL)
        return;
    if (!cp_clip_rect(cp, &x1, &y1, &x2, &y2))
        return;
    SetAPen(cp->cp_RP, cp->cp_FgPen);
    SetDrMd(cp->cp_RP, JAM1);
    Move(cp->cp_RP, x1, y1);
    Draw(cp->cp_RP, x2, y1);
    Draw(cp->cp_RP, x2, y2);
    Draw(cp->cp_RP, x1, y2);
    Draw(cp->cp_RP, x1, y1);
}

void
CP_Line(struct CanvasPlot *cp, WORD x1, WORD y1, WORD x2, WORD y2)
{
    struct ClassBase *cb;
    WORD minx;
    WORD miny;
    WORD maxx;
    WORD maxy;

    if (cp == NULL || cp->cp_RP == NULL)
        return;
    cb = cp->cp_CB;
    if (cb == NULL)
        return;
    minx = x1;
    maxx = x2;
    miny = y1;
    maxy = y2;
    if (minx > maxx)
    {
        minx = x2;
        maxx = x1;
    }
    if (miny > maxy)
    {
        miny = y2;
        maxy = y1;
    }
    if (!cp_clip_visible(cp, minx, miny,
        (WORD)(maxx - minx + 1), (WORD)(maxy - miny + 1)))
        return;
    SetAPen(cp->cp_RP, cp->cp_FgPen);
    SetDrMd(cp->cp_RP, JAM1);
    Move(cp->cp_RP, x1, y1);
    Draw(cp->cp_RP, x2, y2);
}

static void
cp_ensure_font(struct CanvasPlot *cp, struct CP_TextStyle *style)
{
    struct ClassBase *cb;
    struct TextAttr ta;

    cb = cp->cp_CB;
    if (cp->cp_Font != NULL || cb == NULL)
        return;

    /*
     * ROM OpenFont only — never OpenDiskFont here. Diskfont does DOS I/O;
     * calling it from GM_RENDER / measure under Intuition layer locks
     * hard-locks the machine (page/vector open fonts outside paint).
     */
    memset(&ta, 0, sizeof(ta));
    ta.ta_Name = (style != NULL && style->FontName != NULL)
        ? style->FontName : (STRPTR)"topaz.font";
    ta.ta_YSize = (style != NULL && style->Size != 0) ? style->Size : 8;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = FPF_ROMFONT;
    cp->cp_Font = OpenFont(&ta);
}

BOOL
CP_TextMetrics(struct CanvasPlot *cp, CONST_STRPTR text, ULONG len,
    struct CP_TextStyle *style, struct CP_TextExtent *out)
{
    struct ClassBase *cb;
    struct TextExtent te;
    struct RastPort *rp;

    if (out == NULL)
        return FALSE;
    out->Width = 0;
    out->Height = 8;
    out->Baseline = 6;

    if (cp == NULL || text == NULL)
        return FALSE;
    cb = cp->cp_CB;
    if (cb == NULL)
        return FALSE;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }

    rp = cp->cp_RP;
    cp_ensure_font(cp, style);
    if (cp->cp_Font == NULL)
        return FALSE;

    /*
     * TextExtent on a Layer RastPort under RefreshGList/ObtainGIRPort
     * deadlocks on some RTG systems. Only use it on Layer-less RPs
     * (offscreen cache). Otherwise cell-width estimate.
     */
    if (rp != NULL && rp->Layer == NULL)
    {
        SetFont(rp, cp->cp_Font);
        TextExtent(rp, (STRPTR)text, len, &te);
        out->Width = te.te_Width;
        out->Height = te.te_Height;
        out->Baseline = cp->cp_Font->tf_Baseline;
    }
    else
    {
        WORD cw;

        cw = cp->cp_Font->tf_XSize;
        if (cw < 1)
            cw = 8;
        out->Width = (WORD)(len * cw);
        out->Height = cp->cp_Font->tf_YSize;
        out->Baseline = cp->cp_Font->tf_Baseline;
    }
    return TRUE;
}

void
CP_DrawText(struct CanvasPlot *cp, WORD x, WORD y,
    CONST_STRPTR text, ULONG len, struct CP_TextStyle *style)
{
    struct ClassBase *cb;
    WORD fg;
    WORD bg;

    if (cp == NULL || cp->cp_RP == NULL || text == NULL)
        return;
    cb = cp->cp_CB;
    if (cb == NULL)
        return;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }
    if (len == 0)
        return;

    fg = (style != NULL) ? style->FgPen : cp->cp_FgPen;
    bg = (style != NULL) ? style->BgPen : -1;

    cp_ensure_font(cp, style);
    if (cp->cp_Font == NULL)
        return;

    /* Skip Text completely above/below software clip (Layer clip is backup). */
    {
        WORD th;

        th = cp->cp_Font->tf_YSize;
        if (th < 1)
            th = 8;
        if (!cp_clip_visible(cp, x, (WORD)(y - th), 1, (WORD)(th + 2)))
            return;
    }

    SetFont(cp->cp_RP, cp->cp_Font);
    SetAPen(cp->cp_RP, fg);
    if (bg >= 0)
    {
        SetBPen(cp->cp_RP, bg);
        SetDrMd(cp->cp_RP, JAM2);
    }
    else
    {
        SetDrMd(cp->cp_RP, JAM1);
    }
    Move(cp->cp_RP, x, y);
    Text(cp->cp_RP, (STRPTR)text, len);

    /* Underline via cell width — never TextExtent on a Layer RP. */
    if (style != NULL && (style->Style & RTBS_UNDERLINE) && cp->cp_Font != NULL)
    {
        WORD uw;
        WORD cw;

        cw = cp->cp_Font->tf_XSize;
        if (cw < 1)
            cw = 8;
        uw = (WORD)(len * cw);
        CP_SetPens(cp, fg, 0);
        CP_Line(cp, x, (WORD)(y + 1), (WORD)(x + uw), (WORD)(y + 1));
    }
}

void
CP_BltBitMap(struct CanvasPlot *cp, struct BitMap *src,
    WORD srcX, WORD srcY, WORD destX, WORD destY, WORD w, WORD h)
{
    struct ClassBase *cb;

    if (cp == NULL || cp->cp_RP == NULL || src == NULL || w <= 0 || h <= 0)
        return;
    cb = cp->cp_CB;
    if (cb == NULL)
        return;
    BltBitMapRastPort(src, srcX, srcY, cp->cp_RP, destX, destY, w, h,
        0xC0);
}

void
CP_BltMasked(struct CanvasPlot *cp, struct BitMap *src, PLANEPTR mask,
    WORD srcX, WORD srcY, WORD destX, WORD destY, WORD w, WORD h)
{
    if (cp == NULL || cp->cp_RP == NULL || src == NULL)
        return;
    (void)mask;
    CP_BltBitMap(cp, src, srcX, srcY, destX, destY, w, h);
}

BOOL
CP_BeginTT(struct CanvasPlot *cp)
{
    struct ClassBase *cb;

    if (cp == NULL || cp->cp_RP == NULL)
        return FALSE;
    cb = cp->cp_CB;
    if (cb == NULL || cb->cb_TTEngineBase == NULL)
        return FALSE;

#ifdef TTENGINE_AVAILABLE
    if (cp->cp_Window != NULL)
    {
        TT_SetAttrs(cp->cp_RP,
            TT_Window, (ULONG)cp->cp_Window,
            TT_ColorMap, (ULONG)cp->cp_ColorMap,
            TAG_DONE);
        cp->cp_TTActive = TRUE;
        return TRUE;
    }
#else
    (void)cb;
#endif
    return FALSE;
}

void
CP_EndTT(struct CanvasPlot *cp)
{
    struct ClassBase *cb;

    if (cp == NULL || !cp->cp_TTActive)
        return;
    cb = cp->cp_CB;
#ifdef TTENGINE_AVAILABLE
    if (cp->cp_RP != NULL && cb != NULL && TTEngineBase != NULL)
        TT_DoneRastPort(cp->cp_RP);
#else
    (void)cb;
#endif
    cp->cp_TTActive = FALSE;
}
