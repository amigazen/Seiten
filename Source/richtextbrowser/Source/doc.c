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
        case RTBB_IMAGEGRID:
            rtbFreeBlockList(cb, &blk->rb_Data.grid.children);
            break;
        case RTBB_CONTROLROW:
            rtbFreeRunList(cb, &blk->rb_Data.controlrow.controls);
            break;
        case RTBB_IMAGE:
            if (blk->rb_Data.image.alt != NULL)
                FreeVec(blk->rb_Data.image.alt);
            break;
        case RTBB_EMBED:
            if (blk->rb_Data.embed.title != NULL)
                FreeVec(blk->rb_Data.embed.title);
            if (blk->rb_Data.embed.description != NULL)
                FreeVec(blk->rb_Data.embed.description);
            if (blk->rb_Data.embed.site != NULL)
                FreeVec(blk->rb_Data.embed.site);
            if (blk->rb_Data.embed.href != NULL)
                FreeVec(blk->rb_Data.embed.href);
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

static struct RtbBlock *rtb_find_block_in(struct RtbBlock *blk, RTB_BlockID id);
static void rtb_assign_ids(struct RtbDocument *doc, struct RtbBlock *blk);

static struct RtbBlock *
rtb_find_block_in(struct RtbBlock *blk, RTB_BlockID id)
{
    struct RtbBlock *ch;
    struct RtbBlock *found;

    if (blk == NULL || id == 0)
        return NULL;
    if (blk->rb_ID == id)
        return blk;
    if (blk->rb_Type != RTBB_HBOX && blk->rb_Type != RTBB_VBOX &&
        blk->rb_Type != RTBB_QUOTE && blk->rb_Type != RTBB_IMAGEGRID)
        return NULL;
    if (blk->rb_Type == RTBB_IMAGEGRID)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.grid.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            found = rtb_find_block_in(ch, id);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    for (ch = (struct RtbBlock *)blk->rb_Data.box.children.mlh_Head;
        ch->rb_Node.mln_Succ != NULL;
        ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
    {
        found = rtb_find_block_in(ch, id);
        if (found != NULL)
            return found;
    }
    return NULL;
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
        struct RtbBlock *found;

        found = rtb_find_block_in(blk, id);
        if (found != NULL)
            return found;
    }
    return NULL;
}

/* Assign block + run IDs depth-first (nested HBOX/VBOX/QUOTE). */
static void
rtb_assign_ids(struct RtbDocument *doc, struct RtbBlock *blk)
{
    struct MinList *list;
    struct RtbRun *r;
    struct RtbBlock *ch;

    if (doc == NULL || blk == NULL)
        return;

    if (blk->rb_ID == 0)
    {
        blk->rb_ID = doc->rd_NextBlockID++;
        if (doc->rd_NextBlockID == 0)
            doc->rd_NextBlockID = 1;
    }

    list = NULL;
    if (blk->rb_Type == RTBB_PARAGRAPH || blk->rb_Type == RTBB_HEADING)
        list = &blk->rb_Data.flow.runs;
    else if (blk->rb_Type == RTBB_CONTROLROW)
        list = &blk->rb_Data.controlrow.controls;
    if (list != NULL)
    {
        for (r = (struct RtbRun *)list->mlh_Head;
            r->rr_Node.mln_Succ != NULL;
            r = (struct RtbRun *)r->rr_Node.mln_Succ)
        {
            if (r->rr_ID == 0)
            {
                r->rr_ID = doc->rd_NextRunID++;
                if (doc->rd_NextRunID == 0)
                    doc->rd_NextRunID = 1;
            }
        }
    }

    if (blk->rb_Type == RTBB_HBOX || blk->rb_Type == RTBB_VBOX ||
        blk->rb_Type == RTBB_QUOTE)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.box.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            rtb_assign_ids(doc, ch);
        }
    }
    else if (blk->rb_Type == RTBB_IMAGEGRID)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.grid.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            rtb_assign_ids(doc, ch);
        }
    }
}

static struct RtbRun *
rtb_find_run_in(struct RtbBlock *blk, RTB_RunID id)
{
    struct MinList *list;
    struct RtbRun *r;
    struct RtbBlock *ch;

    if (blk == NULL || id == 0)
        return NULL;

    list = NULL;
    if (blk->rb_Type == RTBB_PARAGRAPH || blk->rb_Type == RTBB_HEADING)
        list = &blk->rb_Data.flow.runs;
    else if (blk->rb_Type == RTBB_CONTROLROW)
        list = &blk->rb_Data.controlrow.controls;
    if (list != NULL)
    {
        for (r = (struct RtbRun *)list->mlh_Head;
            r->rr_Node.mln_Succ != NULL;
            r = (struct RtbRun *)r->rr_Node.mln_Succ)
        {
            if (r->rr_ID == id)
                return r;
        }
    }

    if (blk->rb_Type == RTBB_HBOX || blk->rb_Type == RTBB_VBOX ||
        blk->rb_Type == RTBB_QUOTE)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.box.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            r = rtb_find_run_in(ch, id);
            if (r != NULL)
                return r;
        }
    }
    else if (blk->rb_Type == RTBB_IMAGEGRID)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.grid.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            r = rtb_find_run_in(ch, id);
            if (r != NULL)
                return r;
        }
    }
    return NULL;
}

struct RtbRun *
rtbFindRun(struct localData *ld, RTB_RunID id)
{
    struct RtbBlock *blk;

    if (ld == NULL || ld->ld_Doc == NULL || id == 0)
        return NULL;
    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = (struct RtbBlock *)blk->rb_Node.mln_Succ)
    {
        struct RtbRun *r;

        r = rtb_find_run_in(blk, id);
        if (r != NULL)
            return r;
    }
    return NULL;
}

static void
rtb_clear_selected(struct RtbBlock *blk)
{
    struct RtbBlock *ch;

    if (blk == NULL)
        return;
    blk->rb_Flags &= ~RTBBF_SELECTED;
    if (blk->rb_Type == RTBB_HBOX || blk->rb_Type == RTBB_VBOX ||
        blk->rb_Type == RTBB_QUOTE)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.box.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            rtb_clear_selected(ch);
        }
    }
    else if (blk->rb_Type == RTBB_IMAGEGRID)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.grid.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            rtb_clear_selected(ch);
        }
    }
}

void
rtbSelectBlockId(struct localData *ld, RTB_BlockID id)
{
    struct RtbBlock *blk;
    struct RtbBlock *found;

    if (ld == NULL || ld->ld_Doc == NULL)
        return;
    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = (struct RtbBlock *)blk->rb_Node.mln_Succ)
    {
        rtb_clear_selected(blk);
    }
    found = rtbFindBlock(ld, id);
    if (found != NULL)
        found->rb_Flags |= RTBBF_SELECTED;
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
    ld->ld_FontsWarmed = FALSE;
    rtbFaceFlushAll(cb, ld);
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
    rtb_assign_ids(ld->ld_Doc, msg->Block);

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
    struct MinNode *pred;
    struct MinNode *succ;

    ld = INST_DATA(cl, o);
    if (msg->Block == NULL || msg->Block->rb_ID == 0)
        return 0;
    old = rtbFindBlock(ld, msg->Block->rb_ID);
    if (old == NULL)
        return 0;

    /* Splice in-place so nested HBOX/VBOX children stay in their list. */
    pred = old->rb_Node.mln_Pred;
    succ = old->rb_Node.mln_Succ;
    Remove((struct Node *)old);
    rtbFreeBlock(cb, old);

    rtb_assign_ids(ld->ld_Doc, msg->Block);
    msg->Block->rb_Node.mln_Pred = pred;
    msg->Block->rb_Node.mln_Succ = succ;
    if (pred != NULL)
        pred->mln_Succ = &msg->Block->rb_Node;
    if (succ != NULL)
        succ->mln_Pred = &msg->Block->rb_Node;

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

/*
 * Return document-space Y/height for lazy CDN: prefer the top-level ancestor
 * so nested avatar/media IDs still map to the feed row's visible band.
 */
ULONG
rtbMethodBlockBounds(struct ClassBase *cb, Class *cl, Object *o,
    struct rtbBlockBounds *msg)
{
    struct localData *ld;
    struct RtbBlock *top;
    struct RtbBlock *blk;
    struct RtbBlock *found;

    (void)cb;
    ld = INST_DATA(cl, o);
    msg->OutY = 0;
    msg->OutHeight = 0;
    if (msg->BlockID == 0 || ld->ld_Doc == NULL)
        return 0;

    found = NULL;
    for (top = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        top->rb_Node.mln_Succ != NULL;
        top = (struct RtbBlock *)top->rb_Node.mln_Succ)
    {
        blk = rtb_find_block_in(top, msg->BlockID);
        if (blk != NULL)
        {
            found = top; /* top-level row bounds */
            break;
        }
    }
    if (found == NULL)
        return 0;
    msg->OutY = found->rb_Y;
    msg->OutHeight = found->rb_Height;
    return 1;
}
