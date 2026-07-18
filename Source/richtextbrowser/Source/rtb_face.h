/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtb_face.h - scalable face cache (TTEngine → bullet → bitmap fallback)
 */

#ifndef RTB_FACE_H
#define RTB_FACE_H

#ifndef RTB_GLYPHENGINE_H
#include "glyphengine.h"
#endif

#ifndef GRAPHICS_TEXT_H
#include <graphics/text.h>
#endif

#define RTB_FACE_SLOTS         6

#define RTBFK_NONE             0
#define RTBFK_TT               1
#define RTBFK_BULLET           2
#define RTBFK_BITMAP           3

struct RtbFace
{
    UWORD                   rf_Kind;
    UWORD                   rf_Size;
    ULONG                   rf_Style;       /* RTBS_BOLD|ITALIC at open */
    UBYTE                   rf_Name[40];
    APTR                    rf_TT;          /* TT_OpenFont handle */
    struct GlyphRenderCtx   rf_Bullet;
    BOOL                    rf_BulletOpen;
    struct TextFont         *rf_Bitmap;     /* last-resort TextFont */
    BOOL                    rf_OwnsBitmap;
    WORD                    rf_Height;
    WORD                    rf_Baseline;
    WORD                    rf_AvgWidth;
};

struct RtbFace *rtbFaceResolve(struct ClassBase *cb, struct localData *ld,
    CONST_STRPTR name, UWORD size, ULONG style);
void rtbFaceFlushAll(struct ClassBase *cb, struct localData *ld);
void rtbWarmDocumentFonts(struct ClassBase *cb, struct localData *ld);

WORD rtbFaceLineHeight(struct RtbFace *face);
WORD rtbFaceAvgWidth(struct RtbFace *face);
WORD rtbFaceTextWidth(struct ClassBase *cb, struct localData *ld,
    struct RtbFace *face, CONST_STRPTR text, ULONG len);
void rtbFaceDraw(struct ClassBase *cb, struct localData *ld,
    struct RtbFace *face, struct CanvasPlot *cp,
    WORD x, WORD y, CONST_STRPTR text, ULONG len,
    WORD fg, WORD bg, ULONG style);

BOOL rtbMayOpenFonts(struct localData *ld);
void rtbEnsureMeasureRP(struct ClassBase *cb, struct localData *ld);
void rtbFreeMeasureRP(struct ClassBase *cb, struct localData *ld);

#endif /* RTB_FACE_H */
