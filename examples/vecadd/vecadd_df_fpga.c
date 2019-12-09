#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define N 10

int main()
{
  int x __attribute__((stream)), y __attribute__((stream)), z __attribute__((stream));
  int vx[N], vy[N], vz[N];

#pragma omp task output(y << vy[N])
  {
    for (int i = 0; i < N; i++)
      vy[i] = i;
    printf("first\n");
  }

#pragma omp task output(z << vz[N])
  {
    for (int i = 0; i < N; i++)
      vz[i] = i;
    printf("second\n");
  }

#pragma omp task output(x << vx[N]) \
                 input(y >> vy[N], z >> vz[N]) \
                 accel_name(vecadd) \
                 args(vy, vx) \
                 dimensions(1) \
                 work_size(N)
  {
    printf("CPU execution\n");
    for (int i = 0; i < N; i++)
      vx[i] = vy[i] + vz[i];
  }

#pragma omp task input(x >> vx[N])
  {
    for (int i = 0; i < N; i++)
      printf("vx[%d] = %d\n", i, vx[i]);
    fflush(stderr);
  }

#pragma omp taskwait

  return 0;
}
