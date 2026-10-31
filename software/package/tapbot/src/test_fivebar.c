// SPDX-License-Identifier: MIT
/* Host test for fivebar.c: make test */
#include <math.h>
#include <stdio.h>

#include "fivebar.h"

/* As modelled in Fusion (Plotter5Bar_M4), and the limits of tapbot.conf */
static const struct fivebar fb = {
	.base = 50, .l1 = 85, .l2 = 125, .idler_y = 200,
	.min = { 60, -80 }, .max = { 260, 120 },
	.area_on = 1, .area = { -100, 100, 50, 150 },
};

static int failures, checks;

#define CHECK(cond, ...) do {						\
	checks++;							\
	if (!(cond)) {							\
		failures++;						\
		printf("FAIL %s:%d: ", __FILE__, __LINE__);		\
		printf(__VA_ARGS__);					\
		putchar('\n');						\
	}								\
} while (0)

static int near_eq(double a, double b, double tol)
{
	return fabs(a - b) <= tol;
}

static void test_known_points(void)
{
	double th[2], x, y;

	/* Middle of the area: the two sides mirror each other */
	CHECK(fb_inverse(&fb, 0, 100, th) == FB_OK, "IK of the middle");
	CHECK(near_eq(th[0] + th[1], 180, 1e-9) && near_eq(th[0], 158.7, 0.05),
	      "middle: %f %f", th[0], th[1]);

	/* Home, bottom-left corner (computed independently: 237.34, 93.34) */
	CHECK(fb_inverse(&fb, -100, 50, th) == FB_OK, "IK of home");
	CHECK(near_eq(th[0], 237.34, 0.01) && near_eq(th[1], 93.34, 0.01),
	      "home: %f %f", th[0], th[1]);
	CHECK(fb_forward(&fb, th, &x, &y) == FB_OK &&
	      near_eq(x, -100, 1e-9) && near_eq(y, 50, 1e-9),
	      "FK of home: %f %f", x, y);

	/* Both proximal arms along +Y: elbows at (±25, 85), tip above them */
	CHECK(fb_forward(&fb, (double[]){ 90, 90 }, &x, &y) == FB_OK &&
	      near_eq(x, 0, 1e-9) &&
	      near_eq(y, 85 + sqrt(125.0 * 125 - 25 * 25), 1e-9),
	      "FK arms up: %f %f", x, y);

	/* Elbows (-110, 0) and (110, 0): 220 mm apart, the arms still meet */
	CHECK(fb_forward(&fb, (double[]){ 180, 0 }, &x, &y) == FB_OK,
	      "elbows 220 mm apart");
	{
		struct fivebar w = fb;

		/* Now 270 mm apart, more than two distal arms */
		w.base = 100;
		CHECK(fb_forward(&w, (double[]){ 180, 0 }, &x, &y) == FB_REACH,
		      "elbows too far apart");
	}
}

/* Every point of the area, 1 mm apart: reachable, and FK gives it back */
static void test_whole_area(void)
{
	double th[2], x, y, lo[2] = { 1e9, 1e9 }, hi[2] = { -1e9, -1e9 };
	int bad = 0, n = 0;

	for (int i = -100; i <= 100; i++) {
		for (int j = 50; j <= 150; j++) {
			enum fb_result r = fb_inverse(&fb, i, j, th);

			n++;
			if (r != FB_OK) {
				if (!bad++)
					printf("  %d, %d: %s\n", i, j,
					       fb_strerror(r));
				continue;
			}
			if (fb_forward(&fb, th, &x, &y) != FB_OK ||
			    !near_eq(x, i, 1e-9) || !near_eq(y, j, 1e-9)) {
				if (!bad++)
					printf("  %d, %d: FK %f %f\n", i, j,
					       x, y);
			}
			for (int k = 0; k < 2; k++) {
				lo[k] = fmin(lo[k], th[k]);
				hi[k] = fmax(hi[k], th[k]);
			}
		}
	}
	CHECK(!bad, "%d of %d points of the area fail", bad, n);
	printf("  motor angles over the area: left %.1f..%.1f, right "
	       "%.1f..%.1f deg\n", lo[0], hi[0], lo[1], hi[1]);
	/* The limits leave some room around the area */
	CHECK(lo[0] - fb.min[0] > 10 && fb.max[0] - hi[0] > 10 &&
	      lo[1] - fb.min[1] > 10 && fb.max[1] - hi[1] > 10,
	      "limits too tight for the area");
}

/* The machine is symmetric about x = 0 */
static void test_mirror(void)
{
	double a[2], b[2];

	for (int i = 0; i <= 100; i += 10) {
		for (int j = 50; j <= 150; j += 10) {
			if (fb_inverse(&fb, i, j, a) != FB_OK ||
			    fb_inverse(&fb, -i, j, b) != FB_OK)
				continue;
			CHECK(near_eq(a[0], 180 - b[1], 1e-9) &&
			      near_eq(a[1], 180 - b[0], 1e-9),
			      "mirror %d %d: %f %f / %f %f", i, j, a[0], a[1],
			      b[0], b[1]);
		}
	}
}

static void test_refusals(void)
{
	struct fivebar f = fb;
	double th[2] = { 1, 2 };

	CHECK(fb_inverse(&fb, -100.5, 50, th) == FB_AREA, "left of the area");
	CHECK(fb_inverse(&fb, 0, 151, th) == FB_AREA, "above the area");
	CHECK(th[0] == 1 && th[1] == 2, "th written on error");

	f.area_on = 0;
	CHECK(fb_inverse(&f, 0, 260, th) == FB_REACH, "too far");
	CHECK(fb_inverse(&f, -25, 35, th) == FB_REACH, "too close");
	/* Right under the idler pivots: the idler arms can't fold that far */
	CHECK(fb_inverse(&f, 0, 190, th) == FB_IDLER, "idler: %s",
	      fb_strerror(fb_inverse(&f, 0, 190, th)));
	f.idler_y = 0;
	CHECK(fb_inverse(&f, 0, 190, th) == FB_OK, "no idler: %s",
	      fb_strerror(fb_inverse(&f, 0, 190, th)));

	/* Home is at 237 deg on the left motor */
	f = fb;
	f.max[0] = 230;
	CHECK(fb_inverse(&f, -100, 50, th) == FB_LIMITS, "left limit");
	/* Limits given across 0 / 360 still match */
	f = fb;
	f.min[0] = -300;
	f.max[0] = -100;
	CHECK(fb_inverse(&f, -100, 50, th) == FB_OK &&
	      near_eq(th[0], 237.34 - 360, 0.01), "limits a turn away: %f",
	      th[0]);
}

/*
 * How far the tip moves for one microstep of either motor (1/16 of a
 * 1.8 deg step), at its worst over the area: the positioning resolution
 */
static void test_resolution(void)
{
	const double ustep = 1.8 / 16;
	double worst = 0, th[2], x0, y0, x, y;

	for (int i = -100; i <= 100; i += 5) {
		for (int j = 50; j <= 150; j += 5) {
			if (fb_inverse(&fb, i, j, th) != FB_OK)
				continue;
			fb_forward(&fb, th, &x0, &y0);
			for (int k = 0; k < 2; k++) {
				double t[2] = { th[0], th[1] };

				t[k] += ustep;
				fb_forward(&fb, t, &x, &y);
				worst = fmax(worst, hypot(x - x0, y - y0));
			}
		}
	}
	printf("  one microstep moves the tip %.3f mm at most\n", worst);
	CHECK(worst < 0.5, "resolution %f mm", worst);
}

int main(void)
{
	test_known_points();
	test_whole_area();
	test_mirror();
	test_refusals();
	test_resolution();
	printf("test_fivebar: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
