/******************************************************************

         Copyright 1993, 1994 by Hewlett-Packard Company

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose without fee is hereby granted,
provided that the above copyright notice appear in all copies and
that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of Hewlett-Packard not
be used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.
Hewlett-Packard Company makes no representations about the suitability
of this software for any purpose.
It is provided "as is" without express or implied warranty.

HEWLETT-PACKARD COMPANY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL HEWLETT-PACKARD COMPANY BE LIABLE FOR ANY SPECIAL,
INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

Author:
    Hidetoshi Tajima	Hewlett-Packard Company.
			(tajima@kobe.hp.com)
******************************************************************/
#include <stdio.h>
#include <string.h>
#include <X11/Xlocale.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <IMdkit.h>
#include <Xi18n.h>
#include <X11/extensions/winime.h>
#include "winIC.h"

#define DEFAULT_IMNAME "XIME"
#define DEFAULT_LOCALE "ja_JP"

/* flags for debugging */
Bool use_tcp = False;		/* Using TCP/IP Transport or not */
Bool use_local = False;		/* Using Unix domain Tranport or not */
long filter_mask = KeyPressMask|KeyReleaseMask;
int ime_event_base = 0;
int ime_error_base = 0;
Bool preedit_state_flag = False;
Bool preedit_state_changed = False;
#define COMPOSITION_STRING_BUFFER 1024
char composition_string[COMPOSITION_STRING_BUFFER];
Display *dpy;
CARD16 last_connect_id;
CARD16 last_icid;
XIMS g_ims;
int g_major_code;
int g_minor_code;

/* Supported Inputstyles */
static XIMStyle Styles[] = {
  //XIMPreeditCallbacks|XIMStatusCallbacks,
    XIMPreeditPosition|XIMStatusArea,
    XIMPreeditPosition|XIMStatusNothing,
    XIMPreeditPosition|XIMStatusNone,
    //XIMPreeditArea|XIMStatusArea,
    //XIMPreeditNothing|XIMStatusNothing,
    0
};

/* Trigger Keys List */
static XIMTriggerKey Trigger_Keys[] = {
    {XK_space, ShiftMask, ShiftMask},
    {0L, 0L, 0L}
};

/* Conversion Keys List */
static XIMTriggerKey Conversion_Keys[] = {
    {XK_k, ControlMask, ControlMask},
    {0L, 0L, 0L}
};

/* Forward Keys List */
static XIMTriggerKey Forward_Keys[] = {
    {XK_Return, 0, 0},
    {XK_Tab, 0, 0},
    {0L, 0L, 0L}
};

/* Supported Japanese Encodings */
static XIMEncoding jaEncodings[] = {
    "COMPOUND_TEXT",
    "UTF-8",
    NULL
};

/* Supported Taiwanese Encodings */
static XIMEncoding zhEncodings[] = {
    "COMPOUND_TEXT",
    "eucTW",
    NULL
};

Bool
MyGetICValuesHandler(XIMS ims, IMChangeICStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);
  GetIC(call_data);
  return True;
}

Bool
MySetICValuesHandler(XIMS ims, IMChangeICStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);
  SetIC (call_data);
  return True;
}

Bool
MyOpenHandler(XIMS ims, IMOpenStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);
  printf ("new_client lang is %s\n", call_data->lang.name);
  return True;
}

Bool
MyCreateICHandler(XIMS ims, IMChangeICStruct* call_data)
{
  IC *rec;
  printf ("%s\n", __FUNCTION__);
  CreateIC(call_data);

  rec = FindIC(call_data->icid);

  if (rec)
    {
      XWinIMECreateContext (dpy, &rec->context);
      printf ("%d\n", rec->context);
    }
  else
    {
      printf ("IC isn't found\n");
    }
  return True;
}

#define STRBUFLEN 64
Bool
IsMatchKeys(XIMS ims, IMForwardEventStruct *call_data, XIMTriggerKey *trigger)
{
    char strbuf[STRBUFLEN];
    KeySym keysym;
    int i;
    int modifier;
    int modifier_mask;
    XKeyEvent *kev;
    int key_count;

    printf ("%s\n", __FUNCTION__);

    kev = (XKeyEvent*)&call_data->event;
    XLookupString(kev, strbuf, STRBUFLEN, &keysym, NULL);

    for (i = 0; trigger[i].keysym != 0; i++);
    key_count = i;
    for (i = 0; i < key_count; i++) {
	modifier      = trigger[i].modifier;
	modifier_mask = trigger[i].modifier_mask;
	if (((KeySym)trigger[i].keysym == keysym)
	    && ((kev->state & modifier_mask) == modifier))
	  return True;
    }
    return False;
}

Bool
ProcessKey(XIMS ims, IMForwardEventStruct *call_data)
{
    char strbuf[STRBUFLEN];
    KeySym keysym;
    XKeyEvent *kev;
    int count;

    printf ("%s\n", __FUNCTION__);

    kev = (XKeyEvent*)&call_data->event;
    count = XLookupString(kev, strbuf, STRBUFLEN, &keysym, NULL);

    if (count > 0) {
	fprintf(stdout, "'%s' is filtered in sampleIM\n", strbuf);
    }
    return True;
}

Bool
MyForwardEventHandler(XIMS ims, IMForwardEventStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);
#if 0
  printf ("\t%d\n", call_data->event.type);

  if (call_data->event.type != KeyPress || call_data->event.type != KeyRelease)
    {
      XWinIMEFilterKeyEvent (dpy, call_data->event.type,
			     call_data->event.xkey.keycode, call_data->event.xkey.state);
    }
#endif
  /* Lookup KeyPress Events only */
  if (call_data->event.type != KeyPress) return True;

  last_connect_id = call_data->connect_id;
  last_icid = call_data->icid;
  g_major_code = call_data->major_code;
  g_minor_code = call_data->minor_code;

#if 0
  //XXX: Use extension event
  /* In case of Static Event Flow */
  if (!use_trigger) {
    static Bool preedit_state_flag = False;
    if (IsMatchKeys(ims, call_data, Trigger_Keys)) {
      preedit_state_flag = !preedit_state_flag;
      return True;
    }
  }
  /* In case of Dynamic Event Flow without registering OFF keys,
     the end of preediting must be notified from IMserver to
     IMlibrary. */
  if (use_trigger) {
    if (IsMatchKeys(ims, call_data, Trigger_Keys)) {
      return IMPreeditEnd(ims, (XPointer)call_data);
    }
  }
#endif
#if 0
  if (IsMatchKeys(ims, call_data, Conversion_Keys)) {
    XTextProperty tp;
    Display *display = ims->core.display;
    char *text = "これは IM からの確定文字列です。";
    char lang[20];

    setlocale(LC_CTYPE, "");
    XmbTextListToTextProperty(display, (char **)&text, 1,
			      XCompoundTextStyle, &tp);
    ((IMCommitStruct*)call_data)->flag |= XimLookupChars;
    ((IMCommitStruct*)call_data)->commit_string = (char *)tp.value;
    IMCommitString(ims, (XPointer)call_data);
  }
#endif

  if (preedit_state_changed) {
    preedit_state_changed = False;
    if (preedit_state_flag) {
      return IMPreeditStart(ims, (XPointer)call_data);
    } else {
      return IMPreeditEnd(ims, (XPointer)call_data);
    }
  }

#if 0
  if (strlen(composition_string) > 0){
    XTextProperty tp;
    char *str = composition_string;

    printf ("commit: %s\n", composition_string);
    setlocale(LC_CTYPE, "");
    Xutf8TextListToTextProperty(dpy, (char **)&str, 1,
				XCompoundTextStyle, &tp);
    ((IMCommitStruct*)call_data)->flag |= XimLookupChars;
    ((IMCommitStruct*)call_data)->commit_string = (char *)tp.value;
    IMCommitString (ims, (XPointer)call_data);
    composition_string[0] = '\0';
  }
#endif
  if (IsMatchKeys(ims, call_data, Forward_Keys)) {
    IMForwardEvent(ims, (XPointer)call_data);
  } else {
    if (preedit_state_flag) {
      ProcessKey(ims, call_data);
    } else {
      IMForwardEvent(ims, (XPointer)call_data);
    }
  }
  return True;
}

Bool
MySetICFocusHandler(XIMS ims, IMChangeFocusStruct *call_data)
{
  IC *rec;
  printf ("%s\n", __FUNCTION__);
  rec = FindIC(call_data->icid);

  XWinIMESetFocus (dpy, rec->context);
  return True;
}

Bool
MyUnsetICFocusHandler(XIMS ims, IMChangeFocusStruct *call_data)
{
  IC *rec;
  printf ("%s\n", __FUNCTION__);
  rec = FindIC(call_data->icid);

  XWinIMEUnsetFocus (dpy, rec->context);
  return True;
}

Bool
MyTriggerNotifyHandler(XIMS ims, IMTriggerNotifyStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);

  if (call_data->flag == 0) {	/* on key */
    /* Here, the start of preediting is notified from IMlibrary, which
       is the only way to start preediting in case of Dynamic Event
       Flow, because ON key is mandatary for Dynamic Event Flow. */
    return True;
  }
#if 0
  else if (use_offkey && call_data->flag == 1) {	/* off key */
    /* Here, the end of preediting is notified from the IMlibrary, which
       happens only if OFF key, which is optional for Dynamic Event Flow,
       has been registered by IMOpenIM or IMSetIMValues, otherwise,
       the end of preediting must be notified from the IMserver to the
       IMlibrary. */
    return True;
  }
#endif
  else {
    /* never happens */
    return False;
  }
}

Bool
MyPreeditStartReplyHandler(XIMS ims, IMPreeditCBStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);
  return True;
}

Bool
MyPreeditCaretReplyHandler(XIMS ims, IMPreeditCBStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);
  return True;
}

Bool
MyMoveStructHandler(XIMS ims, IMMoveStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);
  printf ("%d, %d\n", call_data->x, call_data->y);
  return True;
}

Bool
MyProtoHandler(XIMS ims, IMProtocol *call_data)
{
  printf ("%s\n", __FUNCTION__);
  switch (call_data->major_code) {
  case XIM_OPEN:
    MyOpenHandler(ims, (IMOpenStruct*)call_data);
    break;
  case XIM_CREATE_IC:
    MyCreateICHandler(ims, (IMChangeICStruct*)call_data);
    break;
  case XIM_DESTROY_IC:
    break;
  case XIM_SET_IC_VALUES:
    MySetICValuesHandler(ims, (IMChangeICStruct*)call_data);
    break;
  case XIM_GET_IC_VALUES:
    MyGetICValuesHandler(ims, (IMChangeICStruct*)call_data);
    break;
  case XIM_FORWARD_EVENT:
    MyForwardEventHandler(ims, (IMForwardEventStruct*)call_data);
    break;
  case XIM_SET_IC_FOCUS:
    MySetICFocusHandler(ims, (IMChangeFocusStruct*)call_data);
    break;
  case XIM_UNSET_IC_FOCUS:
    MyUnsetICFocusHandler(ims, (IMChangeFocusStruct*)call_data);
    break;
  case XIM_RESET_IC:
    break;
  case XIM_TRIGGER_NOTIFY:
    MyTriggerNotifyHandler(ims, (IMTriggerNotifyStruct*)call_data);
    break;
  case XIM_PREEDIT_START_REPLY:
    MyPreeditStartReplyHandler(ims, (IMPreeditCBStruct*)call_data);
    break;
  case XIM_PREEDIT_CARET_REPLY:
    MyPreeditCaretReplyHandler(ims, (IMPreeditCBStruct*)call_data);
  case XIM_EXT_MOVE:
    MyMoveStructHandler(ims, (IMMoveStruct*)call_data);
    break;
  }
  return True;
}

void
MyXEventHandler(Window im_window, XEvent *event)
{
  printf ("%s\n", __FUNCTION__);
  switch (event->type) {
  case DestroyNotify:
    break;
  case ButtonPress:
    switch (event->xbutton.button) {
    case Button3:
      if (event->xbutton.window == im_window)
	goto Exit;
      break;
    }
  default:
    if (event->type == ime_event_base + WinIMEControllerNotify) {
      XWinIMENotifyEvent *ime_event = (XWinIMENotifyEvent*)event;

      switch (ime_event->kind){
      case WinIMEEnabled:
	printf("WinIMEEnabled\n");
	break;
      case WinIMEDisabeled:
	printf("WinIMEDisabeled\n");
	break;
      case WinIMEOpened:
	printf("WinIMEOpened\n");
	break;
      case WinIMEClosed:
	printf("WinIMEClosed\n");
	break;
      case WinIMEComposition:
	printf("WinIMEComposition\n");
	break;
      case WinIMEStartComposition:
	printf("WinIMEStartComposition\n");
	preedit_state_flag = True;
	preedit_state_changed = True;
	break;
      case WinIMEEndComposition:
	{
	  printf("WinIMEEndComposition\n");

	  preedit_state_flag = False;
	  preedit_state_changed = True;
	  if (!XWinIMEGetCompositionString (dpy, ime_event->context,
					    COMPOSITION_STRING_BUFFER,
					    composition_string))
	    {
	      printf("XWinIMEGetCompositionString failed.\n");
	    }

	  if (strlen(composition_string) > 0){
	    XTextProperty tp;
	    char *str = composition_string;
	    IMCommitStruct call_data;

	    printf ("commit: %s\n", composition_string);
	    setlocale(LC_CTYPE, "");
	    Xutf8TextListToTextProperty(dpy, (char **)&str, 1,
					XCompoundTextStyle, &tp);

	    call_data.connect_id = last_connect_id;
	    call_data.icid = last_icid;
	    call_data.major_code = g_major_code;
	    call_data.minor_code = g_minor_code;

	    call_data.flag = XimLookupChars;
	    call_data.commit_string = (char *)tp.value;
	    IMCommitString(g_ims, (XPointer)&call_data);
	    composition_string[0] = '\0';
	  }
	}
	break;
      default:
	break;
      }
    }
    break;
  }
  return;
 Exit:
  XDestroyWindow(event->xbutton.display, im_window);
  exit(0);
}
int ErrorHandler (Display *dpy, XErrorEvent *ev)
{
  printf("Error occurred.\n");

  return 0;
}

int
main(int argc, char **argv)
{
    char *display_name = NULL;
    char *imname = NULL;
    XIMS ims;
    XIMStyles *input_styles, *styles2;
    XIMTriggerKeys *on_keys, *trigger2;
    XIMEncodings *encodings, *encoding2;
    Window im_window;
    register int i;
    char transport[80];		/* enough */

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-name")) {
	    imname = argv[++i];
	} else if (!strcmp(argv[i], "-display")) {
	    display_name = argv[++i];
	} else if (!strcmp(argv[i], "-tcp")) {
	    use_tcp = True;
	} else if (!strcmp(argv[i], "-local")) {
	    use_local = True;
	} else if (!strcmp(argv[i], "-kl")) {
	    filter_mask = (KeyPressMask|KeyReleaseMask);
	}
    }
    if (!imname) imname = DEFAULT_IMNAME;

    if ((dpy = XOpenDisplay(display_name)) == NULL) {
	fprintf(stderr, "Can't Open Display: %s\n", display_name);
	exit(1);
    }
    XSetErrorHandler(ErrorHandler);

    if(!XWinIMEQueryExtension (dpy, &ime_event_base, &ime_error_base)){
	fprintf(stderr, "No IME Extension\n");
	exit(1);
    }

    XWinIMESelectInput (dpy, WinIMENotifyMask);

    im_window = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
				    0, 0, 1, 1, 1, 0, 0);
    if (im_window == (Window)NULL) {
	fprintf(stderr, "Can't Create Window\n");
	exit(1);
    }
    XStoreName(dpy, im_window, "Sample Input Method Server");

    if ((input_styles = (XIMStyles *)malloc(sizeof(XIMStyles))) == NULL) {
	fprintf(stderr, "Can't allocate\n");
	exit(1);
    }
    input_styles->count_styles = sizeof(Styles)/sizeof(XIMStyle) - 1;
    input_styles->supported_styles = Styles;

    if ((on_keys = (XIMTriggerKeys *)
	 malloc(sizeof(XIMTriggerKeys))) == NULL) {
	fprintf(stderr, "Can't allocate\n");
	exit(1);
    }
    on_keys->count_keys = sizeof(Trigger_Keys)/sizeof(XIMTriggerKey) - 1;
    on_keys->keylist = Trigger_Keys;

    if ((encodings = (XIMEncodings *)malloc(sizeof(XIMEncodings))) == NULL) {
	fprintf(stderr, "Can't allocate\n");
	exit(1);
    }
    encodings->count_encodings = sizeof(jaEncodings)/sizeof(XIMEncoding) - 1;
    encodings->supported_encodings = jaEncodings;

    if (use_local) {
	char hostname[64];
	char *address = "/tmp/.ximsock";

	gethostname(hostname, 64);
	sprintf(transport, "local/%s:%s", hostname, address);
    } else if (use_tcp) {
	char hostname[64];
	int port_number = 9010;

	gethostname(hostname, 64);
	sprintf(transport, "tcp/%s:%d", hostname, port_number);
    } else {
	strcpy(transport, "X/");
    }

    g_ims = ims = IMOpenIM(dpy,
			   IMModifiers, "Xi18n",
			   IMServerWindow, im_window,
			   IMServerName, imname,
			   IMLocale, DEFAULT_LOCALE,
			   IMServerTransport, transport,
			   IMInputStyles, input_styles,
			   NULL);
    if (ims == (XIMS)NULL) {
	fprintf(stderr, "Can't Open Input Method Service:\n");
	fprintf(stderr, "\tInput Method Name :%s\n", imname);
	fprintf(stderr, "\tTranport Address:%s\n", transport);
	exit(1);
    }
#if 0
    //XXX: use extension
    if (use_trigger) {
      IMSetIMValues(ims,
		    IMOnKeysList, on_keys,
		    NULL);
    }
#endif
    IMSetIMValues(ims,
		  IMEncodingList, encodings,
		  IMProtocolHandler, MyProtoHandler,
		  IMFilterEventMask, filter_mask,
		  NULL);
    IMGetIMValues(ims,
		  IMInputStyles, &styles2,
		  IMOnKeysList, &trigger2,
		  IMOffKeysList, &trigger2,
		  IMEncodingList, &encoding2,
		  NULL);
    XSelectInput(dpy, im_window, StructureNotifyMask|ButtonPressMask);
    XMapWindow(dpy, im_window);
    XFlush(dpy);		/* necessary flush for tcp/ip connection */

    for (;;) {
	XEvent event;
	XNextEvent(dpy, &event);
	if (XFilterEvent(&event, None) == True)
	  continue;
	MyXEventHandler(im_window, &event);
    }
}
