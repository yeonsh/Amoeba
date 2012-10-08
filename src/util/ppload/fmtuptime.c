/*	@(#)fmtuptime.c	1.3	94/04/07 15:55:41 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

char *
plural(n)
	long n;
{
	return (n == 1) ? "" : "s";
}

void
fmtuptime(p, uptime)
	char *p;
	long uptime; /* Up time in seconds */
{
	if (uptime < 5*60) {
		sprintf(p, "%ld sec", uptime);
		return;
	}
	if (uptime < 60*60) {
		sprintf(p, "%ld min %ld sec", uptime/60, uptime%60);
		return;
	}
	
	uptime /= 60; /* Now it's in minutes */
	if (uptime < 24*60) {
		sprintf(p, "%ld hr%s %ld min",
			uptime/60, plural(uptime/60), uptime%60);
		return;
	}
	
	uptime /= 60; /* Now it's in hours */
	if (uptime < 7*24) {
		sprintf(p, "%ld day%s %ld hr%s",
			uptime/24, plural(uptime/24),
			uptime%24, plural(uptime%24));
		return;
	}
	
	uptime /= 24; /* Now it's in weeks */
	sprintf(p, "%ld week%s %ld day%s",
		uptime/7, plural(uptime/7),
		uptime%7, plural(uptime%7));
}
