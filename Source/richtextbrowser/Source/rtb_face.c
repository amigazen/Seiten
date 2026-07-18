/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtb_face.c - resolve/draw scalable faces (TTEngine → bullet → bitmap)
 */

#include "classbase.h"
#include "rtb_face.h"

BOOL
rtbMayOpenFonts(struct localData *ld)
{
    if (ld == NULL)
        return FALSE;
    /* Font open does DOS I/O — never under InSet / InRender layer locks. */
    if (ld->ld_ContentOk && !ld->ld_InRender && !ld->ld_InSet)
        return TRUE;
    return FALSE;
}

void
rtbEnsureMeasureRP(struct ClassBase *cb, struct localData *ld)
{
    if (ld == NULL || ld->ld_MeasureOk)
        return;
    (void)cb;
    memset(&ld->ld_MeasureBM, 0, sizeof(ld->ld_MeasureBM));
    InitBitMap(&ld->ld_MeasureBM, 1, 8, 8);
    ld->ld_MeasurePlane = (PLANEPTR)AllocMem(8, MEMF_CHIP | MEMF_CLEAR);
    if (ld->ld_MeasurePlane == NULL)
        return;
    ld->ld_MeasureBM.Planes[0] = ld->ld_MeasurePlane;
    InitRastPort(&ld->ld_MeasureRP);
    ld->ld_MeasureRP.BitMap = &ld->ld_MeasureBM;
    ld->ld_MeasureOk = TRUE;
}

void
rtbFreeMeasureRP(struct ClassBase *cb, struct localData *ld)
{
    (void)cb;
    if (ld == NULL)
        return;
    if (ld->ld_MeasurePlane != NULL)
    {
        FreeMem(ld->ld_MeasurePlane, 8);
        ld->ld_MeasurePlane = NULL;
    }
    ld->ld_MeasureOk = FALSE;
}

static void
rtb_face_clear(struct ClassBase *cb, struct RtbFace *face)
{
    if (face == NULL)
        return;
    if (face->rf_Kind == RTBFK_TT && face->rf_TT != NULL)
        CP_TTCloseFace(cb, face->rf_TT);
    if (face->rf_BulletOpen)
    {
        gleCloseFont(cb, &face->rf_Bullet);
        face->rf_BulletOpen = FALSE;
    }
    if (face->rf_Bitmap != NULL && face->rf_OwnsBitmap)
        CloseFont(face->rf_Bitmap);
    face->rf_Bitmap = NULL;
    face->rf_OwnsBitmap = FALSE;
    memset(face, 0, sizeof(*face));
}

void
rtbFaceFlushAll(struct ClassBase *cb, struct localData *ld)
{
    ULONG i;

    if (ld == NULL)
        return;
    for (i = 0; i < RTB_FACE_SLOTS; i++)
        rtb_face_clear(cb, &ld->ld_Faces[i]);
}

static BOOL
rtb_ends_otag(CONST_STRPTR name)
{
    ULONG n;

    if (name == NULL)
        return FALSE;
    n = strlen((char *)name);
    if (n < 5)
        return FALSE;
    if (name[n - 5] == '.' &&
        (name[n - 4] == 'o' || name[n - 4] == 'O') &&
        (name[n - 3] == 't' || name[n - 3] == 'T') &&
        (name[n - 2] == 'a' || name[n - 2] == 'A') &&
        (name[n - 1] == 'g' || name[n - 1] == 'G'))
        return TRUE;
    return FALSE;
}

/* Map CSS/Amiga face names to stock IntelliFont .otag paths. */
static CONST_STRPTR
rtb_bullet_otag_for(CONST_STRPTR name)
{
    if (name == NULL || name[0] == '\0')
        return "FONTS:CGTimes.otag";
    if (rtb_ends_otag(name))
        return name;
    /* Rough family → CG face */
    if (name[0] == 'A' || name[0] == 'a' || name[0] == 'H' || name[0] == 'h')
        return "FONTS:CGTriumvirate.otag";
    if (name[0] == 'C' || name[0] == 'c')
        return "FONTS:CGCourier.otag";
    return "FONTS:CGTimes.otag";
}

static struct RtbFace *
rtb_face_slot(struct localData *ld, CONST_STRPTR name, UWORD size,
    ULONG style)
{
    ULONG i;
    ULONG styleKey;

    styleKey = style & (RTBS_BOLD | RTBS_ITALIC);
    for (i = 0; i < RTB_FACE_SLOTS; i++)
    {
        if (ld->ld_Faces[i].rf_Kind != RTBFK_NONE &&
            ld->ld_Faces[i].rf_Size == size &&
            ld->ld_Faces[i].rf_Style == styleKey &&
            strcmp((char *)ld->ld_Faces[i].rf_Name,
                name != NULL ? (char *)name : "") == 0)
            return &ld->ld_Faces[i];
    }
    return NULL;
}

static struct RtbFace *
rtb_face_alloc_slot(struct ClassBase *cb, struct localData *ld)
{
    ULONG i;

    for (i = 0; i < RTB_FACE_SLOTS; i++)
    {
        if (ld->ld_Faces[i].rf_Kind == RTBFK_NONE)
            return &ld->ld_Faces[i];
    }
    /* Recycle slot 0 */
    rtb_face_clear(cb, &ld->ld_Faces[0]);
    return &ld->ld_Faces[0];
}

static BOOL
rtb_try_tt(struct ClassBase *cb, struct localData *ld, struct RtbFace *face,
    CONST_STRPTR name, UWORD size, ULONG style)
{
    APTR h;
    struct RastPort *rp;

    if (!CP_TTAvailable(cb))
        return FALSE;
    h = CP_TTOpenFace(cb, name, size, style);
    if (h == NULL)
        return FALSE;

    rtbEnsureMeasureRP(cb, ld);
    rp = ld->ld_MeasureOk ? &ld->ld_MeasureRP : NULL;
    if (rp == NULL)
    {
        CP_TTCloseFace(cb, h);
        return FALSE;
    }

    face->rf_Kind = RTBFK_TT;
    face->rf_TT = h;
    face->rf_Size = size;
    face->rf_Style = style & (RTBS_BOLD | RTBS_ITALIC);
    if (name != NULL)
    {
        strncpy((char *)face->rf_Name, (char *)name, sizeof(face->rf_Name) - 1);
        face->rf_Name[sizeof(face->rf_Name) - 1] = 0;
    }
    if (!CP_TTGetMetrics(cb, h, rp, NULL, NULL,
            &face->rf_Height, &face->rf_Baseline, &face->rf_AvgWidth))
    {
        face->rf_Height = (WORD)size;
        face->rf_Baseline = (WORD)(size - 2);
        face->rf_AvgWidth = (WORD)(size / 2);
    }
    return TRUE;
}

static BOOL
rtb_try_bullet(struct ClassBase *cb, struct localData *ld,
    struct RtbFace *face, CONST_STRPTR name, UWORD size)
{
    CONST_STRPTR otag;

    if (cb == NULL || BulletBase == NULL)
        return FALSE;
    otag = rtb_bullet_otag_for(name);
    memset(&face->rf_Bullet, 0, sizeof(face->rf_Bullet));
    if (!gleOpenFont(cb, &face->rf_Bullet, (STRPTR)otag, (LONG)size, 72, 72))
        return FALSE;

    face->rf_Kind = RTBFK_BULLET;
    face->rf_BulletOpen = TRUE;
    face->rf_Size = size;
    face->rf_Style = 0;
    if (name != NULL)
    {
        strncpy((char *)face->rf_Name, (char *)name, sizeof(face->rf_Name) - 1);
        face->rf_Name[sizeof(face->rf_Name) - 1] = 0;
    }
    else
    {
        strncpy((char *)face->rf_Name, (char *)otag, sizeof(face->rf_Name) - 1);
        face->rf_Name[sizeof(face->rf_Name) - 1] = 0;
    }
    face->rf_Height = (WORD)size;
    face->rf_Baseline = (WORD)(size - size / 5);
    face->rf_AvgWidth = (WORD)(face->rf_Bullet.grc_EmWidth > 0 ?
        (face->rf_Bullet.grc_EmWidth / 2) : (size / 2));
    if (face->rf_AvgWidth < 1)
        face->rf_AvgWidth = 1;
    (void)ld;
    return TRUE;
}

static BOOL
rtb_try_bitmap(struct ClassBase *cb, struct localData *ld,
    struct RtbFace *face, CONST_STRPTR name, UWORD size)
{
    struct TextAttr ta;
    struct TextFont *f;

    memset(&ta, 0, sizeof(ta));
    ta.ta_Name = (STRPTR)((name != NULL && name[0] != '\0') ?
        name : (CONST_STRPTR)"topaz.font");
    ta.ta_YSize = size;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = FPF_ROMFONT;
    f = NULL;
    if (cb != NULL && GfxBase != NULL)
        f = OpenFont(&ta);
    if (f == NULL)
    {
        ta.ta_Flags = 0;
        if (cb != NULL && GfxBase != NULL)
            f = OpenFont(&ta);
    }
    if (f == NULL)
    {
        ta.ta_Name = (STRPTR)"topaz.font";
        ta.ta_YSize = 8;
        ta.ta_Flags = FPF_ROMFONT;
        if (cb != NULL && GfxBase != NULL)
            f = OpenFont(&ta);
    }
    if (f == NULL)
        f = ld->ld_Font;
    if (f == NULL)
        return FALSE;

    face->rf_Kind = RTBFK_BITMAP;
    if (f == ld->ld_Font)
    {
        face->rf_Bitmap = f;
        face->rf_OwnsBitmap = FALSE;
    }
    else
    {
        face->rf_Bitmap = f;
        face->rf_OwnsBitmap = TRUE;
    }
    face->rf_Size = size;
    if (name != NULL)
    {
        strncpy((char *)face->rf_Name, (char *)name, sizeof(face->rf_Name) - 1);
        face->rf_Name[sizeof(face->rf_Name) - 1] = 0;
    }
    face->rf_Height = f->tf_YSize;
    face->rf_Baseline = f->tf_Baseline;
    face->rf_AvgWidth = f->tf_XSize > 0 ? f->tf_XSize : 8;
    return TRUE;
}

struct RtbFace *
rtbFaceResolve(struct ClassBase *cb, struct localData *ld,
    CONST_STRPTR name, UWORD size, ULONG style)
{
    struct RtbFace *face;
    CONST_STRPTR key;
    ULONG styleKey;

    if (ld == NULL)
        return NULL;
    if (size < 1)
        size = 8;
    key = (name != NULL) ? name : (CONST_STRPTR)"";
    styleKey = style & (RTBS_BOLD | RTBS_ITALIC);

    face = rtb_face_slot(ld, key, size, styleKey);
    if (face != NULL)
        return face;

    if (!rtbMayOpenFonts(ld))
    {
        /* Paint path: only ROM topaz metrics — faces should already be warm. */
        face = rtb_face_alloc_slot(cb, ld);
        if (rtb_try_bitmap(cb, ld, face, "topaz.font",
                (size <= 9) ? size : 8))
            return face;
        return NULL;
    }

    face = rtb_face_alloc_slot(cb, ld);

    /* 1) TrueType via ttengine.library */
    if (rtb_try_tt(cb, ld, face, name, size, styleKey))
        return face;

    /* 2) Compugraphic / IntelliFont via bullet.library */
    if (rtb_try_bullet(cb, ld, face, name, size))
        return face;

    /* 3) Bitmap TextFont last resort */
    if (rtb_try_bitmap(cb, ld, face, name, size))
        return face;

    return NULL;
}

WORD
rtbFaceLineHeight(struct RtbFace *face)
{
    if (face == NULL || face->rf_Height < 1)
        return 10;
    return (WORD)(face->rf_Height + 2);
}

WORD
rtbFaceAvgWidth(struct RtbFace *face)
{
    if (face == NULL || face->rf_AvgWidth < 1)
        return 8;
    return face->rf_AvgWidth;
}

WORD
rtbFaceTextWidth(struct ClassBase *cb, struct localData *ld,
    struct RtbFace *face, CONST_STRPTR text, ULONG len)
{
    if (face == NULL || text == NULL)
        return 0;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }

    if (face->rf_Kind == RTBFK_TT && face->rf_TT != NULL)
    {
        rtbEnsureMeasureRP(cb, ld);
        if (ld->ld_MeasureOk)
            return CP_TTTextWidth(cb, face->rf_TT, &ld->ld_MeasureRP,
                NULL, NULL, text, len);
    }
    if (face->rf_Kind == RTBFK_BULLET && face->rf_BulletOpen)
        return CP_BulletTextWidthFace(cb, &face->rf_Bullet, text, len,
            face->rf_AvgWidth);
    if (face->rf_Bitmap != NULL)
        return (WORD)(len * (face->rf_Bitmap->tf_XSize > 0 ?
            face->rf_Bitmap->tf_XSize : 8));
    return (WORD)(len * rtbFaceAvgWidth(face));
}

void
rtbFaceDraw(struct ClassBase *cb, struct localData *ld,
    struct RtbFace *face, struct CanvasPlot *cp,
    WORD x, WORD y, CONST_STRPTR text, ULONG len,
    WORD fg, WORD bg, ULONG style)
{
    struct TextFont *useFont;

    (void)style;
    (void)bg;
    if (face == NULL || cp == NULL || cp->cp_RP == NULL || text == NULL)
        return;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }

    if (face->rf_Kind == RTBFK_TT && face->rf_TT != NULL)
    {
        CP_TTDrawText(cb, face->rf_TT, cp->cp_RP, cp->cp_Window,
            cp->cp_ColorMap, x, y, text, len, fg);
        return;
    }
    if (face->rf_Kind == RTBFK_BULLET && face->rf_BulletOpen)
    {
        CP_DrawBulletTextFace(cb, cp, &face->rf_Bullet, x, y, text, len, fg);
        return;
    }

    useFont = face->rf_Bitmap;
    if (useFont == NULL)
        useFont = ld->ld_Font;
    if (useFont == NULL)
        return;
    cp->cp_Font = useFont;
    SetFont(cp->cp_RP, useFont);
    SetAPen(cp->cp_RP, fg);
    SetDrMd(cp->cp_RP, JAM1);
    if (style & RTBS_BOLD)
        SetSoftStyle(cp->cp_RP, FSF_BOLD, AskSoftStyle(cp->cp_RP));
    Move(cp->cp_RP, x, y);
    Text(cp->cp_RP, (STRPTR)text, len);
    SetSoftStyle(cp->cp_RP, FS_NORMAL, AskSoftStyle(cp->cp_RP));
}

static void
rtb_warm_run_fonts(struct ClassBase *cb, struct localData *ld,
    struct MinList *list)
{
    struct RtbRun *r;

    if (list == NULL)
        return;
    for (r = (struct RtbRun *)list->mlh_Head;
        r->rr_Node.mln_Succ != NULL;
        r = (struct RtbRun *)r->rr_Node.mln_Succ)
    {
        UWORD sz;
        ULONG st;

        if (r->rr_Kind != RTBR_TEXT && r->rr_Kind != RTBR_LINK)
            continue;
        sz = r->rr_Size;
        if (sz < 1)
            sz = 8;
        st = r->rr_Style;
        (void)rtbFaceResolve(cb, ld, r->rr_FontName, sz, st);
    }
}

static void
rtb_warm_block_fonts(struct ClassBase *cb, struct localData *ld,
    struct RtbBlock *blk)
{
    struct RtbBlock *ch;

    if (blk == NULL)
        return;
    if (blk->rb_Type == RTBB_PARAGRAPH || blk->rb_Type == RTBB_HEADING)
    {
        rtb_warm_run_fonts(cb, ld, &blk->rb_Data.flow.runs);
        if (blk->rb_Type == RTBB_HEADING)
            (void)rtbFaceResolve(cb, ld, NULL, 11, RTBS_BOLD);
    }
    else if (blk->rb_Type == RTBB_HBOX || blk->rb_Type == RTBB_VBOX ||
        blk->rb_Type == RTBB_QUOTE)
    {
        for (ch = (struct RtbBlock *)blk->rb_Data.box.children.mlh_Head;
            ch->rb_Node.mln_Succ != NULL;
            ch = (struct RtbBlock *)ch->rb_Node.mln_Succ)
        {
            rtb_warm_block_fonts(cb, ld, ch);
        }
    }
}

void
rtbWarmDocumentFonts(struct ClassBase *cb, struct localData *ld)
{
    struct RtbBlock *blk;

    if (ld == NULL || ld->ld_Doc == NULL || !rtbMayOpenFonts(ld))
        return;
    rtbEnsureMeasureRP(cb, ld);
    for (blk = (struct RtbBlock *)ld->ld_Doc->rd_Blocks.mlh_Head;
        blk->rb_Node.mln_Succ != NULL;
        blk = (struct RtbBlock *)blk->rb_Node.mln_Succ)
    {
        rtb_warm_block_fonts(cb, ld, blk);
    }
}
