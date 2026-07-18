/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * dispatch.c - BOOPSI dispatcher for richtextbrowser.gadget
 */

#include "classbase.h"

static Object *newRtb(struct ClassBase *cb, Class *cl, Object *o,
    struct opSet *ops);
static ULONG disposeRtb(struct ClassBase *cb, Class *cl, Object *o,
    Msg msg);

static ULONG
ASM Dispatch(REG(a0) Class *cl, REG(a2) Object *o, REG(a1) Msg msg)
{
    struct ClassBase *cb;
    ULONG retval;

    cb = (struct ClassBase *)cl->cl_UserData;
    retval = 0;

    switch (msg->MethodID)
    {
        case OM_NEW:
            retval = (ULONG)newRtb(cb, cl, o, (struct opSet *)msg);
            break;

        case OM_DISPOSE:
            disposeRtb(cb, cl, o, msg);
            retval = 0;
            break;

        case OM_UPDATE:
        case OM_SET:
            DoSuperMethodA(cl, o, msg);
            setRtbAttrs(cb, cl, o, (struct opSet *)msg);
            retval = 1;
            break;

        case OM_GET:
            retval = getRtbAttr(cb, cl, o, (struct opGet *)msg);
            if (!retval)
                retval = DoSuperMethodA(cl, o, msg);
            break;

        case GM_LAYOUT:
            retval = rtbLayout(cb, cl, o, (struct gpLayout *)msg);
            break;

        case GM_DOMAIN:
            retval = rtbDomain(cb, cl, o, (struct gpDomain *)msg);
            break;

        case GM_RENDER:
            retval = rtbRender(cb, cl, o, (struct gpRender *)msg);
            break;

        case GM_HITTEST:
            retval = rtbHitTest(cb, cl, o, (struct gpHitTest *)msg);
            break;

        case GM_GOACTIVE:
            retval = rtbGoActive(cb, cl, o, (struct gpInput *)msg);
            break;

        case GM_HANDLEINPUT:
            retval = rtbHandleInput(cb, cl, o, (struct gpInput *)msg);
            break;

        case GM_GOINACTIVE:
            retval = rtbGoInactive(cb, cl, o, (struct gpGoInactive *)msg);
            break;

        case RTBM_CLEAR:
            retval = rtbMethodClear(cb, cl, o, (struct rtbGeneral *)msg);
            break;

        case RTBM_SETDOCUMENT:
            retval = rtbMethodSetDocument(cb, cl, o,
                (struct rtbSetDocument *)msg);
            break;

        case RTBM_INSERTBLOCK:
            retval = rtbMethodInsertBlock(cb, cl, o,
                (struct rtbInsertBlock *)msg);
            break;

        case RTBM_REMOVEBLOCK:
            retval = rtbMethodRemoveBlock(cb, cl, o,
                (struct rtbRemoveBlock *)msg);
            break;

        case RTBM_UPDATEBLOCK:
            retval = rtbMethodUpdateBlock(cb, cl, o,
                (struct rtbUpdateBlock *)msg);
            break;

        case RTBM_INVALIDATE:
            retval = rtbMethodInvalidate(cb, cl, o,
                (struct rtbInvalidate *)msg);
            break;

        case RTBM_SCROLLTO:
            retval = rtbMethodScrollTo(cb, cl, o,
                (struct rtbScrollTo *)msg);
            break;

        case RTBM_HITTEST:
            retval = rtbMethodHitTest(cb, cl, o,
                (struct rtbHitTest *)msg);
            break;

        case RTBM_BLOCKBOUNDS:
            retval = rtbMethodBlockBounds(cb, cl, o,
                (struct rtbBlockBounds *)msg);
            break;

        default:
            retval = DoSuperMethodA(cl, o, msg);
            break;
    }

    return retval;
}

Class *
initClass(struct ClassBase *cb)
{
    Class *cl;

    cl = MakeClass(RICHTEXTBROWSERCLASS, "gadgetclass", NULL,
        sizeof(struct localData), 0);
    if (cl == NULL)
        return NULL;

    cl->cl_UserData = (ULONG)cb;
    cl->cl_Dispatcher.h_Entry = (ULONG (*)())Dispatch;
    cl->cl_Dispatcher.h_SubEntry = NULL;

    AddClass(cl);
    return cl;
}

/*
 * Open ROM topaz as bitmap fallback only. Scalable faces (TTEngine / bullet)
 * open later via rtbWarmDocumentFonts once the window is ContentOk.
 */
void
rtbOpenDefaultFont(struct ClassBase *cb, struct localData *ld)
{
    struct TextAttr ta;

    if (ld == NULL || ld->ld_Font != NULL)
        return;

    memset(&ta, 0, sizeof(ta));
    ta.ta_Name = (STRPTR)"topaz.font";
    ta.ta_YSize = 8;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = FPF_ROMFONT;
    if (cb != NULL && GfxBase != NULL)
        ld->ld_Font = OpenFont(&ta);
    ld->ld_Plot.cp_Font = ld->ld_Font;
}

struct TextFont *
rtbFontForSize(struct ClassBase *cb, struct localData *ld, UWORD size)
{
    struct RtbFace *face;

    face = rtbFaceResolve(cb, ld, NULL, size, 0);
    if (face != NULL && face->rf_Bitmap != NULL)
        return face->rf_Bitmap;
    return ld != NULL ? ld->ld_Font : NULL;
}

struct TextFont *
rtbResolveFont(struct ClassBase *cb, struct localData *ld,
    CONST_STRPTR name, UWORD size)
{
    struct RtbFace *face;

    face = rtbFaceResolve(cb, ld, name, size, 0);
    if (face != NULL && face->rf_Bitmap != NULL)
        return face->rf_Bitmap;
    return ld != NULL ? ld->ld_Font : NULL;
}

void
rtbEnsureSysImages(struct ClassBase *cb, struct localData *ld,
    struct DrawInfo *dri)
{
    if (ld == NULL || dri == NULL)
        return;
    ld->ld_DrawInfo = dri;
    if (ld->ld_CheckImg == NULL && cb != NULL)
    {
        /* NewObject needs IntuitionBase via cb macro. */
        ld->ld_CheckImg = NewObject(NULL, "sysiclass",
            SYSIA_Which, CHECKIMAGE,
            SYSIA_DrawInfo, dri,
            TAG_DONE);
    }
}

void
rtbDisposeSysImages(struct ClassBase *cb, struct localData *ld)
{
    if (ld == NULL)
        return;
    if (ld->ld_CheckImg != NULL && cb != NULL)
    {
        DisposeObject(ld->ld_CheckImg);
        ld->ld_CheckImg = NULL;
    }
    ld->ld_DrawInfo = NULL;
}

static Object *
newRtb(struct ClassBase *cb, Class *cl, Object *o, struct opSet *ops)
{
    Object *newObj;
    struct localData *ld;
    struct DrawInfo *dri;

    newObj = (Object *)DoSuperMethodA(cl, o, (Msg)ops);
    if (newObj == NULL)
        return NULL;

    ld = INST_DATA(cl, newObj);
    memset(ld, 0, sizeof(*ld));

    ld->ld_Pool = CreatePool(MEMF_CLEAR | MEMF_PUBLIC, 4096, 1024);
    ld->ld_Overscan = RTB_DEFAULT_OVERSCAN;
    ld->ld_BlockCap = RTB_DEFAULT_BLOCKCAP;
    ld->ld_ReadOnly = TRUE;
    ld->ld_SelectBlocks = TRUE;
    ld->ld_WithBevel = TRUE;
    ld->ld_BgPen = 0;
    ld->ld_TextPen = 1;
    ld->ld_LinkPen = 3;
    ld->ld_QuoteBarPen = 2;
    ld->ld_ShinePen = 2;
    ld->ld_ShadowPen = 1;
    ld->ld_FillPen = 3;
    ld->ld_LayoutDirty = TRUE;
    ld->ld_CacheDirty = TRUE;
    ld->ld_ContentOk = FALSE;

    CP_Init(&ld->ld_Plot, cb);
    rtbOpenDefaultFont(cb, ld);
    rtbEnsureDoc(cb, ld);

    dri = NULL;
    if (ops->ops_GInfo != NULL && ops->ops_GInfo->gi_DrInfo != NULL)
        dri = ops->ops_GInfo->gi_DrInfo;
    if (dri != NULL)
    {
        ld->ld_BgPen = dri->dri_Pens[BACKGROUNDPEN];
        ld->ld_TextPen = dri->dri_Pens[TEXTPEN];
        ld->ld_ShinePen = dri->dri_Pens[SHINEPEN];
        ld->ld_ShadowPen = dri->dri_Pens[SHADOWPEN];
        ld->ld_LinkPen = dri->dri_Pens[FILLPEN];
        ld->ld_QuoteBarPen = dri->dri_Pens[SHADOWPEN];
        ld->ld_FillPen = dri->dri_Pens[FILLPEN];
    }

    setRtbAttrs(cb, cl, newObj, ops);
    if (ld->ld_Font == NULL)
        rtbOpenDefaultFont(cb, ld);
    return newObj;
}

static ULONG
disposeRtb(struct ClassBase *cb, Class *cl, Object *o, Msg msg)
{
    struct localData *ld;

    ld = INST_DATA(cl, o);

    ld->ld_Plot.cp_Font = NULL;
    CP_Done(&ld->ld_Plot);
    CP_BulletClose(cb);
    rtbFreeCache(cb, ld);
    rtbFaceFlushAll(cb, ld);
    rtbFreeMeasureRP(cb, ld);

    if (ld->ld_Font != NULL)
    {
        CloseFont(ld->ld_Font);
        ld->ld_Font = NULL;
    }
    rtbDisposeSysImages(cb, ld);

    rtbFreeOwnedDoc(cb, ld);

    if (ld->ld_Pool != NULL)
    {
        DeletePool(ld->ld_Pool);
        ld->ld_Pool = NULL;
    }

    return DoSuperMethodA(cl, o, msg);
}

ULONG
rtbLayout(struct ClassBase *cb, Class *cl, Object *o, struct gpLayout *msg)
{
    struct localData *ld;
    struct Gadget *gad;
    WORD pad;

    ld = INST_DATA(cl, o);
    gad = (struct Gadget *)o;

    DoSuperMethodA(cl, o, (Msg)msg);

    ld->ld_Left = gad->LeftEdge;
    ld->ld_TopEdge = gad->TopEdge;
    ld->ld_Width = gad->Width;
    ld->ld_Height = gad->Height;

    pad = ld->ld_WithBevel ? RTB_BEVEL_PAD : 0;
    ld->ld_InnerLeft = (WORD)(ld->ld_Left + pad);
    ld->ld_InnerTop = (WORD)(ld->ld_TopEdge + pad);
    ld->ld_InnerWidth = (WORD)(ld->ld_Width - pad * 2);
    ld->ld_InnerHeight = (WORD)(ld->ld_Height - pad * 2);
    if (ld->ld_InnerWidth < 0)
        ld->ld_InnerWidth = 0;
    if (ld->ld_InnerHeight < 0)
        ld->ld_InnerHeight = 0;

    /*
     * Geometry only in GM_LAYOUT. Measure + paint wait for GM_RENDER so we
     * do not run TextLength while layout.gadget holds locks.
     * Never SetAttrs / OM_NOTIFY / ObtainGIRPort here.
     */
    rtbMarkLayoutDirty(ld);
    ld->ld_CacheDirty = TRUE;
    return 1;
}

ULONG
rtbDomain(struct ClassBase *cb, Class *cl, Object *o, struct gpDomain *msg)
{
    (void)cb;
    (void)cl;
    (void)o;

    if (msg->gpd_Which == GDOMAIN_MINIMUM)
    {
        msg->gpd_Domain.Width = 80;
        msg->gpd_Domain.Height = 40;
    }
    else if (msg->gpd_Which == GDOMAIN_NOMINAL)
    {
        msg->gpd_Domain.Width = 200;
        msg->gpd_Domain.Height = 120;
    }
    else
    {
        msg->gpd_Domain.Width = 32767;
        msg->gpd_Domain.Height = 32767;
    }
    return 1;
}

ULONG
rtbRender(struct ClassBase *cb, Class *cl, Object *o, struct gpRender *msg)
{
    struct localData *ld;
    struct RastPort *rp;
    struct ColorMap *cm;
    struct Window *win;
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;

    ld = INST_DATA(cl, o);
    if (ld->ld_InRender)
        return 1;
    ld->ld_InRender = TRUE;

    rp = msg->gpr_RPort;
    if (rp == NULL && msg->gpr_GInfo != NULL)
        rp = msg->gpr_GInfo->gi_RastPort;
    if (rp == NULL)
    {
        ld->ld_InRender = FALSE;
        return 0;
    }

    /*
     * ReAction may refresh without GM_LAYOUT after a window size change.
     * Sync domain so paragraph word-wrap reflows to the new InnerWidth.
     */
    {
        struct Gadget *gad;
        WORD pad;
        WORD nw;
        WORD nh;

        gad = (struct Gadget *)o;
        nw = gad->Width;
        nh = gad->Height;
        if (nw != ld->ld_Width || nh != ld->ld_Height ||
            gad->LeftEdge != ld->ld_Left || gad->TopEdge != ld->ld_TopEdge)
        {
            ld->ld_Left = gad->LeftEdge;
            ld->ld_TopEdge = gad->TopEdge;
            ld->ld_Width = nw;
            ld->ld_Height = nh;
            pad = ld->ld_WithBevel ? RTB_BEVEL_PAD : 0;
            ld->ld_InnerLeft = (WORD)(ld->ld_Left + pad);
            ld->ld_InnerTop = (WORD)(ld->ld_TopEdge + pad);
            ld->ld_InnerWidth = (WORD)(ld->ld_Width - pad * 2);
            ld->ld_InnerHeight = (WORD)(ld->ld_Height - pad * 2);
            if (ld->ld_InnerWidth < 0)
                ld->ld_InnerWidth = 0;
            if (ld->ld_InnerHeight < 0)
                ld->ld_InnerHeight = 0;
            rtbMarkLayoutDirty(ld);
        }
    }

    cm = NULL;
    win = NULL;
    if (msg->gpr_GInfo != NULL)
    {
        win = msg->gpr_GInfo->gi_Window;
        if (win != NULL && win->WScreen != NULL)
            cm = win->WScreen->ViewPort.ColorMap;
        if (msg->gpr_GInfo->gi_DrInfo != NULL)
        {
            ld->ld_BgPen = msg->gpr_GInfo->gi_DrInfo->dri_Pens[BACKGROUNDPEN];
            ld->ld_TextPen = msg->gpr_GInfo->gi_DrInfo->dri_Pens[TEXTPEN];
            ld->ld_ShinePen = msg->gpr_GInfo->gi_DrInfo->dri_Pens[SHINEPEN];
            ld->ld_ShadowPen = msg->gpr_GInfo->gi_DrInfo->dri_Pens[SHADOWPEN];
            ld->ld_FillPen = msg->gpr_GInfo->gi_DrInfo->dri_Pens[FILLPEN];
        }
    }

    if (ld->ld_Font == NULL)
        rtbOpenDefaultFont(cb, ld);
    ld->ld_Plot.cp_Font = ld->ld_Font;

    if (msg->gpr_GInfo != NULL && msg->gpr_GInfo->gi_DrInfo != NULL)
        rtbEnsureSysImages(cb, ld, msg->gpr_GInfo->gi_DrInfo);

    /*
     * Cheap / deferred paint:
     *  - !ContentOk: OpenWindow path — bg + frame only
     *  - InSet: nested under SetGadgetAttrs — draw nothing (avoid wiping);
     *    ObtainGIRPort full paint runs after InSet clears
     */
    if (!ld->ld_ContentOk)
    {
        if (ld->ld_WithBevel && ld->ld_Width > 2 && ld->ld_Height > 2)
        {
            SetAPen(rp, ld->ld_ShadowPen);
            Move(rp, ld->ld_Left, ld->ld_TopEdge);
            Draw(rp, (WORD)(ld->ld_Left + ld->ld_Width - 1), ld->ld_TopEdge);
            Draw(rp, (WORD)(ld->ld_Left + ld->ld_Width - 1),
                (WORD)(ld->ld_TopEdge + ld->ld_Height - 1));
            SetAPen(rp, ld->ld_ShinePen);
            Draw(rp, ld->ld_Left,
                (WORD)(ld->ld_TopEdge + ld->ld_Height - 1));
            Draw(rp, ld->ld_Left, ld->ld_TopEdge);
        }
        x0 = ld->ld_InnerLeft;
        y0 = ld->ld_InnerTop;
        x1 = (WORD)(ld->ld_InnerLeft + ld->ld_InnerWidth - 1);
        y1 = (WORD)(ld->ld_InnerTop + ld->ld_InnerHeight - 1);
        if (ld->ld_InnerWidth > 0 && ld->ld_InnerHeight > 0 &&
            x1 >= x0 && y1 >= y0)
        {
            SetAPen(rp, ld->ld_BgPen);
            SetDrMd(rp, JAM1);
            RectFill(rp, x0, y0, x1, y1);
        }
        ld->ld_InRender = FALSE;
        return 1;
    }
    if (ld->ld_InSet)
    {
        ld->ld_InRender = FALSE;
        return 1;
    }

    if (ld->ld_LayoutDirty)
        rtbRelayout(cb, cl, o, msg->gpr_GInfo, FALSE);

    /* Frame on the GIRPort only — Move/Draw, never bevel.image. */
    if (ld->ld_WithBevel && ld->ld_Width > 2 && ld->ld_Height > 2)
    {
        SetAPen(rp, ld->ld_ShadowPen);
        Move(rp, ld->ld_Left, ld->ld_TopEdge);
        Draw(rp, (WORD)(ld->ld_Left + ld->ld_Width - 1), ld->ld_TopEdge);
        Draw(rp, (WORD)(ld->ld_Left + ld->ld_Width - 1),
            (WORD)(ld->ld_TopEdge + ld->ld_Height - 1));
        SetAPen(rp, ld->ld_ShinePen);
        Draw(rp, ld->ld_Left, (WORD)(ld->ld_TopEdge + ld->ld_Height - 1));
        Draw(rp, ld->ld_Left, ld->ld_TopEdge);
    }

    if (!ld->ld_Busy && ld->ld_InnerWidth > 0 && ld->ld_InnerHeight > 0)
    {
        /*
         * TickBox-style: Text() into the supplied GIRPort. Cache/blit is
         * deferred — friendless depth-8 BltBitMapRastPort to RTG hung here.
         */
        rtbPaintDocument(cb, ld, rp, win, cm,
            ld->ld_InnerLeft, ld->ld_InnerTop);
    }

    ld->ld_InRender = FALSE;
    return 1;
}

ULONG
rtbHitTest(struct ClassBase *cb, Class *cl, Object *o,
    struct gpHitTest *msg)
{
    struct localData *ld;
    WORD x;
    WORD y;

    (void)cb;
    ld = INST_DATA(cl, o);
    x = msg->gpht_Mouse.X;
    y = msg->gpht_Mouse.Y;

    if (x >= 0 && y >= 0 && x < ld->ld_Width && y < ld->ld_Height)
        return GMR_GADGETHIT;
    return 0;
}

ULONG
rtbGoActive(struct ClassBase *cb, Class *cl, Object *o,
    struct gpInput *msg)
{
    struct localData *ld;

    ld = INST_DATA(cl, o);
    ld->ld_Tracking = TRUE;
    ld->ld_DownX = msg->gpi_Mouse.X;
    ld->ld_DownY = msg->gpi_Mouse.Y;
    ld->ld_ContentOk = TRUE;
    (void)cb;
    (void)cl;
    return GMR_MEACTIVE;
}

ULONG
rtbHandleInput(struct ClassBase *cb, Class *cl, Object *o,
    struct gpInput *msg)
{
    struct localData *ld;
    struct InputEvent *ie;
    LONG delta;
    struct rtbHitTest hit;
    WORD gx;
    WORD gy;

    ld = INST_DATA(cl, o);
    ie = msg->gpi_IEvent;

    if (ie == NULL)
        return GMR_MEACTIVE;

    /* Mouse wheel / rawkey scroll */
    if (ie->ie_Class == IECLASS_RAWKEY)
    {
        delta = 0;
        if (ie->ie_Code == 0x7A) /* NM_WHEEL_UP */
            delta = -24;
        else if (ie->ie_Code == 0x7B) /* NM_WHEEL_DOWN */
            delta = 24;
        else if (ie->ie_Code == 0x4C) /* up */
            delta = -12;
        else if (ie->ie_Code == 0x4D) /* down */
            delta = 12;
        else if (ie->ie_Code == 0x48) /* page up-ish */
            delta = -ld->ld_Visible;
        else if (ie->ie_Code == 0x49)
            delta = ld->ld_Visible;

        if (delta != 0)
        {
            ld->ld_Top += delta;
            rtbClampTop(ld);
            ld->ld_CacheDirty = TRUE;
            rtbNotifyScroll(cb, cl, o, msg->gpi_GInfo);
            rtbRedrawGIRPort(cb, cl, o, msg->gpi_GInfo);
            return GMR_MEACTIVE;
        }
    }

    if (ie->ie_Class == IECLASS_RAWMOUSE)
    {
        if (ie->ie_Code == SELECTUP)
        {
            gx = (WORD)(ld->ld_Left + msg->gpi_Mouse.X);
            gy = (WORD)(ld->ld_TopEdge + msg->gpi_Mouse.Y);

            memset(&hit, 0, sizeof(hit));
            if (rtbHitAt(cb, ld, gx, gy, &hit))
            {
                ld->ld_HitBlock = hit.BlockID;
                ld->ld_HitRun = hit.RunID;
                ld->ld_HitKind = hit.HitKind;
                ld->ld_HitUser = hit.User;

                if (hit.HitKind == RTBH_LINK)
                    ld->ld_RelEvent = RTBE_LINKACTIVATE;
                else if (hit.HitKind == RTBH_IMAGE ||
                    hit.HitKind == RTBH_EMBED)
                {
                    /*
                     * No gallery yet — treat as post select (same path as
                     * body text). Early GMR_NOREUSE alone left the gadget
                     * active and crashed Intuition on some clicks.
                     */
                    ld->ld_RelEvent = RTBE_BLOCKSELECT;
                }
                else if (hit.HitKind == RTBH_CONTROL)
                {
                    struct RtbRun *r;

                    ld->ld_RelEvent = RTBE_CONTROLACTIVATE;
                    /* Toggle hosted checkboxes in-place (nested rows OK). */
                    if (hit.RunID != 0)
                    {
                        r = rtbFindRun(ld, hit.RunID);
                        if (r != NULL && r->rr_Kind == RTBR_CONTROL &&
                            (r->rr_Data.control.ctlKind == RTBC_CHECKBOX ||
                             r->rr_Data.control.ctlKind == RTBC_ICON))
                        {
                            r->rr_Style ^= RTBS_CHECKED;
                            ld->ld_CacheDirty = TRUE;
                        }
                    }
                }
                else
                    ld->ld_RelEvent = RTBE_BLOCKSELECT;

                if (ld->ld_SelectBlocks || ld->ld_CacheDirty)
                {
                    if (ld->ld_SelectBlocks)
                    {
                        rtbSelectBlockId(ld, hit.BlockID);
                        ld->ld_SelectedBlock = hit.BlockID;
                        ld->ld_SelectedUser = hit.User;
                        ld->ld_CacheDirty = TRUE;
                    }
                    rtbRedrawGIRPort(cb, cl, o, msg->gpi_GInfo);
                }

                rtbNotify(cb, cl, o, msg->gpi_GInfo,
                    GA_ID, ((struct Gadget *)o)->GadgetID,
                    RTB_RelEvent, ld->ld_RelEvent,
                    RTB_HitBlock, ld->ld_HitBlock,
                    RTB_HitKind, ld->ld_HitKind,
                    RTB_SelectedUser, (ULONG)ld->ld_SelectedUser,
                    TAG_DONE);

                return GMR_NOREUSE | GMR_VERIFY;
            }
            return GMR_NOREUSE;
        }
    }

    return GMR_MEACTIVE;
}

ULONG
rtbGoInactive(struct ClassBase *cb, Class *cl, Object *o,
    struct gpGoInactive *msg)
{
    struct localData *ld;

    (void)cb;
    (void)msg;
    ld = INST_DATA(cl, o);
    ld->ld_Tracking = FALSE;
    return 0;
}
