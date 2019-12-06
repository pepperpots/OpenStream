#ifndef _FIBERS_H_
#define _FIBERS_H_

extern "C" {

#include <stddef.h>
#include "error.h"

/* Our own architecture-dependent implementation of lightweight
   context swapping (no syscalls except if signal masks need to be
   managed, disabled by default).  */
# if defined(__x86_64__)

#  ifndef _WS_SAVE_SSE_ENV
#   include <fenv.h>
#  else
#   include <sys/user.h>
#  endif

#  ifdef _WS_SAVE_SIGNAL_MASKS_ON_FIBER_MIGRATION
#   include <signal.h>
#  endif

typedef struct ws_ctx
{
  void *stack_bp;
  void *stack_sp;
  size_t stack_size;
  void *inst_p;

#  ifndef _WS_SAVE_SSE_ENV
  fenv_t fpenv;
#  else
  /* Save region is required to be 512 bytes and 16-byte aligned
   * (though it segfaults on less than 32), but we can use when
   * available the struct for debug purposes.

     char fxsave_region[512] __attribute__((aligned(32)));
  */
  struct user_fpregs_struct fxsave_region __attribute__((aligned(32)));
#  endif

#  ifdef _WS_SAVE_SIGNAL_MASKS_ON_FIBER_MIGRATION
  sigset_t sset;
#  endif
} ws_ctx_t, *ws_ctx_p;

__attribute__((__optimize__("O1")))
static inline void
ws_prepcontext (ws_ctx_p ctx, void *sp, size_t ssz, void *fn)
{
  /* Hack warning: need to drop 8 bytes from stack to ensure that it
     is 16-bytes aligned.  */
  ctx->stack_bp = sp + ssz - 8;
  ctx->stack_sp = sp + ssz - 8;
  ctx->stack_size = ssz;
  ctx->inst_p = fn;

#  ifdef _WS_SAVE_SIGNAL_MASKS_ON_FIBER_MIGRATION
  sigprocmask (SIG_BLOCK, NULL, &ctx->sset);
#  endif

#  ifndef _WS_SAVE_SSE_ENV
  __asm__ __volatile__ ("fnstenv %0" : "=m" (*&ctx->fpenv) :: "memory");
  __asm__ __volatile__ ("stmxcsr %0" : "=m" (*&ctx->fpenv.__mxcsr) :: "memory");
#  else
  __asm__ __volatile__ ("rex64/fxsave  (%[fx])" : "=m" (ctx->fxsave_region) : [fx] "R" (&ctx->fxsave_region) : "memory");
#  endif
}

int ws_swapcontext (ws_ctx_p old_ctx, ws_ctx_p new_ctx);
int ws_setcontext (ws_ctx_p new_ctx);

/* Portable ucontext based implementation.  */
# else /* ! __x86_64__ */
#  include <ucontext.h>

typedef ucontext_t ws_ctx_t;
typedef ucontext_t * ws_ctx_p;

static inline void
ws_prepcontext (ws_ctx_p ctx, void *sp, size_t ssz, void *fn)
{
  if (getcontext (ctx) == -1)
    wstream_df_fatal ("Cannot get context.");
  if (sp != NULL && fn != NULL)
    {
      ctx->uc_stack.ss_sp = sp;
      ctx->uc_stack.ss_size = ssz;
      ctx->uc_link = NULL;
      makecontext (ctx, fn, 0);
    }
}

static inline int
ws_swapcontext (ws_ctx_p old, ws_ctx_p new_ctx)
{
  return swapcontext (old, new_ctx);
}

static inline int
ws_setcontext (ws_ctx_p new_ctx)
{
  return setcontext (new_ctx);
}

# endif

}

#endif /* _FIBERS_H_ */
