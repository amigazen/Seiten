/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * hit.c - pointer hit-testing against laid-out runs/blocks
 */

#include "classbase.h"

static BOOL
rtb_hit_run_list(struct MinList *list, WORD relX, WORD relY,
    struct rtbHitTest *out, RTB_BlockID blockId, APTR blockUser)
{
    struct RtbRun *run;

    for (run = (struct RtbRun *)list->mlh_Head;
        run->rr_Node.mln_Succ != NULL;
        run = (struct RtbRun *)run->rr_Node.mln_Succ)
    {
        if (relX < run->rr_X || relY < run->rr_Y)
            continue;
        if (relX >= run->rr_X + run->rr_W ||
            relY >= run->rr_Y + run->rr_H)
            continue;

        out->BlockID = blockId;
        out->RunID = run->rr_ID;
        out->User = blockUser;

        if (run->rr_Kind == RTBR_LINK)
        {
            out->HitKind = RTBH_LINK;
            if (run->rr_Data.link.user != NULL)
                out->User = run->rr_Data.link.user;
            return TRUE;
        }
        if (run->rr_Kind == RTBR_IMAGE)
        {
            out->HitKind = RTBH_IMAGE;
            return TRUE;
        }
        if (run->rr_Kind == RTBR_CONTROL)
        {
            out->HitKind = RTBH_CONTROL;
            if (run->rr_Data.control.user != NULL)
                out->User = run->rr_Data.control.user;
            return TRUE;
        }
        out->HitKind = RTBH_BLOCK;
        return TRUE;
    }
    return FALSE;
}

static BOOL
rtb_hit_block_at(struct RtbBlock *blk, WORD localX, WORD localY,
    struct rtbHitTest *out)
{
    if (localY < 0 || localY >= blk->rb_Height)
        return FALSE;
    if (localX < 0)
        return FALSE;

    switch (blk->rb_Type)
    {
        case RTBB_PARAGRAPH:
        case RTBB_HEADING:
            if (rtb_hit_run_list(&blk->rb_Data.flow.runs, localX, localY,
                    out, blk->rb_ID, blk->rb_User))
                return TRUE;
            out->BlockID = blk->rb_ID;
            out->RunID = 0;
            out->HitKind = RTBH_BLOCK;
            out->User = blk->rb_User;
            return TRUE;

        case RTBB_IMAGE:
            out->BlockID = blk->rb_ID;
            out->RunID = 0;
            out->HitKind = RTBH_IMAGE;
            out->User = blk->rb_User;
            return TRUE;

        case RTBB_CONTROLROW:
            if (rtb_hit_run_list(&blk->rb_Data.controlrow.controls,
                    localX, localY, out, blk->rb_ID, blk->rb_User))
                return TRUE;
            out->BlockID = blk->rb_ID;
            out->HitKind = RTBH_BLOCK;
            out->User = blk->rb_User;
            return TRUE;

        case RTBB_VBOX:
        case RTBB_QUOTE:
            {
                struct RtbBlock *ch;
                WORD ox;

                ox = (blk->rb_Type == RTBB_QUOTE) ? 8 : 0;
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.box.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    if (rtb_hit_block_at(ch, (WORD)(localX - ox),
                            (WORD)(localY - ch->rb_Y), out))
                    {
                        if (out->User == NULL)
                            out->User = blk->rb_User;
                        return TRUE;
                    }
                }
            }
            out->BlockID = blk->rb_ID;
            out->HitKind = RTBH_BLOCK;
            out->User = blk->rb_User;
            return TRUE;

        case RTBB_HBOX:
            {
                struct RtbBlock *ch;
                WORD cx;

                cx = 0;
                for (ch = (struct RtbBlock *)
                        blk->rb_Data.box.children.mlh_Head;
                    ch->rb_Node.mln_Succ != NULL;
                    ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
                {
                    WORD cw;

                    cw = ch->rb_Width > 0 ? ch->rb_Width : 16;
                    if (localX >= cx && localX < cx + cw)
                    {
                        if (rtb_hit_block_at(ch, (WORD)(localX - cx),
                                localY, out))
                        {
                            if (out->User == NULL)
                                out->User = blk->rb_User;
                            return TRUE;
                        }
                    }
                    cx = (WORD)(cx + cw + 4);
                }
            }
            out->BlockID = blk->rb_ID;
            out->HitKind = RTBH_BLOCK;
            out->User = blk->rb_User;
            return TRUE;

        case RTBB_RULE:
        case RTBB_SPACER:
            out->BlockID = blk->rb_ID;
            out->HitKind = RTBH_BLOCK;
            out->User = blk->rb_User;
            return TRUE;

        default:
            break;
    }
    return FALSE;
}

BOOL
rtbHitAt(struct ClassBase *cb, struct localData *ld,
    WORD gx, WORD gy, struct rtbHitTest *out)
{
    struct RtbBlock *blk;
    WORD docX;
    LONG docY;

    (void)cb;
    if (out == NULL || ld->ld_Doc == NULL)
        return FALSE;

    out->BlockID = 0;
    out->RunID = 0;
    out->HitKind = RTBH_NONE;
    out->User = NULL;

    if (gx < ld->ld_InnerLeft ||
        gy < ld->ld_InnerTop ||
        gx >= ld->ld_InnerLeft + ld->ld_InnerWidth ||
        gy >= ld->ld_InnerTop + ld->ld_InnerHeight)
        return FALSE;

    docX = (WORD)(gx - ld->ld_InnerLeft);
    docY = ld->ld_Top + (gy - ld->ld_InnerTop);

    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = (struct RtbBlock *)blk->rb_Node.mln_Succ)
    {
        if (docY < blk->rb_Y || docY >= blk->rb_Y + blk->rb_Height)
            continue;
        if (rtb_hit_block_at(blk, docX, (WORD)(docY - blk->rb_Y), out))
            return TRUE;
    }
    return FALSE;
}
