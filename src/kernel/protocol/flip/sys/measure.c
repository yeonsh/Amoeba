/*	@(#)measure.c	1.1	96/02/27 14:05:15 */
#include "amoeba.h"
#include "string.h"
#include "sys/proto.h"    
#include "sys/flip/measure.h"

measure_t measure_kernel;

void measure_init()
{
    int i;
    long start;

#ifdef USE_SUN3_HWTIMER
    extern long sun3_hwtimer();
    measure_kernel.gettime = sun3_hwtimer;
#else
    extern long getmicro();
    measure_kernel.gettime = getmicro;
#endif
    measure_kernel.factor = 3; /* begin/end timer call + update overhead */
    measure_kernel.nruns = 1000;

    start = (*measure_kernel.gettime)();
    for (i = 0; i < measure_kernel.nruns; i++)
        (void) (*measure_kernel.gettime)();

    measure_kernel.cost_gettime =
        (*measure_kernel.gettime)() - start;
    printf("init_measure: measure_kernel.cost_gettime = %ld\n",
           measure_kernel.cost_gettime);

    start = (*measure_kernel.gettime)();
    for (i = 0; i < measure_kernel.nruns - 1; i++) {
        BEGIN_MEASURE(measure);
        END_MEASURE(measure);
    }
    printf("init_measure: overhead of %ld macro calls %ld\n",
           measure_kernel.nruns, (*measure_kernel.gettime)() - start);
}

void
measure_end(name, inf)
char *name;
struct measure_info *inf;
{
    register long thistime, endtime;

    if (inf->me_begin == 0) {
	/* MEASURE_END called without corresponding MEASURE_BEGIN?! */
	return;
    }

    endtime = (*measure_kernel.gettime)();
    thistime = endtime - inf->me_begin;
    if (thistime < 0) {
	/* Clock running backward?  This cannot be right */
	printf("%s: begin = %lx; end = %lx?\n",
	       name, inf->me_begin, endtime);
    } else {
	if (inf->me_min == 0 || thistime < inf->me_min) {
	    inf->me_min = thistime;
	}
	if (thistime > inf->me_max) {
	    inf->me_max = thistime;
	}
	inf->me_total += thistime;
	if (++inf->me_cnt >= measure_kernel.nruns) {
	    printf("%s: count %ld total %ld min %ld max %ld\n",
		   name, inf->me_cnt, inf->me_total -
		   measure_kernel.factor * measure_kernel.cost_gettime,
		   inf->me_min, inf->me_max);
	    inf->me_total = 0;
	    inf->me_cnt = 0;
	    inf->me_min = 0;
	    inf->me_max = 0;
	}
    }
    inf->me_begin = 0;
}

int
measure_reinit(begin, end)
char *begin;
char *end;
{
    char *p;

    /* Zero the current measurement statistics, and print a message
     * so that an external performance analysis program can select
     * the appropriate measurements after performing a new test.
     */
    (void) memset((_VOIDSTAR) &measure_inf(measure).me_begin, '\0',
	   sizeof(measure_kernel) -
	   ((char *)&measure_inf(measure).me_begin - (char *)&measure_kernel));
    printf("-- starting new measurements --\n");

    p = bprintf(begin, end, "starting new measurements\n");
    return (p - begin);
}
