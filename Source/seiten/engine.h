/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * engine.h - Seiten engine (wraps AtpConnection)
 */

#ifndef SEITEN_ENGINE_H
#define SEITEN_ENGINE_H

#include <exec/types.h>
#include <libraries/amiatp.h>

struct SeitenEngine
{
    struct AtpConnection *se_Conn;
    STRPTR                se_CAFile;
    STRPTR                se_Service;
    STRPTR                se_AppView;
    BOOL                  se_Open;
    BOOL                  se_Verbose;
};

LONG SeitenEngineInit(struct SeitenEngine *eng, STRPTR cafile,
    STRPTR service, STRPTR appview, BOOL verbose);
void SeitenEngineShutdown(struct SeitenEngine *eng);

LONG SeitenLogin(struct SeitenEngine *eng, STRPTR handle, STRPTR password);
LONG SeitenLogout(struct SeitenEngine *eng);
LONG SeitenWhoAmI(struct SeitenEngine *eng);
LONG SeitenTimeline(struct SeitenEngine *eng, ULONG limit);
LONG SeitenProfile(struct SeitenEngine *eng, STRPTR actor);
LONG SeitenPost(struct SeitenEngine *eng, STRPTR text);
LONG SeitenResolve(struct SeitenEngine *eng, STRPTR handle);

/* GUI / programmatic helpers (no console dump of feed body). */
LONG SeitenFetchTimeline(struct SeitenEngine *eng, ULONG limit, STRPTR cursor,
    struct AtpFeed **out);
LONG SeitenDoPost(struct SeitenEngine *eng, STRPTR text, STRPTR uri_buf,
    ULONG uri_buflen);

#endif /* SEITEN_ENGINE_H */
