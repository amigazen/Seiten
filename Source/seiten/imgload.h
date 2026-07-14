/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * imgload.h - CDN download + bitmap.image for listbrowser thumbs
 */

#ifndef SEITEN_IMGLOAD_H
#define SEITEN_IMGLOAD_H

#include <exec/types.h>
#include <intuition/intuition.h>

struct SeitenEngine;
struct Screen;

#define SEITEN_THUMB_W  48
#define SEITEN_THUMB_H  48

struct SeitenRow
{
    STRPTR          sr_ImageUrl;
    STRPTR          sr_Uri;
    STRPTR          sr_AuthorHandle;
    Object         *sr_BitMapObj;
    UBYTE           sr_CachePath[108];
    BOOL            sr_Tried;
};

struct SeitenRow *SeitenRowNew(STRPTR image_url, STRPTR uri, STRPTR author);
void SeitenRowFree(struct SeitenRow *row);

/* Download + BitMapObject; returns TRUE if sr_BitMapObj set. */
BOOL SeitenRowLoadImage(struct SeitenRow *row, struct SeitenEngine *eng,
    struct Screen *screen);

#endif /* SEITEN_IMGLOAD_H */
