#include "fibers.h"

# if defined(__x86_64__)

__attribute__((__noinline__,__noclone__,__optimize__("O1")))
int
ws_swapcontext (ws_ctx_p old_ctx, ws_ctx_p new_ctx)
{
  /* Save inst ptr for continuation.  */
  old_ctx->inst_p = &&cont;

  __asm__ __volatile__(/* Save old stack ptr.  */
		       "mov %%rbp, %0\n\t"
		       "mov %%rsp, %1\n\t"
		       : "=m" (*&old_ctx->stack_bp), "=m" (*&old_ctx->stack_sp) :: "memory");

#  ifdef _WS_SAVE_SIGNAL_MASKS_ON_FIBER_MIGRATION
  sigprocmask (SIG_SETMASK, &new_ctx->sset, &old_ctx->sset);
#  endif

#  ifndef _WS_SAVE_SSE_ENV
  __asm__ __volatile__(/* Load new FP env and the new stack ptrs.  */
		       "fldenv %0\n\t"
		       "ldmxcsr %1\n\t"
		       :: "m" (*&new_ctx->fpenv), "m" (*&new_ctx->fpenv.__mxcsr));
#  else
  __asm__ __volatile__("rex64/fxrstor  (%[fx])\n\t" :: [fx] "R" (&new_ctx->fxsave_region), "m" (new_ctx->fxsave_region));
#  endif


  __asm__ __volatile__("mov %0, %%rbp\n\t"
		       "mov %1, %%rsp\n\t"

		       /* Jump to destination continuation/new worker
			  sched loop.  */
		       "jmp *%2"

		       :: "m" (*&new_ctx->stack_bp), "m" (*&new_ctx->stack_sp), "m" (*&new_ctx->inst_p)
		       : "memory", "cc", "%rbx", "%r12","%r13","%r14","%r15");

  /* The continuation.  Jump here once barrier clears.  */
 cont:

  return 0;
}

__attribute__((__noinline__,__noclone__,__optimize__("O1")))
int
ws_setcontext (ws_ctx_p new_ctx)
{
#  ifdef _WS_SAVE_SIGNAL_MASKS_ON_FIBER_MIGRATION
  sigprocmask (SIG_SETMASK, &new_ctx->sset, NULL);
#  endif

#  ifndef _WS_SAVE_SSE_ENV
  __asm__ __volatile__(/* Load new FP env and the new stack ptrs.  */
		       "fldenv %0\n\t"
		       "ldmxcsr %1\n\t"
		       :: "m" (*&new_ctx->fpenv), "m" (*&new_ctx->fpenv.__mxcsr));
#  else
  __asm__ __volatile__("rex64/fxrstor  (%[fx])\n\t" :: [fx] "R" (&new_ctx->fxsave_region), "m" (new_ctx->fxsave_region));
#  endif


  __asm__ __volatile__("mov %0, %%rbp\n\t"
		       "mov %1, %%rsp\n\t"

		       /* Jump to destination continuation/new worker
			  sched loop.  */
		       "jmp *%2"

		       :: "m" (*&new_ctx->stack_bp), "m" (*&new_ctx->stack_sp), "m" (*&new_ctx->inst_p)
		       : "memory", "cc", "%rbx", "%r12","%r13","%r14","%r15");

  return 0;
}

#endif /* __x86_64__ */
