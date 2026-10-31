/* SPDX-License-Identifier: MIT */
/*
 * Straight lines of the tip: the line split into short, equal segments,
 * with the motor angles at every point.  Pure math, no hardware: it builds
 * and is tested on the PC (make test).
 */
#ifndef PATH_H
#define PATH_H

#include "fivebar.h"

#define LINE_SEG_MM	1.0	/* longest segment */
#define LINE_JUMP_DEG	10.0	/* most a motor may turn over one segment */

/* Points (ends included) for a line of len mm in segments <= seg mm */
int line_npts(double len, double seg);

/*
 * Motor angles th[0..npts-1] of npts evenly spaced points on the line from
 * the tip at motor angles start[] to (x, y).  th[0] is start[].  On error,
 * *bx / *by is the first point that fails: the fb_inverse() result, or
 * FB_JUMP when a motor would turn more than LINE_JUMP_DEG between two
 * points.
 */
enum fb_result line_plan(const struct fivebar *f, const double start[2],
			 double x, double y, int npts, double (*th)[2],
			 double *bx, double *by);

#endif
