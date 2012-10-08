/*	@(#)cat.c	1.1	91/04/24 17:32:17 */
/* cat - concatenates files  		Author: Andy Tanenbaum */

#include <stdio.h>
#include <stdlib.h>

#define OUT_BUF_SIZE	512
#define IN_BLOCK_SIZE	(29*1024)

int unbuffered;
char out_buf[OUT_BUF_SIZE];
char in_buf[IN_BLOCK_SIZE];
char *next = out_buf;

main(argc, argv)
int argc;
char *argv[];
{
  int i, k, fd1, sts;
  char *p;

  k = 1;
  /* Check for the -u flag -- unbuffered operation. */
  p = argv[1];
  if (argc >=2 && *p == '-' && *(p+1) == 'u') {
	unbuffered = 1;
	k = 2;
  }

  if (k >= argc) {
	copyfile(0, 1);
	flush();
	exit(0);
  }

  sts = 0;
  for (i = k; i < argc; i++) {
	if (argv[i][0] == '-' && argv[i][1] == 0) {
		fd1 = 0;
	} else {
		fd1 = open(argv[i], 0);
		if (fd1 < 0) {
			std_err("cat: cannot open ");
			std_err(argv[i]);
			std_err("\n");
			sts = 1;
			continue;
		}
	}
	copyfile(fd1, 1);
	if (fd1 != 0)
		(void) close(fd1);
  }
  flush();
  exit(sts);
}



copyfile(fd1, fd2)
int fd1, fd2;
{
  int n, j, m;

  while (1) {
	n = read(fd1, in_buf, IN_BLOCK_SIZE);
	if (n < 0) quit();
	if (n == 0) return;
	if (unbuffered) {
		m = write(fd2, in_buf, n);
		if (m != n) quit();
	} else {
		for (j = 0; j < n; j++) {
			*next++ = in_buf[j];
			if (next == &out_buf[OUT_BUF_SIZE]) {
				m = write(fd2, out_buf, OUT_BUF_SIZE);
				if (m != OUT_BUF_SIZE) quit();
				next = out_buf;
			}
		}
	}
  }
}


flush()
{
  if (next != out_buf && write(1, out_buf, (int) (next - out_buf)) <= 0)
	quit();
}


quit()
{
  perror("cat");
  exit(1);
}
