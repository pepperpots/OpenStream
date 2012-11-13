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

typedef struct {
  float history[2];
} fm_quad_demod_filter;

void
fm_quad_demod_init(fm_quad_demod_filter* filter)
{
  filter->history[0]= 0;
  filter->history[1]= 0;
}

void
fm_quad_demod(fm_quad_demod_filter* filter, float i1, float i2, float* result)
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
  x_N.real = i2; x_N.imag = i1;
  x_N_1.real = filter->history[1]; x_N_1.imag = filter->history[0];

  /* compute */
  complex_conj(& x_N_1, & x_N_1_conj);
  complex_mul(& x_N_1_conj, & x_N, & y_N);
  demod = d_gain * complex_arg(& y_N);

  filter->history[0]= i1;
  filter->history[1]= i2;

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

/* *********************************************** main                 */

int
main(int argc, char* argv[])
{
  FILE* input_file = NULL;
  FILE* output_file = NULL;
  FILE* text_file = NULL;
  fpos_t in_begin_pos;

  ntaps_filter_conf lp_2_conf;
  ntaps_filter_conf lp_11_conf, lp_12_conf;
  ntaps_filter_conf lp_21_conf, lp_22_conf;
  ntaps_filter_conf lp_3_conf;
  fm_quad_demod_filter fm_qd_conf;
  ntaps_filter_conf *lp_2_conf_p = &lp_2_conf;
  ntaps_filter_conf *lp_11_conf_p = &lp_11_conf;
  ntaps_filter_conf *lp_12_conf_p = &lp_12_conf;
  ntaps_filter_conf *lp_21_conf_p = &lp_21_conf;
  ntaps_filter_conf *lp_22_conf_p = &lp_22_conf;
  ntaps_filter_conf *lp_3_conf_p = &lp_3_conf;
  fm_quad_demod_filter *fm_qd_conf_p = &fm_qd_conf;

  int final_audio_frequency = 64*KSMPS;
  float input_rate = 512 * KSMPS;
  float inout_ratio;

  int niter = 1;
  int grain = 1;
  int grain8 = 8;
  int grain16 = 16;
  int option;
  float *read_buffer;

  struct timeval *start = (struct timeval *) malloc (sizeof (struct timeval));
  struct timeval *end = (struct timeval *) malloc (sizeof (struct timeval));

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
	  grain = atoi (optarg);
	  grain8 = 8 * grain;
	  grain16 = 16 * grain;
	  break;
	case 'h':
	  printf("Usage: %s [option]...\n\n"
		 "Options:\n"
		 "  -i <input file>              Read data from input file, default is input.dat\n"
		 "  -o <output file>             Write data to output file, default is %s.raw\n"
		 "  -t <text file>               Write output into a text file, default is %s.txt\n"
		 "  -f <frequency>               Set final audio frequency, default is %d\n"
		 "  -n <iterations>              Number of iterations\n"
		 "  -g <grain>                   Set grain, default is %d\n",
		 argv[0], argv[0], argv[0], final_audio_frequency, grain);
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
  fm_quad_demod_init (fm_qd_conf_p);

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

    float fm_qd_buffer __attribute__((stream)), fm_qd_buffer_v[grain8], fm_qd_buffer_sv;
    float fm_qd_buffer_dup __attribute__((stream)), fm_qd_buffer_dup_v[grain8];
    float ffd_buffer __attribute__((stream)), ffd_buffer_v[grain8];
    int serializer[12] __attribute__((stream));

    float view[8];
    int sin, sout;

    int i, j;
    short a, b;

    gettimeofday (start, NULL);

    for (int i = 0; i < 12; ++i)
      {
#pragma omp task output (serializer[i] << sout)
	sout = 0;
      }

    read_buffer = (float *) malloc (grain16 * sizeof (float));

    fgetpos (input_file, &in_begin_pos);
    for (j = 0; j < niter; ++j)
      {
	fsetpos (input_file, &in_begin_pos);
	while (grain16 == fread (read_buffer, sizeof(float), grain16, input_file))
	  {
	    for (i = 0; i < grain8; i++)
	      {
		float v1 = read_buffer[2*i];
		float v2 = read_buffer[2*i + 1];

#pragma omp task firstprivate (v1, v2, fm_qd_conf_p) output (fm_qd_buffer << fm_qd_buffer_sv/*, fm_qd_buffer_dup*/) input (serializer[0] >> sin) output (serializer[0] << sout)
		{
		  fm_quad_demod (fm_qd_conf_p, v1, v2, &fm_qd_buffer_sv);
		  //fm_qd_buffer_dup = fm_qd_buffer;
		}
	      }

#pragma omp task peek (fm_qd_buffer >> fm_qd_buffer_v[grain8]) output (band_11 << band_11_v[grain8]) firstprivate (lp_11_conf_p) input (serializer[1] >> sin) output (serializer[1] << sout)
	    for (i = 0; i < grain8; i++)
	      ntaps_filter_ffd (lp_11_conf_p, 1, &fm_qd_buffer_v[i], &band_11_v[i]);
#pragma omp task peek (fm_qd_buffer >> fm_qd_buffer_v[grain8]) output (band_12 << band_12_v[grain8]) firstprivate (lp_12_conf_p) input (serializer[2] >> sin) output (serializer[2] << sout)
	    for (i = 0; i < grain8; i++)
	      ntaps_filter_ffd (lp_12_conf_p, 1, &fm_qd_buffer_v[i], &band_12_v[i]);
#pragma omp task peek (fm_qd_buffer >> fm_qd_buffer_v[grain8]) output (band_21 << band_21_v[grain8]) firstprivate (lp_21_conf_p) input (serializer[4] >> sin) output (serializer[4] << sout)
	    for (i = 0; i < grain8; i++)
	      ntaps_filter_ffd (lp_21_conf_p, 1, &fm_qd_buffer_v[i], &band_21_v[i]);
#pragma omp task peek (fm_qd_buffer >> fm_qd_buffer_v[grain8]) output (band_22 << band_22_v[grain8]) firstprivate (lp_22_conf_p) input (serializer[5] >> sin) output (serializer[5] << sout)
	    for (i = 0; i < grain8; i++)
	      ntaps_filter_ffd (lp_22_conf_p, 1, &fm_qd_buffer_v[i], &band_22_v[i]);
	    //#pragma omp tick (fm_qd_buffer >> grain8)

#pragma omp task input (band_11 >> band_11_v[grain8], band_12 >> band_12_v[grain8]) output (resume_1 << resume_1_v[grain8])
	      for (i = 0; i < grain8; i++)
		subctract (band_11_v[i], band_12_v[i], &resume_1_v[i]);
#pragma omp task input (band_21 >> band_21_v[grain8], band_22 >> band_22_v[grain8]) output (resume_2 << resume_2_v[grain8])
	      for (i = 0; i < grain8; i++)
		subctract (band_21_v[i], band_22_v[i], &resume_2_v[i]);
#pragma omp task input (resume_1 >> resume_1_v[grain8], resume_2 >> resume_2_v[grain8]) output (ffd_buffer << ffd_buffer_v[grain8])
	      for (i = 0; i < grain8; i++)
		multiply_square (resume_1_v[i], resume_2_v[i], &ffd_buffer_v[i]);



#pragma omp task input (fm_qd_buffer >> fm_qd_buffer_dup_v[grain8]) output (band_2 << band_2_v[grain]) firstprivate (lp_2_conf_p) input (serializer[8] >> sin) output (serializer[8] << sout)
	      for (i = 0; i < grain; i++)
		ntaps_filter_ffd (lp_2_conf_p, 8, &fm_qd_buffer_dup_v[8*i], &band_2_v[i]);
#pragma omp task input (ffd_buffer >> ffd_buffer_v[grain8]) output (band_3 << band_3_v[grain]) firstprivate (lp_3_conf_p) input (serializer[9] >> sin) output (serializer[9] << sout)
	      for (i = 0; i < grain; i++)
		ntaps_filter_ffd (lp_3_conf_p, 8, &ffd_buffer_v[8*i], &band_3_v[i]);
#pragma omp task input (band_2 >> band_2_v[grain], band_3 >> band_3_v[grain]) output (output1 << output1_v[grain], output2 << output2_v[grain])
	      for (i = 0; i < grain; i++)
		stereo_sum (band_2_v[i], band_3_v[i], &output1_v[i], &output2_v[i]);

#pragma omp task input (output1 >> output1_v[grain], output2 >> output2_v[grain]) firstprivate (output_file, text_file) input (serializer[11] >> sin) output (serializer[11] << sout)
	      for (i = 0; i < grain; i++)
		{
		  a = dac_cast_trunc_and_normalize_to_short (output1_v[i]);
		  b = dac_cast_trunc_and_normalize_to_short (output2_v[i]);
		  fwrite (&a, sizeof(short), 1, output_file);
		  fwrite (&b, sizeof(short), 1, output_file);
		  fprintf (text_file, "%-10.5f %-10.5f\n", output1_v[i], output2_v[i]);
		}
	  }
      }

    for (i = 0; i < 12; ++i)
      {
#pragma omp tick (serializer[i])
      }


#pragma omp taskwait
  }
  gettimeofday (end, NULL);

  printf ("%.5f\n", tdiff (end, start));

  fclose (input_file);
  fclose (output_file);
  fclose (text_file);

  return 0;
}
