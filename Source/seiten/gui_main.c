/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_main.c - Seiten main window (Aeronaut-inspired layout)
 *
 *   [ Post | Reply | Star | Repost | Refresh ]     status…
 *   +--------+----------------------------------+
 *   | Home   |                                  |
 *   | Ment.  |         timeline listbrowser     |
 *   | Feeds  |                                  |
 *   | Search |                                  |
 *   |--------|----------------------------------|
 *   | Profile|  compose string                  |
 *   | Login  |                                  |
 *   +--------+----------------------------------+
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/amiatp.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"

extern struct Library *WindowBase;
extern struct Library *LayoutBase;
extern struct Library *ListBrowserBase;
extern struct Library *ButtonBase;
extern struct Library *StringBase;
extern struct Library *SpeedBarBase;
extern struct Library *LabelBase;

void
SeitenGuiSetStatus(struct SeitenGui *gui, STRPTR text)
{
    if (gui == NULL) {
        return;
    }
    if (text == NULL) {
        text = (STRPTR)"";
    }
    strncpy((char *)gui->sg_StatusBuf, (char *)text,
        sizeof(gui->sg_StatusBuf) - 1);
    gui->sg_StatusBuf[sizeof(gui->sg_StatusBuf) - 1] = '\0';

    if (gui->sg_Status != NULL) {
        if (gui->sg_Window != NULL) {
            SetGadgetAttrs((struct Gadget *)gui->sg_Status, gui->sg_Window,
                NULL, GA_Text, (ULONG)gui->sg_StatusBuf, TAG_DONE);
            RefreshGList((struct Gadget *)gui->sg_Status, gui->sg_Window,
                NULL, 1);
        } else {
            SetAttrs(gui->sg_Status, GA_Text, (ULONG)gui->sg_StatusBuf,
                TAG_DONE);
        }
    }
}

static BOOL
gui_open_main(struct SeitenGui *gui)
{
    Object *top_row;
    Object *sidebar;
    Object *main_col;
    Object *body;

    SeitenTimelineInit(gui);
    if (!SeitenSpeedBarsInit(gui)) {
        PutStr("Seiten: speedbar init failed\n");
        return FALSE;
    }

    strcpy((char *)gui->sg_StatusBuf, "Not logged in");

    gui->sg_ListBrowser = ListBrowserObject,
        GA_ID, GID_TIMELINE,
        GA_RelVerify, TRUE,
        LISTBROWSER_Labels, (ULONG)&gui->sg_Posts,
        LISTBROWSER_ColumnInfo, (ULONG)gui->sg_Columns,
        LISTBROWSER_ColumnTitles, FALSE,
        LISTBROWSER_ShowSelected, TRUE,
        LISTBROWSER_MultiSelect, FALSE,
        /* AutoFit must stay FALSE or WordWrap will not rewrap (V42). */
        LISTBROWSER_AutoFit, FALSE,
        LISTBROWSER_WrapText, TRUE,
        LISTBROWSER_HorizSeparators, TRUE,
        LISTBROWSER_VerticalProp, TRUE,
    ListBrowserEnd;

    if (gui->sg_ListBrowser == NULL) {
        return FALSE;
    }

    gui->sg_SbActions = SpeedBarObject,
        GA_ID, GID_SB_ACTIONS,
        GA_RelVerify, TRUE,
        SPEEDBAR_Orientation, SBORIENT_HORIZ,
        SPEEDBAR_Buttons, (ULONG)&gui->sg_ActButtons,
        SPEEDBAR_BevelStyle, BVS_NONE,
    SpeedBarEnd;

    gui->sg_SbNav = SpeedBarObject,
        GA_ID, GID_SB_NAV,
        GA_RelVerify, TRUE,
        SPEEDBAR_Orientation, SBORIENT_VERT,
        SPEEDBAR_Buttons, (ULONG)&gui->sg_NavButtons,
        SPEEDBAR_BevelStyle, BVS_NONE,
    SpeedBarEnd;

    gui->sg_SbAcct = SpeedBarObject,
        GA_ID, GID_SB_ACCT,
        GA_RelVerify, TRUE,
        SPEEDBAR_Orientation, SBORIENT_VERT,
        SPEEDBAR_Buttons, (ULONG)&gui->sg_AcctButtons,
        SPEEDBAR_BevelStyle, BVS_NONE,
    SpeedBarEnd;

    if (gui->sg_SbActions == NULL || gui->sg_SbNav == NULL ||
        gui->sg_SbAcct == NULL) {
        return FALSE;
    }

    top_row = HLayoutObject,
        LAYOUT_AddChild, gui->sg_SbActions,
        CHILD_WeightedWidth, 0,
        LAYOUT_AddChild, gui->sg_Status = ButtonObject,
            GA_Text, gui->sg_StatusBuf,
            GA_ID, GID_STATUS,
            GA_ReadOnly, TRUE,
        ButtonEnd,
    LayoutEnd;

    sidebar = VLayoutObject,
        LAYOUT_SpaceOuter, FALSE,
        LAYOUT_AddChild, gui->sg_SbNav,
        CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, gui->sg_SbAcct,
        CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, ButtonObject,
            GA_ReadOnly, TRUE,
            GA_Text, " ",
        ButtonEnd,
        CHILD_WeightedHeight, 100,
    LayoutEnd;

    gui->sg_Compose = StringObject,
        GA_ID, GID_COMPOSE,
        GA_RelVerify, TRUE,
        GA_TabCycle, TRUE,
        STRINGA_MaxChars, 300,
        STRINGA_MinVisible, 40,
        STRINGA_TextVal, "",
    StringEnd;

    main_col = VLayoutObject,
        LAYOUT_AddChild, gui->sg_ListBrowser,
        CHILD_MinHeight, 120,
        CHILD_WeightedHeight, 100,
        LAYOUT_AddChild, gui->sg_Compose,
        CHILD_WeightedHeight, 0,
    LayoutEnd;

    body = HLayoutObject,
        LAYOUT_AddChild, sidebar,
        CHILD_WeightedWidth, 0,
        CHILD_MinWidth, 72,
        LAYOUT_AddChild, main_col,
        CHILD_WeightedWidth, 100,
    LayoutEnd;

    gui->sg_Layout = VLayoutObject,
        LAYOUT_SpaceOuter, TRUE,
        LAYOUT_DeferLayout, TRUE,
        LAYOUT_AddChild, top_row,
        CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, body,
        CHILD_WeightedHeight, 100,
    LayoutEnd;

    if (gui->sg_Layout == NULL) {
        return FALSE;
    }

    gui->sg_WinObj = WindowObject,
        WA_Title, "Seiten",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_SmartRefresh, TRUE,
        WA_InnerWidth, 560,
        WA_InnerHeight, 400,
        WA_IDCMP, IDCMP_INTUITICKS,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_ParentGroup, gui->sg_Layout,
    EndWindow;

    if (gui->sg_WinObj == NULL) {
        DisposeObject(gui->sg_Layout);
        gui->sg_Layout = NULL;
        return FALSE;
    }

    gui->sg_Window = (struct Window *)RA_OpenWindow(gui->sg_WinObj);
    if (gui->sg_Window == NULL) {
        DisposeObject(gui->sg_WinObj);
        gui->sg_WinObj = NULL;
        gui->sg_Layout = NULL;
        return FALSE;
    }

    /* Title-bar help while holding speedbar buttons */
    SetGadgetAttrs((struct Gadget *)gui->sg_SbActions, gui->sg_Window, NULL,
        SPEEDBAR_Window, (ULONG)gui->sg_Window, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)gui->sg_SbNav, gui->sg_Window, NULL,
        SPEEDBAR_Window, (ULONG)gui->sg_Window, TAG_DONE);
    SetGadgetAttrs((struct Gadget *)gui->sg_SbAcct, gui->sg_Window, NULL,
        SPEEDBAR_Window, (ULONG)gui->sg_Window, TAG_DONE);

    SeitenTimelineInitPens(gui);
    return TRUE;
}

static void
gui_close_main(struct SeitenGui *gui)
{
    SeitenTimelineFree(gui);
    SeitenTimelineFreePens(gui);
    SeitenSpeedBarsFree(gui);
    if (gui->sg_WinObj != NULL) {
        if (gui->sg_Window != NULL) {
            RA_CloseWindow(gui->sg_WinObj);
            gui->sg_Window = NULL;
        }
        DisposeObject(gui->sg_WinObj);
        gui->sg_WinObj = NULL;
        gui->sg_Layout = NULL;
        gui->sg_ListBrowser = NULL;
        gui->sg_Status = NULL;
        gui->sg_Compose = NULL;
        gui->sg_SbActions = NULL;
        gui->sg_SbNav = NULL;
        gui->sg_SbAcct = NULL;
    }
}

static BOOL
gui_handle(struct SeitenGui *gui)
{
    ULONG result;
    UWORD code;
    BOOL quit;

    quit = FALSE;
    code = 0;
    while ((result = RA_HandleInput(gui->sg_WinObj, &code)) != WMHI_LASTMSG) {
        switch (result & WMHI_CLASSMASK) {
        case WMHI_CLOSEWINDOW:
            quit = TRUE;
            break;
        case WMHI_GADGETUP:
            switch (result & WMHI_GADGETMASK) {
            case GID_SB_ACTIONS:
            case GID_SB_NAV:
            case GID_SB_ACCT:
                SeitenSpeedBarsHandle(gui, result & WMHI_GADGETMASK, code);
                break;
            case GID_COMPOSE:
                SeitenComposePost(gui);
                break;
            case GID_TIMELINE:
                SeitenTimelineCheckScroll(gui);
                break;
            }
            break;
        case WMHI_INTUITICK:
            SeitenTimelineCheckScroll(gui);
            break;
        }
    }
    return quit;
}

LONG
SeitenGuiRun(STRPTR cafile, STRPTR handle, STRPTR password,
    STRPTR service, STRPTR appview, BOOL verbose)
{
    struct SeitenGui gui;
    LONG err;
    ULONG signal;
    BOOL running;

    memset(&gui, 0, sizeof(gui));

    if (WindowBase == NULL || LayoutBase == NULL || ListBrowserBase == NULL ||
        ButtonBase == NULL || StringBase == NULL || SpeedBarBase == NULL) {
        PutStr("Seiten: ReAction classes missing (link lib:reaction.lib)\n");
        return RETURN_FAIL;
    }

    err = SeitenEngineInit(&gui.eng, cafile, service, appview, verbose);
    if (err != 0) {
        return RETURN_FAIL;
    }

    if (!gui_open_main(&gui)) {
        PutStr("Seiten: cannot open main window\n");
        SeitenSpeedBarsFree(&gui);
        SeitenEngineShutdown(&gui.eng);
        return RETURN_FAIL;
    }

    if (handle != NULL && password != NULL) {
        SeitenGuiSetStatus(&gui, (STRPTR)"Logging in...");
        SetAttrs(gui.sg_WinObj, WA_BusyPointer, TRUE, TAG_DONE);
        err = AtpLogin(gui.eng.se_Conn, handle, password);
        SetAttrs(gui.sg_WinObj, WA_BusyPointer, FALSE, TAG_DONE);
        if (err == 0) {
            gui.sg_LoggedIn = TRUE;
            SeitenTimelineReload(&gui);
        } else {
            SeitenGuiSetStatus(&gui, (STRPTR)"Auto-login failed");
        }
    } else {
        SeitenGuiSetStatus(&gui, (STRPTR)"Login to load timeline");
    }

    GetAttr(WINDOW_SigMask, gui.sg_WinObj, &signal);
    running = TRUE;
    while (running) {
        ULONG got;

        got = Wait(signal | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C) {
            running = FALSE;
            break;
        }
        if (got & signal) {
            if (gui_handle(&gui)) {
                running = FALSE;
            }
        }
    }

    if (gui.sg_LoggedIn) {
        AtpLogout(gui.eng.se_Conn);
        gui.sg_LoggedIn = FALSE;
    }
    gui_close_main(&gui);
    SeitenEngineShutdown(&gui.eng);
    return RETURN_OK;
}
