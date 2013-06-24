#ifndef LAPACK_H
#define LAPACK_H

/* Missing declarations from liblapack */
int dlarnv_(long *idist, long *iseed, int *n, double *x);
void dpotrf_( unsigned char *uplo, int * n, double *a, int *lda, int *info );

#endif
