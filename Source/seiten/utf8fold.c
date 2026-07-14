/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * utf8fold.c - Port of AWeb Translate() UTF-8 display fold (no TTEngine,
 * no HTML entities). Source: AWeb3/Source/AWebAPL/parse.c Translate().
 */

#include <exec/types.h>
#include <string.h>

#include "utf8fold.h"

/* Latin Extended-A/B strip tables (trail - 0x80 indexes 0..63). */
static const UBYTE latin_ext_a_c4[] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiiiJjKkkLlLlLlL";
static const UBYTE latin_ext_a_c5[] =
    "lLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
static const UBYTE latin_ext_b_c6[] =
    "bBBbbbCCcDDDddEaEFfGGhIIKkllWNnOCoOoPpRS2EetTttUuUVYyZzEEee255?w";
static const UBYTE latin_ext_b_c7[] =
    "||||DDdLLlNNnAaIiOoUuUuUuUuUueAaAaAaGgGgKkOoOoEejDDdGgHWNnAaAaOo";

static void
fold_putc(UBYTE **dp, ULONG *left, UBYTE ch)
{
    if (*left > 1) {
        **dp = ch;
        (*dp)++;
        (*left)--;
    }
}

static void
fold_puts(UBYTE **dp, ULONG *left, STRPTR s)
{
    while (s != NULL && *s != '\0' && *left > 1) {
        **dp = (UBYTE)*s;
        (*dp)++;
        (*left)--;
        s++;
    }
}

ULONG
Utf8ToAmigaDisplay(UBYTE *dst, ULONG dstmax, STRPTR src)
{
    UBYTE *p;
    UBYTE *end;
    UBYTE *d;
    ULONG left;
    ULONG written;

    if (dst == NULL || dstmax == 0) {
        return 0;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return 0;
    }

    p = (UBYTE *)src;
    end = p + strlen((char *)src);
    d = dst;
    left = dstmax;

    while (p < end && left > 1) {
        ULONG utf8_bytes;
        UBYTE replacement;
        STRPTR replacement_str;
        BOOL skip_char;
        ULONG utf8_char;

        utf8_bytes = 0;
        replacement = 0;
        replacement_str = NULL;
        skip_char = FALSE;
        utf8_char = 0;

        /* 4-byte UTF-8 (emoji / non-BMP) -> middle-dot */
        if ((*p & 0xF8) == 0xF0 && p + 3 < end &&
            (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
            (p[3] & 0xC0) == 0x80) {
            utf8_bytes = 4;
            replacement = 0xB7;
        }
        /* 3-byte */
        else if ((*p & 0xF0) == 0xE0 && p + 2 < end &&
            (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            utf8_bytes = 3;
            utf8_char = ((ULONG)(*p & 0x0F) << 12) |
                ((ULONG)(p[1] & 0x3F) << 6) |
                (ULONG)(p[2] & 0x3F);

            if (*p == 0xE2 && p[1] == 0x80) {
                switch (p[2]) {
                case 0x8A:
                    replacement = 0x20;
                    break;
                case 0x93:
                    replacement = 0x2D;
                    break;
                case 0x94:
                    replacement_str = (STRPTR)"--";
                    break;
                case 0x98:
                    replacement = 0x60;
                    break;
                case 0x99:
                    replacement = 0x27;
                    break;
                case 0x9C:
                case 0x9D:
                    replacement = 0x22;
                    break;
                case 0xA6:
                    replacement_str = (STRPTR)"...";
                    break;
                case 0xAF:
                    replacement = 0x20;
                    break;
                default:
                    replacement = 0xB7;
                    break;
                }
            } else if (*p == 0xE2 && p[1] == 0x96) {
                if (p[2] == 0x88) {
                    replacement = 0x80;
                } else {
                    replacement = 0xB7;
                }
            } else {
                switch (utf8_char) {
                case 0x2018:
                    replacement = 0x60;
                    break;
                case 0x2019:
                case 0x201A:
                    replacement = 0x27;
                    break;
                case 0x201C:
                case 0x201D:
                case 0x201E:
                    replacement = 0x22;
                    break;
                case 0x2013:
                    replacement = 0x2D;
                    break;
                case 0x2014:
                    replacement_str = (STRPTR)"--";
                    break;
                case 0x2022:
                    replacement = 0x95;
                    break;
                case 0x2026:
                    replacement_str = (STRPTR)"...";
                    break;
                case 0x2039:
                    replacement = '<';
                    break;
                case 0x203A:
                    replacement = '>';
                    break;
                case 0x2122:
                    replacement_str = (STRPTR)"TM";
                    break;
                case 0x00A0:
                    replacement = 0x20;
                    break;
                case 0x00A9:
                    replacement = 0xA9;
                    break;
                case 0x00AE:
                    replacement = 0xAE;
                    break;
                default:
                    if (utf8_char >= 0x80 && utf8_char <= 0xFF) {
                        replacement = (UBYTE)utf8_char;
                    } else {
                        replacement = 0xB7;
                    }
                    break;
                }
            }
        }
        /* 2-byte */
        else if ((*p & 0xE0) == 0xC0 && *p >= 0xC2 && p + 1 < end &&
            (p[1] & 0xC0) == 0x80) {
            utf8_bytes = 2;
            utf8_char = ((ULONG)(*p & 0x1F) << 6) | (ULONG)(p[1] & 0x3F);

            if (*p == 0xC2) {
                if (p[1] == 0xAD) {
                    skip_char = TRUE;
                } else if (p[1] == 0xA0) {
                    replacement = 0x20;
                } else if (p[1] >= 0xA0) {
                    replacement = p[1];
                } else if (utf8_char >= 0x80 && utf8_char <= 0xFF) {
                    replacement = (UBYTE)utf8_char;
                } else {
                    replacement = 0xB7;
                }
            } else if (*p == 0xC3) {
                replacement = (UBYTE)(p[1] + 0x40);
            } else if (*p == 0xC4) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_a_c4[p[1] - 0x80];
                } else {
                    replacement = 0xB7;
                }
            } else if (*p == 0xC5) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_a_c5[p[1] - 0x80];
                } else {
                    replacement = 0xB7;
                }
            } else if (*p == 0xC6) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_b_c6[p[1] - 0x80];
                } else {
                    replacement = 0xB7;
                }
            } else if (*p == 0xC7) {
                if (p[1] >= 0x80 && p[1] <= 0xBF) {
                    replacement = latin_ext_b_c7[p[1] - 0x80];
                } else {
                    replacement = 0xB7;
                }
            } else if (*p == 0xCC || (*p == 0xCD && p[1] <= 0xAF)) {
                /* Combining marks — strip */
                skip_char = TRUE;
            } else if (utf8_char >= 0x80 && utf8_char <= 0xFF) {
                replacement = (UBYTE)utf8_char;
            } else {
                replacement = 0xB7;
            }
        }

        if (utf8_bytes > 0) {
            if (skip_char) {
                /* consume only */
            } else if (replacement_str != NULL) {
                fold_puts(&d, &left, replacement_str);
            } else if (replacement > 0) {
                fold_putc(&d, &left, replacement);
            } else {
                fold_putc(&d, &left, *p);
            }
            p += utf8_bytes;
        } else {
            /* ASCII or incomplete/invalid lead — copy one byte */
            fold_putc(&d, &left, *p);
            p++;
        }
    }

    *d = '\0';
    written = (ULONG)(d - dst);
    return written;
}
