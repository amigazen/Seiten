/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * cp_ttfont.c - ttengine.library scalable TrueType faces
 *
 * Priority path for richtextbrowser text. Bitmap diskfont is fallback only.
 * Pattern follows gfx/page/pg_ttfont.c and TTEngine autodoc examples.
 */

#include "classbase.h"

#include <libraries/ttengine.h>
#include <clib/ttengine_protos.h>
#include <pragma/ttengine_lib.h>

static UBYTE
tt_lower(UBYTE c)
{
    if (c >= 'A' && c <= 'Z')
        return (UBYTE)(c + 32);
    return c;
}

static BOOL
tt_name_starts(CONST_STRPTR name, CONST_STRPTR needle)
{
    ULONG i;

    if (name == NULL || needle == NULL)
        return FALSE;
    for (i = 0; needle[i] != '\0'; i++)
    {
        if (name[i] == '\0')
            return FALSE;
        if (tt_lower((UBYTE)name[i]) != tt_lower((UBYTE)needle[i]))
            return FALSE;
    }
    return TRUE;
}

static BOOL
tt_ends_with(CONST_STRPTR name, CONST_STRPTR suf)
{
    ULONG nlen;
    ULONG slen;
    ULONG i;

    if (name == NULL || suf == NULL)
        return FALSE;
    nlen = strlen((char *)name);
    slen = strlen((char *)suf);
    if (nlen < slen)
        return FALSE;
    for (i = 0; i < slen; i++)
    {
        if (tt_lower((UBYTE)name[nlen - slen + i]) !=
            tt_lower((UBYTE)suf[i]))
            return FALSE;
    }
    return TRUE;
}

BOOL
CP_TTAvailable(struct ClassBase *cb)
{
    if (cb != NULL && cb->cb_TTEngineBase != NULL)
        return TRUE;
    return FALSE;
}

BOOL
CP_TTMapFamily(CONST_STRPTR fontName, ULONG style,
    STRPTR familyOut, ULONG familyLen, BOOL *boldOut, BOOL *italicOut)
{
    CONST_STRPTR family;
    BOOL bold;
    BOOL italic;
    ULONG i;

    if (familyOut == NULL || familyLen < 8)
        return FALSE;

    bold = (style & RTBS_BOLD) ? TRUE : FALSE;
    italic = (style & RTBS_ITALIC) ? TRUE : FALSE;
    family = "Times New Roman";

    if (fontName != NULL && fontName[0] != '\0')
    {
        if (tt_ends_with(fontName, ".ttf") || tt_ends_with(fontName, ".otf"))
        {
            /* Caller should use TT_FontFile instead */
            family = fontName;
        }
        else if (tt_name_starts(fontName, "arial") ||
            tt_name_starts(fontName, "helvetica") ||
            tt_name_starts(fontName, "sans") ||
            tt_name_starts(fontName, "emerald") ||
            tt_name_starts(fontName, "dejavu sans") ||
            tt_name_starts(fontName, "cgtrium"))
        {
            family = "Arial";
        }
        else if (tt_name_starts(fontName, "courier") ||
            tt_name_starts(fontName, "mono") ||
            tt_name_starts(fontName, "pearl"))
        {
            family = "Courier New";
        }
        else if (tt_name_starts(fontName, "times") ||
            tt_name_starts(fontName, "roman") ||
            tt_name_starts(fontName, "palatino") ||
            tt_name_starts(fontName, "cgtimes") ||
            tt_name_starts(fontName, "serif"))
        {
            family = "Times New Roman";
        }
        else
        {
            /* Use the provided name as a TT family directly */
            family = fontName;
        }
    }

    if (boldOut != NULL)
        *boldOut = bold;
    if (italicOut != NULL)
        *italicOut = italic;

    for (i = 0; i + 1 < familyLen && family[i] != '\0'; i++)
        familyOut[i] = family[i];
    familyOut[i] = '\0';
    return TRUE;
}

APTR
CP_TTOpenFace(struct ClassBase *cb, CONST_STRPTR preferName, UWORD sizePx,
    ULONG style)
{
    UBYTE family[48];
    BOOL bold;
    BOOL italic;
    UBYTE *familytable[4];
    struct TagItem tags[6];
    APTR handle;

    if (cb == NULL || TTEngineBase == NULL || sizePx < 1)
        return NULL;

    if (preferName != NULL &&
        (tt_ends_with(preferName, ".ttf") || tt_ends_with(preferName, ".otf")))
    {
        tags[0].ti_Tag = TT_FontFile;
        tags[0].ti_Data = (ULONG)preferName;
        tags[1].ti_Tag = TT_FontSize;
        tags[1].ti_Data = (ULONG)sizePx;
        tags[2].ti_Tag = TT_FontWeight;
        tags[2].ti_Data = (ULONG)((style & RTBS_BOLD) ?
            TT_FontWeight_Bold : TT_FontWeight_Normal);
        tags[3].ti_Tag = TT_FontStyle;
        tags[3].ti_Data = (ULONG)((style & RTBS_ITALIC) ?
            TT_FontStyle_Italic : TT_FontStyle_Regular);
        tags[4].ti_Tag = TAG_END;
        tags[4].ti_Data = 0;
        return TT_OpenFontA(tags);
    }

    if (!CP_TTMapFamily(preferName, style, (STRPTR)family, sizeof(family),
            &bold, &italic))
        return NULL;

    familytable[0] = family;
    if (tt_name_starts((STRPTR)family, "Courier"))
        familytable[1] = (UBYTE *)"monospaced";
    else if (tt_name_starts((STRPTR)family, "Arial"))
        familytable[1] = (UBYTE *)"sans-serif";
    else
        familytable[1] = (UBYTE *)"serif";
    familytable[2] = (UBYTE *)"default";
    familytable[3] = NULL;

    tags[0].ti_Tag = TT_FamilyTable;
    tags[0].ti_Data = (ULONG)familytable;
    tags[1].ti_Tag = TT_FontSize;
    tags[1].ti_Data = (ULONG)sizePx;
    tags[2].ti_Tag = TT_FontWeight;
    tags[2].ti_Data = (ULONG)(bold ? TT_FontWeight_Bold : TT_FontWeight_Normal);
    tags[3].ti_Tag = TT_FontStyle;
    tags[3].ti_Data = (ULONG)(italic ? TT_FontStyle_Italic :
        TT_FontStyle_Regular);
    tags[4].ti_Tag = TAG_END;
    tags[4].ti_Data = 0;

    handle = TT_OpenFontA(tags);
    return handle;
}

void
CP_TTCloseFace(struct ClassBase *cb, APTR handle)
{
    if (cb == NULL || handle == NULL || TTEngineBase == NULL)
        return;
    TT_CloseFont(handle);
}

static void
tt_apply_rp_attrs(struct ClassBase *cb, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm)
{
    struct TagItem tags[6];
    LONG idx;

    (void)cb;
    idx = 0;
    if (win != NULL)
    {
        tags[idx].ti_Tag = TT_Window;
        tags[idx].ti_Data = (ULONG)win;
        idx++;
    }
    if (cm != NULL)
    {
        tags[idx].ti_Tag = TT_ColorMap;
        tags[idx].ti_Data = (ULONG)cm;
        idx++;
    }
    tags[idx].ti_Tag = TT_Encoding;
    tags[idx].ti_Data = (ULONG)TT_Encoding_ISO8859_1;
    idx++;
    tags[idx].ti_Tag = TT_Antialias;
    tags[idx].ti_Data = (ULONG)TT_Antialias_Auto;
    idx++;
    tags[idx].ti_Tag = TT_DiskFontMetrics;
    tags[idx].ti_Data = (ULONG)TRUE;
    idx++;
    tags[idx].ti_Tag = TAG_END;
    tags[idx].ti_Data = 0;
    TT_SetAttrsA(rp, tags);
}

BOOL
CP_TTGetMetrics(struct ClassBase *cb, APTR handle, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm,
    WORD *heightOut, WORD *baselineOut, WORD *avgWidthOut)
{
    ULONG h;
    ULONG bl;
    ULONG w;

    if (cb == NULL || handle == NULL || rp == NULL || TTEngineBase == NULL)
        return FALSE;
    if (!TT_SetFont(rp, handle))
        return FALSE;
    tt_apply_rp_attrs(cb, rp, win, cm);

    h = 0;
    bl = 0;
    w = 0;
    TT_GetAttrs(rp, TT_FontHeight, &h, TT_FontBaseline, &bl,
        TT_FontWidth, &w, TAG_DONE);
    if (h < 1)
        h = 12;
    if (bl < 1)
        bl = h - 2;
    if (w < 1)
        w = h / 2;
    if (heightOut != NULL)
        *heightOut = (WORD)h;
    if (baselineOut != NULL)
        *baselineOut = (WORD)bl;
    if (avgWidthOut != NULL)
        *avgWidthOut = (WORD)w;
    return TRUE;
}

WORD
CP_TTTextWidth(struct ClassBase *cb, APTR handle, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm,
    CONST_STRPTR text, ULONG len)
{
    if (cb == NULL || handle == NULL || rp == NULL || text == NULL ||
        TTEngineBase == NULL)
        return 0;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }
    if (!TT_SetFont(rp, handle))
        return 0;
    tt_apply_rp_attrs(cb, rp, win, cm);
    return (WORD)TT_TextLength(rp, (APTR)text, len);
}

BOOL
CP_TTDrawText(struct ClassBase *cb, APTR handle, struct RastPort *rp,
    struct Window *win, struct ColorMap *cm,
    WORD x, WORD y, CONST_STRPTR text, ULONG len, WORD fgPen)
{
    ULONG soft;

    if (cb == NULL || handle == NULL || rp == NULL || text == NULL ||
        TTEngineBase == NULL)
        return FALSE;
    if (len == 0)
    {
        while (text[len] != '\0')
            len++;
    }
    if (!TT_SetFont(rp, handle))
        return FALSE;
    tt_apply_rp_attrs(cb, rp, win, cm);
    soft = TT_SoftStyle_None;
    TT_SetAttrs(rp, TT_SoftStyle, soft, TAG_DONE);
    SetAPen(rp, fgPen);
    SetDrMd(rp, JAM1);
    Move(rp, x, y);
    TT_Text(rp, (APTR)text, len);
    return TRUE;
}
