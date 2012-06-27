#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cblas.h>
#include <complex.h>

#include <fftw3.h>

#include <getopt.h>

#include <sys/time.h>
#include <unistd.h>
double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}

int main (int argc, char *argv[])
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));
  double total_time = 0;
  fftw_complex *in, *out;
  fftw_plan p;
  int i, j;

  int option;

  int radix = 0;
  int N1 = 0;
  int N2 = 0;
  int N = 0;

  int numiters = 1;
  int n_threads = 8;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  int volatile res = 0;

  while ((option = getopt(argc, argv, "r:s:x:i:o:t:")) != -1)
    {
      switch(option)
	{
	case 'r':
	  numiters = atoi(optarg);
	  break;
	case 's':
	  {
	    N = 1 << atoi(optarg);
	  }
	  break;
	case 'x':
	  radix = atoi (optarg);
	  break;
	case 'i':
	  in_file = fopen(optarg, "r");
	  break;
	case 'o':
	  res_file = fopen(optarg, "w");
	  break;
	case 't':
	  n_threads = atoi (optarg);
	  break;
	}
    }

  if (N == 0)
    N = 1 << 10;
  if (radix == 0)
    radix = 3;
  if (N2 == 0)
    N2 = N >> radix;
  N1 = 1 << radix;

  if (res_file == NULL)
    res_file = fopen("seq_fftw.out", "w");

  in = (fftw_complex*) fftw_malloc (sizeof(fftw_complex) * N);
  out = (fftw_complex*) fftw_malloc (sizeof(fftw_complex) * N);

  p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  // Generate random numbers or read from file
  if (in_file == NULL)
    {
      long seed[4] = {0, 0, 0, 1};
      long sp = 2;
      clarnv_(&sp, seed, &N, in);

      // Also allow saving data sessions
      //if (res_file != NULL)
      //fwrite (in, sizeof (double), size, res_file);
    }
  else
    {
      fread (in, sizeof(fftw_complex), N, in_file);
    }


  for (i = 0; i < numiters; ++i)
    {
      gettimeofday (start, NULL);
      fftw_execute(p);
      gettimeofday (end, NULL);

      total_time += tdiff (end, start);
    }

  printf ("%.5f \n", total_time);

  for(j = 0; j < N; ++j)
    {
      int volatile res = 0;
      res += cimag (out[j]);
      //fprintf(res_file,"%f \t %f\n", creal(out[j]), cimag(out[j]));
    }

  fftw_destroy_plan(p);

  fftw_free(in); fftw_free(out);

  if (res_file != NULL) fclose (res_file);
}
