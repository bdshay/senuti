/* Compile a C# program.
   Copyright (C) 2003-2004 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <alloca.h>

/* Specification.  */
#include "csharpcomp.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "execute.h"
#include "pipe.h"
#include "wait-process.h"
#include "getline.h"
#include "sh-quote.h"
#include "xallocsa.h"
#include "error.h"
#include "gettext.h"

#define _(str) gettext (str)


/* Survey of C# compilers.

   Program    from

   cscc       pnet
   mcs        mono
   csc        sscli

   We try the CIL interpreters in the following order:
     1. "cscc", because it is a completely free system.
     2. "mcs", because it is a free system but doesn't integrate so well
        with Unix. (Command line options start with / instead of -. Errors go
        to stdout instead of stderr. Source references are printed as
        "file(lineno)" instead of "file:lineno:".)
     3. "csc", although it is not free, because it is a kind of "reference
        implementation" of C#.
   But the order can be changed through the --enable-csharp configuration
   option.
 */

static int
compile_csharp_using_pnet (const char * const *sources,
			   unsigned int sources_count,
			   const char * const *libdirs,
			   unsigned int libdirs_count,
			   const char * const *libraries,
			   unsigned int libraries_count,
			   const char *output_file, bool output_is_library,
			   bool optimize, bool debug,
			   bool verbose)
{
  static bool cscc_tested;
  static bool cscc_present;

  if (!cscc_tested)
    {
      /* Test for presence of cscc:
	 "cscc --version >/dev/null 2>/dev/null"  */
      char *argv[3];
      int exitstatus;

      argv[0] = "cscc";
      argv[1] = "--version";
      argv[2] = NULL;
      exitstatus = execute ("cscc", "cscc", argv, false, false, true, true,
			    true, false);
      cscc_present = (exitstatus == 0);
      cscc_tested = true;
    }

  if (cscc_present)
    {
      unsigned int argc;
      char **argv;
      char **argp;
      int exitstatus;
      unsigned int i;

      argc =
	1 + (output_is_library ? 1 : 0) + 2 + 2 * libdirs_count
	+ 2 * libraries_count + (optimize ? 1 : 0) + (debug ? 1 : 0)
	+ sources_count;
      argv = (char **) xallocsa ((argc + 1) * sizeof (char *));

      argp = argv;
      *argp++ = "cscc";
      if (output_is_library)
	*argp++ = "-shared";
      *argp++ = "-o";
      *argp++ = (char *) output_file;
      for (i = 0; i < libdirs_count; i++)
	{
	  *argp++ = "-L";
	  *argp++ = (char *) libdirs[i];
	}
      for (i = 0; i < libraries_count; i++)
	{
	  *argp++ = "-l";
	  *argp++ = (char *) libraries[i];
	}
      if (optimize)
	*argp++ = "-O";
      if (debug)
	*argp++ = "-g";
      for (i = 0; i < sources_count; i++)
	{
	  const char *source_file = sources[i];
	  if (strlen (source_file) >= 9
	      && memcmp (source_file + strlen (source_file) - 9, ".resource",
			 9) == 0)
	    {
	      char *option = (char *) xallocsa (12 + strlen (source_file) + 1);

	      memcpy (option, "-fresources=", 12);
	      strcpy (option + 12, source_file);
	      *argp++ = option;
	    }
	  else
	    *argp++ = (char *) source_file;
	}
      *argp = NULL;
      /* Ensure argv length was correctly calculated.  */
      if (argp - argv != argc)
	abort ();

      if (verbose)
	{
	  char *command = shell_quote_argv (argv);
	  printf ("%s\n", command);
	  free (command);
	}

      exitstatus = execute ("cscc", "cscc", argv, false, false, false, false,
			    true, true);

      for (i = 0; i < sources_count; i++)
	if (argv[argc - sources_count + i] != sources[i])
	  freesa (argv[argc - sources_count + i]);
      freesa (argv);

      return (exitstatus != 0);
    }
  else
    return -1;
}

static int
compile_csharp_using_mono (const char * const *sources,
			   unsigned int sources_count,
			   const char * const *libdirs,
			   unsigned int libdirs_count,
			   const char * const *libraries,
			   unsigned int libraries_count,
			   const char *output_file, bool output_is_library,
			   bool optimize, bool debug,
			   bool verbose)
{
  static bool mcs_tested;
  static bool mcs_present;

  if (!mcs_tested)
    {
      /* Test for presence of mcs:
	 "mcs --version >/dev/null 2>/dev/null"  */
      char *argv[3];
      int exitstatus;

      argv[0] = "mcs";
      argv[1] = "--version";
      argv[2] = NULL;
      exitstatus = execute ("mcs", "mcs", argv, false, false, true, true, true,
			    false);
      mcs_present = (exitstatus == 0);
      mcs_tested = true;
    }

  if (mcs_present)
    {
      unsigned int argc;
      char **argv;
      char **argp;
      pid_t child;
      int fd[1];
      FILE *fp;
      char *line[2];
      size_t linesize[2];
      size_t linelen[2];
      unsigned int l;
      int exitstatus;
      unsigned int i;

      argc =
	1 + (output_is_library ? 1 : 0) + 2 + 2 * libdirs_count
	+ 2 * libraries_count + (debug ? 1 : 0) + sources_count;
      argv = (char **) xallocsa ((argc + 1) * sizeof (char *));

      argp = argv;
      *argp++ = "mcs";
      if (output_is_library)
	*argp++ = "-target:library";
      *argp++ = "-o";
      *argp++ = (char *) output_file;
      for (i = 0; i < libdirs_count; i++)
	{
	  *argp++ = "-L";
	  *argp++ = (char *) libdirs[i];
	}
      for (i = 0; i < libraries_count; i++)
	{
	  *argp++ = "-r";
	  *argp++ = (char *) libraries[i];
	}
      if (debug)
	*argp++ = "-g";
      for (i = 0; i < sources_count; i++)
	{
	  const char *source_file = sources[i];
	  if (strlen (source_file) >= 9
	      && memcmp (source_file + strlen (source_file) - 9, ".resource",
			 9) == 0)
	    {
	      char *option = (char *) xallocsa (10 + strlen (source_file) + 1);

	      memcpy (option, "-resource:", 10);
	      strcpy (option + 10, source_file);
	      *argp++ = option;
	    }
	  else
	    *argp++ = (char *) source_file;
	}
      *argp = NULL;
      /* Ensure argv length was correctly calculated.  */
      if (argp - argv != argc)
	abort ();

      if (verbose)
	{
	  char *command = shell_quote_argv (argv);
	  printf ("%s\n", command);
	  free (command);
	}

      child = create_pipe_in ("mcs", "mcs", argv, NULL, false, true, true, fd);

      /* Read the subprocess output, copying it to stderr.  Drop the last
	 line if it starts with "Compilation succeeded".  */
      fp = fdopen (fd[0], "r");
      if (fp == NULL)
	error (EXIT_FAILURE, errno, _("fdopen() failed"));
      line[0] = NULL; linesize[0] = 0;
      line[1] = NULL; linesize[1] = 0;
      l = 0;
      for (;;)
	{
	  linelen[l] = getline (&line[l], &linesize[l], fp);
	  if (linelen[l] == (size_t)(-1))
	    break;
	  l = (l + 1) % 2;
	  if (line[l] != NULL)
	    fwrite (line[l], 1, linelen[l], stderr);
	}
      l = (l + 1) % 2;
      if (line[l] != NULL
	  && !(linelen[l] >= 21
	       && memcmp (line[l], "Compilation succeeded", 21) == 0))
	fwrite (line[l], 1, linelen[l], stderr);
      if (line[0] != NULL)
	free (line[0]);
      if (line[1] != NULL)
	free (line[1]);
      fclose (fp);

      /* Remove zombie process from process list, and retrieve exit status.  */
      exitstatus = wait_subprocess (child, "mcs", false, false, true, true);

      for (i = 0; i < sources_count; i++)
	if (argv[argc - sources_count + i] != sources[i])
	  freesa (argv[argc - sources_count + i]);
      freesa (argv);

      return (exitstatus != 0);
    }
  else
    return -1;
}

static int
compile_csharp_using_sscli (const char * const *sources,
			    unsigned int sources_count,
			    const char * const *libdirs,
			    unsigned int libdirs_count,
			    const char * const *libraries,
			    unsigned int libraries_count,
			    const char *output_file, bool output_is_library,
			    bool optimize, bool debug,
			    bool verbose)
{
  static bool csc_tested;
  static bool csc_present;

  if (!csc_tested)
    {
      /* Test for presence of csc:
	 "csc -help >/dev/null 2>/dev/null"  */
      char *argv[3];
      int exitstatus;

      argv[0] = "csc";
      argv[1] = "-help";
      argv[2] = NULL;
      exitstatus = execute ("csc", "csc", argv, false, false, true, true, true,
			    false);
      csc_present = (exitstatus == 0);
      csc_tested = true;
    }

  if (csc_present)
    {
      unsigned int argc;
      char **argv;
      char **argp;
      int exitstatus;
      unsigned int i;

      argc =
	1 + 1 + 1 + libdirs_count + libraries_count
	+ (optimize ? 1 : 0) + (debug ? 1 : 0) + sources_count;
      argv = (char **) xallocsa ((argc + 1) * sizeof (char *));

      argp = argv;
      *argp++ = "csc";
      *argp++ = (output_is_library ? "-target:library" : "-target:exe");
      {
	char *option = (char *) xallocsa (5 + strlen (output_file) + 1);
	memcpy (option, "-out:", 5);
	strcpy (option + 5, output_file);
	*argp++ = option;
      }
      for (i = 0; i < libdirs_count; i++)
	{
	  char *option = (char *) xallocsa (5 + strlen (libdirs[i]) + 1);
	  memcpy (option, "-lib:", 5);
	  strcpy (option + 5, libdirs[i]);
	  *argp++ = option;
	}
      for (i = 0; i < libraries_count; i++)
	{
	  char *option = (char *) xallocsa (11 + strlen (libraries[i]) + 1);
	  memcpy (option, "-reference:", 11);
	  strcpy (option + 11, libraries[i]);
	  *argp++ = option;
	}
      if (optimize)
	*argp++ = "-optimize+";
      if (debug)
	*argp++ = "-debug+";
      for (i = 0; i < sources_count; i++)
	{
	  const char *source_file = sources[i];
	  if (strlen (source_file) >= 9
	      && memcmp (source_file + strlen (source_file) - 9, ".resource",
			 9) == 0)
	    {
	      char *option = (char *) xallocsa (10 + strlen (source_file) + 1);

	      memcpy (option, "-resource:", 10);
	      strcpy (option + 10, source_file);
	      *argp++ = option;
	    }
	  else
	    *argp++ = (char *) source_file;
	}
      *argp = NULL;
      /* Ensure argv length was correctly calculated.  */
      if (argp - argv != argc)
	abort ();

      if (verbose)
	{
	  char *command = shell_quote_argv (argv);
	  printf ("%s\n", command);
	  free (command);
	}

      exitstatus = execute ("csc", "csc", argv, false, false, false, false,
			    true, true);

      for (i = 2; i < 3 + libdirs_count + libraries_count; i++)
	freesa (argv[i]);
      for (i = 0; i < sources_count; i++)
	if (argv[argc - sources_count + i] != sources[i])
	  freesa (argv[argc - sources_count + i]);
      freesa (argv);

      return (exitstatus != 0);
    }
  else
    return -1;
}

bool
compile_csharp_class (const char * const *sources,
		      unsigned int sources_count,
		      const char * const *libdirs,
		      unsigned int libdirs_count,
		      const char * const *libraries,
		      unsigned int libraries_count,
		      const char *output_file,
		      bool optimize, bool debug,
		      bool verbose)
{
  bool output_is_library =
    (strlen (output_file) >= 4
     && memcmp (output_file + strlen (output_file) - 4, ".dll", 4) == 0);
  int result;

  /* First try the C# implementation specified through --enable-csharp.  */
#if CSHARP_CHOICE_PNET
  result = compile_csharp_using_pnet (sources, sources_count,
				      libdirs, libdirs_count,
				      libraries, libraries_count,
				      output_file, output_is_library,
				      optimize, debug, verbose);
  if (result >= 0)
    return (bool) result;
#endif

#if CSHARP_CHOICE_MONO
  result = compile_csharp_using_mono (sources, sources_count,
				      libdirs, libdirs_count,
				      libraries, libraries_count,
				      output_file, output_is_library,
				      optimize, debug, verbose);
  if (result >= 0)
    return (bool) result;
#endif

  /* Then try the remaining C# implementations in our standard order.  */
#if !CSHARP_CHOICE_PNET
  result = compile_csharp_using_pnet (sources, sources_count,
				      libdirs, libdirs_count,
				      libraries, libraries_count,
				      output_file, output_is_library,
				      optimize, debug, verbose);
  if (result >= 0)
    return (bool) result;
#endif

#if !CSHARP_CHOICE_MONO
  result = compile_csharp_using_mono (sources, sources_count,
				      libdirs, libdirs_count,
				      libraries, libraries_count,
				      output_file, output_is_library,
				      optimize, debug, verbose);
  if (result >= 0)
    return (bool) result;
#endif

  result = compile_csharp_using_sscli (sources, sources_count,
				       libdirs, libdirs_count,
				       libraries, libraries_count,
				       output_file, output_is_library,
				       optimize, debug, verbose);
  if (result >= 0)
    return (bool) result;

  error (0, 0, _("C# compiler not found, try installing pnet"));
  return true;
}
