/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * engine.c - Seiten engine over amiatp.library
 */

#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/amiatp.h>
#include <string.h>

#include "engine.h"

extern struct Library *AtpBase;

static void
seiten_print_err(struct SeitenEngine *eng, LONG err)
{
    STRPTR s;
    LONG cerr;

    s = AtpGetErrorString(err);
    cerr = 0;
    if (eng != NULL && eng->se_Conn != NULL) {
        cerr = AtpConnectionGetLastError(eng->se_Conn);
    }
    Printf("Seiten: error %ld (%s)", err, s != NULL ? s : (STRPTR)"?");
    if (cerr != 0 && cerr != err) {
        Printf(" conn=%ld", cerr);
    }
    PutStr("\n");
}

static void
seiten_v(struct SeitenEngine *eng, STRPTR msg)
{
    if (eng != NULL && eng->se_Verbose && msg != NULL) {
        PutStr("Seiten: ");
        PutStr(msg);
        PutStr("\n");
    }
}

LONG
SeitenEngineInit(struct SeitenEngine *eng, STRPTR cafile,
    STRPTR service, STRPTR appview, BOOL verbose)
{
    if (eng == NULL) {
        return ERROR_ATP_INVALID_ARG;
    }
    memset(eng, 0, sizeof(*eng));
    eng->se_Verbose = verbose;

    if (AtpBase == NULL) {
        AtpBase = OpenLibrary(AMIATPNAME, AMIATPVERSION);
        if (AtpBase == NULL) {
            PutStr("Seiten: cannot open amiatp.library\n");
            return ERROR_ATP_NO_HTTP;
        }
        eng->se_Open = TRUE;
    }

    eng->se_Conn = NewAtpConnection();
    if (eng->se_Conn == NULL) {
        return ERROR_ATP_OUT_OF_MEMORY;
    }

    eng->se_CAFile = cafile;
    eng->se_Service = service;
    eng->se_AppView = appview;

    SetAtpConnectionAttrs(eng->se_Conn,
        ATPA_USERAGENT, (ULONG)"Seiten/0.1 (amiatp)",
        ATPA_CONNECT_TIMEOUT, 15,
        ATPA_READ_TIMEOUT, 30,
        ATPA_VERBOSE, (ULONG)(verbose ? TRUE : FALSE),
        TAG_DONE);

    if (cafile != NULL) {
        SetAtpConnectionAttrs(eng->se_Conn,
            ATPA_CA_BUNDLE, (ULONG)cafile,
            TAG_DONE);
        if (verbose) {
            Printf("Seiten: CAFILE=%s\n", cafile);
        }
    } else if (verbose) {
        PutStr("Seiten: CAFILE=(none — AmiSSL defaults / AmiTLS needs PEM)\n");
    }
    if (service != NULL) {
        SetAtpConnectionAttrs(eng->se_Conn,
            ATPA_SERVICE, (ULONG)service,
            TAG_DONE);
    }
    if (appview != NULL) {
        SetAtpConnectionAttrs(eng->se_Conn,
            ATPA_APPVIEW, (ULONG)appview,
            TAG_DONE);
    }
    if (verbose) {
        Printf("Seiten: verbose on; service/appview overrides %s/%s\n",
            service != NULL ? service : (STRPTR)"(default)",
            appview != NULL ? appview : (STRPTR)"(default)");
    }
    return 0;
}

void
SeitenEngineShutdown(struct SeitenEngine *eng)
{
    if (eng == NULL) {
        return;
    }
    seiten_v(eng, "shutdown");
    if (eng->se_Conn != NULL) {
        DisposeAtpConnection(eng->se_Conn);
        eng->se_Conn = NULL;
    }
    if (eng->se_Open && AtpBase != NULL) {
        CloseLibrary(AtpBase);
        AtpBase = NULL;
        eng->se_Open = FALSE;
    }
}

LONG
SeitenLogin(struct SeitenEngine *eng, STRPTR handle, STRPTR password)
{
    LONG err;

    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    if (handle == NULL || password == NULL) {
        PutStr("Seiten: LOGIN requires HANDLE and PASSWORD (app password)\n");
        return ERROR_ATP_INVALID_ARG;
    }
    if (eng->se_Verbose) {
        Printf("Seiten: LOGIN handle=%s password_len=%lu\n",
            handle, (unsigned long)strlen((char *)password));
    }
    err = AtpLogin(eng->se_Conn, handle, password);
    if (err != 0) {
        seiten_print_err(eng, err);
        return err;
    }
    Printf("Seiten: logged in as @%s\n  %s\n",
        AtpConnectionGetHandle(eng->se_Conn),
        AtpConnectionGetDid(eng->se_Conn));
    return 0;
}

LONG
SeitenLogout(struct SeitenEngine *eng)
{
    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    seiten_v(eng, "LOGOUT");
    AtpLogout(eng->se_Conn);
    PutStr("Seiten: logged out\n");
    return 0;
}

LONG
SeitenWhoAmI(struct SeitenEngine *eng)
{
    STRPTR h;
    STRPTR d;

    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    seiten_v(eng, "WHOAMI (local session; no network)");
    h = AtpConnectionGetHandle(eng->se_Conn);
    d = AtpConnectionGetDid(eng->se_Conn);
    if (h == NULL || d == NULL) {
        PutStr("Seiten: not logged in\n");
        return ERROR_ATP_NOT_LOGGED_IN;
    }
    Printf("Seiten: @%s\n  %s\n", h, d);
    return 0;
}

LONG
SeitenTimeline(struct SeitenEngine *eng, ULONG limit)
{
    struct AtpFeed *feed;
    LONG err;
    ULONG i;
    ULONG n;

    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    if (limit == 0) {
        limit = 20;
    }
    if (eng->se_Verbose) {
        Printf("Seiten: TIMELINE limit=%lu (via PDS when logged in)\n",
            (unsigned long)limit);
    }
    feed = NULL;
    err = AtpGetTimeline(eng->se_Conn, limit, NULL, &feed);
    if (err != 0) {
        seiten_print_err(eng, err);
        return err;
    }
    n = AtpFeedGetCount(feed);
    Printf("Seiten: timeline (%lu)\n", (unsigned long)n);
    for (i = 0; i < n; i++) {
        struct AtpFeedPost *post;
        STRPTR author;
        STRPTR text;
        STRPTR uri;

        post = AtpFeedGetPost(feed, i);
        if (post == NULL) {
            continue;
        }
        author = AtpFeedPostGetAuthorHandle(post);
        text = AtpFeedPostGetText(post);
        uri = AtpFeedPostGetUri(post);
        Printf("----\n@%s\n%s\n",
            author != NULL ? author : (STRPTR)"?",
            text != NULL ? text : (STRPTR)"");
        if (eng->se_Verbose && uri != NULL) {
            Printf("  uri=%s\n", uri);
        }
    }
    DisposeAtpFeed(feed);
    return 0;
}

LONG
SeitenProfile(struct SeitenEngine *eng, STRPTR actor)
{
    struct AtpProfile *prof;
    LONG err;

    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    if (actor == NULL) {
        actor = AtpConnectionGetHandle(eng->se_Conn);
    }
    if (actor == NULL) {
        PutStr("Seiten: PROFILE needs HANDLE or a login session\n");
        return ERROR_ATP_INVALID_ARG;
    }
    if (eng->se_Verbose) {
        Printf("Seiten: PROFILE actor=%s\n", actor);
    }
    prof = NULL;
    err = AtpGetProfile(eng->se_Conn, actor, &prof);
    if (err != 0) {
        seiten_print_err(eng, err);
        return err;
    }
    Printf("Seiten: profile @%s\n",
        AtpProfileGetHandle(prof) != NULL ?
            AtpProfileGetHandle(prof) : (STRPTR)actor);
    if (AtpProfileGetDisplayName(prof) != NULL) {
        Printf("  %s\n", AtpProfileGetDisplayName(prof));
    }
    if (AtpProfileGetDescription(prof) != NULL) {
        Printf("  %s\n", AtpProfileGetDescription(prof));
    }
    DisposeAtpProfile(prof);
    return 0;
}

LONG
SeitenPost(struct SeitenEngine *eng, STRPTR text)
{
    UBYTE uri[256];
    LONG err;

    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    if (text == NULL || text[0] == '\0') {
        PutStr("Seiten: POST requires TEXT=\"...\" (quoted if spaces)\n");
        return ERROR_ATP_INVALID_ARG;
    }
    if (eng->se_Verbose) {
        Printf("Seiten: POST text_len=%lu text=\"%s\"\n",
            (unsigned long)strlen((char *)text), text);
        if (AtpConnectionGetDid(eng->se_Conn) == NULL) {
            PutStr("Seiten: WARNING — not logged in; createRecord will fail\n");
        }
    }
    uri[0] = '\0';
    err = AtpCreatePost(eng->se_Conn, text, (STRPTR)uri, sizeof(uri));
    if (err != 0) {
        seiten_print_err(eng, err);
        return err;
    }
    if (uri[0] == '\0') {
        PutStr("Seiten: POST returned success but empty at:// URI\n");
        return ERROR_ATP_XRPC;
    }
    Printf("Seiten: posted %s\n", uri);
    return 0;
}

LONG
SeitenFetchTimeline(struct SeitenEngine *eng, ULONG limit, STRPTR cursor,
    struct AtpFeed **out)
{
    LONG err;

    if (eng == NULL || eng->se_Conn == NULL || out == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    *out = NULL;
    if (limit == 0) {
        limit = 20;
    }
    err = AtpGetTimeline(eng->se_Conn, limit, cursor, out);
    if (err != 0) {
        seiten_print_err(eng, err);
    }
    return err;
}

LONG
SeitenDoPost(struct SeitenEngine *eng, STRPTR text, STRPTR uri_buf,
    ULONG uri_buflen)
{
    LONG err;

    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    if (text == NULL || text[0] == '\0') {
        return ERROR_ATP_INVALID_ARG;
    }
    if (uri_buf != NULL && uri_buflen > 0) {
        uri_buf[0] = '\0';
    }
    err = AtpCreatePost(eng->se_Conn, text, uri_buf, uri_buflen);
    if (err != 0) {
        seiten_print_err(eng, err);
        return err;
    }
    if (uri_buf != NULL && uri_buflen > 0 && uri_buf[0] == '\0') {
        return ERROR_ATP_XRPC;
    }
    return 0;
}

LONG
SeitenResolve(struct SeitenEngine *eng, STRPTR handle)
{
    UBYTE did[128];
    LONG err;

    if (eng == NULL || eng->se_Conn == NULL) {
        return ERROR_ATP_INVALID_HANDLE;
    }
    if (handle == NULL) {
        PutStr("Seiten: RESOLVE requires HANDLE\n");
        return ERROR_ATP_INVALID_ARG;
    }
    if (eng->se_Verbose) {
        Printf("Seiten: RESOLVE handle=%s\n", handle);
    }
    err = AtpResolveHandle(eng->se_Conn, handle, (STRPTR)did, sizeof(did));
    if (err != 0) {
        seiten_print_err(eng, err);
        return err;
    }
    Printf("Seiten: %s -> %s\n", handle, did);
    return 0;
}
