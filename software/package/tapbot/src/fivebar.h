/* SPDX-License-Identifier: MIT */
/*
 * Kinematics of the tapbot five-bar: motor angles <-> tip X/Y.
 *
 * Pure math, no hardware: it builds and is tested on the PC (make test).
 *
 * Seen from above, motor side at the bottom: motor 1 (left) turns about
 * (-base/2, 0), motor 2 (right) about (+base/2, 0), +Y away from the
 * motors.  Each motor turns a proximal arm (motor axis to elbow axis); a
 * distal arm joins each elbow to the tip, where both meet.  The elbows
 * always point outward: the left one to -X, the right one to +X.
 *
 * The idler side is the same five-bar mirrored about the middle of the
 * work area, with free pivots at (±base/2, idler_y).  It carries no motor,
 * so it does not change the math; it is only checked, so the tip never
 * goes where the idler arms would fold flat or come apart.
 *
 * Angles in degrees, each proximal arm from the +X axis, counter-clockwise
 * positive seen from above.  X/Y in mm.
 */
#ifndef FIVEBAR_H
#define FIVEBAR_H

struct fivebar {
	double base;		/* motor axis to motor axis, mm */
	double l1;		/* proximal arm: motor axis to elbow axis, mm */
	double l2;		/* distal arm: elbow axis to tip axis, mm */
	double idler_y;		/* idler pivots at y = idler_y; 0: no idler */
	double min[2];		/* motor angle limits (deg), left and right */
	double max[2];
	int area_on;		/* the tip must stay inside area[] */
	double area[4];		/* x_min, x_max, y_min, y_max, mm */
};

enum fb_result {
	FB_OK = 0,
	FB_AREA,		/* outside the work area */
	FB_REACH,		/* an arm can't reach the point */
	FB_FLAT,		/* an elbow would fold or stretch almost flat */
	FB_SINGULAR,		/* the distal arms almost in line at the tip */
	FB_LIMITS,		/* reachable, but past a motor angle limit */
	FB_IDLER,		/* the idler side could not follow there */
	FB_JUMP,		/* path.c: a motor would jump between two points */
};

/*
 * Safety margins, in degrees: each elbow stays this far from folded flat
 * and from straight, and the two distal arms meet at the tip at an angle
 * between FB_TIP_DEG and 180 - FB_TIP_DEG.  Near those the arms lose
 * stiffness and one motor step moves the tip a lot.
 */
#define FB_ELBOW_DEG	10.0
#define FB_TIP_DEG	25.0

/*
 * Motor angles th[] that put the tip at (x, y), elbows out.  th[] is only
 * written on FB_OK.
 */
enum fb_result fb_inverse(const struct fivebar *f, double x, double y,
			  double th[2]);

/*
 * Tip X/Y for motor angles th[] (the assembly with the tip on the far side
 * of the elbows, as built).  FB_REACH if the distal arms can't meet; no
 * limit, area or margin check (fb_check does those).
 */
enum fb_result fb_forward(const struct fivebar *f, const double th[2],
			  double *x, double *y);

/* Everything fb_inverse checks, for a point; FB_OK if the tip may go there */
enum fb_result fb_check(const struct fivebar *f, double x, double y);

const char *fb_strerror(enum fb_result r);

#endif
