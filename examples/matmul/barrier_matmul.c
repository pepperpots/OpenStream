/* 
 * Rectangular matrix multiplication.
 *
 * See the paper ``Cache-Oblivious Algorithms'', by
 * Matteo Frigo, Charles E. Leiserson, Harald Prokop, and 
 * Sridhar Ramachandran, FOCS 1999, for an explanation of
 * why this algorithm is good for caches.
 *
 * Author: Matteo Frigo
 */
static const char *ident __attribute__((__unused__))
     = "$HeadURL: https://bradley.csail.mit.edu/svn/repos/cilk/5.4.3/examples/matmul.cilk $ $LastChangedBy: sukhaj $ $Rev: 517 $ $Date: 2003-10-27 10:05:37 -0500 (Mon, 27 Oct 2003) $";

/*
 * Copyright (c) 2003 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define REAL float

#define _WITH_OUTPUT 1

#include <sys/time.h>
#include <unistd.h>
double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}

#if _CUTOFF
#  define __cutoff_ _CUTOFF
#else
#  define __cutoff_ 4
#endif

void
zero (REAL *A, int n)
{
  int i, j;

  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      A[i * n + j] = 0.0;
    }
  }
}

void
init (REAL *A, int n)
{
  int i, j;

  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      A[i * n + j] = (double) rand();
    }
  }
}

double
maxerror (REAL *A, REAL *B, int n)
{
  int i, j;
  double error = 0.0;

  for (i = 0; i < n; i++) {
    for (j = 0; j < n; j++) {
      double diff = (A[i * n + j] - B[i * n + j]) / A[i * n + j];
      if (diff < 0)
	diff = -diff;
      if (diff > error)
	error = diff;
    }
  }
  return error;
}

void
iter_matmul (REAL *A, REAL *B, REAL *C, int n)
{
  int i, j, k;

  for (i = 0; i < n; i++)
    for (k = 0; k < n; k++) {
      REAL c = 0.0;
      for (j = 0; j < n; j++)
	c += A[i * n + j] * B[j * n + k];
      C[i * n + k] = c;
    }
}

/*
 * A \in M(m, n)
 * B \in M(n, p)
 * C \in M(m, p)
 */
void
rec_matmul (REAL *A, REAL *B, REAL *C, int m, int n, int p, int ld, int add)
{
#pragma omp task
  {
    if ((m + n + p) <= __cutoff_) {
      int i, j, k;
      /* base case */
      if (add) {
	for (i = 0; i < m; i++)
	  for (k = 0; k < p; k++) {
	    REAL c = 0.0;
	    for (j = 0; j < n; j++)
	      c += A[i * ld + j] * B[j * ld + k];
	    C[i * ld + k] += c;
	  }
      } else {
	for (i = 0; i < m; i++)
	  for (k = 0; k < p; k++) {
	    REAL c = 0.0;
	    for (j = 0; j < n; j++)
	      c += A[i * ld + j] * B[j * ld + k];
	    C[i * ld + k] = c;
	  }
      }
    } else if (m >= n && n >= p) {
      int m1 = m >> 1;
      /*spawn*/ rec_matmul(A, B, C, m1, n, p, ld, add);
      /*spawn*/ rec_matmul(A + m1 * ld, B, C + m1 * ld, m - m1,
			   n, p, ld, add);
    } else if (n >= m && n >= p) {
      int n1 = n >> 1;
      /*spawn*/ rec_matmul(A, B, C, m, n1, p, ld, add);

#pragma omp taskwait

      /*spawn*/ rec_matmul(A + n1, B + n1 * ld, C, m, n - n1, p, ld, 1);
    } else {
      int p1 = p >> 1;
      /*spawn*/ rec_matmul(A, B, C, m, n, p1, ld, add);
      /*spawn*/ rec_matmul(A, B + p1, C + p1, m, n, p - p1, ld, add);
    }
#pragma omp taskwait
  }
}

int
main (int argc, char *argv[])
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));
  int n;
  REAL *A, *B, *C1, *C2;
  double err;

  if (argc != 2) {
    fprintf (stderr, "Usage: matmul <n>\n");
    exit (1);
  }
  n = atoi(argv[1]);

  A = malloc(n * n * sizeof(REAL));
  B = malloc(n * n * sizeof(REAL));
  C1 = malloc(n * n * sizeof(REAL));
  C2 = malloc(n * n * sizeof(REAL));

  init (A, n);
  init (B, n);
  zero (C1, n);
  zero (C2, n);

  if (_WITH_OUTPUT)
    iter_matmul (A, B, C1, n);


  gettimeofday (start, NULL);

  rec_matmul (A, B, C2, n, n, n, n, 0);

#pragma omp taskwait

  gettimeofday (end, NULL);
  printf ("%.5f\n", tdiff (end, start));

  if (_WITH_OUTPUT)
    {
      err = maxerror (C1, C2, n);

      printf("Max error     = %g\n", err);
      printf("Options: size = %d\n", n);
      printf("Running time  = %4f s\n", tdiff (end, start));
      printf("``MFLOPS''    = %4f\n\n",
	     2.0 * n * n * n / (1.0e6 * tdiff (end, start)));
    }

  free(C2);
  free(C1);
  free(B);
  free(A);
  return 0;
}

