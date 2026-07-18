/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_main.c - Seiten main window (Aeronaut-inspired layout)
 *
 *   +------+------------------------------------+
 *   | Home | [Post Reply Star Repost Refresh] st|
 *   | Ment |------------------------------------|
 *   | Feeds|     richtextbrowser + scroller     |
 *   | Srch |                                    |
 *   | Prof |------------------------------------|
 *   | Login|  compose string                    |
 *   +------+------------------------------------+
 *
 * Left vertical speedbars are the first (leftmost) pane; the horizontal
 * action speedbar lives in the second pane above the timeline.
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
extern struct Library *ButtonBase;
extern struct Library *StringBase;
extern struct Library *SpeedBarBase;
extern struct Library *LabelBase;
extern struct Library *ScrollerBase;
extern struct Library *RichTextBrowserBase;


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

static void
gui_init_hints(struct SeitenGui *gui)
{
    /*
     * OS 3.2 ReAction tooltips (boemann): WINDOW_HintInfo may be empty;
     * real tip strings go on each gadget via GA_GadgetHelpText.
     * Do not use SBNA_Help / SPEEDBAR_Window — that only rewrites the title bar.
     */
    gui->sg_Hints[0].hi_GadgetID = -1;
    gui->sg_Hints[0].hi_Code = -1;
    gui->sg_Hints[0].hi_Text = NULL;
    gui->sg_Hints[0].hi_Flags = 0;
}

static BOOL
gui_open_main(struct SeitenGui *gui)
{
    Object *top_row;
    Object *sidebar;
    Object *main_pane;
    Object *timeline_row;
    Class *rtbClass;

    SeitenTimelineInit(gui);
    if (!SeitenSpeedBarsInit(gui)) {
        PutStr("Seiten: speedbar init failed\n");
        return FALSE;
    }

    gui_init_hints(gui);
    strcpy((char *)gui->sg_StatusBuf, "Not logged in");

    if (RichTextBrowserBase == NULL) {
        PutStr("Seiten: richtextbrowser.gadget missing "
            "(install SYS:Classes/Gadgets/richtextbrowser.gadget)\n");
        return FALSE;
    }
    rtbClass = RICHTEXTBROWSER_GetClass();
    if (rtbClass == NULL) {
        PutStr("Seiten: RICHTEXTBROWSER_GetClass failed\n");
        return FALSE;
    }

    gui->sg_Rtb = NewObject(rtbClass, NULL,
        GA_ID, GID_TIMELINE,
        GA_RelVerify, TRUE,
        GA_GadgetHelpText,
            (ULONG)"Timeline - select a post for Reply / Star / Repost",
        RTB_SelectBlocks, TRUE,
        RTB_BlockCap, SEITEN_NODE_CAP * 3,
        RTB_Overscan, 64,
        TAG_DONE);
    if (gui->sg_Rtb == NULL) {
        return FALSE;
    }

    gui->sg_Scroller = ScrollerObject,
        GA_ID, GID_SCROLLER,
        GA_RelVerify, TRUE,
        GA_Immediate, TRUE,
        GA_GadgetHelpText, (ULONG)"Timeline scroll",
        SCROLLER_Orientation, SORIENT_VERT,
        SCROLLER_Arrows, TRUE,
    ScrollerEnd;
    if (gui->sg_Scroller == NULL) {
        return FALSE;
    }

    gui->sg_SbActions = SpeedBarObject,
        GA_ID, GID_SB_ACTIONS,
        GA_RelVerify, TRUE,
        GA_GadgetHelpText, (ULONG)"Post / Reply / Star / Repost / Refresh",
        SPEEDBAR_Orientation, SBORIENT_HORIZ,
        SPEEDBAR_Buttons, (ULONG)&gui->sg_ActButtons,
        SPEEDBAR_BevelStyle, BVS_NONE,
    SpeedBarEnd;

    gui->sg_SbNav = SpeedBarObject,
        GA_ID, GID_SB_NAV,
        GA_RelVerify, TRUE,
        GA_GadgetHelpText, (ULONG)"Home / Mentions / Feeds / Search",
        SPEEDBAR_Orientation, SBORIENT_VERT,
        SPEEDBAR_Buttons, (ULONG)&gui->sg_NavButtons,
        SPEEDBAR_BevelStyle, BVS_NONE,
    SpeedBarEnd;

    gui->sg_SbAcct = SpeedBarObject,
        GA_ID, GID_SB_ACCT,
        GA_RelVerify, TRUE,
        GA_GadgetHelpText, (ULONG)"Profile / Login",
        SPEEDBAR_Orientation, SBORIENT_VERT,
        SPEEDBAR_Buttons, (ULONG)&gui->sg_AcctButtons,
        SPEEDBAR_BevelStyle, BVS_NONE,
    SpeedBarEnd;

    if (gui->sg_SbActions == NULL || gui->sg_SbNav == NULL ||
        gui->sg_SbAcct == NULL) {
        return FALSE;
    }

    /* Leftmost pane: vertical nav + account speedbars for full window height */
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

    top_row = HLayoutObject,
        LAYOUT_AddChild, gui->sg_SbActions,
        CHILD_WeightedWidth, 0,
        CHILD_MinWidth, 160,
        LAYOUT_AddChild, gui->sg_Status = ButtonObject,
            GA_Text, gui->sg_StatusBuf,
            GA_ID, GID_STATUS,
            GA_ReadOnly, TRUE,
            GA_GadgetHelpText, (ULONG)"Status",
        ButtonEnd,
        CHILD_WeightedWidth, 100,
    LayoutEnd;

    gui->sg_Compose = StringObject,
        GA_ID, GID_COMPOSE,
        GA_RelVerify, TRUE,
        GA_TabCycle, TRUE,
        GA_GadgetHelpText, (ULONG)"Write a post and press Return",
        STRINGA_MaxChars, 300,
        STRINGA_MinVisible, 40,
        STRINGA_TextVal, "",
    StringEnd;

    /* Second pane: action bar + timeline + compose */
    timeline_row = HLayoutObject,
        LAYOUT_AddChild, gui->sg_Rtb,
        CHILD_MinHeight, 120,
        CHILD_WeightedWidth, 100,
        LAYOUT_AddChild, gui->sg_Scroller,
        CHILD_WeightedWidth, 0,
        CHILD_MinWidth, 16,
    LayoutEnd;

    main_pane = VLayoutObject,
        LAYOUT_AddChild, top_row,
        CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, timeline_row,
        CHILD_MinHeight, 120,
        CHILD_WeightedHeight, 100,
        LAYOUT_AddChild, gui->sg_Compose,
        CHILD_WeightedHeight, 0,
    LayoutEnd;

    gui->sg_Layout = HLayoutObject,
        LAYOUT_SpaceOuter, TRUE,
        LAYOUT_DeferLayout, TRUE,
        LAYOUT_AddChild, sidebar,
        CHILD_WeightedWidth, 0,
        CHILD_MinWidth, 40,
        LAYOUT_AddChild, main_pane,
        CHILD_WeightedWidth, 100,
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
        WINDOW_GadgetHelp, TRUE,
        WINDOW_HintInfo, (ULONG)gui->sg_Hints,
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

    SeitenTimelineInitPens(gui);
    return TRUE;
}

static void
gui_close_main(struct SeitenGui *gui)
{
    SeitenTimelineFree(gui);
    SeitenTimelineFreePens(gui);

    /*
     * Detach button lists while window + speedbar.gadget are still open,
     * dispose the window (needs SpeedBarBase), then free nodes / TBImages
     * and only then TBExit().
     */
    SeitenSpeedBarsDetach(gui);

    if (gui->sg_WinObj != NULL) {
        if (gui->sg_Window != NULL) {
            RA_CloseWindow(gui->sg_WinObj);
            gui->sg_Window = NULL;
        }
        DisposeObject(gui->sg_WinObj);
        gui->sg_WinObj = NULL;
        gui->sg_Layout = NULL;
        gui->sg_Rtb = NULL;
        gui->sg_Scroller = NULL;
        gui->sg_Status = NULL;
        gui->sg_Compose = NULL;
        gui->sg_SbActions = NULL;
        gui->sg_SbNav = NULL;
        gui->sg_SbAcct = NULL;
    }

    SeitenSpeedBarsFree(gui);
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
            SeitenImgStop(gui);
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
                /*
                 * Do not CheckScroll/Kick here.  CDN Kick is sync HTTPS;
                 * running it (or RefreshGList) under GM_HANDLEINPUT notify
                 * freezes or deadlocks.  INTUITICK drives the image queue;
                 * post select still arrives via RelVerify for Reply/etc.
                 */
                break;
            case GID_SCROLLER:
                SeitenTimelineScroller(gui);
                break;
            }
            break;
        case WMHI_INTUITICK:
            /* Live prop drag: scroller reports while GA_Immediate is set. */
            if (!gui->sg_ImgStop) {
                SeitenTimelineScroller(gui);
                SeitenTimelineCheckScroll(gui);
            }
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

    /*
     * WindowBase / LayoutBase / … come from reaction.lib auto-open.
     * SpeedBarBase is defined and opened by tb.lib in TBInit() (later),
     * so do not require it here — a NULL SpeedBarBase at this point is normal.
     * RichTextBrowserBase is opened in libbases / SeitenGuiRun below.
     */
    if (WindowBase == NULL || LayoutBase == NULL ||
        ButtonBase == NULL || StringBase == NULL) {
        PutStr("Seiten: ReAction classes missing (need CLASSES: + lib:reaction.lib)\n");
        return RETURN_FAIL;
    }

    if (RichTextBrowserBase == NULL) {
        RichTextBrowserBase = OpenLibrary("gadgets/richtextbrowser.gadget", 0);
    }
    if (RichTextBrowserBase == NULL) {
        RichTextBrowserBase = OpenLibrary("richtextbrowser.gadget", 0);
    }
    if (RichTextBrowserBase == NULL) {
        PutStr("Seiten: open richtextbrowser.gadget failed "
            "(need SYS:Classes/Gadgets/richtextbrowser.gadget)\n");
        return RETURN_FAIL;
    }

    err = SeitenEngineInit(&gui.eng, cafile, service, appview, verbose);
    if (err != 0) {
        return RETURN_FAIL;
    }

    if (!SeitenImgInit(&gui)) {
        PutStr("Seiten: WARN image loader unavailable "
            "(CDN thumbs disabled)\n");
    }

    if (!gui_open_main(&gui)) {
        PutStr("Seiten: cannot open main window\n");
        SeitenSpeedBarsFree(&gui);
        SeitenImgShutdown(&gui);
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
        ULONG waitMask;

        waitMask = signal | SIGBREAKF_CTRL_C;
        got = Wait(waitMask);
        if (got & SIGBREAKF_CTRL_C) {
            SeitenImgStop(&gui);
            running = FALSE;
            break;
        }
        if (got & signal) {
            if (gui_handle(&gui)) {
                running = FALSE;
            }
        }
    }

    SeitenImgStop(&gui);
    if (gui.sg_LoggedIn) {
        AtpLogout(gui.eng.se_Conn);
        gui.sg_LoggedIn = FALSE;
    }
    gui_close_main(&gui);
    SeitenImgShutdown(&gui);
    SeitenEngineShutdown(&gui.eng);
    if (RichTextBrowserBase != NULL) {
        CloseLibrary(RichTextBrowserBase);
        RichTextBrowserBase = NULL;
    }
    return RETURN_OK;
}
