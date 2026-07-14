/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * imgload.c - Fetch CDN thumbs via AtpDownloadUrl, show with bitmap.image
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/amiatp.h>
#include <proto/bitmap.h>
#include <images/bitmap.h>
#include <reaction/reaction_macros.h>
#include <string.h>
#include <stdio.h>

#include "engine.h"
#include "imgload.h"

static ULONG s_cache_seq;

static void
img_ensure_cache_dir(void)
{
    BPTR lock;

    lock = Lock((STRPTR)"T:Seiten", ACCESS_READ);
    if (lock != 0) {
        UnLock(lock);
        return;
    }
    CreateDir((STRPTR)"T:Seiten");
}

static STRPTR
img_strdup(STRPTR s)
{
    ULONG len;
    STRPTR d;

    if (s == NULL || s[0] == '\0') {
        return NULL;
    }
    len = strlen((char *)s) + 1;
    d = (STRPTR)AllocVec(len, MEMF_ANY);
    if (d != NULL) {
        strcpy((char *)d, (char *)s);
    }
    return d;
}

struct SeitenRow *
SeitenRowNew(STRPTR image_url, STRPTR uri, STRPTR author)
{
    struct SeitenRow *row;

    row = (struct SeitenRow *)AllocVec(sizeof(*row), MEMF_CLEAR);
    if (row == NULL) {
        return NULL;
    }
    row->sr_ImageUrl = img_strdup(image_url);
    row->sr_Uri = img_strdup(uri);
    row->sr_AuthorHandle = img_strdup(author);
    return row;
}

void
SeitenRowFree(struct SeitenRow *row)
{
    if (row == NULL) {
        return;
    }
    if (row->sr_BitMapObj != NULL) {
        DisposeObject(row->sr_BitMapObj);
        row->sr_BitMapObj = NULL;
    }
    if (row->sr_CachePath[0] != '\0') {
        DeleteFile(row->sr_CachePath);
        row->sr_CachePath[0] = '\0';
    }
    if (row->sr_ImageUrl != NULL) {
        FreeVec(row->sr_ImageUrl);
    }
    if (row->sr_Uri != NULL) {
        FreeVec(row->sr_Uri);
    }
    if (row->sr_AuthorHandle != NULL) {
        FreeVec(row->sr_AuthorHandle);
    }
    FreeVec(row);
}

BOOL
SeitenRowLoadImage(struct SeitenRow *row, struct SeitenEngine *eng,
    struct Screen *screen)
{
    LONG err;
    Object *obj;

    if (row == NULL || eng == NULL || eng->se_Conn == NULL || screen == NULL) {
        return FALSE;
    }
    if (row->sr_Tried) {
        return (row->sr_BitMapObj != NULL) ? TRUE : FALSE;
    }
    row->sr_Tried = TRUE;
    if (row->sr_ImageUrl == NULL) {
        return FALSE;
    }

    img_ensure_cache_dir();
    s_cache_seq++;
    sprintf((char *)row->sr_CachePath, "T:Seiten/img%lu.img",
        (unsigned long)s_cache_seq);

    err = AtpDownloadUrl(eng->se_Conn, row->sr_ImageUrl, row->sr_CachePath);
    if (err != 0) {
        row->sr_CachePath[0] = '\0';
        return FALSE;
    }

    obj = BitMapObject,
        BITMAP_SourceFile, row->sr_CachePath,
        BITMAP_Screen, screen,
        BITMAP_Width, SEITEN_THUMB_W,
        BITMAP_Height, SEITEN_THUMB_H,
        BITMAP_Masking, TRUE,
        IA_Width, SEITEN_THUMB_W,
        IA_Height, SEITEN_THUMB_H,
    End;
    if (obj == NULL) {
        DeleteFile(row->sr_CachePath);
        row->sr_CachePath[0] = '\0';
        return FALSE;
    }
    row->sr_BitMapObj = obj;
    return TRUE;
}
