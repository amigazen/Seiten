/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * render.c - GM_RENDER path: paint visible blocks into window RastPort
 *
 * Never allocates a full-document CHIP SuperBitMap. Paints only the
 * viewport (plus Overscan) through the canvas plotter.
 */

#include "classbase.h"
#include <images/bitmap.h>

static void
rtb_paint_run_text(struct ClassBase *cb, struct localData *ld,
    struct RtbRun *run, WORD originX, WORD originY, WORD colW,
    BOOL isLink)
{
    CONST_STRPTR text;
    ULONG len;
    struct CP_TextStyle style;
    WORD lineH;
    WORD cw;
    WORD x;
    WORD y;
    WORD fg;
    ULONG pos;
    ULONG chunk;

    (void)cb;
    (void)colW;

    text = NULL;
    len = 0;
    if (isLink)
    {
        text = run->rr_Data.link.text;
        len = run->rr_Data.link.byteLen;
        fg = ld->ld_LinkPen;
    }
    else
    {
        text = run->rr_Data.text.text;
        len = run->rr_Data.text.byteLen;
        fg = ld->ld_TextPen;
    }
    if (run->rr_Fg != (ULONG)~0)
        fg = (WORD)run->rr_Fg;
    if (text == NULL || len == 0)
        return;

    memset(&style, 0, sizeof(style));
    style.FgPen = fg;
    style.BgPen = -1;
    style.Style = isLink ? RTBS_UNDERLINE : run->rr_Style;
    style.Size = 8;

    lineH = (ld->ld_Font != NULL) ? (ld->ld_Font->tf_YSize + 2) : 10;
    cw = 8;
    if (ld->ld_Font != NULL && ld->ld_Font->tf_XSize > 0)
        cw = ld->ld_Font->tf_XSize;

    /*
     * Never TextExtent/TextLength here — cp_RP is the window Layer during
     * GM_RENDER and those calls deadlock under RefreshGList on RTG.
     * Emit fixed-size chunks with Text() only (positions from measure).
     */
    x = originX;
    y = (WORD)(originY + lineH - 3);
    pos = 0;
    chunk = 40;
    if (colW > cw)
        chunk = (ULONG)(colW / cw);
    if (chunk < 1)
        chunk = 1;
    if (chunk > 80)
        chunk = 80;

    while (pos < len)
    {
        ULONG n;

        n = len - pos;
        if (n > chunk)
            n = chunk;
        CP_DrawText(&ld->ld_Plot, x, y, text + pos, n, &style);
        pos += n;
        if (pos < len)
        {
            y = (WORD)(y + lineH);
            x = originX;
        }
    }
}

static void
rtb_paint_image_bm(struct ClassBase *cb, struct localData *ld,
    Object *bmObj, struct BitMap *bm, WORD x, WORD y, WORD w, WORD h)
{
    struct BitMap *src;

    (void)cb;
    src = bm;
    if (src == NULL && bmObj != NULL)
        GetAttr(BITMAP_BitMap, bmObj, (ULONG *)&src);
    if (src == NULL)
    {
        /* Placeholder rectangle */
        CP_SetPens(&ld->ld_Plot, ld->ld_ShadowPen, 0);
        CP_Rect(&ld->ld_Plot, x, y, (WORD)(x + w - 1), (WORD)(y + h - 1));
        return;
    }
    CP_BltBitMap(&ld->ld_Plot, src, 0, 0, x, y, w, h);
}

static void
rtb_paint_block(struct ClassBase *cb, struct localData *ld,
    struct RtbBlock *blk, WORD originX, WORD originY, WORD colW);

static void
rtb_paint_flow(struct ClassBase *cb, struct localData *ld,
    struct RtbBlock *blk, WORD originX, WORD originY, WORD colW)
{
    struct RtbRun *run;

    for (run = (struct RtbRun *)blk->rb_Data.flow.runs.mlh_Head;
        run->rr_Node.mln_Succ != NULL;
        run = (struct RtbRun *)run->rr_Node.mln_Succ)
    {
        WORD rx;
        WORD ry;

        rx = (WORD)(originX + run->rr_X);
        ry = (WORD)(originY + run->rr_Y);

        if (run->rr_Kind == RTBR_IMAGE)
        {
            rtb_paint_image_bm(cb, ld, run->rr_Data.image.bitmapObj,
                run->rr_Data.image.bm, rx, ry,
                run->rr_W > 0 ? run->rr_W : 48,
                run->rr_H > 0 ? run->rr_H : 48);
        }
        else if (run->rr_Kind == RTBR_CONTROL)
        {
            CP_SetPens(&ld->ld_Plot, ld->ld_ShinePen, 0);
            CP_Rect(&ld->ld_Plot, rx, ry,
                (WORD)(rx + run->rr_W - 1), (WORD)(ry + run->rr_H - 1));
            if (run->rr_Data.control.label != NULL)
            {
                struct CP_TextStyle st;

                memset(&st, 0, sizeof(st));
                st.FgPen = ld->ld_TextPen;
                st.BgPen = -1;
                CP_DrawText(&ld->ld_Plot, (WORD)(rx + 4),
                    (WORD)(ry + run->rr_H - 4),
                    run->rr_Data.control.label,
                    strlen((char *)run->rr_Data.control.label), &st);
            }
        }
        else if (run->rr_Kind == RTBR_LINK)
        {
            rtb_paint_run_text(cb, ld, run, rx, ry, colW, TRUE);
        }
        else if (run->rr_Kind == RTBR_TEXT)
        {
            rtb_paint_run_text(cb, ld, run, rx, ry, colW, FALSE);
        }
    }
}

static void
rtb_paint_block(struct ClassBase *cb, struct localData *ld,
    struct RtbBlock *blk, WORD originX, WORD originY, WORD colW)
{
    switch (blk->rb_Type)
    {
        case RTBB_PARAGRAPH:
        case RTBB_HEADING:
            if (blk->rb_Flags & RTBBF_SELECTED)
            {
                CP_SetPens(&ld->ld_Plot, ld->ld_ShinePen, 0);
                CP_FillRect(&ld->ld_Plot, originX, originY,
                    (WORD)(originX + colW - 1),
                    (WORD)(originY + blk->rb_Height - 1));
            }
            rtb_paint_flow(cb, ld, blk, originX, originY, colW);
            break;

        case RTBB_IMAGE:
            rtb_paint_image_bm(cb, ld, blk->rb_Data.image.bitmapObj,
                blk->rb_Data.image.bm, originX, originY,
                blk->rb_Data.image.w ? blk->rb_Data.image.w : 48,
                blk->rb_Data.image.h ? blk->rb_Data.image.h : 48);
            break;

        case RTBB_RULE:
            CP_SetPens(&ld->ld_Plot, ld->ld_ShadowPen, 0);
            CP_Line(&ld->ld_Plot, originX, (WORD)(originY + 2),
                (WORD)(originX + colW - 1), (WORD)(originY + 2));
            break;

        case RTBB_SPACER:
            break;

        case RTBB_CONTROLROW:
            {
                struct RtbRun *run;

                for (run = (struct RtbRun *)
                        blk->rb_Data.controlrow.controls.mlh_Head;
                    run->rr_Node.mln_Succ != NULL;
                    run = (struct RtbRun *)run->rr_Node.mln_Succ)
                {
                    WORD rx;
                    WORD ry;

                    rx = (WORD)(originX + run->rr_X);
                    ry = (WORD)(originY + run->rr_Y);
                    CP_SetPens(&ld->ld_Plot, ld->ld_ShinePen, 0);
                    CP_FillRect(&ld->ld_Plot, rx, ry,
                        (WORD)(rx + run->rr_W - 1),
                        (WORD)(ry + run->rr_H - 1));
                    CP_SetPens(&ld->ld_Plot, ld->ld_ShadowPen, 0);
                    CP_Rect(&ld->ld_Plot, rx, ry,
                        (WORD)(rx + run->rr_W - 1),
                        (WORD)(ry + run->rr_H - 1));
                    if (run->rr_Data.control.label != NULL)
                    {
                        struct CP_TextStyle st;

                        memset(&st, 0, sizeof(st));
                        st.FgPen = ld->ld_TextPen;
                        st.BgPen = -1;
                        CP_DrawText(&ld->ld_Plot, (WORD)(rx + 4),
                            (WORD)(ry + run->rr_H - 4),
                            run->rr_Data.control.label,
                            strlen((char *)run->rr_Data.control.label),
                            &st);
                    }
                }
            }
            break;

        case RTBB_QUOTE:
            CP_SetPens(&ld->ld_Plot, ld->ld_QuoteBarPen, 0);
            CP_FillRect(&ld->ld_Plot, originX, originY,
                (WORD)(originX + 3),
                (WORD)(originY + blk->rb_Height - 1));
            /* fall through children */
        case RTBB_VBOX:
        case RTBB_HBOX:
            {
                struct RtbBlock *ch;
                WORD childX;

                childX = originX;
                if (blk->rb_Type == RTBB_QUOTE)
                    childX = (WORD)(originX + 8);
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.box.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    WORD cx;
                    WORD cy;
                    WORD cw;

                    if (blk->rb_Type == RTBB_HBOX)
                    {
                        cx = childX;
                        cy = originY;
                        cw = ch->rb_Width > 0 ? ch->rb_Width : colW;
                        childX = (WORD)(childX + cw + 4);
                    }
                    else
                    {
                        cx = childX;
                        cy = (WORD)(originY + ch->rb_Y);
                        cw = (WORD)(colW - (childX - originX));
                    }
                    rtb_paint_block(cb, ld, ch, cx, cy, cw);
                }
            }
            break;

        default:
            break;
    }
}

void
rtbPaintDocument(struct ClassBase *cb, struct localData *ld,
    struct RastPort *rp, struct Window *win, struct ColorMap *cm,
    WORD destLeft, WORD destTop)
{
    struct RtbBlock *blk;
    LONG viewTop;
    LONG viewBot;
    WORD originX;
    WORD colW;
    WORD x1;
    WORD y1;
    WORD x2;
    WORD y2;
    struct Region *clip;
    struct Region *oldClip;
    struct Rectangle rect;

    if (ld->ld_Doc == NULL || rp == NULL)
        return;

    if (ld->ld_Plot.cp_CB == NULL)
        CP_Init(&ld->ld_Plot, cb);
    CP_SetTarget(&ld->ld_Plot, rp, win, cm);
    if (ld->ld_Font != NULL)
        ld->ld_Plot.cp_Font = ld->ld_Font;

    originX = destLeft;
    colW = ld->ld_InnerWidth;
    if (colW < 1 || ld->ld_InnerHeight < 1)
        return;

    x1 = destLeft;
    y1 = destTop;
    x2 = (WORD)(destLeft + ld->ld_InnerWidth - 1);
    y2 = (WORD)(destTop + ld->ld_InnerHeight - 1);

    CP_PushClip(&ld->ld_Plot, x1, y1, x2, y2);

    /*
     * Hardware/Layer clip so Text()/Draw cannot spill into sibling gadgets
     * (status bar, scroller). Restored before return.
     *
     * NewRegion() is empty — must OrRectRegion to ADD the box. AndRectRegion
     * on an empty region stays empty and clips away all content.
     */
    clip = NULL;
    oldClip = NULL;
    if (rp->Layer != NULL && LayersBase != NULL)
    {
        clip = NewRegion();
        if (clip != NULL)
        {
            rect.MinX = x1;
            rect.MinY = y1;
            rect.MaxX = x2;
            rect.MaxY = y2;
            if (!OrRectRegion(clip, &rect))
            {
                DisposeRegion(clip);
                clip = NULL;
            }
            else
                oldClip = InstallClipRegion(rp->Layer, clip);
        }
    }

    CP_SetPens(&ld->ld_Plot, ld->ld_BgPen, 0);
    CP_FillRect(&ld->ld_Plot, x1, y1, x2, y2);

    /* Cull to the visible viewport only — overscan must not draw outside. */
    viewTop = ld->ld_Top;
    viewBot = ld->ld_Top + ld->ld_Visible;
    if (viewBot < viewTop)
        viewBot = viewTop;

    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = (struct RtbBlock *)blk->rb_Node.mln_Succ)
    {
        LONG by0;
        LONG by1;
        LONG screenY;

        by0 = blk->rb_Y;
        by1 = blk->rb_Y + blk->rb_Height;
        if (by1 <= viewTop || by0 >= viewBot)
            continue;

        screenY = destTop + (by0 - ld->ld_Top);
        if (screenY + blk->rb_Height < y1 || screenY > y2)
            continue;
        rtb_paint_block(cb, ld, blk, originX, (WORD)screenY, colW);
    }

    if (rp->Layer != NULL && clip != NULL)
    {
        InstallClipRegion(rp->Layer, oldClip);
        DisposeRegion(clip);
    }

    CP_PopClip(&ld->ld_Plot);
}
