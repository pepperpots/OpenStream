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

void *_dt_stream_ref;
void *_current_t_ref;
void *_current_nstep_ref;
void *SStream_ref;
void *DStream_ref;
void *term_ref;


void debug_this ()
{
  volatile int i = 0;
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(10);
}

void create_next_rounds (int, real_t, int, size_t);

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


void
partition_and_initialize_data_stream (hydroparam_t *_init_H, int i, size_t data_size)
{
  hydroparam_t *H = _init_H;
  _state_t SStream_out;
  char data_block[data_size];

  //fprintf(stderr, " >> partition task creation %d\n", i);

#pragma omp task output (SStream_ref << SStream_out) output (DStream_ref << data_block[data_size]) \
  firstprivate (H) proc_bind (master) firstprivate (data_size)
  {
    memset (&SStream_out, 0, sizeof (_state_t));
    memset (data_block, 0, data_size);

    memcpy (&SStream_out.H_, H, sizeof (hydroparam_t));

    //fprintf(stderr, " >> partition task exec %d %d/%d\n", data_size, SStream_out.H_.mype, SStream_out.H_.nproc); fflush (stderr);
    //fprintf(stderr, "H1 - globnx %d\t globny %d\t nx %d\t ny %d\n", SStream_out.H_.globnx, SStream_out.H_.globny, SStream_out.H_.nx, SStream_out.H_.ny);

    setup_subsurface (i, &SStream_out.H_);
    reset_pointers_to_state_structures (data_block, &SStream_out);
    hydro_init(&SStream_out.H_, &SStream_out.Hv_);
    //if (SStream_out.H_.mype == 1)
    //fprintf(stderr, "H->box %d\t  %d\t  %d\t  %d\n", SStream_out.H_.box[0], SStream_out.H_.box[1], SStream_out.H_.box[2], SStream_out.H_.box[3]);

    if (SStream_out.H_.mype == 0)
      fprintf(stdout, "Hydro starts in %s precision.\n", ((sizeof(real_t) == sizeof(double))? "double": "single"));
    gethostname(SStream_out.myhost, 255);
    if (SStream_out.H_.mype == 0) {
      fprintf(stdout, "Hydro: Main process running on %s\n", SStream_out.myhost);
    }
    // PRINTUOLD(H, &SStream_out.Hv_);

    if (SStream_out.H_.dtoutput > 0) {
      // outputs are in physical time not in time steps
      SStream_out.time_output = 1;
      SStream_out.next_output_time = SStream_out.next_output_time + SStream_out.H_.dtoutput;
    }

    if (SStream_out.H_.dtoutput > 0 || SStream_out.H_.noutput > 0)
      vtkfile(++SStream_out.nvtk, SStream_out.H_, &SStream_out.Hv_);

    if (SStream_out.H_.mype == 0)
      fprintf(stdout, "Hydro starts main loop.\n");

    //fprintf(stderr, " >>> partition task exec %d %d/%d\n", data_size, SStream_out.H_.mype, SStream_out.H_.nproc); fflush (stderr);

    //pre-allocate memory before entering in loop
    //For godunov scheme
    //SStream_out.start = cclock();
    //allocate_work_space(SStream_out.H_.nxyt, SStream_out.H_, &SStream_out.Hw_godunov, &SStream_out.Hvw_godunov);
    //compute_deltat_init_mem(SStream_out.H_, &SStream_out.Hw_deltat, &SStream_out.Hvw_deltat);
    //SStream_out.end = cclock();

    //if (SStream_out.H_.mype == 0) fprintf(stdout, "Hydro: init mem %lfs\n", ccelaps(SStream_out.start, SStream_out.end));
    // we start timings here to avoid the cost of initial memory allocation
  }
}

void
create_first_tasks (size_t data_size)
{
  real_t _dt_stream_view;
  real_t _current_t_view;
  int _current_nstep_view;
  _state_t SStream_in, SStream_out;
  char data_block_in[data_size];
  char data_block_out[data_size];

  //fprintf(stderr, " >> first tasks creation\n");

#pragma omp task input (SStream_ref >> SStream_in) output (SStream_ref << SStream_out) \
  input (DStream_ref >> data_block_in[data_size]) output (DStream_ref << data_block_out[data_size]) \
  output (_dt_stream_ref << _dt_stream_view, _current_t_ref << _current_t_view, _current_nstep_ref << _current_nstep_view) \
  proc_bind ( master ) firstprivate (data_size)
  {
    //fprintf(stderr, " \t [FIRST_task]: exec %d %d/%d\n", data_size, SStream_in.H_.mype, SStream_in.H_.nproc); fflush (stderr);
    reset_pointers_to_state_structures (data_block_in, &SStream_in);
    SStream_in.start_time = dcclock();
    if ((SStream_in.H_.t < SStream_in.H_.tend) && (SStream_in.H_.nstep < SStream_in.H_.nstepmax)) {
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
	  //if (SStream_in.H_.mype >= 0) fprintf(stderr, "Hydro PE %d computes initial deltat: %le\n", SStream_in.H_.mype, SStream_in.dt); fflush (stderr);
	}
      }
    }
    memcpy (&SStream_out, &SStream_in, sizeof (_state_t));
    memcpy (data_block_out, data_block_in, data_size);
    _dt_stream_view = SStream_in.dt;
    _current_t_view = SStream_in.H_.t;
    _current_nstep_view = SStream_in.H_.nstep;
  }
}


void
create_first_forward_tasks (size_t data_size)
{
  _state_t SStream_in, SStream_out;
  char data_block_in[data_size];
  char data_block_out[data_size];
#pragma omp task input (SStream_ref >> SStream_in) output (SStream_ref << SStream_out) \
  input (DStream_ref >> data_block_in[data_size]) output (DStream_ref << data_block_out[data_size]) \
  proc_bind ( master ) firstprivate (data_size)
  {
    //fprintf(stderr, " \t [FIRST forward task]: exec %d %d/%d\n", data_size, SStream_in.H_.mype, SStream_in.H_.nproc);
    //fprintf(stderr, " \t [FIRST forward task]: exec\n");
    memcpy (&SStream_out, &SStream_in, sizeof (_state_t));
    memcpy (data_block_out, data_block_in, data_size);
  }
}

void
create_work_tasks (real_t temp_min, size_t data_size)
{
  real_t _dt_stream_view;
  real_t _current_t_view;
  int _current_nstep_view;
  _state_t SStream_in, SStream_out;
  char data_block_in[data_size];
  char data_block_out[data_size];

  //fprintf(stderr, " \t [WORK_task]: create\n");

#pragma omp task input (SStream_ref >> SStream_in) output (SStream_ref << SStream_out) \
  input (DStream_ref >> data_block_in[data_size]) output (DStream_ref << data_block_out[data_size]) \
  output (_dt_stream_ref << _dt_stream_view, _current_t_ref << _current_t_view, _current_nstep_ref << _current_nstep_view) \
  proc_bind ( master ) firstprivate (data_size) firstprivate (temp_min)
  {
    //fprintf(stderr, " \t [Work task]: exec %d %d/%d\n", data_size, SStream_in.H_.mype, SStream_in.H_.nproc);
    reset_pointers_to_state_structures (data_block_in, &SStream_in);

    SStream_in.dt = temp_min;

    int idimStart = (SStream_in.H_.nstep % 2) == 0 ? 1 : 2;
    for (int idimIndex = 0; idimIndex < 2; idimIndex++) {
      int idim = (idimStart - 1 + idimIndex) % 2 + 1;

      // Update boundary conditions
      //if (SStream_in.H_.prt) {
      //fprintf(fic, "godunov %d %le %le\n", idim, dt, SStream_in.H_.t);
      //PRINTUOLD(fic, SStream_in.H_, &SStream_in.Hv_);
      //}
      // if (SStream_in.H_.mype == 1) fprintf(fic, "Hydro makes boundary.\n");
      struct timespec start, end;
      start = cclock();
      make_boundary(idim, SStream_in.H_, &SStream_in.Hv_);
      end = cclock();
      functim[TIM_MAKBOU] += ccelaps(start, end);
      //if (SStream_in.H_.prt) {fprintf(fic, "MakeBoundary\n");}
      //PRINTUOLD(fic, SStream_in.H_, &SStream_in.Hv_);

      hydro_godunov(idim, SStream_in.dt, SStream_in.H_, &SStream_in.Hv_, &SStream_in.Hw_godunov, &SStream_in.Hvw_godunov);
    }

    SStream_in.end_iter = dcclock();
    SStream_in.cellPerCycle = (double) (SStream_in.H_.globnx * SStream_in.H_.globny) / (SStream_in.end_iter - SStream_in.start_iter) / 1000000.0L;
    SStream_in.avgCellPerCycle += SStream_in.cellPerCycle;
    SStream_in.nbCycle++;

    SStream_in.H_.nstep++;
    SStream_in.H_.t += SStream_in.dt;
    {
      real_t iter_time = (real_t) (SStream_in.end_iter - SStream_in.start_iter);
      SStream_in.flops = flopsAri * FLOPSARI + flopsSqr * FLOPSSQR + flopsMin * FLOPSMIN + flopsTra * FLOPSTRA;
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

    SStream_in.start_iter = dcclock();
    SStream_in.outnum[0] = 0;
    if ((SStream_in.H_.nstep % 2) == 0) {
      SStream_in.dt = 0;
      // if (H.mype == 0) fprintf(stdout, "Hydro computes deltat.\n");
      SStream_in.start = cclock();
      compute_deltat(&SStream_in.dt, SStream_in.H_, &SStream_in.Hw_deltat, &SStream_in.Hv_, &SStream_in.Hvw_deltat);
      SStream_in.end = cclock();
      functim[TIM_COMPDT] += ccelaps(SStream_in.start, SStream_in.end);
    }

    memcpy (&SStream_out, &SStream_in, sizeof (_state_t));
    memcpy (data_block_out, data_block_in, data_size);
    _dt_stream_view = SStream_in.dt;
    _current_t_view = SStream_in.H_.t;
    _current_nstep_view = SStream_in.H_.nstep;
  }
}
void
create_work_forward_tasks (size_t data_size)
{
  _state_t SStream_in, SStream_out;
  char data_block_in[data_size];
  char data_block_out[data_size];
#pragma omp task input (SStream_ref >> SStream_in) output (SStream_ref << SStream_out) \
  input (DStream_ref >> data_block_in[data_size]) output (DStream_ref << data_block_out[data_size]) \
  proc_bind ( master ) firstprivate (data_size)
  {
    //fprintf(stderr, " \t [Work forward task]: exec %d %d/%d\n", data_size, SStream_in.H_.mype, SStream_in.H_.nproc);
    memcpy (&SStream_out, &SStream_in, sizeof (_state_t));
    memcpy (data_block_out, data_block_in, data_size);
  }
}

void
create_termination_tasks (size_t data_size, int nproc)
{
  _state_t SStream_in[nproc];
  int term_out;
  char data_block_in[data_size*nproc];

  //fprintf(stderr, " \t\t [TERM_task]: create\n");

#pragma omp task input (SStream_ref >> SStream_in[nproc]) output (term_ref << term_out) \
  input (DStream_ref >> data_block_in[data_size*nproc])			\
  proc_bind ( master ) firstprivate (data_size, nproc)
      {
	for (int i = 0; i < nproc; ++i)
	  {
	    //fprintf(stderr, " \t\t [TERM_task]: exec\n");
	    SStream_in[i].end_time = dcclock();
	    reset_pointers_to_state_structures (&data_block_in[i * data_size], &SStream_in[i]);

	    // Deallocate work spaces
	    //deallocate_work_space(SStream_in.H_.nxyt, SStream_in.H_, &SStream_in.Hw_godunov, &SStream_in.Hvw_godunov);
	    //compute_deltat_clean_mem(SStream_in.H_, &SStream_in.Hw_deltat, &SStream_in.Hvw_deltat);

	    //hydro_finish(SStream_in.H_, &SStream_in.Hv_);
	    SStream_in[i].elaps = (double) (SStream_in[i].end_time - SStream_in[i].start_time);
	    timeToString(SStream_in[i].outnum, SStream_in[i].elaps);
	    if (SStream_in[i].H_.mype == 0) {
	      fprintf(stdout, "Hydro ends in %ss (%.3lf) <%.2lf MFlops>.\n", SStream_in[i].outnum, SStream_in[i].elaps, (float) (MflopsSUM / nbFLOPS));
	      fprintf(stdout, "       ");
	    }
	    if (SStream_in[i].H_.nproc == 1) {
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
	    if (SStream_in[i].H_.mype == 0) {
	      fprintf(stdout, "Average MC/s: %.3lf\n", (double)(SStream_in[i].avgCellPerCycle / SStream_in[i].nbCycle));
	    }
	  }
      }
}

void
create_next_rounds (int nproc, real_t tend, int nstepmax, size_t data_size)
{
  real_t _dt_stream_in[nproc];
  real_t _current_t_in[nproc];
  int _current_nstep_in[nproc];

  //fprintf(stderr, " -- [next_rounds] --: create with nproc = %d\n", nproc);

  // Find out the minimumm dt and
#pragma omp task input (_dt_stream_ref >> _dt_stream_in[nproc], _current_t_ref >> _current_t_in[nproc], _current_nstep_ref >> _current_nstep_in[nproc]) \
  proc_bind (master) firstprivate (nproc)
  {
    //fprintf(stderr, " -- [next_rounds] --: exec\n");
    real_t temp_min = _dt_stream_in[0];
    for (int j = 1; j < nproc; ++j)
      temp_min = (temp_min < _dt_stream_in[j]) ? temp_min : _dt_stream_in[j];

    // If the termination test is negative, setup the next iteration
    //fprintf(stderr, " -- [next_rounds] --: current_t %f \t temp_min %f \t tend %f \t cur_nstep %d \t nstepmax %d\n",
    //_current_t_in[0], temp_min, tend, _current_nstep_in[0], nstepmax);

    if ((_current_t_in[0] + temp_min < tend) && (_current_nstep_in[0] + 1 < nstepmax))
      {
#pragma omp task proc_bind (master) firstprivate (nproc)
	{
	  //fprintf(stderr, " -- [PROXY] --: exec\n");
	  for (int j = 0; j < nproc; ++j)
	    create_work_tasks (temp_min, data_size);
	  for (int j = 0; j < nproc; ++j)
	    create_work_forward_tasks (data_size);
	  create_next_rounds (nproc, tend, nstepmax, data_size);
	}
      }
    else
      create_termination_tasks (data_size, nproc);
  }
}


int
main(int argc, char **argv) {

  hydroparam_t _init_H, *H; H = &_init_H;
  _state_t SStream __attribute__((stream));
  char DStream __attribute__((stream));
  real_t _dt_stream __attribute__((stream));
  real_t _current_t __attribute__((stream));
  int _current_nstep __attribute__((stream));
  int term __attribute__((stream));

  // array of timers to profile the code
  memset(functim, 0, TIM_END * sizeof(functim[0]));

  process_args(argc, argv, H);
  //H->nproc = 4;
  int nproc = H->nproc;
  int nstepmax = H->nstepmax;
  real_t tend = H->tend;

  _dt_stream_ref = _dt_stream;
  _current_t_ref = _current_t;
  _current_nstep_ref = _current_nstep;
  SStream_ref = SStream;
  DStream_ref = DStream;
  term_ref = term;

  setup_subsurface (0, &_init_H);

  size_t data_size = estimate_data_size (&_init_H);

  fprintf(stderr, " >> Hydro starting on %d nodes -- data block size is %d\n", nproc, data_size);

  for (int i = 0; i < nproc; ++i)
    partition_and_initialize_data_stream (&_init_H, i, data_size);
  for (int i = 0; i < nproc; ++i)
    create_first_tasks (data_size);
  for (int i = 0; i < nproc; ++i)
    create_first_forward_tasks (data_size);

  create_next_rounds (nproc, tend, nstepmax, data_size);

#pragma omp task input (term) proc_bind (master)
  {
    fprintf(stderr, "TERMINATE\n");
  }
#pragma omp taskwait

#ifdef MPI
  MPI_Finalize();
#endif
  return 0;
}
