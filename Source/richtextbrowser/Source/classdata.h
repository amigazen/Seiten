/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * classdata.h - per-instance data for richtextbrowser.gadget
 */

#ifndef RTB_CLASSDATA_H
#define RTB_CLASSDATA_H

#ifndef RTB_FACE_H
#include "rtb_face.h"
#endif

struct localData
{
    APTR                    ld_Pool;
    struct RtbDocument     *ld_Doc;
    BOOL                    ld_DocOwned;

    LONG                    ld_Top;
    LONG                    ld_Total;
    LONG                    ld_Visible;
    LONG                    ld_Overscan;
    ULONG                   ld_BlockCap;
    BOOL                    ld_Busy;
    BOOL                    ld_Horiz;
    BOOL                    ld_ReadOnly;
    BOOL                    ld_SelectBlocks;
    BOOL                    ld_WithBevel;
    BOOL                    ld_LayoutDirty;
    BOOL                    ld_InRender;    /* reentrancy guard for GM_RENDER */
    BOOL                    ld_InSet;       /* TRUE inside OM_SET/OM_UPDATE */
    BOOL                    ld_CacheDirty;  /* page.image LPIF_CACHE_DIRTY */
    /*
     * FALSE until SetGadgetAttrs (ops_GInfo != NULL). OpenWindow GM_RENDER
     * stays bg-only. Heavy paint never runs while ld_InSet (SetGadgetAttrs
     * nests GM_RENDER under intuition locks).
     */
    BOOL                    ld_ContentOk;

    /* Viewport cache (inner size only — never full-document CHIP BM) */
    struct BitMap          *ld_CachedBM;
    UWORD                   ld_CacheW;
    UWORD                   ld_CacheH;
    LONG                    ld_CacheTop;    /* Top used when cache was built */

    WORD                    ld_BgPen;
    WORD                    ld_LinkPen;
    WORD                    ld_QuoteBarPen;
    WORD                    ld_TextPen;
    WORD                    ld_ShinePen;
    WORD                    ld_ShadowPen;
    WORD                    ld_FillPen;     /* selected / button face */

    struct TextAttr         *ld_TextAttr;
    struct TextFont         *ld_Font;

    /* Scalable face cache (TTEngine → bullet → bitmap). */
    struct RtbFace          ld_Faces[RTB_FACE_SLOTS];
    struct RastPort         ld_MeasureRP;
    struct BitMap           ld_MeasureBM;
    PLANEPTR                ld_MeasurePlane;
    BOOL                    ld_MeasureOk;
    BOOL                    ld_FontsWarmed;

    /* sysiclass chrome (GadTools-native CHECKIMAGE) */
    Object                 *ld_CheckImg;
    struct DrawInfo        *ld_DrawInfo;    /* non-owning; from GInfo */

    /* Gadget domain (after GM_LAYOUT), window-relative */
    WORD                    ld_Left;
    WORD                    ld_TopEdge;
    WORD                    ld_Width;
    WORD                    ld_Height;
    WORD                    ld_InnerLeft;
    WORD                    ld_InnerTop;
    WORD                    ld_InnerWidth;
    WORD                    ld_InnerHeight;

    /* Hit / notify state */
    ULONG                   ld_RelEvent;
    RTB_BlockID             ld_HitBlock;
    RTB_RunID               ld_HitRun;
    ULONG                   ld_HitKind;
    APTR                    ld_HitUser;
    RTB_BlockID             ld_SelectedBlock;
    APTR                    ld_SelectedUser;

    /* Input */
    WORD                    ld_DownX;
    WORD                    ld_DownY;
    BOOL                    ld_Tracking;

    struct CanvasPlot       ld_Plot;
};

#define RTB_DEFAULT_OVERSCAN   64
#define RTB_DEFAULT_BLOCKCAP   512
#define RTB_BEVEL_PAD          2

#endif /* RTB_CLASSDATA_H */
