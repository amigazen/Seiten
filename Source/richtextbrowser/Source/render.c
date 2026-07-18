/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * render.c - GM_RENDER path: paint visible blocks into window RastPort
 *
 * Never allocates a full-document CHIP SuperBitMap. Paints only the
 * viewport through the canvas plotter with a Layer clip region.
 */

#include "classbase.h"
#include <images/bitmap.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>

struct RtbPaintWrap
{
    struct ClassBase   *pw_CB;
    struct localData   *pw_LD;
    struct RtbFace     *pw_Face;
    WORD                pw_OriginX;
    WORD                pw_OriginY;
    WORD                pw_Baseline;
    WORD                pw_Fg;
    WORD                pw_Bg;
    ULONG               pw_Style;
};

static void
rtb_fill_block_bg(struct localData *ld, struct RtbBlock *blk,
    WORD originX, WORD originY, WORD colW)
{
    WORD bg;

    bg = -1;
    if (blk->rb_Bg != (ULONG)~0)
        bg = (WORD)blk->rb_Bg;
    else if (blk->rb_Flags & RTBBF_SELECTED)
        bg = ld->ld_FillPen;

    if (bg < 0 || blk->rb_Height < 1 || colW < 1)
        return;
    CP_SetPens(&ld->ld_Plot, bg, 0);
    CP_FillRect(&ld->ld_Plot, originX, originY,
        (WORD)(originX + colW - 1),
        (WORD)(originY + blk->rb_Height - 1));
}

static void
rtb_paint_wrap_emit(APTR ud, WORD x, WORD y, CONST_STRPTR text, ULONG len,
    WORD pixelW)
{
    struct RtbPaintWrap *pw;
    WORD ax;
    WORD ay;

    pw = (struct RtbPaintWrap *)ud;
    if (pw == NULL || text == NULL || len < 1)
        return;
    ax = (WORD)(pw->pw_OriginX + x);
    ay = (WORD)(pw->pw_OriginY + y + pw->pw_Baseline);
    rtbFaceDraw(pw->pw_CB, pw->pw_LD, pw->pw_Face, &pw->pw_LD->ld_Plot,
        ax, ay, text, len, pw->pw_Fg, pw->pw_Bg, pw->pw_Style);
    if (pw->pw_Style & RTBS_UNDERLINE)
    {
        CP_SetPens(&pw->pw_LD->ld_Plot, pw->pw_Fg, 0);
        CP_Line(&pw->pw_LD->ld_Plot, ax, (WORD)(ay + 1),
            (WORD)(ax + pixelW), (WORD)(ay + 1));
    }
}

static void
rtb_paint_run_text(struct ClassBase *cb, struct localData *ld,
    struct RtbRun *run, WORD blockX, WORD blockY, WORD colW,
    BOOL isLink)
{
    CONST_STRPTR text;
    ULONG len;
    struct RtbFace *face;
    WORD lineH;
    WORD fg;
    WORD bg;
    UWORD size;
    ULONG style;
    struct RtbPaintWrap pw;

    text = NULL;
    len = 0;
    style = run->rr_Style;
    if (isLink)
    {
        text = run->rr_Data.link.text;
        len = run->rr_Data.link.byteLen;
        fg = ld->ld_LinkPen;
        style |= RTBS_UNDERLINE;
    }
    else
    {
        text = run->rr_Data.text.text;
        len = run->rr_Data.text.byteLen;
        fg = ld->ld_TextPen;
    }
    if (run->rr_Fg != (ULONG)~0)
        fg = (WORD)run->rr_Fg;
    bg = -1;
    if (run->rr_Bg != (ULONG)~0)
        bg = (WORD)run->rr_Bg;
    if (text == NULL || len == 0)
        return;

    size = run->rr_Size;
    if (size < 1)
        size = 8;
    face = rtbFaceResolve(cb, ld, run->rr_FontName, size, style);
    if (face == NULL)
        return;

    lineH = rtbFaceLineHeight(face);

    if (bg >= 0)
    {
        WORD tw;

        tw = rtbFaceTextWidth(cb, ld, face, text, len);
        if (tw > colW && colW > 0)
            tw = colW;
        CP_SetPens(&ld->ld_Plot, bg, 0);
        CP_FillRect(&ld->ld_Plot,
            (WORD)(blockX + run->rr_X), (WORD)(blockY + run->rr_Y),
            (WORD)(blockX + run->rr_X + tw - 1),
            (WORD)(blockY + run->rr_Y + lineH - 1));
    }

    /* Same wrap as layout: block-relative x, wrap at colW. */
    pw.pw_CB = cb;
    pw.pw_LD = ld;
    pw.pw_Face = face;
    pw.pw_OriginX = blockX;
    pw.pw_OriginY = (WORD)(blockY + run->rr_Y);
    pw.pw_Baseline = (WORD)(face->rf_Baseline > 0 ? face->rf_Baseline :
        (lineH - 3));
    pw.pw_Fg = fg;
    pw.pw_Bg = bg;
    pw.pw_Style = style;
    rtbFaceWrapText(cb, ld, face, text, len, colW, run->rr_X,
        rtb_paint_wrap_emit, &pw, NULL);
}

static void
rtb_paint_image_bm(struct ClassBase *cb, struct localData *ld,
    Object *bmObj, struct BitMap *bm, WORD x, WORD y, WORD w, WORD h,
    WORD bgPen, BOOL loadFail)
{
    struct BitMap *src;

    (void)cb;
    if (bgPen >= 0)
    {
        CP_SetPens(&ld->ld_Plot, bgPen, 0);
        CP_FillRect(&ld->ld_Plot, x, y, (WORD)(x + w - 1), (WORD)(y + h - 1));
    }
    src = bm;
    if (src == NULL && bmObj != NULL)
        GetAttr(BITMAP_BitMap, bmObj, (ULONG *)&src);
    /* picture.datatype DestBitMap (AWeb imgsource path) */
    if (src == NULL && bmObj != NULL && DataTypesBase != NULL)
        GetDTAttrs(bmObj, PDTA_DestBitMap, &src, TAG_END);
    if (src == NULL)
    {
        /* Loading: thin frame only. Failed GET: classic broken-image X. */
        CP_SetPens(&ld->ld_Plot, ld->ld_ShadowPen, 0);
        CP_Rect(&ld->ld_Plot, x, y, (WORD)(x + w - 1), (WORD)(y + h - 1));
        if (loadFail)
        {
            CP_SetPens(&ld->ld_Plot, ld->ld_ShinePen, 0);
            CP_Line(&ld->ld_Plot, x, y, (WORD)(x + w - 1), (WORD)(y + h - 1));
            CP_Line(&ld->ld_Plot, (WORD)(x + w - 1), y, x, (WORD)(y + h - 1));
        }
        return;
    }
    CP_BltBitMap(&ld->ld_Plot, src, 0, 0, x, y, w, h);
}

/* Hosted controls: sysiclass CHECKIMAGE; raised button bevel. */
static void
rtb_paint_control(struct ClassBase *cb, struct localData *ld,
    struct RtbRun *run, WORD rx, WORD ry)
{
    WORD x2;
    WORD y2;
    BOOL checked;
    struct CP_TextStyle st;
    struct Image *cimg;
    WORD boxW;
    WORD boxH;

    x2 = (WORD)(rx + run->rr_W - 1);
    y2 = (WORD)(ry + run->rr_H - 1);
    checked = (run->rr_Style & RTBS_CHECKED) ? TRUE : FALSE;

    if (run->rr_Data.control.ctlKind == RTBC_CHECKBOX)
    {
        cimg = (struct Image *)ld->ld_CheckImg;
        boxW = 26;
        boxH = 11;
        if (cimg != NULL)
        {
            boxW = cimg->Width;
            boxH = cimg->Height;
        }
        if (cimg != NULL && ld->ld_Plot.cp_RP != NULL &&
            ld->ld_DrawInfo != NULL && cb != NULL)
        {
            /* DrawImageState needs IntuitionBase via cb macro. */
            DrawImageState(ld->ld_Plot.cp_RP, cimg, (WORD)(rx + 2),
                (WORD)(ry + 2),
                checked ? IDS_SELECTED : IDS_NORMAL,
                ld->ld_DrawInfo);
        }
        else
        {
            /* Fallback if sysiclass unavailable */
            CP_SetPens(&ld->ld_Plot, ld->ld_BgPen, 0);
            CP_FillRect(&ld->ld_Plot, (WORD)(rx + 2), (WORD)(ry + 2),
                (WORD)(rx + 2 + boxW), (WORD)(ry + 2 + boxH));
            CP_SetPens(&ld->ld_Plot, ld->ld_ShadowPen, 0);
            CP_Rect(&ld->ld_Plot, (WORD)(rx + 2), (WORD)(ry + 2),
                (WORD)(rx + 2 + boxW), (WORD)(ry + 2 + boxH));
            if (checked)
            {
                CP_SetPens(&ld->ld_Plot, ld->ld_TextPen, 0);
                CP_Line(&ld->ld_Plot, (WORD)(rx + 4),
                    (WORD)(ry + 2 + boxH / 2),
                    (WORD)(rx + 2 + boxW / 2), (WORD)(ry + 2 + boxH - 2));
                CP_Line(&ld->ld_Plot, (WORD)(rx + 2 + boxW / 2),
                    (WORD)(ry + 2 + boxH - 2),
                    (WORD)(rx + 2 + boxW - 2), (WORD)(ry + 4));
            }
        }
        if (run->rr_Data.control.label != NULL)
        {
            memset(&st, 0, sizeof(st));
            st.FgPen = ld->ld_TextPen;
            st.BgPen = -1;
            if (ld->ld_Font != NULL)
                ld->ld_Plot.cp_Font = ld->ld_Font;
            CP_DrawText(&ld->ld_Plot, (WORD)(rx + boxW + 8),
                (WORD)(ry + run->rr_H - 4),
                run->rr_Data.control.label,
                strlen((char *)run->rr_Data.control.label), &st);
        }
        return;
    }

    /* Borderless AISS icon — DrawImage respects BITMAP_Masking (no magenta). */
    if (run->rr_Data.control.ctlKind == RTBC_ICON)
    {
        WORD iw;
        WORD ih;
        Object *imgObj;
        struct Image *img;

        iw = run->rr_Data.control.iw;
        ih = run->rr_Data.control.ih;
        if (iw < 1)
            iw = 16;
        if (ih < 1)
            ih = 16;
        imgObj = run->rr_Data.control.bitmapObj;
        if (checked && run->rr_Data.control.selBitmapObj != NULL)
            imgObj = run->rr_Data.control.selBitmapObj;
        img = (struct Image *)imgObj;
        if (img != NULL && ld->ld_Plot.cp_RP != NULL && cb != NULL)
        {
            DrawImageState(ld->ld_Plot.cp_RP, img, (WORD)(rx + 2),
                (WORD)(ry + 2),
                checked ? IDS_SELECTED : IDS_NORMAL,
                ld->ld_DrawInfo);
        }
        else
        {
            rtb_paint_image_bm(cb, ld, imgObj, run->rr_Data.control.bm,
                (WORD)(rx + 2), (WORD)(ry + 2), iw, ih, -1, FALSE);
        }
        if (run->rr_Data.control.label != NULL)
        {
            memset(&st, 0, sizeof(st));
            st.FgPen = ld->ld_TextPen;
            st.BgPen = -1;
            if (ld->ld_Font != NULL)
                ld->ld_Plot.cp_Font = ld->ld_Font;
            CP_DrawText(&ld->ld_Plot, (WORD)(rx + iw + 6),
                (WORD)(ry + run->rr_H - 4),
                run->rr_Data.control.label,
                strlen((char *)run->rr_Data.control.label), &st);
        }
        return;
    }

    /* Button: Amiga raised chrome on background (not FILLPEN flood). */
    CP_SetPens(&ld->ld_Plot, ld->ld_BgPen, 0);
    CP_FillRect(&ld->ld_Plot, rx, ry, x2, y2);
    CP_SetPens(&ld->ld_Plot, ld->ld_ShinePen, 0);
    CP_Line(&ld->ld_Plot, rx, ry, x2, ry);
    CP_Line(&ld->ld_Plot, rx, ry, rx, y2);
    CP_SetPens(&ld->ld_Plot, ld->ld_ShadowPen, 0);
    CP_Line(&ld->ld_Plot, x2, ry, x2, y2);
    CP_Line(&ld->ld_Plot, rx, y2, x2, y2);
    if (run->rr_Data.control.label != NULL)
    {
        memset(&st, 0, sizeof(st));
        st.FgPen = ld->ld_TextPen;
        st.BgPen = -1;
        if (ld->ld_Font != NULL)
            ld->ld_Plot.cp_Font = ld->ld_Font;
        CP_DrawText(&ld->ld_Plot, (WORD)(rx + 6), (WORD)(ry + run->rr_H - 5),
            run->rr_Data.control.label,
            strlen((char *)run->rr_Data.control.label), &st);
    }
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
            WORD bg;
            BOOL fail;

            bg = -1;
            if (run->rr_Bg != (ULONG)~0)
                bg = (WORD)run->rr_Bg;
            fail = FALSE;
            rtb_paint_image_bm(cb, ld, run->rr_Data.image.bitmapObj,
                run->rr_Data.image.bm, rx, ry,
                run->rr_W > 0 ? run->rr_W : 48,
                run->rr_H > 0 ? run->rr_H : 48, bg, fail);
        }
        else if (run->rr_Kind == RTBR_CONTROL)
            rtb_paint_control(cb, ld, run, rx, ry);
        else if (run->rr_Kind == RTBR_LINK)
            rtb_paint_run_text(cb, ld, run, originX, originY, colW, TRUE);
        else if (run->rr_Kind == RTBR_TEXT)
            rtb_paint_run_text(cb, ld, run, originX, originY, colW, FALSE);
    }
}

static void
rtb_paint_block(struct ClassBase *cb, struct localData *ld,
    struct RtbBlock *blk, WORD originX, WORD originY, WORD colW)
{
    WORD imgBg;

    rtb_fill_block_bg(ld, blk, originX, originY, colW);

    switch (blk->rb_Type)
    {
        case RTBB_PARAGRAPH:
        case RTBB_HEADING:
            rtb_paint_flow(cb, ld, blk, originX, originY, colW);
            break;

        case RTBB_IMAGE:
            imgBg = -1;
            if (blk->rb_Bg != (ULONG)~0)
                imgBg = (WORD)blk->rb_Bg;
            rtb_paint_image_bm(cb, ld, blk->rb_Data.image.bitmapObj,
                blk->rb_Data.image.bm, originX, originY,
                blk->rb_Data.image.w ? blk->rb_Data.image.w : 48,
                blk->rb_Data.image.h ? blk->rb_Data.image.h : 48, imgBg,
                (blk->rb_Flags & RTBBF_LOADFAIL) ? TRUE : FALSE);
            break;

        case RTBB_IMAGEGRID:
            {
                struct RtbBlock *ch;

                for (ch = (struct RtbBlock *)
                        blk->rb_Data.grid.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    WORD cx;
                    WORD cy;
                    WORD cw;
                    WORD chh;
                    WORD bg;

                    /* Layout stashed cell X in image.maxW */
                    cx = (WORD)(originX + (WORD)ch->rb_Data.image.maxW);
                    cy = (WORD)(originY + ch->rb_Y);
                    cw = ch->rb_Data.image.w ? ch->rb_Data.image.w : 48;
                    chh = ch->rb_Data.image.h ? ch->rb_Data.image.h : 48;
                    bg = -1;
                    if (ch->rb_Bg != (ULONG)~0)
                        bg = (WORD)ch->rb_Bg;
                    else if (blk->rb_Bg != (ULONG)~0)
                        bg = (WORD)blk->rb_Bg;
                    rtb_paint_image_bm(cb, ld, ch->rb_Data.image.bitmapObj,
                        ch->rb_Data.image.bm, cx, cy, cw, chh, bg,
                        (ch->rb_Flags & RTBBF_LOADFAIL) ? TRUE : FALSE);
                }
            }
            break;

        case RTBB_EMBED:
            {
                WORD pad;
                WORD tw;
                WORD th;
                WORD tx;
                WORD ty;
                WORD textW;
                struct CP_TextStyle st;
                WORD x2;
                WORD y2;

                pad = 4;
                x2 = (WORD)(originX + colW - 1);
                y2 = (WORD)(originY + blk->rb_Height - 1);
                /* Thin container frame — not raised button bevel. */
                CP_SetPens(&ld->ld_Plot, ld->ld_ShadowPen, 0);
                CP_Rect(&ld->ld_Plot, originX, originY, x2, y2);

                tw = (WORD)blk->rb_Data.embed.thumbW;
                th = (WORD)blk->rb_Data.embed.thumbH;
                tx = (WORD)(originX + pad);
                ty = (WORD)(originY + pad);
                if (tw > 0 && th > 0)
                {
                    rtb_paint_image_bm(cb, ld,
                        blk->rb_Data.embed.bitmapObj,
                        blk->rb_Data.embed.bm, tx, ty, tw, th, -1, FALSE);
                    tx = (WORD)(tx + tw + 8);
                }
                textW = (WORD)(originX + colW - pad - tx);
                if (textW < 20)
                    textW = 20;
                memset(&st, 0, sizeof(st));
                st.BgPen = -1;
                if (ld->ld_Font != NULL)
                    ld->ld_Plot.cp_Font = ld->ld_Font;
                if (blk->rb_Data.embed.title != NULL)
                {
                    st.FgPen = ld->ld_TextPen;
                    CP_DrawText(&ld->ld_Plot, tx,
                        (WORD)(ty + (ld->ld_Font ? ld->ld_Font->tf_Baseline : 8)),
                        blk->rb_Data.embed.title,
                        strlen((char *)blk->rb_Data.embed.title), &st);
                    ty = (WORD)(ty + (ld->ld_Font ? ld->ld_Font->tf_YSize : 10) + 2);
                }
                if (blk->rb_Data.embed.description != NULL)
                {
                    st.FgPen = ld->ld_TextPen;
                    CP_DrawText(&ld->ld_Plot, tx,
                        (WORD)(ty + (ld->ld_Font ? ld->ld_Font->tf_Baseline : 8)),
                        blk->rb_Data.embed.description,
                        strlen((char *)blk->rb_Data.embed.description), &st);
                    ty = (WORD)(ty + (ld->ld_Font ? ld->ld_Font->tf_YSize : 10) + 2);
                }
                if (blk->rb_Data.embed.site != NULL)
                {
                    st.FgPen = ld->ld_LinkPen; /* FILLPEN accent */
                    CP_DrawText(&ld->ld_Plot, tx,
                        (WORD)(ty + (ld->ld_Font ? ld->ld_Font->tf_Baseline : 8)),
                        blk->rb_Data.embed.site,
                        strlen((char *)blk->rb_Data.embed.site), &st);
                }
                (void)textW;
            }
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
                    rtb_paint_control(cb, ld, run,
                        (WORD)(originX + run->rr_X),
                        (WORD)(originY + run->rr_Y));
                }
            }
            break;

        case RTBB_QUOTE:
            CP_SetPens(&ld->ld_Plot, ld->ld_QuoteBarPen, 0);
            CP_FillRect(&ld->ld_Plot, originX, originY,
                (WORD)(originX + 3),
                (WORD)(originY + blk->rb_Height - 1));
            /* fall through */
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
                        childX = (WORD)(childX + cw + RTB_HBOX_GAP);
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
    (void)CP_BeginTT(&ld->ld_Plot);
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
     * NewRegion() is empty — OrRectRegion ADDs the box. AndRectRegion on
     * empty stays empty and clips away all content.
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
