// SPDX-License-Identifier: MIT
/*
 * tapbot - XY commands for the five-bar phone tapper.
 *
 * Motor 1 turns the left proximal arm, motor 2 the right one (seen from
 * above, motor side at the bottom).  Angles are in degrees from the +X
 * axis, counter-clockwise positive seen from above; a positive
 * 'tmc2209ctl turn' must turn the arm counter-clockwise, or set
 * invert_dir = 1 for that motor.  X/Y in mm, origin halfway between the
 * motor axes, +Y away from the motors (see fivebar.h).
 *
 * There are no endstops: at power-on the arms sit in the home pose
 * (home_mm: both motor arms along the motor line, pointing outward, the
 * left one at 180 deg and the right one at 0 deg), and the boot script runs 'tapbot sethome' to say so.  From then
 * on /run/tapbot.pos counts every step.  'tapbot home' brings the tip
 * back there before switching off.
 *
 * Settings are read from /etc/tapbot.conf (see tmc2209.h).
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawing.h"
#include "fivebar.h"
#include "path.h"
#include "tmc2209.h"

#define NMOTORS	2

static const char *const motor_name[NMOTORS] = { "left", "right" };

/* Set by 'gcode': moves don't print where they end up */
static int quiet;
/* A move was stopped by Ctrl-C */
static int stopped;

static void usage(void)
{
	printf(
"Usage: tapbot [-c CONF] COMMAND [ARGS]\n"
"\n"
"  sethome               the arms are in the home pose now, tip at (%.2f, %.2f):\n"
"                        start counting from there (done at boot)\n"
"  where                 print the motor angles and the tip X/Y\n"
"  move X Y [F]          move the tip to X/Y (mm) in a straight line at\n"
"                        F mm/s (default line_speed, %.0f)\n"
"  home [F]              move the tip back to the home pose; do it\n"
"                        before switching off\n"
"  corners [F]           go round the edge of the work area, then home\n"
"  gcode [-n] FILE       run a drawing: G0 X Y (travel), G1 X Y F (draw,\n"
"                        F in mm/min), M3 / M5 (tip down / up, for the\n"
"                        solenoid); every point is checked first, and\n"
"                        the strokes run in the order of the file.\n"
"                        FILE - reads stdin; -n only checks\n"
"  fk A1 A2              print the tip X/Y for motor angles A1 A2\n"
"  ik X Y                print the motor angles for tip X/Y\n"
"                        fk and ik only compute, they don't move\n"
"  jog M DEG [RPM]       turn motor M (1 left, 2 right) by DEG degrees,\n"
"                        with no check at all: for bring-up only\n"
"\n"
"  Work area: x %.0f..%.0f, y %.0f..%.0f mm (area_mm); X/Y from halfway\n"
"  between the motor axes, +Y away from the motors.  Angles from the +X\n"
"  axis, counter-clockwise seen from above.  Ctrl-C decelerates a move\n"
"  to a stop.\n", motors[0].home_mm[0], motors[0].home_mm[1],
	motors[0].line_speed, motors[0].area_mm[0], motors[0].area_mm[1],
	motors[0].area_mm[2], motors[0].area_mm[3]);
}

/* Parse a number, rejecting trailing junk like "10x" */
static int parse_num(const char *s, double *v)
{
	char *end;

	*v = strtod(s, &end);
	if (end == s || *end || !isfinite(*v)) {
		err("'%s' is not a number", s);
		return -1;
	}
	return 0;
}

/* The geometry, limits and work area from the config file */
static int get_fivebar(struct fivebar *f)
{
	const struct config *c = &motors[0];

	if (nmotors < NMOTORS) {
		err("tapbot needs [motor1] and [motor2] in %s", CONF_PATH);
		return -1;
	}
	f->base = c->base_mm;
	f->l1 = c->proximal_mm;
	f->l2 = c->distal_mm;
	f->idler_y = c->idler_y_mm;
	if (f->base <= 0 || f->l1 <= 0 || f->l2 <= 0) {
		err("set base_mm, proximal_mm and distal_mm in %s", CONF_PATH);
		return -1;
	}
	for (int m = 0; m < NMOTORS; m++) {
		f->min[m] = motors[m].min_angle;
		f->max[m] = motors[m].max_angle;
		if (f->min[m] >= f->max[m]) {
			err("motor %d (%s): set min_angle < max_angle in "
			    "[motor%d] of %s", m + 1, motor_name[m], m + 1,
			    CONF_PATH);
			return -1;
		}
	}
	memcpy(f->area, c->area_mm, sizeof(f->area));
	f->area_on = 1;
	if (f->area[0] >= f->area[1] || f->area[2] >= f->area[3]) {
		err("set area_mm (x_min x_max y_min y_max) in %s", CONF_PATH);
		return -1;
	}
	return 0;
}

/* Current motor angles into th[]; 0 if the position is known */
static int get_angles(double th[NMOTORS])
{
	int m, ok = 1;

	pos_load();
	for (m = 0; m < NMOTORS; m++) {
		double upd = usteps_per_deg(m);

		if (upd <= 0)
			return -1;
		th[m] = pos[m] / upd;
		ok &= homed[m];
	}
	return ok ? 0 : 1;
}

static int cmd_where(void)
{
	double th[NMOTORS], x, y;
	struct fivebar f;
	int m, r = get_angles(th);

	if (r < 0)
		return -1;
	for (m = 0; m < NMOTORS; m++)
		printf("motor %d (%s): %8.2f deg%s\n", m + 1, motor_name[m],
		       th[m], homed[m] ? "" :
		       "  (position unknown: run 'tapbot sethome')");
	if (r == 0 && get_fivebar(&f) == 0) {
		if (fb_forward(&f, th, &x, &y) == FB_OK)
			printf("tip: x %8.2f mm, y %8.2f mm\n", x, y);
		else
			printf("tip: the distal arms can't meet at these "
			       "angles\n");
	}
	return 0;
}

/* sethome: the tip is at home_mm now; no motor moves */
static int cmd_sethome(void)
{
	const double *h = motors[0].home_mm;
	double th[NMOTORS];
	enum fb_result r;
	struct fivebar f;

	if (get_fivebar(&f))
		return -1;
	r = fb_inverse(&f, h[0], h[1], th);
	if (r != FB_OK) {
		err("home_mm %.2f, %.2f: %s", h[0], h[1], fb_strerror(r));
		return -1;
	}
	for (int m = 0; m < NMOTORS; m++) {
		double upd = usteps_per_deg(m);

		if (upd <= 0)
			return -1;
		pos_set(m, lround(th[m] * upd));
	}
	pos_load();
	if (!homed[0] || !homed[1]) {
		err("could not write %s", POS_PATH);
		return -1;
	}
	return cmd_where();
}

static int cmd_fk(int argc, char **argv)
{
	double th[NMOTORS], x, y;
	enum fb_result r;
	struct fivebar f;

	if (argc != 3) {
		err("usage: fk A1 A2");
		return -1;
	}
	if (parse_num(argv[1], &th[0]) || parse_num(argv[2], &th[1]) ||
	    get_fivebar(&f))
		return -1;
	if (fb_forward(&f, th, &x, &y) != FB_OK) {
		err("the distal arms can't meet at these angles");
		return -1;
	}
	printf("tip: x %8.2f mm, y %8.2f mm\n", x, y);
	r = fb_check(&f, x, y);
	if (r != FB_OK)
		printf("(not a place the tip may go: %s)\n", fb_strerror(r));
	return 0;
}

static int cmd_ik(int argc, char **argv)
{
	double x, y, th[NMOTORS];
	enum fb_result r;
	struct fivebar f;

	if (argc != 3) {
		err("usage: ik X Y");
		return -1;
	}
	if (parse_num(argv[1], &x) || parse_num(argv[2], &y) ||
	    get_fivebar(&f))
		return -1;
	r = fb_inverse(&f, x, y, th);
	if (r != FB_OK) {
		err("%.2f, %.2f: %s", x, y, fb_strerror(r));
		return -1;
	}
	for (int m = 0; m < NMOTORS; m++)
		printf("motor %d (%s): %8.2f deg\n", m + 1, motor_name[m],
		       th[m]);
	return 0;
}

/*
 * The tip goes through xy[0..n-1] in straight lines at speed mm/s, in one
 * stepping loop: it accelerates at the start and stops only at the end.
 * Every point is checked before anything moves.
 */
static int path_to(const struct fivebar *f, int n, const double (*xy)[2],
		   double speed)
{
	static const int both[NMOTORS] = { 0, 1 };
	double x0, y0, now[NMOTORS], upd[NMOTORS], (*th)[NMOTORS] = NULL;
	double asked, bx, by, *dist = NULL;
	enum fb_result r;
	int i, m, k, np, off, npts, interrupted, ret = -1;
	long *pts = NULL;

	if (speed <= 0) {
		err("speed must be > 0 mm/s");
		return -1;
	}
	if (get_angles(now)) {
		err("position unknown: put the arms in the home pose and run "
		    "'tapbot sethome'");
		return -1;
	}
	for (m = 0; m < NMOTORS; m++) {
		upd[m] = usteps_per_deg(m);
		if (upd[m] <= 0)
			return -1;
	}
	if (fb_forward(f, now, &x0, &y0) != FB_OK) {
		err("the arms are in a pose the tip can't be in; run "
		    "'tapbot where'");
		return -1;
	}
	npts = 1;
	for (k = 0, bx = x0, by = y0; k < n; k++) {
		npts += line_npts(hypot(xy[k][0] - bx, xy[k][1] - by),
				  LINE_SEG_MM) - 1;
		bx = xy[k][0];
		by = xy[k][1];
	}
	th = malloc(npts * sizeof(*th));
	pts = malloc(npts * NMOTORS * sizeof(*pts));
	dist = malloc(npts * sizeof(*dist));
	if (!th || !pts || !dist) {
		err("out of memory");
		goto out;
	}
	memcpy(th[0], now, sizeof(th[0]));
	dist[0] = 0;
	for (k = 0, off = 0; k < n; k++) {
		double len = hypot(xy[k][0] - x0, xy[k][1] - y0);

		np = line_npts(len, LINE_SEG_MM);
		r = line_plan(f, th[off], xy[k][0], xy[k][1], np, th + off,
			      &bx, &by);
		if (r != FB_OK) {
			err("line to %.2f, %.2f: at %.2f, %.2f: %s", xy[k][0],
			    xy[k][1], bx, by, fb_strerror(r));
			goto out;
		}
		for (i = 1; i < np; i++)
			dist[off + i] = dist[off] + len * i / (np - 1);
		off += np - 1;
		x0 = xy[k][0];
		y0 = xy[k][1];
	}
	/* Point 0 is exactly where the motors are */
	for (m = 0; m < NMOTORS; m++)
		pts[m] = pos[m];
	for (i = 1; i < npts; i++)
		for (m = 0; m < NMOTORS; m++)
			pts[i * NMOTORS + m] = lround(th[i][m] * upd[m]);

	asked = speed;
	if (move_path(NMOTORS, both, npts, pts, dist, &speed,
		      motors[0].line_accel, &interrupted))
		goto out;
	if (speed < asked && !quiet)
		printf("speed lowered to %.1f mm/s: a motor would go over its "
		       "rpm\n", speed);
	if (interrupted) {
		printf("interrupted\n");
		stopped = 1;
	}
	ret = quiet ? 0 : cmd_where();
out:
	free(th);
	free(pts);
	free(dist);
	return ret;
}

/* Optional speed argument argv[i], default line_speed */
static int get_speed(int argc, char **argv, int i, double *speed)
{
	*speed = motors[0].line_speed;
	return argc > i ? parse_num(argv[i], speed) : 0;
}

/* move X Y [F] */
static int cmd_move(int argc, char **argv)
{
	double xy[1][2], speed;
	struct fivebar f;

	if (argc < 3 || argc > 4) {
		err("usage: move X Y [F]");
		return -1;
	}
	if (parse_num(argv[1], &xy[0][0]) || parse_num(argv[2], &xy[0][1]) ||
	    get_speed(argc, argv, 3, &speed) || get_fivebar(&f))
		return -1;
	return path_to(&f, 1, (const double (*)[2])xy, speed);
}

/* home [F] */
static int cmd_home(int argc, char **argv)
{
	double xy[1][2], speed;
	struct fivebar f;

	if (argc > 2) {
		err("usage: home [F]");
		return -1;
	}
	if (get_speed(argc, argv, 1, &speed) || get_fivebar(&f))
		return -1;
	memcpy(xy[0], motors[0].home_mm, sizeof(xy[0]));
	return path_to(&f, 1, (const double (*)[2])xy, speed);
}

/*
 * corners [F]: from home round the edge of the work area, counter-
 * clockwise, and back home: one path, so it stops only at the end
 */
static int cmd_corners(int argc, char **argv)
{
	double xy[6][2], speed;
	const double *a = motors[0].area_mm;
	struct fivebar f;

	if (argc > 2) {
		err("usage: corners [F]");
		return -1;
	}
	if (get_speed(argc, argv, 1, &speed) || get_fivebar(&f))
		return -1;
	xy[0][0] = a[0]; xy[0][1] = a[2];
	xy[1][0] = a[1]; xy[1][1] = a[2];
	xy[2][0] = a[1]; xy[2][1] = a[3];
	xy[3][0] = a[0]; xy[3][1] = a[3];
	xy[4][0] = a[0]; xy[4][1] = a[2];
	memcpy(xy[5], motors[0].home_mm, sizeof(xy[5]));
	return path_to(&f, 6, (const double (*)[2])xy, speed);
}

/*
 * gcode [-n] FILE: run a drawing (see drawing.h).  FILE - reads stdin up
 * to M2 or the end.  The whole drawing is read and every point of it
 * checked before anything moves; then the moves run in the order of the
 * file.  Consecutive G1 lines at the same feed run as one path, without
 * stopping between them, while the path turns by at most corner_deg at
 * each joint (curves made of short segments); a sharper corner stops
 * there.  Each stroke (a run of G1 lines) is announced as "stroke K of M"
 * when it starts.  M3 / M5 will lower and lift the tip once the solenoid
 * is in; for now they do nothing, so travel (G0) draws too.  -n only
 * checks.
 */
#define TRAVEL_SPEED_MUL 2	/* G0 runs at line_speed times this */

struct op {
	enum dr_kind kind;	/* DR_TRAVEL, DR_DRAW, DR_TIP_DOWN, DR_TIP_UP */
	double x, y, speed;	/* moves: target (mm) and speed (mm/s) */
	int stroke;		/* moves: the stroke a G1 starts, else 0 */
	int line;		/* in the file */
};

/* Read and check the whole drawing into *ops; the number of strokes */
static int gcode_load(const struct fivebar *f, FILE *in, const char *name,
		      double x0, double y0, struct op **ops, int *nops)
{
	double x = x0, y = y0, feed = motors[0].line_speed;
	struct op *v = NULL;
	struct dr_cmd c;
	char *buf = NULL, msg[96];
	size_t cap = 0;
	ssize_t len;
	int n = 0, size = 0, line = 0, strokes = 0, drawing = 0, ret = -1;

	while ((len = getline(&buf, &cap, in)) >= 0) {
		line++;
		while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
			buf[--len] = '\0';
		if (dr_parse(buf, &c, msg, sizeof(msg))) {
			err("%s:%d: %s", name, line, msg);
			goto out;
		}
		if (c.kind == DR_NONE)
			continue;
		if (c.kind == DR_END)
			break;
		if (n == size) {
			struct op *nv;

			size = size ? 2 * size : 256;
			nv = realloc(v, size * sizeof(*v));
			if (!nv) {
				err("out of memory");
				goto out;
			}
			v = nv;
		}
		memset(&v[n], 0, sizeof(v[n]));
		v[n].kind = c.kind;
		v[n].line = line;
		if (c.kind == DR_TRAVEL || c.kind == DR_DRAW) {
			double nx = c.has_x ? c.x : x, ny = c.has_y ? c.y : y;
			double d = hypot(nx - x, ny - y);
			enum fb_result r = FB_OK;
			int k, steps = (int)ceil(d / LINE_SEG_MM);

			/* Every millimetre of the way, and the end */
			for (k = 1; k <= steps && r == FB_OK; k++)
				r = fb_check(f, x + (nx - x) * k / steps,
					     y + (ny - y) * k / steps);
			if (r == FB_OK)
				r = fb_check(f, nx, ny);
			if (r != FB_OK) {
				err("%s:%d: %.2f, %.2f: %s", name, line, nx, ny,
				    fb_strerror(r));
				goto out;
			}
			/* Already there: nothing to move */
			if (d < 1e-6)
				continue;
			if (c.kind == DR_DRAW && c.has_f)
				feed = c.f / 60.0;
			v[n].x = x = nx;
			v[n].y = y = ny;
			v[n].speed = c.kind == DR_DRAW ? feed :
				     motors[0].line_speed * TRAVEL_SPEED_MUL;
			if (c.kind == DR_DRAW && !drawing)
				v[n].stroke = ++strokes;
			drawing = c.kind == DR_DRAW;
		} else {
			drawing = 0;
		}
		n++;
	}
	*ops = v;
	*nops = n;
	v = NULL;
	ret = strokes;
out:
	free(v);
	free(buf);
	return ret;
}

static int cmd_gcode(int argc, char **argv)
{
	int dry = argc > 1 && !strcmp(argv[1], "-n");
	double th[NMOTORS], x0, y0, (*xy)[2] = NULL;
	struct op *ops = NULL;
	struct fivebar f;
	const char *name;
	FILE *in;
	int i, j, n, nops = 0, strokes, ret = -1;

	if (argc != 2 + dry) {
		err("usage: gcode [-n] FILE   (FILE - reads stdin)");
		return -1;
	}
	if (get_fivebar(&f))
		return -1;
	name = argv[1 + dry];
	/* From where the tip is; a check alone may start from home */
	if (get_angles(th) == 0 && fb_forward(&f, th, &x0, &y0) == FB_OK) {
		/* x0, y0 set */
	} else if (dry) {
		x0 = motors[0].home_mm[0];
		y0 = motors[0].home_mm[1];
	} else {
		err("position unknown: put the arms in the home pose and run "
		    "'tapbot sethome'");
		return -1;
	}
	in = strcmp(name, "-") ? fopen(name, "r") : stdin;
	if (!in) {
		err("%s: cannot open", name);
		return -1;
	}
	strokes = gcode_load(&f, in, name, x0, y0, &ops, &nops);
	if (in != stdin)
		fclose(in);
	if (strokes < 0)
		goto out;
	printf("%s: %d strokes, all inside the work area\n", name, strokes);
	if (dry) {
		ret = 0;
		goto out;
	}
	xy = malloc((nops + 1) * sizeof(*xy));
	if (!xy) {
		err("out of memory");
		goto out;
	}
	quiet = 1;
	for (i = 0; i < nops; i = j) {
		const struct op *o = &ops[i];
		double px = x0, py = y0;

		if (o->kind == DR_TIP_DOWN || o->kind == DR_TIP_UP) {
			/* The solenoid goes here */
			j = i + 1;
			continue;
		}
		if (o->stroke) {
			printf("stroke %d of %d\n", o->stroke, strokes);
			fflush(stdout);
		}
		/* One path while the moves are alike and turn gently */
		n = 0;
		for (j = i; j < nops; j++) {
			const struct op *p = &ops[j];

			if (p->kind != o->kind || p->speed != o->speed ||
			    (j > i && p->stroke))
				break;
			if (j > i && dr_turn(px, py, ops[j - 1].x,
					     ops[j - 1].y, p->x, p->y) >
				     motors[0].corner_deg)
				break;
			if (j > i) {
				px = ops[j - 1].x;
				py = ops[j - 1].y;
			}
			xy[n][0] = p->x;
			xy[n][1] = p->y;
			n++;
		}
		if (path_to(&f, n, (const double (*)[2])xy, o->speed))
			goto out;
		x0 = ops[j - 1].x;
		y0 = ops[j - 1].y;
		if (stopped || stop_req) {
			printf("stopped at line %d\n", ops[j - 1].line);
			goto out;
		}
	}
	printf("done\n");
	ret = 0;
out:
	free(ops);
	free(xy);
	return ret;
}

/* jog M DEG [RPM]: one motor, no check; the position stays counted */
static int cmd_jog(int argc, char **argv)
{
	double deg, usteps, upd, rpm = motors[0].rpm;
	struct until u = { .fd = -1 };
	int m;

	if (argc < 3 || argc > 4) {
		err("usage: jog M DEG [RPM]");
		return -1;
	}
	m = atoi(argv[1]) - 1;
	if (m < 0 || m >= NMOTORS) {
		err("motor must be 1 (left) or 2 (right)");
		return -1;
	}
	if (parse_num(argv[2], &deg) || (argc > 3 && parse_num(argv[3], &rpm)))
		return -1;
	upd = usteps_per_deg(m);
	if (upd <= 0)
		return -1;
	usteps = lround(deg * upd);
	if (move_axes(1, &m, &usteps, 0, rpm, &u))
		return -1;
	if (u.interrupted)
		printf("interrupted\n");
	return cmd_where();
}

int main(int argc, char **argv)
{
	struct sigaction sa = { .sa_handler = on_sigint };
	const char *conf = getenv("TAPBOT_CONF");
	const char *c;
	int i = 1;

	if (argc > 2 && !strcmp(argv[1], "-c")) {
		conf = argv[2];
		i = 3;
	}
	config_init(conf ? conf : CONF_PATH);
	if (i >= argc) {
		usage();
		return 1;
	}
	/* No SA_RESTART, like tmc2209ctl: Ctrl-C decelerates the move */
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	c = argv[i];
	argc -= i;
	argv += i;
	if (!strcmp(c, "help") || !strcmp(c, "-h") || !strcmp(c, "--help")) {
		usage();
		return 0;
	}
	if (nmotors < NMOTORS) {
		err("tapbot needs [motor1] and [motor2] in %s", CONF_PATH);
		return 1;
	}
	if (!strcmp(c, "sethome"))
		return cmd_sethome() ? 1 : 0;
	if (!strcmp(c, "where"))
		return cmd_where() ? 1 : 0;
	if (!strcmp(c, "move"))
		return cmd_move(argc, argv) ? 1 : 0;
	if (!strcmp(c, "home"))
		return cmd_home(argc, argv) ? 1 : 0;
	if (!strcmp(c, "corners"))
		return cmd_corners(argc, argv) ? 1 : 0;
	if (!strcmp(c, "fk"))
		return cmd_fk(argc, argv) ? 1 : 0;
	if (!strcmp(c, "ik"))
		return cmd_ik(argc, argv) ? 1 : 0;
	if (!strcmp(c, "gcode"))
		return cmd_gcode(argc, argv) ? 1 : 0;
	if (!strcmp(c, "jog"))
		return cmd_jog(argc, argv) ? 1 : 0;
	err("unknown command '%s' (try 'tapbot help')", c);
	return 1;
}
