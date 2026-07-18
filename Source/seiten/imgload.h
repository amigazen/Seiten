/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * imgload.h - CDN thumbs via AtpDownloadUrl on the GUI task
 *
 * Decode matches AWebAPL/imgsource.c: keep picture.datatype objects so
 * remapped pens stay allocated.  PDTM_SCALE before DTM_PROCLAYOUT so Remap
 * runs on a thumb, not a 1000px CDN original.
 */

#ifndef SEITEN_IMGLOAD_H
#define SEITEN_IMGLOAD_H

#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/classusr.h>
#include <gadgets/richtextbrowser.h>

struct SeitenGui;

#define SEITEN_THUMB_W     48
#define SEITEN_THUMB_H     48
#define SEITEN_MEDIA_MAX   4
#define SEITEN_MEDIA_MAX_W 280
#define SEITEN_MEDIA_MAX_H 200
/* Placeholders before decode (aspect updated on load). */
#define SEITEN_MEDIA_W     160
#define SEITEN_MEDIA_H     120
#define SEITEN_IMG_MAX_BYTES     (512UL * 1024UL)
#define SEITEN_LAZY_OVERSCAN     96

#define SEITEN_IMG_AVATAR  0
#define SEITEN_IMG_MEDIA   1

struct SeitenMedia
{
    STRPTR              sm_Url;
    Object             *sm_Dto;         /* kept — owns DestBitMap + pens */
    struct BitMap      *sm_BitMap;      /* PDTA_DestBitMap; do not FreeBitMap */
    UBYTE               sm_CachePath[108];
    BOOL                sm_Queued;
    BOOL                sm_Failed;
    BOOL                sm_IsEmbed;     /* RTBB_EMBED thumb, not IMAGEGRID */
    UWORD               sm_FitW;        /* last decode target width */
    UWORD               sm_FitH;
    RTB_BlockID         sm_BlockId;
};

struct SeitenRow
{
    struct MinNode      sr_Node;
    STRPTR              sr_ImageUrl;
    STRPTR              sr_Uri;
    STRPTR              sr_AuthorHandle;
    STRPTR              sr_Label;
    WORD                sr_BgPen;
    Object             *sr_Dto;         /* kept — owns DestBitMap + pens */
    struct BitMap      *sr_BitMap;      /* PDTA_DestBitMap; do not FreeBitMap */
    UBYTE               sr_CachePath[108];
    BOOL                sr_Queued;
    BOOL                sr_Failed;
    RTB_BlockID         sr_ImageBlockId;
    RTB_BlockID         sr_PostBlockId;
    /* OpenGraph card text retained for EMBED UPDATEBLOCK after thumb load. */
    STRPTR              sr_ExtTitle;
    STRPTR              sr_ExtDesc;
    STRPTR              sr_ExtSite;
    STRPTR              sr_ExtHref;
    struct SeitenMedia  sr_Media[SEITEN_MEDIA_MAX];
    UWORD               sr_MediaCount;
};

struct SeitenRow *SeitenRowNew(STRPTR image_url, STRPTR uri, STRPTR author);
void SeitenRowFree(struct SeitenRow *row);

BOOL SeitenImgInit(struct SeitenGui *gui);
void SeitenImgShutdown(struct SeitenGui *gui);
void SeitenImgStop(struct SeitenGui *gui);   /* quit: no new CDN GETs */
void SeitenImgCancelRow(struct SeitenGui *gui, struct SeitenRow *row);
void SeitenImgKick(struct SeitenGui *gui);
void SeitenImgPoll(struct SeitenGui *gui);
void SeitenImgFlushRefresh(struct SeitenGui *gui);
ULONG SeitenImgSigMask(struct SeitenGui *gui);

#endif /* SEITEN_IMGLOAD_H */
