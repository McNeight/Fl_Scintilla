//
// "$Id: fl_font_mac.cxx 10193 2014-06-14 11:06:42Z manolo $"
//
// MacOS font selection routines for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2011 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

#include "fltk_config.h"
#if __FLTK_IPHONEOS__

/* from fl_utf.c */
extern unsigned fl_utf8toUtf16(const char* src, unsigned srclen, unsigned short* dst, unsigned dstlen);

static CGAffineTransform font_mx = { 1, 0, 0, -1, 0, 0 };
static CFMutableDictionaryRef attributes = NULL;

const int Fl_X::CoreText_threshold = 100500; // this represents Mac OS 10.5

Fl_Font_Descriptor::Fl_Font_Descriptor(const char* name, Fl_Fontsize Size)
{
	next = 0;
#  if HAVE_GL
	listbase = 0;
#  endif

//  knowWidths = 0;
	// OpenGL needs those for its font handling
	size = Size;
	CFStringRef str = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
	fontref = CTFontCreateWithName(str, size, NULL);
	CGGlyph glyph[2];
	const UniChar A[2]= {'W','.'};
	CTFontGetGlyphsForCharacters(fontref, A, glyph, 2);
	CGSize advances[2];
	double w;
	CTFontGetAdvancesForGlyphs(fontref, kCTFontHorizontalOrientation, glyph, advances, 2);
	w = advances[0].width;
	if ( abs(advances[0].width - advances[1].width) < 1E-2 ) {//this is a fixed-width font
		// slightly rescale fixed-width fonts so the character width has an integral value
		CFRelease(fontref);
		CGFloat fsize = size / ( w/floor(w + 0.5) );
		fontref = CTFontCreateWithName(str, fsize, NULL);
		w = CTFontGetAdvancesForGlyphs(fontref, kCTFontHorizontalOrientation, glyph, NULL, 1);
	}
	CFRelease(str);
	ascent = (short)(CTFontGetAscent(fontref) + 0.5);
	descent = (short)(CTFontGetDescent(fontref) + 0.5);
	q_width = w + 0.5;
	for (unsigned i = 0; i < sizeof(width)/sizeof(float*); i++) width[i] = NULL;
	if (!attributes) {
		static CFNumberRef zero_ref;
		float zero = 0.;
		zero_ref = CFNumberCreate(NULL, kCFNumberFloat32Type, &zero);
		// deactivate kerning for all fonts, so that string width = sum of character widths
		// which allows fast fl_width() implementation.
		attributes = CFDictionaryCreateMutable(kCFAllocatorDefault,
		                                       3,
		                                       &kCFTypeDictionaryKeyCallBacks,
		                                       &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue (attributes, kCTKernAttributeName, zero_ref);
	}
	if (ascent == 0) { // this may happen with some third party fonts
		CFDictionarySetValue (attributes, kCTFontAttributeName, fontref);
		CFAttributedStringRef mastr = CFAttributedStringCreate(kCFAllocatorDefault, CFSTR("Wj"), attributes);
		CTLineRef ctline = CTLineCreateWithAttributedString(mastr);
		CFRelease(mastr);
		CGFloat fascent, fdescent;
		CTLineGetTypographicBounds(ctline, &fascent, &fdescent, NULL);
		CFRelease(ctline);
		ascent = (short)(fascent + 0.5);
		descent = (short)(fdescent + 0.5);
	}
}

Fl_Font_Descriptor::~Fl_Font_Descriptor()
{
	/*
	#if HAVE_GL
	 // ++ todo: remove OpenGL font alocations
	// Delete list created by gl_draw().  This is not done by this code
	// as it will link in GL unnecessarily.  There should be some kind
	// of "free" routine pointer, or a subclass?
	// if (listbase) {
	//  int base = font->min_char_or_byte2;
	//  int size = font->max_char_or_byte2-base+1;
	//  int base = 0; int size = 256;
	//  glDeleteLists(listbase+base,size);
	// }
	#endif
	  */
	if (this == fl_graphics_driver->font_descriptor()) fl_graphics_driver->font_descriptor(NULL);
	CFRelease(fontref);
	for (unsigned i = 0; i < sizeof(width)/sizeof(float*); i++) {
		if (width[i]) free(width[i]);
	}
}

////////////////////////////////////////////////////////////////
static Fl_Fontdesc built_in_table_PS[] = { // PostScript font names preferred when Mac OS 閳?10.5
	{"ArialMT"},
	{"Arial-BoldMT"},
	{"Arial-ItalicMT"},
	{"Arial-BoldItalicMT"},
	{"CourierNewPSMT"},
	{"CourierNewPS-BoldMT"},
	{"CourierNewPS-ItalicMT"},
	{"CourierNewPS-BoldItalicMT"},
	{"TimesNewRomanPSMT"},
	{"TimesNewRomanPS-BoldMT"},
	{"TimesNewRomanPS-ItalicMT"},
	{"TimesNewRomanPS-BoldItalicMT"},
	{"Symbol"},
	{"Monaco"},
	{"AndaleMono"}, // there is no bold Monaco font on standard Mac
	{"ZapfDingbatsITC"}
};

static Fl_Fontdesc built_in_table_full[] = { // full font names used before 10.5
	{"Arial"},
	{"Arial Bold"},
	{"Arial Italic"},
	{"Arial Bold Italic"},
	{"Courier New"},
	{"Courier New Bold"},
	{"Courier New Italic"},
	{"Courier New Bold Italic"},
	{"Times New Roman"},
	{"Times New Roman Bold"},
	{"Times New Roman Italic"},
	{"Times New Roman Bold Italic"},
	{"Symbol"},
	{"Monaco"},
	{"Andale Mono"}, // there is no bold Monaco font on standard Mac
	{"Webdings"}
};

static UniChar *utfWbuf = 0;
static unsigned utfWlen = 0;

static UniChar *ios_Utf8_to_Utf16(const char *txt, int len, int *new_len)
{
	unsigned wlen = fl_utf8toUtf16(txt, len, (unsigned short*)utfWbuf, utfWlen);
	if (wlen >= utfWlen) {
		utfWlen = wlen + 100;
		if (utfWbuf) free(utfWbuf);
		utfWbuf = (UniChar*)malloc((utfWlen)*sizeof(UniChar));
		wlen = fl_utf8toUtf16(txt, len, (unsigned short*)utfWbuf, utfWlen);
	}
	*new_len = wlen;
	return utfWbuf;
} // ios_Utf8_to_Utf16

Fl_Fontdesc* Fl_X::calc_fl_fonts(void)
{
	return built_in_table_PS;// : built_in_table_full);
}

static Fl_Font_Descriptor* find(Fl_Font fnum, Fl_Fontsize size)
{
	Fl_Fontdesc* s = fl_fonts+fnum;
	if (!s->name) s = fl_fonts; // use 0 if fnum undefined
	Fl_Font_Descriptor* f;
	for (f = s->first; f; f = f->next)
		if (f->size == size) return f;
	f = new Fl_Font_Descriptor(s->name, size);
	f->next = s->first;
	s->first = f;
	return f;
}

////////////////////////////////////////////////////////////////
// Public interface:

void Fl_Quartz_Graphics_Driver::font(Fl_Font fnum, Fl_Fontsize size)
{
	if (fnum==-1) {
		Fl_Graphics_Driver::font(0, 0);
		return;
	}
	Fl_Graphics_Driver::font(fnum, size);
	this->font_descriptor( find(fnum, size) );
}

int Fl_Quartz_Graphics_Driver::height()
{
	if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);
	Fl_Font_Descriptor *fl_fontsize = font_descriptor();
	return fl_fontsize->ascent + fl_fontsize->descent;
}

int Fl_Quartz_Graphics_Driver::descent()
{
	if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);
	Fl_Font_Descriptor *fl_fontsize = font_descriptor();
	return fl_fontsize->descent+1;
}

// returns width of a pair of UniChar's in the surrogate range
static CGFloat surrogate_width(const UniChar *txt, Fl_Font_Descriptor *fl_fontsize)
{
	CTFontRef font2 = fl_fontsize->fontref;
	bool must_release = false;
	CGGlyph glyphs[2];
	bool b = CTFontGetGlyphsForCharacters(font2, txt, glyphs, 2);
	CGSize a;
	if(!b) { // the current font doesn't contain this char
		CFStringRef str = CFStringCreateWithCharactersNoCopy(NULL, txt, 2, kCFAllocatorNull);
		// find a font that contains it
		font2 = CTFontCreateForString(font2, str, CFRangeMake(0,2));
		must_release = true;
		CFRelease(str);
		b = CTFontGetGlyphsForCharacters(font2, txt, glyphs, 2);
	}
	if (b) CTFontGetAdvancesForGlyphs(font2, kCTFontHorizontalOrientation, glyphs, &a, 1);
	else a.width = fl_fontsize->q_width;
	if(must_release) CFRelease(font2);
	return a.width;
}

static double fl_ios_width(const UniChar* txt, int n, Fl_Font_Descriptor *fl_fontsize)
{
	double retval = 0;
	UniChar uni;
	int i;
	for (i = 0; i < n; i++) { // loop over txt
		uni = txt[i];
		if (uni >= 0xD800 && uni <= 0xDBFF) { // handles the surrogate range
			retval += surrogate_width(&txt[i], fl_fontsize);
			i++; // because a pair of UniChar's represent a single character
			continue;
		}
		const int block = 0x10000 / (sizeof(fl_fontsize->width)/sizeof(float*)); // block size
		// r: index of the character block containing uni
		unsigned int r = uni >> 7; // change 7 if sizeof(width) is changed
		if (!fl_fontsize->width[r]) { // this character block has not been hit yet
			//fprintf(stderr,"r=%d size=%d name=%s\n",r,fl_fontsize->size,fl_fonts[fl_font()].name);
			// allocate memory to hold width of each character in the block
			fl_fontsize->width[r] = (float*) malloc(sizeof(float) * block);
			UniChar ii = r * block;
			CGSize advance_size;
			CGGlyph glyph;
			for (int j = 0; j < block; j++) { // loop over the block
				// ii spans all characters of this block
				bool b = CTFontGetGlyphsForCharacters(fl_fontsize->fontref, &ii, &glyph, 1);
				if (b)
					CTFontGetAdvancesForGlyphs(fl_fontsize->fontref, kCTFontHorizontalOrientation, &glyph, &advance_size, 1);
				else
					advance_size.width = -1e9; // calculate this later
				// the width of one character of this block of characters
				fl_fontsize->width[r][j] = advance_size.width;
				ii++;
			}
		}
		// sum the widths of all characters of txt
		double wdt = fl_fontsize->width[r][uni & (block-1)];
		if (wdt == -1e9) {
			CGSize advance_size;
			CGGlyph glyph;
			CTFontRef font2 = fl_fontsize->fontref;
			bool must_release = false;
			bool b = CTFontGetGlyphsForCharacters(font2, &uni, &glyph, 1);
			if (!b) { // the current font doesn't contain this char
				CFStringRef str = CFStringCreateWithCharactersNoCopy(NULL, &uni, 1, kCFAllocatorNull);
				// find a font that contains it
				font2 = CTFontCreateForString(font2, str, CFRangeMake(0,1));
				must_release = true;
				CFRelease(str);
				b = CTFontGetGlyphsForCharacters(font2, &uni, &glyph, 1);
			}
			if (b) CTFontGetAdvancesForGlyphs(font2, kCTFontHorizontalOrientation, &glyph, &advance_size, 1);
			else advance_size.width = 0.;
			// the width of the 'uni' character
			wdt = fl_fontsize->width[r][uni & (block-1)] = advance_size.width;
			if (must_release) CFRelease(font2);
		}
		retval += wdt;
	}
	return retval;
	return 0;
}

double Fl_Quartz_Graphics_Driver::width(const char* txt, int n)
{
	int wc_len = n;
	UniChar *uniStr = ios_Utf8_to_Utf16(txt, n, &wc_len);
	if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);
	return fl_ios_width(uniStr, wc_len, font_descriptor());
}

double Fl_Quartz_Graphics_Driver::width(unsigned int wc)
{
	if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);

	UniChar utf16[3];
	int l = 1;
	if (wc <= 0xFFFF) {
		*utf16 = wc;
	} else {
//    char buf[4];
//    l = fl_utf8encode(wc, buf);
//    l = (int)fl_utf8toUtf16(buf, l, utf16, 3);
		l = (int)fl_ucs_to_Utf16(wc, utf16, 3);
	}
	return fl_ios_width(utf16, l, font_descriptor());
}

// text extent calculation
void Fl_Quartz_Graphics_Driver::text_extents(const char *str8, int n, int &dx, int &dy, int &w, int &h)
{
	if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);
	Fl_Font_Descriptor *fl_fontsize = font_descriptor();
	UniChar *txt = ios_Utf8_to_Utf16(str8, n, &n);
	CFStringRef str16 = CFStringCreateWithCharactersNoCopy(NULL, txt, n,  kCFAllocatorNull);
	CFDictionarySetValue (attributes, kCTFontAttributeName, fl_fontsize->fontref);
	CFAttributedStringRef mastr = CFAttributedStringCreate(kCFAllocatorDefault, str16, attributes);
	CFRelease(str16);
	CTLineRef ctline = CTLineCreateWithAttributedString(mastr);
	CFRelease(mastr);
	CGContextSetTextPosition(fl_gc, 0, 0);
	CGContextSetShouldAntialias(fl_gc, true);
	CGRect rect = CTLineGetImageBounds(ctline, fl_gc);
	CGContextSetShouldAntialias(fl_gc, false);
	CFRelease(ctline);
	dx = floor(rect.origin.x + 0.5);
	dy = floor(- rect.origin.y - rect.size.height + 0.5);
	w = rect.size.width + 0.5;
	h = rect.size.height + 0.5;
	return;
} // fl_text_extents

static CGColorRef flcolortocgcolor(Fl_Color i)
{
	uchar r, g, b;
	Fl::get_color(i, r, g, b);
	CGFloat components[4] = {r/255.0f, g/255.0f, b/255.0f, 1.};
	static CGColorSpaceRef cspace = NULL;
	if (cspace == NULL) {
		cspace = CGColorSpaceCreateDeviceRGB();
		//cspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
	}
	return CGColorCreate(cspace, components);
}

static void fl_ios_draw(const char *str, int n, float x, float y, Fl_Graphics_Driver *driver)
{
	// convert to UTF-16 first
	UniChar *uniStr = ios_Utf8_to_Utf16(str, n, &n);
	CFStringRef str16 = CFStringCreateWithCharactersNoCopy(NULL, uniStr, n,  kCFAllocatorNull);
	if (str16 == NULL) return; // shd not happen
	CGColorRef color = flcolortocgcolor(driver->color());
	CFDictionarySetValue (attributes, kCTFontAttributeName, driver->font_descriptor()->fontref);
	CFDictionarySetValue (attributes, kCTForegroundColorAttributeName, color);
	CFAttributedStringRef mastr = CFAttributedStringCreate(kCFAllocatorDefault, str16, attributes);
	CFRelease(str16);
	CFRelease(color);
	CTLineRef ctline = CTLineCreateWithAttributedString(mastr);
	CFRelease(mastr);
	CGContextSetTextMatrix(fl_gc, font_mx);
	CGContextSetTextPosition(fl_gc, x, y);
	CGContextSetShouldAntialias(fl_gc, true);
	CTLineDraw(ctline, fl_gc);
	CGContextSetShouldAntialias(fl_gc, false);
	CFRelease(ctline);
}

void Fl_Quartz_Graphics_Driver::draw(const char *str, int n, float x, float y)
{
	// avoid a crash if no font has been selected by user yet !
	if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);
	fl_ios_draw(str, n, x, y, this);
}

void Fl_Quartz_Graphics_Driver::draw(const char* str, int n, int x, int y)
{
	// avoid a crash if no font has been selected by user yet !
	if (!font_descriptor()) font(FL_HELVETICA, FL_NORMAL_SIZE);
	fl_ios_draw(str, n, (float)x-0.0f, (float)y+0.5f, this);
}

void Fl_Quartz_Graphics_Driver::draw(int angle, const char *str, int n, int x, int y)
{
	CGContextSaveGState(fl_gc);
	CGContextTranslateCTM(fl_gc, x, y);
	CGContextRotateCTM(fl_gc, - angle*(M_PI/180) );
	draw(str, n, 0, 0);
	CGContextRestoreGState(fl_gc);
}

void Fl_Quartz_Graphics_Driver::rtl_draw(const char* c, int n, int x, int y)
{
	int dx, dy, w, h;
	text_extents(c, n, dx, dy, w, h);
	draw(c, n, x - w - dx, y);
}

#endif
//
// End of "$Id: fl_font_mac.cxx 10193 2014-06-14 11:06:42Z manolo $".
//