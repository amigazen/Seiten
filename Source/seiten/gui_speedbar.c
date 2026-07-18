/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_speedbar.c - Aeronaut-style action + sidebar speedbars
 *
 * Prefer AISS icons via tb.lib when TBIMAGES: is present; otherwise use
 * LabelObject text faces.  BitmapImages from TBImage stay owned by the
 * TBImage cache until DisposeTBImage after FreeSpeedButtonNode.
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
#include <libraries/tb.h>
#include <libraries/tb_symbols.h>

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
sb_add_button_icon(struct List *list, TBImage **img_slot, ULONG id,
    STRPTR icon_name, STRPTR help, BOOL mx, WORD spacing)
{
    TBImage *img;
    struct Node *node;
    struct TagItem tags[5];
    ULONG ti;

    (void)help;

    img = NewTBImage(icon_name, NULL, PRECISION_ICON, NULL);
    if (img == NULL) {
        Printf("Seiten: AISS icon '%s' failed (TBErr=%ld)\n",
            icon_name, TBErr());
        return FALSE;
    }

    ti = 0;
    tags[ti].ti_Tag = SBNA_Spacing;
    tags[ti].ti_Data = (ULONG)spacing;
    ti++;
    tags[ti].ti_Tag = SBNA_Highlight;
    tags[ti].ti_Data = SBH_RECESS;
    ti++;
    if (mx) {
        tags[ti].ti_Tag = SBNA_MXGroup;
        tags[ti].ti_Data = 1;
        ti++;
        tags[ti].ti_Tag = SBNA_Selected;
        tags[ti].ti_Data = (id == SBN_HOME) ? TRUE : FALSE;
        ti++;
    }
    tags[ti].ti_Tag = TAG_DONE;
    tags[ti].ti_Data = 0;

    /*
     * BitmapImages stay owned by img.  Keep img alive until after
     * FreeSpeedButtonNode — DisposeTBImage would wipe faces still on the node.
     *
     * Always stamp ln_Pri after create: SAS/C register ABI has historically
     * mangled the mid-list buttonID argument into NewSpeedButtonNodeFromTBImage.
     */
    node = NewSpeedButtonNodeFromTBImage(img, TBA_Size24, id, NULL, NULL, tags);
    if (node == NULL) {
        Printf("Seiten: speedbutton '%s' id=%ld failed (TBErr=%ld)\n",
            icon_name, id, TBErr());
        DisposeTBImage(img);
        return FALSE;
    }
    node->ln_Pri = (BYTE)id;
    *img_slot = img;
    AddTail(list, node);
    return TRUE;
}

static BOOL
sb_add_button(struct List *list, Object **lab_slot, TBImage **img_slot,
    ULONG id, STRPTR text, STRPTR icon_name, STRPTR help, BOOL mx,
    WORD spacing, BOOL use_icons)
{
    Object *lab;
    struct Node *node;

    (void)help;

    *lab_slot = NULL;
    *img_slot = NULL;

    if (use_icons) {
        if (sb_add_button_icon(list, img_slot, id, icon_name, help, mx, spacing)) {
            return TRUE;
        }
        Printf("Seiten: falling back to text for '%s'\n", text);
    }

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
            SBNA_MXGroup, 1,
            SBNA_Selected, (id == SBN_HOME) ? TRUE : FALSE,
            TAG_DONE);
    } else {
        node = AllocSpeedButtonNode(id,
            SBNA_Image, (ULONG)lab,
            SBNA_Enabled, TRUE,
            SBNA_Spacing, spacing,
            SBNA_Highlight, SBH_RECESS,
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
sb_detach_buttons(Object *sb, struct Window *win)
{
    if (sb != NULL && win != NULL) {
        SetGadgetAttrs((struct Gadget *)sb, win, NULL,
            SPEEDBAR_Buttons, ~0, TAG_DONE);
    }
}

static void
sb_free_list(struct List *list, Object **labs, TBImage **icons, ULONG count)
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
        if (icons[i] != NULL) {
            DisposeTBImage(icons[i]);
            icons[i] = NULL;
        }
    }
}

BOOL
SeitenSpeedBarsInit(struct SeitenGui *gui)
{
    BOOL use_icons;

    NewList(&gui->sg_ActButtons);
    NewList(&gui->sg_NavButtons);
    NewList(&gui->sg_AcctButtons);
    gui->sg_ActLabelCount = 0;
    gui->sg_NavLabelCount = 0;
    gui->sg_AcctLabelCount = 0;
    gui->sg_TBInited = FALSE;

    TBInit();
    gui->sg_TBInited = TRUE;
    use_icons = (TBImagesExists() == TBERR_NOERROR) ? TRUE : FALSE;
    if (use_icons) {
        PutStr("Seiten: TBIMAGES: found - using AISS icons\n");
    } else {
        Printf("Seiten: TBIMAGES: missing (TBErr=%ld) - text speedbar\n",
            TBErr());
    }

    /* Top action bar — compose / selected-post actions (AISSClassic names) */
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[0],
            &gui->sg_ActIcons[0], SBA_POST, (STRPTR)"Post",
            (STRPTR)TB_SYM_TWEETADD, (STRPTR)"Publish a new post",
            FALSE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 1;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[1],
            &gui->sg_ActIcons[1], SBA_REPLY, (STRPTR)"Reply",
            (STRPTR)TB_SYM_MAIL, (STRPTR)"Reply to selected post",
            FALSE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 2;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[2],
            &gui->sg_ActIcons[2], SBA_STAR, (STRPTR)"Star",
            (STRPTR)TB_SYM_FAVOURITES, (STRPTR)"Like selected post",
            FALSE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 3;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[3],
            &gui->sg_ActIcons[3], SBA_REPOST, (STRPTR)"Repost",
            (STRPTR)TB_SYM_COPY, (STRPTR)"Repost selected",
            FALSE, 4, use_icons)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 4;
    if (!sb_add_button(&gui->sg_ActButtons, &gui->sg_ActLabels[4],
            &gui->sg_ActIcons[4], SBA_REFRESH, (STRPTR)"Refresh",
            (STRPTR)TB_SYM_RELOAD, (STRPTR)"Reload current feed",
            FALSE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_ActLabelCount = 5;

    /* Sidebar nav — MX group so one feed view is selected */
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[0],
            &gui->sg_NavIcons[0], SBN_HOME, (STRPTR)"Home",
            (STRPTR)TB_SYM_HOME, (STRPTR)"Following timeline",
            TRUE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 1;
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[1],
            &gui->sg_NavIcons[1], SBN_MENTIONS, (STRPTR)"Mentions",
            (STRPTR)TB_SYM_FLAGBLUE, (STRPTR)"Mentions (soon)",
            TRUE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 2;
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[2],
            &gui->sg_NavIcons[2], SBN_FEEDS, (STRPTR)"Feeds",
            (STRPTR)TB_SYM_VIEWAS_LIST, (STRPTR)"Custom feeds (soon)",
            TRUE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 3;
    if (!sb_add_button(&gui->sg_NavButtons, &gui->sg_NavLabels[3],
            &gui->sg_NavIcons[3], SBN_SEARCH, (STRPTR)"Search",
            (STRPTR)TB_SYM_SEARCH, (STRPTR)"Search (soon)",
            TRUE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_NavLabelCount = 4;

    /* Account strip under nav */
    if (!sb_add_button(&gui->sg_AcctButtons, &gui->sg_AcctLabels[0],
            &gui->sg_AcctIcons[0], SBC_PROFILE, (STRPTR)"Profile",
            (STRPTR)TB_SYM_EYE, (STRPTR)"Show signed-in profile",
            FALSE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_AcctLabelCount = 1;
    if (!sb_add_button(&gui->sg_AcctButtons, &gui->sg_AcctLabels[1],
            &gui->sg_AcctIcons[1], SBC_LOGIN, (STRPTR)"Login",
            (STRPTR)TB_SYM_LOCKOPEN, (STRPTR)"Log in / switch account",
            FALSE, 2, use_icons)) {
        return FALSE;
    }
    gui->sg_AcctLabelCount = 2;
    return TRUE;
}

void
SeitenSpeedBarsDetach(struct SeitenGui *gui)
{
    if (gui == NULL) {
        return;
    }
    /* Detach while window + speedbar.gadget are still alive */
    sb_detach_buttons(gui->sg_SbActions, gui->sg_Window);
    sb_detach_buttons(gui->sg_SbNav, gui->sg_Window);
    sb_detach_buttons(gui->sg_SbAcct, gui->sg_Window);
}

void
SeitenSpeedBarsFree(struct SeitenGui *gui)
{
    if (gui == NULL) {
        return;
    }

    /*
     * Call after the window object is disposed so speedbar.gadget is gone,
     * but before TBExit — FreeSpeedButtonNode still needs SpeedBarBase.
     */
    sb_free_list(&gui->sg_ActButtons, gui->sg_ActLabels, gui->sg_ActIcons,
        gui->sg_ActLabelCount);
    sb_free_list(&gui->sg_NavButtons, gui->sg_NavLabels, gui->sg_NavIcons,
        gui->sg_NavLabelCount);
    sb_free_list(&gui->sg_AcctButtons, gui->sg_AcctLabels, gui->sg_AcctIcons,
        gui->sg_AcctLabelCount);

    gui->sg_ActLabelCount = 0;
    gui->sg_NavLabelCount = 0;
    gui->sg_AcctLabelCount = 0;

    if (gui->sg_TBInited) {
        TBExit();
        gui->sg_TBInited = FALSE;
    }
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
