#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

static inline off_t file_size(const char* filename)
{
	struct stat stat_buf;

	if(stat(filename, &stat_buf) == -1)
		return -1;

	return stat_buf.st_size;
}

int main(int argc, char** argv)
{
	double delta = 0.001;
	double threshold = 0.0;
	const char* reference_file = NULL;
	const char* compare_file = NULL;
	int verbose = 0;
	char option;
	int err = 1;
	int dontstop = 0;

	void* data_ref;
	void* data_cmp;

	int* n_ref;
	int* n_cmp;

	double* matrix_ref;
	double* matrix_cmp;

	off_t size_ref;
	off_t size_cmp;

	int fd_ref;
	int fd_cmp;

	while ((option = getopt(argc, argv, "r:c:d:t:hkv")) != -1)
	{
		switch(option)
		{
			case 'r':
				reference_file = optarg;
				break;
			case 'c':
				compare_file = optarg;
				break;
			case 'd':
				delta = strtod(optarg, NULL);
				break;
			case 't':
				threshold = strtod(optarg, NULL);
				break;
			case 'v':
				verbose = 1;
				break;
			case 'k':
				dontstop = 1;
				break;
			case 'h':
				printf("Usage: %s [option]...\n\n"
				       "Options:\n"
				       "  -r <filename>                Name of the file containing the reference data\n"
				       "  -c <filename>                Name of the file containing data to be checked\n"
				       "  -d <relative difference>     Maximum relative difference, e.g. 0.01 means that |(d-d')|/d must be < 0.01\n"
				       "  -t <threshold>               Absolute minimal value to be considered\n"
				       "  -v                           Turns on verbose mode\n"
				       "  -k                           Don't stop at first difference\n"
				       "  -h                           Show this help\n",
				       argv[0]);
				exit(0);
				break;
			case '?':
				fprintf(stderr, "Run %s -h for usage.\n", argv[0]);
				exit(1);
				break;
		}
	}

	if(!reference_file) {
		fprintf(stderr, "No reference file specified.\n");
		goto out_err;
	}

	if(!compare_file) {
		fprintf(stderr, "No file specified.\n");
		goto out_err;
	}

	if((size_ref = file_size(reference_file)) == -1) {
		fprintf(stderr, "Cannot determine size of file %s.\n", reference_file);
		goto out_err;
	}

	if((size_cmp = file_size(compare_file)) == -1) {
		fprintf(stderr, "Cannot determine size of file %s.\n", compare_file);
		goto out_err;
	}

	if(size_cmp != size_ref) {
		fprintf(stderr, "File size different from size of the reference file.\n");
		goto out_err;
	}

	if(!(fd_ref = open(reference_file, O_RDONLY))) {
		fprintf(stderr, "Cannot open file %s.\n", reference_file);
		goto out_err;
	}

	if(!(fd_cmp = open(compare_file, O_RDONLY))) {
		fprintf(stderr, "Cannot open file %s.\n", compare_file);
		goto out_err_fd_ref;
	}

	if((data_ref = mmap(NULL, size_ref, PROT_READ, MAP_SHARED, fd_ref, 0)) == MAP_FAILED) {
		fprintf(stderr, "Cannot mmap file %s: %s.\n", reference_file, strerror(errno));
		goto out_err_fd;
	}

	if((data_cmp = mmap(NULL, size_cmp, PROT_READ, MAP_SHARED, fd_cmp, 0)) == MAP_FAILED) {
		fprintf(stderr, "Cannot mmap file %s: %s.\n", compare_file, strerror(errno));
		goto out_err_mmap_ref;
	}

	if(verbose)
		printf("Comparing %s to %s (reference), relative delta = %lf\n", reference_file, compare_file, delta);

	n_ref = data_ref;
	n_cmp = data_cmp;

	if(*n_cmp != *n_ref) {
		fprintf(stderr, "Matrix dimensions differ (%d != %d).\n", *n_ref, *n_cmp);
		goto out_err_mmap_ref;
	}

	int num_elements = (*n_ref) * (*n_ref);
	matrix_ref = (double*)(((char*)data_ref)+sizeof(int));
	matrix_cmp = (double*)(((char*)data_cmp)+sizeof(int));

	for(int i = 0; i < num_elements; i++) {
		double absdiff = fabs(matrix_ref[i] - matrix_cmp[i]);
		double absref = fabs(matrix_ref[i]);
		double abscmp = fabs(matrix_cmp[i]);

		if((absref > threshold || abscmp > threshold) && ((absref == 0.0 && absdiff > 0.0) || (absdiff / absref) > delta)) {
			fprintf(stderr,
				"Difference at element %d, %d: expected %.30f, but is %.30f (diff = %.30f)\n",
				i/(*n_ref), i%(*n_ref), matrix_ref[i], matrix_cmp[i], absdiff);

			if(!dontstop)
				goto out_err_mmap;
		}
	}

	if(verbose)
		printf("%d values compared\n", num_elements);

	err = 0;

out_err_mmap:
	munmap(data_cmp, size_cmp);
out_err_mmap_ref:
	munmap(data_ref, size_ref);
out_err_fd:
	close(fd_cmp);
out_err_fd_ref:
	close(fd_ref);
out_err:
	return err;
}
