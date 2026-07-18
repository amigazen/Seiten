/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtb_profile.h - AtpFeedPost → RtbDocument helpers for Seiten
 */

#ifndef SEITEN_RTB_PROFILE_H
#define SEITEN_RTB_PROFILE_H

#include <exec/types.h>
#include <gadgets/richtextbrowser.h>

struct SeitenGui;
struct AtpFeedPost;
struct SeitenRow;

/* Map one feed post into HBOX(avatar | VBOX(text, IMAGEGRID?, actions)). */
BOOL SeitenRtbAppendPost(struct SeitenGui *gui, struct AtpFeedPost *post,
    ULONG index);

/* Update avatar IMAGE after CDN load. */
BOOL SeitenRtbUpdateRowImage(struct SeitenGui *gui, struct SeitenRow *row);

/* Update one IMAGEGRID cell after CDN load. */
BOOL SeitenRtbUpdateMediaImage(struct SeitenGui *gui, struct SeitenRow *row,
    UWORD index);

/*
 * Ready helper for OpenGraph / external embed cards (amiatp fields TBD).
 * Inserts RTBB_EMBED under parent VBOX when title or href is non-NULL.
 */
BOOL SeitenRtbAppendCard(struct SeitenGui *gui, struct RtbBlock *parent,
    STRPTR title, STRPTR description, STRPTR site, STRPTR href,
    Object *thumbObj, WORD bg, RTB_BlockID *outId);

#endif /* SEITEN_RTB_PROFILE_H */
