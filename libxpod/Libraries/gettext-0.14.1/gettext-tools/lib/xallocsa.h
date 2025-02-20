/* Safe automatic memory allocation with out of memory checking.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2003.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _XALLOCSA_H
#define _XALLOCSA_H

#include "allocsa.h"

/* xallocsa(N) is a checking safe variant of alloca(N).  It allocates N bytes
   of memory allocated on the stack, that must be freed using freesa() before
   the function returns.  Upon failure, it exits with an error message.  */
#if HAVE_ALLOCA
# define xallocsa(N) \
  ((N) < 4032 - sa_increment					    \
   ? (void *) ((char *) alloca ((N) + sa_increment) + sa_increment) \
   : xmallocsa (N))
extern void * xmallocsa (size_t n);
#else
# define xallocsa(N) \
  xmalloc (N)
#endif

/* Maybe we should also define a variant
    xnallocsa (size_t n, size_t s) - behaves like xallocsa (n * s)
   If this would be useful in your application. please speak up.  */

#endif /* _XALLOCSA_H */
