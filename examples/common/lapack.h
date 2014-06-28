#ifndef LAPACK_H
#define LAPACK_H

/* Missing declarations from liblapack */
int dlarnv_(int *idist, int *iseed, int *n, double *x);
void dpotrf_( char *uplo, int * n, double *a, int *lda, int *info );

#endif
