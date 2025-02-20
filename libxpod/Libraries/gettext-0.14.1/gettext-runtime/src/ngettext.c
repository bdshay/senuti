/* ngettext - retrieve plural form string from message catalog and print it.
   Copyright (C) 1995-1997, 2000-2004 Free Software Foundation, Inc.

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

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#include "closeout.h"
#include "error.h"
#include "progname.h"
#include "relocatable.h"
#include "basename.h"
#include "xalloc.h"
#include "exit.h"

#include "gettext.h"

#define _(str) gettext (str)

/* If true, expand escape sequences in strings before looking in the
   message catalog.  */
static int do_expand;

/* Long options.  */
static const struct option long_options[] =
{
  { "domain", required_argument, NULL, 'd' },
  { "help", no_argument, NULL, 'h' },
  { "version", no_argument, NULL, 'V' },
  { NULL, 0, NULL, 0 }
};

/* Forward declaration of local functions.  */
static void usage (int status)
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
     __attribute__ ((noreturn))
#endif
;
static const char *expand_escape (const char *str);

int
main (int argc, char *argv[])
{
  int optchar;
  const char *msgid;
  const char *msgid_plural;
  const char *count;
  unsigned long n;

  /* Default values for command line options.  */
  bool do_help = false;
  bool do_version = false;
  const char *domain = getenv ("TEXTDOMAIN");
  const char *domaindir = getenv ("TEXTDOMAINDIR");
  do_expand = false;

  /* Set program name for message texts.  */
  set_program_name (argv[0]);

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, relocate (LOCALEDIR));
  textdomain (PACKAGE);

  /* Ensure that write errors on stdout are detected.  */
  atexit (close_stdout);

  /* Parse command line options.  */
  while ((optchar = getopt_long (argc, argv, "+d:eEhV", long_options, NULL))
	 != EOF)
    switch (optchar)
    {
    case '\0':		/* Long option.  */
      break;
    case 'd':
      domain = optarg;
      break;
    case 'e':
      do_expand = true;
      break;
    case 'E':
      /* Ignore.  Just for compatibility.  */
      break;
    case 'h':
      do_help = true;
      break;
    case 'V':
      do_version = true;
      break;
    default:
      usage (EXIT_FAILURE);
    }

  /* Version information is requested.  */
  if (do_version)
    {
      printf ("%s (GNU %s) %s\n", basename (program_name), PACKAGE, VERSION);
      /* xgettext: no-wrap */
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"),
	      "1995-1997, 2000-2004");
      printf (_("Written by %s.\n"), "Ulrich Drepper");
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  /* More optional command line options.  */
  switch (argc - optind)
    {
    default:
      error (EXIT_FAILURE, 0, _("too many arguments"));

    case 4:
      domain = argv[optind++];
      /* FALLTHROUGH */

    case 3:
      break;

    case 2:
    case 1:
    case 0:
      error (EXIT_FAILURE, 0, _("missing arguments"));
    }

  /* Now the mandatory command line options.  */
  msgid = argv[optind++];
  msgid_plural = argv[optind++];
  count = argv[optind++];

  if (optind != argc)
    abort ();

  {
    char *endp;
    unsigned long tmp_val;

    errno = 0;
    tmp_val = strtoul (count, &endp, 10);
    if (errno == 0 && count[0] != '\0' && endp[0] == '\0')
      n = tmp_val;
    else
      /* When COUNT is not valid, use plural.  */
      n = 99;
  }

  /* Expand escape sequences if enabled.  */
  if (do_expand)
    {
      msgid = expand_escape (msgid);
      msgid_plural = expand_escape (msgid_plural);
    }

  /* If no domain name is given we don't translate, and we use English
     plural form handling.  */
  if (domain == NULL || domain[0] == '\0')
    fputs (n == 1 ? msgid : msgid_plural, stdout);
  else
    {
      /* Bind domain to appropriate directory.  */
      if (domaindir != NULL && domaindir[0] != '\0')
	bindtextdomain (domain, domaindir);

      /* Write out the result.  */
      fputs (dngettext (domain, msgid, msgid_plural, n), stdout);
    }

  exit (EXIT_SUCCESS);
}


/* Display usage information and exit.  */
static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      /* xgettext: no-wrap */
      printf (_("\
Usage: %s [OPTION] [TEXTDOMAIN] MSGID MSGID-PLURAL COUNT\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Display native language translation of a textual message whose grammatical\n\
form depends on a number.\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
  -d, --domain=TEXTDOMAIN   retrieve translated message from TEXTDOMAIN\n\
  -e                        enable expansion of some escape sequences\n\
  -E                        (ignored for compatibility)\n\
  -h, --help                display this help and exit\n\
  -V, --version             display version information and exit\n\
  [TEXTDOMAIN]              retrieve translated message from TEXTDOMAIN\n\
  MSGID MSGID-PLURAL        translate MSGID (singular) / MSGID-PLURAL (plural)\n\
  COUNT                     choose singular/plural form based on this value\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
If the TEXTDOMAIN parameter is not given, the domain is determined from the\n\
environment variable TEXTDOMAIN.  If the message catalog is not found in the\n\
regular directory, another location can be specified with the environment\n\
variable TEXTDOMAINDIR.\n\
Standard search directory: %s\n"),
	      getenv ("IN_HELP2MAN") == NULL ? LOCALEDIR : "@localedir@");
      printf ("\n");
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"), stdout);
    }

  exit (status);
}


/* Expand some escape sequences found in the argument string.  */
static const char *
expand_escape (const char *str)
{
  char *retval, *rp;
  const char *cp = str;

  do
    {
      while (cp[0] != '\0' && cp[0] != '\\')
	++cp;
    }
  while (cp[0] != '\0' && cp[1] != '\0'
	 && strchr ("bfnrt\\01234567", cp[1]) == NULL);

  if (cp[0] == '\0')
    return str;

  retval = (char *) xmalloc (strlen (str));

  rp = retval + (cp - str);
  memcpy (retval, str, cp - str);

  do
    {
      switch (*++cp)
	{
	case 'b':		/* backspace */
	  *rp++ = '\b';
	  ++cp;
	  break;
	case 'f':		/* form feed */
	  *rp++ = '\f';
	  ++cp;
	  break;
	case 'n':		/* new line */
	  *rp++ = '\n';
	  ++cp;
	  break;
	case 'r':		/* carriage return */
	  *rp++ = '\r';
	  ++cp;
	  break;
	case 't':		/* horizontal tab */
	  *rp++ = '\t';
	  ++cp;
	  break;
	case '\\':
	  *rp = '\\';
	  ++cp;
	  break;
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	  {
	    int ch = *cp++ - '0';

	    if (*cp >= '0' && *cp <= '7')
	      {
		ch *= 8;
		ch += *cp++ - '0';

		if (*cp >= '0' && *cp <= '7')
		  {
		    ch *= 8;
		    ch += *cp++ - '0';
		  }
	      }
	    *rp = ch;
	  }
	  break;
	default:
	  *rp = '\\';
	  break;
	}

      while (cp[0] != '\0' && cp[0] != '\\')
	*rp++ = *cp++;
    }
  while (cp[0] != '\0');

  /* Terminate string.  */
  *rp = '\0';

  return (const char *) retval;
}
