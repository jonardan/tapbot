// SPDX-License-Identifier: MIT
/* Host test for drawing.c: make test */
#include <math.h>
#include <stdio.h>

#include "drawing.h"

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

static int ok(const char *line, struct dr_cmd *c)
{
	char err[80];

	return dr_parse(line, c, err, sizeof(err)) == 0;
}

static void test_parse(void)
{
	struct dr_cmd c;
	char err[80];

	CHECK(ok("G1 X10.5 Y-20 F1200", &c) && c.kind == DR_DRAW &&
	      c.has_x && c.x == 10.5 && c.has_y && c.y == -20 && c.has_f &&
	      c.f == 1200, "G1");
	CHECK(ok("g0 x1 y2 ; travel", &c) && c.kind == DR_TRAVEL &&
	      c.x == 1 && c.y == 2 && !c.has_f, "lower case, comment");
	CHECK(ok("G1 Y5", &c) && c.kind == DR_DRAW && !c.has_x && c.has_y,
	      "only Y");
	CHECK(ok("N10 G0 X1 (go) Y2 Z5", &c) && c.kind == DR_TRAVEL &&
	      c.y == 2, "N, parenthesis, Z");
	CHECK(ok("M3 S1000", &c) && c.kind == DR_TIP_DOWN, "M3");
	CHECK(ok("M5", &c) && c.kind == DR_TIP_UP, "M5");
	CHECK(ok("M2", &c) && c.kind == DR_END, "M2");
	CHECK(ok("G90", &c) && c.kind == DR_NONE, "G90");
	CHECK(ok("G21", &c) && c.kind == DR_NONE, "G21");
	CHECK(ok("", &c) && c.kind == DR_NONE, "empty");
	CHECK(ok("; tratto 3", &c) && c.kind == DR_NONE, "comment");
	CHECK(ok("%", &c) && c.kind == DR_NONE, "percent");

	CHECK(dr_parse("G1", &c, err, sizeof(err)) < 0, "G1 alone");
	CHECK(dr_parse("G91", &c, err, sizeof(err)) < 0, "G91");
	CHECK(dr_parse("G2 X1 Y1", &c, err, sizeof(err)) < 0, "G2");
	CHECK(dr_parse("X5", &c, err, sizeof(err)) < 0, "X alone");
	CHECK(dr_parse("G1 Xab", &c, err, sizeof(err)) < 0, "bad number");
	CHECK(dr_parse("G1 X1 F0", &c, err, sizeof(err)) < 0, "F0");
	CHECK(dr_parse("G1 X1 (open", &c, err, sizeof(err)) < 0, "open (");
	CHECK(dr_parse("G1 M3 X1", &c, err, sizeof(err)) < 0, "G and M");
	CHECK(dr_parse("Q1", &c, err, sizeof(err)) < 0, "Q");
}

static void test_turn(void)
{
	CHECK(fabs(dr_turn(0, 0, 1, 0, 2, 0)) < 1e-9, "straight");
	CHECK(fabs(dr_turn(0, 0, 1, 0, 1, 1) - 90) < 1e-9, "left 90");
	CHECK(fabs(dr_turn(0, 0, 1, 0, 1, -1) - 90) < 1e-9, "right 90");
	CHECK(fabs(dr_turn(0, 0, 1, 0, 0, 0) - 180) < 1e-9, "back");
	CHECK(dr_turn(0, 0, 0, 0, 1, 1) == 0, "empty leg");
	/* A 72-sided circle turns 5 degrees at every vertex */
	CHECK(fabs(dr_turn(1, 0, cos(5 * M_PI / 180), sin(5 * M_PI / 180),
			   cos(10 * M_PI / 180), sin(10 * M_PI / 180)) - 5) <
	      1e-9, "circle vertex");
}

int main(void)
{
	test_parse();
	test_turn();
	printf("test_drawing: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
