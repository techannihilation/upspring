//
// "$Id: Cursor.cxx 6793 2009-06-22 18:01:01Z yuri $"
//
// Mouse cursor support for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2006 by Bill Spitzak and others.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems to "fltk-bugs@fltk.org".
//

// Change the current cursor.
//
// This needs a lot of work and is commented out for now...
//
// Under X the cursor is attached to the X window.  I tried to hide
// this and pretend that changing the cursor is a drawing function.
// This avoids a field in the Window, and I suspect is more
// portable to other systems.

#include <config.h>
#include <fltk/Cursor.h>
#include <fltk/Window.h>
#include <fltk/x.h>
#if defined(USE_X11) && defined(USE_XCURSOR)
#include <X11/Xcursor/Xcursor.h>
#endif
#include <fltk/draw.h>
#include <fltk/Color.h>
#include <fltk/Image.h>

using namespace fltk;

/*! \fn void Widget::cursor(Cursor* c) const

  Change the cursor being displayed on the screen. A widget should do
  this in response to \c fltk::ENTER and \c fltk::MOVE events. FLTK will change
  it back to \c fltk::CURSOR_DEFAULT if the mouse is moved outside this
  widget, unless another widget calls this.

  On X you can mess with the colors by setting the Color variables
  \c fl_cursor_fg and \c fl_cursor_bg to the colors you want, before calling
  this.
*/

/*! \class fltk::Cursor

  Cursor is an opaque system-dependent class. Currently you can only
  use the built-in cursors but a method to turn an Image into a Cursor
  will be added in the future.

  To display a cursor, call Widget::cursor().

  Built-in cursors are:
  - \c fltk::CURSOR_DEFAULT - the default cursor, usually an arrow.
  - \c fltk::CURSOR_ARROW - up-left arrow pointer
  - \c fltk::CURSOR_CROSS - crosshairs
  - \c fltk::CURSOR_WAIT - watch or hourglass
  - \c fltk::CURSOR_INSERT - I-beam
  - \c fltk::CURSOR_HAND - hand / pointing finger
  - \c fltk::CURSOR_HELP - question mark
  - \c fltk::CURSOR_MOVE - 4-pointed arrow
  - \c fltk::CURSOR_NS - up/down arrow
  - \c fltk::CURSOR_WE - left/right arrow
  - \c fltk::CURSOR_NWSE - diagonal arrow
  - \c fltk::CURSOR_NESW - diagonal arrow
  - \c fltk::CURSOR_NO - circle with slash
  - \c fltk::CURSOR_NONE - invisible
*/

#ifdef __sgi
FL_API Color fl_cursor_fg = RED;
#else
FL_API Color fl_cursor_fg = BLACK;
#endif
FL_API Color fl_cursor_bg = WHITE;

#ifdef DOXYGEN
// don't let it print internal stuff
////////////////////////////////////////////////////////////////
#elif USE_X11

struct fltk::Cursor {
  ::Cursor cursor;
  uchar fontid;
  uchar tableid;
};

FL_API fltk::Cursor *fltk::cursor(void *raw) {
  fltk::Cursor *c = new fltk::Cursor;
  c->cursor = (::Cursor)raw;
  c->fontid = 0;
  c->tableid = 0;
  return c;
}

#ifdef USE_XCURSOR
#warning we assume PixelType = 6 ARGB32 in test it is true for color images
static XcursorImage* create_cursor_image(Image *cimg, int x, int y) {
  XcursorImage *xcimage = XcursorImageCreate(cimg->w(),cimg->h());
  XcursorPixel *dest;

  xcimage->xhot = x;
  xcimage->yhot = y;

  dest = xcimage->pixels;
  unsigned char *src = cimg->buffer();
  //cimg->buffer_pixeltype() 
  for (int j = 0; j < cimg->h() ; ++j){
    for (int i = 0; i < cimg->w() ; ++i){
     
        *dest = (src[3] <<24) | (src[2] << 16) | (src[1] << 8) | src[0];
        src += 4;
        dest++;
    }
  }
  return xcimage;
}

FL_API fltk::Cursor *fltk::cursor(Image *img, int x, int y) {
  fltk::Cursor *c = new fltk::Cursor;
  if(!xdisplay)fltk::open_display();
  img->fetch();
  XcursorImage *xcimage = create_cursor_image(img, x, y);
  c->cursor = XcursorImageLoadCursor(xdisplay, xcimage);
  XcursorImageDestroy (xcimage);
  
  c->fontid = 0;
  c->tableid = 0;
  return c;
}

#endif //USE_XCURSOR

static fltk::Cursor arrow = {0,35};
static fltk::Cursor cross = {0,66};
static fltk::Cursor wait_c = {0,76};
static fltk::Cursor insert = {0,77};
static fltk::Cursor hand = {0,31};
static fltk::Cursor help = {0,47};
static fltk::Cursor move = {0,27};
static fltk::Cursor ns = {0,0,0};
static fltk::Cursor we = {0,0,1};
static fltk::Cursor nwse = {0,0,2};
static fltk::Cursor nesw = {0,0,3};
static fltk::Cursor no = {0,0,4};
static fltk::Cursor none = {0,0,5};

// this probably should be a nicer bitmap:
fltk::Cursor fl_drop_ok_cursor = {0,21};

#define CURSORSIZE 16
#define HOTXY 8
static struct TableEntry {
  uchar bits[CURSORSIZE*CURSORSIZE/8];
  uchar mask[CURSORSIZE*CURSORSIZE/8];
} table[] = {
  {{	// CURSOR_NS
   0x00, 0x00, 0x80, 0x01, 0xc0, 0x03, 0xe0, 0x07, 0x80, 0x01, 0x80, 0x01,
   0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
   0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00},
   {
   0x80, 0x01, 0xc0, 0x03, 0xe0, 0x07, 0xf0, 0x0f, 0xf0, 0x0f, 0xc0, 0x03,
   0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xf0, 0x0f,
   0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01}},
  {{	// CURSOR_EW
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10,
   0x0c, 0x30, 0xfe, 0x7f, 0xfe, 0x7f, 0x0c, 0x30, 0x08, 0x10, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
   {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x1c, 0x38,
   0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0x1c, 0x38, 0x18, 0x18,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
  {{	// CURSOR_NWSE
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x38, 0x00, 0x78, 0x00,
   0xe8, 0x00, 0xc0, 0x01, 0x80, 0x03, 0x00, 0x17, 0x00, 0x1e, 0x00, 0x1c,
   0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
   {
   0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0xfc, 0x00, 0x7c, 0x00, 0xfc, 0x00,
   0xfc, 0x01, 0xec, 0x03, 0xc0, 0x37, 0x80, 0x3f, 0x00, 0x3f, 0x00, 0x3e,
   0x00, 0x3f, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00}},
  {{	// CURSOR_NESW
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x1c, 0x00, 0x1e,
   0x00, 0x17, 0x80, 0x03, 0xc0, 0x01, 0xe8, 0x00, 0x78, 0x00, 0x38, 0x00,
   0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
   {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x3f, 0x00, 0x3e, 0x00, 0x3f,
   0x80, 0x3f, 0xc0, 0x37, 0xec, 0x03, 0xfc, 0x01, 0xfc, 0x00, 0x7c, 0x00,
   0xfc, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00}},
  {{	// CURSOR_NO
   0x00, 0x00, 0xc0, 0x03, 0xf0, 0x0f, 0x38, 0x1c, 0x3c, 0x30, 0x7c, 0x30,
   0xe6, 0x60, 0xc6, 0x61, 0x86, 0x63, 0x06, 0x67, 0x0c, 0x3e, 0x0c, 0x3c,
   0x38, 0x1c, 0xf0, 0x0f, 0xc0, 0x03, 0x00, 0x00},
   {
   0xc0, 0x03, 0xf0, 0x0f, 0xf8, 0x1f, 0xfc, 0x3f, 0x7e, 0x7c, 0xfe, 0x78,
   0xff, 0xf1, 0xef, 0xf3, 0xcf, 0xf7, 0x8f, 0xff, 0x1e, 0x7f, 0x3e, 0x7e,
   0xfc, 0x3f, 0xf8, 0x1f, 0xf0, 0x0f, 0xc0, 0x03}},
  {{0}, {0}} // CURSOR_NONE
};

void Widget::cursor(fltk::Cursor* c) const {
  Window* window = is_window() ? (Window*)this : this->window();
  if (!window) return;
  CreatedWindow* i = CreatedWindow::find(window);
  if (!i) return;
  ::Cursor xcursor;
  if (!c) {
    xcursor = None;
  } else {
    // Figure out the X cursor if not already figured out:
    if (!c->cursor) {
      if (c->fontid) {
	c->cursor = XCreateFontCursor(xdisplay, (c->fontid-1)*2);
      } else {
	TableEntry *q = table+c->tableid;
	XColor dummy;
	Pixmap p = XCreateBitmapFromData(xdisplay,
		RootWindow(xdisplay, xscreen), (const char*)(q->bits),
		CURSORSIZE, CURSORSIZE);
	Pixmap m = XCreateBitmapFromData(xdisplay,
		RootWindow(xdisplay, xscreen), (const char*)(q->mask),
		CURSORSIZE, CURSORSIZE);
	c->cursor = XCreatePixmapCursor(xdisplay, p,m,&dummy, &dummy,
					HOTXY, HOTXY);
	XFreePixmap(xdisplay, m);
	XFreePixmap(xdisplay, p);
      }
      uchar r,g,b;
      XColor fgc;
      split_color(fl_cursor_fg, r,g,b);
      fgc.red = r*0x101;
      fgc.green = g*0x101;
      fgc.blue = b*0x101;
      XColor bgc;
      split_color(fl_cursor_bg, r,g,b);
      bgc.red = r*0x101;
      bgc.green = g*0x101;
      bgc.blue = b*0x101;
      XRecolorCursor(xdisplay, c->cursor, &fgc, &bgc);
    }
    xcursor = c->cursor;
  }
  i->cursor_for = this;
  if (xcursor != i->cursor) {
    i->cursor = xcursor;
    XDefineCursor(xdisplay, i->xid, xcursor);
  }
}

////////////////////////////////////////////////////////////////
#elif defined(_WIN32)

struct fltk::Cursor {
  LPTSTR resource;
  HCURSOR cursor;
};

FL_API fltk::Cursor *fltk::cursor(void *raw) {
  fltk::Cursor *c = new fltk::Cursor;
  c->cursor = (HCURSOR)raw;
  c->resource = 0;
  return c;
}

//thanx to gtk team for reference
//http://www.dotnet247.com/247reference/msgs/13/66301.aspx
#warning we assume PixelType = 6 ARGB32 in test it is true for color images

#include <stdio.h>

static HCURSOR create_cursor_from_image(Image *img, int x, int y)
{
  BITMAPV5HEADER bi;
  HBITMAP hBitmap;
  const int cw = 32, ch = 32; // Hm it seams only one size of cursors for Windows

  ZeroMemory(&bi,sizeof(BITMAPV5HEADER));
  bi.bV5Size = sizeof(BITMAPV5HEADER);
  bi.bV5Width = cw;
  bi.bV5Height = ch;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  // The following mask specification specifies a supported 32 BPP
  // alpha format for Windows XP.
  bi.bV5RedMask = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  HDC hdc;
  hdc = GetDC(NULL);

  // Create the DIB section with an alpha channel.
  void *lpBits;
  hBitmap = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,(void **)&lpBits, NULL, (DWORD)0);

  ReleaseDC(NULL,hdc);

  printf("Img format:%d\n",img->buffer_pixeltype());
  int w = img->w() , h = img->h();
  unsigned char* isrc = img->buffer();
  DWORD *lpdwPixel = (DWORD *)lpBits;
  for (int i=0;i<cw;i++)
    for (int j=0;j<ch;j++)
    {
    fprintf(stderr,"%d %d\n",i,j);
    unsigned char* src = isrc + 4*(j + w*(cw - i -1));
    //*lpdwPixel = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
    *lpdwPixel = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
    lpdwPixel++;
    }

  // Create an empty mask bitmap
  HBITMAP hMonoBitmap = CreateBitmap(cw,cw,1,1,NULL);

  ICONINFO ii;
  ii.fIcon = FALSE; // Change fIcon to TRUE to create an alpha icon
  ii.xHotspot = x;
  ii.yHotspot = y;
  ii.hbmMask = hMonoBitmap;
  ii.hbmColor = hBitmap;

  // Create the alpha cursor with the alpha DIB section
  HCURSOR hAlphaCursor = CreateIconIndirect(&ii);

  DeleteObject(hBitmap);
  DeleteObject(hMonoBitmap);

  return hAlphaCursor;
}

FL_API fltk::Cursor *fltk::cursor(Image *img, int x, int y) {
  img->fetch();
  fltk::Cursor *c = new fltk::Cursor;
  c->cursor = create_cursor_from_image(img, x, y);
  c->resource = 0;
  return c;
}


static fltk::Cursor arrow = {TEXT(IDC_ARROW)};
static fltk::Cursor cross = {TEXT(IDC_CROSS)};
static fltk::Cursor wait_c = {TEXT(IDC_WAIT)};
static fltk::Cursor insert = {TEXT(IDC_IBEAM)};
#ifndef IDC_HAND
# define IDC_HAND IDC_UPARROW
#endif
static fltk::Cursor hand = {TEXT(IDC_HAND)};
static fltk::Cursor help = {TEXT(IDC_HELP)};
static fltk::Cursor move = {TEXT(IDC_SIZEALL)};
static fltk::Cursor ns = {TEXT(IDC_SIZENS)};
static fltk::Cursor we = {TEXT(IDC_SIZEWE)};
static fltk::Cursor nwse = {TEXT(IDC_SIZENWSE)};
static fltk::Cursor nesw = {TEXT(IDC_SIZENESW)};
static fltk::Cursor no = {TEXT(IDC_NO)};
static fltk::Cursor none = {0};

void Widget::cursor(fltk::Cursor* c) const {
  Window* window = is_window() ? (Window*)this : this->window();
  if (!window) return;
  while (window->parent()) window = window->window();
  CreatedWindow* i = CreatedWindow::find(window);
  if (!i) return;
  HCURSOR xcursor;
  if (!c) {
    xcursor = default_cursor;
  } else {
    if (!c->cursor && c->resource) c->cursor = LoadCursor(NULL, c->resource);
    xcursor = c->cursor;
  }
  i->cursor_for = this;
  if (xcursor != i->cursor) {
    i->cursor = xcursor;
    SetCursor(xcursor);
  }
}

////////////////////////////////////////////////////////////////
#elif defined(__APPLE__)

struct fltk::Cursor {
  int resource;
  uchar data[32];
  uchar mask[32];
  Point hotspot;
  ::Cursor* cursor() {return (::Cursor*)data;}
};

static fltk::Cursor hand =
{ 0,
  { 0x06,0x00, 0x09,0x00, 0x09,0x00, 0x09,0x00, 0x09,0xC0, 0x09,0x38, 0x69,0x26, 0x98,0x05,
    0x88,0x01, 0x48,0x01, 0x20,0x02, 0x20,0x02, 0x10,0x04, 0x08,0x04, 0x04,0x08, 0x04,0x08 },
  { 0x06,0x00, 0x0F,0x00, 0x0F,0x00, 0x0F,0x00, 0x0F,0xC0, 0x0F,0xF8, 0x6F,0xFE, 0xFF,0xFF,
    0xFF,0xFF, 0x7F,0xFF, 0x3F,0xFE, 0x3F,0xFE, 0x1F,0xFC, 0x0F,0xFC, 0x07,0xF8, 0x07,0xF8 },
  { 1, 5 } // Hotspot: ( y, x )
};

static fltk::Cursor help =
{ 0,
  { 0x00,0x00, 0x40,0x00, 0x60,0x00, 0x70,0x00, 0x78,0x3C, 0x7C,0x7E, 0x7E,0x66, 0x7F,0x06,
    0x7F,0x8C, 0x7C,0x18, 0x6C,0x18, 0x46,0x00, 0x06,0x18, 0x03,0x18, 0x03,0x00, 0x00,0x00 },
  { 0xC0,0x00, 0xE0,0x00, 0xF0,0x00, 0xF8,0x3C, 0xFC,0x7E, 0xFE,0xFF, 0xFF,0xFF, 0xFF,0xFF,
    0xFF,0xFE, 0xFF,0xFC, 0xFE,0x3C, 0xEF,0x3C, 0xCF,0x3C, 0x07,0xBC, 0x07,0x98, 0x03,0x80 },
  { 1, 1 }
};

static fltk::Cursor move =
{ 0,
  { 0x00,0x00, 0x01,0x80, 0x03,0xC0, 0x07,0xE0, 0x07,0xE0, 0x19,0x98, 0x39,0x9C, 0x7F,0xFE,
    0x7F,0xFE, 0x39,0x9C, 0x19,0x98, 0x07,0xE0, 0x07,0xE0, 0x03,0xC0, 0x01,0x80, 0x00,0x00 },
  { 0x01,0x80, 0x03,0xC0, 0x07,0xE0, 0x0F,0xF0, 0x1F,0xF8, 0x3F,0xFC, 0x7F,0xFE, 0xFF,0xFF,
    0xFF,0xFF, 0x7F,0xFE, 0x3F,0xFC, 0x1F,0xF8, 0x0F,0xF0, 0x07,0xE0, 0x03,0xC0, 0x01,0x80 },
  { 8, 8 }
};

static fltk::Cursor ns =
{ 0,
  { 0x00,0x00, 0x01,0x80, 0x03,0xC0, 0x07,0xE0, 0x0F,0xF0, 0x01,0x80, 0x01,0x80, 0x01,0x80,
    0x01,0x80, 0x01,0x80, 0x01,0x80, 0x0F,0xF0, 0x07,0xE0, 0x03,0xC0, 0x01,0x80, 0x00,0x00 },
  { 0x01,0x80, 0x03,0xC0, 0x07,0xE0, 0x0F,0xF0, 0x1F,0xF8, 0x1F,0xF8, 0x03,0xC0, 0x03,0xC0,
    0x03,0xC0, 0x03,0xC0, 0x1F,0xF8, 0x1F,0xF8, 0x0F,0xF0, 0x07,0xE0, 0x03,0xC0, 0x01,0x80 },
  { 8, 8 }
};

static fltk::Cursor we =
{ 0,
  { 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x08,0x10, 0x18,0x18, 0x38,0x1C, 0x7F,0xFE,
    0x7F,0xFE, 0x38,0x1C, 0x18,0x18, 0x08,0x10, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x0C,0x30, 0x1C,0x38, 0x3C,0x3C, 0x7F,0xFE, 0xFF,0xFF,
    0xFF,0xFF, 0x7F,0xFE, 0x3C,0x3C, 0x1C,0x38, 0x0C,0x30, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 8, 8 }
};

static fltk::Cursor nwse =
{ 0,
  { 0x00,0x00, 0x7E,0x00, 0x7C,0x00, 0x78,0x00, 0x7C,0x00, 0x6E,0x00, 0x47,0x10, 0x03,0xB0,
    0x01,0xF0, 0x00,0xF0, 0x01,0xF0, 0x03,0xF0, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 0xFF,0x00, 0xFF,0x00, 0xFE,0x00, 0xFC,0x00, 0xFE,0x00, 0xFF,0x18, 0xEF,0xB8, 0xC7,0xF8,
    0x03,0xF8, 0x01,0xF8, 0x03,0xF8, 0x07,0xF8, 0x07,0xF8, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 8, 8 }
};

static fltk::Cursor nesw =
{ 0,
  { 0x00,0x00, 0x03,0xF0, 0x01,0xF0, 0x00,0xF0, 0x01,0xF0, 0x03,0xB0, 0x47,0x10, 0x6E,0x00,
    0x7C,0x00, 0x78,0x00, 0x7C,0x00, 0x7E,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 0x07,0xF8, 0x07,0xF8, 0x03,0xF8, 0x01,0xF8, 0x03,0xF8, 0xC7,0xF8, 0xEF,0xB8, 0xFF,0x18,
    0xFE,0x00, 0xFC,0x00, 0xFE,0x00, 0xFF,0x00, 0xFF,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 8, 8 }
};

static fltk::Cursor none =
{ 0,
  { 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00 },
  { 0, 0 }
};

static fltk::Cursor arrow =
{ 0,
  { 0x00,0x00, 0x40,0x00, 0x60,0x00, 0x70,0x00, 0x78,0x00, 0x7C,0x00, 0x7E,0x00, 0x7F,0x00,
    0x7F,0x80, 0x7C,0x00, 0x6C,0x00, 0x46,0x00, 0x06,0x00, 0x03,0x00, 0x03,0x00, 0x00,0x00 },
  { 0xC0,0x00, 0xE0,0x00, 0xF0,0x00, 0xF8,0x00, 0xFC,0x00, 0xFE,0x00, 0xFF,0x00, 0xFF,0x80,
    0xFF,0xC0, 0xFF,0xC0, 0xFE,0x00, 0xEF,0x00, 0xCF,0x00, 0x07,0x80, 0x07,0x80, 0x03,0x80 },
  { 1, 1 }
};

static fltk::Cursor cross = {crossCursor};
static fltk::Cursor wait_c  = {watchCursor};
static fltk::Cursor insert= {iBeamCursor};

static fltk::Cursor no =
{ 0,
  { 0x00,0x00, 0x03,0xc0, 0x0f,0xf0, 0x1c,0x38, 0x30,0x3c, 0x30,0x7c, 0x60,0xe6, 0x61,0xc6,
    0x63,0x86, 0x67,0x06, 0x3e,0x0c, 0x3c,0x0c, 0x1c,0x38, 0x0f,0xf0, 0x03,0xc0, 0x00,0x00 },
  { 0x03,0xc0, 0x0f,0xf0, 0x1f,0xf8, 0x3f,0xfc, 0x7c,0x7e, 0x78,0xfe, 0xf1,0xff, 0xf3,0xef,
    0xf7,0xcf, 0xff,0x8f, 0x7f,0x1e, 0x7e,0x3e, 0x3f,0xfc, 0x1f,0xf8, 0x0f,0xf0, 0x03,0xc0 },
  { 8, 8 }
};

/*  Setting the cursor on the Macintosh is very easy, as there is a
    single cursor rather than one per window. The main loop checks to
    see if we are still pointing at the cursor widget and puts the
    cursor back to the default if not. */
void Widget::cursor(fltk::Cursor* c) const {
  CursPtr xcursor;
  if (!c) {
    xcursor = fltk::default_cursor;
  } else if (c->resource) {
    xcursor = *GetCursor(c->resource);
  } else {
    xcursor = c->cursor();
  }
  fltk::cursor_for = this;
  if (fltk::current_cursor != xcursor) {
    fltk::current_cursor = xcursor;
    SetCursor(xcursor);
  }
}

// FC: check me ! bill don't commit unfinished & non compiling work on the trunk please !
// WAS: I'm going to guess that "raw" is an OS9 "Cursor". The problem is
// that the structure itself did not port to Intel, as it is defined with
// 16-bit shorts but apparently the system api wants the bytes the same,
// probably because they expect it to be resource data.
FL_API fltk::Cursor *fltk::cursor(void *raw) {
  fltk::Cursor *c = new fltk::Cursor;
  c->resource=0;
  if(raw) {
     // This overwrites the data, mask, and hotspot:
     memcpy(c->data, raw, sizeof(::Cursor));
  } else {
     memset(c->data, 0, sizeof(::Cursor));
  }
  return c;
}

#endif

fltk::Cursor* const fltk::CURSOR_DEFAULT= 0;
fltk::Cursor* const fltk::CURSOR_ARROW	= &arrow;
fltk::Cursor* const fltk::CURSOR_CROSS	= &cross;
fltk::Cursor* const fltk::CURSOR_WAIT	= &wait_c;
fltk::Cursor* const fltk::CURSOR_INSERT	= &insert;
fltk::Cursor* const fltk::CURSOR_HAND	= &hand;
fltk::Cursor* const fltk::CURSOR_HELP	= &help;
fltk::Cursor* const fltk::CURSOR_MOVE	= &move;
fltk::Cursor* const fltk::CURSOR_NS	= &ns;
fltk::Cursor* const fltk::CURSOR_WE	= &we;
fltk::Cursor* const fltk::CURSOR_NWSE	= &nwse;
fltk::Cursor* const fltk::CURSOR_NESW	= &nesw;
fltk::Cursor* const fltk::CURSOR_NO	= &no;
fltk::Cursor* const fltk::CURSOR_NONE	= &none;

//
// End of "$Id: Cursor.cxx 6793 2009-06-22 18:01:01Z yuri $".
//
