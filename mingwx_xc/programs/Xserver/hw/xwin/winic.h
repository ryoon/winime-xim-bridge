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

typedef struct
{
  XRectangle area;		/* area */
  XRectangle area_needed;	/* area needed */
  XPoint spot_location;		/* spot location */
  Colormap cmap;		/* colormap */
  CARD32 foreground;		/* foreground */
  CARD32 background;		/* background */
  Pixmap bg_pixmap;		/* background pixmap */
  char *base_font;		/* base font of fontset */
  CARD32 line_space;		/* line spacing */
  Cursor cursor;		/* cursor */
} PreeditAttributes;

typedef struct
{
  XRectangle area;		/* area */
  XRectangle area_needed;	/* area needed */
  Colormap cmap;		/* colormap */
  CARD32 foreground;		/* foreground */
  CARD32 background;		/* background */
  Pixmap bg_pixmap;		/* background pixmap */
  char *base_font;		/* base font of fontset */
  CARD32 line_space;		/* line spacing */
  Cursor cursor;		/* cursor */
} StatusAttributes;

typedef struct _IMIC
{
  CARD16 id;			/* ic id */
  INT32 input_style;		/* input style */
  Display *dpy;			/* display */
  Window client_win;		/* client window */
  Window focus_win;		/* focus window */
  char *resource_name;		/* resource name */
  char *resource_class;		/* resource class */
  int context;			/* */
  PreeditAttributes pre_attr;	/* preedit attributes */
  StatusAttributes sts_attr;	/* status attributes */
  int length;
  int caret;
  int toggled;
  IMProtocol pCallData;
  struct _IMIC *next;
} IMIC;

IMIC *FindIMIC (CARD16 icid);

IMIC *FindIMICbyContext (int context);

void CreateIMIC (Display *pDisplay, IMChangeICStruct * pCallData);

void SetIMIC (IMChangeICStruct * pCallData);

void GetIMIC (IMChangeICStruct * pCallData);
