/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_compose.c - Compose / reply helpers
 */

#include <exec/types.h>
#include <proto/intuition.h>
#include <proto/string.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"

BOOL
SeitenComposePost(struct SeitenGui *gui)
{
    STRPTR text;
    UBYTE uri[256];
    LONG err;
    UBYTE status[160];

    if (gui == NULL || gui->sg_Compose == NULL) {
        return FALSE;
    }
    if (!gui->sg_LoggedIn) {
        SeitenGuiSetStatus(gui, (STRPTR)"Login first");
        return FALSE;
    }

    text = NULL;
    GetAttr(STRINGA_TextVal, gui->sg_Compose, (ULONG *)&text);
    if (text == NULL || text[0] == '\0') {
        SeitenGuiSetStatus(gui, (STRPTR)"Nothing to post");
        return FALSE;
    }

    SeitenGuiSetStatus(gui, (STRPTR)"Posting...");
    if (gui->sg_WinObj != NULL) {
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
    }

    uri[0] = '\0';
    err = SeitenDoPost(&gui->eng, text, (STRPTR)uri, sizeof(uri));

    if (gui->sg_WinObj != NULL) {
        SetAttrs(gui->sg_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
    }

    if (err != 0) {
        SeitenGuiSetStatus(gui, (STRPTR)"Post failed");
        return FALSE;
    }

    sprintf((char *)status, "Posted %s", uri);
    SeitenGuiSetStatus(gui, status);

    SetGadgetAttrs((struct Gadget *)gui->sg_Compose, gui->sg_Window, NULL,
        STRINGA_TextVal, (ULONG)"",
        TAG_DONE);
    RefreshGList((struct Gadget *)gui->sg_Compose, gui->sg_Window, NULL, 1);

    SeitenTimelineReload(gui);
    return TRUE;
}

BOOL
SeitenComposeReply(struct SeitenGui *gui)
{
    struct SeitenRow *row;
    UBYTE prefix[160];
    STRPTR handle;

    if (gui == NULL || gui->sg_Compose == NULL) {
        return FALSE;
    }
    if (!gui->sg_LoggedIn) {
        SeitenGuiSetStatus(gui, (STRPTR)"Login first");
        return FALSE;
    }

    row = SeitenTimelineGetSelected(gui);
    if (row == NULL) {
        SeitenGuiSetStatus(gui, (STRPTR)"Select a post to reply");
        return FALSE;
    }

    handle = row->sr_AuthorHandle;
    if (handle == NULL || handle[0] == '\0') {
        handle = (STRPTR)"someone";
    }
    sprintf((char *)prefix, "@%s ", handle);

    SetGadgetAttrs((struct Gadget *)gui->sg_Compose, gui->sg_Window, NULL,
        STRINGA_TextVal, (ULONG)prefix,
        TAG_DONE);
    RefreshGList((struct Gadget *)gui->sg_Compose, gui->sg_Window, NULL, 1);
    ActivateGadget((struct Gadget *)gui->sg_Compose, gui->sg_Window, NULL);

    if (row->sr_Uri != NULL) {
        UBYTE st[200];

        sprintf((char *)st, "Reply to %s", row->sr_Uri);
        SeitenGuiSetStatus(gui, st);
    } else {
        SeitenGuiSetStatus(gui, (STRPTR)"Reply — edit compose then Post");
    }
    /*
     * Full Lexicon reply (root/parent refs) needs amiatp createPost
     * reply fields — compose prefix is the interim UX.
     */
    return TRUE;
}
