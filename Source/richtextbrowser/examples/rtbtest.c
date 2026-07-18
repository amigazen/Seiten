/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtbtest.c - richtextbrowser.gadget Bluesky-flavoured feed harness
 *
 * Exercises: zebra RTBA_BgPen, heading size, BitMap blit, placeholder
 * thumbs, hosted Like/Repost buttons, checkbox toggle, links, quote,
 * live scroller, hit RelEvent.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <intuition/gadgetclass.h>
#include <graphics/gfx.h>
#include <proto/alib.h>

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/scroller.h>
#include <proto/button.h>

#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/scroller.h>
#include <gadgets/button.h>
#include <gadgets/richtextbrowser.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

#include <stdio.h>
#include <string.h>

struct Library *RichTextBrowserBase;

/* Sample avatar thumb — freed after the window closes. */
static struct BitMap *g_ThumbBM;

static void
log_msg(CONST_STRPTR s)
{
    PutStr((STRPTR)s);
    Flush(Output());
}

static void
log_fmt(CONST_STRPTR fmt, ULONG a, ULONG b, ULONG c)
{
    UBYTE buf[128];

    sprintf((char *)buf, (char *)fmt,
        (unsigned long)a, (unsigned long)b, (unsigned long)c);
    log_msg(buf);
}

static void
log_metrics(Object *rtb, CONST_STRPTR where)
{
    ULONG top;
    ULONG tot;
    ULONG vis;

    top = 0;
    tot = 0;
    vis = 0;
    GetAttr(RTB_Top, rtb, &top);
    GetAttr(RTB_Total, rtb, &tot);
    GetAttr(RTB_Visible, rtb, &vis);
    log_msg(where);
    log_fmt("  RTB_Top=%lu Total=%lu Visible=%lu\n", top, tot, vis);
}

static void
sync_scroller(Object *scroller, Object *rtb, struct Window *win)
{
    ULONG top;
    ULONG tot;
    ULONG vis;

    top = 0;
    tot = 0;
    vis = 0;
    GetAttr(RTB_Top, rtb, &top);
    GetAttr(RTB_Total, rtb, &tot);
    GetAttr(RTB_Visible, rtb, &vis);
    SetGadgetAttrs((struct Gadget *)scroller, win, NULL,
        SCROLLER_Top, top,
        SCROLLER_Total, tot > 0 ? tot : 1,
        SCROLLER_Visible, vis > 0 ? vis : 1,
        TAG_DONE);
}

static void
apply_scroller_top(Object *scroller, Object *rtb, struct Window *win)
{
    ULONG stop;
    ULONG rtop;

    stop = 0;
    rtop = 0;
    GetAttr(SCROLLER_Top, scroller, &stop);
    GetAttr(RTB_Top, rtb, &rtop);
    if (stop != rtop)
    {
        SetGadgetAttrs((struct Gadget *)rtb, win, NULL,
            RTB_Top, stop, TAG_DONE);
    }
}

static BOOL
insert_block(Object *rtb, struct RtbBlock *blk)
{
    struct rtbInsertBlock ins;

    if (blk == NULL)
        return FALSE;
    memset(&ins, 0, sizeof(ins));
    ins.MethodID = RTBM_INSERTBLOCK;
    ins.GInfo = NULL;
    ins.Block = blk;
    ins.AfterID = 0;
    DoMethodA(rtb, (Msg)&ins);
    return TRUE;
}

/*
 * Checkerboard 48x48 BitMap for the first post avatar.
 * Later posts use a placeholder (no BitMap) so both paint paths run.
 */
static struct BitMap *
make_thumb_bm(void)
{
    struct BitMap *bm;
    struct RastPort rp;
    WORD x;
    WORD y;

    bm = AllocBitMap(48, 48, 1, BMF_CLEAR, NULL);
    if (bm == NULL)
        return NULL;
    InitRastPort(&rp);
    rp.BitMap = bm;
    SetAPen(&rp, 1);
    for (y = 0; y < 48; y++)
    {
        for (x = 0; x < 48; x++)
        {
            if (((x / 6) ^ (y / 6)) & 1)
                WritePixel(&rp, x, y);
        }
    }
    Move(&rp, 0, 0);
    Draw(&rp, 47, 0);
    Draw(&rp, 47, 47);
    Draw(&rp, 0, 47);
    Draw(&rp, 0, 0);
    return bm;
}

/* One feed row: HBOX(avatar | VBOX(text, actions)) with zebra bg. */
static BOOL
insert_feed_post(Object *rtb, LONG i, struct BitMap *thumb)
{
    ULONG bg;
    UBYTE handle[48];
    UBYTE body[160];
    struct RtbBlock *hbox;
    struct RtbBlock *vbox;
    struct RtbBlock *img;
    struct RtbBlock *para;
    struct RtbBlock *ctl;
    struct RtbRun *run;

    /* Pen 0 = even row; pen 3 = odd fill (DrawInfo FILLPEN-ish). */
    bg = ((i & 1) == 0) ? 0UL : 3UL;

    sprintf((char *)handle, "@amiga.user%ld.bsky.social", (long)i);
    sprintf((char *)body,
        "Post %ld: Amiga-flavoured Bluesky row. Zebra bg, "
        "hosted Like/Repost, checkbox notify. Wrap words "
        "words words so soft layout and clip stay honest.",
        (long)i);

    hbox = AllocRtbBlockTags(RTBB_HBOX, RTBA_BgPen, bg, TAG_DONE);
    if (hbox == NULL)
        return FALSE;

    if (thumb != NULL && i == 0)
    {
        img = AllocRtbBlockTags(RTBB_IMAGE,
            RTBA_Width, 48,
            RTBA_Height, 48,
            RTBA_BgPen, bg,
            RTBA_BitMap, (ULONG)thumb,
            RTBA_Alt, (ULONG)"avatar",
            TAG_DONE);
    }
    else
    {
        img = AllocRtbBlockTags(RTBB_IMAGE,
            RTBA_Width, 48,
            RTBA_Height, 48,
            RTBA_BgPen, bg,
            RTBA_Alt, (ULONG)"avatar",
            TAG_DONE);
    }
    if (img == NULL)
    {
        FreeRtbBlock(hbox);
        return FALSE;
    }

    vbox = AllocRtbBlockTags(RTBB_VBOX, RTBA_BgPen, bg, TAG_DONE);
    para = AllocRtbBlockTags(RTBB_PARAGRAPH, RTBA_BgPen, bg, TAG_DONE);
    if (vbox == NULL || para == NULL)
    {
        if (vbox != NULL)
            FreeRtbBlock(vbox);
        if (para != NULL)
            FreeRtbBlock(para);
        FreeRtbBlock(img);
        FreeRtbBlock(hbox);
        return FALSE;
    }

    run = AllocRtbRunTags(RTBR_TEXT,
        RTBA_Text, (ULONG)handle,
        RTBA_Style, RTBS_BOLD,
        RTBA_FontName, (ULONG)"Arial",
        RTBA_Size, 13,
        TAG_DONE);
    if (run != NULL)
        RtbBlockAddRun(para, run);

    run = AllocRtbRunTags(RTBR_TEXT,
        RTBA_Text, (ULONG)"  ·  2h\n",
        RTBA_FontName, (ULONG)"Times New Roman",
        RTBA_Size, 10,
        TAG_DONE);
    if (run != NULL)
        RtbBlockAddRun(para, run);

    run = AllocRtbRunTags(RTBR_TEXT,
        RTBA_Text, (ULONG)body,
        RTBA_FontName, (ULONG)"Times New Roman",
        RTBA_Size, 12,
        TAG_DONE);
    if (run != NULL)
        RtbBlockAddRun(para, run);

    if ((i % 4) == 1)
    {
        run = AllocRtbRunTags(RTBR_TEXT,
            RTBA_Text, (ULONG)"  See ",
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(para, run);
        run = AllocRtbRunTags(RTBR_LINK,
            RTBA_Text, (ULONG)"bsky.app",
            RTBA_Href, (ULONG)"https://bsky.app/",
            RTBA_User, (ULONG)(0xBEEF0000UL + (ULONG)i),
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(para, run);
    }

    ctl = AllocRtbBlockTags(RTBB_CONTROLROW, RTBA_BgPen, bg, TAG_DONE);
    if (ctl != NULL)
    {
        run = AllocRtbRunTags(RTBR_CONTROL,
            RTBA_CtlKind, RTBC_BUTTON,
            RTBA_Label, (ULONG)"Like",
            RTBA_User, (ULONG)(0xCAFE0000UL + (ULONG)i),
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(ctl, run);
        run = AllocRtbRunTags(RTBR_CONTROL,
            RTBA_CtlKind, RTBC_BUTTON,
            RTBA_Label, (ULONG)"Repost",
            RTBA_User, (ULONG)(0xC0DE0000UL + (ULONG)i),
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(ctl, run);
        run = AllocRtbRunTags(RTBR_CONTROL,
            RTBA_CtlKind, RTBC_CHECKBOX,
            RTBA_Label, (ULONG)"Notify",
            RTBA_Checked, (i & 1) ? TRUE : FALSE,
            RTBA_User, (ULONG)(0xFACE0000UL + (ULONG)i),
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(ctl, run);
    }

    RtbBlockAddChild(vbox, para);
    if (ctl != NULL)
        RtbBlockAddChild(vbox, ctl);
    RtbBlockAddChild(hbox, img);
    RtbBlockAddChild(hbox, vbox);
    insert_block(rtb, hbox);
    insert_block(rtb, AllocRtbBlockTags(RTBB_RULE, TAG_DONE));
    return TRUE;
}

static BOOL
insert_sample_doc(Object *rtb, LONG nPosts)
{
    struct RtbBlock *blk;
    struct RtbRun *run;
    LONG i;

    log_msg("rtbtest: SetAttrs RTB_Busy TRUE\n");
    SetAttrs(rtb, RTB_Busy, TRUE, TAG_DONE);

    blk = AllocRtbBlockTags(RTBB_HEADING, TAG_DONE);
    if (blk != NULL)
    {
        /* TTEngine family (maps to Arial); bullet CGTriumvirate if no TTF. */
        run = AllocRtbRunTags(RTBR_TEXT,
            RTBA_Text, (ULONG)"Seiten feed · richtextbrowser",
            RTBA_FontName, (ULONG)"Arial",
            RTBA_Size, 18,
            RTBA_Style, RTBS_BOLD,
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(blk, run);
        insert_block(rtb, blk);
    }

    blk = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
    if (blk != NULL)
    {
        run = AllocRtbRunTags(RTBR_TEXT,
            RTBA_Text, (ULONG)
            "Harness: TTEngine/bullet scalable faces, sysiclass CHECKIMAGE, "
            "zebra rows, BitMap thumbs, scroll. Bitmap fonts are fallback only.",
            RTBA_FontName, (ULONG)"Times New Roman",
            RTBA_Size, 12,
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(blk, run);
        insert_block(rtb, blk);
    }

    /* Quoted reply sample */
    blk = AllocRtbBlockTags(RTBB_QUOTE, TAG_DONE);
    if (blk != NULL)
    {
        struct RtbBlock *child;

        child = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
        if (child != NULL)
        {
            run = AllocRtbRunTags(RTBR_TEXT,
                RTBA_Text, (ULONG)
                "Quoted skeet (bullet CGTimes.otag when TTEngine absent).",
                RTBA_FontName, (ULONG)"FONTS:CGTimes.otag",
                RTBA_Size, 14,
                TAG_DONE);
            if (run != NULL)
                RtbBlockAddRun(child, run);
            AddTail((struct List *)&blk->rb_Data.box.children,
                (struct Node *)child);
        }
        insert_block(rtb, blk);
    }

    insert_block(rtb, AllocRtbBlockTags(RTBB_SPACER,
        RTBA_Pixels, 8, TAG_DONE));

    g_ThumbBM = make_thumb_bm();
    if (g_ThumbBM == NULL)
        log_msg("rtbtest: WARN AllocBitMap thumb failed (placeholders only)\n");

    for (i = 0; i < nPosts; i++)
    {
        if ((i & 7) == 0)
            log_fmt("rtbtest: insert progress i=%lu\n", (ULONG)i, 0, 0);
        if (!insert_feed_post(rtb, i, g_ThumbBM))
        {
            SetAttrs(rtb, RTB_Busy, FALSE, TAG_DONE);
            return FALSE;
        }
    }

    log_msg("rtbtest: SetAttrs RTB_Busy FALSE\n");
    SetAttrs(rtb, RTB_Busy, FALSE, TAG_DONE);

    log_msg("rtbtest: RTBM_INVALIDATE\n");
    {
        struct rtbInvalidate inv;

        memset(&inv, 0, sizeof(inv));
        inv.MethodID = RTBM_INVALIDATE;
        DoMethodA(rtb, (Msg)&inv);
    }
    return TRUE;
}

int
main(void)
{
    Object *winobj;
    Object *layout;
    Object *rtb;
    Object *scroller;
    Object *status;
    struct Window *win;
    Class *rtbClass;
    ULONG signal;
    BOOL running;
    LONG n;

    log_msg("rtbtest: start (v12 TTEngine + bullet scalable)\n");

    RichTextBrowserBase = OpenLibrary("gadgets/richtextbrowser.gadget", 0);
    if (RichTextBrowserBase == NULL)
        RichTextBrowserBase = OpenLibrary("richtextbrowser.gadget", 0);
    if (RichTextBrowserBase == NULL)
    {
        log_msg("rtbtest: FAIL open richtextbrowser.gadget\n");
        return RETURN_FAIL;
    }
    log_fmt("rtbtest: RichTextBrowserBase=%08lx ver=%lu\n",
        (ULONG)RichTextBrowserBase,
        (ULONG)RichTextBrowserBase->lib_Version, 0);

    rtbClass = RICHTEXTBROWSER_GetClass();
    if (rtbClass == NULL)
    {
        log_msg("rtbtest: FAIL RICHTEXTBROWSER_GetClass\n");
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }

    log_msg("rtbtest: NewObject richtextbrowser\n");
    rtb = NewObject(rtbClass, NULL,
        GA_ID, 1,
        GA_RelVerify, TRUE,
        RTB_SelectBlocks, TRUE,
        RTB_BlockCap, 512,
        RTB_Overscan, 0,
        TAG_DONE);
    if (rtb == NULL)
    {
        log_msg("rtbtest: FAIL NewObject\n");
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }
    log_msg("rtbtest: NewObject OK\n");

    n = 24;
    log_fmt("rtbtest: building feed posts=%lu\n", (ULONG)n, 0, 0);
    if (!insert_sample_doc(rtb, n))
    {
        DisposeObject(rtb);
        if (g_ThumbBM != NULL)
            FreeBitMap(g_ThumbBM);
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }
    log_metrics(rtb, "rtbtest: metrics before window\n");

    log_msg("rtbtest: create scroller + window\n");
    scroller = ScrollerObject,
        GA_ID, 2,
        GA_RelVerify, TRUE,
        GA_Immediate, TRUE,
        SCROLLER_Orientation, SORIENT_VERT,
        SCROLLER_Arrows, TRUE,
    ScrollerEnd;

    status = ButtonObject,
        GA_ID, 3,
        GA_ReadOnly, TRUE,
        GA_Text, "scroll · click Like/Notify/links · zebra + thumb",
    ButtonEnd;

    layout = VLayoutObject,
        LAYOUT_AddChild, status,
        CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, HLayoutObject,
            LAYOUT_AddChild, rtb,
            CHILD_WeightedWidth, 100,
            LAYOUT_AddChild, scroller,
            CHILD_WeightedWidth, 0,
            CHILD_MinWidth, 16,
        LayoutEnd,
        CHILD_WeightedHeight, 100,
    LayoutEnd;

    winobj = WindowObject,
        WA_Title, "richtextbrowser · Seiten feed",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_InnerWidth, 440,
        WA_InnerHeight, 300,
        WA_IDCMP, IDCMP_INTUITICKS,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_ParentGroup, layout,
    EndWindow;

    if (winobj == NULL)
    {
        log_msg("rtbtest: FAIL WindowObject\n");
        DisposeObject(rtb);
        if (g_ThumbBM != NULL)
            FreeBitMap(g_ThumbBM);
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }

    log_msg("rtbtest: RA_OpenWindow\n");
    win = (struct Window *)RA_OpenWindow(winobj);
    if (win == NULL)
    {
        log_msg("rtbtest: FAIL RA_OpenWindow\n");
        DisposeObject(winobj);
        if (g_ThumbBM != NULL)
            FreeBitMap(g_ThumbBM);
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }
    log_msg("rtbtest: window open OK\n");

    log_msg("rtbtest: enable content paint\n");
    SetGadgetAttrs((struct Gadget *)rtb, win, NULL, RTB_Top, 0, TAG_DONE);
    log_metrics(rtb, "rtbtest: after content paint\n");
    sync_scroller(scroller, rtb, win);

    log_msg("rtbtest: programmatic Top=80\n");
    SetGadgetAttrs((struct Gadget *)rtb, win, NULL, RTB_Top, 80, TAG_DONE);
    sync_scroller(scroller, rtb, win);
    log_metrics(rtb, "rtbtest: after Top=80\n");

    log_msg("rtbtest: RTBM_HITTEST at (20,40)\n");
    {
        struct rtbHitTest ht;

        memset(&ht, 0, sizeof(ht));
        ht.MethodID = RTBM_HITTEST;
        ht.MouseX = 20;
        ht.MouseY = 40;
        DoMethodA(rtb, (Msg)&ht);
        log_fmt("rtbtest: hit kind=%lu block=%lu run=%lu\n",
            ht.HitKind, ht.BlockID, ht.RunID);
    }

    log_msg("rtbtest: event loop (Notify toggles; Ctrl-C/close)\n");
    GetAttr(WINDOW_SigMask, winobj, &signal);
    running = TRUE;
    while (running)
    {
        ULONG got;
        ULONG result;
        UWORD code;

        got = Wait(signal | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C)
        {
            log_msg("rtbtest: Ctrl-C\n");
            break;
        }

        code = 0;
        while ((result = RA_HandleInput(winobj, &code)) != WMHI_LASTMSG)
        {
            switch (result & WMHI_CLASSMASK)
            {
                case WMHI_CLOSEWINDOW:
                    log_msg("rtbtest: close window\n");
                    running = FALSE;
                    break;
                case WMHI_GADGETUP:
                    if ((result & WMHI_GADGETMASK) == 2)
                        apply_scroller_top(scroller, rtb, win);
                    else if ((result & WMHI_GADGETMASK) == 1)
                    {
                        ULONG ev;
                        ULONG kind;

                        ev = 0;
                        kind = 0;
                        GetAttr(RTB_RelEvent, rtb, &ev);
                        GetAttr(RTB_HitKind, rtb, &kind);
                        log_fmt("rtbtest: RelEvent=%lu HitKind=%lu\n",
                            ev, kind, 0);
                        sync_scroller(scroller, rtb, win);
                    }
                    break;
                case WMHI_INTUITICK:
                    apply_scroller_top(scroller, rtb, win);
                    break;
            }
        }
    }

    log_msg("rtbtest: close / dispose\n");
    RA_CloseWindow(winobj);
    DisposeObject(winobj);
    /* Document held the BitMap pointer; free after gadget dispose. */
    if (g_ThumbBM != NULL)
    {
        FreeBitMap(g_ThumbBM);
        g_ThumbBM = NULL;
    }
    CloseLibrary(RichTextBrowserBase);
    log_msg("rtbtest: done OK\n");
    return RETURN_OK;
}
