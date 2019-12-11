#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define N 256

int main()
{
  int x __attribute__((stream)), y __attribute__((stream)), z __attribute__((stream));
  int vx[N], vy[N], vz[N];
  int n = 256;

#pragma omp task output(x << vx[N])
  {
    for (int i = 0; i < N; i++)
      vx[i] = i;
  }

#pragma omp task output(y << vy[N])
  {
    for (int i = 0; i < N; i++)
      vy[i] = i;
  }

#pragma omp task output(z << vz[N]) \
                 input(x >> vx[N], y >> vy[N]) \
                 accel_name(Partial_vec_add) \
                 args(vz, vx, vy, n)
  {
    for (int i = 0; i < N; i++)
      vz[i] = vx[i] + vy[i];
  }

#pragma omp task input(z >> vz[N])
  {
    for (int i = 0; i < N; i++)
      printf("vz[%d] = %d\n", i, vz[i]);
    fflush(stderr);
  }

#pragma omp taskwait

  return 0;
}
