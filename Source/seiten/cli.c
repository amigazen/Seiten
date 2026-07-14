/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * cli.c - Seiten ReadArgs dispatch (CLI or GUI)
 *
 * Default (no command flags) opens the ReAction GUI.
 * Protocol regression tests: use AtpTest in amiatp SDK/Examples.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/dos.h>
#include <string.h>

#include "engine.h"
#include "gui.h"

LONG SeitenAppRun(void);

#define TEMPLATE \
    "GUI/S,LOGIN/S,LOGOUT/S,WHOAMI/S,TIMELINE/S,PROFILE/S,POST/S,RESOLVE/S,VERBOSE/S," \
    "HANDLE/K,PASSWORD/K,TEXT/K,CAFILE/K,SERVICE/K,APPVIEW/K,LIMIT/N"

struct CliArgs
{
    LONG   GUI;
    LONG   LOGIN;
    LONG   LOGOUT;
    LONG   WHOAMI;
    LONG   TIMELINE;
    LONG   PROFILE;
    LONG   POST;
    LONG   RESOLVE;
    LONG   VERBOSE;
    STRPTR HANDLE;
    STRPTR PASSWORD;
    STRPTR TEXT;
    STRPTR CAFILE;
    STRPTR SERVICE;
    STRPTR APPVIEW;
    LONG  *LIMIT;
};

static void
print_usage(void)
{
    PutStr(
        "Seiten - Bluesky / AT Protocol client (amiatp.library)\n\n"
        "  Seiten                         — ReAction GUI\n"
        "  Seiten GUI [HANDLE=] [PASSWORD=] [CAFILE=] [VERBOSE]\n"
        "  Seiten LOGIN HANDLE=<h> PASSWORD=<app-password> [CAFILE=] [VERBOSE]\n"
        "  Seiten LOGIN ... TIMELINE [LIMIT=n]\n"
        "  Seiten LOGIN ... POST TEXT=\"hello from Amiga\"\n"
        "  Seiten PROFILE HANDLE=<h>\n"
        "  Seiten RESOLVE HANDLE=<h>\n"
        "  Seiten WHOAMI / LOGOUT\n\n"
        "Protocol smoke tests: AtpTest (amiatp SDK/Examples).\n");
}

LONG
SeitenAppRun(void)
{
    struct RDArgs *rdargs;
    struct CliArgs args;
    struct SeitenEngine eng;
    LONG err;
    ULONG limit;
    LONG did_cmd;
    LONG want_gui;

    memset(&args, 0, sizeof(args));
    rdargs = ReadArgs(TEMPLATE, (LONG *)&args, NULL);
    if (rdargs == NULL) {
        print_usage();
        return RETURN_ERROR;
    }

    did_cmd = 0;
    if (args.LOGIN || args.LOGOUT || args.WHOAMI || args.TIMELINE ||
        args.PROFILE || args.POST || args.RESOLVE) {
        did_cmd = 1;
    }

    want_gui = args.GUI || !did_cmd;
    if (want_gui) {
        err = SeitenGuiRun(args.CAFILE, args.HANDLE, args.PASSWORD,
            args.SERVICE, args.APPVIEW, args.VERBOSE ? TRUE : FALSE);
        FreeArgs(rdargs);
        return err;
    }

    err = SeitenEngineInit(&eng, args.CAFILE, args.SERVICE, args.APPVIEW,
        args.VERBOSE ? TRUE : FALSE);
    if (err != 0) {
        FreeArgs(rdargs);
        return RETURN_FAIL;
    }

    limit = 20;
    if (args.LIMIT != NULL && *args.LIMIT > 0) {
        limit = (ULONG)*args.LIMIT;
    }

    err = 0;

    if (args.LOGIN) {
        err = SeitenLogin(&eng, args.HANDLE, args.PASSWORD);
        if (err != 0) {
            SeitenEngineShutdown(&eng);
            FreeArgs(rdargs);
            return RETURN_ERROR;
        }
    }

    if (args.WHOAMI) {
        err = SeitenWhoAmI(&eng);
    }

    if (args.TIMELINE) {
        err = SeitenTimeline(&eng, limit);
    }

    if (args.PROFILE) {
        err = SeitenProfile(&eng, args.HANDLE);
    }

    if (args.POST) {
        err = SeitenPost(&eng, args.TEXT);
    }

    if (args.RESOLVE) {
        err = SeitenResolve(&eng, args.HANDLE);
    }

    if (args.LOGOUT) {
        err = SeitenLogout(&eng);
    }

    if (args.LOGIN && !args.LOGOUT) {
        SeitenLogout(&eng);
    }

    SeitenEngineShutdown(&eng);
    FreeArgs(rdargs);
    return (err == 0) ? RETURN_OK : RETURN_ERROR;
}
