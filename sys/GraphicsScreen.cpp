/* GraphicsScreen.cpp
 *
 * Copyright (C) 1992-2012,2014,2015 Paul Boersma, 2013 Tom Naughton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 2002/05/28 GPL
 * pb 2004/10/18 ReleaseDC
 * pb 2007/08/01 reintroduced yIsZeroAtTheTop
 * sdk 2008/03/24 cairo
 * sdk 2008/05/09 cairo
 * pb 2009/07/24 quartz
 * fb 2010/02/23 cairo & clipping on updateWs()
 * pb 2010/05/13 support XOR mode
 * pb 2010/07/13 split erasure of recording off from Graphics_clearWs
 * pb 2011/03/17 C++
 */

#include "GraphicsP.h"
#include "Printer.h"
#include "GuiP.h"

#if gtk
	#include <cairo/cairo-pdf.h>
#elif win
	//#include "winport_on.h"
	#include <gdiplus.h>
	//#include "winport_off.h"
	//using namespace Gdiplus;
	static bool _GraphicsWindows_tryToInitializeGdiPlus () {
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		GdiplusStartup (& gdiplusToken, & gdiplusStartupInput, nullptr);
		return true;
	}
#elif mac
	#include "macport_on.h"
	static RGBColor theBlackColour = { 0, 0, 0 };
	static bool _GraphicsMacintosh_tryToInitializeQuartz () {
		return _GraphicsMac_tryToInitializeFonts ();
	}
#endif

Thing_implement (GraphicsScreen, Graphics, 0);

void structGraphicsScreen :: v_destroy () {
	#if cairo
		#if ALLOW_GDK_DRAWING
			if (d_gdkGraphicsContext) {
				g_object_unref (d_gdkGraphicsContext);			
				d_gdkGraphicsContext = nullptr;
			}
		#endif
		if (d_cairoGraphicsContext) {
			cairo_destroy (d_cairoGraphicsContext);
			d_cairoGraphicsContext = nullptr;
		}
		if (d_cairoSurface) {
			cairo_surface_flush (d_cairoSurface);
			if (d_isPng) {
				#if 1
					cairo_surface_write_to_png (d_cairoSurface, Melder_peek32to8 (d_file. path));
				#else
					unsigned char *bitmap = cairo_image_surface_get_data (my d_cairoSurface);   // peeking into the internal bits
					// copy bitmap to PNG structure created with the PNG library
					// save the PNG tructure to a file
				#endif
			}
			cairo_surface_destroy (d_cairoSurface);
		}
	#elif win
		if (d_gdiGraphicsContext) {
			SelectPen (d_gdiGraphicsContext, GetStockPen (BLACK_PEN));
			SelectBrush (d_gdiGraphicsContext, GetStockBrush (NULL_BRUSH));
		}
		if (d_winPen) {
			DeleteObject (d_winPen);
			d_winPen = nullptr;
		}
		if (d_winBrush) {
			DeleteObject (d_winBrush);
			d_winBrush = nullptr;
		}
		if (d_isPng && d_gdiBitmap) {
			trace (U"saving the filled bitmap to a PNG file");
			/*
			 * Deselect the bitmap from the device context (otherwise GetDIBits won't work).
			 */
			//SelectBitmap (d_gdiGraphicsContext, nullptr);
			//SelectBitmap (d_gdiGraphicsContext, CreateCompatibleBitmap (nullptr, 1, 1));

			BITMAP bitmap;
			GetObject (d_gdiBitmap, sizeof (BITMAP), & bitmap);
			int width = bitmap. bmWidth, height = bitmap. bmHeight;
			//int width = 3600, height = 3600;
			trace (U"width ", width, U", height ", height);

			/*
			 * Get the bits from the HBITMAP;
			 */
			struct { BITMAPINFOHEADER header; } bitmapInfo;
			bitmapInfo. header.biSize = sizeof (BITMAPINFOHEADER);
			bitmapInfo. header.biWidth = width;
			bitmapInfo. header.biHeight = height;
			bitmapInfo. header.biPlanes = 1;
			bitmapInfo. header.biBitCount = 32;
			bitmapInfo. header.biCompression = BI_RGB;
			bitmapInfo. header.biSizeImage = 0;
			bitmapInfo. header.biXPelsPerMeter = 0;
			bitmapInfo. header.biYPelsPerMeter = 0;
			bitmapInfo. header.biClrUsed = 0;
			bitmapInfo. header.biClrImportant = 0;
			unsigned char *bits = Melder_calloc (unsigned char, 4 * width * height);
			//HANDLE handle = GlobalAlloc (GHND, 4 * width * height);
			//unsigned char *bits = (unsigned char *) GlobalLock (handle);
			int numberOfLinesScanned = GetDIBits (GetDC (nullptr), d_gdiBitmap, 0, height, bits, (BITMAPINFO *) & bitmapInfo, DIB_RGB_COLORS);
			trace (numberOfLinesScanned, U" lines scanned");

			trace (U"creating a savable bitmap");

			//Gdiplus::Bitmap gdiplusBitmap (width, height, PixelFormat32bppARGB);
			Gdiplus::Bitmap gdiplusBitmap ((BITMAPINFO *) & bitmapInfo, bits);
			gdiplusBitmap. SetResolution (resolution, resolution);

			trace (U"copying the device-independent bits to the savable bitmap.");

			/*
			for (long irow = 1; irow <= height; irow ++) {
				for (long icol = 1; icol <= width; icol ++) {
					unsigned char blue = *bits ++, green = *bits ++, red = *bits ++, alpha = 255 - *bits ++;
					Gdiplus::Color gdiplusColour (alpha, red, green, blue);
					gdiplusBitmap. SetPixel (icol - 1, height - irow, gdiplusColour);
				}
			}
			*/

			trace (U"saving");

			UINT numberOfImageEncoders, sizeOfImageEncoderArray;
			Gdiplus::GetImageEncodersSize (& numberOfImageEncoders, & sizeOfImageEncoderArray);
			Gdiplus::ImageCodecInfo *imageEncoderInfos = Melder_malloc (Gdiplus::ImageCodecInfo, sizeOfImageEncoderArray);
			Gdiplus::GetImageEncoders (numberOfImageEncoders, sizeOfImageEncoderArray, imageEncoderInfos);
			for (int iencoder = 0; iencoder < numberOfImageEncoders; iencoder ++) {
				if (! wcscmp (imageEncoderInfos [iencoder]. MimeType, L"image/png")) {
					gdiplusBitmap. Save (Melder_peek32toW (our d_file. path), & imageEncoderInfos [iencoder]. Clsid, nullptr);
				}
			}

			trace (U"cleaning up");

			Melder_free (imageEncoderInfos);
			//bits -= 4 * width * height;
			Melder_free (bits);
			//GlobalUnlock (handle);
			//GlobalFree (handle);
			DeleteObject (d_gdiBitmap);
			Melder_assert (d_gdiGraphicsContext);
			DeleteDC (d_gdiGraphicsContext);   // this was a memory leak before 5.3.83
		}
		/*
		 * No ReleaseDC here, because we have not created it ourselves,
		 * not even with GetDC. Is this a BUG?
		 */
		d_gdiGraphicsContext = nullptr;
	#elif mac
        #if useCarbon
            if (! d_macPort && ! d_isPng) {
                CGContextEndPage (d_macGraphicsContext);
                CGContextRelease (d_macGraphicsContext);
            }
        #else
            if (! d_macView && ! d_isPng) {
                CGContextEndPage (d_macGraphicsContext);
                CGContextRelease (d_macGraphicsContext);
            }
        #endif
		if (d_isPng && d_macGraphicsContext) {
			/*
			 * Turn the offscreen bitmap into an image.
			 */
			CGImageRef image = CGBitmapContextCreateImage (d_macGraphicsContext);
			Melder_assert (image);
			//CGContextRelease (d_macGraphicsContext);
			/*
			 * Create a dictionary with resolution properties.
			 */
			CFTypeRef keys [2], values [2];
			keys [0] = kCGImagePropertyDPIWidth;
			keys [1] = kCGImagePropertyDPIHeight;
			float resolution_float = resolution;
			values [1] = values [0] = CFNumberCreate (nullptr, kCFNumberFloatType, & resolution_float);
			CFDictionaryRef properties = CFDictionaryCreate (nullptr,
				(const void **) keys, (const void **) values, 2,
				& kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
			Melder_assert (properties);
			/*
			 */
			CFURLRef url = CFURLCreateWithFileSystemPath (nullptr,
				(CFStringRef) Melder_peek32toCfstring (d_file. path), kCFURLPOSIXPathStyle, false);
			CGImageDestinationRef imageDestination = CGImageDestinationCreateWithURL (url, kUTTypePNG, 1, nullptr);
			if (imageDestination) {
				CGImageDestinationAddImage (imageDestination, image, properties);
				CGImageDestinationFinalize (imageDestination);
				CFRelease (imageDestination);
			}
			CGImageRelease (image);
			CFRelease (properties);
			CFRelease (url);
		}
		Melder_free (d_bits);
	#endif
	trace (U"destroying parent");
	GraphicsScreen_Parent :: v_destroy ();
	trace (U"exit");
}

void structGraphicsScreen :: v_flushWs () {
	#if cairo
		// Ik weet niet of dit is wat het zou moeten zijn ;)
		//gdk_window_process_updates (d_window, true);   // this "works" but is incorrect because it's not the expose events that have to be carried out
		//gdk_window_flush (d_window);
		gdk_flush ();
		// TODO: een aanroep die de eventuele grafische buffer ledigt,
		// zodat de gebruiker de grafica ziet ook al blijft Praat in hetzelfde event zitten
	#elif cocoa
		if (d_drawingArea) {
			GuiShell shell = d_drawingArea -> d_shell;
			Melder_assert (shell);
        	[shell -> d_cocoaWindow   flushWindow];
		}
	#elif win
		/*GdiFlush ();*/
	#elif mac
		if (d_drawingArea) {
			GuiShell shell = d_drawingArea -> d_shell;
			Melder_assert (shell);
			GuiShell_drain (shell);
		}
	#endif
}

void Graphics_flushWs (Graphics me) {
	my v_flushWs ();
}

void structGraphicsScreen :: v_clearWs () {
	#if cairo
		GdkRectangle rect;
		if (this -> d_x1DC < this -> d_x2DC) {
			rect.x = this -> d_x1DC;
			rect.width = this -> d_x2DC - this -> d_x1DC;
		} else {
			rect.x = this -> d_x2DC;
			rect.width = this -> d_x1DC - this -> d_x2DC;
		}
		if (this -> d_y1DC < this -> d_y2DC) {
			rect.y = this -> d_y1DC;
			rect.height = this -> d_y2DC - this -> d_y1DC;
		} else {
			rect.y = this -> d_y2DC;
			rect.height = this -> d_y1DC - this -> d_y2DC;
		}
		if (! d_cairoGraphicsContext) {
			trace (U"clear and null");
			//gdk_window_clear (this -> window);
			//gdk_window_invalidate_rect (this -> window, & rect, true);   // BUG: it seems weird that this is necessary.
		} else {
			trace (U"clear and not null");
			cairo_set_source_rgb (d_cairoGraphicsContext, 1.0, 1.0, 1.0);
			cairo_rectangle (d_cairoGraphicsContext, rect.x, rect.y, rect.width, rect.height);
			cairo_fill (d_cairoGraphicsContext);
			cairo_set_source_rgb (d_cairoGraphicsContext, 0.0, 0.0, 0.0);
		}
	#elif cocoa
        GuiCocoaDrawingArea *cocoaDrawingArea = (GuiCocoaDrawingArea *) d_drawingArea -> d_widget;
        if (cocoaDrawingArea) {
            NSRect rect;
            if (this -> d_x1DC < this -> d_x2DC) {
                rect.origin.x = this -> d_x1DC;
                rect.size.width = this -> d_x2DC - this -> d_x1DC;
            } else {
                rect.origin.x = this -> d_x2DC;
                rect.size.width = this -> d_x1DC - this -> d_x2DC;
            }
            if (this -> d_y1DC < this -> d_y2DC) {
                rect.origin.y = this -> d_y1DC;
                rect.size.height = this -> d_y2DC - this -> d_y1DC;
            } else {
                rect.origin.y = this -> d_y2DC;
                rect.size.height = this -> d_y1DC - this -> d_y2DC;
            }
			[cocoaDrawingArea lockFocus];
            CGContextRef context = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
            CGContextSaveGState (context);
            CGContextSetAlpha (context, 1.0);
            CGContextSetBlendMode (context, kCGBlendModeNormal);
            CGContextSetRGBFillColor (context, 1.0, 1.0, 1.0, 1.0);
			//rect.origin.x -= 1000;
			//rect.origin.y -= 1000;
			//rect.size.width += 2000;
			//rect.size.height += 2000;
			trace (U"clearing ", rect.origin.x, U" ", rect.origin.y, U" ", rect.size.width, U" ", rect.size.height);
                //CGContextTranslateCTM (context, 0, cocoaDrawingArea.bounds.size.height);
                //CGContextScaleCTM (context, 1.0, -1.0);
            CGContextFillRect (context, rect);
            //CGContextSynchronize (context);
            CGContextRestoreGState (context);
			[cocoaDrawingArea unlockFocus];
			[cocoaDrawingArea setNeedsDisplay: YES];
        }
	#elif win
		RECT rect;
		rect. left = rect. top = 0;
		rect. right = d_x2DC - d_x1DC;
		rect. bottom = d_y2DC - d_y1DC;
		FillRect (d_gdiGraphicsContext, & rect, GetStockBrush (WHITE_BRUSH));
		/*if (d_winWindow) SendMessage (d_winWindow, WM_ERASEBKGND, (WPARAM) d_gdiGraphicsContext, 0);*/
	#elif mac
		QDBeginCGContext (d_macPort, & d_macGraphicsContext);
		CGContextSetAlpha (d_macGraphicsContext, 1.0);
		CGContextSetBlendMode (d_macGraphicsContext, kCGBlendModeNormal);
		//CGContextSetAllowsAntialiasing (my macGraphicsContext, false);
		int shellHeight = GuiMac_clipOn_graphicsContext (d_drawingArea -> d_widget, d_macGraphicsContext);
		CGContextSetRGBFillColor (d_macGraphicsContext, 1.0, 1.0, 1.0, 1.0);
		CGContextFillRect (d_macGraphicsContext, CGRectMake (this -> d_x1DC, shellHeight - this -> d_y1DC, this -> d_x2DC - this -> d_x1DC, this -> d_y1DC - this -> d_y2DC));
		QDEndCGContext (d_macPort, & d_macGraphicsContext);
	#endif
}

void Graphics_clearWs (Graphics me) {
	my v_clearWs ();
}

void structGraphicsScreen :: v_updateWs () {
	/*
	 * A function that invalidates the graphics.
	 * This function is typically called by the owner of the drawing area
	 * whenever the data to be displayed in the drawing area has changed;
	 * the idea is to generate an expose event to which the drawing area will
	 * respond by redrawing its contents from the (changed) data.
	 */
	#if gtk
		//GdkWindow *window = gtk_widget_get_parent_window (GTK_WIDGET (my drawingArea -> d_widget));
		GdkRectangle rect;

		if (this -> d_x1DC < this -> d_x2DC) {
			rect.x = this -> d_x1DC;
			rect.width = this -> d_x2DC - this -> d_x1DC;
		} else {
			rect.x = this -> d_x2DC;
			rect.width = this -> d_x1DC - this -> d_x2DC;
		}

		if (this -> d_y1DC < this -> d_y2DC) {
			rect.y = this -> d_y1DC;
			rect.height = this -> d_y2DC - this -> d_y1DC;
		} else {
			rect.y = this -> d_y2DC;
			rect.height = this -> d_y1DC - this -> d_y2DC;
		}

		if (d_cairoGraphicsContext && d_drawingArea) {  // update clipping rectangle to new graphics size
			cairo_reset_clip (d_cairoGraphicsContext);
			cairo_rectangle (d_cairoGraphicsContext, rect.x, rect.y, rect.width, rect.height);
			cairo_clip (d_cairoGraphicsContext);
		}
		#if ALLOW_GDK_DRAWING
			gdk_window_clear (d_window);
		#endif
		gdk_window_invalidate_rect (d_window, & rect, true);
		//gdk_window_process_updates (d_window, true);
	#elif cocoa
        NSView *view =  d_macView;
        NSRect rect;
    
        if (this -> d_x1DC < this -> d_x2DC) {
            rect.origin.x = this -> d_x1DC;
            rect.size.width = this -> d_x2DC - this -> d_x1DC;
        } else {
            rect.origin.x = this -> d_x2DC;
            rect.size.width = this -> d_x1DC - this -> d_x2DC;
        }
        
        if (this -> d_y1DC < this -> d_y2DC) {
            rect.origin.y = this -> d_y1DC;
            rect.size.height = this -> d_y2DC - this -> d_y1DC;
        } else {
            rect.origin.y = this -> d_y2DC;
            rect.size.height = this -> d_y1DC - this -> d_y2DC;
        }
    
        //[view setNeedsDisplayInRect: rect];
        [view setNeedsDisplay: YES];
    
	#elif win
		//clear (this); // lll
		if (d_winWindow) InvalidateRect (d_winWindow, nullptr, true);
	#elif mac
		Rect r;
		if (d_drawingArea) GuiMac_clipOn (d_drawingArea -> d_widget);   // to prevent invalidating invisible parts of the canvas
		SetRect (& r, this -> d_x1DC, this -> d_y1DC, this -> d_x2DC, this -> d_y2DC);
		SetPort (d_macPort);
		/*EraseRect (& r);*/
		InvalWindowRect (GetWindowFromPort ((CGrafPtr) d_macPort), & r);
		if (d_drawingArea) GuiMac_clipOff ();
	#endif
}

void Graphics_updateWs (Graphics me) {
	if (me)
		my v_updateWs ();
}

static int GraphicsScreen_init (GraphicsScreen me, void *voidDisplay, void *voidWindow) {

	/* Fill in new members. */

	#if cairo
		my d_display = (GdkDisplay *) gdk_display_get_default ();
		_GraphicsScreen_text_init (me);
		#if ALLOW_GDK_DRAWING
			trace (U"retrieving window");
			my d_window = GDK_DRAWABLE (GTK_WIDGET (voidDisplay) -> window);
			trace (U"retrieved window");
			my d_gdkGraphicsContext = gdk_gc_new (my d_window);
		#else
			my d_window = gtk_widget_get_window (GTK_WIDGET (voidDisplay));
		#endif
		my d_cairoGraphicsContext = gdk_cairo_create (my d_window);
	#elif win
		if (my printer) {
			my d_gdiGraphicsContext = (HDC) voidWindow;
		} else if (voidDisplay) {
			my d_gdiGraphicsContext = (HDC) voidDisplay;
			my metafile = true;
		} else {
			my d_winWindow = (HWND) voidWindow;
			my d_gdiGraphicsContext = GetDC (my d_winWindow);   // window must have a constant display context; see XtInitialize ()
		}
		Melder_assert (my d_gdiGraphicsContext);
		SetBkMode (my d_gdiGraphicsContext, TRANSPARENT);   // not the default!
		/*
		 * Create pens and brushes.
		 */
		my d_winPen = CreatePen (PS_SOLID, 0, RGB (0, 0, 0));
		my d_winBrush = CreateSolidBrush (RGB (0, 0, 0));
		SelectBrush (my d_gdiGraphicsContext, GetStockBrush (NULL_BRUSH));
		SetTextAlign (my d_gdiGraphicsContext, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);   // baseline is not the default!
		_GraphicsScreen_text_init (me);
	#elif mac
		(void) voidDisplay;
        #if useCarbon
            my d_macPort = (GrafPtr) voidWindow;
			(void) my d_macGraphicsContext;   // will be retrieved from QuickDraw with every drawing command!
        #else
			if (my printer) {
				my d_macView = (NSView *) voidWindow;   // in case we do view-based printing
				//my d_macGraphicsContext = (CGContextRef) voidWindow;   // in case we do context-based printing
			} else {
				my d_macView = (NSView *) voidWindow;
				(void) my d_macGraphicsContext;   // will be retrieved from Core Graphics with every drawing command!
			}
        #endif
		my d_macColour = theBlackColour;
		my d_depth = my resolution > 150 ? 1 : 8;   /* BUG: replace by true depth (1=black/white) */
		_GraphicsScreen_text_init (me);
	#endif
	return 1;
}

Graphics Graphics_create_screen (void *display, void *window, int resolution) {
	autoGraphicsScreen me = Thing_new (GraphicsScreen);
	my screen = true;
	#if win
		my d_useGdiplus = _GraphicsWindows_tryToInitializeGdiPlus ();
	#elif mac
		_GraphicsMacintosh_tryToInitializeQuartz ();
	#endif
	my yIsZeroAtTheTop = true;
	Graphics_init (me.get(), resolution);
	Graphics_setWsViewport (me.get(), 0.0, 100.0, 0.0, 100.0);
	#if mac && useCarbon
		GraphicsScreen_init (me.get(), display, GetWindowPort ((WindowRef) window));
	#else
		GraphicsScreen_init (me.get(), display, window);
	#endif
	return me.transfer();
}

Graphics Graphics_create_screenPrinter (void *display, void *window) {
	autoGraphicsScreen me = Thing_new (GraphicsScreen);
	my screen = true;
	my yIsZeroAtTheTop = true;
	my printer = true;
	#if win
		my d_useGdiplus = _GraphicsWindows_tryToInitializeGdiPlus ();
	#elif mac
		_GraphicsMacintosh_tryToInitializeQuartz ();
	#endif
	Graphics_init (me.get(), thePrinter. resolution);
	my paperWidth = (double) thePrinter. paperWidth / thePrinter. resolution;
	my paperHeight = (double) thePrinter. paperHeight / thePrinter. resolution;
	//NSLog (@"Graphics_create_screenPrinter: %f %f", my paperWidth, my paperHeight);
	my d_x1DC = my d_x1DCmin = thePrinter. resolution / 2;
	my d_x2DC = my d_x2DCmax = (my paperWidth - 0.5) * thePrinter. resolution;
	my d_y1DC = my d_y1DCmin = thePrinter. resolution / 2;
	my d_y2DC = my d_y2DCmax = (my paperHeight - 0.5) * thePrinter. resolution;
	#if win
		/*
		 * Map page coordinates to paper coordinates.
		 */
		my d_x1DC -= GetDeviceCaps ((HDC) window, PHYSICALOFFSETX);
		my d_x2DC -= GetDeviceCaps ((HDC) window, PHYSICALOFFSETX);
		my d_y1DC -= GetDeviceCaps ((HDC) window, PHYSICALOFFSETY);
		my d_y2DC -= GetDeviceCaps ((HDC) window, PHYSICALOFFSETY);
	#endif
	Graphics_setWsWindow (me.get(), 0, my paperWidth - 1.0, 13.0 - my paperHeight, 12.0);
	GraphicsScreen_init (me.get(), display, window);
	return me.transfer();
}

#if mac && useCarbon
/* Drawing areas support resize callbacks.
 * However, Mac drawing areas also support move callbacks.
 * This is the only way for a graphics driver to get notified of a move,
 * which would bring about a change in device coordinates.
 * On Xwin and Win, we are not interested in moves, because we draw in widget coordinates.
 */
static void cb_move (GuiObject w, XtPointer void_me, XtPointer call) {
	iam (GraphicsScreen);
	Dimension width, height;
	XtVaGetValues (w, XmNwidth, & width, XmNheight, & height, nullptr);

	/* The four values returned are probably equal to the previous ones.
	 * However, the following call forces a new computation of the device coordinates
	 * by widgetToWindowCoordinates ().
	 */

	Graphics_setWsViewport ((Graphics) me, 0 /* Left x value in widget coordinates */,
		width, 0, height);
	Graphics_updateWs ((Graphics) me);
}
#endif

Graphics Graphics_create_xmdrawingarea (GuiDrawingArea w) {
	trace (U"begin");
	autoGraphicsScreen me = Thing_new (GraphicsScreen);
	#if gtk
		GtkRequisition realsize;
		GtkAllocation allocation;
	#elif motif
		Dimension width, height;
	#endif

	my d_drawingArea = static_cast <GuiDrawingArea> (w);   /* Now !!!!!!!!!! */
	Melder_assert (my d_drawingArea -> d_widget);
	my screen = true;
	my yIsZeroAtTheTop = true;
	#if win
		my d_useGdiplus = _GraphicsWindows_tryToInitializeGdiPlus ();
	#elif mac
		_GraphicsMacintosh_tryToInitializeQuartz ();
	#endif
	#if mac
		Graphics_init (me.get(), Gui_getResolution (nullptr));
		#if useCarbon
            GraphicsScreen_init (me.get(),
                XtDisplay (my d_drawingArea -> d_widget),
                GetWindowPort ((WindowRef) XtWindow (my d_drawingArea -> d_widget)));
		#else
            GraphicsScreen_init (me.get(),
                                 my d_drawingArea -> d_widget,
                                 my d_drawingArea -> d_widget);
		#endif
	#else
		#if gtk
			Graphics_init (me.get(), Gui_getResolution (my d_drawingArea -> d_widget));
			GraphicsScreen_init (me, GTK_WIDGET (my d_drawingArea -> d_widget), GTK_WIDGET (my d_drawingArea -> d_widget));
		#elif motif
			Graphics_init (me.get(), Gui_getResolution (my d_drawingArea -> d_widget));
			GraphicsScreen_init (me, XtDisplay (my d_drawingArea -> d_widget), XtWindow (my d_drawingArea -> d_widget));
		#endif
	#endif

	#if gtk
		// fb: is really the request meant or rather the actual size, aka allocation?
		gtk_widget_size_request (GTK_WIDGET (my d_drawingArea -> d_widget), & realsize);
		gtk_widget_get_allocation (GTK_WIDGET (my d_drawingArea -> d_widget), & allocation);
		// HIER WAS IK
		trace (U"requested ", realsize.width, U" x ", realsize.height, U", allocated ", allocation.width, U" x ", allocation.height);
		Graphics_setWsViewport (me.get(), 0.0, realsize.width, 0.0, realsize.height);
	#elif motif
		XtVaGetValues (my d_drawingArea -> d_widget, XmNwidth, & width, XmNheight, & height, nullptr);
		Graphics_setWsViewport (me.get(), 0.0, width, 0.0, height);
    #elif cocoa
        NSView *view = (NSView *)my d_drawingArea -> d_widget;
        NSRect bounds = [view bounds];
        Graphics_setWsViewport (me.get(), 0.0, bounds.size.width, 0, bounds.size.height);
	#endif
	#if mac && useCarbon
		XtAddCallback (my d_drawingArea -> d_widget, XmNmoveCallback, cb_move, (XtPointer) me);
	#endif
	return me.transfer();
}

Graphics Graphics_create_pngfile (MelderFile file, int resolution,
	double x1inches, double x2inches, double y1inches, double y2inches)
{
	autoGraphicsScreen me = Thing_new (GraphicsScreen);
	my screen = true;
	my yIsZeroAtTheTop = true;
	#if win
		my d_useGdiplus = _GraphicsWindows_tryToInitializeGdiPlus ();
	#elif mac
		_GraphicsMacintosh_tryToInitializeQuartz ();
	#endif
	Graphics_init (me.peek(), resolution);
	my d_isPng = true;
	my d_file = *file;
	my d_x1DC = my d_x1DCmin = 0;
	my d_x2DC = my d_x2DCmax = (x2inches - x1inches) * resolution;
	my d_y1DC = my d_y1DCmin = 0;
	my d_y2DC = my d_y2DCmax = (y2inches - y1inches) * resolution;
	Graphics_setWsWindow ((Graphics) me.peek(), x1inches, x2inches, y1inches, y2inches);
	#if gtk
		my d_cairoSurface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
			(x2inches - x1inches) * resolution, (y2inches - y1inches) * resolution);
		my d_cairoGraphicsContext = cairo_create (my d_cairoSurface);
		//cairo_scale (my d_cairoGraphicsContext, 72.0 / resolution, 72.0 / resolution);
		/*
		 * Fill in the whole area with a white background.
		 */
		cairo_set_source_rgb (my d_cairoGraphicsContext, 1.0, 1.0, 1.0);
		cairo_rectangle (my d_cairoGraphicsContext, 0, 0, my d_x2DC, my d_y2DC);
		cairo_fill (my d_cairoGraphicsContext);
		cairo_set_source_rgb (my d_cairoGraphicsContext, 0.0, 0.0, 0.0);
	#elif mac
		long width = (x2inches - x1inches) * resolution, height = (y2inches - y1inches) * resolution;
		long stride = width * 4;
		stride = (stride + 15) & ~15;   // CommonCode/AppDrawing.c: "a multiple of 16 bytes, for best performance"
		my d_bits = Melder_malloc (uint8_t, stride * height);
		static CGColorSpaceRef colourSpace = nullptr;
		if (! colourSpace) {
			colourSpace = CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB);
			Melder_assert (colourSpace);
		}
		my d_macGraphicsContext = CGBitmapContextCreate (my d_bits,
			width, height,
			8,   // bits per component
			stride,
			colourSpace,
			kCGImageAlphaPremultipliedLast);
    	if (! my d_macGraphicsContext)
			Melder_throw (U"Could not create PNG file ", file, U".");
		CGRect rect = CGRectMake (0, 0, width, height);
		CGContextSetAlpha (my d_macGraphicsContext, 1.0);
		CGContextSetBlendMode (my d_macGraphicsContext, kCGBlendModeNormal);
		CGContextSetRGBFillColor (my d_macGraphicsContext, 1.0, 1.0, 1.0, 1.0);
		CGContextFillRect (my d_macGraphicsContext, rect);
		CGContextTranslateCTM (my d_macGraphicsContext, 0, height);
		CGContextScaleCTM (my d_macGraphicsContext, 1.0, -1.0);
	#elif win
		my metafile = true;
		HDC screenDC = GetDC (nullptr);
		my d_gdiGraphicsContext = CreateCompatibleDC (screenDC);
		trace (U"d_gdiGraphicsContext ", Melder_pointer (my d_gdiGraphicsContext));
		Melder_assert (my d_gdiGraphicsContext);
		my d_gdiBitmap = CreateCompatibleBitmap (screenDC, (x2inches - x1inches) * resolution, (y2inches - y1inches) * resolution);
		trace (U"d_gdiBitmap ", Melder_pointer (my d_gdiBitmap));
		Melder_assert (my d_gdiBitmap);
		ReleaseDC (nullptr, screenDC);
		SelectObject (my d_gdiGraphicsContext, my d_gdiBitmap);
		trace (U"bitmap selected into device context");
		my resolution = resolution;
		SetBkMode (my d_gdiGraphicsContext, TRANSPARENT);
		my d_winPen = CreatePen (PS_SOLID, 0, RGB (0, 0, 0));
		my d_winBrush = CreateSolidBrush (RGB (0, 0, 0));
		SetTextAlign (my d_gdiGraphicsContext, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);
		/*
		 * Fill in the whole area with a white background.
		 */
		SelectPen (my d_gdiGraphicsContext, GetStockPen (NULL_PEN));
		SelectBrush (my d_gdiGraphicsContext, GetStockBrush (WHITE_BRUSH));
		Rectangle (my d_gdiGraphicsContext, 0, 0, my d_x2DC + 1, my d_y2DC + 1);   // plus 1, in order to prevent two black edges
		SelectPen (my d_gdiGraphicsContext, GetStockPen (BLACK_PEN));
		SelectBrush (my d_gdiGraphicsContext, GetStockBrush (NULL_BRUSH));
	#endif
	return (Graphics) me.transfer();
}

Graphics Graphics_create_pdffile (MelderFile file, int resolution,
	double x1inches, double x2inches, double y1inches, double y2inches)
{
	autoGraphicsScreen me = Thing_new (GraphicsScreen);
	my screen = true;
	my yIsZeroAtTheTop = true;
	#ifdef macintosh
		_GraphicsMacintosh_tryToInitializeQuartz ();
	#endif
	Graphics_init (me.peek(), resolution);
	#if gtk
		my d_cairoSurface = cairo_pdf_surface_create (Melder_peek32to8 (file -> path),
			(NUMdefined (x1inches) ? x2inches - x1inches : x2inches) * 72.0,
			(NUMdefined (y1inches) ? y2inches - y1inches : y2inches) * 72.0);
		my d_cairoGraphicsContext = cairo_create (my d_cairoSurface);
		my d_x1DC = my d_x1DCmin = 0;
		my d_x2DC = my d_x2DCmax = (NUMdefined (x1inches) ?  7.5 : x2inches) * resolution;
		my d_y1DC = my d_y1DCmin = 0;
		my d_y2DC = my d_y2DCmax = (NUMdefined (y1inches) ? 11.0 : y2inches) * resolution;
		Graphics_setWsWindow ((Graphics) me.peek(),
			NUMdefined (x1inches) ? 0.0 : 0.0, NUMdefined (x1inches) ?  7.5 : x2inches,
			NUMdefined (y1inches) ? 1.0 : 0.0, NUMdefined (y1inches) ? 12.0 : y2inches);
		cairo_scale (my d_cairoGraphicsContext, 72.0 / resolution, 72.0 / resolution);
	#elif mac
		CFURLRef url = CFURLCreateWithFileSystemPath (nullptr, (CFStringRef) Melder_peek32toCfstring (file -> path), kCFURLPOSIXPathStyle, false);
		CGRect rect = CGRectMake (0, 0,
			(NUMdefined (x1inches) ? x2inches - x1inches : x2inches) * 72.0,
			(NUMdefined (y1inches) ? y2inches - y1inches : y2inches) * 72.0);   // don't tire PDF viewers with funny origins
		CFStringRef key = (CFStringRef) Melder_peek32toCfstring (U"Creator");
		CFStringRef value = (CFStringRef) Melder_peek32toCfstring (U"Praat");
		CFIndex numberOfValues = 1;
		CFDictionaryRef dictionary = CFDictionaryCreate (nullptr, (const void **) & key, (const void **) & value, numberOfValues,
			& kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
		my d_macGraphicsContext = CGPDFContextCreateWithURL (url, & rect, dictionary);
		CFRelease (url);
		CFRelease (dictionary);
    	if (! my d_macGraphicsContext)
			Melder_throw (U"Could not create PDF file ", file, U".");
		my d_x1DC = my d_x1DCmin = 0;
		my d_x2DC = my d_x2DCmax = (NUMdefined (x1inches) ?  7.5 : x2inches) * resolution;
		my d_y1DC = my d_y1DCmin = 0;
		my d_y2DC = my d_y2DCmax = (NUMdefined (y1inches) ? 11.0 : y2inches) * resolution;
		Graphics_setWsWindow ((Graphics) me.peek(),
			NUMdefined (x1inches) ? 0.0 : 0.0, NUMdefined (x1inches) ?  7.5 : x2inches,
			NUMdefined (y1inches) ? 1.0 : 0.0, NUMdefined (y1inches) ? 12.0 : y2inches);
		CGContextBeginPage (my d_macGraphicsContext, & rect);
		CGContextScaleCTM (my d_macGraphicsContext, 72.0 / resolution, 72.0 / resolution);
		CGContextTranslateCTM (my d_macGraphicsContext,
			(NUMdefined (x1inches) ? - x1inches : 0.0) * resolution,
			(NUMdefined (y1inches) ? (12.0 - y1inches) : y2inches) * resolution);
		CGContextScaleCTM (my d_macGraphicsContext, 1.0, -1.0);
	#endif
	return (Graphics) me.transfer();
}
Graphics Graphics_create_pdf (void *context, int resolution,
	double x1inches, double x2inches, double y1inches, double y2inches)
{
	autoGraphicsScreen me = Thing_new (GraphicsScreen);
	my screen = true;
	my yIsZeroAtTheTop = true;
	#ifdef macintosh
		_GraphicsMacintosh_tryToInitializeQuartz ();
	#endif
	Graphics_init (me.get(), resolution);
	#ifdef macintosh
		my d_macGraphicsContext = static_cast <CGContext *> (context);
		CGRect rect = CGRectMake (0, 0, (x2inches - x1inches) * 72, (y2inches - y1inches) * 72);   // don't tire PDF viewers with funny origins
		my d_x1DC = my d_x1DCmin = 0;
		my d_x2DC = my d_x2DCmax = 7.5 * resolution;
		my d_y1DC = my d_y1DCmin = 0;
		my d_y2DC = my d_y2DCmax = 11.0 * resolution;
		Graphics_setWsWindow (me.get(), 0, 7.5, 1.0, 12.0);
    	Melder_assert (my d_macGraphicsContext);
		CGContextBeginPage (my d_macGraphicsContext, & rect);
		CGContextScaleCTM (my d_macGraphicsContext, 72.0 / resolution, 72.0 / resolution);
		CGContextTranslateCTM (my d_macGraphicsContext, - x1inches * resolution, (12.0 - y1inches) * resolution);
		CGContextScaleCTM (my d_macGraphicsContext, 1.0, -1.0);
	#endif
	return me.transfer();
}

#if cairo
	void *Graphics_x_getCR (Graphics me) {
		return ((GraphicsScreen) me) -> d_cairoGraphicsContext;
	}
	void Graphics_x_setCR (Graphics me, void *cairoGraphicsContext) {
		((GraphicsScreen) me) -> d_cairoGraphicsContext = (cairo_t *) cairoGraphicsContext;
	}
#endif

#if mac
	void GraphicsQuartz_initDraw (GraphicsScreen me) {
		#if useCarbon
			if (my d_macPort) {
				QDBeginCGContext (my d_macPort, & my d_macGraphicsContext);
				//CGContextSetAlpha (my macGraphicsContext, 1.0);
				CGContextSetAllowsAntialiasing (my d_macGraphicsContext, true);
				if (my d_drawingArea) {
					int shellHeight = GuiMac_clipOn_graphicsContext (my d_drawingArea -> d_widget, my d_macGraphicsContext);
					CGContextTranslateCTM (my d_macGraphicsContext, 0, shellHeight);
				} else if (my printer) {
					CGContextTranslateCTM (my d_macGraphicsContext, 0, my d_y2DC - my d_y1DC);
				}
				CGContextScaleCTM (my d_macGraphicsContext, 1.0, -1.0);
			}
        #else
            if (my d_macView) {            
                [my d_macView lockFocus];
				//if (! my printer) {
					my d_macGraphicsContext = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
				//}
                Melder_assert (my d_macGraphicsContext);
				if (my printer) {
					//CGContextTranslateCTM (my d_macGraphicsContext, 0, [my d_macView bounds]. size. height);
					//CGContextScaleCTM (my d_macGraphicsContext, 1.0, -1.0);
				}
			}
		#endif
	}
	void GraphicsQuartz_exitDraw (GraphicsScreen me) {
		#if useCarbon
			if (my d_macPort) {
				CGContextSynchronize (my d_macGraphicsContext);
				QDEndCGContext (my d_macPort, & my d_macGraphicsContext);
			}
        #else
            if (my d_macView) {
                //CGContextSynchronize (my d_macGraphicsContext);   // BUG: should not be needed
                [my d_macView unlockFocus];
            }
		#endif
	}
#endif

/* End of file GraphicsScreen.cpp */
