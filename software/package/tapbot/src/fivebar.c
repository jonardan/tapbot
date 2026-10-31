// SPDX-License-Identifier: MIT
/*
 * Kinematics of the tapbot five-bar (see fivebar.h).
 *
 * Inverse, one arm at a time: with d the distance from the motor axis to
 * the tip and φ its direction,
 *
 *   cos α = (l1² + d² - l2²) / (2 l1 d)
 *   θ     = φ ± α
 *
 * where the sign puts the elbow on the outer side (+ for the left motor,
 * - for the right one).  Forward: the tip is where the two circles of
 * radius l2 about the elbows cross, on the side away from the motors.
 */
#include <math.h>
#include <stddef.h>

#include "fivebar.h"

#define DEG	(M_PI / 180.0)
/*
 * Rounding slack, so a point exactly on the edge of the area or exactly
 * on a limit (FK of such a pose) still passes
 */
#define AREA_EPS 1e-6	/* mm */
#define LIM_EPS	1e-6	/* deg */

/* One arm, motor (or idler pivot) axis at (ax, ay) */
struct chain {
	double th;		/* proximal arm angle, deg */
	double ex, ey;		/* elbow axis */
};

/*
 * Solve one arm for the tip at (x, y); out = +1 turns the elbow
 * counter-clockwise from the axis-to-tip line, -1 clockwise
 */
static enum fb_result arm(const struct fivebar *f, double ax, double ay,
			  int out, double x, double y, struct chain *c)
{
	double dx = x - ax, dy = y - ay, d = hypot(dx, dy);
	double ca, cg, elbow;

	if (d > f->l1 + f->l2 || d < fabs(f->l1 - f->l2) || d == 0)
		return FB_REACH;
	ca = (f->l1 * f->l1 + d * d - f->l2 * f->l2) / (2.0 * f->l1 * d);
	c->th = (atan2(dy, dx) + out * acos(fmax(-1.0, fmin(1.0, ca)))) / DEG;
	c->ex = ax + f->l1 * cos(c->th * DEG);
	c->ey = ay + f->l1 * sin(c->th * DEG);
	/* The angle between the two arms at the elbow: 0 folded, 180 straight */
	cg = (f->l1 * f->l1 + f->l2 * f->l2 - d * d) / (2.0 * f->l1 * f->l2);
	elbow = acos(fmax(-1.0, fmin(1.0, cg))) / DEG;
	if (elbow < FB_ELBOW_DEG || elbow > 180.0 - FB_ELBOW_DEG)
		return FB_FLAT;
	return FB_OK;
}

/* The angle (deg) between the two distal arms where they meet at the tip */
static double tip_angle(const struct chain *a, const struct chain *b,
			double x, double y)
{
	double ux = a->ex - x, uy = a->ey - y, vx = b->ex - x, vy = b->ey - y;

	return fabs(atan2(ux * vy - uy * vx, ux * vx + uy * vy)) / DEG;
}

/* Both arms of one side, plus the angle at the tip */
static enum fb_result side(const struct fivebar *f, double y0, int out,
			   double x, double y, struct chain c[2])
{
	enum fb_result r;
	double t;

	r = arm(f, -f->base / 2, y0, out, x, y, &c[0]);
	if (r == FB_OK)
		r = arm(f, f->base / 2, y0, -out, x, y, &c[1]);
	if (r != FB_OK)
		return r;
	t = tip_angle(&c[0], &c[1], x, y);
	if (t < FB_TIP_DEG || t > 180.0 - FB_TIP_DEG)
		return FB_SINGULAR;
	return FB_OK;
}

/*
 * Bring angle t inside min..max by whole turns if it can be; returns 0 if
 * it fits.  atan2 gives -180..180, but the left motor works around 180.
 */
static int fit(double *t, double min, double max)
{
	for (int k = -1; k <= 1; k++) {
		double v = *t + 360.0 * k;

		if (v >= min - LIM_EPS && v <= max + LIM_EPS) {
			*t = fmax(min, fmin(max, v));
			return 0;
		}
	}
	return -1;
}

static enum fb_result solve(const struct fivebar *f, double x, double y,
			    double th[2])
{
	struct chain m[2], idl[2];
	enum fb_result r;

	if (f->area_on &&
	    (x < f->area[0] - AREA_EPS || x > f->area[1] + AREA_EPS ||
	     y < f->area[2] - AREA_EPS || y > f->area[3] + AREA_EPS))
		return FB_AREA;
	/* Motor side: the left elbow turns counter-clockwise, out to -X */
	r = side(f, 0, 1, x, y, m);
	if (r != FB_OK)
		return r;
	th[0] = m[0].th;
	th[1] = m[1].th;
	if (fit(&th[0], f->min[0], f->max[0]) ||
	    fit(&th[1], f->min[1], f->max[1]))
		return FB_LIMITS;
	/* The idler side is the mirror image: its left elbow turns clockwise */
	if (f->idler_y != 0 && side(f, f->idler_y, -1, x, y, idl) != FB_OK)
		return FB_IDLER;
	return FB_OK;
}

enum fb_result fb_inverse(const struct fivebar *f, double x, double y,
			  double th[2])
{
	double t[2];
	enum fb_result r = solve(f, x, y, t);

	if (r == FB_OK) {
		th[0] = t[0];
		th[1] = t[1];
	}
	return r;
}

enum fb_result fb_check(const struct fivebar *f, double x, double y)
{
	double t[2];

	return solve(f, x, y, t);
}

enum fb_result fb_forward(const struct fivebar *f, const double th[2],
			  double *x, double *y)
{
	double x1 = -f->base / 2 + f->l1 * cos(th[0] * DEG);
	double y1 = f->l1 * sin(th[0] * DEG);
	double x2 = f->base / 2 + f->l1 * cos(th[1] * DEG);
	double y2 = f->l1 * sin(th[1] * DEG);
	double dx = x2 - x1, dy = y2 - y1, d = hypot(dx, dy), h;

	if (d == 0 || d > 2.0 * f->l2)
		return FB_REACH;
	h = sqrt(f->l2 * f->l2 - d * d / 4.0);
	/*
	 * Left of the line from the left elbow to the right one: with the
	 * elbows out that is the side away from the motors
	 */
	*x = (x1 + x2) / 2 - h * dy / d;
	*y = (y1 + y2) / 2 + h * dx / d;
	return FB_OK;
}

const char *fb_strerror(enum fb_result r)
{
	switch (r) {
	case FB_OK: return "ok";
	case FB_AREA: return "outside the work area (area_mm)";
	case FB_REACH: return "out of reach of the arms";
	case FB_FLAT: return "an elbow would fold or stretch almost flat";
	case FB_SINGULAR: return "the distal arms would be almost in line";
	case FB_LIMITS: return "reachable only past a motor angle limit";
	case FB_IDLER: return "the idler arms could not follow there";
	case FB_JUMP: return "a motor would jump between two points";
	}
	return "unknown error";
}
