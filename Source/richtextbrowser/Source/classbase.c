/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * classbase.c - library lifecycle for richtextbrowser.gadget
 */

#include "classbase.h"

#ifdef __SASC
ASM int _XCEXIT (void)
{
    return 0;
}
void __regargs __chkabort (void)
{
}
void __regargs _CXBRK (void)
{
}
#endif

struct Library *
ASM LibInit (REG(d0) struct ClassBase *cb,
             REG(a0) BPTR seglist,
             REG(a6) struct Library *sysbase)
{
    cb->cb_SegList = seglist;
    cb->cb_SysBase = sysbase;

    InitSemaphore(&(cb->cb_Lock));

    if (sysbase->lib_Version >= 39)
    {
        cb->cb_DOSBase = OpenLibrary("dos.library", 39);
        cb->cb_IntuitionBase = OpenLibrary("intuition.library", 39);
        cb->cb_GfxBase = OpenLibrary("graphics.library", 39);
        cb->cb_UtilityBase = OpenLibrary("utility.library", 39);
        cb->cb_LayersBase = OpenLibrary("layers.library", 37);
        cb->cb_DataTypesBase = OpenLibrary("datatypes.library", 37);
        cb->cb_DiskfontBase = OpenLibrary("diskfont.library", 0);
        cb->cb_BulletBase = OpenLibrary("bullet.library", 0);
        cb->cb_TTEngineBase = OpenLibrary("ttengine.library", 0);
        cb->cb_BevelBase = NULL;

        if (cb->cb_DOSBase && cb->cb_IntuitionBase &&
            cb->cb_GfxBase && cb->cb_UtilityBase && cb->cb_LayersBase)
        {
            return (struct Library *)cb;
        }
    }

    if (cb->cb_BevelBase) {
        CloseLibrary(cb->cb_BevelBase);
        cb->cb_BevelBase = NULL;
    }
    if (cb->cb_TTEngineBase) {
        CloseLibrary(cb->cb_TTEngineBase);
        cb->cb_TTEngineBase = NULL;
    }
    if (cb->cb_BulletBase) {
        CloseLibrary(cb->cb_BulletBase);
        cb->cb_BulletBase = NULL;
    }
    if (cb->cb_DiskfontBase) {
        CloseLibrary(cb->cb_DiskfontBase);
        cb->cb_DiskfontBase = NULL;
    }
    if (cb->cb_DataTypesBase) {
        CloseLibrary(cb->cb_DataTypesBase);
        cb->cb_DataTypesBase = NULL;
    }
    if (cb->cb_LayersBase) {
        CloseLibrary(cb->cb_LayersBase);
        cb->cb_LayersBase = NULL;
    }
    if (cb->cb_UtilityBase) {
        CloseLibrary(cb->cb_UtilityBase);
        cb->cb_UtilityBase = NULL;
    }
    if (cb->cb_GfxBase) {
        CloseLibrary(cb->cb_GfxBase);
        cb->cb_GfxBase = NULL;
    }
    if (cb->cb_IntuitionBase) {
        CloseLibrary(cb->cb_IntuitionBase);
        cb->cb_IntuitionBase = NULL;
    }
    if (cb->cb_DOSBase) {
        CloseLibrary(cb->cb_DOSBase);
        cb->cb_DOSBase = NULL;
    }
    return NULL;
}

LONG
ASM LibOpen (REG(a6) struct ClassBase *cb)
{
    struct ExecBase *eb;
    LONG retval;
    BOOL success;
    BYTE nest;

    eb = (struct ExecBase *)cb->cb_SysBase;
    retval = (LONG)cb;
    success = TRUE;
    nest = 0;

    ObtainSemaphore(&(cb->cb_Lock));
    nest = eb->TDNestCnt;
    Permit();

    cb->cb_UsageCnt++;
    cb->cb_Lib.lib_Flags &= ~LIBF_DELEXP;

    if (cb->cb_UsageCnt == 1 && cb->cb_Class == NULL)
    {
        if ((cb->cb_Class = initClass(cb)) == NULL)
            success = FALSE;
    }

    if (!success)
    {
        cb->cb_UsageCnt--;
        retval = NULL;
    }

    eb->TDNestCnt = nest;
    ReleaseSemaphore(&(cb->cb_Lock));

    return retval;
}

LONG
ASM LibClose (REG(a6) struct ClassBase *cb)
{
    struct ExecBase *eb;
    LONG retval;
    BYTE nest;

    eb = (struct ExecBase *)cb->cb_SysBase;
    retval = NULL;
    nest = 0;

    ObtainSemaphore(&(cb->cb_Lock));
    nest = eb->TDNestCnt;
    Permit();

    if (cb->cb_UsageCnt)
        cb->cb_UsageCnt--;

    if ((cb->cb_UsageCnt == 0) && cb->cb_Class)
    {
        RemoveClass(cb->cb_Class);
        if (FreeClass(cb->cb_Class))
            cb->cb_Class = NULL;
        else
        {
            AddClass(cb->cb_Class);
            cb->cb_Lib.lib_Flags |= LIBF_DELEXP;
        }
    }

    if (cb->cb_Lib.lib_Flags & LIBF_DELEXP)
        retval = LibExpunge(cb);

    eb->TDNestCnt = nest;
    ReleaseSemaphore(&(cb->cb_Lock));

    return retval;
}

LONG
ASM LibExpunge (REG(a6) struct ClassBase *cb)
{
    BPTR seg;

    seg = cb->cb_SegList;

    if (cb->cb_UsageCnt)
    {
        cb->cb_Lib.lib_Flags |= LIBF_DELEXP;
        return NULL;
    }

    if (cb->cb_Class)
    {
        RemoveClass(cb->cb_Class);
        if (!FreeClass(cb->cb_Class))
        {
            AddClass(cb->cb_Class);
            cb->cb_Lib.lib_Flags |= LIBF_DELEXP;
            return NULL;
        }
        cb->cb_Class = NULL;
    }

    Remove((struct Node *)cb);

    if (cb->cb_BevelBase)
        CloseLibrary(cb->cb_BevelBase);
    if (cb->cb_TTEngineBase)
        CloseLibrary(cb->cb_TTEngineBase);
    if (cb->cb_BulletBase)
        CloseLibrary(cb->cb_BulletBase);
    if (cb->cb_DiskfontBase)
        CloseLibrary(cb->cb_DiskfontBase);
    if (cb->cb_DataTypesBase)
        CloseLibrary(cb->cb_DataTypesBase);
    if (cb->cb_LayersBase)
        CloseLibrary(cb->cb_LayersBase);
    CloseLibrary(cb->cb_UtilityBase);
    CloseLibrary(cb->cb_GfxBase);
    CloseLibrary(cb->cb_IntuitionBase);
    CloseLibrary(cb->cb_DOSBase);

    FreeMem((APTR)((ULONG)cb - (ULONG)cb->cb_Lib.lib_NegSize),
        cb->cb_Lib.lib_NegSize + cb->cb_Lib.lib_PosSize);

    return (LONG)seg;
}
