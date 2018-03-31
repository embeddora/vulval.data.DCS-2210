#ifndef _ERROR_H
#define _ERROR_H

#if HAVE_STRERROR
/* extern int errno; */
#include <errno.h> /* By Alex */
#  define error_string strerror(errno)
#elif HAVE_SYS_ERRLIST
extern const char *const sys_errlist[];
/* extern int errno; */
#include <errno.h>  /* By Alex */
#  define error_string (sys_errlist[errno])
#else
#  define error_string "error message not found"
#endif

#endif
