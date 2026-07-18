/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * cp_ttfont.c - TTEngine bind helpers for plotter-owned RastPorts
 *
 * Never call TT_Text from a listbrowser LBNCA_RenderHook — DoHookClipRects
 * nesting deadlocks. Only CP_BeginTT / CP_DrawText on the gadget plotter RP.
 */

#include "classbase.h"

BOOL
CP_TTAvailable(struct ClassBase *cb)
{
    if (cb != NULL && cb->cb_TTEngineBase != NULL)
        return TRUE;
    return FALSE;
}
