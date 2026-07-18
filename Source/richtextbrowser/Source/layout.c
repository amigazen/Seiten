/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * layout.c - measure blocks (fixed line metrics; no TextExtent in loops)
 *
 * Width wrapping uses tf_XSize estimates only. Never OpenDiskFont / TextExtent
 * here — those under intuition locks hard-lock on some RTG paths.
 */

#include "classbase.h"

static WORD
rtb_line_height(struct localData *ld)
{
    if (ld->ld_Font != NULL)
        return (WORD)(ld->ld_Font->tf_YSize + 2);
    return 10;
}

static WORD
rtb_char_width(struct localData *ld)
{
    if (ld->ld_Font != NULL && ld->ld_Font->tf_XSize > 0)
        return ld->ld_Font->tf_XSize;
    return 8;
}

/* Word-wrap using character-cell widths only (no graphics.library calls). */
static LONG
rtb_measure_flow(struct ClassBase *cb, struct localData *ld,
    struct RtbBlock *blk, WORD colW)
{
    struct RtbRun *run;
    WORD lineH;
    WORD x;
    LONG y;
    WORD cw;
    ULONG guard;

    (void)cb;

    y = 0;
    x = 0;
    lineH = rtb_line_height(ld);
    cw = rtb_char_width(ld);
    guard = 0;

    for (run = (struct RtbRun *)blk->rb_Data.flow.runs.mlh_Head;
        run->rr_Node.mln_Succ != NULL;
        run = (struct RtbRun *)run->rr_Node.mln_Succ)
    {
        CONST_STRPTR text;
        ULONG len;

        if (++guard > 10000UL)
            break;

        if (run->rr_Kind == RTBR_IMAGE)
        {
            WORD iw;
            WORD ih;

            iw = run->rr_Data.image.w;
            ih = run->rr_Data.image.h;
            if (iw == 0)
                iw = 48;
            if (ih == 0)
                ih = 48;
            if (x > 0 && x + iw > colW)
            {
                y += lineH;
                x = 0;
            }
            run->rr_X = x;
            run->rr_Y = (WORD)y;
            run->rr_W = iw;
            run->rr_H = ih;
            if (ih + 2 > lineH)
                lineH = (WORD)(ih + 2);
            x = (WORD)(x + iw + 4);
            continue;
        }

        if (run->rr_Kind == RTBR_CONTROL)
        {
            WORD ctlw;

            ctlw = 48;
            if (run->rr_Data.control.label != NULL)
                ctlw = (WORD)(strlen((char *)run->rr_Data.control.label) *
                    cw + 16);
            if (x > 0 && x + ctlw > colW)
            {
                y += lineH;
                x = 0;
            }
            run->rr_X = x;
            run->rr_Y = (WORD)y;
            run->rr_W = ctlw;
            run->rr_H = lineH;
            x = (WORD)(x + ctlw + 4);
            continue;
        }

        text = NULL;
        len = 0;
        if (run->rr_Kind == RTBR_LINK)
        {
            text = run->rr_Data.link.text;
            len = run->rr_Data.link.byteLen;
        }
        else if (run->rr_Kind == RTBR_TEXT)
        {
            text = run->rr_Data.text.text;
            len = run->rr_Data.text.byteLen;
        }

        run->rr_X = x;
        run->rr_Y = (WORD)y;

        if (text == NULL || len == 0)
        {
            run->rr_W = 0;
            run->rr_H = lineH;
            continue;
        }

        /* Cell-width estimate; soft-wrap long runs without TextExtent. */
        {
            WORD tw;

            tw = (WORD)(len * cw);
            if (x > 0 && x + tw > colW)
            {
                y += lineH;
                x = 0;
                run->rr_X = 0;
                run->rr_Y = (WORD)y;
            }
            if (tw > colW && colW > cw)
            {
                ULONG lines;

                lines = (ULONG)((tw + colW - 1) / colW);
                if (lines < 1)
                    lines = 1;
                if (lines > 64)
                    lines = 64;
                run->rr_W = colW;
                run->rr_H = (WORD)(lines * lineH);
                y += (LONG)(lines * lineH);
                x = 0;
            }
            else
            {
                run->rr_W = tw;
                run->rr_H = lineH;
                x = (WORD)(x + tw);
            }
        }
    }

    if (x > 0 || y == 0)
        y += lineH;

    blk->rb_Width = colW;
    return y + 4;
}

static LONG
rtb_measure_block(struct ClassBase *cb, struct localData *ld,
    struct RtbBlock *blk, WORD colW)
{
    LONG h;
    ULONG guard;

    h = 0;
    guard = 0;

    switch (blk->rb_Type)
    {
        case RTBB_PARAGRAPH:
        case RTBB_HEADING:
            h = rtb_measure_flow(cb, ld, blk, colW);
            break;

        case RTBB_IMAGE:
            h = blk->rb_Data.image.h;
            if (h == 0)
                h = 48;
            blk->rb_Width = blk->rb_Data.image.w;
            if (blk->rb_Width == 0)
                blk->rb_Width = 48;
            h += 4;
            break;

        case RTBB_RULE:
            h = blk->rb_Data.rule.thickness + 6;
            if (h < 3)
                h = 3;
            blk->rb_Width = colW;
            break;

        case RTBB_SPACER:
            h = blk->rb_Data.spacer.pixels;
            blk->rb_Width = colW;
            break;

        case RTBB_CONTROLROW:
            {
                struct RtbRun *run;
                WORD x;
                WORD lineH;
                WORD cw;

                x = 0;
                lineH = (WORD)(rtb_line_height(ld) + 6);
                cw = rtb_char_width(ld);
                for (run = (struct RtbRun *)
                        blk->rb_Data.controlrow.controls.mlh_Head;
                    run->rr_Node.mln_Succ != NULL;
                    run = (struct RtbRun *)run->rr_Node.mln_Succ)
                {
                    WORD ctlw;

                    if (++guard > 1000UL)
                        break;
                    ctlw = 56;
                    if (run->rr_Data.control.label != NULL)
                        ctlw = (WORD)(strlen((char *)
                            run->rr_Data.control.label) * cw + 16);
                    run->rr_X = x;
                    run->rr_Y = 0;
                    run->rr_W = ctlw;
                    run->rr_H = lineH;
                    x = (WORD)(x + ctlw + 6);
                }
                h = lineH + 4;
                blk->rb_Width = colW;
            }
            break;

        case RTBB_VBOX:
        case RTBB_QUOTE:
            {
                struct RtbBlock *ch;
                LONG cy;
                WORD childW;

                cy = 0;
                childW = colW;
                if (blk->rb_Type == RTBB_QUOTE)
                    childW = (WORD)(colW - 12);
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.box.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    if (++guard > 1000UL)
                        break;
                    ch->rb_Y = cy;
                    ch->rb_Height = rtb_measure_block(cb, ld, ch, childW);
                    cy += ch->rb_Height;
                }
                h = cy;
                if (blk->rb_Type == RTBB_QUOTE)
                    h += 4;
                blk->rb_Width = colW;
            }
            break;

        case RTBB_HBOX:
            {
                struct RtbBlock *ch;
                WORD cx;
                LONG maxH;

                cx = 0;
                maxH = 0;
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.box.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    WORD avail;

                    if (++guard > 1000UL)
                        break;
                    avail = (WORD)(colW - cx);
                    if (avail < 16)
                        avail = 16;
                    ch->rb_Height = rtb_measure_block(cb, ld, ch, avail);
                    ch->rb_Y = 0;
                    if (ch->rb_Width <= 0)
                        ch->rb_Width = avail;
                    cx = (WORD)(cx + ch->rb_Width + 4);
                    if (ch->rb_Height > maxH)
                        maxH = ch->rb_Height;
                }
                h = maxH;
                blk->rb_Width = colW;
            }
            break;

        default:
            h = 8;
            blk->rb_Width = colW;
            break;
    }
    return h;
}

void
rtbMeasureDocument(struct ClassBase *cb, struct localData *ld)
{
    struct RtbBlock *blk;
    LONG y;
    WORD colW;
    ULONG guard;

    if (ld->ld_Doc == NULL)
    {
        ld->ld_Total = 0;
        return;
    }

    colW = ld->ld_InnerWidth;
    if (colW < 16)
        colW = 16;

    if (ld->ld_Plot.cp_CB == NULL)
        CP_Init(&ld->ld_Plot, cb);
    if (ld->ld_Font != NULL)
        ld->ld_Plot.cp_Font = ld->ld_Font;

    y = 0;
    guard = 0;
    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = (struct RtbBlock *)blk->rb_Node.mln_Succ)
    {
        if (++guard > 5000UL)
            break;
        blk->rb_Y = y;
        blk->rb_Height = rtb_measure_block(cb, ld, blk, colW);
        y += blk->rb_Height;
    }
    ld->ld_Total = y;
}
