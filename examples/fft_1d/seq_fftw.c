#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cblas.h>
#include <complex.h>
#include <fftw3.h>
#include <getopt.h>
#include <unistd.h>
#include "../common/common.h"

/* Missing declarations from liblapack */
int clarnv_(long *idist, long *iseed, int *n, complex *x);

int main (int argc, char *argv[])
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));
  double total_time = 0;
  fftw_complex *in, *out;
  fftw_plan p;
  int i, j;

  int option;
  int N = 0;

  int numiters = 1;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  while ((option = getopt(argc, argv, "r:s:i:o:h")) != -1)
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
	case 'i':
	  in_file = fopen(optarg, "r");
	  break;
	case 'o':
	  res_file = fopen(optarg, "w");
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -r <iterations>              Number of iterations\n"
		 "  -s <power>                   Set the number FFT samples to 1 << <power>\n"
		 "  -i <input file>              Read FFT data from an input file\n"
		 "  -o <output file>             Write FFT data to an output file, default is seq_fftw.out\n",
		 argv[0]);
	  exit(0);
	  break;
	case '?':
	  fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
	  exit(1);
	  break;
	}
    }

  if(optind != argc) {
	  fprintf(stderr, "Too many arguments. Run %s -h for usage.\n", argv[0]);
	  exit(1);
  }

  if (N == 0)
    N = 1 << 10;

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

  return 0;
}
