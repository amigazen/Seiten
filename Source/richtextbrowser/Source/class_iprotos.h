/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 */

#ifndef RTB_CLASS_IPROTOS_H
#define RTB_CLASS_IPROTOS_H

struct Library *ASM LibInit    (REG(d0) struct ClassBase *cb,
                                REG(a0) BPTR seglist,
                                REG(a6) struct Library *sysbase);
LONG ASM            LibOpen    (REG(a6) struct ClassBase *cb);
LONG ASM            LibClose   (REG(a6) struct ClassBase *cb);
LONG ASM            LibExpunge (REG(a6) struct ClassBase *cb);

Class *initClass (struct ClassBase *cb);

ULONG getRtbAttr  (struct ClassBase *cb, Class *cl, Object *o,
                   struct opGet *msg);
ULONG setRtbAttrs (struct ClassBase *cb, Class *cl, Object *o,
                   struct opSet *msg);

ULONG rtbLayout   (struct ClassBase *cb, Class *cl, Object *o,
                   struct gpLayout *msg);
ULONG rtbDomain   (struct ClassBase *cb, Class *cl, Object *o,
                   struct gpDomain *msg);
ULONG rtbRender   (struct ClassBase *cb, Class *cl, Object *o,
                   struct gpRender *msg);
ULONG rtbHitTest  (struct ClassBase *cb, Class *cl, Object *o,
                   struct gpHitTest *msg);
ULONG rtbGoActive (struct ClassBase *cb, Class *cl, Object *o,
                   struct gpInput *msg);
ULONG rtbHandleInput(struct ClassBase *cb, Class *cl, Object *o,
                   struct gpInput *msg);
ULONG rtbGoInactive(struct ClassBase *cb, Class *cl, Object *o,
                   struct gpGoInactive *msg);

ULONG rtbMethodClear(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbGeneral *msg);
ULONG rtbMethodSetDocument(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbSetDocument *msg);
ULONG rtbMethodInsertBlock(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbInsertBlock *msg);
ULONG rtbMethodRemoveBlock(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbRemoveBlock *msg);
ULONG rtbMethodUpdateBlock(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbUpdateBlock *msg);
ULONG rtbMethodInvalidate(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbInvalidate *msg);
ULONG rtbMethodScrollTo(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbScrollTo *msg);
ULONG rtbMethodHitTest(struct ClassBase *cb, Class *cl, Object *o,
                   struct rtbHitTest *msg);

void  rtbOpenDefaultFont(struct ClassBase *cb, struct localData *ld);
struct TextFont *rtbFontForSize(struct ClassBase *cb, struct localData *ld,
                               UWORD size);
struct TextFont *rtbResolveFont(struct ClassBase *cb, struct localData *ld,
                                CONST_STRPTR name, UWORD size);
void  rtbEnsureSysImages(struct ClassBase *cb, struct localData *ld,
                         struct DrawInfo *dri);
void  rtbDisposeSysImages(struct ClassBase *cb, struct localData *ld);
void  rtbEnsureDoc(struct ClassBase *cb, struct localData *ld);
void  rtbClearDocContents(struct ClassBase *cb, struct localData *ld);
void  rtbFreeOwnedDoc(struct ClassBase *cb, struct localData *ld);
struct RtbBlock *rtbFindBlock(struct localData *ld, RTB_BlockID id);
struct RtbRun *rtbFindRun(struct localData *ld, RTB_RunID id);
void  rtbSelectBlockId(struct localData *ld, RTB_BlockID id);
void  rtbMarkLayoutDirty(struct localData *ld);
/* doNotify: NEVER TRUE when called from GM_RENDER (OM_NOTIFY during paint locks up). */
void  rtbRelayout(struct ClassBase *cb, Class *cl, Object *o,
                  struct GadgetInfo *gi, BOOL doNotify);
void  rtbClampTop(struct localData *ld);
void  rtbNotifyScroll(struct ClassBase *cb, Class *cl, Object *o,
                      struct GadgetInfo *gi);

void  rtbMeasureDocument(struct ClassBase *cb, struct localData *ld);
/* destLeft/destTop: 0,0 for viewport cache; InnerLeft/InnerTop for direct. */
void  rtbPaintDocument(struct ClassBase *cb, struct localData *ld,
                       struct RastPort *rp, struct Window *win,
                       struct ColorMap *cm, WORD destLeft, WORD destTop);
BOOL  rtbHitAt(struct ClassBase *cb, struct localData *ld,
               WORD gx, WORD gy, struct rtbHitTest *out);

void  rtbFreeCache(struct ClassBase *cb, struct localData *ld);
ULONG rtbRebuildCache(struct ClassBase *cb, struct localData *ld,
                      struct RastPort *friendRP, struct ColorMap *cm);
void  rtbBlitCache(struct ClassBase *cb, struct localData *ld,
                   struct RastPort *dest);
/* TickBox: ObtainGIRPort + GM_RENDER + ReleaseGIRPort */
void  rtbRedrawGIRPort(struct ClassBase *cb, Class *cl, Object *o,
                       struct GadgetInfo *gi);

APTR  poolAlloc(struct ClassBase *cb, struct localData *ld, ULONG size);
APTR  poolAllocClear(struct ClassBase *cb, struct localData *ld, ULONG size);
STRPTR poolStrDup(struct ClassBase *cb, struct localData *ld,
                  CONST_STRPTR src, ULONG len);

void  rtbNotify(struct ClassBase *cb, Class *cl, Object *o,
                struct GadgetInfo *gi, ULONG tag1, ...);

#endif /* RTB_CLASS_IPROTOS_H */
