/*
 Edited by Fernand Veilleux, fernveilleux@gmail.com in 2015 to include
 in linuxcnc-features. Only part for gcode was edited.

 This file is part of TTT.  TTT is free software; you can redistribute
 it and/or modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2 of the
 License, or (at your option) any later version.  TTT is distributed in
 the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with TTT; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

 Copyright © 2004, 2005, 2006, 2007, 2008 Chris Radek <chris@timeguy.com>

 This source gets compiled into 2 executables. The first, ttt generates
 G-code from a true type font and a text string. The second executable
 is ttt_dxf which generates an autocad dxf file from a true type font
 and a text string.

 example commandline:
 ttt "Hello World" > hw.ngc
 ttt_dxf "Hello World" > hw.dxf

 MANY THANKS go to Lawrence Glaister <ve7it@shaw.ca> for updates based
 on the new FreeType FT_Outline API and .pfb support!
 */

#include <stdio.h>
#include <ctype.h>
#include <getopt.h>
#include <locale.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s )	{ e, s },
#define FT_ERROR_START_LIST	{
#define FT_ERROR_END_LIST	{ 0, 0 } };

// this is the default font used if not specified on commandline
#define TTFONT "/usr/share/fonts/truetype/freefont/FreeSerifBoldItalic.ttf"

const struct ftError
{
	int err_code;
	const char* err_msg;
}ft_errors[] =
#include FT_ERRORS_H

// define the number of linear segments we use to approximate beziers
// in the gcode and the number of polyline control points for dxf code.
int csteps=100;
// define the subdivision of curves into arcs: approximate curve length
// in font coordinates to get one arc pair (minimum of two arc pairs
// per curve)
double dsteps = 200;

typedef struct g_Code GCode;
struct g_Code {
	GCode * nextGCode;
	char * str;
};

typedef struct line_of_text Text_Line;
struct line_of_text {
	Text_Line * nextLine;
	GCode * firstGCode;
	long extend_xto;
};

Text_Line * TextLine = NULL;
Text_Line * CurrentTextLine = NULL;
GCode * LastGCode;

void addGCode(int len, char * s) {
	GCode * new_gcode = malloc(sizeof(GCode));

	if (new_gcode == NULL )
		exit(101);

	LastGCode->nextGCode = new_gcode;
	LastGCode = new_gcode;

	LastGCode->nextGCode = NULL;

	LastGCode->str = malloc(strlen(s) + 1);
	strcpy(LastGCode->str, s);
}

void addGCodeLine() {
	Text_Line * aLine = malloc(sizeof(Text_Line));
	GCode * gcode = malloc(sizeof(GCode));

	if (aLine == NULL || gcode == NULL ) {
		exit(101);
	}

	if (TextLine == NULL )
		TextLine = aLine;
	else
		CurrentTextLine->nextLine = aLine;

	CurrentTextLine = aLine;
	CurrentTextLine->nextLine = NULL;
	CurrentTextLine->firstGCode = gcode;

	gcode->nextGCode = NULL;
	gcode->str = malloc(1);
	strcpy(gcode->str, "");

	LastGCode = gcode;

	aLine->extend_xto = 0;
}
/*
 void deleteGCode() {
 GCode *del_gc, *gc;
 Text_Line *del_ln, *al;

 al = TextLine;
 while (al != NULL) {

 gc = al->firstGCode;
 while (gc != NULL ) {
 del_gc = gc;
 gc = gc->nextGCode;
 free(del_gc->str);
 free(del_gc);
 }
 del_ln = al;
 al = al->nextLine;
 free(del_ln);
 }
 }*/

void printGCode(Text_Line * aLine) {
	if (aLine == NULL )
		exit(102);

	Text_Line *del_line;
	GCode *del_gc, *gc;

	gc = aLine->firstGCode;
	while (gc != NULL ) {
		printf("%s\n", gc->str);
		del_gc = gc;
		gc = gc->nextGCode;
		free(del_gc->str);
		free(del_gc);
	}
	free(aLine->firstGCode);
	del_line = aLine;
	CurrentTextLine = del_line->nextLine;
	free(aLine);
}

#define NEQ(a,b) ((a).x != (b).x || (a).y != (b).y)
#define SQ(a) ((a)*(a))
#define CUBE(a) ((a)*(a)*(a))

typedef struct {
	double x, y;
} P;

static double max(double a, double b) {
	if (a < b)
		return b;
	else
		return a;
}

static P ft2p(const FT_Vector *v) {
	P r = { v->x, v->y };
	return r;
}
static double dot(P a, P b) {
	return a.x * b.x + a.y * b.y;
}
static double mag(P a) {
	return sqrt(dot(a, a));
}
static P scale(P a, double b) {
	P r = { a.x * b, a.y * b };
	return r;
}
static P add(P a, P b) {
	P r = { a.x + b.x, a.y + b.y };
	return r;
}
static P add3(P a, P b, P c) {
	P r = { a.x + b.x + c.x, a.y + b.y + c.y };
	return r;
}
static P add4(P a, P b, P c, P d) {
	P r = { a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y };
	return r;
}
static P sub(P a, P b) {
	P r = { a.x - b.x, a.y - b.y };
	return r;
}
static P unit(P a) {
	double m = mag(a);
	if (m) {
		P r = { a.x / m, a.y / m };
		return r;
	} else {
		P r = { 0, 0 };
		return r;
	}
}

void line(P p) {
	char s[100];
#ifdef DXF
	printf("  0\nVERTEX\n  8\n0\n 10\n%.4f\n 20\n%.4f\n 30\n0.0\n",
			p.x, p.y);
#else
	addGCode(sprintf(s, "G1  X[%6f * #3 + #5] Y[%6f * #4 + #6]", p.x, p.y), s);
#endif
}

void arc(P p1, P p2, P d) {
	char s[100];

	d = unit(d);
	P p = sub(p2, p1);
	double den = 2 * (p.y * d.x - p.x * d.y);

	if (fabs(den) < 1e-10) {
		addGCode(
				sprintf(s, "G1  X[%6f * #3 + #5] Y[%6f * #4 + #6]", p2.x, p2.y),
				s);
		return;
	}

	double r = -dot(p, p) / den;

	double i = d.y * r;
	double j = -d.x * r;

	P c = { p1.x + i, p1.y + j };
	double st = atan2(p1.y - c.y, p1.x - c.x);
	double en = atan2(p2.y - c.y, p2.x - c.x);

	if (r < 0)
		while (en <= st)
			en += 2 * M_PI;
	else
		while (en >= st)
			en -= 2 * M_PI;

#ifdef DXF
	double bulge = tan(fabs(en-st)/4);
	if(r > 0) bulge = -bulge;
	printf("  42\n%.4f\n  70\n1\n"
			"  0\nVERTEX\n  8\n0\n  10\n%.4f\n  20\n%.4f\n  30\n0.0\n",
			bulge, p2.x, p2.y);
#else
	double gr = (en - st) < M_PI ? fabs(r) : -fabs(r);
	if (r < 0)
		addGCode(
				sprintf(s, "G3  X[%6f * #3 + #5] Y[%6f * #4 + #6] R[%5f * #3]",
						p2.x, p2.y, gr), s);
	else
		addGCode(
				sprintf(s, "G2  X[%6f * #3 + #5] Y[%6f * #4 + #6] R[%5f * #3]",
						p2.x, p2.y, gr), s);
#endif
}

void biarc(P p0, P ts, P p4, P te, double r) {
	ts = unit(ts);
	te = unit(te);

	P v = sub(p0, p4);

	double c = dot(v, v);
	double b = 2 * dot(v, add(scale(ts, r), te));
	double a = 2 * r * (dot(ts, te) - 1);

	double disc = b * b - 4 * a * c;

	if (a == 0 || disc < 0) {
		line(p4);
		return;
	}

	double disq = sqrt(disc);
	double beta1 = (-b - disq) / 2 / a;
	double beta2 = (-b + disq) / 2 / a;
	double beta = max(beta1, beta2);

	if (beta <= 0) {
		line(p4);
		return;
	}

	double alpha = beta * r;
	double ab = alpha + beta;
	P p1 = add(p0, scale(ts, alpha));
	P p3 = add(p4, scale(te, -beta));
	P p2 = add(scale(p1, beta / ab), scale(p3, alpha / ab));
	P tm = sub(p3, p2);

	arc(p0, p2, ts);
	arc(p2, p4, tm);
}

#ifdef DXF
static int bootstrap = 1;
#endif
static FT_Vector last_point;
static int debug = 0;

struct extents {
	long int minx;
	long int maxx;
	long int miny;
	long int maxy;
} glyph_extents, line_extents;

static FT_Vector advance;

// routine to print out hopefully-useful error messages
void handle_ft_error(char *where, int f, int x) {
	const struct ftError *e = &ft_errors[0];
	for (; e->err_msg && e->err_code != f; e++)
		;
	if (e->err_msg) {
		fprintf(stderr, "Fatal error in %s: %s (%d) at line:%d\n", where,
				e->err_msg, f, x);
	} else {
		fprintf(stderr, "Fatal error in %s: %d at line:%d\n", where, f, x);
	}
	exit(x);
}

// resets extents struct members min and max to +big and -big respectively
// next call to extents_add_point(point) will set them to that point
void extents_reset(struct extents *e) {
	e->maxx = -2000000000;
	e->maxy = -2000000000;
	e->minx = 2000000000;
	e->miny = 2000000000;
}

// updates extents struct to include the point
void extents_add_point(struct extents *e, const FT_Vector *point) {
	if (point->x > e->maxx)
		e->maxx = point->x;
	if (point->y > e->maxy)
		e->maxy = point->y;
	if (point->x < e->minx)
		e->minx = point->x;
	if (point->y < e->miny)
		e->miny = point->y;
}

// updates extents struct e1 to include all of e2
void extents_add_extents(struct extents *e1, struct extents *e2) {
	if (e2->maxx > e1->maxx)
		e1->maxx = e2->maxx;
	if (e2->maxy > e1->maxy)
		e1->maxy = e2->maxy;
	if (e2->minx < e1->minx)
		e1->minx = e2->minx;
	if (e2->miny < e1->miny)
		e1->miny = e2->miny;
}

// move with 'pen up' to a new position and then put 'pen down' 
int my_move_to(const FT_Vector* to, void* user) {
	char s[100];
#ifdef DXF
	/* every move but the first one means we are starting a new polyline */
	/* make sure we terminate previous polyline with a seqend */
	if(bootstrap == 0) printf("  0\nSEQEND\n");
	bootstrap=0;
	printf("  0\nPOLYLINE\n  8\n0\n 66\n     1\n 10\n0.0\n 20\n0.0\n 30\n0.0\n");
	printf("  0\nVERTEX\n  8\n0\n 10\n%ld.000\n 20\n%ld.000\n 30\n0.0\n",
			to->x, to->y);
#else
	if (debug)
		addGCode(sprintf(s, "\n(moveto %ld,%ld)", to->x, to->y), s);
	addGCode(sprintf(s, "G0   Z[#1]"), s);
	addGCode(
			sprintf(s, "G0   X[%6ld * #3 + #5] Y[%6ld * #4 + #6]",
					to->x, to->y), s);
	addGCode(sprintf(s, "G1   Z[#2]\n"), s);
#endif
	last_point = *to;
	extents_add_point(&glyph_extents, to);

	return 0;
}

// plot with pen down to a new endpoint drawing a line segment 
// Linear Bézier curves (a line)
int my_line_to(const FT_Vector* to, void* user) {
	char s[100];
#ifdef DXF
	printf("  0\nVERTEX\n  8\n0\n 10\n%ld.000\n 20\n%ld.000\n 30\n0.0\n",
			to->x,to->y);
#else
	addGCode(
			sprintf(s, "G1   X[%6ld * #3 + #5] Y[%6ld * #4 + #6]",
					to->x, to->y), s);
#endif
	last_point = *to;
	extents_add_point(&glyph_extents, to);

	return 0;
}

// draw a second order curve from current pos to 'to' using control
// Quadratic Bézier curves (a curve)
// B(t) = (1 - t)^2A + 2t(1 - t)B + t^2C,  t in [0,1]. 
int my_conic_to(const FT_Vector* control, const FT_Vector* to, void* user) {
	char s[100];
	int t;
	double x, y;
	FT_Vector point = last_point;
	double len = 0;
	double l[csteps + 1];

#ifndef DXF
	addGCode(
			sprintf(s,
					"G5.1 X[%6ld * #3 + #5] Y[%6ld * #4 + #6] I[%5ld * #3] J[%5ld * #4]",
					to->x, to->y, control->x - last_point.x,
					control->y - last_point.y), s);
	last_point = *to;
	return 0;
#endif

	l[0] = 0;
	for (t = 1; t <= csteps; t++) {
		double tf = (double) t / (double) csteps;
		x = SQ(1-tf) * last_point.x + 2 * tf * (1 - tf) * control->x
				+ SQ(tf) * to->x;
		y = SQ(1-tf) * last_point.y + 2 * tf * (1 - tf) * control->y
				+ SQ(tf) * to->y;
		len += hypot(x - point.x, y - point.y);
		point.x = x;
		point.y = y;
		extents_add_point(&glyph_extents, &point);
	}

	P p0 = ft2p(&last_point), p1 = ft2p(control), p2 = ft2p(to);
	P q0 = sub(p1, p0), q1 = sub(p2, p1);
	P ps = p0;
	P ts = q0;
	int steps = (int) max(2, len / dsteps);
	for (t = 1; t <= steps; t++) {
		double tf = (double) t / (double) steps;
		double t1 = 1 - tf;
		P p = add3(scale(p0, SQ(t1)), scale(p1, 2 * tf * t1),
				scale(p2, SQ(tf)));
		P t = add(scale(q0, t1), scale(q1, tf));

		biarc(ps, ts, p, t, 1.0);

		ps = p;
		ts = t;
	}

	last_point = *to;
	return 0;
}

// draw a cubic spline from current pos to 'to' using control1,2
// Cubic Bézier curves ( a compound curve )
// B(t)=A(1-t)^3 + 3Bt(1-t)^2 + 3Ct^2(1-t) + Dt^3 , t in [0,1]. 
int my_cubic_to(const FT_Vector* control1, const FT_Vector* control2,
		const FT_Vector *to, void* user) {
	char s[100];
	int t;
	double x, y;
	FT_Vector point = last_point;
	double len = 0;

#ifndef DXF
	addGCode(
			sprintf(s, "G5.2 X[%6ld * #3 + #5] Y[%6ld * #4 + #6] L4 P1",
					control1->x, control1->y), s);
	addGCode(
			sprintf(s, "     X[%6ld * #3 + #5] Y[%6ld * #4 + #6] P1",
					control2->x, control2->y), s);
	addGCode(
			sprintf(s, "     X[%6ld * #3 + #5] Y[%6ld * #4 + #6] P1", to->x,
					to->y), s);
	addGCode(sprintf(s, "G5.3\n"), s);
	return 0;
#endif

	for (t = 1; t <= csteps; t++) {
		double tf = (double) t / (double) csteps;
		x = CUBE(1-tf) * last_point.x + SQ(1-tf) * 3 * tf * control1->x
				+ SQ(tf) * (1 - tf) * 3 * control2->x + CUBE(tf) * to->x;
		y = CUBE(1-tf) * last_point.y + SQ(1-tf) * 3 * tf * control1->y
				+ SQ(tf) * (1 - tf) * 3 * control2->y + CUBE(tf) * to->y;
		;
		len += hypot(x - point.x, y - point.y);
		point.x = x;
		point.y = y;
		extents_add_point(&glyph_extents, &point);
	}

	int steps = (int) max(2, len / dsteps);
#ifndef DXF
	addGCode(
			sprintf(s, ";cubicto %ld,%ld %ld,%ld %ld,%ld) ", control1->x,
					control1->y, control2->x, control2->y, to->x, to->y), s);
	addGCode(sprintf(s, "len=%f steps=%d\n", len, steps), s);
#endif
	P p0 = ft2p(&last_point), p1 = ft2p(control1), p2 = ft2p(control2), p3 =
			ft2p(to);
	P q0 = sub(p1, p0), q1 = sub(p2, p1), q2 = sub(p3, p2);
	P ps = p0;
	P ts = q0;
	for (t = 1; t <= steps; t++) {
		double tf = t * 1.0 / steps;
		double t1 = 1 - tf;
		P p = add4(scale(p0, CUBE(t1)), scale(p1, 3 * tf * SQ(t1)),
				scale(p2, 3 * SQ(tf) * t1), scale(p3, CUBE(tf)));
		P t = add3(scale(q0, SQ(t1)), scale(q1, 2 * tf * t1),
				scale(q2, SQ(tf)));

		biarc(ps, ts, p, t, 1.0);

		ps = p;
		ts = t;
	}

	last_point = *to;
	return 0;
}

static void my_draw_bitmap(FT_Bitmap *b, FT_Int x, FT_Int y, int linescale) {
	FT_Int i, j;
	static int oldbit;
	FT_Vector oldv = { 99999, 0 };
	FT_Vector vbuf[100]; //freetype says no more than 32 ever?
	int spans = 0;
	int pitch = abs(b->pitch);
	static int odd = 0;
	for (j = 0; j < b->rows; j++) {
		FT_Vector v;
		oldbit = 0;
		spans = 0;
		for (i = 0; i < pitch; i++) {
			unsigned char byte = b->buffer[j * pitch + i], mask, bits;
			for (bits = 0, mask = 0x80; mask; bits++, mask >>= 1) {
				unsigned char bit = byte & mask;
				v.x = i * 8 + bits + x;
				v.y = (y - j) * 64 * 64 / linescale - 64 * 32 / linescale;
				if (!oldbit && bit) {
					v.x += 8;
					oldv = v;
					vbuf[spans++] = v;
				}
				if (oldbit && !bit) {
					v.x -= 8;
					if (oldv.x < v.x) {
						vbuf[spans++] = v;
					} else
						spans--;
				}
				oldbit = bit;
			}
		}
		if (oldbit) {
			v.x -= 8;
			vbuf[spans++] = v;
		}
		odd = !odd;
		spans /= 2;
		if (odd) {
			for (int i = spans - 1; i >= 0; i--) {
				my_move_to(vbuf + 1 + (i * 2), (void*) 0);
				my_line_to(vbuf + (i * 2), (void*) 0);
			}
		} else {
			for (int i = 0; i < spans; i++) {
				my_move_to(vbuf + (i * 2), (void*) 0);
				my_line_to(vbuf + 1 + (i * 2), (void*) 0);
			}
		}
	}
}

// lookup glyph and extract all the shapes required to draw the outline
static long int render_char(FT_Face face, wchar_t c, long int offset,
		int linescale) {
	int error;
	int glyph_index;
	FT_Outline outline;
	FT_Outline_Funcs func_interface;

	error = FT_Set_Pixel_Sizes(face, 4096, linescale ? linescale : 64);
	if (error)
		handle_ft_error("FT_Set_Pixel_Sizes", error, __LINE__);

	/* lookup glyph */
	glyph_index = FT_Get_Char_Index(face, (FT_ULong) c);
	if (!glyph_index)
		handle_ft_error("FT_Get_Char_Index", 0, __LINE__);

	/* load glyph */
	error = FT_Load_Glyph(face, glyph_index,
			FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
	if (error)
		handle_ft_error("FT_Load_Glyph", error, __LINE__);
	error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
	if (error)
		handle_ft_error("FT_Render_Glyph", error, __LINE__);

	if (linescale > 0)
		my_draw_bitmap(&face->glyph->bitmap, face->glyph->bitmap_left + offset,
				face->glyph->bitmap_top, linescale);

	error = FT_Set_Pixel_Sizes(face, 0, 64);
	if (error)
		handle_ft_error("FT_Set_Pixel_Sizes", error, __LINE__);
	error = FT_Load_Glyph(face, glyph_index,
			FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
	if (error)
		handle_ft_error("FT_Load_Glyph", error, __LINE__);

	/* shortcut to the outline for our desired character */
	outline = face->glyph->outline;

	/* set up entries in the interface used by FT_Outline_Decompose() */
	func_interface.shift = 0;
	func_interface.delta = 0;
	func_interface.move_to = my_move_to;
	func_interface.line_to = my_line_to;
	func_interface.conic_to = my_conic_to;
	func_interface.cubic_to = my_cubic_to;

	/* offset the outline to the correct position in x */
	FT_Outline_Translate(&outline, offset, 0L);

	/* plot the current character */
	error = FT_Outline_Decompose(&outline, &func_interface, NULL );
	if (error)
		handle_ft_error("FT_Outline_Decompose", error, __LINE__);

	/* save advance in a global */
	advance.x = face->glyph->advance.x;
	advance.y = face->glyph->advance.y;

	/* offset will get bumped up by the x size of the char just plotted */
	return face->glyph->advance.x;
}

int main(int argc, char **argv) {
	FT_Library library;
	FT_Face face;
	int error;
	int i, l, k;
	long int offset;
	int unicode = 0;
	char *s, *list[10];
	char ps[100];
	char *ttfont = TTFONT;
	double scale = 0.0003;
	int linescale = 0, saved_linescale = 0;
	int lineloop = 0;
	int linecount = 0;

	int valign = 1, halign = 1;
	double z_safe = 0.1;
	double cut_depth = -0.015;
	double ref_X = 0;
	double ref_Y = 0;
	double line_spacing = 1.0;
	double text_height = 0.5;
	double XY_offset = 0.0;
	double stretch = 1.0;
	double flip_factor = 1;
	double mirror_factor = 1;
	double max_Y_offset = 0.0;
	int filled = 0;

	for (i = 0; i < 10; i++)
		list[i] = NULL;

	while ((i = getopt(argc, argv,
			"s:df:u:v:h:H:i:X:Y:D:z:e:l:t:m:0:1:2:3:4:5:6:7:8:9:?")) != -1) {
		switch (i) {
		case 's':
			dsteps = atof(optarg);
			break;
		case 'd':
			debug = 1;
			break;
		case 'i':
			line_spacing = atof(optarg);
			break;
		case 'v':
			k = atoi(optarg);
			if ((k >= 0) && (k <= 2))
				valign = k;
			break;
		case 'h':
			k = atoi(optarg);
			if ((k >= 0) && (k <= 2))
				halign = k;
			break;
		case 'z':
			z_safe = atof(optarg);
			break;
		case 'D':
			cut_depth = atof(optarg);
			break;
		case 'X':
			ref_X = atof(optarg);
			break;
		case 'Y':
			ref_Y = atof(optarg);
			break;
		case 'f':
			ttfont = optarg;
			break;
		case 'H':
			text_height = atof(optarg);
			break;
		case 't':
			stretch = atof(optarg);
			break;
		case 'u':
			unicode = atoi(optarg);
			break;
		case 'm':
			switch (atoi(optarg)){
			case 1: mirror_factor = -1; break;
			case 2: flip_factor = -1; break;
			}
			break;
		case '0':
			if (strcmp(optarg, " ") != 0) {
				list[0] = optarg;
				if (list[0][0] == ' ')
					list[0]++;
			}
			break;
		case '1':
			if (strcmp(optarg, " ") != 0) {
				list[1] = optarg;
				if (list[1][0] == ' ')
					list[1]++;
			}
			break;
		case '2':
			if (strcmp(optarg, " ") != 0) {
				list[2] = optarg;
				if (list[2][0] == ' ')
					list[2]++;
			}
			break;
		case '3':
			if (strcmp(optarg, " ") != 0) {
				list[3] = optarg;
				if (list[3][0] == ' ')
					list[3]++;
			}
			break;
		case '4':
			if (strcmp(optarg, " ") != 0) {
				list[4] = optarg;
				if (list[4][0] == ' ')
					list[4]++;
			}
			break;
		case '5':
			if (strcmp(optarg, " ") != 0) {
				list[5] = optarg;
				if (list[5][0] == ' ')
					list[5]++;
			}
			break;
		case '6':
			if (strcmp(optarg, " ") != 0) {
				list[6] = optarg;
				if (list[6][0] == ' ')
					list[6]++;
			}
			break;
		case '7':
			if (strcmp(optarg, " ") != 0) {
				list[7] = optarg;
				if (list[7][0] == ' ')
					list[7]++;
			}
			break;
		case '8':
			if (strcmp(optarg, " ") != 0) {
				list[8] = optarg;
				if (list[8][0] == ' ')
					list[8]++;
			}
			break;
		case '9':
			if (strcmp(optarg, " ") != 0) {
				list[9] = optarg;
				if (list[9][0] == ' ')
					list[9]++;
			}
			break;
		case 'e':
			filled = atoi(optarg);
			break;
		case 'l':
			saved_linescale = atoi(optarg);
			if ((saved_linescale < 24) && (saved_linescale > 0))
				saved_linescale = 24;
			break;
		case '?':
			fprintf(stderr, "\n\tusage : %s [-?] [-args...]\n", argv[0]);
			fprintf(stderr, "\tAll args are optionnal, they are :\n");
			fprintf(stderr, "\tX, Y\tcoords\n");
			fprintf(stderr, "\tu\tunicode 0 = False, 1 = True\n");
			fprintf(stderr,
					"\tv\talign vertical   0=Bottom, 1=Center, 2=Top\n");
			fprintf(stderr,
					"\th\talign horizontal 0=Left,   1=Center, 2=Right\n");
			fprintf(stderr, "\tH\ttext height\n");
			fprintf(stderr, "\ti\tinterline (ratio to text height)\n");
			fprintf(stderr, "\tz\tz safe height\n");
			fprintf(stderr, "\tD\tdepth of engraving\n");
			fprintf(stderr, "\te\tcharacters filled of horiz lines\t0=False, 1=True\n");
			fprintf(stderr, "\tl\tfilling lines of characters\n");
			fprintf(stderr, "\tt\tstretch ratio to text height\n");
			fprintf(stderr, "\tp\tflip, 0 = False, 1 = True\n");
			fprintf(stderr, "\tm\tmirror, 0 = False, 1 = True\n");
			fprintf(stderr, "\tf\ttruetype font file\n");
			fprintf(stderr,
					"\t0-9\ttext line\tIf first char is a space it is trimmed. Add a second if needed\n\n");
			return 99;
			break;
		default:
			return 99;
		}
	}

	error = FT_Init_FreeType(&library);
	if (error)
		handle_ft_error("FT_Init_FreeType", error, __LINE__);

	error = FT_New_Face(library, ttfont, 0, &face);
	if (error)
		handle_ft_error("FT_New_Face", error, __LINE__);

	/* An error can occur with a fixed-size font format (like FNT or PCF)
	 when trying to set the pixel size to a value that is not listed in the
	 face->fixed_sizes array.
	 */
#define MYFSIZE 64
	error = FT_Set_Pixel_Sizes(face, 0, MYFSIZE);
	if (error)
		handle_ft_error("FT_Set_Pixel_Sizes", error, __LINE__);

	if (unicode) {
		setlocale(LC_CTYPE, "");
	}

	l = 0;
	for (i = 0; i < 10; i++) {
		if (list[i])
			l += strlen(list[i]);
	}
	if (l == 0) { // Set default values
		list[0] = "TrueType Font";
		list[1] = "engraving for";
		list[2] = "LinuxCNC-Features";
	}

	//get text maxY
	addGCodeLine();
	extents_reset(&line_extents);
	offset = 0;
	for (wchar_t wc = 'A'; wc <= 'Z'; wc++) {
		extents_reset(&glyph_extents);
		offset += render_char(face, wc, offset, linescale);
		extents_add_extents(&line_extents, &glyph_extents);
	}
	max_Y_offset = line_extents.maxy;

	//delete text line
	GCode *del_gc, *gc;
	gc = CurrentTextLine->firstGCode;
	while (gc != NULL ) {
		del_gc = gc;
		gc = gc->nextGCode;
		free(del_gc->str);
		free(del_gc);
	}
	free(TextLine);
	TextLine = NULL;

	linescale = saved_linescale * filled;

	while (lineloop < 10) {
		s = list[lineloop];
		if (s == NULL ) {
			lineloop++;
			continue;
		}
		l = strlen(s);

		if (!unicode) {
			printf("(text: ");
			for (i = 0; i < l; i++)
				if (isalnum(s[i]))
					printf("%c", s[i]);
				else
					printf("*");
			printf(")\n");
		}

		addGCodeLine();
		extents_reset(&line_extents);
		offset = 0;

		/* loop through rendering all the chars in the string */
		while (*s) {
			wchar_t wc;
			int r = mbtowc(&wc, s, l);
			if (r == -1) {
				s++;
				continue;
			}

#ifndef DXF
			if (!unicode) {
				/* comment gcode at start of each letter		   */
				/* to keep out of trouble, only print numbers/letters, */
				/* The rest get hex representation			   */
				if (isalnum(*s))
					addGCode(sprintf(ps, "\t(start of symbol : %c)", wc), ps);
				else
					addGCode(sprintf(ps, "\t(start of symbol : 0x%02X)", wc),
							ps);
			}
			/* comment with offset info */
			addGCode(sprintf(ps, "\t(starting X offset: %6ld)", offset), ps);
#endif
			extents_reset(&glyph_extents);
			offset += render_char(face, wc, offset, linescale);
			extents_add_extents(&line_extents, &glyph_extents);
			s += r;
			l -= r;
#ifndef DXF
			/* comment with the extents of each letter		 */
			if (glyph_extents.maxx > glyph_extents.minx) {
				addGCode(
						sprintf(ps,
								"\n\t(symbol extents: X = %6ld to %6ld, Y = %6ld to %6ld)",
								glyph_extents.minx, glyph_extents.maxx,
								glyph_extents.miny, glyph_extents.maxy), ps);
			}
			addGCode(
					sprintf(ps, "\t(symbol advance: X = %6ld, Y = %6ld)",
							advance.x, advance.y), ps);
			addGCode(
					sprintf(ps,
							"\t(-----------------------------------------)\n"),
					ps);
			CurrentTextLine->extend_xto += advance.x;
#endif
		}

		/* write out the post amble stuff */
#ifdef DXF
		printf("  0\nSEQEND\n");
		printf("  0\nENDSEC\n  0\nEOF\n");
#else
		addGCode(sprintf(ps, "\t(final X offset: %ld)", offset), ps);
		if (line_extents.maxx > line_extents.minx) {
			addGCode(
					sprintf(ps,
							"\t(overall extents: X = %6ld to %6ld, Y = %6ld to %6ld)\n",
							line_extents.minx, line_extents.maxx,
							line_extents.miny, line_extents.maxy), ps);
		}
		addGCode(sprintf(ps, "\nG0   Z[#1]\n"), ps);

		CurrentTextLine->extend_xto = max(line_extents.maxx, CurrentTextLine->extend_xto);
		linecount++;
		lineloop++;
	}

#endif

	/* write out preamble */
#ifdef DXF
	printf("  0\nSECTION\n  2\nENTITIES\n");
#else
	printf("(font: %s)\n", ttfont);
	printf("#1=%10f\t(SafeHeight)\n", z_safe);
	printf("#2=%10f\t(Bottom of Cut)\n", cut_depth);
#endif

	scale = text_height / max_Y_offset;
	printf("#3=%10f\t(X Scale)\n", scale * stretch * mirror_factor);
	printf("#4=%10f\t(Y Scale)\n\n", scale * flip_factor);

	lineloop = 0;
	line_spacing *= text_height;
	CurrentTextLine = TextLine;
	while (CurrentTextLine != NULL ) {
		switch (halign) {
		case 1: /* center */
			XY_offset = ref_X
					- scale * CurrentTextLine->extend_xto / 2.0 * mirror_factor
							* stretch;
			break;
		case 2: /* right */
			XY_offset = ref_X
					- scale * CurrentTextLine->extend_xto * mirror_factor
							* stretch;
			break;
		default: /* left*/
			XY_offset = ref_X;
			break;
		}
		printf("#5=%10f\t(X offset can change every line)\n", XY_offset);

		switch (valign) {
		case 1: /* center */
			XY_offset = ref_Y
					+ (((((text_height + line_spacing) * (linecount - 1)
							- text_height) / 2)
							- (text_height + line_spacing) * lineloop))
							* flip_factor;
			break;
		case 2: /* top */
			XY_offset = ref_Y
					- (text_height + (text_height + line_spacing) * lineloop)
							* flip_factor;
			break;
		default: /* bottom */
			XY_offset =
					ref_Y
							+ ((text_height + line_spacing)
									* (linecount - lineloop - 1)) * flip_factor;
			break;
		}
		printf("#6=%10f\t(Y offset will change every line)\n", XY_offset);

		printGCode(CurrentTextLine);
		lineloop++;
	}

	if (debug)
		printf("(SUCCESS)\nM2");
	return 0;
}
