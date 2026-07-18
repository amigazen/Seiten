/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * doc.c - document ownership and RTBM_* helpers
 *
 * AllocRtb* / FreeRtb* are LVOs on richtextbrowser.gadget (apps call via
 * RichTextBrowserBase). Internal free/alloc here uses classbase.h + cb.
 */

#include "classbase.h"

static void rtbFreeRun(struct ClassBase *cb, struct RtbRun *run);
static void rtbFreeBlock(struct ClassBase *cb, struct RtbBlock *blk);

static void
rtbFreeRunList(struct ClassBase *cb, struct MinList *list)
{
    struct RtbRun *run;
    struct RtbRun *next;

    if (list == NULL)
        return;
    for (run = (struct RtbRun *)list->mlh_Head;
        run->rr_Node.mln_Succ != NULL;
        run = next)
    {
        next = (struct RtbRun *)run->rr_Node.mln_Succ;
        Remove((struct Node *)run);
        rtbFreeRun(cb, run);
    }
}

static void
rtbFreeBlockList(struct ClassBase *cb, struct MinList *list)
{
    struct RtbBlock *blk;
    struct RtbBlock *next;

    if (list == NULL)
        return;
    for (blk = (struct RtbBlock *)list->mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = next)
    {
        next = (struct RtbBlock *)blk->rb_Node.mln_Succ;
        Remove((struct Node *)blk);
        rtbFreeBlock(cb, blk);
    }
}

static void
rtbFreeRun(struct ClassBase *cb, struct RtbRun *run)
{
    if (run == NULL)
        return;
    switch (run->rr_Kind)
    {
        case RTBR_TEXT:
            if (run->rr_Data.text.text != NULL)
                FreeVec(run->rr_Data.text.text);
            break;
        case RTBR_LINK:
            if (run->rr_Data.link.href != NULL)
                FreeVec(run->rr_Data.link.href);
            if (run->rr_Data.link.text != NULL)
                FreeVec(run->rr_Data.link.text);
            break;
        case RTBR_CONTROL:
            if (run->rr_Data.control.label != NULL)
                FreeVec(run->rr_Data.control.label);
            break;
        default:
            break;
    }
    if (run->rr_FontName != NULL)
        FreeVec(run->rr_FontName);
    FreeVec(run);
}

static void
rtbFreeBlock(struct ClassBase *cb, struct RtbBlock *blk)
{
    if (blk == NULL)
        return;
    switch (blk->rb_Type)
    {
        case RTBB_PARAGRAPH:
        case RTBB_HEADING:
            rtbFreeRunList(cb, &blk->rb_Data.flow.runs);
            break;
        case RTBB_HBOX:
        case RTBB_VBOX:
        case RTBB_QUOTE:
            rtbFreeBlockList(cb, &blk->rb_Data.box.children);
            break;
        case RTBB_CONTROLROW:
            rtbFreeRunList(cb, &blk->rb_Data.controlrow.controls);
            break;
        case RTBB_IMAGE:
            if (blk->rb_Data.image.alt != NULL)
                FreeVec(blk->rb_Data.image.alt);
            break;
        default:
            break;
    }
    FreeVec(blk);
}

void
rtbEnsureDoc(struct ClassBase *cb, struct localData *ld)
{
    struct RtbDocument *doc;

    if (ld->ld_Doc != NULL)
        return;
    doc = (struct RtbDocument *)AllocVec(sizeof(*doc), MEMF_CLEAR);
    if (doc == NULL)
        return;
    NewList((struct List *)&doc->rd_Blocks);
    doc->rd_NextBlockID = 1;
    doc->rd_NextRunID = 1;
    ld->ld_Doc = doc;
    ld->ld_DocOwned = TRUE;
}

void
rtbClearDocContents(struct ClassBase *cb, struct localData *ld)
{
    struct RtbBlock *blk;
    struct RtbBlock *next;

    if (ld->ld_Doc == NULL)
        return;
    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = next)
    {
        next = (struct RtbBlock *)blk->rb_Node.mln_Succ;
        Remove((struct Node *)blk);
        rtbFreeBlock(cb, blk);
    }
    NewList((struct List *)&ld->ld_Doc->rd_Blocks);
    ld->ld_Total = 0;
    ld->ld_Top = 0;
    ld->ld_SelectedBlock = 0;
    ld->ld_SelectedUser = NULL;
    rtbMarkLayoutDirty(ld);
}

void
rtbFreeOwnedDoc(struct ClassBase *cb, struct localData *ld)
{
    if (ld->ld_Doc == NULL)
        return;
    /* Only free blocks when we own the document; else app retains it. */
    if (ld->ld_DocOwned)
    {
        rtbClearDocContents(cb, ld);
        FreeVec(ld->ld_Doc);
    }
    ld->ld_Doc = NULL;
    ld->ld_DocOwned = FALSE;
}

struct RtbBlock *
rtbFindBlock(struct localData *ld, RTB_BlockID id)
{
    struct RtbBlock *blk;

    if (ld == NULL || ld->ld_Doc == NULL || id == 0)
        return NULL;
    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = (struct RtbBlock *)blk->rb_Node.mln_Succ)
    {
        if (blk->rb_ID == id)
            return blk;
    }
    return NULL;
}

void
rtbMarkLayoutDirty(struct localData *ld)
{
    ld->ld_LayoutDirty = TRUE;
    ld->ld_CacheDirty = TRUE;
}

void
rtbClampTop(struct localData *ld)
{
    LONG maxTop;

    maxTop = ld->ld_Total - ld->ld_Visible;
    if (maxTop < 0)
        maxTop = 0;
    if (ld->ld_Top < 0)
        ld->ld_Top = 0;
    if (ld->ld_Top > maxTop)
        ld->ld_Top = maxTop;
}

void
rtbNotify(struct ClassBase *cb, Class *cl, Object *o,
    struct GadgetInfo *gi, ULONG tag1, ...)
{
    struct TagItem buf[12];
    ULONG i;
    ULONG t;
    ULONG d;
    va_list ap;
    struct TagItem *map;
    struct opUpdate opu;

    (void)cb;
    i = 0;
    t = tag1;
    va_start(ap, tag1);
    while (t != TAG_DONE && i < 11)
    {
        d = va_arg(ap, ULONG);
        buf[i].ti_Tag = t;
        buf[i].ti_Data = d;
        i++;
        t = va_arg(ap, ULONG);
    }
    va_end(ap);
    buf[i].ti_Tag = TAG_DONE;
    buf[i].ti_Data = 0;

    map = buf;
    opu.MethodID = OM_NOTIFY;
    opu.opu_AttrList = map;
    opu.opu_GInfo = gi;
    opu.opu_Flags = 0;
    DoSuperMethodA(cl, o, (Msg)&opu);
}

void
rtbNotifyScroll(struct ClassBase *cb, Class *cl, Object *o,
    struct GadgetInfo *gi)
{
    struct localData *ld;

    ld = INST_DATA(cl, o);
    rtbNotify(cb, cl, o, gi,
        RTB_Top, (ULONG)ld->ld_Top,
        RTB_Total, (ULONG)ld->ld_Total,
        RTB_Visible, (ULONG)ld->ld_Visible,
        TAG_DONE);
}

void
rtbRelayout(struct ClassBase *cb, Class *cl, Object *o,
    struct GadgetInfo *gi, BOOL doNotify)
{
    struct localData *ld;

    ld = INST_DATA(cl, o);

    if (ld->ld_InnerWidth <= 0)
    {
        ld->ld_Visible = 0;
        return;
    }

    /* No PutStr when called from GM_RENDER (doNotify FALSE). */
    if (doNotify)
        RTB_LOG(cb, "rtb: measure begin\n");
    rtbMeasureDocument(cb, ld);
    ld->ld_Visible = ld->ld_InnerHeight;
    rtbClampTop(ld);
    ld->ld_LayoutDirty = FALSE;
    if (doNotify)
        RTB_LOG(cb, "rtb: measure done\n");

    /* Never OM_NOTIFY from GM_RENDER — that deadlocks intuition. */
    if (doNotify && gi != NULL && !ld->ld_Busy)
        rtbNotifyScroll(cb, cl, o, gi);
}

ULONG
rtbMethodClear(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbGeneral *msg)
{
    struct localData *ld;

    ld = INST_DATA(cl, o);
    rtbEnsureDoc(cb, ld);
    rtbClearDocContents(cb, ld);
    rtbRelayout(cb, cl, o, msg->GInfo, TRUE);
    return 1;
}

ULONG
rtbMethodSetDocument(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbSetDocument *msg)
{
    struct localData *ld;

    ld = INST_DATA(cl, o);

    /* Drop previous owned doc without freeing app's new pointer */
    if (ld->ld_Doc != NULL && ld->ld_DocOwned)
    {
        rtbClearDocContents(cb, ld);
        FreeVec(ld->ld_Doc);
    }
    ld->ld_Doc = msg->Document;
    ld->ld_DocOwned = FALSE;
    if (ld->ld_Doc == NULL)
    {
        rtbEnsureDoc(cb, ld);
    }
    rtbMarkLayoutDirty(ld);
    rtbRelayout(cb, cl, o, msg->GInfo, TRUE);
    return 1;
}

ULONG
rtbMethodInsertBlock(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbInsertBlock *msg)
{
    struct localData *ld;
    struct RtbBlock *after;
    ULONG count;

    ld = INST_DATA(cl, o);
    if (msg->Block == NULL)
        return 0;
    rtbEnsureDoc(cb, ld);

    if (msg->Block->rb_ID == 0)
    {
        msg->Block->rb_ID = ld->ld_Doc->rd_NextBlockID++;
        if (ld->ld_Doc->rd_NextBlockID == 0)
            ld->ld_Doc->rd_NextBlockID = 1;
    }

    if (msg->AfterID == (RTB_BlockID)~0UL)
    {
        AddHead((struct List *)&ld->ld_Doc->rd_Blocks,
            (struct Node *)msg->Block);
    }
    else if (msg->AfterID == 0)
    {
        AddTail((struct List *)&ld->ld_Doc->rd_Blocks,
            (struct Node *)msg->Block);
    }
    else
    {
        after = rtbFindBlock(ld, msg->AfterID);
        if (after != NULL)
            Insert((struct List *)&ld->ld_Doc->rd_Blocks,
                (struct Node *)msg->Block, (struct Node *)after);
        else
            AddTail((struct List *)&ld->ld_Doc->rd_Blocks,
                (struct Node *)msg->Block);
    }

    /* Soft cap: drop oldest from head */
    if (ld->ld_BlockCap > 0)
    {
        count = 0;
        {
            struct RtbBlock *b;

            for (b = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
                b->rb_Node.mln_Succ != NULL;
                b = (struct RtbBlock *)b->rb_Node.mln_Succ)
                count++;
        }
        while (count > ld->ld_BlockCap)
        {
            struct RtbBlock *head;

            head = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
            if (head->rb_Node.mln_Succ == NULL)
                break;
            Remove((struct Node *)head);
            rtbFreeBlock(cb, head);
            count--;
        }
    }

    rtbMarkLayoutDirty(ld);
    if (!ld->ld_Busy)
        rtbRelayout(cb, cl, o, msg->GInfo, TRUE);
    return 1;
}

ULONG
rtbMethodRemoveBlock(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbRemoveBlock *msg)
{
    struct localData *ld;
    struct RtbBlock *blk;

    ld = INST_DATA(cl, o);
    blk = rtbFindBlock(ld, msg->BlockID);
    if (blk == NULL)
        return 0;
    if (ld->ld_SelectedBlock == blk->rb_ID)
    {
        ld->ld_SelectedBlock = 0;
        ld->ld_SelectedUser = NULL;
    }
    Remove((struct Node *)blk);
    rtbFreeBlock(cb, blk);
    rtbMarkLayoutDirty(ld);
    if (!ld->ld_Busy)
        rtbRelayout(cb, cl, o, msg->GInfo, TRUE);
    return 1;
}

ULONG
rtbMethodUpdateBlock(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbUpdateBlock *msg)
{
    struct localData *ld;
    struct RtbBlock *old;
    struct Node *pred;

    ld = INST_DATA(cl, o);
    if (msg->Block == NULL || msg->Block->rb_ID == 0)
        return 0;
    old = rtbFindBlock(ld, msg->Block->rb_ID);
    if (old == NULL)
        return 0;
    pred = (struct Node *)old->rb_Node.mln_Pred;
    Remove((struct Node *)old);
    rtbFreeBlock(cb, old);
    Insert((struct List *)&ld->ld_Doc->rd_Blocks,
        (struct Node *)msg->Block, pred);
    rtbMarkLayoutDirty(ld);
    if (!ld->ld_Busy)
        rtbRelayout(cb, cl, o, msg->GInfo, TRUE);
    return 1;
}

ULONG
rtbMethodInvalidate(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbInvalidate *msg)
{
    struct localData *ld;

    ld = INST_DATA(cl, o);
    (void)msg;
    rtbMarkLayoutDirty(ld);
    if (!ld->ld_Busy)
        rtbRelayout(cb, cl, o, msg->GInfo, TRUE);
    return 1;
}

ULONG
rtbMethodScrollTo(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbScrollTo *msg)
{
    struct localData *ld;
    struct RtbBlock *blk;

    ld = INST_DATA(cl, o);
    if (msg->BlockID != 0)
    {
        blk = rtbFindBlock(ld, msg->BlockID);
        if (blk != NULL)
            ld->ld_Top = blk->rb_Y;
    }
    else
    {
        ld->ld_Top = msg->PixelY;
    }
    rtbClampTop(ld);
    rtbNotifyScroll(cb, cl, o, msg->GInfo);
    return 1;
}

ULONG
rtbMethodHitTest(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbHitTest *msg)
{
    struct localData *ld;
    WORD gx;
    WORD gy;

    ld = INST_DATA(cl, o);
    /* Method mouse coords are gadget-relative; paint space is window-rel. */
    gx = (WORD)(ld->ld_Left + msg->MouseX);
    gy = (WORD)(ld->ld_TopEdge + msg->MouseY);
    if (rtbHitAt(cb, ld, gx, gy, msg))
        return 1;
    return 0;
}
