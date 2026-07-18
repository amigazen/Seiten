/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_timeline.c - richtextbrowser timeline, zebra pens, embed thumbs
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/amiatp.h>
#include <gadgets/richtextbrowser.h>
#include <gadgets/scroller.h>
#include <graphics/view.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"
#include "utf8fold.h"
#include "imgload.h"
#include "rtb_profile.h"
#include <libraries/tb.h>
#include <libraries/tb_symbols.h>

static void
tl_free_cursor(struct SeitenGui *gui)
{
    if (gui->sg_Cursor != NULL) {
        FreeVec(gui->sg_Cursor);
        gui->sg_Cursor = NULL;
    }
}

static BOOL
tl_store_cursor(struct SeitenGui *gui, STRPTR cursor)
{
    ULONG len;

    tl_free_cursor(gui);
    if (cursor == NULL || cursor[0] == '\0') {
        gui->sg_Exhausted = TRUE;
        return TRUE;
    }
    len = strlen((char *)cursor) + 1;
    gui->sg_Cursor = (STRPTR)AllocVec(len, MEMF_ANY);
    if (gui->sg_Cursor == NULL) {
        return FALSE;
    }
    strcpy((char *)gui->sg_Cursor, (char *)cursor);
    gui->sg_Exhausted = FALSE;
    return TRUE;
}

static void
tl_sync_scroller(struct SeitenGui *gui)
{
    ULONG top;
    ULONG tot;
    ULONG vis;

    if (gui->sg_Rtb == NULL || gui->sg_Scroller == NULL)
        return;

    top = 0;
    tot = 0;
    vis = 0;
    GetAttr(RTB_Top, gui->sg_Rtb, &top);
    GetAttr(RTB_Total, gui->sg_Rtb, &tot);
    GetAttr(RTB_Visible, gui->sg_Rtb, &vis);

    if (gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_Scroller, gui->sg_Window,
            NULL,
            SCROLLER_Top, top,
            SCROLLER_Total, tot > 0 ? tot : 1,
            SCROLLER_Visible, vis > 0 ? vis : 1,
            TAG_DONE);
    } else {
        SetAttrs(gui->sg_Scroller,
            SCROLLER_Top, top,
            SCROLLER_Total, tot > 0 ? tot : 1,
            SCROLLER_Visible, vis > 0 ? vis : 1,
            TAG_DONE);
    }
}

static void
tl_set_busy(struct SeitenGui *gui, BOOL busy)
{
    if (gui->sg_Rtb == NULL)
        return;
    if (gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL,
            RTB_Busy, busy, TAG_DONE);
    } else {
        SetAttrs(gui->sg_Rtb, RTB_Busy, busy, TAG_DONE);
    }
}

static void
tl_free_rows(struct SeitenGui *gui)
{
    struct SeitenRow *row;
    struct SeitenRow *next;

    tl_set_busy(gui, TRUE);
    if (gui->sg_Rtb != NULL) {
        DoMethod(gui->sg_Rtb, RTBM_CLEAR, NULL);
    }

    for (row = (struct SeitenRow *)gui->sg_Posts.mlh_Head;
        row->sr_Node.mln_Succ != NULL;
        row = next) {
        next = (struct SeitenRow *)row->sr_Node.mln_Succ;
        Remove((struct Node *)&row->sr_Node);
        SeitenImgCancelRow(gui, row);
        SeitenRowFree(row);
    }
    NewList((struct List *)&gui->sg_Posts);
    gui->sg_NodeCount = 0;
    tl_set_busy(gui, FALSE);
    tl_sync_scroller(gui);
}

static void
tl_trim_head(struct SeitenGui *gui)
{
    struct SeitenRow *row;

    while (gui->sg_NodeCount > SEITEN_NODE_CAP) {
        row = (struct SeitenRow *)gui->sg_Posts.mlh_Head;
        if (row == NULL || row->sr_Node.mln_Succ == NULL)
            break;
        if (gui->sg_Rtb != NULL) {
            /* PostBlockId is the HBOX row root (frees nested image/para/ctl). */
            if (row->sr_PostBlockId != 0)
                DoMethod(gui->sg_Rtb, RTBM_REMOVEBLOCK, NULL,
                    row->sr_PostBlockId);
            else if (row->sr_ImageBlockId != 0)
                DoMethod(gui->sg_Rtb, RTBM_REMOVEBLOCK, NULL,
                    row->sr_ImageBlockId);
        }
        Remove((struct Node *)&row->sr_Node);
        SeitenImgCancelRow(gui, row);
        SeitenRowFree(row);
        gui->sg_NodeCount--;
    }
}

static BOOL
tl_append_feed(struct SeitenGui *gui, struct AtpFeed *feed, ULONG start_index)
{
    ULONG i;
    ULONG n;

    n = AtpFeedGetCount(feed);
    tl_set_busy(gui, TRUE);
    for (i = 0; i < n; i++) {
        struct AtpFeedPost *post;

        post = AtpFeedGetPost(feed, i);
        if (post == NULL)
            continue;
        if (!SeitenRtbAppendPost(gui, post, start_index + i)) {
            tl_set_busy(gui, FALSE);
            return FALSE;
        }
    }
    tl_trim_head(gui);
    tl_set_busy(gui, FALSE);
    if (gui->sg_Rtb != NULL)
        DoMethod(gui->sg_Rtb, RTBM_INVALIDATE, NULL, 0, 0);
    tl_sync_scroller(gui);

    if (!tl_store_cursor(gui, AtpFeedGetCursor(feed)))
        return FALSE;
    if (n == 0)
        gui->sg_Exhausted = TRUE;
    /* Ahead-of-time CDN kick happens after Loading clears (Reload/LoadMore). */
    return TRUE;
}

BOOL
SeitenTimelineInit(struct SeitenGui *gui)
{
    NewList((struct List *)&gui->sg_Posts);
    gui->sg_Cursor = NULL;
    gui->sg_NodeCount = 0;
    gui->sg_Exhausted = FALSE;
    gui->sg_Loading = FALSE;
    gui->sg_PenFg = 1;
    gui->sg_PenFill = 3;
    gui->sg_PenBgEven = 0;
    gui->sg_PenBgOdd = 2;
    gui->sg_PenOddOwned = FALSE;
    gui->sg_ColorMap = NULL;
    gui->sg_TickCount = 0;
    gui->sg_FeedTBLike = NULL;
    gui->sg_FeedTBRepost = NULL;
    gui->sg_FeedTBReply = NULL;
    gui->sg_FeedImgLike = NULL;
    gui->sg_FeedImgRepost = NULL;
    gui->sg_FeedImgReply = NULL;
    gui->sg_FeedImgLikeSel = NULL;
    gui->sg_FeedImgRepostSel = NULL;
    gui->sg_FeedImgReplySel = NULL;
    return TRUE;
}

static void
tl_free_feed_icons(struct SeitenGui *gui)
{
    if (gui == NULL)
        return;
    gui->sg_FeedImgLike = NULL;
    gui->sg_FeedImgRepost = NULL;
    gui->sg_FeedImgReply = NULL;
    gui->sg_FeedImgLikeSel = NULL;
    gui->sg_FeedImgRepostSel = NULL;
    gui->sg_FeedImgReplySel = NULL;
    if (gui->sg_FeedTBLike != NULL) {
        DisposeTBImage(gui->sg_FeedTBLike);
        gui->sg_FeedTBLike = NULL;
    }
    if (gui->sg_FeedTBRepost != NULL) {
        DisposeTBImage(gui->sg_FeedTBRepost);
        gui->sg_FeedTBRepost = NULL;
    }
    if (gui->sg_FeedTBReply != NULL) {
        DisposeTBImage(gui->sg_FeedTBReply);
        gui->sg_FeedTBReply = NULL;
    }
}

static void
tl_load_feed_icons(struct SeitenGui *gui, struct Screen *scr)
{
    struct TagItem tags[2];

    if (gui == NULL || scr == NULL || !gui->sg_TBInited)
        return;
    if (gui->sg_FeedImgLike != NULL)
        return;
    if (TBImagesExists() != TBERR_NOERROR)
        return;

    tags[0].ti_Tag = TBA_SizeMask;
    tags[0].ti_Data = TBA_Size16;
    tags[1].ti_Tag = TAG_DONE;
    tags[1].ti_Data = 0;

    gui->sg_FeedTBLike = NewTBImage((STRPTR)TB_SYM_FAVOURITES, scr,
        PRECISION_ICON, tags);
    gui->sg_FeedTBRepost = NewTBImage((STRPTR)TB_SYM_COPY, scr,
        PRECISION_ICON, tags);
    gui->sg_FeedTBReply = NewTBImage((STRPTR)TB_SYM_MAIL, scr,
        PRECISION_ICON, tags);

    /*
     * NewBitMapFromTBImage(..., copy=TRUE) always fails (copy unimplemented).
     * Use shared bitmap.image objects owned by the TBImage cache.
     */
    if (gui->sg_FeedTBLike != NULL) {
        gui->sg_FeedImgLike = NewBitmapImageFromTBImage(gui->sg_FeedTBLike,
            TBA_Size16, TB_STATE_NORMAL, NULL);
        gui->sg_FeedImgLikeSel = NewBitmapImageFromTBImage(gui->sg_FeedTBLike,
            TBA_Size16, TB_STATE_SELECTED, NULL);
    }
    if (gui->sg_FeedTBRepost != NULL) {
        gui->sg_FeedImgRepost = NewBitmapImageFromTBImage(gui->sg_FeedTBRepost,
            TBA_Size16, TB_STATE_NORMAL, NULL);
        gui->sg_FeedImgRepostSel = NewBitmapImageFromTBImage(
            gui->sg_FeedTBRepost, TBA_Size16, TB_STATE_SELECTED, NULL);
    }
    if (gui->sg_FeedTBReply != NULL) {
        gui->sg_FeedImgReply = NewBitmapImageFromTBImage(gui->sg_FeedTBReply,
            TBA_Size16, TB_STATE_NORMAL, NULL);
        gui->sg_FeedImgReplySel = NewBitmapImageFromTBImage(gui->sg_FeedTBReply,
            TBA_Size16, TB_STATE_SELECTED, NULL);
    }
}

void
SeitenTimelineInitPens(struct SeitenGui *gui)
{
    struct DrawInfo *dri;
    struct Screen *scr;
    LONG pen;

    if (gui == NULL || gui->sg_Window == NULL)
        return;
    scr = gui->sg_Window->WScreen;
    if (scr == NULL)
        return;
    dri = GetScreenDrawInfo(scr);
    if (dri == NULL)
        return;
    gui->sg_PenFg = dri->dri_Pens[TEXTPEN];
    gui->sg_PenFill = dri->dri_Pens[FILLPEN];
    gui->sg_PenBgEven = dri->dri_Pens[BACKGROUNDPEN];
    FreeScreenDrawInfo(scr, dri);

    gui->sg_ColorMap = scr->ViewPort.ColorMap;
    gui->sg_PenOddOwned = FALSE;
    pen = -1;
    if (gui->sg_ColorMap != NULL) {
        pen = ObtainBestPen(gui->sg_ColorMap,
            0xCCCCCCCCUL, 0xCCCCCCCCUL, 0xCCCCCCCCUL,
            OBP_Precision, PRECISION_GUI,
            TAG_DONE);
    }
    if (pen >= 0) {
        gui->sg_PenBgOdd = (WORD)pen;
        gui->sg_PenOddOwned = TRUE;
    } else {
        dri = GetScreenDrawInfo(scr);
        if (dri != NULL) {
            gui->sg_PenBgOdd = dri->dri_Pens[SHADOWPEN];
            FreeScreenDrawInfo(scr, dri);
        }
    }

    tl_load_feed_icons(gui, scr);

    if (gui->sg_Rtb != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL,
            RTB_BackgroundPen, gui->sg_PenBgEven,
            RTB_LinkPen, gui->sg_PenFill,
            TAG_DONE);
        RefreshGList((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL, 1);
    }
}

void
SeitenTimelineFreePens(struct SeitenGui *gui)
{
    if (gui == NULL)
        return;
    tl_free_feed_icons(gui);
    if (gui->sg_PenOddOwned && gui->sg_ColorMap != NULL) {
        ReleasePen(gui->sg_ColorMap, gui->sg_PenBgOdd);
        gui->sg_PenOddOwned = FALSE;
    }
    gui->sg_ColorMap = NULL;
}

void
SeitenTimelineFree(struct SeitenGui *gui)
{
    tl_free_rows(gui);
    tl_free_cursor(gui);
}

BOOL
SeitenTimelineReload(struct SeitenGui *gui)
{
    struct AtpFeed *feed;
    LONG err;
    BOOL ok;

    if (gui->sg_Loading)
        return FALSE;
    if (!gui->sg_LoggedIn) {
        SeitenGuiSetStatus(gui, (STRPTR)"Not logged in");
        return FALSE;
    }

    gui->sg_Loading = TRUE;
    SeitenGuiSetStatus(gui, (STRPTR)"Loading timeline...");
    PutStr("Seiten: timeline reload begin\n");
    Flush(Output());
    if (gui->sg_WinObj != NULL)
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, TRUE, TAG_DONE);

    tl_free_rows(gui);
    PutStr("Seiten: timeline cleared\n");
    Flush(Output());
    tl_free_cursor(gui);
    gui->sg_Exhausted = FALSE;

    feed = NULL;
    err = SeitenFetchTimeline(&gui->eng, SEITEN_PAGE_LIMIT, NULL, &feed);
    PutStr("Seiten: timeline fetch done\n");
    Flush(Output());
    ok = FALSE;
    if (err == 0 && feed != NULL) {
        PutStr("Seiten: timeline append begin\n");
        Flush(Output());
        ok = tl_append_feed(gui, feed, 0);
        DisposeAtpFeed(feed);
        PutStr("Seiten: timeline append done\n");
        Flush(Output());
    }

    if (gui->sg_Rtb != NULL && gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL,
            RTB_Top, 0, TAG_DONE);
        RefreshGList((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL, 1);
    }
    tl_sync_scroller(gui);
    PutStr("Seiten: timeline refresh/sync done\n");
    Flush(Output());

    if (gui->sg_WinObj != NULL)
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
    gui->sg_Loading = FALSE;
    /* Do not Kick here — sync CDN during reload blocks quit. INTUITICK loads. */
    gui->sg_ImgStop = FALSE;

    if (ok) {
        UBYTE buf[64];

        sprintf((char *)buf, "Timeline (%lu)",
            (unsigned long)gui->sg_NodeCount);
        SeitenGuiSetStatus(gui, buf);
    } else {
        SeitenGuiSetStatus(gui, (STRPTR)"Timeline failed");
    }
    return ok;
}

BOOL
SeitenTimelineLoadMore(struct SeitenGui *gui)
{
    struct AtpFeed *feed;
    LONG err;
    BOOL ok;
    ULONG top;
    ULONG start;

    if (gui->sg_Loading || gui->sg_Exhausted || !gui->sg_LoggedIn)
        return FALSE;
    if (gui->sg_Cursor == NULL) {
        gui->sg_Exhausted = TRUE;
        return FALSE;
    }

    gui->sg_Loading = TRUE;
    SeitenGuiSetStatus(gui, (STRPTR)"Loading more...");
    if (gui->sg_WinObj != NULL)
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, TRUE, TAG_DONE);

    top = 0;
    if (gui->sg_Rtb != NULL)
        GetAttr(RTB_Top, gui->sg_Rtb, &top);

    start = gui->sg_NodeCount;

    feed = NULL;
    err = SeitenFetchTimeline(&gui->eng, SEITEN_PAGE_LIMIT, gui->sg_Cursor,
        &feed);
    ok = FALSE;
    if (err == 0 && feed != NULL) {
        ok = tl_append_feed(gui, feed, start);
        DisposeAtpFeed(feed);
    }

    if (gui->sg_Rtb != NULL && gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL,
            RTB_Top, top, TAG_DONE);
        RefreshGList((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL, 1);
    }
    tl_sync_scroller(gui);

    if (gui->sg_WinObj != NULL)
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
    gui->sg_Loading = FALSE;
    /* INTUITICK drives CDN — avoid sync GET chains that block quit. */

    if (ok) {
        UBYTE buf[64];

        sprintf((char *)buf, "Timeline (%lu)",
            (unsigned long)gui->sg_NodeCount);
        SeitenGuiSetStatus(gui, buf);
    } else {
        gui->sg_Exhausted = TRUE;
        SeitenGuiSetStatus(gui, (STRPTR)"Load more failed");
    }
    return ok;
}

void
SeitenTimelineLoadNextImage(struct SeitenGui *gui)
{
    if (gui == NULL || gui->sg_ImgStop)
        return;
    SeitenImgKick(gui);
    SeitenImgFlushRefresh(gui);
}

void
SeitenTimelineCheckScroll(struct SeitenGui *gui)
{
    ULONG top;
    ULONG vis;
    ULONG tot;

    if (gui->sg_Rtb == NULL || gui->sg_Loading)
        return;

    /* Drive CDN queue while idle — one image per tick.
     * Flush before/after Kick so paint never nests inside NewDTObject. */
    SeitenImgFlushRefresh(gui);
    SeitenImgKick(gui);
    SeitenImgFlushRefresh(gui);

    if (gui->sg_Exhausted)
        return;

    top = 0;
    vis = 0;
    tot = 0;
    GetAttr(RTB_Top, gui->sg_Rtb, &top);
    GetAttr(RTB_Visible, gui->sg_Rtb, &vis);
    GetAttr(RTB_Total, gui->sg_Rtb, &tot);
    if (tot > 0 && top + vis + 48 >= tot)
        SeitenTimelineLoadMore(gui);
}

void
SeitenTimelineScroller(struct SeitenGui *gui)
{
    ULONG stop;
    ULONG rtop;

    if (gui == NULL || gui->sg_Rtb == NULL || gui->sg_Scroller == NULL)
        return;
    stop = 0;
    rtop = 0;
    GetAttr(SCROLLER_Top, gui->sg_Scroller, &stop);
    GetAttr(RTB_Top, gui->sg_Rtb, &rtop);
    if (stop == rtop)
        return;
    /* SetGadgetAttrs already redraws via ObtainGIRPort — no RefreshGList. */
    if (gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL,
            RTB_Top, stop, TAG_DONE);
    } else {
        SetAttrs(gui->sg_Rtb, RTB_Top, stop, TAG_DONE);
    }
}

struct SeitenRow *
SeitenTimelineGetSelected(struct SeitenGui *gui)
{
    APTR user;

    if (gui == NULL || gui->sg_Rtb == NULL)
        return NULL;
    user = NULL;
    GetAttr(RTB_SelectedUser, gui->sg_Rtb, (ULONG *)&user);
    return (struct SeitenRow *)user;
}
