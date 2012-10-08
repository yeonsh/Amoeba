/*	@(#)hypot.c	1.1	91/04/09 09:23:07 */
/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

/*
 * The function hypot() calculates the length of a vector. After three
 * iterations there are 20 significant digits so the algorithm stops.
 * (After 2 iterations 6.5 digits, after 4 iterations 64 digits.)
 */

double
hypot(p, q)
register double p,q;
{
	register int i = 0;
	register double r;

	if (p < 0) p = -p;
	if (q < 0) q = -q;
	if (p < q) { r = p; p = q; q = r; }
	if (p == 0) return 0.0;
	while (1) {
		r = q/p;
		r *= r;
		r /= r + 4;
		p += 2 * p * r;
		if (++i == 3) return p;
		q *= r;
	}
}
