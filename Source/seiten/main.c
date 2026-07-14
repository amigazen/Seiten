/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * main.c - Seiten entry point
 */

#include <exec/types.h>
#include <dos/dos.h>

extern LONG SeitenAppRun(void);

int
main(void)
{
    return (int)SeitenAppRun();
}
