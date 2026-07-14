/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_timeline.c - listbrowser timeline, zebra pens, embed thumbs
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/listbrowser.h>
#include <proto/amiatp.h>
#include <graphics/view.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"
#include "utf8fold.h"
#include "imgload.h"

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
tl_detach(struct SeitenGui *gui)
{
    if (gui->sg_ListBrowser == NULL) {
        return;
    }
    if (gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_ListBrowser, gui->sg_Window,
            NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
    } else {
        SetAttrs(gui->sg_ListBrowser, LISTBROWSER_Labels, ~0, TAG_DONE);
    }
}

static void
tl_attach(struct SeitenGui *gui, ULONG top)
{
    if (gui->sg_ListBrowser == NULL) {
        return;
    }
    if (gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_ListBrowser, gui->sg_Window,
            NULL,
            LISTBROWSER_Labels, (ULONG)&gui->sg_Posts,
            LISTBROWSER_Top, top,
            TAG_DONE);
        RefreshGList((struct Gadget *)gui->sg_ListBrowser, gui->sg_Window,
            NULL, 1);
    } else {
        SetAttrs(gui->sg_ListBrowser,
            LISTBROWSER_Labels, (ULONG)&gui->sg_Posts,
            LISTBROWSER_Top, top,
            TAG_DONE);
    }
}

static void
tl_free_node(struct Node *n)
{
    struct SeitenRow *row;

    row = NULL;
    GetListBrowserNodeAttrs(n, LBNA_UserData, (ULONG *)&row, TAG_DONE);
    if (row != NULL) {
        SeitenRowFree(row);
    }
    FreeListBrowserNode(n);
}

static void
tl_free_nodes(struct SeitenGui *gui)
{
    struct Node *n;
    struct Node *next;

    tl_detach(gui);
    for (n = gui->sg_Posts.lh_Head;
        n->ln_Succ != NULL;
        n = next) {
        next = n->ln_Succ;
        Remove(n);
        tl_free_node(n);
    }
    NewList(&gui->sg_Posts);
    gui->sg_NodeCount = 0;
}

static void
tl_trim_head(struct SeitenGui *gui)
{
    struct Node *n;

    while (gui->sg_NodeCount > SEITEN_NODE_CAP) {
        n = gui->sg_Posts.lh_Head;
        if (n == NULL || n->ln_Succ == NULL) {
            break;
        }
        Remove(n);
        tl_free_node(n);
        gui->sg_NodeCount--;
    }
}

static STRPTR
tl_pick_image_url(struct AtpFeedPost *post)
{
    STRPTR url;

    if (post == NULL) {
        return NULL;
    }
    if (AtpFeedPostGetImageCount(post) > 0) {
        url = AtpFeedPostGetImageUrl(post, 0);
        if (url != NULL) {
            return url;
        }
    }
    return AtpFeedPostGetAvatarUrl(post);
}

static BOOL
tl_add_post(struct SeitenGui *gui, struct AtpFeedPost *post, ULONG index)
{
    struct Node *node;
    struct SeitenRow *row;
    UBYTE label[SEITEN_LABEL_MAX];
    UBYTE folded[SEITEN_LABEL_MAX];
    STRPTR author;
    STRPTR text;
    STRPTR imgurl;
    WORD bg;
    ULONG n;

    author = AtpFeedPostGetAuthorHandle(post);
    text = AtpFeedPostGetText(post);
    if (author == NULL) {
        author = (STRPTR)"?";
    }
    if (text == NULL) {
        text = (STRPTR)"";
    }

    Utf8ToAmigaDisplay(folded, sizeof(folded), text);

    n = 0;
    label[0] = '\0';
    if (n + 1 < sizeof(label)) {
        label[n++] = '@';
        label[n] = '\0';
    }
    while (*author != '\0' && n + 1 < sizeof(label)) {
        label[n++] = *author++;
        label[n] = '\0';
    }
    if (n + 1 < sizeof(label)) {
        label[n++] = '\n';
        label[n] = '\0';
    }
    {
        STRPTR p;

        p = folded;
        while (*p != '\0' && n + 1 < sizeof(label)) {
            label[n++] = *p++;
            label[n] = '\0';
        }
    }

    imgurl = tl_pick_image_url(post);
    row = SeitenRowNew(imgurl, AtpFeedPostGetUri(post),
        AtpFeedPostGetAuthorHandle(post));
    /* Always keep a row so selection can yield uri/handle even without image */
    if (row == NULL) {
        return FALSE;
    }

    bg = ((index & 1) == 0) ? gui->sg_PenBgEven : gui->sg_PenBgOdd;

    node = AllocListBrowserNode(2,
        LBNA_Flags, LBFLG_CUSTOMPENS,
        LBNA_UserData, (ULONG)row,
        LBNA_Column, 0,
        LBNCA_CopyText, TRUE,
        LBNCA_Text, (ULONG)" ",
        LBNCA_FGPen, gui->sg_PenFg,
        LBNCA_BGPen, bg,
        LBNA_Column, 1,
        LBNCA_WordWrap, TRUE,
        LBNCA_CopyText, TRUE,
        LBNCA_Text, (ULONG)label,
        LBNCA_FGPen, gui->sg_PenFg,
        LBNCA_BGPen, bg,
        TAG_DONE);
    if (node == NULL) {
        SeitenRowFree(row);
        return FALSE;
    }
    AddTail(&gui->sg_Posts, node);
    gui->sg_NodeCount++;
    return TRUE;
}

static BOOL
tl_append_feed(struct SeitenGui *gui, struct AtpFeed *feed, ULONG start_index)
{
    ULONG i;
    ULONG n;

    n = AtpFeedGetCount(feed);
    for (i = 0; i < n; i++) {
        struct AtpFeedPost *post;

        post = AtpFeedGetPost(feed, i);
        if (post == NULL) {
            continue;
        }
        if (!tl_add_post(gui, post, start_index + i)) {
            return FALSE;
        }
    }
    tl_trim_head(gui);
    if (!tl_store_cursor(gui, AtpFeedGetCursor(feed))) {
        return FALSE;
    }
    if (n == 0) {
        gui->sg_Exhausted = TRUE;
    }
    return TRUE;
}

BOOL
SeitenTimelineInit(struct SeitenGui *gui)
{
    NewList(&gui->sg_Posts);
    /* Col 0: thumb; col 1: wrapped text; sentinel width -1 */
    gui->sg_Columns[0].ci_Width = SEITEN_THUMB_W + 8;
    gui->sg_Columns[0].ci_Title = NULL;
    gui->sg_Columns[0].ci_Flags = CIF_FIXED;
    gui->sg_Columns[1].ci_Width = 100;
    gui->sg_Columns[1].ci_Title = NULL;
    gui->sg_Columns[1].ci_Flags = CIF_WEIGHTED;
    gui->sg_Columns[2].ci_Width = -1;
    gui->sg_Columns[2].ci_Title = NULL;
    gui->sg_Columns[2].ci_Flags = 0;
    gui->sg_Cursor = NULL;
    gui->sg_NodeCount = 0;
    gui->sg_Exhausted = FALSE;
    gui->sg_Loading = FALSE;
    gui->sg_PenFg = 1;
    gui->sg_PenBgEven = 0;
    gui->sg_PenBgOdd = 2;
    gui->sg_PenOddOwned = FALSE;
    gui->sg_ColorMap = NULL;
    return TRUE;
}

void
SeitenTimelineInitPens(struct SeitenGui *gui)
{
    struct DrawInfo *dri;
    struct Screen *scr;
    LONG pen;

    if (gui == NULL || gui->sg_Window == NULL) {
        return;
    }
    scr = gui->sg_Window->WScreen;
    if (scr == NULL) {
        return;
    }
    dri = GetScreenDrawInfo(scr);
    if (dri == NULL) {
        return;
    }
    gui->sg_PenFg = dri->dri_Pens[TEXTPEN];
    gui->sg_PenBgEven = dri->dri_Pens[BACKGROUNDPEN];
    FreeScreenDrawInfo(scr, dri);

    /* Odd rows: soft gray ~#CCCCCC (not SHINEPEN white). */
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
        /* Fallback if the screen cannot allocate another pen */
        dri = GetScreenDrawInfo(scr);
        if (dri != NULL) {
            gui->sg_PenBgOdd = dri->dri_Pens[SHADOWPEN];
            FreeScreenDrawInfo(scr, dri);
        }
    }
}

void
SeitenTimelineFreePens(struct SeitenGui *gui)
{
    if (gui == NULL) {
        return;
    }
    if (gui->sg_PenOddOwned && gui->sg_ColorMap != NULL) {
        ReleasePen(gui->sg_ColorMap, gui->sg_PenBgOdd);
        gui->sg_PenOddOwned = FALSE;
    }
    gui->sg_ColorMap = NULL;
}

void
SeitenTimelineFree(struct SeitenGui *gui)
{
    tl_free_nodes(gui);
    tl_free_cursor(gui);
}

BOOL
SeitenTimelineReload(struct SeitenGui *gui)
{
    struct AtpFeed *feed;
    LONG err;
    BOOL ok;

    if (gui->sg_Loading) {
        return FALSE;
    }
    if (!gui->sg_LoggedIn) {
        SeitenGuiSetStatus(gui, (STRPTR)"Not logged in");
        return FALSE;
    }

    gui->sg_Loading = TRUE;
    SeitenGuiSetStatus(gui, (STRPTR)"Loading timeline...");
    if (gui->sg_WinObj != NULL) {
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
    }

    tl_free_nodes(gui);
    tl_free_cursor(gui);
    gui->sg_Exhausted = FALSE;

    feed = NULL;
    err = SeitenFetchTimeline(&gui->eng, SEITEN_PAGE_LIMIT, NULL, &feed);
    ok = FALSE;
    if (err == 0 && feed != NULL) {
        ok = tl_append_feed(gui, feed, 0);
        DisposeAtpFeed(feed);
    }

    tl_attach(gui, 0);

    if (gui->sg_WinObj != NULL) {
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
    }
    gui->sg_Loading = FALSE;

    if (ok) {
        UBYTE buf[64];

        sprintf((char *)buf, "Timeline (%lu)", (unsigned long)gui->sg_NodeCount);
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

    if (gui->sg_Loading || gui->sg_Exhausted || !gui->sg_LoggedIn) {
        return FALSE;
    }
    if (gui->sg_Cursor == NULL) {
        gui->sg_Exhausted = TRUE;
        return FALSE;
    }

    gui->sg_Loading = TRUE;
    SeitenGuiSetStatus(gui, (STRPTR)"Loading more...");
    if (gui->sg_WinObj != NULL) {
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
    }

    top = 0;
    if (gui->sg_ListBrowser != NULL) {
        GetAttr(LISTBROWSER_Top, gui->sg_ListBrowser, &top);
    }

    start = gui->sg_NodeCount;
    tl_detach(gui);

    feed = NULL;
    err = SeitenFetchTimeline(&gui->eng, SEITEN_PAGE_LIMIT, gui->sg_Cursor,
        &feed);
    ok = FALSE;
    if (err == 0 && feed != NULL) {
        ok = tl_append_feed(gui, feed, start);
        DisposeAtpFeed(feed);
    }

    tl_attach(gui, top);

    if (gui->sg_WinObj != NULL) {
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
    }
    gui->sg_Loading = FALSE;

    if (ok) {
        UBYTE buf[64];

        sprintf((char *)buf, "Timeline (%lu)", (unsigned long)gui->sg_NodeCount);
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
    struct Node *n;
    struct SeitenRow *row;
    struct Screen *scr;
    ULONG top;

    if (gui == NULL || gui->sg_Loading || gui->sg_Window == NULL ||
        !gui->sg_LoggedIn) {
        return;
    }
    scr = gui->sg_Window->WScreen;
    if (scr == NULL) {
        return;
    }

    for (n = gui->sg_Posts.lh_Head; n->ln_Succ != NULL; n = n->ln_Succ) {
        row = NULL;
        GetListBrowserNodeAttrs(n, LBNA_UserData, (ULONG *)&row, TAG_DONE);
        if (row == NULL || row->sr_Tried || row->sr_ImageUrl == NULL) {
            continue;
        }

        if (!SeitenRowLoadImage(row, &gui->eng, scr)) {
            return;
        }
        if (row->sr_BitMapObj == NULL) {
            return;
        }

        top = 0;
        GetAttr(LISTBROWSER_Top, gui->sg_ListBrowser, &top);
        tl_detach(gui);
        SetListBrowserNodeAttrs(n,
            LBNA_Column, 0,
            LBNCA_Image, (ULONG)row->sr_BitMapObj,
            TAG_DONE);
        tl_attach(gui, top);
        return;
    }
}

void
SeitenTimelineCheckScroll(struct SeitenGui *gui)
{
    ULONG top;
    ULONG vis;
    ULONG tot;

    if (gui->sg_ListBrowser == NULL || gui->sg_Loading) {
        return;
    }

    /* One CDN thumb decode per tick keeps the UI responsive */
    SeitenTimelineLoadNextImage(gui);

    if (gui->sg_Exhausted) {
        return;
    }
    top = 0;
    vis = 0;
    tot = 0;
    GetAttr(LISTBROWSER_VPropTop, gui->sg_ListBrowser, &top);
    GetAttr(LISTBROWSER_VPropVisible, gui->sg_ListBrowser, &vis);
    GetAttr(LISTBROWSER_VPropTotal, gui->sg_ListBrowser, &tot);
    if (tot > 0 && top + vis + 3 >= tot) {
        SeitenTimelineLoadMore(gui);
    }
}

struct SeitenRow *
SeitenTimelineGetSelected(struct SeitenGui *gui)
{
    struct Node *sel;
    struct SeitenRow *row;

    if (gui == NULL || gui->sg_ListBrowser == NULL) {
        return NULL;
    }
    sel = NULL;
    GetAttr(LISTBROWSER_SelectedNode, gui->sg_ListBrowser, (ULONG *)&sel);
    if (sel == NULL) {
        return NULL;
    }
    row = NULL;
    GetListBrowserNodeAttrs(sel, LBNA_UserData, (ULONG *)&row, TAG_DONE);
    return row;
}
