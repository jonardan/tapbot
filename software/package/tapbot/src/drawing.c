// SPDX-License-Identifier: MIT
/* Drawings: G-code lines (see drawing.h) */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawing.h"

#define DEG	(M_PI / 180.0)

int dr_parse(const char *line, struct dr_cmd *c, char *err, size_t errlen)
{
	const char *p = line;
	int g = -1, m = -1;

	memset(c, 0, sizeof(*c));
	while (*p) {
		char letter;
		char *end;
		double v;

		if (isspace((unsigned char)*p)) {
			p++;
			continue;
		}
		if (*p == ';' || *p == '%')
			break;
		if (*p == '(') {
			p = strchr(p, ')');
			if (!p) {
				snprintf(err, errlen, "'(' without ')'");
				return -1;
			}
			p++;
			continue;
		}
		letter = toupper((unsigned char)*p++);
		v = strtod(p, &end);
		if (end == p || !isfinite(v)) {
			snprintf(err, errlen, "'%c' without a number", letter);
			return -1;
		}
		p = end;
		switch (letter) {
		case 'G':
		case 'M':
			if (v != floor(v) || (letter == 'G' ? g : m) >= 0) {
				snprintf(err, errlen, "one %c per line, whole "
					 "numbers", letter);
				return -1;
			}
			*(letter == 'G' ? &g : &m) = (int)v;
			break;
		case 'X':
			c->has_x = 1;
			c->x = v;
			break;
		case 'Y':
			c->has_y = 1;
			c->y = v;
			break;
		case 'F':
			if (v <= 0) {
				snprintf(err, errlen, "F must be > 0");
				return -1;
			}
			c->has_f = 1;
			c->f = v;
			break;
		case 'N':
		case 'Z':
		case 'S':
			break;
		default:
			snprintf(err, errlen, "'%c' not supported", letter);
			return -1;
		}
	}
	if (g >= 0 && m >= 0) {
		snprintf(err, errlen, "G and M on one line");
		return -1;
	}
	switch (g) {
	case -1:
		break;
	case 0:
	case 1:
		if (!c->has_x && !c->has_y) {
			snprintf(err, errlen, "G%d without X or Y", g);
			return -1;
		}
		c->kind = g ? DR_DRAW : DR_TRAVEL;
		return 0;
	case 21:
	case 90:
		break;
	default:
		snprintf(err, errlen, "G%d not supported", g);
		return -1;
	}
	switch (m) {
	case -1:
		break;
	case 3:
		c->kind = DR_TIP_DOWN;
		return 0;
	case 5:
		c->kind = DR_TIP_UP;
		return 0;
	case 2:
	case 30:
		c->kind = DR_END;
		return 0;
	default:
		snprintf(err, errlen, "M%d not supported", m);
		return -1;
	}
	if (c->has_x || c->has_y) {
		snprintf(err, errlen, "X/Y without G0 or G1");
		return -1;
	}
	return 0;
}

double dr_turn(double ax, double ay, double bx, double by, double cx,
	       double cy)
{
	double ux = bx - ax, uy = by - ay, vx = cx - bx, vy = cy - by;

	if (hypot(ux, uy) < 1e-9 || hypot(vx, vy) < 1e-9)
		return 0;
	return fabs(atan2(ux * vy - uy * vx, ux * vx + uy * vy)) / DEG;
}
