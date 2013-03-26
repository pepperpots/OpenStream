#define _POSIX_C_SOURCE 200112L
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <cblas.h>
#include <getopt.h>
#include "../common/sync.h"
#include "../common/common.h"

#define _WITH_OUTPUT 0

#include <unistd.h>

/* Missing declarations from liblapack */
int dlarnv_(long *idist, long *iseed, int *n, double *x);
void dpotrf_( unsigned char *uplo, int * n, double *a, int *lda, int *info );

int
main(int argc, char *argv[])
{
  int option;
  int i, iter;
  int size;

  int N = 4096;

  int numiters = 10;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  double * data;
  int nfo;
  unsigned char lower = 'L';
  struct profiler_sync sync;

  PROFILER_NOTIFY_PREPARE(&sync);

  while ((option = getopt(argc, argv, "n:s:r:i:o:h")) != -1)
    {
      switch(option)
	{
	case 'n':
	  N = atoi(optarg);
	  break;
	case 's':
	  N = 1 << atoi(optarg);
	  break;
	case 'r':
	  numiters = atoi (optarg);
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
		 "  -n <size>                    Number of colums of the square matrix, default is %d\n"
		 "  -s <power>                   Set the number of colums of the square matrix to 1 << <power>\n"
		 "  -r <number of iterations>    Number of repetitions of the execution\n"
		 "  -i <input file>              Read matrix data from an input file\n"
		 "  -o <output file>             Write matrix data to an output file\n",
		 argv[0], N);
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

  size = N * N;

  struct timeval *sstart = (struct timeval *) malloc (numiters * sizeof (struct timeval));
  struct timeval *send = (struct timeval *) malloc (numiters * sizeof (struct timeval));

   if (posix_memalign ((void **) &data, 64, size * sizeof (double)))
    {
      printf ("Out of memory.\n");
      exit (1);
    }

  // Generate random numbers or read from file
  if (in_file == NULL)
    {
      long seed[4] = {0, 0, 0, 1};
      long sp = 1;
      dlarnv_(&sp, seed, &size, data);

      // Also allow saving data sessions
      if (res_file != NULL)
	fwrite (data, sizeof (double), size, res_file);
    }
  else
    {
      fread (data, sizeof(double), size, in_file);
    }

  // Ensure matrix is definite positive
  for(i = 0; i < N; ++i)
    data[i*N + i] += N;


  for (iter = 0; iter < numiters; ++iter)
    {
      double * seq_data;
      if (posix_memalign ((void **) &seq_data, 64, size * sizeof (double)))
	{
	  printf ("Out of memory.\n");
	  exit (1);
	}
      memcpy (seq_data, data, size * sizeof (double));

      gettimeofday (&sstart[iter], NULL);
      PROFILER_NOTIFY_RECORD(&sync);
      dpotrf_(&lower, &N, seq_data, &N, &nfo);
      PROFILER_NOTIFY_PAUSE(&sync);
      gettimeofday (&send[iter], NULL);

      free (seq_data);
    }

  double seq_time = 0;
  for (iter = 0; iter < numiters; ++iter)
    {
      seq_time += tdiff (&send[iter], &sstart[iter]);
    }

  printf ("%.5f \n", seq_time);

  PROFILER_NOTIFY_FINISH(&sync);

  return 0;
}





