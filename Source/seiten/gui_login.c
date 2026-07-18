/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * gui_login.c - Login secondary window (handle + app password + CA path)
 */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/amiatp.h>
#include <string.h>

#include "gui.h"

BOOL
SeitenGuiOpenLogin(struct SeitenGui *gui)
{
    Object *winobj;
    Object *layout;
    Object *g_handle;
    Object *g_password;
    Object *g_cafile;
    struct Window *win;
    ULONG signal;
    BOOL done;
    BOOL ok;
    UBYTE handle[128];
    UBYTE password[128];
    UBYTE cafile[256];
    STRPTR def_ca;

    handle[0] = '\0';
    password[0] = '\0';
    cafile[0] = '\0';
    def_ca = gui->eng.se_CAFile;
    if (def_ca != NULL) {
        strncpy((char *)cafile, (char *)def_ca, sizeof(cafile) - 1);
        cafile[sizeof(cafile) - 1] = '\0';
    } else {
        strcpy((char *)cafile, "PROGDIR:cacert.pem");
    }
    if (AtpConnectionGetHandle(gui->eng.se_Conn) != NULL) {
        strncpy((char *)handle,
            (char *)AtpConnectionGetHandle(gui->eng.se_Conn),
            sizeof(handle) - 1);
        handle[sizeof(handle) - 1] = '\0';
    }

    g_handle = NULL;
    g_password = NULL;
    g_cafile = NULL;

    layout = VLayoutObject,
        LAYOUT_SpaceOuter, TRUE,
        LAYOUT_DeferLayout, TRUE,

        LAYOUT_AddChild, g_handle = StringObject,
            GA_ID, GID_LG_HANDLE,
            GA_RelVerify, TRUE,
            GA_TabCycle, TRUE,
            STRINGA_TextVal, handle,
            STRINGA_MaxChars, 120,
        StringEnd,
        CHILD_Label, LabelObject, LABEL_Text, "_Handle", LabelEnd,

        LAYOUT_AddChild, g_password = StringObject,
            GA_ID, GID_LG_PASSWORD,
            GA_RelVerify, TRUE,
            GA_TabCycle, TRUE,
            STRINGA_TextVal, password,
            STRINGA_MaxChars, 120,
            STRINGA_MinVisible, 20,
        StringEnd,
        CHILD_Label, LabelObject, LABEL_Text, "_App password", LabelEnd,

        LAYOUT_AddChild, g_cafile = StringObject,
            GA_ID, GID_LG_CAFILE,
            GA_RelVerify, TRUE,
            GA_TabCycle, TRUE,
            STRINGA_TextVal, cafile,
            STRINGA_MaxChars, 240,
        StringEnd,
        CHILD_Label, LabelObject, LABEL_Text, "_CA file", LabelEnd,

        LAYOUT_AddChild, HLayoutObject,
            LAYOUT_AddChild, ButtonObject,
                GA_Text, "_Login",
                GA_ID, GID_LG_OK,
                GA_RelVerify, TRUE,
            ButtonEnd,
            LAYOUT_AddChild, ButtonObject,
                GA_Text, "_Cancel",
                GA_ID, GID_LG_CANCEL,
                GA_RelVerify, TRUE,
            ButtonEnd,
        LayoutEnd,
    LayoutEnd;

    if (layout == NULL) {
        return FALSE;
    }

    winobj = WindowObject,
        WA_Title, "Seiten — Login",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_SmartRefresh, TRUE,
        WINDOW_Position, WPOS_CENTERMOUSE,
        WINDOW_ParentGroup, layout,
    EndWindow;

    if (winobj == NULL) {
        DisposeObject(layout);
        return FALSE;
    }

    win = (struct Window *)RA_OpenWindow(winobj);
    if (win == NULL) {
        DisposeObject(winobj);
        return FALSE;
    }

    GetAttr(WINDOW_SigMask, winobj, &signal);
    done = FALSE;
    ok = FALSE;

    while (!done) {
        ULONG result;
        UWORD code;

        Wait(signal | SIGBREAKF_CTRL_C);
        code = 0;
        while ((result = RA_HandleInput(winobj, &code)) != WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
            case WMHI_CLOSEWINDOW:
                done = TRUE;
                break;
            case WMHI_GADGETUP:
                switch (result & WMHI_GADGETMASK) {
                case GID_LG_OK:
                {
                    STRPTR h;
                    STRPTR p;
                    STRPTR c;
                    LONG err;

                    h = NULL;
                    p = NULL;
                    c = NULL;
                    GetAttr(STRINGA_TextVal, g_handle, (ULONG *)&h);
                    GetAttr(STRINGA_TextVal, g_password, (ULONG *)&p);
                    GetAttr(STRINGA_TextVal, g_cafile, (ULONG *)&c);

                    if (h == NULL || h[0] == '\0' || p == NULL ||
                        p[0] == '\0') {
                        SeitenGuiSetStatus(gui,
                            (STRPTR)"Handle and app password required");
                        break;
                    }

                    if (c != NULL && c[0] != '\0') {
                        SetAtpConnectionAttrs(gui->eng.se_Conn,
                            ATPA_CA_BUNDLE, (ULONG)c,
                            TAG_DONE);
                    }

                    SeitenGuiSetStatus(gui, (STRPTR)"Logging in...");
                    SetAttrs(winobj, WA_BusyPointer, TRUE, TAG_DONE);
                    /* Verbose XRPC/TLS lines on the console for this attempt */
                    SetAtpConnectionAttrs(gui->eng.se_Conn,
                        ATPA_VERBOSE, TRUE,
                        TAG_DONE);
                    err = AtpLogin(gui->eng.se_Conn, h, p);
                    SetAtpConnectionAttrs(gui->eng.se_Conn,
                        ATPA_VERBOSE, (ULONG)(gui->eng.se_Verbose ? TRUE : FALSE),
                        TAG_DONE);
                    SetAttrs(winobj, WA_BusyPointer, FALSE, TAG_DONE);

                    if (err != 0) {
                        STRPTR es;
                        UBYTE msg[160];

                        es = AtpGetErrorString(err);
                        Printf("Seiten: login failed err=%ld (%s) handle=%s ca=%s\n",
                            err,
                            es != NULL ? es : (STRPTR)"?",
                            h,
                            c != NULL && c[0] != '\0' ? c : (STRPTR)"(none)");
                        if (err == ERROR_ATP_HTTP) {
                            strcpy((char *)msg,
                                "HTTP/TLS failed - check CA file & network");
                        } else if (err == ERROR_ATP_HTTP_STATUS) {
                            strcpy((char *)msg,
                                "Login rejected by server (HTTP status)");
                        } else if (err == ERROR_ATP_AUTH) {
                            strcpy((char *)msg,
                                "Auth failed - use a Bluesky app password");
                        } else {
                            strncpy((char *)msg,
                                es != NULL ? (char *)es : "Login failed",
                                sizeof(msg) - 1);
                            msg[sizeof(msg) - 1] = '\0';
                        }
                        SeitenGuiSetStatus(gui, msg);
                        gui->sg_LoggedIn = FALSE;
                    } else {
                        gui->sg_LoggedIn = TRUE;
                        ok = TRUE;
                        done = TRUE;
                    }
                    break;
                }
                case GID_LG_CANCEL:
                    done = TRUE;
                    break;
                }
                break;
            }
        }
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            done = TRUE;
        }
    }

    RA_CloseWindow(winobj);
    DisposeObject(winobj);
    return ok;
}
