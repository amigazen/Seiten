/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * imgload.c - CDN thumbs via AtpDownloadUrl on the GUI task
 *
 * Log evidence: a SeitenCDN Process always got HttpError=8704 (SSL handshake)
 * while the same CA + amiatp XRPC on the main task succeeded.  AmiSSL is not
 * safe across concurrent Exec tasks here — download on the GUI task instead.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>
#include <proto/amiatp.h>
#include <intuition/gadgetclass.h>
#include <libraries/amiatp.h>
#include <string.h>
#include <stdio.h>

#include "engine.h"
#include "gui.h"
#include "imgload.h"
#include "rtb_profile.h"

extern struct Library *DataTypesBase;

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

    if (s == NULL || s[0] == '\0')
        return NULL;
    len = strlen((char *)s) + 1;
    d = (STRPTR)AllocVec(len, MEMF_ANY);
    if (d != NULL)
        CopyMem((APTR)s, (APTR)d, len);
    return d;
}

static void
img_free_dto(Object **dto, struct BitMap **bm)
{
    if (bm != NULL)
        *bm = NULL;
    if (dto == NULL || *dto == NULL)
        return;
    DisposeDTObject(*dto);
    *dto = NULL;
}

static void
img_free_media(struct SeitenMedia *m)
{
    if (m == NULL)
        return;
    img_free_dto(&m->sm_Dto, &m->sm_BitMap);
    if (m->sm_CachePath[0] != '\0') {
        DeleteFile(m->sm_CachePath);
        m->sm_CachePath[0] = '\0';
    }
    if (m->sm_Url != NULL) {
        FreeVec(m->sm_Url);
        m->sm_Url = NULL;
    }
}

struct SeitenRow *
SeitenRowNew(STRPTR image_url, STRPTR uri, STRPTR author)
{
    struct SeitenRow *row;

    row = (struct SeitenRow *)AllocVec(sizeof(*row), MEMF_CLEAR);
    if (row == NULL)
        return NULL;
    row->sr_ImageUrl = img_strdup(image_url);
    row->sr_Uri = img_strdup(uri);
    row->sr_AuthorHandle = img_strdup(author);
    return row;
}

void
SeitenRowFree(struct SeitenRow *row)
{
    UWORD i;

    if (row == NULL)
        return;
    img_free_dto(&row->sr_Dto, &row->sr_BitMap);
    if (row->sr_CachePath[0] != '\0') {
        DeleteFile(row->sr_CachePath);
        row->sr_CachePath[0] = '\0';
    }
    if (row->sr_ImageUrl != NULL)
        FreeVec(row->sr_ImageUrl);
    if (row->sr_Uri != NULL)
        FreeVec(row->sr_Uri);
    if (row->sr_AuthorHandle != NULL)
        FreeVec(row->sr_AuthorHandle);
    if (row->sr_Label != NULL)
        FreeVec(row->sr_Label);
    if (row->sr_ExtTitle != NULL)
        FreeVec(row->sr_ExtTitle);
    if (row->sr_ExtDesc != NULL)
        FreeVec(row->sr_ExtDesc);
    if (row->sr_ExtSite != NULL)
        FreeVec(row->sr_ExtSite);
    if (row->sr_ExtHref != NULL)
        FreeVec(row->sr_ExtHref);
    for (i = 0; i < SEITEN_MEDIA_MAX; i++)
        img_free_media(&row->sr_Media[i]);
    FreeVec(row);
}

static CONST_STRPTR
img_ext_from_path(STRPTR path)
{
    BPTR fh;
    UBYTE mag[12];
    LONG n;

    if (path == NULL || path[0] == '\0')
        return (CONST_STRPTR)".bin";
    fh = Open(path, MODE_OLDFILE);
    if (fh == 0)
        return (CONST_STRPTR)".bin";
    n = Read(fh, mag, 12);
    Close(fh);
    if (n >= 3 && mag[0] == 0xFF && mag[1] == 0xD8 && mag[2] == 0xFF)
        return (CONST_STRPTR)".jpg";
    if (n >= 8 && mag[0] == 0x89 && mag[1] == 'P' && mag[2] == 'N' &&
        mag[3] == 'G')
        return (CONST_STRPTR)".png";
    if (n >= 6 && mag[0] == 'G' && mag[1] == 'I' && mag[2] == 'F')
        return (CONST_STRPTR)".gif";
    /* Bluesky CDN default is often WebP (RIFF....WEBP) — Amiga DT can't decode. */
    if (n >= 12 && mag[0] == 'R' && mag[1] == 'I' && mag[2] == 'F' &&
        mag[3] == 'F' && mag[8] == 'W' && mag[9] == 'E' && mag[10] == 'B' &&
        mag[11] == 'P')
        return (CONST_STRPTR)".webp";
    return (CONST_STRPTR)".bin";
}

/*
 * Bluesky CDN: append @jpeg so picture.datatype gets JFIF, not WebP.
 * e.g. .../bafkrei...@jpeg
 */
static void
img_url_prefer_jpeg(CONST_STRPTR in, STRPTR out, ULONG outmax)
{
    ULONG n;
    BOOL hasAt;

    if (out == NULL || outmax < 8) {
        return;
    }
    out[0] = '\0';
    if (in == NULL || in[0] == '\0')
        return;

    n = 0;
    hasAt = FALSE;
    while (in[n] != '\0' && n + 1 < outmax) {
        if (in[n] == '@')
            hasAt = TRUE;
        out[n] = in[n];
        n++;
    }
    out[n] = '\0';
    if (hasAt)
        return;

    /* Only rewrite known Bluesky image CDN paths. */
    if (strstr((char *)out, "cdn.bsky.app/img/") == NULL)
        return;

    if (n + 6 >= outmax)
        return;
    out[n++] = '@';
    out[n++] = 'j';
    out[n++] = 'p';
    out[n++] = 'e';
    out[n++] = 'g';
    out[n] = '\0';
}

static BOOL
img_rename_with_ext(STRPTR path, ULONG pathmax, CONST_STRPTR ext)
{
    UBYTE newpath[108];
    ULONG i;
    ULONG dot;
    ULONG len;

    if (path == NULL || ext == NULL || pathmax < 2)
        return FALSE;
    i = 0;
    dot = 0;
    while (path[i] != '\0' && i + 1 < sizeof(newpath)) {
        newpath[i] = path[i];
        if (path[i] == '.')
            dot = i;
        i++;
    }
    newpath[i] = '\0';
    if (dot > 0)
        i = dot;
    while (*ext != '\0' && i + 1 < sizeof(newpath) && i + 1 < pathmax) {
        newpath[i++] = *ext++;
        newpath[i] = '\0';
    }
    if (Rename(path, newpath) == 0)
        return FALSE;
    len = strlen((char *)newpath) + 1;
    if (len > pathmax)
        len = pathmax;
    CopyMem((APTR)newpath, (APTR)path, len);
    path[pathmax - 1] = '\0';
    return TRUE;
}

/*
 * Fit nw×nh inside maxW×maxH preserving aspect (letterbox into the max box).
 */
static void
img_fit_aspect(ULONG nw, ULONG nh, ULONG maxW, ULONG maxH,
    ULONG *outW, ULONG *outH)
{
    ULONG dw;
    ULONG dh;

    if (nw < 1)
        nw = 1;
    if (nh < 1)
        nh = 1;
    if (maxW < 1)
        maxW = 1;
    if (maxH < 1)
        maxH = 1;

    /* dw/dh = nw/nh scaled by min(maxW/nw, maxH/nh) using integer math. */
    if (nw * maxH <= nh * maxW) {
        dh = maxH;
        dw = (nw * maxH) / nh;
    } else {
        dw = maxW;
        dh = (nh * maxW) / nw;
    }
    if (dw < 1)
        dw = 1;
    if (dh < 1)
        dh = 1;
    if (dw > maxW)
        dw = maxW;
    if (dh > maxH)
        dh = maxH;
    *outW = dw;
    *outH = dh;
}

/*
 * AWebAPL/imgsource.c Makeobject tags, plus PDTM_SCALE before layout so
 * Remap/ObtainBestPen runs on a thumb.  Scale preserves aspect within maxW×maxH.
 * Keep the DT object: Dispose releases exclusive pens and remaps the palette.
 */
static BOOL
img_decode_aweb(STRPTR path, struct Screen *screen,
    ULONG maxW, ULONG maxH, Object **outDto, struct BitMap **outBm,
    ULONG *outW, ULONG *outH)
{
    Object *dto;
    struct gpLayout gpl;
    struct pdtScale sc;
    struct BitMap *src;
    ULONG nw;
    ULONG nh;
    ULONG dw;
    ULONG dh;
    ULONG w;
    ULONG h;

    if (outDto != NULL)
        *outDto = NULL;
    if (outBm != NULL)
        *outBm = NULL;
    if (outW != NULL)
        *outW = 0;
    if (outH != NULL)
        *outH = 0;
    if (path == NULL || screen == NULL || DataTypesBase == NULL ||
        outDto == NULL || outBm == NULL)
        return FALSE;
    if (maxW < 1)
        maxW = 48;
    if (maxH < 1)
        maxH = 48;

    src = NULL;
    nw = 0;
    nh = 0;
    /* Exact tag set from AWebAPL/imgsource.c (normal datatypes path). */
    dto = NewDTObject(path,
        DTA_SourceType, DTST_FILE,
        DTA_GroupID, GID_PICTURE,
        PDTA_Remap, TRUE,
        PDTA_Screen, screen,
        PDTA_FreeSourceBitMap, TRUE,
        PDTA_DestMode, PMODE_V43,
        PDTA_UseFriendBitMap, FALSE,
        OBP_Precision, PRECISION_IMAGE,
        TAG_END);
    if (dto == NULL) {
        Printf("Seiten: NewDTObject fail path=%s IoErr=%ld\n",
            path, IoErr());
        Flush(Output());
        return FALSE;
    }

    GetDTAttrs(dto,
        DTA_NominalHoriz, &nw,
        DTA_NominalVert, &nh,
        TAG_END);
    if (nw < 1 || nh < 1) {
        nw = maxW;
        nh = maxH;
    }
    img_fit_aspect(nw, nh, maxW, maxH, &dw, &dh);

    /* V45: scale pixel buffer before first GM_LAYOUT / Remap. */
    memset(&sc, 0, sizeof(sc));
    sc.MethodID = PDTM_SCALE;
    sc.ps_NewWidth = dw;
    sc.ps_NewHeight = dh;
    sc.ps_Flags = 0;
    DoMethodA(dto, (Msg)&sc);

    memset(&gpl, 0, sizeof(gpl));
    gpl.MethodID = DTM_PROCLAYOUT;
    gpl.gpl_GInfo = NULL;
    gpl.gpl_Initial = TRUE;
    if (!DoMethodA(dto, (Msg)&gpl)) {
        PutStr("Seiten: DTM_PROCLAYOUT fail\n");
        Flush(Output());
        DisposeDTObject(dto);
        return FALSE;
    }

    GetDTAttrs(dto, PDTA_DestBitMap, &src, TAG_END);
    if (src == NULL) {
        PutStr("Seiten: PDTA_DestBitMap NULL\n");
        Flush(Output());
        DisposeDTObject(dto);
        return FALSE;
    }

    w = GetBitMapAttr(src, BMA_WIDTH);
    h = GetBitMapAttr(src, BMA_HEIGHT);
    if (w < 1 || h < 1) {
        PutStr("Seiten: DestBitMap empty dims\n");
        Flush(Output());
        DisposeDTObject(dto);
        return FALSE;
    }

    *outDto = dto;
    *outBm = src;
    if (outW != NULL)
        *outW = w;
    if (outH != NULL)
        *outH = h;
    Printf("Seiten: CDN decode %lux%lu -> %lux%lu (AWeb DTO kept)\n",
        (unsigned long)nw, (unsigned long)nh,
        (unsigned long)w, (unsigned long)h);
    Flush(Output());
    return TRUE;
}

static void
img_mark_fail(struct SeitenRow *row, UWORD kind, UWORD index)
{
    if (row == NULL)
        return;
    if (kind == SEITEN_IMG_AVATAR)
        row->sr_Failed = TRUE;
    else if (index < SEITEN_MEDIA_MAX)
        row->sr_Media[index].sm_Failed = TRUE;
}

static void
img_show_fail(struct SeitenGui *gui, struct SeitenRow *row,
    UWORD kind, UWORD index)
{
    (void)gui;
    img_mark_fail(row, kind, index);
}

/*
 * Content column for media: gadget width minus avatar + gaps.
 * Side-by-side posts split this across columns.
 */
static void
img_media_target(struct SeitenGui *gui, struct SeitenRow *row,
    UWORD index, ULONG *maxW, ULONG *maxH)
{
    ULONG gw;
    ULONG content;
    ULONG cols;
    ULONG gridN;
    UWORD i;

    gw = 0;
    if (gui != NULL && gui->sg_Rtb != NULL)
        GetAttr(GA_Width, gui->sg_Rtb, &gw);
    content = (gw > 120UL) ? (gw - 100UL) : (ULONG)SEITEN_MEDIA_MAX_W;

    gridN = 0;
    for (i = 0; i < row->sr_MediaCount; i++)
    {
        if (!row->sr_Media[i].sm_IsEmbed)
            gridN++;
    }
    if (index < SEITEN_MEDIA_MAX && row->sr_Media[index].sm_IsEmbed)
    {
        *maxW = 72;
        *maxH = 72;
        return;
    }
    cols = (gridN >= 2) ? 2 : 1;
    *maxW = content / cols;
    if (*maxW > (ULONG)SEITEN_MEDIA_MAX_W)
        *maxW = SEITEN_MEDIA_MAX_W;
    if (*maxW < 48)
        *maxW = 48;
    *maxH = SEITEN_MEDIA_MAX_H;
}

static void
img_apply_success(struct SeitenGui *gui, struct SeitenRow *row,
    UWORD kind, UWORD index, STRPTR path)
{
    struct Screen *scr;
    Object *dto;
    struct BitMap *bm;
    struct RtbBlock *blk;
    ULONG maxW;
    ULONG maxH;
    ULONG aw;
    ULONG ah;
    WORD w;
    WORD h;

    if (gui == NULL || row == NULL || gui->sg_Rtb == NULL || path == NULL)
        return;
    if (gui->sg_ImgStop) {
        img_show_fail(gui, row, kind, index);
        return;
    }
    scr = (gui->sg_Window != NULL) ? gui->sg_Window->WScreen : NULL;
    if (scr == NULL) {
        img_show_fail(gui, row, kind, index);
        return;
    }

    if (kind == SEITEN_IMG_AVATAR) {
        maxW = SEITEN_THUMB_W;
        maxH = SEITEN_THUMB_H;
    } else {
        img_media_target(gui, row, index, &maxW, &maxH);
    }

    dto = NULL;
    bm = NULL;
    aw = 0;
    ah = 0;
    if (!img_decode_aweb(path, scr, maxW, maxH, &dto, &bm, &aw, &ah) ||
        dto == NULL || bm == NULL || aw < 1 || ah < 1)
    {
        Printf("Seiten: CDN decode failed (%s)\n", path);
        Flush(Output());
        /* Only delete fresh downloads — keep T: cache for a later retry. */
        if (strstr((char *)path, ".tmp") != NULL)
            DeleteFile(path);
        img_show_fail(gui, row, kind, index);
        return;
    }
    w = (WORD)aw;
    h = (WORD)ah;

    if (kind == SEITEN_IMG_AVATAR) {
        CopyMem((APTR)path, (APTR)row->sr_CachePath, sizeof(row->sr_CachePath));
        img_free_dto(&row->sr_Dto, &row->sr_BitMap);
        row->sr_Dto = dto;
        row->sr_BitMap = bm;
        blk = AllocRtbBlockTags(RTBB_IMAGE,
            RTBA_ID, row->sr_ImageBlockId,
            RTBA_User, (ULONG)row,
            RTBA_BitMap, (ULONG)bm,
            RTBA_Width, w,
            RTBA_Height, h,
            RTBA_BgPen, (ULONG)row->sr_BgPen,
            TAG_DONE);
    } else if (index < SEITEN_MEDIA_MAX) {
        struct SeitenMedia *m;

        m = &row->sr_Media[index];
        CopyMem((APTR)path, (APTR)m->sm_CachePath, sizeof(m->sm_CachePath));
        img_free_dto(&m->sm_Dto, &m->sm_BitMap);
        m->sm_Dto = dto;
        m->sm_BitMap = bm;
        m->sm_FitW = (UWORD)maxW;
        m->sm_FitH = (UWORD)maxH;
        if (m->sm_IsEmbed)
        {
            blk = AllocRtbBlockTags(RTBB_EMBED,
                RTBA_ID, m->sm_BlockId,
                RTBA_User, (ULONG)row,
                RTBA_BitMap, (ULONG)bm,
                RTBA_Width, w,
                RTBA_Height, h,
                RTBA_BgPen, (ULONG)row->sr_BgPen,
                RTBA_Title, row->sr_ExtTitle != NULL ?
                    (ULONG)row->sr_ExtTitle : (ULONG)"",
                RTBA_Description, row->sr_ExtDesc != NULL ?
                    (ULONG)row->sr_ExtDesc : (ULONG)"",
                RTBA_Site, row->sr_ExtSite != NULL ?
                    (ULONG)row->sr_ExtSite : (ULONG)"",
                RTBA_Href, row->sr_ExtHref != NULL ?
                    (ULONG)row->sr_ExtHref : (ULONG)"",
                TAG_DONE);
        }
        else
        {
            blk = AllocRtbBlockTags(RTBB_IMAGE,
                RTBA_ID, m->sm_BlockId,
                RTBA_User, (ULONG)row,
                RTBA_BitMap, (ULONG)bm,
                RTBA_Width, w,
                RTBA_Height, h,
                RTBA_BgPen, (ULONG)row->sr_BgPen,
                TAG_DONE);
        }
    } else {
        img_free_dto(&dto, &bm);
        return;
    }

    if (blk != NULL) {
        DoMethod(gui->sg_Rtb, RTBM_UPDATEBLOCK, NULL, blk);
        gui->sg_ImgNeedRefresh = TRUE;
        gui->sg_ImgRefreshHold++;
    }
}

BOOL
SeitenImgInit(struct SeitenGui *gui)
{
    if (gui == NULL)
        return FALSE;

    gui->sg_ImgBusy = FALSE;
    gui->sg_ImgStop = FALSE;
    gui->sg_ImgNeedRefresh = FALSE;
    gui->sg_ImgRefreshHold = 0;

    if (DataTypesBase == NULL) {
        DataTypesBase = OpenLibrary("datatypes.library", 39);
        if (DataTypesBase == NULL)
            return FALSE;
    }
    PutStr("Seiten: CDN loader ready (AWeb picture.datatype + main-task GET)\n");
    Flush(Output());
    return TRUE;
}

void
SeitenImgStop(struct SeitenGui *gui)
{
    struct SeitenRow *row;
    UWORD mi;

    if (gui == NULL)
        return;
    gui->sg_ImgStop = TRUE;
    /* Drop the queue — do not start further AtpDownloadUrl calls. */
    for (row = (struct SeitenRow *)gui->sg_Posts.mlh_Head;
        row->sr_Node.mln_Succ != NULL;
        row = (struct SeitenRow *)row->sr_Node.mln_Succ)
    {
        if (row->sr_BitMap == NULL && row->sr_ImageUrl != NULL)
            row->sr_Failed = TRUE;
        for (mi = 0; mi < row->sr_MediaCount; mi++) {
            if (row->sr_Media[mi].sm_BitMap == NULL &&
                row->sr_Media[mi].sm_Url != NULL)
                row->sr_Media[mi].sm_Failed = TRUE;
        }
    }
}

void
SeitenImgShutdown(struct SeitenGui *gui)
{
    if (gui == NULL)
        return;
    SeitenImgStop(gui);
    gui->sg_ImgBusy = FALSE;
    gui->sg_ImgNeedRefresh = FALSE;
}

ULONG
SeitenImgSigMask(struct SeitenGui *gui)
{
    (void)gui;
    return 0;
}

void
SeitenImgCancelRow(struct SeitenGui *gui, struct SeitenRow *row)
{
    (void)gui;
    (void)row;
}

void
SeitenImgPoll(struct SeitenGui *gui)
{
    (void)gui;
}

void
SeitenImgFlushRefresh(struct SeitenGui *gui)
{
    BOOL doIt;

    if (gui == NULL || gui->sg_ImgStop || gui->sg_ImgBusy)
        return;
    if (!gui->sg_ImgNeedRefresh)
        return;

    /*
     * RefreshGList every image remaps the whole feed and flickers.
     * Batch: every 4 thumbs, or when the queue is idle on next Kick miss.
     */
    doIt = FALSE;
    if (gui->sg_ImgRefreshHold >= 4) {
        doIt = TRUE;
    }
    if (!doIt)
        return;

    gui->sg_ImgNeedRefresh = FALSE;
    gui->sg_ImgRefreshHold = 0;
    if (gui->sg_Window != NULL && gui->sg_Rtb != NULL)
        RefreshGList((struct Gadget *)gui->sg_Rtb, gui->sg_Window, NULL, 1);
}

void
SeitenImgKick(struct SeitenGui *gui)
{
    struct SeitenRow *row;
    UWORD mi;
    STRPTR url;
    UWORD kind;
    UWORD index;
    UBYTE path[108];
    UBYTE fetchUrl[320];
    LONG err;
    CONST_STRPTR ext;
    BOOL anyPending;
    ULONG top;
    ULONG vis;
    LONG viewTop;
    LONG viewBot;

    if (gui == NULL || gui->sg_ImgStop || gui->sg_Loading ||
        !gui->sg_LoggedIn || gui->sg_ImgBusy || gui->eng.se_Conn == NULL)
        return;

    url = NULL;
    kind = 0;
    index = 0;
    row = NULL;
    anyPending = FALSE;
    top = 0;
    vis = 0;

    /* Lazy like AWeb displayed decode: only fetch rows in/near the viewport. */
    GetAttr(RTB_Top, gui->sg_Rtb, &top);
    GetAttr(RTB_Visible, gui->sg_Rtb, &vis);
    viewTop = (LONG)top;
    viewBot = (LONG)(top + vis + (ULONG)SEITEN_LAZY_OVERSCAN);
    if (viewTop > SEITEN_LAZY_OVERSCAN)
        viewTop -= SEITEN_LAZY_OVERSCAN;
    else
        viewTop = 0;

    for (row = (struct SeitenRow *)gui->sg_Posts.mlh_Head;
        row->sr_Node.mln_Succ != NULL;
        row = (struct SeitenRow *)row->sr_Node.mln_Succ)
    {
        struct rtbBlockBounds bb;
        BOOL inView;

        inView = TRUE;
        if (row->sr_PostBlockId != 0) {
            memset(&bb, 0, sizeof(bb));
            bb.MethodID = RTBM_BLOCKBOUNDS;
            bb.GInfo = NULL;
            bb.BlockID = row->sr_PostBlockId;
            if (DoMethodA(gui->sg_Rtb, (Msg)&bb)) {
                if (bb.OutY + bb.OutHeight < viewTop ||
                    bb.OutY > viewBot)
                    inView = FALSE;
            }
        }

        if (!inView)
            continue;

        /* Viewport resized: dispose DTO and redecode from T: cache. */
        for (mi = 0; mi < row->sr_MediaCount; mi++) {
            struct SeitenMedia *m;
            ULONG tw;
            ULONG th;
            LONG diff;

            m = &row->sr_Media[mi];
            if (m->sm_BitMap == NULL || m->sm_CachePath[0] == '\0' ||
                m->sm_IsEmbed)
                continue;
            img_media_target(gui, row, mi, &tw, &th);
            diff = (LONG)m->sm_FitW - (LONG)tw;
            if (diff < 0)
                diff = -diff;
            if (diff > 16) {
                img_free_dto(&m->sm_Dto, &m->sm_BitMap);
                m->sm_Queued = FALSE;
                m->sm_FitW = 0;
            }
        }

        if (!row->sr_Queued && row->sr_ImageUrl != NULL &&
            row->sr_BitMap == NULL && !row->sr_Failed)
        {
            url = row->sr_ImageUrl;
            kind = SEITEN_IMG_AVATAR;
            index = 0;
            anyPending = TRUE;
            break;
        }
        for (mi = 0; mi < row->sr_MediaCount; mi++) {
            struct SeitenMedia *m;

            m = &row->sr_Media[mi];
            /* Prefer T: cache redecode (no CDN GET). */
            if (!m->sm_Queued && m->sm_CachePath[0] != '\0' &&
                m->sm_BitMap == NULL && !m->sm_Failed)
            {
                m->sm_Queued = TRUE;
                gui->sg_ImgBusy = TRUE;
                img_apply_success(gui, row, SEITEN_IMG_MEDIA, mi,
                    m->sm_CachePath);
                gui->sg_ImgBusy = FALSE;
                return;
            }
            if (!m->sm_Queued &&
                m->sm_Url != NULL &&
                m->sm_BitMap == NULL &&
                !m->sm_Failed)
            {
                url = m->sm_Url;
                kind = SEITEN_IMG_MEDIA;
                index = mi;
                anyPending = TRUE;
                break;
            }
        }
        if (url != NULL)
            break;
    }
    if (url == NULL || row == NULL) {
        /* Queue empty (or nothing visible) — flush held refresh. */
        if (gui->sg_ImgNeedRefresh && !gui->sg_ImgBusy) {
            gui->sg_ImgRefreshHold = 4;
            SeitenImgFlushRefresh(gui);
        }
        (void)anyPending;
        return;
    }

    if (kind == SEITEN_IMG_AVATAR)
        row->sr_Queued = TRUE;
    else
        row->sr_Media[index].sm_Queued = TRUE;

    gui->sg_ImgBusy = TRUE;
    img_ensure_cache_dir();
    s_cache_seq++;
    if (kind == SEITEN_IMG_AVATAR)
        sprintf((char *)path, "T:Seiten/img%lu.tmp",
            (unsigned long)s_cache_seq);
    else
        sprintf((char *)path, "T:Seiten/med%lu.tmp",
            (unsigned long)s_cache_seq);

    img_url_prefer_jpeg(url, fetchUrl, sizeof(fetchUrl));
    if (fetchUrl[0] == '\0' || gui->sg_ImgStop) {
        img_show_fail(gui, row, kind, index);
        gui->sg_ImgBusy = FALSE;
        return;
    }

    Printf("Seiten: CDN GET %s\n", fetchUrl);
    Flush(Output());

    err = AtpDownloadUrl(gui->eng.se_Conn, fetchUrl, path);
    if (gui->sg_ImgStop) {
        DeleteFile(path);
        img_show_fail(gui, row, kind, index);
        gui->sg_ImgBusy = FALSE;
        return;
    }
    if (err != 0) {
        Printf("Seiten: CDN AtpDownloadUrl err=%ld\n", err);
        Flush(Output());
        DeleteFile(path);
        img_show_fail(gui, row, kind, index);
        gui->sg_ImgBusy = FALSE;
        return;
    }

    ext = img_ext_from_path(path);
    if (ext[0] == '.' && ext[1] == 'w') {
        PutStr("Seiten: CDN got WebP (need @jpeg); skip decode\n");
        Flush(Output());
        DeleteFile(path);
        img_show_fail(gui, row, kind, index);
        gui->sg_ImgBusy = FALSE;
        return;
    }
    if (ext[0] == '.' && ext[1] == 'b') {
        Printf("Seiten: CDN unknown format magic; skip decode\n");
        Flush(Output());
        DeleteFile(path);
        img_show_fail(gui, row, kind, index);
        gui->sg_ImgBusy = FALSE;
        return;
    }

    img_rename_with_ext(path, sizeof(path), ext);
    Printf("Seiten: CDN ok -> %s\n", path);
    Flush(Output());
    if (!gui->sg_ImgStop)
        img_apply_success(gui, row, kind, index, path);
    else {
        DeleteFile(path);
        img_show_fail(gui, row, kind, index);
    }
    gui->sg_ImgBusy = FALSE;
}
