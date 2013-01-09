/* A simple approach to FMradio.
 *
 * Authors:
 * Antoniu Pop, INRIA, 2008-2012
 * David Rodenas-Pico, BSC, 2007
 * Marco Cornero, 2006
 */


#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include "../common/common.h"
#include "../common/sync.h"

#define _WITH_OUTPUT 0

#include <unistd.h>

#define KSMPS 1024
#define FM_MAX       5
#define FM_LIMIT     (65536/2)-1


/* *********************************************** complex operations   */

typedef struct
{
  float real;
  float imag;
} complex;

#define M_PI   3.14159265358979323846  /* pi */
#define GAIN_LMR  2000

#define complex_conj(s, d)			\
  {						\
    (d)->real = (s)->real;			\
    (d)->imag = -1 * (s)->imag;			\
  }

#define complex_arg(x)				\
  atan2((x)->imag, (x)->real)

#define complex_mul(a, b, d)						\
  {									\
    (d)->real = (a)->real * (b)->real - ((a)->imag * (b)->imag);	\
    (d)->imag = (a)->real * (b)->imag + (a)->imag * (b)->real;		\
  }


/* *********************************************** auxiliar functions   */

short
dac_cast_trunc_and_normalize_to_short(float f)
{
  short x;
  f = (f / FM_MAX)*FM_LIMIT;
  x = f;
  return x;
}

#if 0
/* *********************************************** file filters         */

void
file_to_stream(FILE* file, stream_t* output)
{
  int i;
  int room;
  float value;

  room= stream_room(output);
  for (i= 0; i < room; i++) {
    if (fread(&value, sizeof(value), 1, file)) {
      stream_push(output, &value);
    }
  }
}

void
stream_to_file(stream_t* input, FILE* file)
{
  int i;
  int count;
  float value;
  short res_int;

  count= stream_count(input);
  for (i= 0; i < count; i++)
    {
      stream_pop(input, &value);
      //fwrite(&value, sizeof(value), 1, file);
      res_int= dac_cast_trunc_and_normalize_to_short(value);
      fwrite(&res_int, sizeof(res_int), 1, file);
    }
}
#endif


/* *********************************************** fm_quad_demod        */

void
fm_quad_demod(float *in_raw, float* result)
{
  complex x_N;
  complex x_N_1;
  complex x_N_1_conj;
  complex y_N;
  float demod, d_gain = 0.5;

  /*
   *
   * y(n) = angle(x(n)*conj(x(n-1))
   *
   */

  /* read two complex data */
  x_N.real = in_raw[3]; x_N.imag = in_raw[2];
  x_N_1.real = in_raw[1]; x_N_1.imag = in_raw[0];

  /* compute */
  complex_conj(& x_N_1, & x_N_1_conj);
  complex_mul(& x_N_1_conj, & x_N, & y_N);
  demod = d_gain * complex_arg(& y_N);

  *result= demod;
}



/* *********************************************** ntaps_filter         */

typedef struct ntaps_filter_conf
{
  double* coeff;
  int decimation;
  int taps;
  float*  history;
  int next;
} ntaps_filter_conf;

#define WIN_HAMMING     0
#define WIN_HANNING     1
#define WIN_BLACKMAN    2

void
compute_window(unsigned int ntaps, double *taps, unsigned int type)
{
  int n;
  int M = ntaps - 1;    // filter order

  if(type == WIN_HAMMING)
    {
      for (n = 0; n < ntaps; n++)
        taps[n] = 0.54 - 0.46 * cos ((2 * M_PI * n) / M);
    }
  else if(type == WIN_HANNING)
    {
      for (n = 0; n < ntaps; n++)
        taps[n] = 0.5 - 0.5 * cos ((2 * M_PI * n) / M);
    }
  else if(type == WIN_BLACKMAN)
    {
      for (n = 0; n < ntaps; n++)
        taps[n] = 0.42 - 0.50 * cos ((2*M_PI * n) / (M-1)) - 0.08 * cos ((4*M_PI * n) / (M-1));
    }
}

int
compute_ntaps (float sampling_freq,
	       float transition_width,
	       int window_type)
{
  /* Mormalized transition width */
  float delta_f = transition_width / sampling_freq;

  float width_factor[3] = { 3.3, 3.1, 5.5 };

  /* compute number of taps required for given transition width */
  int ntaps = (int) (width_factor[window_type]/ delta_f + 0.5);

  if ((ntaps & 1) == 0)
    ntaps++;

  return ntaps;
}

void
ntaps_filter_ffd_init (ntaps_filter_conf *conf,
		       double cutoff_freq,
		       double transition_band,
		       double gain,
		       int decimation,
		       double sampling_rate,
		       int window_type)
{
  /* Taken from the GNU software radio .. */

  int n;
  int i;
  unsigned int ntaps;
  double* w;
  int M;
  double fwT0;
  double fmax;

  ntaps = compute_ntaps(sampling_rate, transition_band, window_type);
  conf->coeff = malloc(ntaps * sizeof(double));
  w = malloc(ntaps * sizeof(double));
  conf->taps = ntaps;
  conf->decimation = decimation;

  compute_window(ntaps, w, window_type);

  M = (ntaps - 1) / 2;

  fwT0 = 2 * M_PI * cutoff_freq / sampling_rate;

  for (n=0; n<ntaps; n++)
    conf->coeff[n]=0.0;

  for (n = -M; n <= M; n++) {
    if (n == 0)
      conf->coeff[n + M] = fwT0 / M_PI * w[n + M];
    else
      conf->coeff[n + M] =  sin (n * fwT0) / (n * M_PI) * w[n + M];
  }

  fmax = conf->coeff[0 + M];

  for (n = 1; n <= M; n++)
    fmax += 2 * conf->coeff[n + M];

  gain /= fmax; // normalize

  for (i = 0; i < ntaps; i++)
    conf->coeff[i] *= gain;

  // init history
  conf->next= 0;
  conf->history= (float*)malloc(sizeof(float) * conf->taps);
  for (n= 0; n < conf->taps; n++)
    {
      conf->history[n]= 0;
    }

  free(w);
}

void
ntaps_filter_ffd(ntaps_filter_conf* conf
		 , int input_size, float input[]
		 , float* result)
{
  int i;
  float sum;

  assert(input_size == conf->decimation);

  for (i= 0; i < conf->decimation; i++)
    {
      conf->history[conf->next]= input[i];
      conf->next= (conf->next + 1) % conf->taps;
    }

  sum= 0.0;
  for (i= 0; i < conf->taps; i++)
    {
      sum= sum
	+ conf->history[(conf->next + i) % conf->taps]
	* conf->coeff[conf->taps - i - 1];
    }

  *result= sum;
}

/* *********************************************** stereo_sum           */
void
stereo_sum (float data_spm, float data_smm, float* left, float* right)
{
  *left = (data_spm + data_smm);
  *right = (data_spm - data_smm);
}

/* *********************************************** subctract            */
void
subctract (float i1, float i2, float* result)
{
  *result = i1 - i2;
}

/* *********************************************** multiply_square      */
void
multiply_square (float i1, float i2, float* result)
{
  *result = GAIN_LMR * i1 * i2 * i2;
}



void
stream_ntaps_filter_ffd (ntaps_filter_conf* conf, int size, int decimation,
			 float in_stream __attribute__((stream_ref)),
			 float out_stream __attribute__((stream_ref)),
			 int serializer __attribute__((stream_ref)))
{
  float temp_stream __attribute__((stream));
  int temp_view_size = conf->taps + size;
  float temp_view[temp_view_size];
  int out_size = size/decimation;
  float in_view[size], out_view[out_size];
  int sin, sout, i, j;

  /* Get a data chunk, merge with history, then send full
     data input to work task.  */
#pragma omp task peek (in_stream >> in_view[size]) output (temp_stream << temp_view[temp_view_size]) firstprivate (conf) input (serializer >> sin) output (serializer << sout)
  {
    /* Copy current history plus new data to output
       window.  Split the copy according to wrap-around in
       history buffer.  */
    memcpy (&temp_view[0], &conf->history[conf->next], (conf->taps - conf->next) * sizeof (float));
    memcpy (&temp_view[conf->taps - conf->next], &conf->history[0], conf->next * sizeof (float));
    memcpy (&temp_view[conf->taps], &in_view[0], size * sizeof (float));
    /* Update the history.  */
    for (i = 0; i < size; ++i)
      {
	conf->history[conf->next] = in_view[i];
	conf->next++;
	if (conf->next == conf->taps) conf->next = 0;
      }
  }

#pragma omp task input (temp_stream >> temp_view[temp_view_size]) output (out_stream << out_view[out_size]) firstprivate (conf)
  for (i = 0; i < out_size; ++i)
    {
      float sum = 0.0;

      for (j = 0; j < conf->taps; ++j)
	sum += temp_view[i*decimation + j + decimation] * conf->coeff[conf->taps - j - 1];

      out_view[i] = sum;
    }
}

void
stream_source_raw_data (float *data_in, int in_samples, int grain8, int grain16, int iter, int pos,
			float in_raw __attribute__((stream_ref)),
			int serializer __attribute__((stream_ref)))
{
  float in_raw_v[grain16+2];
  int sin, sout, i;

  /* Special cases for the initialization of the data
     stream: the sequential version will use two 0.0 input
     elements in the very first data samples, then reusing
     the previous two.  We reproduce this feature here.  */


  /* 0.0 case.  */
  if (iter == 0 && pos == 0)
#pragma omp task firstprivate (data_in) output (in_raw << in_raw_v[grain16+2]) input (serializer >> sin) output (serializer << sout)
    {
      /* Peeled first iteration.  */
      in_raw_v[0] = 0.0;
      in_raw_v[1] = 0.0;

      /* Shifted input from data array.  */
      for (i = 1; i <= grain8; i++)
	{
	  in_raw_v[2*i] = data_in[2*i - 2];
	  in_raw_v[2*i + 1] = data_in[2*i - 1];
	}
    }
  /* wrap-around history case.  */
  else if (iter != 0 && pos == 0)
#pragma omp task firstprivate (data_in) output (in_raw << in_raw_v[grain16+2]) input (serializer >> sin) output (serializer << sout)
    {
      /* Peeled first iteration.  */
      in_raw_v[0] = data_in[in_samples - 2];
      in_raw_v[1] = data_in[in_samples - 1];

      /* Shifted input from data array.  */
      for (i = 1; i <= grain8; i++)
	{
	  in_raw_v[2*i] = data_in[2*i - 2];
	  in_raw_v[2*i + 1] = data_in[2*i - 1];
	}
    }
  /* Default case.  */
  else /* Importantly: pos != 0  */
#pragma omp task firstprivate (data_in) output (in_raw << in_raw_v[grain16+2]) input (serializer >> sin) output (serializer << sout)
    for (i = 0; i <= grain8; i++)
      {
	in_raw_v[2*i] = data_in[pos + 2*i - 2];
	in_raw_v[2*i + 1] = data_in[pos + 2*i - 1];
      }
}

/* *********************************************** main                 */

int
main(int argc, char* argv[])
{
  FILE *input_file = NULL;
  FILE *output_file = NULL;
  FILE *text_file = NULL;

  int in_samples = 1 << 19;
  int out_samples = 1 << 17;

  ntaps_filter_conf lp_2_conf;
  ntaps_filter_conf lp_11_conf, lp_12_conf;
  ntaps_filter_conf lp_21_conf, lp_22_conf;
  ntaps_filter_conf lp_3_conf;
  ntaps_filter_conf *lp_2_conf_p = &lp_2_conf;
  ntaps_filter_conf *lp_11_conf_p = &lp_11_conf;
  ntaps_filter_conf *lp_12_conf_p = &lp_12_conf;
  ntaps_filter_conf *lp_21_conf_p = &lp_21_conf;
  ntaps_filter_conf *lp_22_conf_p = &lp_22_conf;
  ntaps_filter_conf *lp_3_conf_p = &lp_3_conf;

  int final_audio_frequency = 64*KSMPS;
  float input_rate = 512 * KSMPS;
  float inout_ratio;

  int niter = 1;
  int grain = 4;
  int option;

  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

  struct profiler_sync sync;

  PROFILER_NOTIFY_PREPARE(&sync);

  while ((option = getopt(argc, argv, "i:o:t:f:n:g:h")) != -1)
    {
      switch(option)
	{
	case 'i':
	  input_file = fopen (optarg, "r");
	  break;
	case 'o':
	  output_file = fopen (optarg, "w");
	  break;
	case 't':
	  text_file = fopen (optarg, "w");
	  break;
	case 'f':
	  final_audio_frequency = atoi(optarg);
	  break;
	case 'n':
	  niter = atoi(optarg);
	  break;
	case 'g':
	  grain = 1 << atoi (optarg);
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -i <input file>              Read data from input file, default is input.dat\n"
		 "  -o <output file>             Write data to output file, default is %s.raw\n"
		 "  -t <text file>               Write output into a text file, default is %s.txt\n"
		 "  -f <frequency>               Set final audio frequency, default is %d\n"
		 "  -n <iterations>              Number of iterations\n"
		 "  -g <grain power>             Set grain as a power of 2, default is %d\n",
		 argv[0], argv[0], argv[0], final_audio_frequency, grain);
	  exit(0);
	  break;
	case '?':
	  fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
	  exit(1);
	  break;
	}
    }

  if (optind != argc)
    {
      fprintf(stderr, "Too many arguments. Run %s -h for usage.\n", argv[0]);
      exit(1);
    }

  int grain8 = 8 * grain;
  int grain16 = 16 * grain;

  if (input_file == NULL)
    input_file = fopen ("input.dat", "r");
  if (output_file == NULL)
    {
      char output_filename[4096];
      sprintf (output_filename, "%s.raw", argv[0]);
      output_file = fopen (output_filename, "w");
    }
  if (text_file == NULL)
    {
      char text_filename[4096];
      sprintf (text_filename, "%s.txt", argv[0]);
      text_file = fopen (text_filename, "w");
    }

  inout_ratio = ((int) input_rate)/final_audio_frequency;
  ntaps_filter_ffd_init (lp_2_conf_p,  15000, 4000, 0.5, inout_ratio, input_rate, WIN_HANNING);
  ntaps_filter_ffd_init (lp_11_conf_p, 53000, 4000, 1,   1,           input_rate, WIN_HANNING);
  ntaps_filter_ffd_init (lp_12_conf_p, 23000, 4000, 1,   1,           input_rate, WIN_HANNING);
  ntaps_filter_ffd_init (lp_21_conf_p, 21000, 2000, 1,   1,           input_rate, WIN_HANNING);
  ntaps_filter_ffd_init (lp_22_conf_p, 17000, 2000, 1,   1,           input_rate, WIN_HANNING);
  ntaps_filter_ffd_init (lp_3_conf_p,  15000, 4000, 1.0, inout_ratio, input_rate, WIN_HANNING);

  float *data_in = malloc (in_samples * sizeof (float));
  float *data_out_raw = malloc (out_samples * sizeof (short));
  float *data_out_flt = malloc (out_samples * sizeof (float));
  fread (data_in, sizeof (float), in_samples, input_file);


  {
    float band_2 __attribute__((stream)), band_2_v[grain];
    float band_3 __attribute__((stream)), band_3_v[grain];
    float band_11 __attribute__((stream)), band_11_v[grain8];
    float band_12 __attribute__((stream)), band_12_v[grain8];
    float resume_1 __attribute__((stream)), resume_1_v[grain8];
    float band_21 __attribute__((stream)), band_21_v[grain8];
    float band_22 __attribute__((stream)), band_22_v[grain8];
    float resume_2 __attribute__((stream)), resume_2_v[grain8];

    float output1 __attribute__((stream)), output1_v[grain];
    float output2 __attribute__((stream)), output2_v[grain];

    float in_raw __attribute__((stream)), in_raw_v[grain16+2];

    float fm_qd_buffer __attribute__((stream)), fm_qd_buffer_v[grain8];
    float ffd_buffer __attribute__((stream)), ffd_buffer_v[grain8];

    int serializer[7] __attribute__((stream));

    int sin, sout;

    int i, j, k;

    gettimeofday (start, NULL);
    PROFILER_NOTIFY_RECORD(&sync);

    for (i = 0; i < 7; ++i)
#pragma omp task output (serializer[i] << sout)
      sout = 0;


    for (j = 0; j < niter; ++j)
      {
	for (k = 0; k < in_samples; k += grain16)
	  {
	    stream_source_raw_data (data_in, in_samples, grain8, grain16, j, k, in_raw, serializer[0]);

#pragma omp task input (in_raw >> in_raw_v[grain16+2]) output (fm_qd_buffer << fm_qd_buffer_v[grain8])
	    for (i = 0; i < grain8; i++)
	      fm_quad_demod (&in_raw_v[2*i], &fm_qd_buffer_v[i]);


	    stream_ntaps_filter_ffd (lp_11_conf_p, grain8, 1, fm_qd_buffer, band_11, serializer[1]);
	    stream_ntaps_filter_ffd (lp_12_conf_p, grain8, 1, fm_qd_buffer, band_12, serializer[2]);
	    stream_ntaps_filter_ffd (lp_21_conf_p, grain8, 1, fm_qd_buffer, band_21, serializer[3]);
	    stream_ntaps_filter_ffd (lp_22_conf_p, grain8, 1, fm_qd_buffer, band_22, serializer[4]);

#pragma omp task input (band_11 >> band_11_v[grain8], band_12 >> band_12_v[grain8]) output (resume_1 << resume_1_v[grain8])
	    for (i = 0; i < grain8; i++)
	      subctract (band_11_v[i], band_12_v[i], &resume_1_v[i]);
#pragma omp task input (band_21 >> band_21_v[grain8], band_22 >> band_22_v[grain8]) output (resume_2 << resume_2_v[grain8])
	    for (i = 0; i < grain8; i++)
	      subctract (band_21_v[i], band_22_v[i], &resume_2_v[i]);
#pragma omp task input (resume_1 >> resume_1_v[grain8], resume_2 >> resume_2_v[grain8]) output (ffd_buffer << ffd_buffer_v[grain8])
	    for (i = 0; i < grain8; i++)
	      multiply_square (resume_1_v[i], resume_2_v[i], &ffd_buffer_v[i]);


	    stream_ntaps_filter_ffd (lp_2_conf_p, grain8, 8, fm_qd_buffer, band_2, serializer[5]);
#pragma omp tick (fm_qd_buffer >> grain8)
	    stream_ntaps_filter_ffd (lp_3_conf_p, grain8, 8, ffd_buffer, band_3, serializer[6]);
#pragma omp tick (ffd_buffer >> grain8)


#pragma omp task input (band_2 >> band_2_v[grain], band_3 >> band_3_v[grain]) output (output1 << output1_v[grain], output2 << output2_v[grain])
	    for (i = 0; i < grain; i++)
	      stereo_sum (band_2_v[i], band_3_v[i], &output1_v[i], &output2_v[i]);

#pragma omp task input (output1 >> output1_v[grain], output2 >> output2_v[grain])
	    for (i = 0; i < grain; i++)
	      {
		short a, b;
		a = dac_cast_trunc_and_normalize_to_short (output1_v[i]);
		b = dac_cast_trunc_and_normalize_to_short (output2_v[i]);
		data_out_raw[(k >> 3) + 2*i] = a;
		data_out_raw[(k >> 3) + 2*i + 1] = b;
		data_out_flt[(k >> 3) + 2*i] = output1_v[i];
		data_out_flt[(k >> 3) + 2*i + 1] = output2_v[i];
	      }
	  }
      }

    for (i = 0; i < 7; ++i)
      {
#pragma omp tick (serializer[i])
      }

#pragma omp taskwait

    PROFILER_NOTIFY_PAUSE(&sync);

    gettimeofday (end, NULL);

    printf ("%.5f\n", tdiff (end, start));
  }

#if _WITH_OUTPUT
  int i;

  fwrite (data_out_raw, sizeof (short), out_samples, output_file);
  for (i = 0; i < out_samples; i += 2)
    fprintf (text_file, "%-10.4f %-10.4f\n", data_out_flt[i], data_out_flt[i + 1]);
#endif

  fclose (input_file);
  fclose (output_file);
  fclose (text_file);

  PROFILER_NOTIFY_FINISH(&sync);

  return 0;
}
