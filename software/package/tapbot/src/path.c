// SPDX-License-Identifier: MIT
/*
 * Straight lines of the tip (see path.h).
 *
 * A move of both motors from A to B traces a curve; a line is drawn by
 * going through IK points about a millimetre apart, so between two of
 * them the tip strays from the line by a negligible amount.
 */
#include <math.h>
#include <stddef.h>

#include "path.h"

int line_npts(double len, double seg)
{
	int n = (int)ceil(len / seg - 1e-9);

	return (n < 1 ? 1 : n) + 1;
}

enum fb_result line_plan(const struct fivebar *f, const double start[2],
			 double x, double y, int npts, double (*th)[2],
			 double *bx, double *by)
{
	double x0, y0;
	int i;

	if (fb_forward(f, start, &x0, &y0) != FB_OK) {
		*bx = *by = NAN;
		return FB_REACH;
	}
	th[0][0] = start[0];
	th[0][1] = start[1];
	for (i = 1; i < npts; i++) {
		double t = (double)i / (npts - 1);
		double px = x0 + (x - x0) * t, py = y0 + (y - y0) * t;
		enum fb_result r;

		if (i == npts - 1) {
			px = x;
			py = y;
		}
		r = fb_inverse(f, px, py, th[i]);
		if (r == FB_OK &&
		    (fabs(th[i][0] - th[i - 1][0]) > LINE_JUMP_DEG ||
		     fabs(th[i][1] - th[i - 1][1]) > LINE_JUMP_DEG))
			r = FB_JUMP;
		if (r != FB_OK) {
			*bx = px;
			*by = py;
			return r;
		}
	}
	return FB_OK;
}
