/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * layout.c - measure blocks (GM_RENDER path via dirty flag)
 *
 * Width wrapping uses rtbFaceTextWidth (TT_TextLength / TextLength) with
 * word boundaries. Never OpenDiskFont here — face cache is warmed earlier.
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
rtb_line_height_size(struct ClassBase *cb, struct localData *ld, UWORD size)
{
    struct RtbFace *f;

    if (size < 1)
        return rtb_line_height(ld);
    f = rtbFaceResolve(cb, ld, NULL, size, 0);
    if (f != NULL)
        return rtbFaceLineHeight(f);
    return rtb_line_height(ld);
}

static WORD
rtb_char_width(struct localData *ld)
{
    if (ld->ld_Font != NULL && ld->ld_Font->tf_XSize > 0)
        return (WORD)ld->ld_Font->tf_XSize;
    return 8;
}

/* Word-wrap using engine TextLength; soft-break at spaces. */
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
        UWORD runSize;
        struct RtbFace *face;

        if (++guard > 10000UL)
            break;

        face = NULL;
        runSize = run->rr_Size;
        if (runSize < 1 && blk->rb_Type == RTBB_HEADING)
            runSize = 11;
        if (runSize > 0 || run->rr_FontName != NULL)
        {
            UWORD sz;

            sz = runSize;
            if (sz < 1)
                sz = 8;
            face = rtbFaceResolve(cb, ld, run->rr_FontName, sz, run->rr_Style);
            if (face != NULL)
            {
                lineH = rtbFaceLineHeight(face);
                cw = rtbFaceAvgWidth(face);
            }
            else
                lineH = rtb_line_height_size(cb, ld, sz);
        }

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
            WORD labelW;

            labelW = 0;
            if (run->rr_Data.control.label != NULL)
                labelW = (WORD)(strlen((char *)run->rr_Data.control.label) *
                    cw);
            if (run->rr_Data.control.ctlKind == RTBC_CHECKBOX)
            {
                WORD iw;
                WORD ih;

                iw = 26;
                ih = 11;
                if (ld->ld_CheckImg != NULL)
                {
                    struct Image *im;

                    im = (struct Image *)ld->ld_CheckImg;
                    iw = im->Width;
                    ih = im->Height;
                }
                ctlw = (WORD)(iw + 8 + labelW);
                if (ih + 4 > lineH)
                    lineH = (WORD)(ih + 4);
            }
            else if (run->rr_Data.control.ctlKind == RTBC_ICON)
            {
                WORD iw;
                WORD ih;

                iw = run->rr_Data.control.iw;
                ih = run->rr_Data.control.ih;
                if (iw < 1)
                    iw = 16;
                if (ih < 1)
                    ih = 16;
                ctlw = (WORD)(iw + 4 + (labelW > 0 ? (labelW + 4) : 0));
                if (ih + 4 > lineH)
                    lineH = (WORD)(ih + 4);
            }
            else
                ctlw = (WORD)(labelW + 16);
            if (ctlw < 48 && run->rr_Data.control.ctlKind != RTBC_ICON)
                ctlw = 48;
            if (run->rr_Data.control.ctlKind == RTBC_ICON && ctlw < 20)
                ctlw = 20;
            if (x > 0 && x + ctlw > colW)
            {
                y += lineH;
                x = 0;
            }
            run->rr_X = x;
            run->rr_Y = (WORD)y;
            run->rr_W = ctlw;
            run->rr_H = (WORD)(lineH + 6);
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

        if (face == NULL)
        {
            UWORD sz;

            sz = runSize;
            if (sz < 1)
                sz = 8;
            face = rtbFaceResolve(cb, ld, run->rr_FontName, sz, run->rr_Style);
        }
        if (face != NULL)
        {
            WORD endX;
            LONG h;
            LONG startY;
            WORD runLineH;
            BOOL endsNL;

            startY = y;
            endX = x;
            runLineH = rtbFaceLineHeight(face);
            h = rtbFaceWrapText(cb, ld, face, text, len, colW, x,
                NULL, NULL, &endX);
            run->rr_W = (WORD)(colW > run->rr_X ? (colW - run->rr_X) : colW);
            if (run->rr_W < 1)
                run->rr_W = 1;
            run->rr_H = (WORD)h;
            if (run->rr_H < runLineH)
                run->rr_H = runLineH;
            /*
             * Cursor on the run's last line. Always subtract this run's
             * line height (not the previous run's) so a taller handle
             * followed by body does not leave y stuck on line 0.
             */
            y = startY + h - runLineH;
            if (y < startY)
                y = startY;
            endsNL = FALSE;
            if (len > 0)
            {
                UBYTE last;

                last = (UBYTE)text[len - 1];
                if (last == (UBYTE)'\n' || last == (UBYTE)'\r')
                    endsNL = TRUE;
            }
            /*
             * Hard break must clear the prior line when faces differ
             * (e.g. orphan "\\n" run with a smaller default face).
             */
            if (endsNL && y < startY + lineH)
                y = startY + lineH;
            x = endX;
            lineH = runLineH;
        }
        else
        {
            /* No face — fall back to cell width (should be rare). */
            ULONG i;
            LONG startY;
            WORD cx;

            startY = y;
            cx = x;
            for (i = 0; i < len; i++)
            {
                UBYTE ch;

                ch = (UBYTE)text[i];
                if (ch == (UBYTE)'\n' || ch == (UBYTE)'\r')
                {
                    y += lineH;
                    cx = 0;
                    continue;
                }
                if (cx > 0 && cx + cw > colW)
                {
                    y += lineH;
                    cx = 0;
                }
                cx = (WORD)(cx + cw);
            }
            run->rr_W = (WORD)(colW > run->rr_X ? (colW - run->rr_X) : colW);
            run->rr_H = (WORD)(y - startY + lineH);
            if (run->rr_H < lineH)
                run->rr_H = lineH;
            x = cx;
        }
    }

    if (x > 0 || y == 0)
        y += lineH;

    blk->rb_Width = colW;
    return y + RTB_ROW_PAD;
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

        case RTBB_IMAGEGRID:
            {
                struct RtbBlock *ch;
                UWORD n;
                UWORD cols;
                UWORD gap;
                WORD cellW;
                WORD maxCellH;
                UWORD i;
                LONG rowHeights[4];
                UWORD rows;
                LONG rowH;
                UWORD r;

                n = 0;
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.grid.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    if (++guard > 1000UL)
                        break;
                    n++;
                }
                if (n < 1)
                {
                    h = 8;
                    blk->rb_Width = 0;
                    break;
                }
                cols = blk->rb_Data.grid.columns;
                if (cols < 1)
                {
                    if (n <= 1)
                        cols = 1;
                    else
                        cols = 2;
                }
                if (cols > n)
                    cols = n;
                rows = (UWORD)((n + cols - 1) / cols);
                if (rows > 4)
                    rows = 4;
                gap = blk->rb_Data.grid.gap;
                if (gap < 1)
                    gap = RTB_HBOX_GAP;
                maxCellH = (WORD)blk->rb_Data.grid.maxCellH;
                if (maxCellH < 1)
                    maxCellH = 200;

                /*
                 * Split content width across columns so side-by-side images
                 * share the gadget viewport; height follows aspect ratio.
                 */
                cellW = colW;
                if (cols > 1)
                    cellW = (WORD)((colW - (WORD)((cols - 1) * gap)) /
                        (WORD)cols);
                if (cellW < 24)
                    cellW = 24;

                for (r = 0; r < 4; r++)
                    rowHeights[r] = 0;

                i = 0;
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.grid.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    UWORD col;
                    UWORD row;
                    WORD nw;
                    WORD nh;
                    WORD dw;
                    WORD dh;

                    if (++guard > 1000UL)
                        break;
                    col = (UWORD)(i % cols);
                    row = (UWORD)(i / cols);
                    if (row >= 4)
                        break;
                    nw = (WORD)ch->rb_Data.image.w;
                    nh = (WORD)ch->rb_Data.image.h;
                    if (ch->rb_Data.image.bm == NULL &&
                        ch->rb_Data.image.bitmapObj == NULL)
                    {
                        /* Placeholder: span cell width until CDN decode. */
                        dw = cellW;
                        dh = (WORD)((cellW * 3) / 4);
                        if (dh > maxCellH)
                            dh = maxCellH;
                    }
                    else
                    {
                        if (nw < 1)
                            nw = 4;
                        if (nh < 1)
                            nh = 3;
                        /* Shrink to cell — never upscale past decoded pixels. */
                        if (nw > cellW)
                        {
                            dw = cellW;
                            dh = (WORD)((nh * cellW) / nw);
                        }
                        else
                        {
                            dw = nw;
                            dh = nh;
                        }
                        if (dh > maxCellH)
                        {
                            dh = maxCellH;
                            dw = (WORD)((nw * maxCellH) / nh);
                            if (dw > cellW)
                                dw = cellW;
                        }
                    }
                    if (dw < 1)
                        dw = 1;
                    if (dh < 1)
                        dh = 1;

                    ch->rb_Data.image.w = (UWORD)dw;
                    ch->rb_Data.image.h = (UWORD)dh;
                    ch->rb_Width = dw;
                    ch->rb_Height = dh;
                    ch->rb_Data.image.maxW = (UWORD)(col * (cellW + gap));
                    if (dh > (WORD)rowHeights[row])
                        rowHeights[row] = dh;
                    i++;
                }

                i = 0;
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.grid.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    UWORD row;
                    LONG yoff;

                    if (++guard > 1000UL)
                        break;
                    row = (UWORD)(i / cols);
                    yoff = 0;
                    for (r = 0; r < row && r < 4; r++)
                        yoff += rowHeights[r] + gap;
                    ch->rb_Y = yoff;
                    i++;
                }
                rowH = 0;
                for (r = 0; r < rows; r++)
                {
                    rowH += rowHeights[r];
                    if (r + 1 < rows)
                        rowH += gap;
                }
                h = rowH + 4;
                blk->rb_Width = (WORD)(cols * cellW +
                    (cols > 1 ? (cols - 1) * gap : 0));
                if (blk->rb_Width > colW)
                    blk->rb_Width = colW;
            }
            break;

        case RTBB_EMBED:
            {
                WORD thumbW;
                WORD thumbH;
                WORD textW;
                WORD lineH;
                WORD cw;
                LONG textH;
                ULONG tlen;
                ULONG dlen;
                ULONG slen;

                thumbW = (WORD)blk->rb_Data.embed.thumbW;
                thumbH = (WORD)blk->rb_Data.embed.thumbH;
                if (thumbW < 0)
                    thumbW = 0;
                if (thumbH < 1 && thumbW > 0)
                    thumbH = 64;
                lineH = rtb_line_height(ld);
                cw = rtb_char_width(ld);
                textW = (WORD)(colW - 12);
                if (thumbW > 0)
                    textW = (WORD)(textW - thumbW - 8);
                if (textW < 40)
                    textW = 40;
                textH = 0;
                tlen = 0;
                dlen = 0;
                slen = 0;
                if (blk->rb_Data.embed.title != NULL)
                    tlen = strlen((char *)blk->rb_Data.embed.title);
                if (blk->rb_Data.embed.description != NULL)
                    dlen = strlen((char *)blk->rb_Data.embed.description);
                if (blk->rb_Data.embed.site != NULL)
                    slen = strlen((char *)blk->rb_Data.embed.site);
                if (tlen > 0 && cw > 0)
                    textH += ((LONG)((tlen * cw) / textW) + 1) * lineH;
                if (dlen > 0 && cw > 0)
                    textH += ((LONG)((dlen * cw) / textW) + 1) * lineH;
                if (slen > 0)
                    textH += lineH;
                if (textH < lineH)
                    textH = lineH;
                h = textH + 12;
                if (thumbH + 12 > h)
                    h = thumbH + 12;
                blk->rb_Width = colW;
            }
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
                    WORD labelW;

                    if (++guard > 1000UL)
                        break;
                    labelW = 0;
                    if (run->rr_Data.control.label != NULL)
                        labelW = (WORD)(strlen((char *)
                            run->rr_Data.control.label) * cw);
                    if (run->rr_Data.control.ctlKind == RTBC_CHECKBOX)
                    {
                        WORD iw;
                        WORD ih;

                        iw = 26;
                        ih = 11;
                        if (ld->ld_CheckImg != NULL)
                        {
                            struct Image *im;

                            im = (struct Image *)ld->ld_CheckImg;
                            iw = im->Width;
                            ih = im->Height;
                        }
                        ctlw = (WORD)(iw + 8 + labelW);
                        if (ih + 4 > lineH)
                            lineH = (WORD)(ih + 4);
                    }
                    else if (run->rr_Data.control.ctlKind == RTBC_ICON)
                    {
                        WORD iw;
                        WORD ih;

                        iw = run->rr_Data.control.iw;
                        ih = run->rr_Data.control.ih;
                        if (iw < 1)
                            iw = 16;
                        if (ih < 1)
                            ih = 16;
                        ctlw = (WORD)(iw + 6 + (labelW > 0 ? (labelW + 4) : 0));
                        if (ih + 6 > lineH)
                            lineH = (WORD)(ih + 6);
                    }
                    else
                        ctlw = (WORD)(labelW + 20);
                    if (ctlw < 52 && run->rr_Data.control.ctlKind != RTBC_ICON)
                        ctlw = 52;
                    if (run->rr_Data.control.ctlKind == RTBC_ICON && ctlw < 22)
                        ctlw = 22;
                    run->rr_X = x;
                    run->rr_Y = (WORD)(RTB_ROW_PAD / 2);
                    run->rr_W = ctlw;
                    run->rr_H = lineH;
                    if (run->rr_H < 14)
                        run->rr_H = 14;
                    x = (WORD)(x + ctlw + 8);
                }
                h = lineH + RTB_ROW_PAD;
                if (h < 18)
                    h = 18;
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
                    cx = (WORD)(cx + ch->rb_Width + RTB_HBOX_GAP);
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
