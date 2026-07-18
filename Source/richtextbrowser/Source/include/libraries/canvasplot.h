#ifndef LIBRARIES_CANVASPLOT_H
#define LIBRARIES_CANVASPLOT_H
/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * Immediate-mode canvas plotter used by richtextbrowser.gadget.
 * Draws into a caller-owned RastPort (window clip or viewport offscreen).
 * TTEngine may only be used on a plotter-owned RastPort — never nested
 * under listbrowser LBNCA_RenderHook.
 */

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef GRAPHICS_GFX_H
#include <graphics/gfx.h>
#endif

#ifndef GRAPHICS_RASTPORT_H
#include <graphics/rastport.h>
#endif

#ifndef INTUITION_INTUITION_H
struct Window;
struct ColorMap;
#endif

struct ClassBase; /* opaque to public clients */

#define CP_CLIP_DEPTH  8

struct CanvasPlot
{
    struct RastPort    *cp_RP;
    struct Window      *cp_Window;     /* for TTEngine TT_Window */
    struct ColorMap    *cp_ColorMap;
    struct ClassBase   *cp_CB;
    struct Rectangle    cp_ClipStack[CP_CLIP_DEPTH];
    UWORD               cp_ClipDepth;
    struct TextFont     *cp_Font;       /* OpenDiskFont fallback */
    BOOL                cp_TTActive;   /* TTEngine bound to this RP */
    WORD                cp_FgPen;
    WORD                cp_BgPen;
};

/* Font metric / draw request */
struct CP_TextStyle
{
    STRPTR  FontName;   /* NULL = use current / default */
    UWORD   Size;       /* points; 0 = current */
    ULONG   Style;      /* RTBS_* */
    WORD    FgPen;
    WORD    BgPen;      /* -1 = transparent */
};

struct CP_TextExtent
{
    WORD    Width;
    WORD    Height;
    WORD    Baseline;
};

void CP_Init(struct CanvasPlot *cp, struct ClassBase *cb);
void CP_SetTarget(struct CanvasPlot *cp, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm);
void CP_Done(struct CanvasPlot *cp);

BOOL CP_PushClip(struct CanvasPlot *cp, WORD minx, WORD miny,
    WORD maxx, WORD maxy);
void CP_PopClip(struct CanvasPlot *cp);

void CP_SetPens(struct CanvasPlot *cp, WORD fg, WORD bg);
void CP_FillRect(struct CanvasPlot *cp, WORD x1, WORD y1, WORD x2, WORD y2);
void CP_Rect(struct CanvasPlot *cp, WORD x1, WORD y1, WORD x2, WORD y2);
void CP_Line(struct CanvasPlot *cp, WORD x1, WORD y1, WORD x2, WORD y2);

BOOL CP_TextMetrics(struct CanvasPlot *cp, CONST_STRPTR text, ULONG len,
    struct CP_TextStyle *style, struct CP_TextExtent *out);
void CP_DrawText(struct CanvasPlot *cp, WORD x, WORD y,
    CONST_STRPTR text, ULONG len, struct CP_TextStyle *style);

void CP_BltBitMap(struct CanvasPlot *cp, struct BitMap *src,
    WORD srcX, WORD srcY, WORD destX, WORD destY, WORD w, WORD h);
void CP_BltMasked(struct CanvasPlot *cp, struct BitMap *src, PLANEPTR mask,
    WORD srcX, WORD srcY, WORD destX, WORD destY, WORD w, WORD h);

/* Optional: bind/unbind TTEngine to plotter RP (no-op if library absent). */
BOOL CP_BeginTT(struct CanvasPlot *cp);
void CP_EndTT(struct CanvasPlot *cp);

BOOL CP_TTAvailable(struct ClassBase *cb);
BOOL CP_TTMapFamily(CONST_STRPTR fontName, ULONG style,
    STRPTR familyOut, ULONG familyLen, BOOL *boldOut, BOOL *italicOut);
APTR CP_TTOpenFace(struct ClassBase *cb, CONST_STRPTR preferName, UWORD sizePx,
    ULONG style);
void CP_TTCloseFace(struct ClassBase *cb, APTR handle);
BOOL CP_TTGetMetrics(struct ClassBase *cb, APTR handle, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm,
    WORD *heightOut, WORD *baselineOut, WORD *avgWidthOut);
WORD CP_TTTextWidth(struct ClassBase *cb, APTR handle, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm,
    CONST_STRPTR text, ULONG len);
BOOL CP_TTDrawText(struct ClassBase *cb, APTR handle, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm,
    WORD x, WORD y, CONST_STRPTR text, ULONG len, WORD fgPen);

/* bullet.library outline face (otag). Falls back to diskfont when closed. */
struct GlyphRenderCtx;
BOOL CP_BulletOpen(struct ClassBase *cb, STRPTR otagPath, UWORD pointSize);
void CP_BulletClose(struct ClassBase *cb);
BOOL CP_BulletIsOpen(void);
BOOL CP_DrawBulletText(struct ClassBase *cb, struct CanvasPlot *cp,
    WORD x, WORD y, CONST_STRPTR text, ULONG len, WORD fgPen);
WORD CP_BulletTextWidth(struct ClassBase *cb, CONST_STRPTR text, ULONG len);
BOOL CP_DrawBulletTextFace(struct ClassBase *cb, struct CanvasPlot *cp,
    struct GlyphRenderCtx *ctx, WORD x, WORD y, CONST_STRPTR text, ULONG len,
    WORD fgPen);
WORD CP_BulletTextWidthFace(struct ClassBase *cb, struct GlyphRenderCtx *ctx,
    CONST_STRPTR text, ULONG len, WORD avgWidth);

#endif /* LIBRARIES_CANVASPLOT_H */
