/* SPDX-License-Identifier: MIT */
/*
 * Drawings: the G-code the drawing page sends, one line at a time, without
 * moving anything.  Pure parsing, no hardware: it builds and is tested on
 * the PC (make test).
 *
 * Supported: G0 X Y (travel, tip up), G1 X Y F (draw a straight line at
 * feed F, mm/min), M3 / M5 (tip down / up), G90 and G21 (absolute mm, the
 * only mode), M2 / M30 (end).  A missing X or Y keeps the last one.
 * Comments ';' to the end of the line and '( )'; N line numbers are
 * skipped; Z and S are accepted and ignored.
 */
#ifndef DRAWING_H
#define DRAWING_H

#include <stddef.h>

enum dr_kind {
	DR_NONE = 0,		/* empty, comment, G90, G21 */
	DR_TRAVEL,		/* G0 */
	DR_DRAW,		/* G1 */
	DR_TIP_DOWN,		/* M3 */
	DR_TIP_UP,		/* M5 */
	DR_END,			/* M2 / M30 */
};

struct dr_cmd {
	enum dr_kind kind;
	int has_x, has_y, has_f;
	double x, y;		/* mm */
	double f;		/* feed, mm/min */
};

/*
 * Parse one line (no newline needed).  Returns 0, or -1 with a message in
 * err[errlen].
 */
int dr_parse(const char *line, struct dr_cmd *c, char *err, size_t errlen);

/*
 * How much the path turns at (bx, by) going from (ax, ay) to (cx, cy),
 * in degrees: 0 straight on, 180 straight back.  0 if a leg is empty.
 */
double dr_turn(double ax, double ay, double bx, double by, double cx,
	       double cy);

#endif
