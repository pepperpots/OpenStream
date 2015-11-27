#include <stdio.h>
#include <getopt.h>
#include "../common/common.h"
#include "../common/sync.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "cholesky_common.h"

#ifdef USE_MKL
  #include <mkl_cblas.h>
  #include <mkl_lapack.h>
#else
  #include <cblas.h>
  #include "../common/lapack.h"
#endif

void* screate_ref;
void* sfinal_ref;
void* sdpotrf_ref;
void* sdsyrk_ref;
void* sdtrsm_ref;
void* sdgemm_ref;
double* global_matrix;
int N;
int T;
int ntiles;
int padding;
int padding_elements;

//#define DEBUG

#ifdef DEBUG
#define debug_printf(...) printf(__VA_ARGS___)
#else
#define debug_printf(...)
#endif

void cholesky_init_random_matrix(double* matrix, int N)
{
	int seed[4] = {1092, 43, 77, 1};
	int sp = 1;
	int size = N*N;
	dlarnv_(&sp, seed, &size, matrix);

	for(int i = 0; i < N; ++i)
		matrix[i*N + i] += N;
}

int cholesky_verify(double* matrix, double* seq_input_matrix, int N, int padding_elements)
{
	char upper = 'U';
	int Npad = N+padding_elements;
	int nfo;

	dpotrf_(&upper, &N, seq_input_matrix, &Npad, &nfo);

	for(int x = 0; x < N; x++) {
		for(int y = 0; y < N-x; y++) {
			if(!double_equal(matrix[y*Npad+x], seq_input_matrix[y*Npad+x])) {
				fprintf(stderr, "Data differs at Y = %d, X = %d: expect %.20f, but was %.20f, diff is %.20f, reldiff = %.20f\n", y, x, seq_input_matrix[y*Npad+x], matrix[y*Npad+x], fabs(seq_input_matrix[y*Npad+x] - matrix[y*Npad+x]), fabs(seq_input_matrix[y*Npad+x] - matrix[y*Npad+x]) / fabs(seq_input_matrix[y*Npad+x]));
				return 0;
			}
		}
	}

	return 1;
}

static void copy_block_from_global(double* out, int id_x, int id_y)
{
	int Npad = N + padding_elements;

	for(int y = 0; y < T; y++)
		for(int x = 0; x < T; x++)
			out[y*T+x] = global_matrix[(id_y*T+y)*Npad+id_x*T+x];
}

static void copy_block_to_global(double* in, int id_x, int id_y)
{
	int Npad = N + padding_elements;

	for(int y = 0; y < T; y++)
		for(int x = 0; x < T; x++)
			global_matrix[(id_y*T+y)*Npad+id_x*T+x] = in[y*T+x];
}

static inline int IDX(int id_x, int id_y)
{
	return id_y*ntiles+id_x;
}

static inline int IDX_IT(int id_x, int id_y, int it)
{
	return (it*ntiles*ntiles)+(id_y*ntiles+id_x);
}

static inline int IDX_DSYRK(int id_x, int id_y, int it)
{
	return IDX_IT(id_x, id_y, it);
}

static inline int IDX_DPOTRF(int id_x)
{
	return IDX(id_x, id_x);
}

static inline int IDX_DTRSM(int id_x, int id_y)
{
	return IDX(id_x, id_y);
}

static inline int IDX_DGEMM(int id_x, int id_y, int it)
{
	return IDX_IT(id_x, id_y, it);
}

static inline int IDX_BASE(int id_x, int id_y)
{
	return id_y*ntiles+id_x;
}

void dpotrf_block_global(int id_x, double* out)
{
	char upper = 'U';
	int nfo;

	copy_block_from_global(out, id_x, id_x);
	dpotrf_(&upper, &T, out, &T, &nfo);
}

void dpotrf_block(double* in_out)
{
	char upper = 'U';
	int nfo;

	dpotrf_(&upper, &T, in_out, &T, &nfo);
}

void create_dpotrf_task(int id_x)
{
	double out[T*T];
	double in_dsyrk[T*T];

	debug_printf("Create dpotrf Task %d, %d\n", id_x, id_x);

	if(id_x == 0) {
		/* Left column: no inputs from dsyrk */
		#pragma omp task output(sdpotrf_ref[IDX_DPOTRF(0)] << out[T*T])
		{
			debug_printf("Dpotrf Task %d, %d\n", id_x, id_x);
			dpotrf_block_global(id_x, out);
		}
	} else {
		/* Other columns: Input from a dsyrk task on the left */
		#pragma omp task input(sdsyrk_ref[IDX_DSYRK(id_x, id_x, id_x-1)] >> in_dsyrk[T*T]) \
			output(sdpotrf_ref[IDX_DPOTRF(id_x)] << out[T*T])
		{
			debug_printf("Dpotrf Task %d, %d\n", id_x, id_x);
			memcpy(out, in_dsyrk, T*T*sizeof(double));
			dpotrf_block(out);
		}
	}
}

void create_dgemm_task(int id_x, int id_y, int it)
{
	double out[T*T];
	double inout[T*T];
	double in_top[T*T];
	double in_left[T*T];
	double in_self[T*T];

	debug_printf("Create dgemm Task %d, %d, it = %d\n", id_x, id_y, it);
	//if(id_x == 1) {
	if(it == 0) {
		#pragma omp task peek(sdtrsm_ref[IDX_DTRSM(0, id_y)] >> in_left[T*T]) \
			peek(sdtrsm_ref[IDX_DTRSM(0, id_x)] >> in_top[T*T]) \
			output(sdgemm_ref[IDX_DGEMM(id_x, id_y, 0)] << out[T*T])
		{
			debug_printf("dgemm Task %d, %d, it = %d\n", id_x, id_y, it);

			copy_block_from_global(out, id_x, id_y);
			cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasTrans,
				     T, T, T,
				     -1.0, in_left, T,
				     in_top, T,
				     1.0, out, T);
		}
	} else {
		#pragma omp task peek(sdtrsm_ref[IDX_DTRSM(it, id_y)] >> in_left[T*T]) \
			peek(sdtrsm_ref[IDX_DTRSM(it, id_x)] >> in_top[T*T]) \
			inout_reuse(sdgemm_ref[IDX_DGEMM(id_x, id_y, it-1)] >> inout[T*T] >> sdgemm_ref[IDX_DGEMM(id_x, id_y, it)])
		{
			debug_printf("dgemm Task %d, %d, it = %d\n", id_x, id_y, it);
			cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasTrans,
				     T, T, T,
				     -1.0, in_left, T,
				     in_top, T,
				     1.0, inout, T);
		}
	}
}

void create_dtrsm_task(int id_x, int id_y)
{
	double out[T*T];
	double in[T*T];
	double in_self[T*T];

	debug_printf("Create dtrsm Task %d, %d\n", id_x, id_y);

	if(id_x == 0) {
		#pragma omp task peek(sdpotrf_ref[IDX_DPOTRF(id_x)] >> in[T*T]) \
				output(sdtrsm_ref[IDX_DTRSM(id_x, id_y)] << out[T*T])
		{
			debug_printf("Dtrsm Task %d, %d\n", id_x, id_y);
			copy_block_from_global(out, id_x, id_y);

			cblas_dtrsm (CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
				     T, T,
				     1.0, in, T,
				     out, T);
		}
	} else {
		#pragma omp task peek(sdpotrf_ref[IDX_DPOTRF(id_x)] >> in[T*T]) \
				peek(sdgemm_ref[IDX_DGEMM(id_x, id_y, id_x-1)] >> in_self[T*T]) \
				output(sdtrsm_ref[IDX_DTRSM(id_x, id_y)] << out[T*T])
		{
			debug_printf("Dtrsm Task %d, %d\n", id_x, id_y);
			memcpy(out, in_self, T*T*sizeof(double));

			cblas_dtrsm (CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
				     T, T,
				     1.0, in, T,
				     out, T);
		}
	}
}

void create_dsyrk_task(int id_x, int id_y, int it)
{
	double in_dtrsm[T*T];
	double out[T*T];
	double inout[T*T];

	debug_printf("Create Dsyrk Task %d, %d, it = %d\n", id_x, id_y, it);

	if(it == 0) {
		#pragma omp task peek(sdtrsm_ref[IDX_DTRSM(it, id_y)] >> in_dtrsm[T*T]) \
			output(sdsyrk_ref[IDX_DSYRK(id_x, id_y, it)] << out[T*T])
		{
			debug_printf("Dsyrk Task %d, %d, it = %d\n", id_x, id_y, it);
			copy_block_from_global(out, id_x, id_y);
			cblas_dsyrk (CblasRowMajor, CblasLower, CblasNoTrans,
				     T, T,
				     -1.0, in_dtrsm, T,
				     1.0, out, T);
		}
	} else {
		#pragma omp task peek(sdtrsm_ref[IDX_DTRSM(it, id_y)] >> in_dtrsm[T*T]) \
			inout_reuse(sdsyrk_ref[IDX_DSYRK(id_x, id_y, it-1)] >> inout[T*T] >> sdsyrk_ref[IDX_DSYRK(id_x, id_y, it)])
		{
			debug_printf("Dsyrk Task %d, %d, it = %d\n", id_x, id_y, it);
			cblas_dsyrk (CblasRowMajor, CblasLower, CblasNoTrans,
				     T, T,
				     -1.0, in_dtrsm, T,
				     1.0, inout, T);
		}
	}
}

void create_terminal_task(int id_x, int id_y)
{
	double in[T*T];
	int token;

	if(id_x == id_y) {
		if(id_x == ntiles-1) {
			#pragma omp task peek(sdpotrf_ref[IDX_DPOTRF(id_x)] >> in[T*T]) \
				output(sfinal_ref[0] << token)
			{
				copy_block_to_global(in, id_x, id_y);
				debug_printf("Terminal Task %d, %d\n", id_x, id_y);
			}
		} else {
			#pragma omp task peek(sdpotrf_ref[IDX_DPOTRF(id_x)] >> in[T*T])
			{
				copy_block_to_global(in, id_x, id_y);
				debug_printf("Terminal Task %d, %d\n", id_x, id_y);
			}
		}
	} else {
		#pragma omp task peek(sdtrsm_ref[IDX_DTRSM(id_x, id_y)] >> in[T*T])
		{
			copy_block_to_global(in, id_x, id_y);
			debug_printf("Terminal Task %d, %d\n", id_x, id_y);
		}
	}
}

int main(int argc, char** argv)
{
	int option;
	int verify = 0;

	N = 1 << 13;
	T = 256;

	while ((option = getopt(argc, argv, "n:s:b:i:o:p:hv")) != -1)
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
				T = 1 << atoi (optarg);
				break;
			case 'p':
				padding = atoi(optarg);

				if(padding % sizeof(double)) {
					fprintf(stderr, "Padding must be a multiple of sizeof(double) (%lu).\n",
						sizeof(double));
					exit(1);
				}

				padding_elements = padding / sizeof(double);
				break;
			case 'v':
				verify = 1;
				break;
			case 'h':
				printf("Usage: %s [option]...\n\n"
				       "Options:\n"
				       "  -n <size>                    Number of colums of the square matrix, default is %d\n"
				       "  -s <power>                   Set the number of colums of the square matrix to 1 << <power>\n"
				       "  -b <block size power>        Set the block size 1 << <block size power>\n"
				       "  -v                           Verify result (compare with output of sequential call)\n"
				       "  -p <padding>                 Padding at the end of aline of the global matrix. Should be\n"
				       "                               equal to the size of one cache line, e.g. 64.\n",
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

	printf("N=%d, padding = %d, T = %d\n", N, padding, T);

	ntiles = N/T;

	double sfinal[1] __attribute__((stream));
	double screate[1] __attribute__((stream));
	double sdpotrf[ntiles*ntiles] __attribute__((stream));
	double sdsyrk[ntiles*ntiles*ntiles] __attribute__((stream));
	double sdtrsm[ntiles*ntiles] __attribute__((stream));
	double sdgemm[ntiles*ntiles*ntiles] __attribute__((stream));

	struct timeval start;
	struct timeval end;

	//global_matrix = malloc_interleaved(N*(N+padding_elements)*sizeof(double));
	posix_memalign_interleaved((void**)&global_matrix, 64, N*(N+padding_elements)*sizeof(double));

	if(!global_matrix) {
		fprintf(stderr, "Could not allocate matrix.\n");
		exit(1);
	}

	cholesky_init_random_matrix(global_matrix, N);
	//printf("Initial parallel matrix:\n");
	//dump_matrix_2d(global_matrix, stdout, N, N);
	//printf("\n\n");
	matrix_add_padding(global_matrix, N, padding_elements);

	sdpotrf_ref = malloc(ntiles*ntiles*sizeof(void*));
	screate_ref = malloc(1*sizeof(void*));
	sdsyrk_ref = malloc(ntiles*ntiles*ntiles*sizeof(void*));
	sdtrsm_ref = malloc(ntiles*ntiles*sizeof(void*));
	sdgemm_ref = malloc(ntiles*ntiles*ntiles*sizeof(void*));
	sfinal_ref = malloc(1*sizeof(void*));

	memcpy(sdpotrf_ref, sdpotrf, ntiles*ntiles*sizeof(void*));
	memcpy(screate_ref, screate, 1*sizeof(void*));
	memcpy(sdsyrk_ref, sdsyrk, ntiles*ntiles*ntiles*sizeof(void*));
	memcpy(sdtrsm_ref, sdtrsm, ntiles*ntiles*sizeof(void*));
	memcpy(sdgemm_ref, sdgemm, ntiles*ntiles*ntiles*sizeof(void*));
	memcpy(sfinal_ref, sfinal, 1*sizeof(void*));

	gettimeofday(&start, NULL);
	openstream_start_hardware_counters();

	int px = 4;
	int py = px;

	int ncreatetoken = (ntiles % px == 0) ? (ntiles / px) : (ntiles / px) + 1;
	int tk = 0;

	for(int id_x = 0; id_x < ntiles; id_x++) {
		for(int id_y = id_x; id_y < ntiles; id_y++) {
			#pragma omp task
			{
				if(id_x == id_y)
					create_dpotrf_task(id_x);

				create_terminal_task(id_x, id_y);

				if(id_x != id_y)
					create_dtrsm_task(id_x, id_y);

				if(id_x == id_y && id_x != 0)
					for(int it = 0; it < id_x; it++)
						create_dsyrk_task(id_x, id_y, it);

				if(id_x != id_y)
					for(int it = 0; it < id_x; it++)
						create_dgemm_task(id_x, id_y, it);
			}
		}
	}

	#pragma omp taskwait

	for(int id_xx = 0; id_xx < ntiles; id_xx += px) {
		for(int id_x = id_xx; id_x < ntiles && id_x < id_xx + px; id_x++) {
			#pragma omp task
			{
				#pragma omp tick(sdpotrf_ref[IDX_DPOTRF(id_x)] >> T*T)

				for(int id_y = id_x; id_y < ntiles; id_y++) {
					if(id_x != id_y) {
						#pragma omp tick(sdtrsm_ref[IDX_DTRSM(id_x, id_y)] >> T*T)
					}

					if(id_x != 0 && id_x != id_y) {
						#pragma omp tick(sdgemm_ref[IDX_DGEMM(id_x, id_y, id_x-1)] >> T*T)
					}
				}
			}
		}
	}


	/* #pragma omp task(screate_ref[0] >>  */

	#pragma omp taskwait

	int token;
	#pragma omp task input(sfinal_ref[0] >> token)
	{
		printf("All tasks finished\n");
	}

	#pragma omp taskwait

	openstream_pause_hardware_counters();
	gettimeofday(&end, NULL);

	printf("%.5f\n", tdiff(&end, &start));

	//matrix_strip_padding(global_matrix, N, padding_elements);
	//printf("Resulting parallel matrix:\n");
	//dump_matrix_2d(global_matrix, stdout, N, N);
	//printf("\n\n");
	//matrix_add_padding(global_matrix, N, padding_elements);

	if(verify) {
		double* seq_input_matrix = malloc(N*(N+padding_elements)*sizeof(double));

		if(!seq_input_matrix) {
			fprintf(stderr, "Could not allocate space for verification matrix.\n");
			exit(1);
		}

		cholesky_init_random_matrix(seq_input_matrix, N);
		matrix_add_padding(seq_input_matrix, N, padding_elements);

		if(!cholesky_verify(global_matrix, seq_input_matrix, N, padding_elements)) {
			fprintf(stderr, "Verification: Failed!\n");
			//exit(1);
		} else {
			printf("Verification: OK!\n");
		}

		//matrix_strip_padding(seq_input_matrix, N, padding_elements);
		//printf("Resulting sequential matrix:\n");
		//dump_matrix_2d(seq_input_matrix, stdout, N, N);
		//printf("\n\n");
		free(seq_input_matrix);
	}

	free(screate_ref);
	free(sdpotrf_ref);
	free(sdsyrk_ref);
	free(sdtrsm_ref);
	free(sdgemm_ref);
	free(sfinal_ref);
	free(global_matrix);

	return 0;
}
