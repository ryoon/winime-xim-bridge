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

#ifndef _WINIME_H_
#define _WINIME_H_

#include <X11/Xfuncproto.h>

#define X_WinIMEQueryVersion		0
#define X_WinIMESelectInput		1
#define X_WinIMEEnable			2
#define X_WinIMEDisable			3
#define X_WinIMEOpen			4
#define X_WinIMEClose			5
#define X_WinIMESetCompositionPoint	6
#define X_WinIMESetCompositionRect	7
#define X_WinIMEGetCompositionString	8

/* Events */
#define WinIMEControllerNotify		0
#define WinIMENumberEvents		1

/* Masks */
#define WinIMENotifyMask		(1L << 0)

/* "Kinds" of ControllerNotify events */
#define WinIMEEnabled			0
#define WinIMEDisabeled			1
#define WinIMEOpened			2
#define WinIMEClosed			3
#define WinIMEComposition		4
#define WinIMEStartComposition		5
#define WinIMEEndComposition		6

/* Errors */
#define WinIMEClientNotLocal		0
#define WinIMEOperationNotSupported	1
#define WinIMEDisabled			2
#define WinIMENumberErrors		3

#ifndef _WINIME_SERVER_

typedef struct {
    int	type;		    /* of event */
    unsigned long serial;   /* # of last request processed by server */
    Bool send_event;	    /* true if this came frome a SendEvent request */
    Display *display;	    /* Display the event was read from */
    Window window;	    /* window of event */
    Time time;		    /* server timestamp when event happened */
    int kind;		    /* subtype of event */
    int arg;
} XWinIMENotifyEvent;

_XFUNCPROTOBEGIN

Bool XWinIMEQueryExtension (Display *dpy, int *event_base, int *error_base);

Bool XWinIMEQueryVersion (Display *dpy, int *majorVersion,
			  int *minorVersion, int *patchVersion);

Bool XWinIMESelectInput (Display *dpy, unsigned long mask);

Bool XWinIMEEnable (Display *dpy, Window window);

Bool XWinIMEDisable (Display *dpy, Window window);

Bool XWinIMEOpen (Display *dpy, Window window);

Bool XWinIMEClose (Display *dpy, Window window);

Bool XWinIMESetCompositionPoint (Display *dpy, Window window,
				 short cf_x, short cf_y);

Bool XWinIMESetCompositionRect (Display *dpy, Window window,
				short cf_x, short cf_y,
				short cf_w, short cf_h);

Bool XWinIMEGetCompositionString (Display *dpy, Window window,
				  int count,
				  char* str_return);

_XFUNCPROTOEND

#endif /* _WINDOWSWM_SERVER_ */
#endif /* _WINDOWSWM_H_ */