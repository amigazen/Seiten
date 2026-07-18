#ifndef GADGETS_RICHTEXTBROWSER_H
#define GADGETS_RICHTEXTBROWSER_H
/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * richtextbrowser.gadget -- scrollable rich-document ReAction gadget.
 */

#ifndef REACTION_REACTION_H
#include <reaction/reaction.h>
#endif

#ifndef EXEC_LISTS_H
#include <exec/lists.h>
#endif

#ifndef INTUITION_GADGETCLASS_H
#include <intuition/gadgetclass.h>
#endif

#ifndef GRAPHICS_GFX_H
#include <graphics/gfx.h>
#endif

/*****************************************************************************/

#define RICHTEXTBROWSERCLASS   "richtextbrowser.gadget"

typedef ULONG RTB_BlockID;
typedef ULONG RTB_RunID;

struct RtbDocument;
struct RtbBlock;
struct RtbRun;

/*****************************************************************************/

#define RTB_Dummy              (REACTION_Dummy + 0x28000)

/* Pixel-space scroll (external scroller sync; cf. texteditor Prop_*) */
#define RTB_Top                (RTB_Dummy + 1)   /* (LONG) ISGN */
#define RTB_Total              (RTB_Dummy + 2)   /* (LONG) GN */
#define RTB_Visible            (RTB_Dummy + 3)   /* (LONG) GN */
#define RTB_Horiz              (RTB_Dummy + 4)   /* (BOOL) ISG — v1 FALSE */

/* Document */
#define RTB_Document           (RTB_Dummy + 10)  /* (struct RtbDocument *) ISG; ~0 detach */
#define RTB_BlockCap           (RTB_Dummy + 11)  /* (ULONG) ISG soft max */
#define RTB_Overscan           (RTB_Dummy + 12)  /* (LONG) ISG extra paint px */
#define RTB_Busy               (RTB_Dummy + 13)  /* (BOOL) ISG bulk-update quiet */

/* Pens / behaviour */
#define RTB_BackgroundPen      (RTB_Dummy + 20)  /* (WORD) ISG */
#define RTB_LinkPen            (RTB_Dummy + 21)  /* (WORD) ISG */
#define RTB_QuoteBarPen        (RTB_Dummy + 22)  /* (WORD) ISG */
#define RTB_ReadOnly           (RTB_Dummy + 23)  /* (BOOL) ISG default TRUE */
#define RTB_SelectBlocks       (RTB_Dummy + 24)  /* (BOOL) ISG */
#define RTB_WithBevel          (RTB_Dummy + 25)  /* (BOOL) I default TRUE */

/* Hit / selection detail after RelVerify (listbrowser RelEvent pattern) */
#define RTB_RelEvent           (RTB_Dummy + 30)  /* (ULONG) GN — RTBE_* */
#define RTB_HitBlock           (RTB_Dummy + 31)  /* (ULONG) GN block id */
#define RTB_HitRun             (RTB_Dummy + 32)  /* (ULONG) GN run id */
#define RTB_HitKind            (RTB_Dummy + 33)  /* (ULONG) GN RTBH_* */
#define RTB_HitUser            (RTB_Dummy + 34)  /* (APTR) GN */
#define RTB_SelectedBlock      (RTB_Dummy + 35)  /* (ULONG) GN selected id */
#define RTB_SelectedUser       (RTB_Dummy + 36)  /* (APTR) GN */

/*****************************************************************************/

/* RelEvent / Code values */
#define RTBE_NONE              0
#define RTBE_BLOCKSELECT       1
#define RTBE_LINKACTIVATE      2
#define RTBE_IMAGEACTIVATE     3
#define RTBE_CONTROLACTIVATE   4
#define RTBE_SCROLL            5
#define RTBE_VISIBLERANGE      6
#define RTBE_EMBEDACTIVATE     7

/* Hit kinds */
#define RTBH_NONE              0
#define RTBH_BLOCK             1
#define RTBH_LINK              2
#define RTBH_IMAGE             3
#define RTBH_CONTROL           4
#define RTBH_EMBED             5

/*****************************************************************************/

/* Methods */
#define RTBM_CLEAR             (0x582000L)
#define RTBM_SETDOCUMENT       (0x582001L)
#define RTBM_INSERTBLOCK       (0x582002L)
#define RTBM_REMOVEBLOCK       (0x582003L)
#define RTBM_UPDATEBLOCK       (0x582004L)
#define RTBM_INVALIDATE        (0x582005L)
#define RTBM_SCROLLTO          (0x582006L)
#define RTBM_HITTEST           (0x582007L)
#define RTBM_BLOCKBOUNDS       (0x582008L)

struct rtbGeneral
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
};

struct rtbSetDocument
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    struct RtbDocument *Document;   /* NULL = clear; gadget adopts */
};

struct rtbInsertBlock
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    struct RtbBlock    *Block;      /* adopted into document */
    RTB_BlockID         AfterID;    /* 0 = append; ~0 = prepend */
};

struct rtbRemoveBlock
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    RTB_BlockID         BlockID;
};

struct rtbUpdateBlock
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    struct RtbBlock    *Block;      /* replaces matching rb_ID */
};

struct rtbInvalidate
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    RTB_BlockID         FirstID;    /* 0 = all */
    RTB_BlockID         LastID;
};

struct rtbScrollTo
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    RTB_BlockID         BlockID;    /* 0 = use PixelY */
    LONG                PixelY;
};

struct rtbHitTest
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    WORD                MouseX;     /* relative to gadget */
    WORD                MouseY;
    /* outputs filled on success */
    RTB_BlockID         BlockID;
    RTB_RunID           RunID;
    ULONG               HitKind;
    APTR                User;
};

/* RTBM_BLOCKBOUNDS — document Y/height for a top-level or nested block ID.
 * Nested blocks return the enclosing top-level row's bounds (lazy load). */
struct rtbBlockBounds
{
    ULONG               MethodID;
    struct GadgetInfo  *GInfo;
    RTB_BlockID         BlockID;
    LONG                OutY;       /* document Y (filled) */
    LONG                OutHeight;  /* filled */
};

/*****************************************************************************/

/* Block types */
#define RTBB_PARAGRAPH         1
#define RTBB_HEADING           2
#define RTBB_HBOX              3
#define RTBB_VBOX              4
#define RTBB_IMAGE             5
#define RTBB_VECTOR            6
#define RTBB_EMBED             7
#define RTBB_RULE              8
#define RTBB_SPACER            9
#define RTBB_QUOTE             10
#define RTBB_CONTROLROW        11
#define RTBB_CUSTOM            12
#define RTBB_IMAGEGRID         13

/* Run kinds */
#define RTBR_TEXT              1
#define RTBR_LINK              2
#define RTBR_IMAGE             3
#define RTBR_CONTROL           4

/* Run style bits */
#define RTBS_BOLD              (1L << 0)
#define RTBS_ITALIC            (1L << 1)
#define RTBS_UNDERLINE         (1L << 2)
#define RTBS_MONO              (1L << 3)

/* Control kinds (hosted, not child BOOPSI) */
#define RTBC_BUTTON            1
#define RTBC_CHECKBOX          2
#define RTBC_ICON              3   /* borderless BitMap / BitmapImage blit */

/* Block flags */
#define RTBBF_DIRTY            (1L << 0)
#define RTBBF_SELECTED         (1L << 1)
#define RTBBF_LOADFAIL         (1L << 2)  /* image GET/decode failed — show X */

/* Control run state (or into rr_Style for hosted controls) */
#define RTBS_CHECKED           (1L << 8)

struct RtbRun
{
    struct MinNode      rr_Node;
    RTB_RunID           rr_ID;
    UWORD               rr_Kind;
    ULONG               rr_Style;
    UWORD               rr_Size;        /* points; 0 = inherit */
    STRPTR              rr_FontName;    /* NULL = inherit */
    ULONG               rr_Fg;          /* pen; ~0 = inherit */
    ULONG               rr_Bg;          /* pen; ~0 = none */
    /* layout cache (gadget-owned) */
    WORD                rr_X;
    WORD                rr_Y;
    WORD                rr_W;
    WORD                rr_H;
    union {
        struct {
            STRPTR      text;
            ULONG       byteLen;
        } text;
        struct {
            STRPTR      href;
            STRPTR      text;
            ULONG       byteLen;
            APTR        user;
        } link;
        struct {
            Object     *bitmapObj;
            struct BitMap *bm;
            UWORD       w;
            UWORD       h;
            UWORD       maxW;
        } image;
        struct {
            UWORD       ctlKind;
            RTB_BlockID ctlId;
            STRPTR      label;
            APTR        user;
            Object     *bitmapObj;     /* RTBC_ICON normal */
            Object     *selBitmapObj;  /* RTBC_ICON selected (optional) */
            struct BitMap *bm;
            UWORD       iw;
            UWORD       ih;
        } control;
    } rr_Data;
};

struct RtbBlock
{
    struct MinNode      rb_Node;
    RTB_BlockID         rb_ID;
    UWORD               rb_Type;
    ULONG               rb_Flags;
    APTR                rb_User;
    ULONG               rb_Fg;          /* pen; ~0 = inherit */
    ULONG               rb_Bg;          /* pen; ~0 = none (zebra rows) */
    /* layout cache */
    LONG                rb_Y;
    LONG                rb_Height;
    WORD                rb_Width;
    union {
        struct {
            struct MinList runs;    /* RtbRun */
        } flow;                     /* PARAGRAPH / HEADING */
        struct {
            struct MinList children;
        } box;                      /* HBOX / VBOX / QUOTE */
        struct {
            Object     *bitmapObj;
            struct BitMap *bm;
            UWORD       w;
            UWORD       h;
            UWORD       maxW;
            STRPTR      alt;
        } image;
        struct {
            Object     *vecObj;
            UWORD       w;
            UWORD       h;
        } vector;
        /* OpenGraph / link-preview card (thin SHADOWPEN frame) */
        struct {
            Object     *bitmapObj;
            struct BitMap *bm;
            UWORD       thumbW;
            UWORD       thumbH;
            STRPTR      title;
            STRPTR      description;
            STRPTR      site;
            STRPTR      href;
        } embed;
        struct {
            UWORD       thickness;
        } rule;
        struct {
            UWORD       pixels;
        } spacer;
        struct {
            struct MinList controls; /* RtbRun RTBR_CONTROL */
        } controlrow;
        /* Equal-ish cells of RTBB_IMAGE children (1–4 typical) */
        struct {
            struct MinList children;
            UWORD       columns;    /* 0 = auto from child count */
            UWORD       gap;
            UWORD       maxCellH;
        } grid;
    } rb_Data;
};

struct RtbDocument
{
    struct MinList      rd_Blocks;
    ULONG               rd_Flags;
    ULONG               rd_NextBlockID;
    ULONG               rd_NextRunID;
};

/*****************************************************************************/

/* Tag helpers — RTBA_* for AllocRtbBlockA / AllocRtbRunA (and tagcall Tags) */
#define RTBA_ID                (TAG_USER + 0x100)
#define RTBA_User              (TAG_USER + 0x101)
#define RTBA_Text              (TAG_USER + 0x102)  /* STRPTR — text run */
#define RTBA_Href              (TAG_USER + 0x103)
#define RTBA_BitMapObject      (TAG_USER + 0x104)
#define RTBA_BitMap            (TAG_USER + 0x105)
#define RTBA_SelBitMapObject   (TAG_USER + 0x11A)  /* RTBC_ICON selected */
#define RTBA_Width             (TAG_USER + 0x106)
#define RTBA_Height            (TAG_USER + 0x107)
#define RTBA_MaxWidth          (TAG_USER + 0x108)
#define RTBA_FgPen             (TAG_USER + 0x109)
#define RTBA_BgPen             (TAG_USER + 0x10A)
#define RTBA_Style             (TAG_USER + 0x10B)
#define RTBA_Pixels            (TAG_USER + 0x10C)
#define RTBA_Thickness         (TAG_USER + 0x10D)
#define RTBA_CtlKind           (TAG_USER + 0x10E)
#define RTBA_CtlId             (TAG_USER + 0x10F)
#define RTBA_Label             (TAG_USER + 0x110)
#define RTBA_Alt               (TAG_USER + 0x111)
#define RTBA_Checked           (TAG_USER + 0x112)  /* BOOL — checkbox / toggle */
#define RTBA_Size              (TAG_USER + 0x113)  /* UWORD points for run font */
#define RTBA_FontName          (TAG_USER + 0x114)  /* STRPTR e.g. "emerald.font" */
#define RTBA_Columns           (TAG_USER + 0x115)  /* UWORD — IMAGEGRID; 0=auto */
#define RTBA_Gap               (TAG_USER + 0x116)  /* UWORD — IMAGEGRID cell gap */
#define RTBA_Title             (TAG_USER + 0x117)  /* STRPTR — EMBED card title */
#define RTBA_Description       (TAG_USER + 0x118)  /* STRPTR — EMBED card body */
#define RTBA_Site              (TAG_USER + 0x119)  /* STRPTR — EMBED host line */

/*****************************************************************************/

/*
 * Library LVOs after OpenLibrary("gadgets/richtextbrowser.gadget").
 * Same pattern as listbrowser AllocListBrowserNodeA — not a static .o.
 *
 * When building the gadget (DEFINE=RTB_LIBRARY), skip app prototypes /
 * libcall pragmas — implementations use ASM REG(a6) ClassBase *.
 */
#ifndef RTB_LIBRARY

#ifndef INTUITION_CLASSES_H
#include <intuition/classes.h>
#endif
#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif

extern struct Library *RichTextBrowserBase;

Class *RICHTEXTBROWSER_GetClass(void);

struct RtbDocument *AllocRtbDocument(void);
void FreeRtbDocument(struct RtbDocument *doc);

struct RtbBlock *AllocRtbBlock(ULONG type);
void FreeRtbBlock(struct RtbBlock *blk);

struct RtbRun *AllocRtbRun(ULONG kind);
void FreeRtbRun(struct RtbRun *run);

struct RtbBlock *AllocRtbBlockA(ULONG type, struct TagItem *tags);
struct RtbBlock *AllocRtbBlockTags(ULONG type, ...);
struct RtbRun *AllocRtbRunA(ULONG kind, struct TagItem *tags);
struct RtbRun *AllocRtbRunTags(ULONG kind, ...);

BOOL RtbBlockAddRun(struct RtbBlock *blk, struct RtbRun *run);
BOOL RtbBlockAddChild(struct RtbBlock *parent, struct RtbBlock *child);

#ifdef __SASC
#pragma libcall RichTextBrowserBase RICHTEXTBROWSER_GetClass 1e 00
#pragma libcall RichTextBrowserBase AllocRtbDocument 24 00
#pragma libcall RichTextBrowserBase FreeRtbDocument 2a 801
#pragma libcall RichTextBrowserBase AllocRtbBlock 30 001
#pragma libcall RichTextBrowserBase FreeRtbBlock 36 801
#pragma libcall RichTextBrowserBase AllocRtbRun 3c 001
#pragma libcall RichTextBrowserBase FreeRtbRun 42 801
#pragma libcall RichTextBrowserBase AllocRtbBlockA 48 8002
#pragma tagcall RichTextBrowserBase AllocRtbBlockTags 48 8002
#pragma libcall RichTextBrowserBase AllocRtbRunA 4e 8002
#pragma tagcall RichTextBrowserBase AllocRtbRunTags 4e 8002
#pragma libcall RichTextBrowserBase RtbBlockAddRun 54 9802
#pragma libcall RichTextBrowserBase RtbBlockAddChild 5a 9802
#endif

#endif /* !RTB_LIBRARY */

#endif /* GADGETS_RICHTEXTBROWSER_H */
