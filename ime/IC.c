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

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/extensions/winime.h>
#include <stdio.h>
#include "IMdkit/IMdkit.h"
#include "IMdkit/Xi18n.h"
#include "IC.h"

extern Display *dpy;
static IMIC *ic_list = (IMIC *)NULL;
static IMIC *free_list = (IMIC *)NULL;

static IMIC
*NewIMIC()
{
    static CARD16 icid = 0;
    IMIC *rec;

    if (free_list != NULL) {
	rec = free_list;
	free_list = free_list->next;
    } else {
	rec = (IMIC *)malloc(sizeof(IMIC));
    }
    memset(rec, 0, sizeof(IMIC));
    rec->id = ++icid;

    XWinIMECreateContext (dpy, &rec->context);
    printf ("%d\n", rec->context);

    rec->next = ic_list;
    ic_list = rec;
    return rec;
}

static Window
TopLevelWindow(Window win)
{
  Window w, root, parent, *children;
  int nchildren;

  printf ("%s\n", __FUNCTION__);
  parent = win;

  do {
    w = parent;
    if (!XQueryTree(dpy, w, &root, &parent, &children, &nchildren)) return None;

    if (!children) XFree (children);
  } while (root != parent);

  return w;
}
static void
StoreIMIC(rec, call_data)
IMIC *rec;
IMChangeICStruct *call_data;
{
    XICAttribute *ic_attr = call_data->ic_attr;
    XICAttribute *pre_attr = call_data->preedit_attr;
    XICAttribute *sts_attr = call_data->status_attr;
    register int i;

    for (i = 0; i < (int)call_data->ic_attr_num; i++, ic_attr++) {
      printf ("StoreIMIC.ic: %s\n", pre_attr->name);
      if (!strcmp(XNInputStyle, ic_attr->name))
	{
	  printf ("XNInputStyle");
	  rec->input_style = *(INT32*)ic_attr->value;
	  switch (rec->input_style & (XIMPreeditCallbacks|XIMPreeditPosition|XIMPreeditArea|XIMPreeditNothing))
	    {
	    case XIMPreeditCallbacks:
	      printf ("XIMPreeditCallbacks:\n");
	      XWinIMESetCompositionDraw (dpy, rec->context, False);
	      break;
	    case XIMPreeditPosition:
	      printf ("XIMPreeditPosition:\n");
	      XWinIMESetCompositionDraw (dpy, rec->context, True);
	      break;
	    case XIMPreeditArea:
	      printf ("XIMPreeditArea:\n");
	      XWinIMESetCompositionDraw (dpy, rec->context, True);
	      break;
	    case XIMPreeditNothing:
	      printf ("XIMPreeditNothing:\n");
	      XWinIMESetCompositionDraw (dpy, rec->context, True);
	      XWinIMESetCompositionWindow (dpy, rec->context,
					   WinIMECSDefault,
					   0, 0, 0, 0);
	      break;
	    default:
	      printf ("No preedit style:\n");
	      break;
	    }
	  switch (rec->input_style & (XIMStatusCallbacks|XIMStatusArea|XIMStatusNothing|XIMStatusNone))
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
      else if (!strcmp(XNClientWindow, ic_attr->name))
	rec->client_win = *(Window*)ic_attr->value;
      else if (!strcmp(XNFocusWindow, ic_attr->name))
	rec->focus_win = *(Window*)ic_attr->value;
    }
    for (i = 0; i < (int)call_data->preedit_attr_num; i++, pre_attr++) {
      printf ("StoreIMIC.preedit: %s\n", pre_attr->name);
	if (!strcmp(XNArea, pre_attr->name))
	  {
	    int x, y, w, h;
	    Window child;

	    rec->pre_attr.area = *(XRectangle*)pre_attr->value;
	    printf ("StoreIMIC: XArea(%d, %d, %d, %d)\n",
		    rec->pre_attr.area.x, rec->pre_attr.area.y,
		    rec->pre_attr.area.width, rec->pre_attr.area.height);
	    x = rec->pre_attr.area.x;
	    y = rec->pre_attr.area.y;
	    w = rec->pre_attr.area.width;
	    h = rec->pre_attr.area.height;

	    if (rec->focus_win)
	      {
		Window top = TopLevelWindow (rec->focus_win);
		if (top != None)
		  {
		    XTranslateCoordinates (dpy, rec->focus_win,
					   top, x, y, &x, &y, &child);
		    XTranslateCoordinates (dpy, rec->focus_win,
					   top, w, h, &w, &h, &child);
		    printf ("(%d, %d) to (%d %d)\n", rec->pre_attr.area.x, rec->pre_attr.area.y, x, y);
		  }
		else
		  {
		    printf ("failed. use (%d, %d)\n", x, y);
		  }
	      }

	    XWinIMESetCompositionWindow (dpy, rec->context,
					 WinIMECSRect,
					 x, y, w, h);
	  }
	else if (!strcmp(XNAreaNeeded, pre_attr->name))
	  {
	    rec->pre_attr.area_needed = *(XRectangle*)pre_attr->value;
	  }
	else if (!strcmp(XNSpotLocation, pre_attr->name))
	  {
	    int x, y;
	    Window child;

	    rec->pre_attr.spot_location = *(XPoint*)pre_attr->value;
	    printf ("StoreIMIC: XNSpotLocation(%d,%d)\n",
		    rec->pre_attr.spot_location.x, rec->pre_attr.spot_location.y);

	    x = rec->pre_attr.spot_location.x;
	    y = rec->pre_attr.spot_location.y;

	    if (rec->focus_win)
	      {
		Window top = TopLevelWindow (rec->focus_win);
		if (top != None)
		  {
		    XTranslateCoordinates (dpy, rec->focus_win,
					   top, x, y, &x, &y, &child);
		    printf ("(%d, %d) to (%d %d)\n",
			    rec->pre_attr.spot_location.x, rec->pre_attr.spot_location.y,
			    x, y);
		  }
		else
		  {
		    printf ("failed. use (%d, %d)\n", x, y);
		  }
	      }
	    XWinIMESetCompositionWindow (dpy, rec->context,
					 WinIMECSPoint,
					 x, y, 0, 0);
	  }
	else if (!strcmp(XNColormap, pre_attr->name))
	  rec->pre_attr.cmap = *(Colormap*)pre_attr->value;
	else if (!strcmp(XNStdColormap, pre_attr->name))
	  rec->pre_attr.cmap = *(Colormap*)pre_attr->value;
	else if (!strcmp(XNForeground, pre_attr->name))
	  rec->pre_attr.foreground = *(CARD32*)pre_attr->value;
	else if (!strcmp(XNBackground, pre_attr->name))
	  rec->pre_attr.background = *(CARD32*)pre_attr->value;
	else if (!strcmp(XNBackgroundPixmap, pre_attr->name))
	  rec->pre_attr.bg_pixmap = *(Pixmap*)pre_attr->value;
	else if (!strcmp(XNFontSet, pre_attr->name)) {
	    int str_length = strlen(pre_attr->value);
	    if (rec->pre_attr.base_font != NULL) {
		if (strcmp(rec->pre_attr.base_font, pre_attr->value)) {
		    XFree(rec->pre_attr.base_font);
		} else {
		    continue;
		}
	    }
	    rec->pre_attr.base_font = malloc(str_length + 1);
	    strcpy(rec->pre_attr.base_font, pre_attr->value);
	} else if (!strcmp(XNLineSpace, pre_attr->name))
	  rec->pre_attr.line_space = *(CARD32*)pre_attr->value;
	else if (!strcmp(XNCursor, pre_attr->name))
	  rec->pre_attr.cursor = *(Cursor*)pre_attr->value;
    }
    for (i = 0; i < (int)call_data->status_attr_num; i++, sts_attr++) {
      printf ("StoreIMIC.status: %s", sts_attr->name);
      if (!strcmp(XNArea, sts_attr->name))
	{
	  rec->sts_attr.area = *(XRectangle*)sts_attr->value;
#if 0
	  printf ("StoreIMIC: XArea(%d, %d, %d, %d)\n",
		  rec->sts_attr.area.x, rec->sts_attr.area.y,
		  rec->sts_attr.area.width, rec->sts_attr.area.height);
	  XWinIMESetCompositionRect (dpy, rec->context,
				     rec->pre_attr.area.x, rec->sts_attr.area.y,
				     rec->sts_attr.area.width, rec->sts_attr.area.height);
#endif
	}
      else if (!strcmp(XNAreaNeeded, sts_attr->name))
	rec->sts_attr.area_needed = *(XRectangle*)sts_attr->value;
      else if (!strcmp(XNColormap, sts_attr->name))
	rec->sts_attr.cmap = *(Colormap*)sts_attr->value;
      else if (!strcmp(XNStdColormap, sts_attr->name))
	rec->sts_attr.cmap = *(Colormap*)sts_attr->value;
      else if (!strcmp(XNForeground, sts_attr->name))
	rec->sts_attr.foreground = *(CARD32*)sts_attr->value;
      else if (!strcmp(XNBackground, sts_attr->name))
	rec->sts_attr.background = *(CARD32*)sts_attr->value;
      else if (!strcmp(XNBackgroundPixmap, sts_attr->name))
	rec->sts_attr.bg_pixmap = *(Pixmap*)sts_attr->value;
      else if (!strcmp(XNFontSet, sts_attr->name)) {
	int str_length = strlen(sts_attr->value);
	if (rec->sts_attr.base_font != NULL) {
	  if (strcmp(rec->sts_attr.base_font, sts_attr->value)) {
	    XFree(rec->sts_attr.base_font);
	  } else {
	    continue;
	  }
	}
	rec->sts_attr.base_font = malloc(str_length + 1);
	strcpy(rec->sts_attr.base_font, sts_attr->value);
      } else if (!strcmp(XNLineSpace, sts_attr->name))
	rec->sts_attr.line_space= *(CARD32*)sts_attr->value;
      else if (!strcmp(XNCursor, sts_attr->name))
	rec->sts_attr.cursor = *(Cursor*)sts_attr->value;
    }

    rec->call_data.major_code = call_data->major_code;
    rec->call_data.any.minor_code = call_data->minor_code;
    rec->call_data.any.connect_id = call_data->connect_id;
}

IMIC*
FindIMIC(icid)
CARD16 icid;
{
    IMIC *rec = ic_list;

    while (rec != NULL) {
	if (rec->id == icid)
	  return rec;
	rec = rec->next;
    }

    return NULL;
}

IMIC*
FindIMICbyContext(int context)
{
    IMIC *rec = ic_list;

    while (rec != NULL) {
	if (rec->context == context)
	  return rec;
	rec = rec->next;
    }

    return NULL;
}

void
CreateIMIC(call_data)
IMChangeICStruct *call_data;
{
    IMIC *rec;

    rec = NewIMIC();
    if (rec == NULL)
      return;

    memset (&rec->call_data, 0, sizeof(IMProtocol));

    StoreIMIC(rec, call_data);
    call_data->icid = rec->id;

    return;
}

void
SetIMIC(call_data)
IMChangeICStruct *call_data;
{
    IMIC *rec = FindIMIC(call_data->icid);

    if (rec == NULL)
      return;
    StoreIMIC(rec, call_data);
    return;
}

void
GetIMIC(call_data)
IMChangeICStruct *call_data;
{
    XICAttribute *ic_attr = call_data->ic_attr;
    XICAttribute *pre_attr = call_data->preedit_attr;
    XICAttribute *sts_attr = call_data->status_attr;
    register int i;
    IMIC *rec = FindIMIC(call_data->icid);

    if (rec == NULL)
      return;
    for (i = 0; i < (int)call_data->ic_attr_num; i++, ic_attr++) {
	if (!strcmp(XNFilterEvents, ic_attr->name)) {
	    ic_attr->value = (void *)malloc(sizeof(CARD32));
	    *(CARD32*)ic_attr->value = KeyPressMask|KeyReleaseMask;
	    ic_attr->value_length = sizeof(CARD32);
	}
    }

    /* preedit attributes */
    for (i = 0; i < (int)call_data->preedit_attr_num; i++, pre_attr++) {
	if (!strcmp(XNArea, pre_attr->name)) {
	    pre_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)pre_attr->value = rec->pre_attr.area;
	    pre_attr->value_length = sizeof(XRectangle);
	} else if (!strcmp(XNAreaNeeded, pre_attr->name)) {
	    pre_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)pre_attr->value = rec->pre_attr.area_needed;
	    pre_attr->value_length = sizeof(XRectangle);
	} else if (!strcmp(XNSpotLocation, pre_attr->name)) {
	    pre_attr->value = (void *)malloc(sizeof(XPoint));
	    *(XPoint*)pre_attr->value = rec->pre_attr.spot_location;
	    pre_attr->value_length = sizeof(XPoint);
	} else if (!strcmp(XNFontSet, pre_attr->name)) {
	    CARD16 base_len = (CARD16)strlen(rec->pre_attr.base_font);
	    int total_len = sizeof(CARD16) + (CARD16)base_len;
	    char *p;

	    pre_attr->value = (void *)malloc(total_len);
	    p = (char *)pre_attr->value;
	    memmove(p, &base_len, sizeof(CARD16));
	    p += sizeof(CARD16);
	    strncpy(p, rec->pre_attr.base_font, base_len);
	    pre_attr->value_length = total_len;
	} else if (!strcmp(XNForeground, pre_attr->name)) {
	    pre_attr->value = (void *)malloc(sizeof(long));
	    *(long*)pre_attr->value = rec->pre_attr.foreground;
	    pre_attr->value_length = sizeof(long);
	} else if (!strcmp(XNBackground, pre_attr->name)) {
	    pre_attr->value = (void *)malloc(sizeof(long));
	    *(long*)pre_attr->value = rec->pre_attr.background;
	    pre_attr->value_length = sizeof(long);
	} else if (!strcmp(XNLineSpace, pre_attr->name)) {
	    pre_attr->value = (void *)malloc(sizeof(long));
#if 0
	    *(long*)pre_attr->value = rec->pre_attr.line_space;
#endif
	    *(long*)pre_attr->value = 18;
	    pre_attr->value_length = sizeof(long);
	}
    }

    /* status attributes */
    for (i = 0; i < (int)call_data->status_attr_num; i++, sts_attr++) {
	if (!strcmp(XNArea, sts_attr->name)) {
	    sts_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)sts_attr->value = rec->sts_attr.area;
	    sts_attr->value_length = sizeof(XRectangle);
	} else if (!strcmp(XNAreaNeeded, sts_attr->name)) {
	    sts_attr->value = (void *)malloc(sizeof(XRectangle));
	    *(XRectangle*)sts_attr->value = rec->sts_attr.area_needed;
	    sts_attr->value_length = sizeof(XRectangle);
	} else if (!strcmp(XNFontSet, sts_attr->name)) {
	    CARD16 base_len = (CARD16)strlen(rec->sts_attr.base_font);
	    int total_len = sizeof(CARD16) + (CARD16)base_len;
	    char *p;

	    sts_attr->value = (void *)malloc(total_len);
	    p = (char *)sts_attr->value;
	    memmove(p, &base_len, sizeof(CARD16));
	    p += sizeof(CARD16);
	    strncpy(p, rec->sts_attr.base_font, base_len);
	    sts_attr->value_length = total_len;
	} else if (!strcmp(XNForeground, sts_attr->name)) {
	    sts_attr->value = (void *)malloc(sizeof(long));
	    *(long*)sts_attr->value = rec->sts_attr.foreground;
	    sts_attr->value_length = sizeof(long);
	} else if (!strcmp(XNBackground, sts_attr->name)) {
	    sts_attr->value = (void *)malloc(sizeof(long));
	    *(long*)sts_attr->value = rec->sts_attr.background;
	    sts_attr->value_length = sizeof(long);
	} else if (!strcmp(XNLineSpace, sts_attr->name)) {
	    sts_attr->value = (void *)malloc(sizeof(long));
#if 0
	    *(long*)sts_attr->value = rec->sts_attr.line_space;
#endif
	    *(long*)sts_attr->value = 18;
	    sts_attr->value_length = sizeof(long);
	}
    }
}
