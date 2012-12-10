/*
 * Heat diffusion (Jacobi-type iteration)
 *
 * Usage: see function usage();
 * 
 * Volker Strumpen, Boston                                 August 1996
 *
 * Copyright (c) 1996 Massachusetts Institute of Technology
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>

extern int errno;

#define _WITH_OUTPUT 1

#include <sys/time.h>
#include <unistd.h>
double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}


#define f(x,y)     (sin(x)*sin(y))
#define randa(x,t) (0.0)
#define randb(x,t) (exp(-2*(t))*sin(x))
#define randc(y,t) (0.0)
#define randd(y,t) (exp(-2*(t))*sin(y))
#define solu(x,y,t) (exp(-2*(t))*sin(x)*sin(y))

int nx, ny, nt;
double xu, xo, yu, yo, tu, to;
double dx, dy, dt;

double dtdxsq, dtdysq;
double t;

int leafmaxcol;


/*****************   Allocation of grid partition  ********************/

void 
allcgrid(double **new, double **old, int lb, int ub)
{
  int j;
  double **rne, **rol;

  for (j=lb, rol=old+lb, rne=new+lb; j < ub; j++, rol++, rne++) {
    *rol = (double *) malloc(ny * sizeof(double));
    *rne = (double *) malloc(ny * sizeof(double));
  }
}

/*****************   Initialization of grid partition  ********************/

void 
initgrid(double **old, int lb, int ub)
{
  int a, b, llb, lub;

  llb = (lb == 0) ? 1 : lb;
  lub = (ub == nx) ? nx - 1 : ub;
  
  for (a=llb, b=0; a < lub; a++)		/* boundary nodes */
    old[a][b] = randa(xu + a * dx, 0);
  
  for (a=llb, b=ny-1; a < lub; a++)
    old[a][b] = randb(xu + a * dx, 0);
  
  if (lb == 0) {
    for (a=0, b=0; b < ny; b++)
      old[a][b] = randc(yu + b * dy, 0);
  }
  if (ub == nx) {
    for (a=nx-1, b=0; b < ny; b++)
      old[a][b] = randd(yu + b * dy, 0);
  }
  for (a=llb; a < lub; a++) {	/* inner nodes */
    for (b=1; b < ny-1; b++) {
      old[a][b] = f(xu + a * dx, yu + b * dy);
    }
  }
}


/***************** Five-Point-Stencil Computation ********************/

void
compstrip_ (register double **new, register double **old, int lb, int ub)
{
  int ylb, yub, i, num_strips;

  num_strips = ny >> 4;

  for (i = 0; i < num_strips; ++i)
    {
#pragma omp task
      {
	register int a, b, llb, lub;

	ylb = i << 4;
	yub = ylb + 1 << 4;

	llb = (lb == 0) ? 1 : lb;
	lub = (ub == nx) ? nx - 1 : ub;

	for (a=llb; a < lub; a++) {
	  for (b=ylb + 1; b < yub-1; b++) {
	    new[a][b] =   dtdxsq * (old[a+1][b] - 2 * old[a][b] + old[a-1][b])
	      + dtdysq * (old[a][b+1] - 2 * old[a][b] + old[a][b-1])
	      + old[a][b];
	  }
	}

	for (a=llb, b=yub-1; a < lub; a++)
	  new[a][b] = randb(xu + a * dx, t);

	for (a=llb, b=ylb; a < lub; a++)
	  new[a][b] = randa(xu + a * dx, t);

	if (lb == 0) {
	  for (a=0, b=ylb; b < yub; b++)
	    new[a][b] = randc(yu + b * dy, t);
	}
	if (ub == nx) {
	  for (a=nx-1, b=ylb; b < yub; b++)
	    new[a][b] = randd(yu + b * dy, t);
	}
      }
    }
}

void
compstripe (register double **new, register double **old, int lb, int ub)
{
  register int a, b, llb, lub;

  llb = (lb == 0) ? 1 : lb;
  lub = (ub == nx) ? nx - 1 : ub;

  for (a=llb; a < lub; a++) {
    for (b=1; b < ny-1; b++) {
      new[a][b] =   dtdxsq * (old[a+1][b] - 2 * old[a][b] + old[a-1][b])
	+ dtdysq * (old[a][b+1] - 2 * old[a][b] + old[a][b-1])
	+ old[a][b];
    }
  }

  for (a=llb, b=ny-1; a < lub; a++)
    new[a][b] = randb(xu + a * dx, t);

  for (a=llb, b=0; a < lub; a++)
    new[a][b] = randa(xu + a * dx, t);

  if (lb == 0) {
    for (a=0, b=0; b < ny; b++)
      new[a][b] = randc(yu + b * dy, t);
  }
  if (ub == nx) {
    for (a=nx-1, b=0; b < ny; b++)
      new[a][b] = randd(yu + b * dy, t);
  }
}


/***************** Decomposition of 2D grids in stripes ********************/

#define ALLC       0
#define INIT       1
#define COMP       2

void
divide (int lb, int ub, double **new, double **old, int mode, int timestep)
{
#pragma omp task
  {
    if (ub - lb > leafmaxcol)
      {
	divide (lb, (ub + lb) / 2, new, old, mode, timestep);
	divide ((ub + lb) / 2, ub, new, old, mode, timestep);
#pragma omp taskwait
      }
    else
      {
	switch (mode) {
	case COMP:
	  if (timestep % 2)
	    compstripe (new, old, lb, ub);
	  else
	    compstripe (old, new, lb, ub);
	  break;

	case ALLC:
	  allcgrid (new, old, lb, ub);
	  break;

	case INIT:
	  initgrid (old, lb, ub);
	  break;
	}
      }
  }
}


int
heat (void)
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));
  double **old, **new;
  int  c;
#if _WITH_OUTPUT
  double tmp, **mat;
  double mae = 0.0;
  double mre = 0.0;
  double me = 0.0;
  int a, b;
#endif

  /* Memory Allocation */
  old = (double **) malloc(nx * sizeof(double *));
  new = (double **) malloc(nx * sizeof(double *));
  divide (0, nx, new, old, ALLC, 0);
#pragma omp taskwait
  /*
   * Sequential allocation might be faster than parallel!
   * Substitute the following call for the preceding divide:
   *
   * allcgrid(new, old, 0, nx);
   */

  /* Initialization */
  divide (0, nx, new, old, INIT, 0);

  /* Jacobi Iteration (divide x-dimension of 2D grid into stripes) */
#pragma omp taskwait

  gettimeofday (start, NULL);

  for (c = 1; c <= nt; c++) {
    t = tu + c * dt;
    divide (0, nx, new, old, COMP, c);
#pragma omp taskwait
  }

  gettimeofday (end, NULL);
  printf ("%.5f\n", tdiff (end, start));



#ifdef _WITH_OUTPUT
  /* Error summary computation: Not parallelized! */
  mat = (c % 2) ? old : new;
  printf("\n Error summary of last time frame comparing with exact solution:");
  for (a=0; a<nx; a++)
    for (b=0; b<ny; b++) {
      tmp = fabs(mat[a][b] - solu(xu + a * dx, yu + b * dy, to));
      if (tmp > mae)
	mae = tmp;
    }
  printf("\n   Local maximal absolute error  %10e ", mae);

  for (a=0; a<nx; a++)
    for (b=0; b<ny; b++) {
      tmp = fabs(mat[a][b] - solu(xu + a * dx, yu + b * dy, to));
      if (mat[a][b] != 0.0)
	tmp = tmp / mat[a][b];
      if (tmp > mre)
	mre = tmp;
    }
  printf("\n   Local maximal relative error  %10e %s ", mre * 100, "%");

  me = 0.0;
  for (a=0; a<nx; a++)
    for (b=0; b<ny; b++) {
      me += fabs(mat[a][b] - solu(xu + a * dx, yu + b * dy, to));
    }
  me = me / (nx * ny);
  printf("\n   Global Mean absolute error    %10e\n\n", me);
#endif
  return 0;
}

void
read_heatparams (char *filefn)
{
  FILE *f;
  int l;

  if ((f = fopen(filefn, "r")) == NULL) {
    printf("\n Can't open %s\n", filefn);
    exit(0);
  }
  l = fscanf(f, "%d %d %d %lf %lf %lf %lf %lf %lf",
	     &nx, &ny, &nt, &xu, &xo, &yu, &yo, &tu, &to);
  if (l != 9)
    printf("\n Warning: fscanf errno %d", errno);
  fclose(f);
}

int
main (int argc, char *argv[])
{
  int ret, benchmark;

  nx = 512;
  ny = 512;
  nt = 100;
  xu = 0.0;
  xo = 1.570796326794896558;
  yu = 0.0;
  yo = 1.570796326794896558;
  tu = 0.0;
  to = 0.0000001;
  leafmaxcol = 10;

  if (argc != 2 || strcmp(argv[1], "-h") == 0) {
    fprintf (stderr, "Usage: %s <benchID>\n\n"
	     "Options:\n"
	     "  benchID                      Select a benchmark; valid values are 1, 2 and 3\n", argv[0]);
    exit (1);
  }
  benchmark = atoi(argv[1]);

  if (benchmark) {
    switch (benchmark) {
    case 1:      /* short benchmark options -- a little work*/
      nx = 512;
      ny = 512;
      nt = 1;
      xu = 0.0;
      xo = 1.570796326794896558;
      yu = 0.0;
      yo = 1.570796326794896558;
      tu = 0.0;
      to = 0.0000001;
      leafmaxcol = 10;
      break;
    case 2:      /* standard benchmark options*/
      nx = 4096;
      ny = 512;
      nt = 40;
      xu = 0.0;
      xo = 1.570796326794896558;
      yu = 0.0;
      yo = 1.570796326794896558;
      tu = 0.0;
      to = 0.0000001;
      leafmaxcol = 10;
      break;
    case 3:      /* long benchmark options -- a lot of work*/
      nx = 4096;
      ny = 16;
      nt = 1600;
      xu = 0.0;
      xo = 1.570796326794896558;
      yu = 0.0;
      yo = 1.570796326794896558;
      tu = 0.0;
      to = 0.0000001;
      leafmaxcol = 1;
      break;
    }
  }

  dx = (xo - xu) / (nx - 1);
  dy = (yo - yu) / (ny - 1);
  dt = (to - tu) / nt;	/* nt effective time steps! */

  dtdxsq = dt / (dx * dx);
  dtdysq = dt / (dy * dy);

  ret = heat ();

  if (_WITH_OUTPUT)
    {
      printf("Ret     = %d\n", ret);
    }

/*   printf("\nCilk Example: heat\n"); */
/*   printf("	      running on %d processor%s\n\n", Cilk_active_size, Cilk_active_size > 1 ? "s" : ""); */
/*   printf("\n   dx = %f", dx); */
/*   printf("\n   dy = %f", dy); */
/*   printf("\n   dt = %f", dt); */

/*   printf("\n\n Stability Value for explicit method must be > 0:  %f\n\n", */
/* 	 0.5 - (dt / (dx * dx) + dt / (dy * dy))); */
/*   printf("Options: granularity = %d\n", leafmaxcol); */
/*   printf("         nx          = %d\n", nx); */
/*   printf("         ny          = %d\n", ny); */
/*   printf("         nt          = %d\n", nt); */

/*   printf("Running time  = %4f s\n", Cilk_wall_time_to_sec(tm_elapsed)); */
/*   printf("Work          = %4f s\n", Cilk_time_to_sec(wk_elapsed)); */
/*   printf("Critical path = %4f s\n\n", Cilk_time_to_sec(cp_elapsed)); */

  return 0;
}
