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
  #include <mkl.h>
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
#define debug_printf printf
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

void warmup_lapack(void)
{
	double* A = malloc(T*T*sizeof(double));
	double* B = malloc(T*T*sizeof(double));
	double* C = malloc(T*T*sizeof(double));

	cholesky_init_random_matrix(A, T);
	cholesky_init_random_matrix(B, T);
	cholesky_init_random_matrix(C, T);

	char upper = 'U';
	int nfo;

	for(int i = 0; i < 10; i++) {
		dpotrf_(&upper, &T, A, &T, &nfo);

		cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasTrans,
			     T, T, T,
			     -1.0, A, T,
			     B, T,
			     1.0, C, T);

		cblas_dtrsm (CblasRowMajor, CblasRight, CblasLower, CblasTrans, CblasNonUnit,
			     T, T,
			     1.0, A, T,
			     B, T);

		cblas_dsyrk (CblasRowMajor, CblasLower, CblasNoTrans,
			     T, T,
			     -1.0, A, T,
			     1.0, B, T);
	}

	free(A);
	free(B);
	free(C);
}


int cholesky_verify(double* matrix, double* seq_input_matrix, int N, int padding_elements)
{
	char upper = 'U';
	int Npad = N+padding_elements;
	int nfo;

	dpotrf_(&upper, &N, seq_input_matrix, &Npad, &nfo);

	for(int x = 0; x < N; x++) {
		for(int y = 0; y < N; y++) {
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
	double out_self[T*T];

	debug_printf("Create dgemm Task %d, %d, it = %d\n", id_x, id_y, it);

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
			input(sdgemm_ref[IDX_DGEMM(id_x, id_y, it-1)] >> in_self[T*T]) \
			output(sdgemm_ref[IDX_DGEMM(id_x, id_y, it)] << out_self[T*T])
		{
			debug_printf("dgemm Task %d, %d, it = %d\n", id_x, id_y, it);

			memcpy(out_self, in_self, T*T*sizeof(double));

			cblas_dgemm (CblasRowMajor, CblasNoTrans, CblasTrans,
				     T, T, T,
				     -1.0, in_left, T,
				     in_top, T,
				     1.0, out_self, T);
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
	double in_self[T*T];
	double out_self[T*T];

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
			input(sdsyrk_ref[IDX_DSYRK(id_x, id_y, it-1)] >> in_self[T*T]) \
			output(sdsyrk_ref[IDX_DSYRK(id_x, id_y, it)] << out_self[T*T])
		{
			debug_printf("Dsyrk Task %d, %d, it = %d\n", id_x, id_y, it);

			memcpy(out_self, in_self, T*T*sizeof(double));

			cblas_dsyrk (CblasRowMajor, CblasLower, CblasNoTrans,
				     T, T,
				     -1.0, in_dtrsm, T,
				     1.0, out_self, T);
		}
	}
}

void create_terminal_task(int id_x, int id_y)
{
	double in[T*T];
	int token;

	if(id_x == id_y) {
		#pragma omp task peek(sdpotrf_ref[IDX_DPOTRF(id_x)] >> in[T*T]) \
			output(sfinal_ref[0] << token)
		{
			copy_block_to_global(in, id_x, id_y);
			debug_printf("Terminal Task %d, %d\n", id_x, id_y);
		}
	} else {
		#pragma omp task peek(sdtrsm_ref[IDX_DTRSM(id_x, id_y)] >> in[T*T]) \
			output(sfinal_ref[0] << token)
		{
			copy_block_to_global(in, id_x, id_y);
			debug_printf("Terminal Task %d, %d\n", id_x, id_y);
		}
	}
}

enum task_type {
	TASK_TYPE_DPOTRF,
	TASK_TYPE_TERMINAL,
	TASK_TYPE_DTRSM,
	TASK_TYPE_DSYRK,
	TASK_TYPE_DGEMM
};

struct tdesc {
	enum task_type type;
	int id_x;
	int id_y;
	int it;
};

static inline void add_task(struct tdesc* tdesc, int* curr_idx, enum task_type type, int id_x, int id_y, int it)
{
	tdesc[*curr_idx].type = type;
	tdesc[*curr_idx].id_x = id_x;
	tdesc[*curr_idx].id_y = id_y;
	tdesc[*curr_idx].it = it;

	(*curr_idx)++;
}

static inline void create_task(struct tdesc* tdesc)
{
	switch(tdesc->type) {
		case TASK_TYPE_DPOTRF:
			create_dpotrf_task(tdesc->id_x);
			break;
		case TASK_TYPE_TERMINAL:
			create_terminal_task(tdesc->id_x, tdesc->id_y);
			break;
		case TASK_TYPE_DTRSM:
			create_dtrsm_task(tdesc->id_x, tdesc->id_y);
			break;
		case TASK_TYPE_DSYRK:
			create_dsyrk_task(tdesc->id_x, tdesc->id_y, tdesc->it);
			break;
		case TASK_TYPE_DGEMM:
			create_dgemm_task(tdesc->id_x, tdesc->id_y, tdesc->it);
			break;
	}
}

void create_tasks(struct tdesc* tdesc, int ntasks, int batch)
{
	if(ntasks > batch && ntasks > 1) {
		#pragma omp task
		{
			create_tasks(tdesc, ntasks / 2, batch);
		}

		#pragma omp task
		{
			create_tasks(&tdesc[ntasks / 2], ntasks - (ntasks / 2), batch);
		}
	} else {
		char tokens[ntasks];
		#pragma omp task output(screate_ref[0] << tokens[ntasks])
		{
			for(int i = 0; i < ntasks; i++)
				create_task(&tdesc[i]);
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

	ntiles = N/T;

	double sfinal[1] __attribute__((stream));
	int screate[1] __attribute__((stream));
	double sdpotrf[ntiles*ntiles] __attribute__((stream));
	double sdsyrk[ntiles*ntiles*ntiles] __attribute__((stream));
	double sdtrsm[ntiles*ntiles] __attribute__((stream));
	double sdgemm[ntiles*ntiles*ntiles] __attribute__((stream));

	struct timeval start;
	struct timeval end;

	posix_memalign_interleaved((void**)&global_matrix, 64, N*(N+padding_elements)*sizeof(double));

	if(!global_matrix) {
		fprintf(stderr, "Could not allocate matrix.\n");
		exit(1);
	}

	cholesky_init_random_matrix(global_matrix, N);

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

	int token[ntiles*(ntiles+1)/2];
	int ntasks = 0;
	int nntasks = 0;

	int idx = 0;

	for(int id_x = ntiles-1; id_x >= 0; id_x--) {
		for(int id_y = id_x; id_y < ntiles; id_y++) {
			if(id_x == id_y)
				nntasks++;

			nntasks++;

			if(id_x != id_y)
				nntasks++;

			if(id_x == id_y && id_x != 0)
				for(int it = 0; it < id_x; it++)
					nntasks++;

			if(id_x != id_y)
				for(int it = 0; it < id_x; it++)
					nntasks++;
		}
	}

	struct tdesc* tdesc = malloc(nntasks*sizeof(struct tdesc));

	for(int id_x = ntiles-1; id_x >= 0; id_x--) {
		for(int id_y = id_x; id_y < ntiles; id_y++) {
			if(id_x == id_y)
				add_task(tdesc, &ntasks, TASK_TYPE_DPOTRF, id_x, id_y, 0);

			add_task(tdesc, &ntasks, TASK_TYPE_TERMINAL, id_x, id_y, 0);

			if(id_x != id_y)
				add_task(tdesc, &ntasks, TASK_TYPE_DTRSM, id_x, id_y, 0);

			if(id_x == id_y && id_x != 0)
				for(int it = 0; it < id_x; it++)
					add_task(tdesc, &ntasks, TASK_TYPE_DSYRK, id_x, id_y, it);

			if(id_x != id_y)
				for(int it = 0; it < id_x; it++)
					add_task(tdesc, &ntasks, TASK_TYPE_DGEMM, id_x, id_y, it);
		}
	}

	char tokens[ntasks];
	#pragma omp task input(screate_ref[0] >> tokens[ntasks])
	{
		debug_printf("ALL tasks created\n");
	}

	create_tasks(tdesc, ntasks, ntasks/(2*192));

	#pragma omp taskwait

	free(tdesc);

	for(int id_y = 0; id_y < ntiles; id_y++) {
		#pragma omp task
		{
			for(int id_x = 0; id_x <= id_y; id_x++) {
				if(id_x == id_y) {
					#pragma omp tick(sdpotrf_ref[IDX_DPOTRF(id_x)] >> T*T)
				}

				if(id_x != id_y) {
					#pragma omp tick(sdtrsm_ref[IDX_DTRSM(id_x, id_y)] >> T*T)
				}

				if(id_x != 0 && id_x != id_y) {
					#pragma omp tick(sdgemm_ref[IDX_DGEMM(id_x, id_y, id_x-1)] >> T*T)
				}
			}
		}
	}

	#pragma omp task input(sfinal_ref[0] >> token[ntiles*(ntiles+1)/2])
	{
		printf("All tasks finished\n");
	}

	#pragma omp taskwait

	openstream_pause_hardware_counters();
	gettimeofday(&end, NULL);

	printf("%.5f\n", tdiff(&end, &start));


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
			exit(1);
		} else {
			printf("Verification: OK!\n");
		}

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
