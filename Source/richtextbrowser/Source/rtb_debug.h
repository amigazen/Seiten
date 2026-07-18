/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtb_debug.h - conditional trace for richtextbrowser.gadget
 *
 * Enable with DEFINE=RTB_DEBUG in SCOPTIONS.
 * Included from classbase.h AFTER the DOSBase macros — do not include
 * proto/dos.h here (it would expand DOSBase into an invalid declaration).
 */

#ifndef RTB_DEBUG_H
#define RTB_DEBUG_H

#ifdef RTB_DEBUG

static void rtbDbgFlush(struct ClassBase *cb)
{
    BPTR out;

    (void)cb;
    if (DOSBase) {
        out = Output();
        if (out)
            Flush(out);
    }
}

static void rtbDbgPut(struct ClassBase *cb, STRPTR text)
{
    (void)cb;
    if (DOSBase && text) {
        PutStr((STRPTR)"[rtb] ");
        PutStr(text);
        rtbDbgFlush(cb);
    }
}

#define RTB_LOG(cb, msg) rtbDbgPut((cb), (STRPTR)(msg))

#else

#define rtbDbgPut(cb, text) ((void)0)
#define RTB_LOG(cb, msg)    ((void)0)

#endif

#endif /* RTB_DEBUG_H */
