#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include "../common/common.h"
#include <getopt.h>

#define _WITH_OUTPUT 0

#include <unistd.h>

/* Simple ad hoc dependence resolver for Seidel.  */
static inline void
resolve_dependences (int block_i, int block_j, int num_blocks,
		     void **streams_peek, int *num_peek,
		     void **streams_out, int *num_out,
		     void **streams)
{
  int pos_peek = 0;
  int pos_out = 0;

  // position of the block
  int block_pos = block_i + block_j*num_blocks;

  /*
           _______
          |   2   |
	  |_______|
     ___   _______   ___
    |   | |       | |   |
    | 1 | |   0   | | 3 |
    |___| |_______| |___|
           _______
          |   4   |
	  |_______|



    — streams_peek: the set of streams attached: (1) to any region
      that overlaps with the write regions of task T (output and
      inout); or (2) to any write region that overlaps with the read
      regions of task T (input and inout); or (3) to any of the own
      access regions of task T.

    — streams_out: the set of streams attached to the regions of task T,
      irrespectively of their type.
   */

  // STREAMS_PEEK
  // (1) regions overlapping with the write region [[ 0 ]]
  if (block_i != 0)
    streams_peek[pos_peek++] = streams[(block_pos - 1) * 5 + 3];          // West block's eastern read region
  if (block_j < num_blocks - 1)
    streams_peek[pos_peek++] = streams[(block_pos + num_blocks) * 5 + 4]; // North block's southern read region
  if (block_i < num_blocks - 1)
    streams_peek[pos_peek++] = streams[(block_pos + 1) * 5 + 1];          // East block's western read region
  if (block_j != 0)
    streams_peek[pos_peek++] = streams[(block_pos - num_blocks) * 5 + 2]; // South block's northern read region
  // (2) regions [[ 0 ]] of other blocks overlapping with read regions [[ 1-4 ]]
  if (block_i != 0)
    streams_peek[pos_peek++] = streams[(block_pos - 1) * 5];          // West block
  if (block_j < num_blocks - 1)
    streams_peek[pos_peek++] = streams[(block_pos + num_blocks) * 5]; // North block
  if (block_i < num_blocks - 1)
    streams_peek[pos_peek++] = streams[(block_pos + 1) * 5];          // East block
  if (block_j != 0)
    streams_peek[pos_peek++] = streams[(block_pos - num_blocks) * 5]; // South block
  // (3) all own regions
  streams_peek[pos_peek++] = streams[block_pos * 5 + 0];
  streams_peek[pos_peek++] = streams[block_pos * 5 + 1];
  streams_peek[pos_peek++] = streams[block_pos * 5 + 2];
  streams_peek[pos_peek++] = streams[block_pos * 5 + 3];
  streams_peek[pos_peek++] = streams[block_pos * 5 + 4];

  // STREAMS_OUT -- all own regions
  streams_out[pos_out++] = streams[block_pos * 5 + 0];
  streams_out[pos_out++] = streams[block_pos * 5 + 1];
  streams_out[pos_out++] = streams[block_pos * 5 + 2];
  streams_out[pos_out++] = streams[block_pos * 5 + 3];
  streams_out[pos_out++] = streams[block_pos * 5 + 4];

  *num_peek = pos_peek;
  *num_out = pos_out;
}


void
gauss_seidel (int N, double a[N][N], int block_size)
{
  int i, j;

  for (i = 1; i <= block_size; i++)
    for (j = 1; j <= block_size; j++)
      a[i][j] = 0.2 * (a[i][j] + a[i-1][j] + a[i+1][j] + a[i][j-1] + a[i][j+1]);
}


int
main (int argc, char **argv)
{
  int option;
  int i, j, iter;
  int N = 64;

  int numiters = 10;
  int block_size = 4;

  FILE *res_file = NULL;

  int volatile res = 0;

  while ((option = getopt(argc, argv, "n:s:b:r:o:h")) != -1)
    {
      switch(option)
	{
	case 'n':
	  N = atoi(optarg);
	  break;
	case 's':
	  N = 1 << atoi(optarg);
	  break;
	case 'b':
	  block_size = 1 << atoi (optarg);
	  break;
	case 'r':
	  numiters = atoi (optarg);
	  break;
	case 'o':
	  res_file = fopen(optarg, "w");
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -n <size>                    Number of colums of the square matrix, default is %d\n"
		 "  -s <power>                   Set the number of colums of the square matrix to 1 << <power>\n"
		 "  -b <block size power>        Set the block size 1 << <block size power>, default is %d\n"
		 "  -r <iterations>              Number of iterations\n"
		 "  -o <output file>             Write data to output file, default is starss_to_stream_seidel.out\n",
		 argv[0], N, block_size);
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

  if (res_file == NULL)
    res_file = fopen("starss_to_stream_seidel.out", "w");

  N += 2;

  {
    int num_blocks = (N - 2) / block_size;
    int num_regions = 5;
    int nstreams = num_blocks * num_blocks * num_regions;
    int streams[nstreams] __attribute__ ((stream));
    double *data;

    int sel_streams[9] __attribute__ ((stream_ref));
    int streams_peek[15] __attribute__ ((stream_ref));
    int streams_out[5] __attribute__ ((stream_ref));
    int peek_view[15][1];
    int out_view[5][1];
    int out_view2[5][1];

    int sel_in_view[9][1];
    int sel_out_view[9][1];

    int block_i, block_j;

    struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
    struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

    posix_memalign ((void**) &(data), 128, sizeof(double[N][N]));

    for (i = 0; i < N; ++i)
      for (j = 0; j < N; ++j)
	data[N*i + j] = ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0; //(i*7 +j*13) % 17;

    for (i = 0; i < nstreams; ++i)
      {
	int x;
#pragma omp task output (streams[i] << x)
	x = 0;
      }

    gettimeofday (start, NULL);

    /* Main kernel start.  ------------------------------------------------------------ */
    for (iter = 0; iter < numiters; iter++)
      for (i = 0, block_i = 0; i < N - 2; i += block_size, block_i++)
	for (j = 0, block_j = 0; j < N - 2; j += block_size, block_j++)
	  {
	    int k;
	    int num_peek, num_out, num_sel_streams;

	    /* Runtime dependence resolution function.  Here, we
	       simulate the behaviour desired for such a function
	       based on the static knowledge of dependences in Seidel.
	       The input parameters are the position of the block of
	       data that will be processed by the next task and two
	       stream selection arrays for storing the input and
	       output streams/dependences of this task.  We rely here
	       on the static knowledge of the maximum number of
	       dependences possible to infer the size of these
	       arrays.  */

	    resolve_dependences (block_i, block_j, num_blocks,
				 streams_peek, &num_peek,
				 streams_out, &num_out,
				 streams);

	    /* Work tasks: all the computation occurs here.  */
#pragma omp task firstprivate (N, block_size, data)			\
  peek (streams_peek >> peek_view[num_peek][1])				\
  output (streams_out << out_view[num_out][1])
	    gauss_seidel (N, data + N * i + j, block_size);


	    for (k = 0; k < num_out; ++k)
	      {
#pragma omp tick (streams_out[k] >> 1)
	      }

    /* Main kernel end.  -------------------------------------------------------------- */


#ifdef _PRINT_TASKGRAPH
	    printf ("Block (%d, %d, %d) has %d in deps and %d out deps\n", block_i, block_j, iter, num_in_streams, num_out_streams); fflush (stdout);
	    printf ("    IN: ");
	    for (k = 0; k < num_in_streams; ++k)
	      printf ("%zu  ", (((size_t) streams_in[k]) - ((size_t) streams[0]))/(sizeof (void*) * 8));
	    printf ("\n    OUT: ");
	    for (k = 0; k < num_out_streams; ++k)
	      printf ("%zu  ", (((size_t) streams_out[k]) - ((size_t) streams[0]))/(sizeof (void*) * 8));
	    printf ("\n");
#endif
	  }



    /* Output the results to a file when requested.  */
    int output;
#pragma omp task input (streams[(num_blocks * num_blocks - 1) * 5] >> output) firstprivate (res_file, data, N) firstprivate (start, end)
    {
      gettimeofday (end, NULL);

      printf ("%.5f\n", tdiff (end, start));

      if (_WITH_OUTPUT)
	{
	  printf ("[StarSs translated to Stream] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
		  N - 2, block_size, numiters, tdiff (end, start));

	  for (i = 0; i < N; ++i)
	    {
	      for (j = 0; j < N; ++j)
		fprintf (res_file, "%f \t", data[N * i + j]);
	      fprintf (res_file, "\n");
	    }
	}
    }
  }

  return 0;
}
