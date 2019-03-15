/*
  A simple 2D hydro code
  (C) Romain Teyssier : CEA/IRFU           -- original F90 code
  (C) Pierre-Francois Lavallee : IDRIS      -- original F90 code
  (C) Guillaume Colin de Verdiere : CEA/DAM -- for the C version
  (C) Adele Villiermet : CINES            -- for FTI integration
*/
/*

  This software is governed by the CeCILL license under French law and
  abiding by the rules of distribution of free software.  You can  use, 
  modify and/ or redistribute the software under the terms of the CeCILL
  license as circulated by CEA, CNRS and INRIA at the following URL
  "http://www.cecill.info". 

  As a counterpart to the access to the source code and  rights to copy,
  modify and redistribute granted by the license, users are provided only
  with a limited warranty  and the software's author,  the holder of the
  economic rights,  and the successive licensors  have only  limited
  liability. 

  In this respect, the user's attention is drawn to the risks associated
  with loading,  using,  modifying and/or developing or reproducing the
  software by the user in light of its specific status of free software,
  that may mean  that it is complicated to manipulate,  and  that  also
  therefore means  that it is reserved for developers  and  experienced
  professionals having in-depth computer knowledge. Users are therefore
  encouraged to load and test the software's suitability as regards their
  requirements in conditions enabling the security of their systems and/or 
  data to be ensured and,  more generally, to use and operate it in the 
  same conditions as regards security. 

  The fact that you are presently reading this means that you have had
  knowledge of the CeCILL license and that you accept its terms.

*/
#ifdef MPI
#include <mpi.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
//#ifdef _OPENMP
//#include <omp.h>
//#endif

#include "parametres.h"
#include "hydro_funcs.h"
#include "vtkfile.h"
#include "compute_deltat.h"
#include "hydro_godunov.h"
#include "perfcnt.h"
#include "cclock.h"
#include "utils.h"

double functim[TIM_END];

int sizeLabel(double *tim, const int N) {
  double maxi = 0;
  int i;

  for (i = 0; i < N; i++) 
    if (maxi < tim[i]) maxi = tim[i];

  // if (maxi < 100) return 8;
  // if (maxi < 1000) return 9;
  // if (maxi < 10000) return 10;
  return 9;
}
void percentTimings(double *tim, const int N)
{
  double sum = 0;
  int i;

  for (i = 0; i < N; i++) 
    sum += tim[i];

  for (i = 0; i < N; i++)
    tim[i] = 100.0 * tim[i] / sum;
}

void avgTimings(double *tim, const int N, const int nbr)
{
  int i;

  for (i = 0; i < N; i++)
    tim[i] = tim[i] / nbr;
}

void printTimings(double *tim, const int N, const int sizeFmt)
{
  double sum = 0;
  int i;
  char fmt[256];

  sprintf(fmt, "%%-%d.4lf ", sizeFmt);

  for (i = 0; i < N; i++) 
    fprintf(stdout, fmt, tim[i]);
}
void printTimingsLabel(const int N, const int fmtSize)
{
  int i;
  char *txt;
  char fmt[256];

  sprintf(fmt, "%%-%ds ", fmtSize);
  for (i = 0; i < N; i++) {
    switch(i) {
    case TIM_COMPDT: txt = "COMPDT"; break;
    case TIM_MAKBOU: txt = "MAKBOU"; break;
    case TIM_GATCON: txt = "GATCON"; break;
    case TIM_CONPRI: txt = "CONPRI"; break;
    case TIM_EOS: txt = "EOS"; break;
    case TIM_SLOPE: txt = "SLOPE"; break;
    case TIM_TRACE: txt = "TRACE"; break;
    case TIM_QLEFTR: txt = "QLEFTR"; break;
    case TIM_RIEMAN: txt = "RIEMAN"; break;
    case TIM_CMPFLX: txt = "CMPFLX"; break;
    case TIM_UPDCON: txt = "UPDCON"; break;
    case TIM_ALLRED: txt = "ALLRED"; break;
    default:;
    }
    fprintf(stdout, fmt, txt);
  }
}


int
main(int argc, char **argv) {

  hydroparam_t _init_H, *H; H = &_init_H;
  _state_t SStream __attribute__((stream));
  _state_t SStream_in, SStream_out;
  real_t _dt_stream;
  // array of timers to profile the code
  memset(functim, 0, TIM_END * sizeof(functim[0]));

#ifdef MPI
  MPI_Init(&argc, &argv);
#endif

  process_args(argc, argv, H);

  for (int i = 0; i < H->nproc; ++i)
    {
#pragma omp task output(SStream) firstprivate (H)
      {
	memcpy (&SStream.H_, H, sizeof (hydroparam_t));
	setup_subsurface (i, &SStream.H_);
	hydro_init(&SStream.H_, &SStream.Hv_);

	if (SStream.H_.mype == 0)
	  fprintf(stdout, "Hydro starts in %s precision.\n", ((sizeof(real_t) == sizeof(double))? "double": "single"));
	gethostname(SStream.myhost, 255);
	if (SStream.H_.mype == 0) {
	  fprintf(stdout, "Hydro: Main process running on %s\n", SStream.myhost);
	}
	// PRINTUOLD(H, &SStream.Hv_);
      }

#pragma omp task input (SStream >> SStream_in) output (SStream << SStream_out) //output (_dt_stream)
      {
	if (SStream_in.H_.dtoutput > 0) {
	  // outputs are in physical time not in time steps
	  SStream_in.time_output = 1;
	  SStream_in.next_output_time = SStream_in.next_output_time + SStream_in.H_.dtoutput;
	}

	if (SStream_in.H_.dtoutput > 0 || SStream_in.H_.noutput > 0)
	  vtkfile(++SStream_in.nvtk, SStream_in.H_, &SStream_in.Hv_);

	if (SStream_in.H_.mype == 0)
	  fprintf(stdout, "Hydro starts main loop.\n");

	//pre-allocate memory before entering in loop
	//For godunov scheme
	//SStream_in.start = cclock();
	SStream_in.start = cclock();
	allocate_work_space(SStream_in.H_.nxyt, SStream_in.H_, &SStream_in.Hw_godunov, &SStream_in.Hvw_godunov);
	compute_deltat_init_mem(SStream_in.H_, &SStream_in.Hw_deltat, &SStream_in.Hvw_deltat);
	SStream_in.end = cclock();

	if (SStream_in.H_.mype == 0) fprintf(stdout, "Hydro: init mem %lfs\n", ccelaps(SStream_in.start, SStream_in.end));
	// we start timings here to avoid the cost of initial memory allocation
	SStream_in.start_time = dcclock();

	while ((SStream_in.H_.t < SStream_in.H_.tend) && (SStream_in.H_.nstep < SStream_in.H_.nstepmax)) {
	  //system("top -b -n1");
	  // reset perf counter for this iteration
	  flopsAri = flopsSqr = flopsMin = flopsTra = 0;
	  SStream_in.start_iter = dcclock();
	  SStream_in.outnum[0] = 0;
	  if ((SStream_in.H_.nstep % 2) == 0) {
	    SStream_in.dt = 0;
	    // if (H.mype == 0) fprintf(stdout, "Hydro computes deltat.\n");
	    SStream_in.start = cclock();
	    compute_deltat(&SStream_in.dt, SStream_in.H_, &SStream_in.Hw_deltat, &SStream_in.Hv_, &SStream_in.Hvw_deltat);
	    SStream_in.end = cclock();
	    functim[TIM_COMPDT] += ccelaps(SStream_in.start, SStream_in.end);
	    if (SStream_in.H_.nstep == 0) {
	      SStream_in.dt = SStream_in.dt / 2.0;
	      if (SStream_in.H_.mype == 0) fprintf(stdout, "Hydro computes initial deltat: %le\n", SStream_in.dt);
	    }
	    ////////_dt_stream = SStream_in.dt;
#ifdef MPI
	    if (SStream_in.H_.nproc > 1) {
	      real_t dtmin;
	      // printf("pe=%4d\tdt=%lg\n",SStream_in.H_.mype, SStream_in.dt);
	      if (sizeof(real_t) == sizeof(double)) {
		MPI_Allreduce(&SStream_in.dt, &dtmin, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
	      } else {
		MPI_Allreduce(&SStream_in.dt, &dtmin, 1, MPI_FLOAT, MPI_MIN, MPI_COMM_WORLD);
	      }
	      SStream_in.dt = dtmin;
	    }
#endif
	  }
	  // SStream_in.dt = 1.e-3;
	  // if (SStream_in.H_.mype == 1) fprintf(stdout, "Hydro starts godunov.\n");
	  if ((SStream_in.H_.nstep % 2) == 0) {
	    hydro_godunov(1, SStream_in.dt, SStream_in.H_, &SStream_in.Hv_, &SStream_in.Hw_godunov, &SStream_in.Hvw_godunov);
	    //            hydro_godunov(2, SStream_in.dt, SStream_in.H_, &SStream_in.Hv_, &SStream_in.Hw, &SStream_in.Hvw);
	  } else {
	    hydro_godunov(2, SStream_in.dt, SStream_in.H_, &SStream_in.Hv_, &SStream_in.Hw_godunov, &SStream_in.Hvw_godunov);
	    //            hydro_godunov(1, SStream_in.dt, SStream_in.H_, &SStream_in.Hv_, &SStream_in.Hw, &SStream_in.Hvw);
	  }
	  SStream_in.end_iter = dcclock();
	  SStream_in.cellPerCycle = (double) (SStream_in.H_.globnx * SStream_in.H_.globny) / (SStream_in.end_iter - SStream_in.start_iter) / 1000000.0L;
	  SStream_in.avgCellPerCycle += SStream_in.cellPerCycle;
	  SStream_in.nbCycle++;

	  SStream_in.H_.nstep++;
	  SStream_in.H_.t += SStream_in.dt;
	  {
	    real_t iter_time = (real_t) (SStream_in.end_iter - SStream_in.start_iter);
#ifdef MPI
	    long flopsAri_t, flopsSqr_t, flopsMin_t, flopsTra_t;
	    SStream_in.start = cclock();
	    MPI_Allreduce(&flopsAri, &flopsAri_t, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
	    MPI_Allreduce(&flopsSqr, &flopsSqr_t, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
	    MPI_Allreduce(&flopsMin, &flopsMin_t, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
	    MPI_Allreduce(&flopsTra, &flopsTra_t, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
	    //       if (SStream_in.H_.mype == 1)
	    //        printf("%ld %ld %ld %ld %ld %ld %ld %ld \n", flopsAri, flopsSqr, flopsMin, flopsTra, flopsAri_t, flopsSqr_t, flopsMin_t, flopsTra_t);
	    SStream_in.flops = flopsAri_t * FLOPSARI + flopsSqr_t * FLOPSSQR + flopsMin_t * FLOPSMIN + flopsTra_t * FLOPSTRA;
	    SStream_in.end = cclock();
	    functim[TIM_ALLRED] += ccelaps(SStream_in.start, SStream_in.end);
#else
	    SStream_in.flops = flopsAri * FLOPSARI + flopsSqr * FLOPSSQR + flopsMin * FLOPSMIN + flopsTra * FLOPSTRA;
#endif
	    nbFLOPS++;

	    if (SStream_in.flops > 0) {
	      if (iter_time > 1.e-9) {
		double mflops = (double) SStream_in.flops / (double) 1.e+6 / iter_time;
		MflopsSUM += mflops;
		sprintf(SStream_in.outnum, "%s {%.2f Mflops %ld Ops} (%.3fs)", SStream_in.outnum, mflops, SStream_in.flops, iter_time);
	      }
	    } else {
	      sprintf(SStream_in.outnum, "%s (%.3fs)", SStream_in.outnum, iter_time);
	    }
	  }
	  if (SStream_in.time_output == 0 && SStream_in.H_.noutput > 0) {
	    if ((SStream_in.H_.nstep % SStream_in.H_.noutput) == 0) {
	      vtkfile(++SStream_in.nvtk, SStream_in.H_, &SStream_in.Hv_);
	      sprintf(SStream_in.outnum, "%s [%04d]", SStream_in.outnum, SStream_in.nvtk);
	    }
	  } else {
	    if (SStream_in.time_output == 1 && SStream_in.H_.t >= SStream_in.next_output_time) {
	      vtkfile(++SStream_in.nvtk, SStream_in.H_, &SStream_in.Hv_);
	      SStream_in.next_output_time = SStream_in.next_output_time + SStream_in.H_.dtoutput;
	      sprintf(SStream_in.outnum, "%s [%04d]", SStream_in.outnum, SStream_in.nvtk);
	    }
	  }
	  if (SStream_in.H_.mype == 0) {
	    fprintf(stdout, "--> step=%4d, %12.5e, %10.5e %.3lf MC/s%s\n", SStream_in.H_.nstep, SStream_in.H_.t, SStream_in.dt, SStream_in.cellPerCycle, SStream_in.outnum);
	    fflush(stdout);
	  }
	} // while
	memcpy (&SStream_out, &SStream_in, sizeof (_state_t));
    }

	// Term
#pragma omp task input (SStream >> SStream_in) output (SStream << SStream_out) //output (_dt_stream)
      {
	SStream_in.end_time = dcclock();

	// Deallocate work spaces
	deallocate_work_space(SStream_in.H_.nxyt, SStream_in.H_, &SStream_in.Hw_godunov, &SStream_in.Hvw_godunov);
	compute_deltat_clean_mem(SStream_in.H_, &SStream_in.Hw_deltat, &SStream_in.Hvw_deltat);

	hydro_finish(SStream_in.H_, &SStream_in.Hv_);
	SStream_in.elaps = (double) (SStream_in.end_time - SStream_in.start_time);
	timeToString(SStream_in.outnum, SStream_in.elaps);
	if (SStream_in.H_.mype == 0) {
	  fprintf(stdout, "Hydro ends in %ss (%.3lf) <%.2lf MFlops>.\n", SStream_in.outnum, SStream_in.elaps, (float) (MflopsSUM / nbFLOPS));
	  fprintf(stdout, "       ");
	}
	if (SStream_in.H_.nproc == 1) {
	  int sizeFmt = sizeLabel(functim, TIM_END);
	  printTimingsLabel(TIM_END, sizeFmt);
	  fprintf(stdout, "\n");
	  if (sizeof(real_t) == sizeof(double)) {
	    fprintf(stdout, "PE0_DP ");
	  } else {
	    fprintf(stdout, "PE0_SP ");
	  }
	  printTimings(functim, TIM_END, sizeFmt);
	  fprintf(stdout, "\n");
	  fprintf(stdout, "%%      ");
	  percentTimings(functim, TIM_END);
	  printTimings(functim, TIM_END, sizeFmt);
	  fprintf(stdout, "\n");
	}
#ifdef MPI
	if (SStream_in.H_.nproc > 1) {
	  double timMAX[TIM_END];
	  double timMIN[TIM_END];
	  double timSUM[TIM_END];
	  MPI_Allreduce(functim, timMAX, TIM_END, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
	  MPI_Allreduce(functim, timMIN, TIM_END, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
	  MPI_Allreduce(functim, timSUM, TIM_END, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

	  if (SStream_in.H_.mype == 0) {
	    int sizeFmt = sizeLabel(timMAX, TIM_END);
	    printTimingsLabel(TIM_END, sizeFmt);
	    fprintf(stdout, "\n");
	    fprintf(stdout, "MIN ");
	    printTimings(timMIN, TIM_END, sizeFmt);
	    fprintf(stdout, "\n");
	    fprintf(stdout, "MAX ");
	    printTimings(timMAX, TIM_END, sizeFmt);
	    fprintf(stdout, "\n");
	    fprintf(stdout, "AVG ");
	    avgTimings(timSUM, TIM_END, SStream_in.H_.nproc);
	    printTimings(timSUM, TIM_END, sizeFmt);
	    fprintf(stdout, "\n");
	  }
	}
#endif
	if (SStream_in.H_.mype == 0) {
	  fprintf(stdout, "Average MC/s: %.3lf\n", (double)(SStream_in.avgCellPerCycle / SStream_in.nbCycle));
	}
	memcpy (&SStream_out, &SStream_in, sizeof (_state_t));
      }
    }

  for (int i = 0; i < H->nproc; ++i)
    {
#pragma omp tick (SStream >> 1)
    }

#pragma omp taskwait

#ifdef MPI
  MPI_Finalize();
#endif
  return 0;
}
