/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui.h - Seiten ReAction GUI shared state (Aeronaut-style chrome)
 */

#ifndef SEITEN_GUI_H
#define SEITEN_GUI_H

#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>

#include <proto/window.h>
#include <classes/window.h>
#include <proto/layout.h>
#include <gadgets/layout.h>
#include <proto/listbrowser.h>
#include <gadgets/listbrowser.h>
#include <proto/button.h>
#include <gadgets/button.h>
#include <proto/string.h>
#include <gadgets/string.h>
#include <proto/label.h>
#include <images/label.h>
#include <proto/speedbar.h>
#include <gadgets/speedbar.h>
#include <images/bevel.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

#include "engine.h"
#include "imgload.h"

#define SEITEN_NODE_CAP     200
#define SEITEN_PAGE_LIMIT   25
#define SEITEN_LABEL_MAX    512

/* Gadget IDs for window objects */
enum {
    GID_SB_ACTIONS = 1, /* horizontal: Post / Reply / Star / … */
    GID_SB_NAV,         /* vertical sidebar: feeds */
    GID_SB_ACCT,        /* vertical sidebar: account */
    GID_STATUS,
    GID_TIMELINE,
    GID_COMPOSE,
    GID_LG_HANDLE,
    GID_LG_PASSWORD,
    GID_LG_CAFILE,
    GID_LG_OK,
    GID_LG_CANCEL
};

/* Horizontal action speedbar button numbers (ln_Pri) */
enum {
    SBA_POST = 0,
    SBA_REPLY,
    SBA_STAR,
    SBA_REPOST,
    SBA_REFRESH
};

/* Vertical nav speedbar */
enum {
    SBN_HOME = 0,
    SBN_MENTIONS,
    SBN_FEEDS,
    SBN_SEARCH
};

/* Vertical account speedbar */
enum {
    SBC_PROFILE = 0,
    SBC_LOGIN
};

struct SeitenGui
{
    struct SeitenEngine eng;
    Object             *sg_WinObj;
    Object             *sg_Layout;
    Object             *sg_ListBrowser;
    Object             *sg_Status;
    Object             *sg_Compose;
    Object             *sg_SbActions;
    Object             *sg_SbNav;
    Object             *sg_SbAcct;
    struct Window      *sg_Window;
    struct List         sg_Posts;
    struct List         sg_ActButtons;
    struct List         sg_NavButtons;
    struct List         sg_AcctButtons;
    Object             *sg_ActLabels[8];
    Object             *sg_NavLabels[8];
    Object             *sg_AcctLabels[8];
    ULONG               sg_ActLabelCount;
    ULONG               sg_NavLabelCount;
    ULONG               sg_AcctLabelCount;
    struct ColumnInfo   sg_Columns[3];
    STRPTR              sg_Cursor;
    BOOL                sg_Loading;
    BOOL                sg_LoggedIn;
    BOOL                sg_Exhausted;
    ULONG               sg_NodeCount;
    WORD                sg_PenFg;
    WORD                sg_PenBgEven;
    WORD                sg_PenBgOdd;
    BOOL                sg_PenOddOwned;
    struct ColorMap    *sg_ColorMap;
    UBYTE               sg_StatusBuf[128];
};

LONG SeitenGuiRun(STRPTR cafile, STRPTR handle, STRPTR password,
    STRPTR service, STRPTR appview, BOOL verbose);

BOOL SeitenGuiOpenLogin(struct SeitenGui *gui);
void SeitenGuiSetStatus(struct SeitenGui *gui, STRPTR text);

BOOL SeitenTimelineInit(struct SeitenGui *gui);
void SeitenTimelineInitPens(struct SeitenGui *gui);
void SeitenTimelineFreePens(struct SeitenGui *gui);
void SeitenTimelineFree(struct SeitenGui *gui);
BOOL SeitenTimelineReload(struct SeitenGui *gui);
BOOL SeitenTimelineLoadMore(struct SeitenGui *gui);
void SeitenTimelineCheckScroll(struct SeitenGui *gui);
void SeitenTimelineLoadNextImage(struct SeitenGui *gui);
struct SeitenRow *SeitenTimelineGetSelected(struct SeitenGui *gui);

BOOL SeitenComposePost(struct SeitenGui *gui);
BOOL SeitenComposeReply(struct SeitenGui *gui);

BOOL SeitenSpeedBarsInit(struct SeitenGui *gui);
void SeitenSpeedBarsFree(struct SeitenGui *gui);
void SeitenSpeedBarsHandle(struct SeitenGui *gui, ULONG which, UWORD code);

#endif /* SEITEN_GUI_H */
