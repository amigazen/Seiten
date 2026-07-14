/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * utf8fold.h - UTF-8 to Amiga Latin-1 display fold (AWeb Translate cheats)
 */

#ifndef SEITEN_UTF8FOLD_H
#define SEITEN_UTF8FOLD_H

#include <exec/types.h>

/*
 * Fold UTF-8 source into dst for Amiga UI gadgets (Latin-1-ish).
 * Unmapped / emoji / 4-byte sequences become 0xB7 middle-dot.
 * Returns number of bytes written (not including NUL). Always NUL-terminates
 * if dstmax > 0.
 */
ULONG Utf8ToAmigaDisplay(UBYTE *dst, ULONG dstmax, STRPTR src);

#endif /* SEITEN_UTF8FOLD_H */
