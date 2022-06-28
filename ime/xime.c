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
    Kensuke Matsuzaki   (zakki@peppermint.jp)
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
Display *dpy;
XIMS g_ims;

/* Supported Inputstyles */
static XIMStyle Styles[] = {
  XIMPreeditCallbacks|XIMStatusCallbacks,
  XIMPreeditPosition|XIMStatusArea,
  XIMPreeditPosition|XIMStatusNothing,
  XIMPreeditPosition|XIMStatusNone,
  XIMPreeditArea|XIMStatusArea,
  XIMPreeditNothing|XIMStatusNothing,
  XIMPreeditNothing|XIMStatusNone,
  0
};

#if 0
/* Trigger Keys List */
static XIMTriggerKey Trigger_Keys[] = {
    {XK_space, ShiftMask, ShiftMask},
    {0L, 0L, 0L}
};

/* Forward Keys List */
static XIMTriggerKey Forward_Keys[] = {
    {XK_Return, 0, 0},
    {XK_Tab, 0, 0},
    {0L, 0L, 0L}
};
#endif

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
  printf ("%s\n", __FUNCTION__);
  CreateIC(call_data);

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

  if (count > 0)
    {
      //fprintf(stdout, "'%s' is filtered in XIME\n", strbuf);
    }
  return True;
}

Bool
MyForwardEventHandler(XIMS ims, IMForwardEventStruct *call_data)
{
  printf ("%s\n", __FUNCTION__);

  /* Lookup KeyPress Events only */
  if (call_data->event.type != KeyPress) return True;

#if 1
  IMForwardEvent(ims, (XPointer)call_data);
#else
#if 0
  if (IsMatchKeys(ims, call_data, Forward_Keys))
    {
      IMForwardEvent(ims, (XPointer)call_data);
    }
  else
#endif
    {
      if (preedit_state_flag) {
	ProcessKey(ims, call_data);
      } else {
	IMForwardEvent(ims, (XPointer)call_data);
      }
    }
#endif
  return True;
}

Bool
MySetICFocusHandler(XIMS ims, IMChangeFocusStruct *call_data)
{
  IC *rec;
  printf ("%s\n", __FUNCTION__);
  rec = FindIC(call_data->icid);

  XWinIMESetFocus (dpy, rec->context, True);
  return True;
}

Bool
MyUnsetICFocusHandler(XIMS ims, IMChangeFocusStruct *call_data)
{
  IC *rec;
  printf ("%s\n", __FUNCTION__);
  rec = FindIC(call_data->icid);

  XWinIMESetFocus (dpy, rec->context, False);
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
#define COMPOSITION_STRING_BUFFER 1024
  char composition_string[COMPOSITION_STRING_BUFFER];
  int i;

  printf ("%s\n", __FUNCTION__);
  composition_string[0] = '\0';

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
    break;

  default:
    if (event->type == ime_event_base + WinIMEControllerNotify)
      {
	XWinIMENotifyEvent *ime_event = (XWinIMENotifyEvent*)event;
	IC *rec;

	rec = FindICbyContext (ime_event->context);

	switch (ime_event->kind){
	case WinIMEOpenStatus:
	  {
	    printf("WinIMEOpenStatus %d %s\n", ime_event->context,
		   ime_event->arg ? "Open" : "Close");

	    if (!rec)
	      {
		printf ("context %d IC is not found", ime_event->context);
		break;
	      }

	    if (ime_event->arg)
	      {
		IMPreeditStart(g_ims, (XPointer)&rec->call_data);
	      }
	    else
	      {
		IMPreeditStart(g_ims, (XPointer)&rec->call_data);
	      }

	    preedit_state_flag = ime_event->arg;
	  }
	  break;

	case WinIMEComposition:
	  {
	    printf("WinIMEComposition %d %d\n", ime_event->context, ime_event->arg);

	    if (!rec)
	      {
		printf ("context %d IC is not found", ime_event->context);
		break;
	      }

	    switch (ime_event->arg)
	      {
	      case WinIMECMPCompStr:
		{
		  int cursor;
		  if (!XWinIMEGetCompositionString (dpy, ime_event->context,
						    WinIMECMPCompStr,
						    COMPOSITION_STRING_BUFFER,
						    composition_string))
		    {
		      printf("XWinIMEGetCompositionString failed.\n");
		    }

		  if (!XWinIMEGetCursorPosition (dpy, ime_event->context,
						 &cursor))
		    {
		      printf("XWinIMEGetCursorPosition failed.\n");
		    }
		  rec->caret = cursor;

		  if (strlen(composition_string) > 0)
		    {
		      XTextProperty tp;
		      char *str = composition_string;
		      XIMText text;
		      XIMFeedback feedback[COMPOSITION_STRING_BUFFER] = {XIMUnderline};

		      printf ("compstr: %s\n", composition_string);
		      //setlocale (LC_CTYPE, "");
		      Xutf8TextListToTextProperty(dpy, (char **)&str, 1,
						  XCompoundTextStyle, &tp);

		      text.length = tp.nitems;
		      text.string.multi_byte = tp.value;
		      for (i = 0; i < text.length; i ++) feedback[i] = XIMUnderline;
		      feedback[text.length] = 0;
		      text.feedback = feedback;

		      rec->call_data.major_code = XIM_PREEDIT_DRAW;
		      rec->call_data.preedit_callback.icid = rec->id;
		      rec->call_data.preedit_callback.todo.draw.caret = rec->caret;
		      rec->call_data.preedit_callback.todo.draw.chg_first = 0;
		      rec->call_data.preedit_callback.todo.draw.chg_length = rec->length;
		      rec->call_data.preedit_callback.todo.draw.text = &text;

		      rec->length = tp.nitems;

		      printf("%d\n", rec->length);

		      IMCallCallback(g_ims, (XPointer)&rec->call_data);
		      composition_string[0] = '\0';
		    }
		}
		break;

	      case WinIMECMPResultStr:
		{
		  XIMFeedback feedback[1] = {0};

		  if (!XWinIMEGetCompositionString (dpy, ime_event->context,
						    WinIMECMPResultStr,
						    COMPOSITION_STRING_BUFFER,
						    composition_string))
		    {
		      printf("XWinIMEGetCompositionString failed.\n");
		    }

		  if (strlen(composition_string) > 0)
		    {
		      XTextProperty tp;
		      char *str = composition_string;
		      XIMText text;

		      printf ("commit result: %s\n", composition_string);
		      //setlocale (LC_CTYPE, "");
		      Xutf8TextListToTextProperty(dpy, (char **)&str, 1,
						  XCompoundTextStyle, &tp);

		      rec->call_data.commitstring.flag = XimLookupChars;
		      rec->call_data.commitstring.commit_string = (char *)tp.value;
		      rec->call_data.commitstring.icid = rec->id;

		      IMCommitString(g_ims, (XPointer)&rec->call_data);
		      composition_string[0] = '\0';

		      text.length = 0;
		      text.string.multi_byte = "";
		      text.feedback = feedback;

		      /* Clear preedit. */
		      rec->call_data.major_code = XIM_PREEDIT_DRAW;
		      rec->call_data.preedit_callback.icid = rec->id;
		      rec->call_data.preedit_callback.todo.draw.caret = 0;
		      rec->call_data.preedit_callback.todo.draw.chg_first = 0;
		      rec->call_data.preedit_callback.todo.draw.chg_length = rec->length;
		      rec->call_data.preedit_callback.todo.draw.text = &text;

		      rec->length = 0;

		      IMCallCallback(g_ims, (XPointer)&rec->call_data);
		    }
		}
		break;

	      default:
		break;
	      }
	  }
	  break;
	case WinIMEStartComposition:
	  {
	    printf("WinIMEStartComposition %d\n", ime_event->context);

	    if (!rec)
	      {
		printf ("context %d IC is not found", ime_event->context);
		break;
	      }

	    rec->call_data.major_code = XIM_PREEDIT_START;
	    IMCallCallback(g_ims, (XPointer)&rec->call_data);
	}
	break;
      case WinIMEEndComposition:
	{
	  printf("WinIMEEndComposition %d\n", ime_event->context);

	    if (!rec)
	      {
		printf ("context %d IC is not found", ime_event->context);
		break;
	      }

	    rec->call_data.major_code = XIM_PREEDIT_DONE;
	    IMCallCallback (g_ims, (XPointer)&rec->call_data);
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

    setlocale(LC_CTYPE, "");

    if ((dpy = XOpenDisplay(display_name)) == NULL) {
	fprintf(stderr, "Can't Open Display: %s\n", display_name);
	exit(1);
    }

    //XSetErrorHandler(ErrorHandler);

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
    XStoreName(dpy, im_window, "Windows IME Input Method Server");

    if ((input_styles = (XIMStyles *)malloc(sizeof(XIMStyles))) == NULL) {
	fprintf(stderr, "Can't allocate\n");
	exit(1);
    }
    input_styles->count_styles = sizeof(Styles)/sizeof(XIMStyle) - 1;
    input_styles->supported_styles = Styles;

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
    {
      int i;
      for (i = 0; i < styles2->count_styles; i ++)
	{
	  switch (styles2->supported_styles[i] & (XIMPreeditCallbacks|XIMPreeditPosition|XIMPreeditArea|XIMPreeditNothing))
	    {
	    case XIMPreeditCallbacks:
	      printf ("XIMPreeditCallbacks:");
	      break;
	    case XIMPreeditPosition:
	      printf ("XIMPreeditPosition:");
	      break;
	    case XIMPreeditArea:
	      printf ("XIMPreeditArea:");
	      break;
	    case XIMPreeditNothing:
	      printf ("XIMPreeditNothing:");
	      break;
	    default:
	      printf ("No preedit style:");
	      break;
	    }
	  switch (styles2->supported_styles[i] & (XIMStatusCallbacks|XIMStatusArea|XIMStatusNothing|XIMStatusNone))
	    {
	    case XIMStatusCallbacks:
	      printf ("XIMStatusCallbacks\n");
	      break;
	    case XIMStatusArea:
	      printf ("XIMStatusArea\n");
	      break;
	    case XIMStatusNothing:
	      printf ("XIMStatusNothing\n");
	      break;
	    case XIMStatusNone:
	      printf ("XIMStatusNone\n");
	      break;
	    default:
	      printf ("No status style\n");
	      break;
	    }
	}
    }
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
