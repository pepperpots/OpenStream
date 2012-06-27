#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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

static inline void
reorder_block (int size_out, fftw_complex *in, fftw_complex *out1, fftw_complex *out2)
{
  int k;
  for (k = 0; k < size_out; ++k)
    {
      out1[k] = in[2*k];
      out2[k] = in[2*k + 1];
    }
}

static inline void
partial_DFT_block (int size_in, fftw_complex *in1, fftw_complex *in2, fftw_complex *out, fftw_complex *coeffs)
{
  int k;
  for (k = 0; k < size_in; ++k)
    {
      fftw_complex t = in2[k] * coeffs[k];
      fftw_complex in_place_temp = in1[k];
      out[k] = in_place_temp + t;
      out[size_in + k] = in_place_temp - t;
    }
}


int
main (int argc, char *argv[])
{
  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));
  const double TWOPI = 6.2831853071795864769252867665590057683943388;
  int i, j, k, l, m, option, it;
  fftw_complex **twids;

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
    res_file = fopen("radix_fft_stream.out", "w");


  twids = fftw_malloc (sizeof(fftw_complex *) * radix);
  for (i = 0; i < radix; ++i)
    {
      int block_size = N >> i;

      twids[i] = fftw_malloc (sizeof(fftw_complex) * block_size);

      for (j = 0; j < block_size; ++j)
	twids[i][j] = cexp((I * FFTW_FORWARD * TWOPI/block_size) * j);
    }

  fftw_complex *data = (fftw_complex*) fftw_malloc (sizeof(fftw_complex) * N);

  // Generate random numbers or read from file
  if (in_file == NULL)
    {
      long seed[4] = {0, 0, 0, 1};
      long sp = 2;
      clarnv_(&sp, seed, &N, data);

      // Also allow saving data sessions
      //if (res_file != NULL)
      //fwrite (data, sizeof (double), size, res_file);
    }
  else
    {
      fread (data, sizeof(fftw_complex), N, in_file);
    }

  /* WARNING: running multiple iterations is too relaxed for now
     (parallelism exploited between different iterations) and the exit
     time is only measured on the last iteration's exit, which may not
     be the last task (needs a barrier).  */

  {
    fftw_complex streams[(1 << (radix+2))] __attribute__((stream));
    fftw_complex fake_plan_in, fake_plan_out;
    fftw_plan plan =
      fftw_plan_dft_1d (N2, &fake_plan_in, &fake_plan_out,
			FFTW_FORWARD, FFTW_ESTIMATE);

    gettimeofday (start, NULL);
    for (it = 0; it < numiters; ++it)
      {
	fftw_complex vout[N];

	//printf ("T0 in[ ] out[ %d]\n", 0); fflush (stdout);
	/* 1. Get input data.  */
#pragma omp task output (streams[0] << vout[N]) firstprivate (data)
	{
	  memcpy (vout, data, N * sizeof (fftw_complex));
	}

	/* 2. Apply radix block reorder stages.  */
	for(i = 0; i < radix; ++i)
	  {
	    int num_blocks = 1 << i;
	    int block_size = N >> i;
	    int stream_index_base = (1 << (i + 1)) - 1;

	    for (k = 0; k < num_blocks; ++k)
	      {
		fftw_complex vin[block_size];
		fftw_complex vout1[block_size >> 1];
		fftw_complex vout2[block_size >> 1];

		//printf ("T1 in[%d ] out[%d   %d]\n", stream_index_base - num_blocks + k, stream_index_base + 2 * k, stream_index_base + 2 * k + 1); fflush (stdout);
#pragma omp task input (streams[stream_index_base - num_blocks + k] >> vin[block_size])						\
  output (streams[stream_index_base + 2 * k] << vout1[block_size >> 1]) \
  output (streams[stream_index_base + 2 * k + 1] << vout2[block_size >> 1])
		{
		  //printf ("  >> reorder block [%d] (%d,%d) \n", block_size, i, k); fflush (stdout);
		  reorder_block (block_size >> 1, vin, vout1, vout2);
		}
	      }
	  }

	for(i = 0; i < N1; ++i)
	  {
	    fftw_complex vin[N2];
	    fftw_complex vout[N2];

	    //printf ("T2 in[%d  ] out[%d  ]\n", N1 - 1 + i, 2*N1 - 1 + i); fflush (stdout);
	    /* 3. Apply N1 size-N2=N/N1 DFTs.  */
#pragma omp task input (streams[N1 - 1 + i] >> vin[N2]) firstprivate (plan) output (streams[2*N1 - 1 + i] << vout[N2])
	    {
	      //printf ("  >> processing block [%d] (%d) [[ %zu | %f | %f ]]\n", N2, i, plan, creal(vin[0]), cimag(vout[0])); fflush (stdout);
	      fftw_execute_dft (plan, vin, vout);
	    }
	  }

	/* 4. Apply radix partial block DFT stages.  */
	for(i = radix - 1; i >= 0; --i)
	  {
	    int num_blocks = 1 << i;
	    int block_size = N >> i;
	    int stream_index_base_in = (1 << (radix + 2)) - (1 << (i + 2)) - 1;

	    for (k = 0; k < num_blocks; ++k)
	      {
		fftw_complex vin1[block_size >> 1];
		fftw_complex vin2[block_size >> 1];
		fftw_complex vout[block_size];
		fftw_complex *twid = twids[i];

		//printf ("T3 in[%d   %d] out[%d  ]\n", stream_index_base_in + 2 * k, stream_index_base_in + 2 * k + 1, stream_index_base_in + 2 * num_blocks + k); fflush (stdout);
#pragma omp task input (streams[stream_index_base_in + 2 * k] >> vin1[block_size >> 1]) \
  input (streams[stream_index_base_in + 2 * k + 1] >> vin2[block_size >> 1]) \
  firstprivate (twid)							\
  output (streams[stream_index_base_in + 2 * num_blocks + k] << vout[block_size])
		{
		  partial_DFT_block (block_size >> 1, vin1, vin2, vout, twid);
		  //printf ("  >> DFT merge block [%d] (%d,%d) \n", block_size, i, k); fflush (stdout);
		}
	      }
	  }

	{
	  fftw_complex vin[N];
	  //printf ("T4 in[%d  ] out[  ]\n", (1 << (radix + 2)) - 3); fflush (stdout);
#pragma omp task input (streams[(1 << (radix + 2)) - 3] >> vin[N])
	  {
	    if (it == numiters - 1)
	      {
		gettimeofday (end, NULL);
		printf ("%.5f \n", tdiff (end, start));
	      }

	    //printf ("  OUTPUT  \n"); fflush (stdout);
	    for(i = 0; i < N; ++i)
	      {
		res += cimag (vin[i]);
		//fprintf(res_file, "%f \t %f\n", creal(vin[i]), cimag(vin[i]));
	      }
	  }
	}
      }
  }

  if (in_file != NULL) fclose (in_file);
}
