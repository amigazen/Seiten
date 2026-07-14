/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_speedbar.c - Aeronaut-style action + sidebar speedbars
 *
 * Faces are LabelObject images (text) so we need no custom bitmaps.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/amiatp.h>
#include <proto/speedbar.h>
#include <proto/label.h>
#include <graphics/text.h>
#include <string.h>
#include <stdio.h>

#include "gui.h"

static Object *
sb_make_label(STRPTR text)
{
    return LabelObject,
        LABEL_Text, text,
        LABEL_Justification, LJ_CENTER,
        LABEL_SoftStyle, FSF_BOLD,
    LabelEnd;
}

static BOOL
sb_add_button(struct List *list, Object **lab_slot, UWORD id,
    STRPTR text, STRPTR help, BOOL mx, WORD spacing)
{
    Object *lab;
    struct Node *node;

    lab = sb_make_label(text);
    if (lab == NULL) {
        return FALSE;
    }
    *lab_slot = lab;

    if (mx) {
        node = AllocSpeedButtonNode(id,
            SBNA_Image, (ULONG)lab,
            SBNA_Enabled, TRUE,
            SBNA_Spacing, spacing,
            SBNA_Highlight, SBH_RECESS,
            SBNA_Help, help,
            SBNA_MXGroup, 1,
            SBNA_Selected, (id == SBN_HOME) ? TRUE : FALSE,
            TAG_DONE);
    } else {
        node = AllocSpeedButtonNode(id,
            SBNA_Image, (ULONG)lab,
            SBNA_Enabled, TRUE,
            SBNA_Spacing, spacing,
            SBNA_Highlight, SBH_RECESS,
            SBNA_Help, help,
            TAG_DONE);
    }
    if (node == NULL) {
        DisposeObject(lab);
        *lab_slot = NULL;
        return FALSE;
    }
    AddTail(list, node);
    return TRUE;
}

static void
sb_free_list(struct List *list, Object **labs, ULONG count)
{
    struct Node *n;
    ULONG i;

    while ((n = RemHead(list)) != NULL) {
        FreeSpeedButtonNode(n);
    }
    NewList(list);
    for (i = 0; i < count; i++) {
        if (labs[i] != NULL) {
            DisposeObject(labs[i]);
            labs[i] = NULL;
        }
    }
}

BOOL
SeitenSpeedBarsInit(struct SeitenGui *gui)
{
    NewList(&gui->sg_ActButtons);
    NewList(&gui->sg_NavButtons);
    NewList(&gui->sg_AcctButtons);
    gui->sg_ActLabelCount = 0;
    gui->sg_NavLabelCount = 0;
    gui->sg_AcctLabelCount = 0;

    /* Top action bar — acts on compose / selected post */
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[0],
            SBA_POST, (STRPTR)"Post", (STRPTR)"Publish compose text",
            FALSE, 2)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 1;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[1],
            SBA_REPLY, (STRPTR)"Reply", (STRPTR)"Reply to selected post",
            FALSE, 2)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 2;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[2],
            SBA_STAR, (STRPTR)"Star", (STRPTR)"Like selected post",
            FALSE, 2)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 3;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[3],
            SBA_REPOST, (STRPTR)"Repost", (STRPTR)"Repost selected",
            FALSE, 4)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 4;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[4],
            SBA_REFRESH, (STRPTR)"Refresh", (STRPTR)"Reload current feed",
            FALSE, 2)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 5;

    /* Sidebar nav — MX group so one feed view is selected */
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[0],
            SBN_HOME, (STRPTR)"Home", (STRPTR)"Following timeline",
            TRUE, 2)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 1;
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[1],
            SBN_MENTIONS, (STRPTR)"Mentions", (STRPTR)"Mentions (soon)",
            TRUE, 2)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 2;
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[2],
            SBN_FEEDS, (STRPTR)"Feeds", (STRPTR)"Custom feeds (soon)",
            TRUE, 2)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 3;
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[3],
            SBN_SEARCH, (STRPTR)"Search", (STRPTR)"Search (soon)",
            TRUE, 2)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 4;

    /* Account strip under nav */
    if (!sb_add_button(&gui->sg_AcctButtons, &gui->sg_AcctLabels[0],
            SBC_PROFILE, (STRPTR)"Profile", (STRPTR)"Show signed-in profile",
            FALSE, 2)) {
        return FALSE;
    }
    gui->sg_AcctLabelCount = 1;
    if (!sb_add_button(&gui->sg_AcctButtons, &gui->sg_AcctLabels[1],
            SBC_LOGIN, (STRPTR)"Login", (STRPTR)"Log in / switch account",
            FALSE, 2)) {
        return FALSE;
    }
    gui->sg_AcctLabelCount = 2;
    return TRUE;
}

void
SeitenSpeedBarsFree(struct SeitenGui *gui)
{
    if (gui == NULL) {
        return;
    }
    if (gui->sg_SbActions != NULL && gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_SbActions, gui->sg_Window,
            NULL, SPEEDBAR_Buttons, ~0, TAG_DONE);
    }
    if (gui->sg_SbNav != NULL && gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_SbNav, gui->sg_Window,
            NULL, SPEEDBAR_Buttons, ~0, TAG_DONE);
    }
    if (gui->sg_SbAcct != NULL && gui->sg_Window != NULL) {
        SetGadgetAttrs((struct Gadget *)gui->sg_SbAcct, gui->sg_Window,
            NULL, SPEEDBAR_Buttons, ~0, TAG_DONE);
    }
    sb_free_list(&gui->sg_ActButtons, gui->sg_ActLabels, gui->sg_ActLabelCount);
    sb_free_list(&gui->sg_NavButtons, gui->sg_NavLabels, gui->sg_NavLabelCount);
    sb_free_list(&gui->sg_AcctButtons, gui->sg_AcctLabels, gui->sg_AcctLabelCount);
    gui->sg_ActLabelCount = 0;
    gui->sg_NavLabelCount = 0;
    gui->sg_AcctLabelCount = 0;
}

static void
sb_do_profile(struct SeitenGui *gui)
{
    STRPTR h;
    STRPTR d;
    UBYTE buf[160];

    if (!gui->sg_LoggedIn) {
        SeitenGuiSetStatus(gui, (STRPTR)"Not logged in");
        return;
    }
    h = AtpConnectionGetHandle(gui->eng.se_Conn);
    d = AtpConnectionGetDid(gui->eng.se_Conn);
    sprintf((char *)buf, "@%s", h != NULL ? (char *)h : "?");
    SeitenGuiSetStatus(gui, buf);
    (void)d;
}

void
SeitenSpeedBarsHandle(struct SeitenGui *gui, ULONG which, UWORD code)
{
    if (gui == NULL) {
        return;
    }

    if (which == GID_SB_ACTIONS) {
        switch (code) {
        case SBA_POST:
            SeitenComposePost(gui);
            break;
        case SBA_REPLY:
            SeitenComposeReply(gui);
            break;
        case SBA_STAR:
            if (SeitenTimelineGetSelected(gui) == NULL) {
                SeitenGuiSetStatus(gui, (STRPTR)"Select a post to star");
            } else {
                SeitenGuiSetStatus(gui, (STRPTR)"Star: coming soon");
            }
            break;
        case SBA_REPOST:
            if (SeitenTimelineGetSelected(gui) == NULL) {
                SeitenGuiSetStatus(gui, (STRPTR)"Select a post to repost");
            } else {
                SeitenGuiSetStatus(gui, (STRPTR)"Repost: coming soon");
            }
            break;
        case SBA_REFRESH:
            if (gui->sg_LoggedIn) {
                SeitenTimelineReload(gui);
            } else {
                SeitenGuiSetStatus(gui, (STRPTR)"Login first");
            }
            break;
        }
        return;
    }

    if (which == GID_SB_NAV) {
        switch (code) {
        case SBN_HOME:
            if (gui->sg_LoggedIn) {
                SeitenTimelineReload(gui);
            } else {
                SeitenGuiSetStatus(gui, (STRPTR)"Login to load Home");
            }
            break;
        case SBN_MENTIONS:
            SeitenGuiSetStatus(gui, (STRPTR)"Mentions: coming soon");
            break;
        case SBN_FEEDS:
            SeitenGuiSetStatus(gui, (STRPTR)"Feeds: coming soon");
            break;
        case SBN_SEARCH:
            SeitenGuiSetStatus(gui, (STRPTR)"Search: coming soon");
            break;
        }
        return;
    }

    if (which == GID_SB_ACCT) {
        switch (code) {
        case SBC_PROFILE:
            sb_do_profile(gui);
            break;
        case SBC_LOGIN:
            if (SeitenGuiOpenLogin(gui)) {
                SeitenGuiSetStatus(gui, (STRPTR)"Logged in");
                SeitenTimelineReload(gui);
            }
            break;
        }
    }
}
