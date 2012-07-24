#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>

#include <getopt.h>

#define _WITH_OUTPUT 0

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
		     void **stream_in_deps, int *num_in_deps,
		     void **stream_out_deps, int *num_out_deps,
		     void **streams)
{
  int pos_in = 0;
  int pos_out = 0;

  // position of the block
  int block_pos = block_i + block_j*num_blocks + iter*num_blocks*num_blocks;

  /* PRODUCER ROLE: producer block position identifies the base index
     of the stream in the array (with a *4 multiplier).  */

  /* A task is always depended on by 4 other tasks, except on the
     borders of the cube.  */
  if (block_i != num_blocks - 1) // there are successors on the i dimension
    stream_out_deps[pos_out++] = streams[block_pos * 4];
  if (block_j != num_blocks - 1) // there are successors on the j dimension
    stream_out_deps[pos_out++] = streams[block_pos * 4 + 1];
  if (iter != numiters - 1) // there are successors on the iter dimension
    {
      /* If this is not the first block on this dimension, there's an
	 anti-dependence to the next iteration's previous block along
	 this dimension.  */
      if (block_i != 0)
	stream_out_deps[pos_out++] = streams[block_pos * 4 + 2];
      if (block_j != 0)
	stream_out_deps[pos_out++] = streams[block_pos * 4 + 3];
    }

  /* CONSUMER ROLE: as the producer is the base, identify the source
     of input deps.  */

  /* A task always depends on 4 other tasks, except on the borders of
     the cube.  */
  if (block_i != 0) // there are predecessors on the i dimension
    stream_in_deps[pos_in++] = streams[(block_pos - 1) * 4];
  if (block_j != 0) // there are predecessors on the j dimension
    stream_in_deps[pos_in++] = streams[(block_pos - num_blocks) * 4 + 1];
  if (iter != 0) // there are predecessors on the iter dimension
    {
      /* If this is not the last block on this dimension, there's an
	 anti-dependence from the previous iteration's next block
	 along this dimension.  */
      if (block_i < num_blocks - 1)
	stream_in_deps[pos_in++] = streams[(block_pos + 1 - num_blocks*num_blocks) * 4 + 2];
      if (block_j < num_blocks - 1)
	stream_in_deps[pos_in++] = streams[(block_pos + num_blocks - num_blocks*num_blocks) * 4 + 3];
    }

  /* SPECIAL case: once we get to the end of each iter level, there's
     no cross-level anti dependence to subsume the output dependence
     => we need to add the output dep */
  if ((block_i == num_blocks - 1) && (block_j == num_blocks - 1))
    {
      if (iter != 0)
	stream_in_deps[pos_in++] = streams[(block_pos - num_blocks*num_blocks) * 4 + 2];
      if (iter != numiters - 1)
	stream_out_deps[pos_out++] = streams[block_pos * 4 + 2];

      // for the final output, just send notice to the output task.
      if (iter == numiters - 1)
	stream_out_deps[pos_out++] = streams[block_pos * 4 + 3];
    }


  *num_in_deps = pos_in;
  *num_out_deps = pos_out;
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
  int block_size = 8;

  FILE *res_file = NULL;
  FILE *in_file = NULL;

  int volatile res = 0;

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
    res_file = fopen("stream_seidel.out", "w");

  N += 2;

  {
    int num_blocks = (N - 2) / block_size;
    int nstreams = (num_blocks * num_blocks * numiters) * 4;
    int streams[nstreams] __attribute__ ((stream));
    double *data;

    int streams_in[4] __attribute__ ((stream_ref));
    int streams_out[4] __attribute__ ((stream_ref));
    //void *streams_in[4];
    //void *streams_out[4];
    int in_view[4][1];
    int out_view[4][1];

    int block_i, block_j;

    struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
    struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

    posix_memalign ((void**) &(data), 128, sizeof(double[N][N]));

    for (i = 0; i < N; ++i)
      for (j = 0; j < N; ++j)
	data[N*i + j] = ((i == 25 && j == 25) || (i == N-25 && j == N-25)) ? 500 : 0; //(i*7 +j*13) % 17;

    gettimeofday (start, NULL);

    /* Main kernel start.  ------------------------------------------------------------ */
    for (iter = 0; iter < numiters; iter++)
      for (i = 0, block_i = 0; i < N - 2; i += block_size, block_i++)
	for (j = 0, block_j = 0; j < N - 2; j += block_size, block_j++)
	  {
	    int k;
	    int num_in_streams, num_out_streams;

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
	    resolve_dependences (block_i, block_j, iter, num_blocks, numiters,
				 streams_in, &num_in_streams,
				 streams_out, &num_out_streams,
				 streams);

	    /* Work tasks: all the computation occurs here.  */
#pragma omp task input (streams_in >> in_view[num_in_streams][1]) output (streams_out << out_view[num_out_streams][1]) firstprivate (N, block_size, data)
	    gauss_seidel (N, data + N * i + j, block_size);

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
#pragma omp task input (streams[nstreams-1] >> output) firstprivate (res_file, data, N) firstprivate (start, end)
    {
      int i, j;

      gettimeofday (end, NULL);

      printf ("%.5f\n", tdiff (end, start));

      if (_WITH_OUTPUT)
	{
	  printf ("[Stream] Seidel (size %d, tile %d, iterations %d) executed in %.5f seconds\n",
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
