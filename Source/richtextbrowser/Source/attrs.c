/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * attrs.c - OM_GET / OM_SET for RTB_* attributes
 *
 * Visual attrs follow page.image: store value, mark viewport cache dirty.
 * Never ObtainGIRPort / GM_RENDER from OM_SET (deadlocks under OpenWindow).
 * Caller RefreshGList or next natural GM_RENDER.
 */

#include "classbase.h"

ULONG
getRtbAttr(struct ClassBase *cb, Class *cl, Object *o, struct opGet *msg)
{
    struct localData *ld;
    ULONG retval;

    (void)cb;
    ld = INST_DATA(cl, o);
    retval = 1;

    switch (msg->opg_AttrID)
    {
        case RTB_Top:
            *msg->opg_Storage = (ULONG)ld->ld_Top;
            break;
        case RTB_Total:
            *msg->opg_Storage = (ULONG)ld->ld_Total;
            break;
        case RTB_Visible:
            *msg->opg_Storage = (ULONG)ld->ld_Visible;
            break;
        case RTB_Horiz:
            *msg->opg_Storage = (ULONG)ld->ld_Horiz;
            break;
        case RTB_Document:
            *msg->opg_Storage = (ULONG)ld->ld_Doc;
            break;
        case RTB_BlockCap:
            *msg->opg_Storage = ld->ld_BlockCap;
            break;
        case RTB_Overscan:
            *msg->opg_Storage = (ULONG)ld->ld_Overscan;
            break;
        case RTB_Busy:
            *msg->opg_Storage = (ULONG)ld->ld_Busy;
            break;
        case RTB_BackgroundPen:
            *msg->opg_Storage = (ULONG)ld->ld_BgPen;
            break;
        case RTB_LinkPen:
            *msg->opg_Storage = (ULONG)ld->ld_LinkPen;
            break;
        case RTB_QuoteBarPen:
            *msg->opg_Storage = (ULONG)ld->ld_QuoteBarPen;
            break;
        case RTB_ReadOnly:
            *msg->opg_Storage = (ULONG)ld->ld_ReadOnly;
            break;
        case RTB_SelectBlocks:
            *msg->opg_Storage = (ULONG)ld->ld_SelectBlocks;
            break;
        case RTB_RelEvent:
            *msg->opg_Storage = ld->ld_RelEvent;
            break;
        case RTB_HitBlock:
            *msg->opg_Storage = ld->ld_HitBlock;
            break;
        case RTB_HitRun:
            *msg->opg_Storage = ld->ld_HitRun;
            break;
        case RTB_HitKind:
            *msg->opg_Storage = ld->ld_HitKind;
            break;
        case RTB_HitUser:
            *msg->opg_Storage = (ULONG)ld->ld_HitUser;
            break;
        case RTB_SelectedBlock:
            *msg->opg_Storage = ld->ld_SelectedBlock;
            break;
        case RTB_SelectedUser:
            *msg->opg_Storage = (ULONG)ld->ld_SelectedUser;
            break;
        default:
            retval = 0;
            break;
    }
    return retval;
}

ULONG
setRtbAttrs(struct ClassBase *cb, Class *cl, Object *o, struct opSet *msg)
{
    struct localData *ld;
    struct TagItem *tstate;
    struct TagItem *tag;
    BOOL relayout;

    ld = INST_DATA(cl, o);
    relayout = FALSE;
    ld->ld_InSet = TRUE;

    /*
     * SetGadgetAttrs supplies GInfo — allow content on a later GM_RENDER
     * that is NOT nested inside this OM_SET (intuition often refreshes
     * before SetGadgetAttrs returns; that nested paint must stay cheap).
     */
    if (msg->ops_GInfo != NULL)
        ld->ld_ContentOk = TRUE;

    tstate = msg->ops_AttrList;
    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case RTB_Top:
                /* Like PIMA_PageTop: store + dirty cache; no OM_NOTIFY. */
                ld->ld_Top = (LONG)tag->ti_Data;
                rtbClampTop(ld);
                ld->ld_CacheDirty = TRUE;
                break;

            case RTB_Document:
                if (tag->ti_Data == ~0UL)
                {
                    if (ld->ld_Doc != NULL && !ld->ld_DocOwned)
                        ld->ld_Doc = NULL;
                    rtbEnsureDoc(cb, ld);
                    rtbClearDocContents(cb, ld);
                }
                else
                {
                    if (ld->ld_Doc != NULL && ld->ld_DocOwned)
                    {
                        rtbClearDocContents(cb, ld);
                        FreeVec(ld->ld_Doc);
                    }
                    ld->ld_Doc = (struct RtbDocument *)tag->ti_Data;
                    ld->ld_DocOwned = FALSE;
                    if (ld->ld_Doc == NULL)
                        rtbEnsureDoc(cb, ld);
                }
                relayout = TRUE;
                break;

            case RTB_BlockCap:
                ld->ld_BlockCap = tag->ti_Data;
                break;

            case RTB_Overscan:
                ld->ld_Overscan = (LONG)tag->ti_Data;
                ld->ld_CacheDirty = TRUE;
                break;

            case RTB_Busy:
                ld->ld_Busy = (BOOL)tag->ti_Data;
                if (!ld->ld_Busy && ld->ld_LayoutDirty)
                    relayout = TRUE;
                break;

            case RTB_Horiz:
                ld->ld_Horiz = (BOOL)tag->ti_Data;
                break;

            case RTB_BackgroundPen:
                ld->ld_BgPen = (WORD)tag->ti_Data;
                ld->ld_CacheDirty = TRUE;
                break;

            case RTB_LinkPen:
                ld->ld_LinkPen = (WORD)tag->ti_Data;
                ld->ld_CacheDirty = TRUE;
                break;

            case RTB_QuoteBarPen:
                ld->ld_QuoteBarPen = (WORD)tag->ti_Data;
                ld->ld_CacheDirty = TRUE;
                break;

            case RTB_ReadOnly:
                ld->ld_ReadOnly = (BOOL)tag->ti_Data;
                break;

            case RTB_SelectBlocks:
                ld->ld_SelectBlocks = (BOOL)tag->ti_Data;
                break;

            case RTB_WithBevel:
                ld->ld_WithBevel = (BOOL)tag->ti_Data;
                ld->ld_CacheDirty = TRUE;
                break;

            case GA_TextAttr:
                ld->ld_TextAttr = (struct TextAttr *)tag->ti_Data;
                if (ld->ld_Font != NULL)
                {
                    CloseFont(ld->ld_Font);
                    ld->ld_Font = NULL;
                }
                if (ld->ld_TextAttr != NULL)
                {
                    /* Prefer ROM OpenFont; diskfont only outside GM_RENDER. */
                    ld->ld_Font = OpenFont(ld->ld_TextAttr);
                    if (ld->ld_Font == NULL && DiskfontBase != NULL &&
                        !ld->ld_InRender)
                        ld->ld_Font = OpenDiskFont(ld->ld_TextAttr);
                }
                if (ld->ld_Font == NULL)
                    rtbOpenDefaultFont(cb, ld);
                ld->ld_Plot.cp_Font = ld->ld_Font;
                relayout = TRUE;
                break;

            default:
                break;
        }
    }

    if (relayout)
        rtbMarkLayoutDirty(ld);

    ld->ld_InSet = FALSE;

    /*
     * TickBox: after OM_SET returns its nested cheap paint, ObtainGIRPort
     * + GM_RENDER while InSet is FALSE. Avoids RefreshGList from the app
     * (also layer-locked) for the first content paint.
     */
    if (msg->ops_GInfo != NULL && ld->ld_ContentOk && !ld->ld_Busy &&
        !ld->ld_InRender)
        rtbRedrawGIRPort(cb, cl, o, msg->ops_GInfo);

    return 1;
}
