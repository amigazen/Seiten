/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2026 amigazen project
 *
 * rtbdemo.c - minimal richtextbrowser.gadget demo with prop sync
 *
 * Build on Amiga (after installing the gadget class):
 *   sc rtbdemo.c link LIB LIB:reaction.lib LIB:sc.lib LIB:amiga.lib
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include <proto/alib.h>

#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/scroller.h>
#include <gadgets/richtextbrowser.h>
#include <reaction/reaction.h>
#include <reaction/reaction_macros.h>

#include <stdio.h>
#include <string.h>

struct Library *RichTextBrowserBase;

int
main(void)
{
    Object *winobj;
    Object *layout;
    Object *rtb;
    Object *scroller;
    struct Window *win;
    Class *rtbClass;
    struct RtbBlock *blk;
    ULONG signal;
    BOOL running;
    LONG i;

    RichTextBrowserBase = OpenLibrary("gadgets/richtextbrowser.gadget", 0);
    if (RichTextBrowserBase == NULL)
        RichTextBrowserBase = OpenLibrary("richtextbrowser.gadget", 0);
    if (RichTextBrowserBase == NULL)
    {
        PutStr("rtbdemo: open richtextbrowser.gadget failed "
            "(install to SYS:Classes/Gadgets/)\n");
        return RETURN_FAIL;
    }

    rtbClass = RICHTEXTBROWSER_GetClass();
    if (rtbClass == NULL)
    {
        PutStr("rtbdemo: GetClass failed\n");
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }

    rtb = NewObject(rtbClass, NULL,
        GA_ID, 1,
        GA_RelVerify, TRUE,
        RTB_SelectBlocks, TRUE,
        RTB_BlockCap, 256,
        TAG_DONE);
    if (rtb == NULL)
    {
        PutStr("rtbdemo: NewObject failed\n");
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }

    /* Seed paragraphs */
    SetAttrs(rtb, RTB_Busy, TRUE, TAG_DONE);
    for (i = 0; i < 40; i++)
    {
        UBYTE buf[80];

        sprintf((char *)buf,
            "Paragraph %ld — richtextbrowser virtualized scroll demo.",
            (long)i);
        blk = AllocRtbBlockTags(RTBB_PARAGRAPH,
            RTBA_Text, (ULONG)buf,
            TAG_DONE);
        if (blk != NULL)
        {
            DoMethod(rtb, RTBM_INSERTBLOCK, NULL, blk, 0);
        }
        blk = AllocRtbBlockTags(RTBB_RULE, TAG_DONE);
        if (blk != NULL)
            DoMethod(rtb, RTBM_INSERTBLOCK, NULL, blk, 0);
    }
    SetAttrs(rtb, RTB_Busy, FALSE, TAG_DONE);
    DoMethod(rtb, RTBM_INVALIDATE, NULL, 0, 0);

    scroller = ScrollerObject,
        GA_ID, 2,
        GA_RelVerify, TRUE,
        GA_Immediate, TRUE,
        SCROLLER_Orientation, SORIENT_VERT,
        SCROLLER_Arrows, TRUE,
    ScrollerEnd;

    layout = HLayoutObject,
        LAYOUT_AddChild, rtb,
        CHILD_WeightedWidth, 100,
        LAYOUT_AddChild, scroller,
        CHILD_WeightedWidth, 0,
        CHILD_MinWidth, 16,
    LayoutEnd;

    winobj = WindowObject,
        WA_Title, "richtextbrowser demo",
        WA_DragBar, TRUE,
        WA_CloseGadget, TRUE,
        WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE,
        WA_Activate, TRUE,
        WA_InnerWidth, 400,
        WA_InnerHeight, 300,
        WA_IDCMP, IDCMP_INTUITICKS,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_ParentGroup, layout,
    EndWindow;

    if (winobj == NULL)
    {
        DisposeObject(rtb);
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }

    win = (struct Window *)RA_OpenWindow(winobj);
    if (win == NULL)
    {
        DisposeObject(winobj);
        CloseLibrary(RichTextBrowserBase);
        return RETURN_FAIL;
    }

    /* Enable content compose — class ObtainGIRPort redraws after OM_SET. */
    SetGadgetAttrs((struct Gadget *)rtb, win, NULL, RTB_Top, 0, TAG_DONE);

    /* Sync scroller to document metrics */
    {
        ULONG top;
        ULONG tot;
        ULONG vis;

        GetAttr(RTB_Top, rtb, &top);
        GetAttr(RTB_Total, rtb, &tot);
        GetAttr(RTB_Visible, rtb, &vis);
        SetGadgetAttrs((struct Gadget *)scroller, win, NULL,
            SCROLLER_Top, top,
            SCROLLER_Total, tot > 0 ? tot : 1,
            SCROLLER_Visible, vis > 0 ? vis : 1,
            TAG_DONE);
    }

    GetAttr(WINDOW_SigMask, winobj, &signal);
    running = TRUE;
    while (running)
    {
        ULONG got;
        ULONG result;
        UWORD code;

        got = Wait(signal | SIGBREAKF_CTRL_C);
        if (got & SIGBREAKF_CTRL_C)
            break;

        code = 0;
        while ((result = RA_HandleInput(winobj, &code)) != WMHI_LASTMSG)
        {
            switch (result & WMHI_CLASSMASK)
            {
                case WMHI_CLOSEWINDOW:
                    running = FALSE;
                    break;
                case WMHI_GADGETUP:
                    if ((result & WMHI_GADGETMASK) == 2)
                    {
                        ULONG top;

                        GetAttr(SCROLLER_Top, scroller, &top);
                        SetGadgetAttrs((struct Gadget *)rtb, win, NULL,
                            RTB_Top, top, TAG_DONE);
                    }
                    else if ((result & WMHI_GADGETMASK) == 1)
                    {
                        ULONG tot;
                        ULONG vis;
                        ULONG top;

                        GetAttr(RTB_Top, rtb, &top);
                        GetAttr(RTB_Total, rtb, &tot);
                        GetAttr(RTB_Visible, rtb, &vis);
                        SetGadgetAttrs((struct Gadget *)scroller, win, NULL,
                            SCROLLER_Top, top,
                            SCROLLER_Total, tot,
                            SCROLLER_Visible, vis,
                            TAG_DONE);
                    }
                    break;
                case WMHI_INTUITICK:
                    {
                        ULONG stop;
                        ULONG rtop;

                        stop = 0;
                        rtop = 0;
                        GetAttr(SCROLLER_Top, scroller, &stop);
                        GetAttr(RTB_Top, rtb, &rtop);
                        if (stop != rtop)
                        {
                            SetGadgetAttrs((struct Gadget *)rtb, win, NULL,
                                RTB_Top, stop, TAG_DONE);
                        }
                    }
                    break;
            }
        }
    }

    RA_CloseWindow(winobj);
    DisposeObject(winobj);
    CloseLibrary(RichTextBrowserBase);
    return RETURN_OK;
}
