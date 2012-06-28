#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <getopt.h>

#define _WITH_OUTPUT 0
//#define _PRINT_TASKGRAPH

#include <sys/time.h>
#include <unistd.h>
double
tdiff (struct timeval *end, struct timeval *start)
{
  return (double)end->tv_sec - (double)start->tv_sec +
    (double)(end->tv_usec - start->tv_usec) / 1e6;
}

/* Simple ad hoc dependence resolver for Seidel.  */
static inline void
resolve_dependences (int block_i, int block_j, int iter,
		     int num_blocks, int numiters,
		     void **stream_deps, int *num_deps,
		     int *max_depth, void **streams)
{
  // position of the block
  int block_depth = block_i + block_j + ((iter == 0) ? 0 : (iter + 1));

  *num_deps = 0;
  *max_depth = block_depth - 1;

  if (block_depth != 0)  // Nothing left to do if depth == 0
    //goto do_out; // was for test depth==0

  if (iter == 0) // Only dependences within the same plan
    stream_deps[(*num_deps)++] = streams[block_depth - 1];
  else // iter > 0 => block_depth > 1
    {
      stream_deps[(*num_deps)++] = streams[block_depth - 1];
      stream_deps[(*num_deps)++] = streams[block_depth - 2];
    }

  // The "real" output stream, it's not taken into account in the input clause.
  //do_out:
  stream_deps[*num_deps] = streams[block_depth];

  /* We need to have an additional task to consume from the last
     stream (which is good for an output task anyway).  No need to
     avoid the last output.  */
}


void
gauss_seidel (int N, double a[N][N], int block_size)
{
  int i, j;

  //printf ("Executing block...\n"); fflush (stdout);
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
  FILE *in_file = NULL;

  while ((option = getopt(argc, argv, "n:s:b:r:i:o:")) != -1)
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
	case 'i':
	  in_file = fopen(optarg, "r");
	  break;
	case 'o':
	  res_file = fopen(optarg, "w");
	  break;
	}
    }

  if (res_file == NULL)
    res_file = fopen("topsort-deps_stream_seidel.out", "w");

  N += 2;

  {
    int num_blocks = (N - 2) / block_size;
    /* Wavefront: one stream per dependence depth. Maximum depth in
       the cube of dependences is the maximum length of a path between
       2 points.  */
    int nstreams = 2 * (num_blocks - 1) + (numiters - 1) + 2; // Last +1 for output task
    int streams[nstreams] __attribute__ ((stream));
    double *data = malloc (N * N * sizeof(double));

    int block_i, block_j;

    struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
    struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));


    /* Add one element to each stream to avoid using a test on the
       tick.  */
    /* Assuming that the sequential traversal order is admissible,
       then there will necessarily be a producer on each stream before
       any consumer.  */
    for (i = 0; i < nstreams; ++i)
      {
	int x;
#pragma omp task output (streams[i] << x)
	x = 0;
      }

    for (i = 0; i < N; ++i)
      for (j = 0; j < N; ++j)
	data[N*i + j] = (double) ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0; //(i*7 +j*13) % 17;

    gettimeofday (start, NULL);
    /* Main kernel start.  ------------------------------------------------------------ */
    for (iter = 0; iter < numiters; iter++)
      for (i = 0, block_i = 0; i < N - 2; i += block_size, block_i++)
	for (j = 0, block_j = 0; j < N - 2; j += block_size, block_j++)
	  {
	    int num_in_streams, max_depth;
	    int sel_streams[3] __attribute__ ((stream_ref));
	    int scal_in_view, scal_out_view;

	    resolve_dependences (block_i, block_j, iter, num_blocks, numiters,
				 sel_streams, &num_in_streams,
				 &max_depth, streams);

	    int in_view[num_in_streams][1];
	    int num_out_streams = num_in_streams + 1;
	    int out_view[num_out_streams][1];


	    /* Work tasks: all the computation occurs here.  */
#pragma omp task peek (sel_streams >> in_view[num_in_streams][1])	\
                 input (sel_streams[num_in_streams] >> scal_in_view)	\
                 output (sel_streams[num_in_streams] << scal_out_view)  \
                 firstprivate (N, block_size, data)
	    {
	      gauss_seidel (N, data + N * i + j, block_size);
	    }

    /* Main kernel end.  -------------------------------------------------------------- */

	    /* In work-streaming, the closed-prefix synchronization
	       scheme means that an old value may be discarded
	       (implicit horizontal synchronization in the
	       dependence depth tree), but in the point-to-point
	       synchronization scheme of DF, we need to use the
	       input clause above to ensure horizontal synch.  */
	    /* The input clause implicitely "ticks" the streams, so
	       the following is not required in the DF expansion
	       scheme.  */
	    /* #pragma omp tick (streams[max_depth + 1]) */

#ifdef _PRINT_TASKGRAPH
	    {
	      int k;
	      printf ("Block (%d, %d, %d) has %d in deps and %d out deps\n", block_i, block_j, iter, num_in_streams, 1); fflush (stdout);
	      printf ("    IN: ");
	      for (k = 0; k < num_in_streams; ++k)
		printf ("%zu  ", (((size_t) sel_streams[k]) - ((size_t) streams[0]))/(sizeof (void*) * 8));
	      printf ("\n    OUT: ");
	      for (k = 0; k < num_out_streams; ++k)
		printf ("%zu  ", (((size_t) sel_streams[k]) - ((size_t) streams[0]))/(sizeof (void*) * 8));
	      printf ("\n");
	    }
#endif
	  }


    int output;
#pragma omp task input (streams[nstreams-1] >> output) firstprivate (res_file, data, N) private (i, j) firstprivate (start, end)
    {
      gettimeofday (end, NULL);

      printf ("%.5f\n", tdiff (end, start));

      if (_WITH_OUTPUT)
	{
	  printf ("[Stream wavefront] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
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
}
