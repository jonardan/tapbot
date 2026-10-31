// SPDX-License-Identifier: MIT
/* Host test for path.c: make test */
#include <math.h>
#include <stdio.h>

#include "path.h"

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

#define MAXPTS	1000

static double th[MAXPTS][2];

/* Every point lies on the segment (x0,y0)-(x1,y1), evenly spaced */
static void check_on_line(int n, double x0, double y0, double x1, double y1)
{
	double len = hypot(x1 - x0, y1 - y0), x, y;

	for (int i = 0; i < n; i++) {
		double t = (double)i / (n - 1);

		fb_forward(&fb, th[i], &x, &y);
		CHECK(fabs(x - (x0 + (x1 - x0) * t)) < 1e-6 &&
		      fabs(y - (y0 + (y1 - y0) * t)) < 1e-6,
		      "point %d of %d at %f %f", i, n, x, y);
	}
	CHECK(len / (n - 1) <= LINE_SEG_MM + 1e-9, "segments too long");
}

static void test_npts(void)
{
	CHECK(line_npts(0, 1) == 2, "zero length: %d", line_npts(0, 1));
	CHECK(line_npts(1, 1) == 2, "1 mm: %d", line_npts(1, 1));
	CHECK(line_npts(1.5, 1) == 3, "1.5 mm: %d", line_npts(1.5, 1));
	CHECK(line_npts(100, 1) == 101, "100 mm: %d", line_npts(100, 1));
}

/* From home to every corner and along every edge of the area */
static void test_corners(void)
{
	static const double c[4][2] = {
		{ -100, 50 }, { 100, 50 }, { 100, 150 }, { -100, 150 },
	};
	double start[2], bx, by;

	for (int a = 0; a < 4; a++) {
		for (int b = 0; b < 4; b++) {
			double len = hypot(c[b][0] - c[a][0],
					   c[b][1] - c[a][1]);
			int n = line_npts(len, LINE_SEG_MM);
			enum fb_result r;

			if (a == b)
				continue;
			fb_inverse(&fb, c[a][0], c[a][1], start);
			r = line_plan(&fb, start, c[b][0], c[b][1], n, th,
				      &bx, &by);
			CHECK(r == FB_OK, "corner %d to %d: %s at %f %f", a, b,
			      fb_strerror(r), bx, by);
			if (r == FB_OK)
				check_on_line(n, c[a][0], c[a][1], c[b][0],
					      c[b][1]);
		}
	}
}

/* A line leaving the area stops at its edge */
static void test_leave_area(void)
{
	double start[2], bx, by;
	int n = line_npts(120, LINE_SEG_MM);

	fb_inverse(&fb, 0, 100, start);
	CHECK(line_plan(&fb, start, 0, 220, n, th, &bx, &by) == FB_AREA &&
	      by > 150 && by <= 151, "out of the top: %f %f", bx, by);
}

int main(void)
{
	test_npts();
	test_corners();
	test_leave_area();
	printf("test_path: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
