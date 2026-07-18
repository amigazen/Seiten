/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtballoc.c - AllocRtb* / FreeRtb* LVOs (listbrowser AllocListBrowserNodeA
 * pattern). Apps call these via RichTextBrowserBase after OpenLibrary —
 * never link a static helper .o into the application.
 */

#include "classbase.h"

static void free_run(struct ClassBase *cb, struct RtbRun *run);
static void free_block(struct ClassBase *cb, struct RtbBlock *blk);

static void
free_run_list(struct ClassBase *cb, struct MinList *list)
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
        free_run(cb, run);
    }
}

static void
free_block_list(struct ClassBase *cb, struct MinList *list)
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
        free_block(cb, blk);
    }
}

static void
free_run(struct ClassBase *cb, struct RtbRun *run)
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
free_block(struct ClassBase *cb, struct RtbBlock *blk)
{
    if (blk == NULL)
        return;
    switch (blk->rb_Type)
    {
        case RTBB_PARAGRAPH:
        case RTBB_HEADING:
            free_run_list(cb, &blk->rb_Data.flow.runs);
            break;
        case RTBB_HBOX:
        case RTBB_VBOX:
        case RTBB_QUOTE:
            free_block_list(cb, &blk->rb_Data.box.children);
            break;
        case RTBB_CONTROLROW:
            free_run_list(cb, &blk->rb_Data.controlrow.controls);
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

static struct RtbRun *
alloc_run(struct ClassBase *cb, UWORD kind)
{
    struct RtbRun *run;

    run = (struct RtbRun *)AllocVec(sizeof(*run), MEMF_CLEAR);
    if (run == NULL)
        return NULL;
    run->rr_Kind = kind;
    run->rr_Fg = (ULONG)~0;
    run->rr_Bg = (ULONG)~0;
    return run;
}

static struct RtbBlock *
alloc_block(struct ClassBase *cb, UWORD type)
{
    struct RtbBlock *blk;

    blk = (struct RtbBlock *)AllocVec(sizeof(*blk), MEMF_CLEAR);
    if (blk == NULL)
        return NULL;
    blk->rb_Type = type;
    blk->rb_Fg = (ULONG)~0;
    blk->rb_Bg = (ULONG)~0;
    switch (type)
    {
        case RTBB_PARAGRAPH:
        case RTBB_HEADING:
            NewList((struct List *)&blk->rb_Data.flow.runs);
            break;
        case RTBB_HBOX:
        case RTBB_VBOX:
        case RTBB_QUOTE:
            NewList((struct List *)&blk->rb_Data.box.children);
            break;
        case RTBB_CONTROLROW:
            NewList((struct List *)&blk->rb_Data.controlrow.controls);
            break;
        case RTBB_SPACER:
            blk->rb_Data.spacer.pixels = 8;
            break;
        case RTBB_RULE:
            blk->rb_Data.rule.thickness = 1;
            break;
        default:
            break;
    }
    return blk;
}

static STRPTR
dup_str(struct ClassBase *cb, CONST_STRPTR s)
{
    ULONG len;
    STRPTR d;

    if (s == NULL)
        return NULL;
    len = strlen((char *)s) + 1;
    d = (STRPTR)AllocVec(len, MEMF_ANY);
    if (d == NULL)
        return NULL;
    CopyMem((APTR)s, (APTR)d, len);
    return d;
}

static BOOL
add_run(struct RtbBlock *blk, struct RtbRun *run)
{
    if (blk == NULL || run == NULL)
        return FALSE;
    if (blk->rb_Type == RTBB_PARAGRAPH || blk->rb_Type == RTBB_HEADING)
    {
        AddTail((struct List *)&blk->rb_Data.flow.runs, (struct Node *)run);
        return TRUE;
    }
    if (blk->rb_Type == RTBB_CONTROLROW)
    {
        AddTail((struct List *)&blk->rb_Data.controlrow.controls,
            (struct Node *)run);
        return TRUE;
    }
    return FALSE;
}

static void
apply_run_tags(struct ClassBase *cb, struct RtbRun *run, struct TagItem *tags)
{
    struct TagItem *tstate;
    struct TagItem *tag;
    STRPTR s;

    tstate = tags;
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case RTBA_ID:
                run->rr_ID = tag->ti_Data;
                break;
            case RTBA_Style:
                run->rr_Style = tag->ti_Data;
                break;
            case RTBA_FgPen:
                run->rr_Fg = tag->ti_Data;
                break;
            case RTBA_BgPen:
                run->rr_Bg = tag->ti_Data;
                break;
            case RTBA_Size:
                run->rr_Size = (UWORD)tag->ti_Data;
                break;
            case RTBA_FontName:
                if (run->rr_FontName != NULL)
                    FreeVec(run->rr_FontName);
                run->rr_FontName = dup_str(cb, (CONST_STRPTR)tag->ti_Data);
                break;
            case RTBA_Checked:
                if (tag->ti_Data)
                    run->rr_Style |= RTBS_CHECKED;
                else
                    run->rr_Style &= ~RTBS_CHECKED;
                break;
            case RTBA_Text:
                s = dup_str(cb, (CONST_STRPTR)tag->ti_Data);
                if (run->rr_Kind == RTBR_LINK)
                {
                    if (run->rr_Data.link.text != NULL)
                        FreeVec(run->rr_Data.link.text);
                    run->rr_Data.link.text = s;
                    run->rr_Data.link.byteLen =
                        (s != NULL) ? strlen((char *)s) : 0;
                }
                else
                {
                    if (run->rr_Data.text.text != NULL)
                        FreeVec(run->rr_Data.text.text);
                    run->rr_Data.text.text = s;
                    run->rr_Data.text.byteLen =
                        (s != NULL) ? strlen((char *)s) : 0;
                }
                break;
            case RTBA_Href:
                if (run->rr_Data.link.href != NULL)
                    FreeVec(run->rr_Data.link.href);
                run->rr_Data.link.href = dup_str(cb, (CONST_STRPTR)tag->ti_Data);
                break;
            case RTBA_User:
                if (run->rr_Kind == RTBR_LINK)
                    run->rr_Data.link.user = (APTR)tag->ti_Data;
                else if (run->rr_Kind == RTBR_CONTROL)
                    run->rr_Data.control.user = (APTR)tag->ti_Data;
                break;
            case RTBA_BitMapObject:
                run->rr_Data.image.bitmapObj = (Object *)tag->ti_Data;
                break;
            case RTBA_BitMap:
                run->rr_Data.image.bm = (struct BitMap *)tag->ti_Data;
                break;
            case RTBA_Width:
                run->rr_Data.image.w = (UWORD)tag->ti_Data;
                break;
            case RTBA_Height:
                run->rr_Data.image.h = (UWORD)tag->ti_Data;
                break;
            case RTBA_MaxWidth:
                run->rr_Data.image.maxW = (UWORD)tag->ti_Data;
                break;
            case RTBA_CtlKind:
                run->rr_Data.control.ctlKind = (UWORD)tag->ti_Data;
                break;
            case RTBA_CtlId:
                run->rr_Data.control.ctlId = tag->ti_Data;
                break;
            case RTBA_Label:
                if (run->rr_Data.control.label != NULL)
                    FreeVec(run->rr_Data.control.label);
                run->rr_Data.control.label =
                    dup_str(cb, (CONST_STRPTR)tag->ti_Data);
                break;
            default:
                break;
        }
    }
}

static void
apply_block_tags(struct ClassBase *cb, struct RtbBlock *blk,
    struct TagItem *tags)
{
    struct TagItem *tstate;
    struct TagItem *tag;

    tstate = tags;
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case RTBA_ID:
                blk->rb_ID = tag->ti_Data;
                break;
            case RTBA_User:
                blk->rb_User = (APTR)tag->ti_Data;
                break;
            case RTBA_FgPen:
                blk->rb_Fg = tag->ti_Data;
                break;
            case RTBA_BgPen:
                blk->rb_Bg = tag->ti_Data;
                break;
            case RTBA_BitMapObject:
                blk->rb_Data.image.bitmapObj = (Object *)tag->ti_Data;
                break;
            case RTBA_BitMap:
                blk->rb_Data.image.bm = (struct BitMap *)tag->ti_Data;
                break;
            case RTBA_Width:
                if (blk->rb_Type == RTBB_IMAGE)
                    blk->rb_Data.image.w = (UWORD)tag->ti_Data;
                break;
            case RTBA_Height:
                if (blk->rb_Type == RTBB_IMAGE)
                    blk->rb_Data.image.h = (UWORD)tag->ti_Data;
                break;
            case RTBA_MaxWidth:
                if (blk->rb_Type == RTBB_IMAGE)
                    blk->rb_Data.image.maxW = (UWORD)tag->ti_Data;
                break;
            case RTBA_Pixels:
                blk->rb_Data.spacer.pixels = (UWORD)tag->ti_Data;
                break;
            case RTBA_Thickness:
                blk->rb_Data.rule.thickness = (UWORD)tag->ti_Data;
                break;
            case RTBA_Alt:
                if (blk->rb_Data.image.alt != NULL)
                    FreeVec(blk->rb_Data.image.alt);
                blk->rb_Data.image.alt = dup_str(cb, (CONST_STRPTR)tag->ti_Data);
                break;
            case RTBA_Text:
                if (blk->rb_Type == RTBB_PARAGRAPH ||
                    blk->rb_Type == RTBB_HEADING)
                {
                    struct RtbRun *run;
                    STRPTR s;

                    run = alloc_run(cb, RTBR_TEXT);
                    if (run != NULL)
                    {
                        s = dup_str(cb, (CONST_STRPTR)tag->ti_Data);
                        run->rr_Data.text.text = s;
                        run->rr_Data.text.byteLen =
                            (s != NULL) ? strlen((char *)s) : 0;
                        add_run(blk, run);
                    }
                }
                break;
            default:
                break;
        }
    }
}

/* --- LVOs (A6 = ClassBase *) ------------------------------------------- */

struct RtbDocument *
ASM AllocRtbDocument(REG(a6) struct ClassBase *cb)
{
    struct RtbDocument *doc;

    doc = (struct RtbDocument *)AllocVec(sizeof(*doc), MEMF_CLEAR);
    if (doc == NULL)
        return NULL;
    NewList((struct List *)&doc->rd_Blocks);
    doc->rd_NextBlockID = 1;
    doc->rd_NextRunID = 1;
    return doc;
}

void
ASM FreeRtbDocument(REG(a0) struct RtbDocument *doc,
    REG(a6) struct ClassBase *cb)
{
    if (doc == NULL)
        return;
    free_block_list(cb, &doc->rd_Blocks);
    FreeVec(doc);
}

struct RtbBlock *
ASM AllocRtbBlock(REG(d0) ULONG type, REG(a6) struct ClassBase *cb)
{
    return alloc_block(cb, (UWORD)type);
}

void
ASM FreeRtbBlock(REG(a0) struct RtbBlock *blk, REG(a6) struct ClassBase *cb)
{
    free_block(cb, blk);
}

struct RtbRun *
ASM AllocRtbRun(REG(d0) ULONG kind, REG(a6) struct ClassBase *cb)
{
    return alloc_run(cb, (UWORD)kind);
}

void
ASM FreeRtbRun(REG(a0) struct RtbRun *run, REG(a6) struct ClassBase *cb)
{
    free_run(cb, run);
}

struct RtbBlock *
ASM AllocRtbBlockA(REG(d0) ULONG type, REG(a0) struct TagItem *tags,
    REG(a6) struct ClassBase *cb)
{
    struct RtbBlock *blk;

    blk = alloc_block(cb, (UWORD)type);
    if (blk == NULL)
        return NULL;
    if (tags != NULL)
        apply_block_tags(cb, blk, tags);
    return blk;
}

struct RtbRun *
ASM AllocRtbRunA(REG(d0) ULONG kind, REG(a0) struct TagItem *tags,
    REG(a6) struct ClassBase *cb)
{
    struct RtbRun *run;

    run = alloc_run(cb, (UWORD)kind);
    if (run == NULL)
        return NULL;
    if (tags != NULL)
        apply_run_tags(cb, run, tags);
    return run;
}

ULONG
ASM RtbBlockAddRun(REG(a0) struct RtbBlock *blk, REG(a1) struct RtbRun *run,
    REG(a6) struct ClassBase *cb)
{
    (void)cb;
    return add_run(blk, run) ? 1UL : 0UL;
}

ULONG
ASM RtbBlockAddChild(REG(a0) struct RtbBlock *parent,
    REG(a1) struct RtbBlock *child, REG(a6) struct ClassBase *cb)
{
    (void)cb;
    if (parent == NULL || child == NULL)
        return 0;
    if (parent->rb_Type != RTBB_HBOX && parent->rb_Type != RTBB_VBOX &&
        parent->rb_Type != RTBB_QUOTE)
        return 0;
    AddTail((struct List *)&parent->rb_Data.box.children,
        (struct Node *)child);
    return 1;
}
