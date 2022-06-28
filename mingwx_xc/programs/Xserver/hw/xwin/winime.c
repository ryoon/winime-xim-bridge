/*
 *Copyright (C) 2004 Kensuke Matsuzaki. All Rights Reserved.
 *
 *Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 *"Software"), to deal in the Software without restriction, including
 *without limitation the rights to use, copy, modify, merge, publish,
 *distribute, sublicense, and/or sell copies of the Software, and to
 *permit persons to whom the Software is furnished to do so, subject to
 *the following conditions:
 *
 *The above copyright notice and this permission notice shall be
 *included in all copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *NONINFRINGEMENT. IN NO EVENT SHALL THE KENSUKE MATSUZAKI BE LIABLE FOR
 *ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *Except as contained in this notice, the name of the Kensuke Matsuzaki
 *shall not be used in advertising or otherwise to promote the sale, use
 *or other dealings in this Software without prior written authorization
 *from the Kensuke Matsuzaki Project.
 *
 * Authors:	Kensuke Matsuzaki <zakki@peppermint.jp>
 */

#include "win.h"

#define NEED_REPLIES
#define NEED_EVENTS
#include "misc.h"
#include "dixstruct.h"
#include "extnsionst.h"
//#include "colormapst.h"
//#include "cursorstr.h"
//#include "scrnintstr.h"
#include "servermd.h"
//#include "swaprep.h"
#define _WINIME_SERVER_
#include "winimestr.h"
#include <imm.h>

#define CYGIME_DEBUG TRUE

static int WinIMEErrorBase;

static DISPATCH_PROC(ProcWinIMEDispatch);
static DISPATCH_PROC(SProcWinIMEDispatch);

static void WinIMEResetProc(ExtensionEntry* extEntry);

static unsigned char WinIMEReqCode = 0;
static int WinIMEEventBase = 0;

static RESTYPE ClientType, EventType; /* resource types for event masks */
static XID eventResource;

/* Currently selected events */
static unsigned int eventMask = 0;

static int WinIMEFreeClient (pointer data, XID id);
static int WinIMEFreeEvents (pointer data, XID id);
static void SNotifyEvent(xWinIMENotifyEvent *from, xWinIMENotifyEvent *to);

typedef struct _WinIMEEvent *WinIMEEventPtr;
typedef struct _WinIMEEvent {
  WinIMEEventPtr	next;
  ClientPtr	client;
  XID		clientResource;
  unsigned int	mask;
} WinIMEEventRec;

static Bool
IsRoot(WindowPtr pWin)
{
  return pWin == WindowTable[(pWin)->drawable.pScreen->myNum];
}

static Bool
IsTopLevel(WindowPtr pWin)
{
  return pWin && (pWin)->parent && IsRoot(pWin->parent);
}

static WindowPtr
GetTopLevelParent(WindowPtr pWindow)
{
  WindowPtr pWin = pWindow;

  if (!pWin || IsRoot(pWin)) return NULL;

  while (pWin && !IsTopLevel(pWin))
    {
      pWin = pWin->parent;
    }
  return pWin;
}

void
winWinIMEExtensionInit ()
{
  ExtensionEntry* extEntry;

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  ClientType = CreateNewResourceType(WinIMEFreeClient);
  EventType = CreateNewResourceType(WinIMEFreeEvents);
  eventResource = FakeClientID(0);

  if (ClientType && EventType &&
      (extEntry = AddExtension(WINIMENAME,
			       WinIMENumberEvents,
			       WinIMENumberErrors,
			       ProcWinIMEDispatch,
			       SProcWinIMEDispatch,
			       WinIMEResetProc,
			       StandardMinorOpcode)))
    {
      WinIMEReqCode = (unsigned char)extEntry->base;
      WinIMEErrorBase = extEntry->errorBase;
      WinIMEEventBase = extEntry->eventBase;
      EventSwapVector[WinIMEEventBase] = (EventSwapPtr) SNotifyEvent;
    }
}

/*ARGSUSED*/
static void
WinIMEResetProc (ExtensionEntry* extEntry)
{
#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif
}

static int
ProcWinIMEQueryVersion(register ClientPtr client)
{
  xWinIMEQueryVersionReply rep;
  register int n;

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  REQUEST_SIZE_MATCH(xWinIMEQueryVersionReq);
  rep.type = X_Reply;
  rep.length = 0;
  rep.sequenceNumber = client->sequence;
  rep.majorVersion = WIN_IME_MAJOR_VERSION;
  rep.minorVersion = WIN_IME_MINOR_VERSION;
  rep.patchVersion = WIN_IME_PATCH_VERSION;
  if (client->swapped)
    {
      swaps(&rep.sequenceNumber, n);
      swapl(&rep.length, n);
    }
  WriteToClient(client, sizeof(xWinIMEQueryVersionReply), (char *)&rep);
  return (client->noClientException);
}


/* events */

static inline void
updateEventMask (WinIMEEventPtr *pHead)
{
  WinIMEEventPtr pCur;

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  eventMask = 0;
  for (pCur = *pHead; pCur != NULL; pCur = pCur->next)
    eventMask |= pCur->mask;
}

/*ARGSUSED*/
static int
WinIMEFreeClient (pointer data, XID id)
{
  WinIMEEventPtr   pEvent;
  WinIMEEventPtr   *pHead, pCur, pPrev;

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  pEvent = (WinIMEEventPtr) data;
  pHead = (WinIMEEventPtr *) LookupIDByType(eventResource, EventType);
  if (pHead)
    {
      pPrev = 0;
      for (pCur = *pHead; pCur && pCur != pEvent; pCur=pCur->next)
	pPrev = pCur;
      if (pCur)
	{
	  if (pPrev)
	    pPrev->next = pEvent->next;
	  else
	    *pHead = pEvent->next;
	}
      updateEventMask (pHead);
    }
  xfree ((pointer) pEvent);

  return 1;
}

/*ARGSUSED*/
static int
WinIMEFreeEvents (pointer data, XID id)
{
  WinIMEEventPtr   *pHead, pCur, pNext;

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  pHead = (WinIMEEventPtr *) data;
  for (pCur = *pHead; pCur; pCur = pNext)
    {
      pNext = pCur->next;
      FreeResource (pCur->clientResource, ClientType);
      xfree ((pointer) pCur);
    }
  xfree ((pointer) pHead);
  eventMask = 0;

  return 1;
}

static int
ProcWinIMESelectInput (register ClientPtr client)
{
  REQUEST(xWinIMESelectInputReq);
  WinIMEEventPtr	pEvent, pNewEvent, *pHead;
  XID			clientResource;

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  REQUEST_SIZE_MATCH (xWinIMESelectInputReq);
  pHead = (WinIMEEventPtr *)SecurityLookupIDByType(client, eventResource,
						   EventType,
						   SecurityWriteAccess);
  if (stuff->mask != 0)
    {
      if (pHead)
	{
	  /* check for existing entry. */
	  for (pEvent = *pHead; pEvent; pEvent = pEvent->next)
	    {
	      if (pEvent->client == client)
		{
		  pEvent->mask = stuff->mask;
		  updateEventMask (pHead);
		  return Success;
		}
	    }
	}

      /* build the entry */
      pNewEvent = (WinIMEEventPtr) xalloc (sizeof (WinIMEEventRec));
      if (!pNewEvent)
	return BadAlloc;
      pNewEvent->next = 0;
      pNewEvent->client = client;
      pNewEvent->mask = stuff->mask;
      /*
       * add a resource that will be deleted when
       * the client goes away
       */
      clientResource = FakeClientID (client->index);
      pNewEvent->clientResource = clientResource;
      if (!AddResource (clientResource, ClientType, (pointer)pNewEvent))
	return BadAlloc;
      /*
       * create a resource to contain a pointer to the list
       * of clients selecting input.  This must be indirect as
       * the list may be arbitrarily rearranged which cannot be
       * done through the resource database.
       */
      if (!pHead)
	{
	  pHead = (WinIMEEventPtr *) xalloc (sizeof (WinIMEEventRec));
	  if (!pHead ||
	      !AddResource (eventResource, EventType, (pointer)pHead))
	    {
	      FreeResource (clientResource, RT_NONE);
	      return BadAlloc;
	    }
	  *pHead = 0;
	}
      pNewEvent->next = *pHead;
      *pHead = pNewEvent;
      updateEventMask (pHead);
    }
  else if (stuff->mask == 0)
    {
      /* delete the interest */
      if (pHead)
	{
	  pNewEvent = 0;
	  for (pEvent = *pHead; pEvent; pEvent = pEvent->next)
	    {
	      if (pEvent->client == client)
		break;
	      pNewEvent = pEvent;
	    }
	  if (pEvent)
	    {
	      FreeResource (pEvent->clientResource, ClientType);
	      if (pNewEvent)
		pNewEvent->next = pEvent->next;
	      else
		*pHead = pEvent->next;
	      xfree (pEvent);
	      updateEventMask (pHead);
	    }
	}
    }
  else
    {
      client->errorValue = stuff->mask;
      return BadValue;
    }
  return Success;
}

static int
ProcWinIMEEnable (register ClientPtr client)
{
  REQUEST(xWinIMEEnableReq);
  WindowPtr pWin;
  winPrivWinPtr	pWinPriv;
  HIMC hIMC;
  REQUEST_SIZE_MATCH(xWinIMEEnableReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }

  //FIXME:
  if (!(pWinPriv = winGetWindowPriv (GetTopLevelParent(pWin))))
    {
      return BadValue;
    }

  hIMC = ImmCreateContext ();
  ImmAssociateContext (pWinPriv->hWnd, hIMC);

  return (client->noClientException);
}

static int
ProcWinIMEDisable (register ClientPtr client)
{
  REQUEST(xWinIMEDisableReq);
  WindowPtr pWin;
  winPrivWinPtr	pWinPriv;
  HIMC hIMC;
  REQUEST_SIZE_MATCH(xWinIMEDisableReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }

  //FIXME:
  if (!(pWinPriv = winGetWindowPriv (GetTopLevelParent(pWin))))
    {
      return BadValue;
    }

  hIMC = ImmAssociateContext (pWinPriv->hWnd, (HIMC) NULL);
  ImmDestroyContext (hIMC);

  return (client->noClientException);
}

static int
ProcWinIMEOpen (register ClientPtr client)
{
  REQUEST(xWinIMEOpenReq);
  WindowPtr pWin;
  winPrivWinPtr	pWinPriv;
  HIMC hIMC;

  REQUEST_SIZE_MATCH(xWinIMEOpenReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }

  //FIXME:
  if (!(pWinPriv = winGetWindowPriv (GetTopLevelParent(pWin))))
    {
      return BadValue;
    }

  hIMC = ImmGetContext (pWinPriv->hWnd);

  ImmSetOpenStatus (hIMC, TRUE);

  return (client->noClientException);
}

static int
ProcWinIMEClose (register ClientPtr client)
{
  REQUEST(xWinIMECloseReq);
  WindowPtr pWin;
  winPrivWinPtr	pWinPriv;
  HIMC hIMC;

  REQUEST_SIZE_MATCH(xWinIMECloseReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }

  //FIXME:
  if (!(pWinPriv = winGetWindowPriv (GetTopLevelParent(pWin))))
    {
      return BadValue;
    }

  hIMC = ImmGetContext (pWinPriv->hWnd);

  ImmSetOpenStatus (hIMC, FALSE);

  return (client->noClientException);
}

static int
ProcWinIMESetCompositionPoint (register ClientPtr client)
{
  REQUEST(xWinIMESetCompositionPointReq);
  WindowPtr pWin;
  winPrivWinPtr	pWinPriv;
  REQUEST_SIZE_MATCH(xWinIMESetCompositionPointReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }

  //FIXME:
  pWinPriv = winGetWindowPriv (GetTopLevelParent(pWin));

  if (pWinPriv == NULL)
    {
      return BadValue;
    }

  pWinPriv->dwCompositionStyle = CFS_POINT;
  pWinPriv->ptCompositionPos.x = stuff->ix;
  pWinPriv->ptCompositionPos.y = stuff->iy;

  return (client->noClientException);
}

static int
ProcWinIMESetCompositionRect (register ClientPtr client)
{
  REQUEST(xWinIMESetCompositionRectReq);
  WindowPtr pWin;
  winPrivWinPtr	pWinPriv;
  REQUEST_SIZE_MATCH(xWinIMESetCompositionRectReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }

  //FIXME: DO I USE TOP LEVEL WINDOW?
  pWinPriv = winGetWindowPriv (GetTopLevelParent(pWin));

  if (pWinPriv == NULL)
    {
      return BadValue;
    }

  pWinPriv->dwCompositionStyle = CFS_RECT;
  pWinPriv->rcCompositionArea.left = stuff->ix;
  pWinPriv->rcCompositionArea.top = stuff->iy;
  pWinPriv->rcCompositionArea.right = stuff->ix + stuff->iw;
  pWinPriv->rcCompositionArea.bottom = stuff->iy + stuff->ih;

  return (client->noClientException);
}

static int
ProcWinIMEGetCompositionString (register ClientPtr client)
{
  REQUEST(xWinIMEGetCompositionStringReq);
  int len;
  xWinIMEGetCompositionStringReply rep;
  WindowPtr pWin;
  winPrivWinPtr	pWinPriv;

#if CYGIME_DEBUG
  winDebug ("%s %d(%d) %d\n", __FUNCTION__,
	    sizeof(xWinIMEGetCompositionStringReq) >> 2,
	    sizeof(xWinIMEGetCompositionStringReq),
	    client->req_len);
#endif

  REQUEST_SIZE_MATCH(xWinIMEGetCompositionStringReq);

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }

  //FIXME: Need to create window for each x window?
  pWinPriv = winGetWindowPriv (GetTopLevelParent(pWin));

  if (pWinPriv == NULL)
    {
      return BadValue;
    }

  if (pWinPriv->pszCompositionResult)
    {
      len = strlen(pWinPriv->pszCompositionResult);
      rep.type = X_Reply;
      rep.length = (len + 3) >> 2;
      rep.sequenceNumber = client->sequence;
      rep.strLength = len;
      WriteReplyToClient(client, sizeof(xWinIMEGetCompositionStringReply), &rep);
      (void)WriteToClient(client, len, pWinPriv->pszCompositionResult);
    }
  else
    {
      return BadValue;
    }

  return (client->noClientException);
}
/*
 * deliver the event
 */

void
winWinIMESendEvent (int type, unsigned int mask, int kind, int arg, Window window)
{
  WinIMEEventPtr	*pHead, pEvent;
  ClientPtr		client;
  xWinIMENotifyEvent se;
#if CYGIME_DEBUG
  ErrorF ("%s %d %d %d %d\n",
	  __FUNCTION__, type, mask, kind, arg);
#endif
  pHead = (WinIMEEventPtr *) LookupIDByType(eventResource, EventType);
  if (!pHead)
    return;
  for (pEvent = *pHead; pEvent; pEvent = pEvent->next)
    {
      client = pEvent->client;
#if CYGIME_DEBUG
      ErrorF ("winWindowsWMSendEvent - x%08x\n", (int) client);
#endif
      if ((pEvent->mask & mask) == 0
	  || client == serverClient || client->clientGone)
	{
	  continue;
	}
#if CYGIME_DEBUG 
      ErrorF ("winWindowsWMSendEvent - send\n");
#endif
      se.type = type + WinIMEEventBase;
      se.kind = kind;
      se.window = window;
      se.arg = arg;
      se.sequenceNumber = client->sequence;
      se.time = currentTime.milliseconds;
      WriteEventsToClient (client, 1, (xEvent *) &se);
    }
}

#if 0

/* Safe to call from any thread. */
unsigned int
WindowsWMSelectedEvents (void)
{
#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif
  return eventMask;
}


/* frame functions */

static int
ProcWindowsWMFrameGetRect (register ClientPtr client)
{
  xWindowsWMFrameGetRectReply rep;
  BoxRec ir;
  RECT rcNew;
  REQUEST(xWindowsWMFrameGetRectReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameGetRect %d %d\n",
	  (sizeof(xWindowsWMFrameGetRectReq) >> 2), (int) client->req_len);
#endif
  
  REQUEST_SIZE_MATCH(xWindowsWMFrameGetRectReq);
  rep.type = X_Reply;
  rep.length = 0;
  rep.sequenceNumber = client->sequence;

  ir = make_box (stuff->ix, stuff->iy, stuff->iw, stuff->ih);

  if (stuff->frame_rect != 0)
    {
      ErrorF ("ProcWindowsWMFrameGetRect - stuff->frame_rect != 0\n");
      return BadValue;
    }

  /* Store the origin, height, and width in a rectangle structure */
  SetRect (&rcNew, stuff->ix, stuff->iy,
	   stuff->ix + stuff->iw, stuff->iy + stuff->ih);
    
#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameGetRect - %d %d %d %d\n",
	  stuff->ix, stuff->iy, stuff->ix + stuff->iw, stuff->iy + stuff->ih);
#endif

  /*
   * Calculate the required size of the Windows window rectangle,
   * given the size of the Windows window client area.
   */
  AdjustWindowRectEx (&rcNew, stuff->frame_style, FALSE, stuff->frame_style_ex);
  rep.x = rcNew.left;
  rep.y = rcNew.top;
  rep.w = rcNew.right - rcNew.left;
  rep.h = rcNew.bottom - rcNew.top;
#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameGetRect - %d %d %d %d\n",
	  rep.x, rep.y, rep.w, rep.h);
#endif

  WriteToClient(client, sizeof(xWindowsWMFrameGetRectReply), (char *)&rep);
  return (client->noClientException);
}


static int
ProcWindowsWMFrameDraw (register ClientPtr client)
{
  REQUEST(xWindowsWMFrameDrawReq);
  WindowPtr pWin;
  win32RootlessWindowPtr pRLWinPriv;
  RECT rcNew;
  int nCmdShow;
  RegionRec newShape;
  ScreenPtr pScreen;

  REQUEST_SIZE_MATCH (xWindowsWMFrameDrawReq);

#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameDraw\n");
#endif
  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }
#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameDraw - Window found\n");
#endif

  pRLWinPriv = (win32RootlessWindowPtr) RootlessFrameForWindow (pWin, TRUE);
  if (pRLWinPriv == 0) return BadWindow;

#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameDraw - HWND 0x%08x 0x%08x 0x%08x\n",
	  (int) pRLWinPriv->hWnd, (int) stuff->frame_style,
	  (int) stuff->frame_style_ex);
  ErrorF ("ProcWindowsWMFrameDraw - %d %d %d %d\n",
	  stuff->ix, stuff->iy, stuff->iw, stuff->ih);
#endif

  /* Store the origin, height, and width in a rectangle structure */
  SetRect (&rcNew, stuff->ix, stuff->iy,
	   stuff->ix + stuff->iw, stuff->iy + stuff->ih);

  /*
   * Calculate the required size of the Windows window rectangle,
   * given the size of the Windows window client area.
   */
  AdjustWindowRectEx (&rcNew, stuff->frame_style, FALSE, stuff->frame_style_ex);
  
  /* Set the window extended style flags */
  if (!SetWindowLongPtr (pRLWinPriv->hWnd, GWL_EXSTYLE, stuff->frame_style_ex))
    {
      return BadValue;
    }

  /* Set the window standard style flags */
  if (!SetWindowLongPtr (pRLWinPriv->hWnd, GWL_STYLE, stuff->frame_style))
    {
      return BadValue;
    }

  /* Flush the window style */
  if (!SetWindowPos (pRLWinPriv->hWnd, NULL,
		     rcNew.left, rcNew.top,
		     rcNew.right - rcNew.left, rcNew.bottom - rcNew.top,
		     SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE))
    {
      return BadValue;
    }
  if (!IsWindowVisible(pRLWinPriv->hWnd))
    nCmdShow = SW_HIDE;
  else 
    nCmdShow = SW_SHOWNA;

  ShowWindow (pRLWinPriv->hWnd, nCmdShow);

  winMWExtWMUpdateIcon (pWin->drawable.id);

  if (wBoundingShape(pWin) != NULL)
    {
      pScreen = pWin->drawable.pScreen;
      /* wBoundingShape is relative to *inner* origin of window.
	 Translate by borderWidth to get the outside-relative position. */
      
      REGION_NULL(pScreen, &newShape);
      REGION_COPY(pScreen, &newShape, wBoundingShape(pWin));
      REGION_TRANSLATE(pScreen, &newShape, pWin->borderWidth, pWin->borderWidth);
      winMWExtWMReshapeFrame (pRLWinPriv, &newShape);
      REGION_UNINIT(pScreen, &newShape);
    }
#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameDraw - done\n");
#endif

  return (client->noClientException);
}

static int
ProcWindowsWMFrameSetTitle(
			   register ClientPtr client
			   )
{
  unsigned int title_length, title_max;
  unsigned char *title_bytes;
  REQUEST(xWindowsWMFrameSetTitleReq);
  WindowPtr pWin;
  win32RootlessWindowPtr pRLWinPriv;

#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameSetTitle\n");
#endif

  REQUEST_AT_LEAST_SIZE(xWindowsWMFrameSetTitleReq);

  if (!(pWin = SecurityLookupWindow((Drawable)stuff->window,
				    client, SecurityReadAccess)))
    {
      return BadValue;
    }
#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameSetTitle - Window found\n");
#endif

  title_length = stuff->title_length;
  title_max = (stuff->length << 2) - sizeof(xWindowsWMFrameSetTitleReq);

  if (title_max < title_length)
    return BadValue;

#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameSetTitle - length is valid\n");
#endif

  title_bytes = malloc (title_length+1);
  strncpy (title_bytes, (unsigned char *) &stuff[1], title_length);
  title_bytes[title_length] = '\0';

  pRLWinPriv = (win32RootlessWindowPtr) RootlessFrameForWindow (pWin, FALSE);

  if (pRLWinPriv == 0)
    {
      free (title_bytes);
      return BadWindow;
    }
    
  /* Flush the window style */
  SetWindowText (pRLWinPriv->hWnd, title_bytes);

  free (title_bytes);

#if CYGIME_DEBUG
  ErrorF ("ProcWindowsWMFrameSetTitle - done\n");
#endif

  return (client->noClientException);
}
#endif

/* dispatch */

static int
ProcWinIMEDispatch (register ClientPtr client)
{
  REQUEST(xReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  switch (stuff->data)
    {
    case X_WinIMEQueryVersion:
      return ProcWinIMEQueryVersion(client);
    case X_WinIMESelectInput:
      return ProcWinIMESelectInput(client);
    case X_WinIMEEnable:
      return ProcWinIMEEnable(client);
    case X_WinIMEDisable:
      return ProcWinIMEDisable(client);
    case X_WinIMEOpen:
      return ProcWinIMEOpen(client);
    case X_WinIMEClose:
      return ProcWinIMEClose(client);
    case X_WinIMESetCompositionPoint:
      return ProcWinIMESetCompositionPoint(client);
    case X_WinIMESetCompositionRect:
      return ProcWinIMESetCompositionRect(client);
    case X_WinIMEGetCompositionString:
      return ProcWinIMEGetCompositionString(client);
    default:
      return BadRequest;
    }
}

static void
SNotifyEvent (xWinIMENotifyEvent *from, xWinIMENotifyEvent *to)
{
#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  to->type = from->type;
  to->kind = from->kind;
  cpswaps (from->sequenceNumber, to->sequenceNumber);
  cpswapl (from->window, to->window);
  cpswapl (from->time, to->time);
  cpswapl (from->arg, to->arg);
}

static int
SProcWinIMEQueryVersion (register ClientPtr client)
{
  register int n;
  REQUEST(xWinIMEQueryVersionReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  swaps(&stuff->length, n);
  return ProcWinIMEQueryVersion(client);
}

static int
SProcWinIMEDispatch (register ClientPtr client)
{
  REQUEST(xReq);

#if CYGIME_DEBUG
  winDebug ("%s\n", __FUNCTION__);
#endif

  /* It is bound to be non-local when there is byte swapping */
  if (!LocalClient(client))
    return WinIMEErrorBase + WinIMEClientNotLocal;

  /* only local clients are allowed access */
  switch (stuff->data)
    {
    case X_WinIMEQueryVersion:
      return SProcWinIMEQueryVersion(client);
    default:
      return BadRequest;
    }
}
