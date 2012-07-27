#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

static void
wstream_df_verror (const char *fmt, va_list list)
{
  fputs ("\nlibwstream_df: ", stderr);
  vfprintf (stderr, fmt, list);
  fputc ('\n', stderr);
}

void
wstream_df_error (const char *fmt, ...)
{
  va_list list;

  va_start (list, fmt);
  wstream_df_verror (fmt, list);
  va_end (list);
}

void
wstream_df_fatal (const char *fmt, ...)
{
  va_list list;

  va_start (list, fmt);
  wstream_df_verror (fmt, list);
  va_end (list);

  exit (EXIT_FAILURE);
}
