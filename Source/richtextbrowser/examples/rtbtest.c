/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtbtest.c - richtextbrowser.gadget stress harness with step logging
 *
 * Covers: OpenLibrary, document builders (heading/quote/rule/link/control/
 * long wrap), window open, ICA scroller map, live scroll, hit-test, dispose.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
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

static BOOL
insert_sample_doc(Object *rtb, LONG nParagraphs)
{
    struct RtbBlock *blk;
    struct RtbRun *run;
    LONG i;

    log_msg("rtbtest: SetAttrs RTB_Busy TRUE\n");
    SetAttrs(rtb, RTB_Busy, TRUE, TAG_DONE);

    /* Heading */
    blk = AllocRtbBlockTags(RTBB_HEADING, TAG_DONE);
    if (blk != NULL)
    {
        run = AllocRtbRunTags(RTBR_TEXT,
            RTBA_Text, (ULONG)"richtextbrowser harness",
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(blk, run);
        insert_block(rtb, blk);
    }

    /* Long wrapping paragraph */
    blk = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
    if (blk != NULL)
    {
        run = AllocRtbRunTags(RTBR_TEXT,
            RTBA_Text, (ULONG)
            "Long wrap: the viewport must clip Text that would otherwise "
            "spill into the status bar or scroller while the document is "
            "scrolled. Alphabet pack: abcdefghijklmnopqrstuvwxyz 0123456789 "
            "repeated words words words words words words words words end.",
            TAG_DONE);
        if (run != NULL)
            RtbBlockAddRun(blk, run);
        insert_block(rtb, blk);
    }

    /* Quote with nested paragraph */
    blk = AllocRtbBlockTags(RTBB_QUOTE, TAG_DONE);
    if (blk != NULL)
    {
        struct RtbBlock *child;

        child = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
        if (child != NULL)
        {
            run = AllocRtbRunTags(RTBR_TEXT,
                RTBA_Text, (ULONG)"Quoted block - bar should stay clipped.",
                TAG_DONE);
            if (run != NULL)
                RtbBlockAddRun(child, run);
            AddTail((struct List *)&blk->rb_Data.box.children,
                (struct Node *)child);
        }
        insert_block(rtb, blk);
    }

    insert_block(rtb, AllocRtbBlockTags(RTBB_RULE, TAG_DONE));
    insert_block(rtb, AllocRtbBlockTags(RTBB_SPACER,
        RTBA_Pixels, 12, TAG_DONE));

    for (i = 0; i < nParagraphs; i++)
    {
        UBYTE buf[120];

        sprintf((char *)buf,
            "Para %ld: scroll/clip harness line with enough wrap words "
            "to exercise soft layout metrics and hit testing.",
            (long)i);

        if ((i & 7) == 0)
            log_fmt("rtbtest: insert progress i=%lu\n", (ULONG)i, 0, 0);

        blk = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
        if (blk == NULL)
        {
            SetAttrs(rtb, RTB_Busy, FALSE, TAG_DONE);
            return FALSE;
        }
        run = AllocRtbRunTags(RTBR_TEXT, RTBA_Text, (ULONG)buf, TAG_DONE);
        if (run == NULL || !RtbBlockAddRun(blk, run))
        {
            if (run != NULL)
                FreeRtbRun(run);
            FreeRtbBlock(blk);
            SetAttrs(rtb, RTB_Busy, FALSE, TAG_DONE);
            return FALSE;
        }
        insert_block(rtb, blk);

        if ((i % 5) == 4)
        {
            blk = AllocRtbBlockTags(RTBB_PARAGRAPH, TAG_DONE);
            if (blk != NULL)
            {
                run = AllocRtbRunTags(RTBR_TEXT,
                    RTBA_Text, (ULONG)"See also: ", TAG_DONE);
                if (run != NULL)
                    RtbBlockAddRun(blk, run);
                run = AllocRtbRunTags(RTBR_LINK,
                    RTBA_Text, (ULONG)"example.com",
                    RTBA_Href, (ULONG)"https://example.com/",
                    RTBA_User, (ULONG)(0xBEEF0000UL + (ULONG)i),
                    TAG_DONE);
                if (run != NULL)
                    RtbBlockAddRun(blk, run);
                insert_block(rtb, blk);
            }
        }

        if ((i % 6) == 5)
            insert_block(rtb, AllocRtbBlockTags(RTBB_RULE, TAG_DONE));

        if ((i % 10) == 9)
        {
            struct RtbBlock *ctl;
            struct RtbRun *btn;

            ctl = AllocRtbBlock(RTBB_CONTROLROW);
            if (ctl != NULL)
            {
                btn = AllocRtbRunTags(RTBR_CONTROL,
                    RTBA_CtlKind, RTBC_BUTTON,
                    RTBA_Label, (ULONG)"Like",
                    RTBA_User, (ULONG)(0xCAFE0000UL + (ULONG)i),
                    TAG_DONE);
                if (btn != NULL)
                    RtbBlockAddRun(ctl, btn);
                btn = AllocRtbRunTags(RTBR_CONTROL,
                    RTBA_CtlKind, RTBC_BUTTON,
                    RTBA_Label, (ULONG)"Repost",
                    RTBA_User, (ULONG)(0xC0DE0000UL + (ULONG)i),
                    TAG_DONE);
                if (btn != NULL)
                    RtbBlockAddRun(ctl, btn);
                insert_block(rtb, ctl);
            }
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

    log_msg("rtbtest: start (v9 clip + live scroller)\n");

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

    n = 40;
    log_fmt("rtbtest: building sample doc paragraphs=%lu\n", (ULONG)n, 0, 0);
    if (!insert_sample_doc(rtb, n))
    {
        DisposeObject(rtb);
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
        GA_Text, "rtbtest: scroll / click links / controls",
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
        WA_Title, "richtextbrowser rtbtest",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_InnerWidth, 420,
        WA_InnerHeight, 280,
        WA_IDCMP, IDCMP_INTUITICKS,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_ParentGroup, layout,
    EndWindow;

    if (winobj == NULL)
    {
        log_msg("rtbtest: FAIL WindowObject\n");
        DisposeObject(rtb);
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }

    log_msg("rtbtest: RA_OpenWindow\n");
    win = (struct Window *)RA_OpenWindow(winobj);
    if (win == NULL)
    {
        log_msg("rtbtest: FAIL RA_OpenWindow\n");
        DisposeObject(winobj);
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

    log_msg("rtbtest: event loop (scroll should track live; Ctrl-C/close)\n");
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
    CloseLibrary(RichTextBrowserBase);
    log_msg("rtbtest: done OK\n");
    return RETURN_OK;
}
