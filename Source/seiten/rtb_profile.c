/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtb_profile.c - AtpFeedPost → richtextbrowser feed rows
 *
 * Row shape:
 *   HBOX( avatar IMAGE | VBOX( para, IMAGEGRID?, EMBED?, CONTROLROW, RULE ) )
 * Handle: DejaVu Sans bold (taller); body: Helvetica. URLs → RTBR_LINK.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/amiatp.h>
#include <proto/datatypes.h>
#include <datatypes/pictureclass.h>
#include <images/bitmap.h>
#include <string.h>

#include "gui.h"
#include "utf8fold.h"
#include "imgload.h"
#include "rtb_profile.h"

extern struct Library *DataTypesBase;

static RTB_BlockID
rtb_next_id(struct SeitenGui *gui)
{
    struct RtbDocument *doc;

    doc = NULL;
    GetAttr(RTB_Document, gui->sg_Rtb, (ULONG *)&doc);
    if (doc == NULL)
        return 0;
    if (doc->rd_NextBlockID == 0)
        doc->rd_NextBlockID = 1;
    return doc->rd_NextBlockID++;
}

static void
rtb_add_icon(struct RtbBlock *ctl, struct SeitenRow *row,
    Object *imgObj, Object *selObj, CONST_STRPTR fallbackLabel)
{
    struct RtbRun *run;

    if (ctl == NULL)
        return;
    if (imgObj != NULL)
    {
        run = AllocRtbRunTags(RTBR_CONTROL,
            RTBA_CtlKind, RTBC_ICON,
            RTBA_BitMapObject, (ULONG)imgObj,
            RTBA_SelBitMapObject, (ULONG)selObj,
            RTBA_Width, 16,
            RTBA_Height, 16,
            RTBA_User, (ULONG)row,
            TAG_DONE);
    }
    else
    {
        /* Fallback labelled bevel if AISS artwork missing. */
        run = AllocRtbRunTags(RTBR_CONTROL,
            RTBA_CtlKind, RTBC_BUTTON,
            RTBA_Label, (ULONG)fallbackLabel,
            RTBA_User, (ULONG)row,
            TAG_DONE);
    }
    if (run != NULL)
        RtbBlockAddRun(ctl, run);
}

static STRPTR
rtb_strdup(STRPTR s)
{
    ULONG n;
    STRPTR d;

    if (s == NULL)
        return NULL;
    n = strlen((char *)s) + 1;
    d = (STRPTR)AllocVec(n, MEMF_ANY);
    if (d == NULL)
        return NULL;
    CopyMem((APTR)s, (APTR)d, n);
    return d;
}

/* Extract host from http(s) URL into out (truncated). */
static void
rtb_site_from_uri(STRPTR uri, STRPTR out, ULONG outMax)
{
    STRPTR p;
    ULONG n;

    if (out == NULL || outMax < 2)
        return;
    out[0] = '\0';
    if (uri == NULL)
        return;
    p = uri;
    if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p')
    {
        p += 4;
        if (*p == 's')
            p++;
        if (p[0] == ':' && p[1] == '/' && p[2] == '/')
            p += 3;
    }
    n = 0;
    while (*p != '\0' && *p != '/' && *p != '?' && *p != '#' &&
        n + 1 < outMax)
    {
        out[n++] = *p++;
        out[n] = '\0';
    }
}

/*
 * Append body as TEXT / LINK runs. http(s) URLs become RTBR_LINK (FILLPEN).
 */
static BOOL
rtb_append_body_runs(struct SeitenGui *gui, struct RtbBlock *para,
    STRPTR body)
{
    ULONG pos;
    ULONG len;

    if (para == NULL || body == NULL)
        return FALSE;
    len = strlen((char *)body);
    pos = 0;
    while (pos < len)
    {
        ULONG start;
        ULONG urlStart;
        ULONG urlEnd;
        struct RtbRun *run;
        UBYTE ch;

        start = pos;
        urlStart = len;
        while (pos < len)
        {
            if (pos + 7 <= len &&
                body[pos] == 'h' && body[pos + 1] == 't' &&
                body[pos + 2] == 't' && body[pos + 3] == 'p' &&
                ((body[pos + 4] == ':' && body[pos + 5] == '/' &&
                  body[pos + 6] == '/') ||
                 (body[pos + 4] == 's' && body[pos + 5] == ':' &&
                  body[pos + 6] == '/' && pos + 8 <= len &&
                  body[pos + 7] == '/')))
            {
                urlStart = pos;
                break;
            }
            pos++;
        }
        if (urlStart > start)
        {
            UBYTE tmp;
            ULONG chunk;

            chunk = urlStart - start;
            tmp = body[urlStart];
            body[urlStart] = '\0';
            run = AllocRtbRunTags(RTBR_TEXT,
                RTBA_Text, (ULONG)(body + start),
                RTBA_FgPen, (ULONG)gui->sg_PenFg,
                RTBA_FontName, (ULONG)"Helvetica",
                RTBA_Size, 12,
                TAG_DONE);
            body[urlStart] = tmp;
            if (run == NULL || !RtbBlockAddRun(para, run))
            {
                if (run != NULL)
                    FreeRtbRun(run);
                return FALSE;
            }
            (void)chunk;
        }
        if (urlStart >= len)
            break;

        urlEnd = urlStart;
        while (urlEnd < len)
        {
            ch = (UBYTE)body[urlEnd];
            if (ch == (UBYTE)' ' || ch == (UBYTE)'\t' ||
                ch == (UBYTE)'\n' || ch == (UBYTE)'\r' ||
                ch == (UBYTE)'<' || ch == (UBYTE)'>' ||
                ch == (UBYTE)'"' || ch == (UBYTE)')' ||
                ch == (UBYTE)']')
                break;
            urlEnd++;
        }
        {
            UBYTE save;
            UBYTE hrefBuf[256];
            ULONG ulen;
            ULONG i;

            ulen = urlEnd - urlStart;
            if (ulen >= sizeof(hrefBuf))
                ulen = sizeof(hrefBuf) - 1;
            for (i = 0; i < ulen; i++)
                hrefBuf[i] = body[urlStart + i];
            hrefBuf[ulen] = '\0';

            save = body[urlEnd];
            body[urlEnd] = '\0';
            run = AllocRtbRunTags(RTBR_LINK,
                RTBA_Text, (ULONG)(body + urlStart),
                RTBA_Href, (ULONG)hrefBuf,
                RTBA_FgPen, (ULONG)gui->sg_PenFill,
                RTBA_FontName, (ULONG)"Helvetica",
                RTBA_Size, 12,
                RTBA_Style, RTBS_UNDERLINE,
                TAG_DONE);
            body[urlEnd] = save;
            if (run == NULL || !RtbBlockAddRun(para, run))
            {
                if (run != NULL)
                    FreeRtbRun(run);
                return FALSE;
            }
        }
        pos = urlEnd;
    }
    return TRUE;
}

BOOL
SeitenRtbAppendCard(struct SeitenGui *gui, struct RtbBlock *parent,
    STRPTR title, STRPTR description, STRPTR site, STRPTR href,
    Object *thumbObj, WORD bg, RTB_BlockID *outId)
{
    struct RtbBlock *card;
    RTB_BlockID id;

    if (outId != NULL)
        *outId = 0;
    if (gui == NULL || parent == NULL)
        return FALSE;
    if (title == NULL && href == NULL)
        return FALSE;

    id = rtb_next_id(gui);
    card = AllocRtbBlockTags(RTBB_EMBED,
        RTBA_ID, id,
        RTBA_BgPen, (ULONG)bg,
        RTBA_Width, 72,
        RTBA_Height, 72,
        RTBA_Title, title != NULL ? (ULONG)title : (ULONG)"",
        RTBA_Description, description != NULL ? (ULONG)description : (ULONG)"",
        RTBA_Site, site != NULL ? (ULONG)site : (ULONG)"",
        RTBA_Href, href != NULL ? (ULONG)href : (ULONG)"",
        RTBA_BitMapObject, (ULONG)thumbObj,
        TAG_DONE);
    if (card == NULL)
        return FALSE;
    if (!RtbBlockAddChild(parent, card))
    {
        FreeRtbBlock(card);
        return FALSE;
    }
    if (outId != NULL)
        *outId = id;
    return TRUE;
}

BOOL
SeitenRtbAppendPost(struct SeitenGui *gui, struct AtpFeedPost *post,
    ULONG index)
{
    struct SeitenRow *row;
    struct RtbBlock *hbox;
    struct RtbBlock *vbox;
    struct RtbBlock *imgBlk;
    struct RtbBlock *para;
    struct RtbBlock *ctl;
    struct RtbBlock *rule;
    struct RtbBlock *grid;
    struct RtbRun *run;
    UBYTE handle[96];
    UBYTE body[SEITEN_LABEL_MAX];
    STRPTR author;
    STRPTR text;
    STRPTR avatarUrl;
    WORD bg;
    ULONG n;
    ULONG imgCount;
    ULONG mi;
    RTB_BlockID imgId;
    RTB_BlockID rowId;

    if (gui == NULL || gui->sg_Rtb == NULL || post == NULL)
        return FALSE;

    author = AtpFeedPostGetAuthorHandle(post);
    text = AtpFeedPostGetText(post);
    if (author == NULL)
        author = (STRPTR)"?";
    if (text == NULL)
        text = (STRPTR)"";

    Utf8ToAmigaDisplay(body, sizeof(body), text);

    n = 0;
    handle[0] = '\0';
    if (n + 1 < sizeof(handle))
    {
        handle[n++] = '@';
        handle[n] = '\0';
    }
    while (*author != '\0' && n + 1 < sizeof(handle))
    {
        handle[n++] = *author++;
        handle[n] = '\0';
    }

    /* Avatar URL only — post images go on IMAGEGRID below. */
    avatarUrl = AtpFeedPostGetAvatarUrl(post);
    row = SeitenRowNew(avatarUrl, AtpFeedPostGetUri(post),
        AtpFeedPostGetAuthorHandle(post));
    if (row == NULL)
        return FALSE;

    bg = ((index & 1) == 0) ? gui->sg_PenBgEven : gui->sg_PenBgOdd;
    row->sr_BgPen = bg;

    imgCount = AtpFeedPostGetImageCount(post);
    if (imgCount > SEITEN_MEDIA_MAX)
        imgCount = SEITEN_MEDIA_MAX;
    for (mi = 0; mi < imgCount; mi++)
    {
        STRPTR url;
        ULONG len;
        STRPTR d;

        url = AtpFeedPostGetImageUrl(post, mi);
        if (url == NULL)
            continue;
        len = strlen((char *)url) + 1;
        d = (STRPTR)AllocVec(len, MEMF_ANY);
        if (d == NULL)
            break;
        CopyMem((APTR)url, (APTR)d, len);
        row->sr_Media[row->sr_MediaCount].sm_Url = d;
        row->sr_MediaCount++;
    }

    {
        ULONG hn;
        ULONG bn;
        ULONG i;

        hn = strlen((char *)handle);
        bn = strlen((char *)body);
        row->sr_Label = (STRPTR)AllocVec(hn + bn + 2, MEMF_ANY);
        if (row->sr_Label == NULL)
        {
            SeitenRowFree(row);
            return FALSE;
        }
        for (i = 0; i < hn; i++)
            row->sr_Label[i] = handle[i];
        row->sr_Label[hn] = '\n';
        for (i = 0; i < bn; i++)
            row->sr_Label[hn + 1 + i] = body[i];
        row->sr_Label[hn + 1 + bn] = '\0';
    }

    imgId = rtb_next_id(gui);
    rowId = rtb_next_id(gui);
    if (imgId == 0 || rowId == 0)
    {
        SeitenRowFree(row);
        return FALSE;
    }

    hbox = AllocRtbBlockTags(RTBB_HBOX,
        RTBA_ID, rowId,
        RTBA_User, (ULONG)row,
        RTBA_BgPen, (ULONG)bg,
        TAG_DONE);
    vbox = AllocRtbBlockTags(RTBB_VBOX,
        RTBA_BgPen, (ULONG)bg,
        TAG_DONE);
    imgBlk = AllocRtbBlockTags(RTBB_IMAGE,
        RTBA_ID, imgId,
        RTBA_User, (ULONG)row,
        RTBA_Width, SEITEN_THUMB_W,
        RTBA_Height, SEITEN_THUMB_H,
        RTBA_BgPen, (ULONG)bg,
        RTBA_Alt, (ULONG)"avatar",
        TAG_DONE);
    para = AllocRtbBlockTags(RTBB_PARAGRAPH,
        RTBA_User, (ULONG)row,
        RTBA_BgPen, (ULONG)bg,
        TAG_DONE);
    ctl = AllocRtbBlockTags(RTBB_CONTROLROW,
        RTBA_User, (ULONG)row,
        RTBA_BgPen, (ULONG)bg,
        TAG_DONE);
    rule = AllocRtbBlockTags(RTBB_RULE,
        RTBA_User, (ULONG)row,
        TAG_DONE);
    grid = NULL;

    if (hbox == NULL || vbox == NULL || imgBlk == NULL || para == NULL)
    {
        if (hbox != NULL)
            FreeRtbBlock(hbox);
        if (vbox != NULL)
            FreeRtbBlock(vbox);
        if (imgBlk != NULL)
            FreeRtbBlock(imgBlk);
        if (para != NULL)
            FreeRtbBlock(para);
        if (ctl != NULL)
            FreeRtbBlock(ctl);
        if (rule != NULL)
            FreeRtbBlock(rule);
        SeitenRowFree(row);
        return FALSE;
    }

    /* Handle + hard break in one run so line spacing uses DejaVu metrics. */
    {
        ULONG hn;

        hn = strlen((char *)handle);
        if (hn + 1 < sizeof(handle))
        {
            handle[hn] = '\n';
            handle[hn + 1] = '\0';
        }
    }
    run = AllocRtbRunTags(RTBR_TEXT,
        RTBA_Text, (ULONG)handle,
        RTBA_FgPen, (ULONG)gui->sg_PenFg,
        RTBA_FontName, (ULONG)"DejaVu Sans",
        RTBA_Size, 15,
        RTBA_Style, RTBS_BOLD,
        TAG_DONE);
    if (run == NULL || !RtbBlockAddRun(para, run))
    {
        if (run != NULL)
            FreeRtbRun(run);
        FreeRtbBlock(para);
        FreeRtbBlock(imgBlk);
        FreeRtbBlock(vbox);
        FreeRtbBlock(hbox);
        if (ctl != NULL)
            FreeRtbBlock(ctl);
        if (rule != NULL)
            FreeRtbBlock(rule);
        SeitenRowFree(row);
        return FALSE;
    }

    /* Body — Helvetica; http(s) spans become RTBR_LINK (FILLPEN). */
    if (body[0] != '\0')
    {
        if (!rtb_append_body_runs(gui, para, body))
        {
            FreeRtbBlock(para);
            FreeRtbBlock(imgBlk);
            FreeRtbBlock(vbox);
            FreeRtbBlock(hbox);
            if (ctl != NULL)
                FreeRtbBlock(ctl);
            if (rule != NULL)
                FreeRtbBlock(rule);
            SeitenRowFree(row);
            return FALSE;
        }
    }

    if (row->sr_MediaCount > 0)
    {
        UWORD mi2;

        grid = AllocRtbBlockTags(RTBB_IMAGEGRID,
            RTBA_BgPen, (ULONG)bg,
            RTBA_Gap, 6,
            RTBA_MaxWidth, SEITEN_MEDIA_MAX_H,
            TAG_DONE);
        if (grid != NULL)
        {
            for (mi2 = 0; mi2 < row->sr_MediaCount; mi2++)
            {
                struct RtbBlock *cell;
                RTB_BlockID mid;

                mid = rtb_next_id(gui);
                row->sr_Media[mi2].sm_BlockId = mid;
                cell = AllocRtbBlockTags(RTBB_IMAGE,
                    RTBA_ID, mid,
                    RTBA_User, (ULONG)row,
                    RTBA_Width, SEITEN_MEDIA_W,
                    RTBA_Height, SEITEN_MEDIA_H,
                    RTBA_BgPen, (ULONG)bg,
                    RTBA_Alt, (ULONG)"media",
                    TAG_DONE);
                if (cell != NULL)
                    RtbBlockAddChild(grid, cell);
            }
        }
    }

    /* OpenGraph card built after grid so VBOX child order stays correct. */
    {
        STRPTR extUri;
        STRPTR extTitle;
        STRPTR extDesc;
        STRPTR extThumb;
        UBYTE site[64];
        UBYTE titleDisp[160];
        UBYTE descDisp[240];
        BOOL haveCard;

        extUri = AtpFeedPostGetExtUri(post);
        extTitle = AtpFeedPostGetExtTitle(post);
        extDesc = AtpFeedPostGetExtDescription(post);
        extThumb = AtpFeedPostGetExtThumb(post);
        haveCard = FALSE;
        titleDisp[0] = '\0';
        descDisp[0] = '\0';
        site[0] = '\0';
        if (extUri != NULL || extTitle != NULL)
        {
            if (extTitle != NULL)
                Utf8ToAmigaDisplay(titleDisp, sizeof(titleDisp), extTitle);
            if (extDesc != NULL)
                Utf8ToAmigaDisplay(descDisp, sizeof(descDisp), extDesc);
            rtb_site_from_uri(extUri, site, sizeof(site));
            haveCard = TRUE;
        }

    if (ctl != NULL)
    {
        /* Reply / Like / Repost — borderless tb.lib BitMaps when available. */
        rtb_add_icon(ctl, row, gui->sg_FeedImgReply, gui->sg_FeedImgReplySel,
            (CONST_STRPTR)"Reply");
        rtb_add_icon(ctl, row, gui->sg_FeedImgLike, gui->sg_FeedImgLikeSel,
            (CONST_STRPTR)"Like");
        rtb_add_icon(ctl, row, gui->sg_FeedImgRepost, gui->sg_FeedImgRepostSel,
            (CONST_STRPTR)"Repost");
    }

    RtbBlockAddChild(vbox, para);
    if (grid != NULL)
        RtbBlockAddChild(vbox, grid);
    if (haveCard)
    {
        RTB_BlockID cardId;

        cardId = 0;
        row->sr_ExtTitle = rtb_strdup(
            titleDisp[0] != '\0' ? titleDisp : (STRPTR)"Link");
        if (descDisp[0] != '\0')
            row->sr_ExtDesc = rtb_strdup(descDisp);
        if (site[0] != '\0')
            row->sr_ExtSite = rtb_strdup(site);
        if (extUri != NULL)
            row->sr_ExtHref = rtb_strdup(extUri);

        if (SeitenRtbAppendCard(gui, vbox,
            row->sr_ExtTitle,
            row->sr_ExtDesc,
            row->sr_ExtSite,
            row->sr_ExtHref, NULL, bg, &cardId) &&
            extThumb != NULL && cardId != 0 &&
            row->sr_MediaCount < SEITEN_MEDIA_MAX)
        {
            ULONG tlen;
            STRPTR d;

            tlen = strlen((char *)extThumb) + 1;
            d = (STRPTR)AllocVec(tlen, MEMF_ANY);
            if (d != NULL)
            {
                CopyMem((APTR)extThumb, (APTR)d, tlen);
                row->sr_Media[row->sr_MediaCount].sm_Url = d;
                row->sr_Media[row->sr_MediaCount].sm_BlockId = cardId;
                row->sr_Media[row->sr_MediaCount].sm_IsEmbed = TRUE;
                row->sr_MediaCount++;
            }
        }
    }
    if (ctl != NULL)
        RtbBlockAddChild(vbox, ctl);
    {
        struct RtbBlock *sp;

        sp = AllocRtbBlockTags(RTBB_SPACER, RTBA_Pixels, 10, TAG_DONE);
        if (sp != NULL)
            RtbBlockAddChild(vbox, sp);
    }
    if (rule != NULL)
        RtbBlockAddChild(vbox, rule);
    RtbBlockAddChild(hbox, imgBlk);
    RtbBlockAddChild(hbox, vbox);

    row->sr_ImageBlockId = imgId;
    row->sr_PostBlockId = rowId;
    AddTail((struct List *)&gui->sg_Posts, (struct Node *)&row->sr_Node);
    gui->sg_NodeCount++;

    DoMethod(gui->sg_Rtb, RTBM_INSERTBLOCK, NULL, hbox, 0);
    return TRUE;
    }
}

BOOL
SeitenRtbUpdateRowImage(struct SeitenGui *gui, struct SeitenRow *row)
{
    struct RtbBlock *blk;
    WORD w;
    WORD h;

    if (gui == NULL || gui->sg_Rtb == NULL || row == NULL)
        return FALSE;
    if (row->sr_BitMap == NULL || row->sr_ImageBlockId == 0)
        return FALSE;

    w = (WORD)GetBitMapAttr(row->sr_BitMap, BMA_WIDTH);
    h = (WORD)GetBitMapAttr(row->sr_BitMap, BMA_HEIGHT);
    if (w < 1)
        w = SEITEN_THUMB_W;
    if (h < 1)
        h = SEITEN_THUMB_H;

    blk = AllocRtbBlockTags(RTBB_IMAGE,
        RTBA_ID, row->sr_ImageBlockId,
        RTBA_User, (ULONG)row,
        RTBA_BitMap, (ULONG)row->sr_BitMap,
        RTBA_Width, w,
        RTBA_Height, h,
        RTBA_BgPen, (ULONG)row->sr_BgPen,
        TAG_DONE);
    if (blk == NULL)
        return FALSE;

    DoMethod(gui->sg_Rtb, RTBM_UPDATEBLOCK, NULL, blk);
    return TRUE;
}

BOOL
SeitenRtbUpdateMediaImage(struct SeitenGui *gui, struct SeitenRow *row,
    UWORD index)
{
    struct RtbBlock *blk;
    struct SeitenMedia *m;
    WORD w;
    WORD h;

    if (gui == NULL || gui->sg_Rtb == NULL || row == NULL)
        return FALSE;
    if (index >= row->sr_MediaCount || index >= SEITEN_MEDIA_MAX)
        return FALSE;
    m = &row->sr_Media[index];
    if (m->sm_BitMap == NULL || m->sm_BlockId == 0)
        return FALSE;

    w = (WORD)GetBitMapAttr(m->sm_BitMap, BMA_WIDTH);
    h = (WORD)GetBitMapAttr(m->sm_BitMap, BMA_HEIGHT);
    if (w < 1)
        w = SEITEN_MEDIA_W;
    if (h < 1)
        h = SEITEN_MEDIA_H;

    if (m->sm_IsEmbed)
    {
        blk = AllocRtbBlockTags(RTBB_EMBED,
            RTBA_ID, m->sm_BlockId,
            RTBA_User, (ULONG)row,
            RTBA_BitMap, (ULONG)m->sm_BitMap,
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
            RTBA_BitMap, (ULONG)m->sm_BitMap,
            RTBA_Width, w,
            RTBA_Height, h,
            RTBA_BgPen, (ULONG)row->sr_BgPen,
            TAG_DONE);
    }
    if (blk == NULL)
        return FALSE;

    DoMethod(gui->sg_Rtb, RTBM_UPDATEBLOCK, NULL, blk);
    return TRUE;
}
