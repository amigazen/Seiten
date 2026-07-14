/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 */

#include <exec/types.h>

static const char version_tag[] = "\0$VER: Seiten 0.2 (13.7.2026)";

/* Keep $VER visible to Version command / Search. */
const char *Seiten_GetVersion(void)
{
    return version_tag + 1;
}
