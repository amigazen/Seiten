/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * classbase.h - internal header for richtextbrowser.gadget
 */

#ifndef RTB_CLASSBASE_H
#define RTB_CLASSBASE_H

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <exec/types.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <intuition/imageclass.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <graphics/text.h>
#include <graphics/gfxmacros.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <diskfont/diskfont.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/glyph.h>
#include <diskfont/oterrors.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>

#include <clib/macros.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/utility_protos.h>
#include <clib/layers_protos.h>
#include <clib/datatypes_protos.h>
#include <clib/diskfont_protos.h>
#include <clib/bullet_protos.h>

#include <pragmas/exec_pragmas.h>
#include <pragmas/dos_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/graphics_pragmas.h>
#include <pragmas/utility_pragmas.h>
#include <pragmas/layers_pragmas.h>
#include <pragmas/datatypes_pragmas.h>
#include <pragmas/diskfont_pragmas.h>
#include <pragmas/bullet_pragmas.h>

/* amiga.lib linkage (NewList et al.); link with LIB:amiga.lib. */
#include <proto/alib.h>

#include <gadgets/richtextbrowser.h>
#include <libraries/canvasplot.h>

struct ClassBase
{
    struct Library         cb_Lib;
    UWORD                  cb_UsageCnt;
    BPTR                   cb_SegList;

    struct Library        *cb_SysBase;
    struct Library        *cb_DOSBase;
    struct Library        *cb_IntuitionBase;
    struct Library        *cb_GfxBase;
    struct Library        *cb_UtilityBase;
    struct Library        *cb_LayersBase;
    struct Library        *cb_DataTypesBase;
    struct Library        *cb_DiskfontBase;
    struct Library        *cb_BulletBase;
    struct Library        *cb_TTEngineBase;
    struct Library        *cb_BevelBase;

    struct SignalSemaphore cb_Lock;
    Class                 *cb_Class;
};

#define SysBase             cb->cb_SysBase
#define DOSBase             cb->cb_DOSBase
#define IntuitionBase       cb->cb_IntuitionBase
#define GfxBase             cb->cb_GfxBase
#define UtilityBase         cb->cb_UtilityBase
#define LayersBase          cb->cb_LayersBase
#define DataTypesBase       cb->cb_DataTypesBase
#define DiskfontBase        cb->cb_DiskfontBase
#define BulletBase          cb->cb_BulletBase
#define TTEngineBase        cb->cb_TTEngineBase

#define ASM       __asm
#define REG(x)    register __ ## x

#include "rtb_debug.h"
#include "glyphengine.h"
#include "rtb_face.h"
#include "classdata.h"
#include "class_iprotos.h"

#endif /* RTB_CLASSBASE_H */
