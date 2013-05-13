/*
 * getopt.c:
 *      Derived from AT&T public domain source of getopt(3),
 *      modified for use with MS C 6.0 on MS DOS systems. For
 *      unknown reasons the variable optopt is exported here.
 *
 *  Note that each option may occur more than once in the command
 *  line, this may require special action like occurence counting.
 *  Each option is indicated by a single character in opts string
 *  followed by : if an option argument is required. So for "abo:"
 *  the following combinations are possible:
 *
 *      -a -b -o value  sets a, b, and argument value for o
 *      -ab -o value    equivalent
 *      -ab -ovalue     equivalent, but not recommended
 *      -abovalue       equivalent, but not recommended
 *      -a -- -b        sets only a, optind advanced to -b
 *      -a - -b         sets only a, optind stays at single -
 *      -A              error message for A, returned as ?
 *      -o              error message if no more arguments
 *
 * example code:
 *
 *  ...
 *  extern int getopt(int, char **, char *);
 *  ...
 *  int main(int argc, char *argv[]) {
 *      extern int   opterr;
 *      extern int   optind;
 *      extern char *optarg;
 *      int c,  aset = 0,  bset = 0;
 *      char *oarg = NULL;
 *
 *      while ((c = getopt(argc, argv, "abo:")) != -1)
 *      switch (c) {
 *          case 'a':
 *              ++aset;     continue;
 *          case 'b':
 *              ++bset;     continue;
 *          case 'o':
 *              oarg = optarg;  continue;
 *          default:
 *                 ...      return 1;
 *      }
 *      ...
 *  }
 *
 * Note the following cases are NEVE NEEDED:
 *
 *          case '?': NOT EXPLICITLY NEEDED WITH DEFAULT
 *          case ':': WILL NEVER HAPPEN, ':' NOT ALLOWED
 *          case '-': WILL NEVER HAPPEN, '-' NOT ALLOWED
 */

#include <stdio.h>
#include <string.h>
#include "getopt.h"

/* ------------ EXPORT variables -------------------------------------- */

char* optarg;     // option argument if ':' in opts
int   optind = 1; // next argv index
int   opterr = 1; // show error message if not 0
int   optopt;     // last option (export dubious)

/* ------------ private section --------------------------------------- */

static int sp = 1;     /* offset within option word    */

static int badopt(const char* text)
{
    /* show error message if not 0  */
    if (opterr) {
        fprintf(stderr, "%s: '%c'\n", text, optopt);
    }

    /* ?: result for invalid option */
    return '?';
}

/* ------------ EXPORT function --------------------------------------- */

int getopt(int argc, char* argv[], const char* opts)
{
    char* cp;
    char ch;

    if (sp == 1) {
        if (argc <= optind || argv[optind][1] == '\0') {
            /* no more words or single '-'  */
            return EOF;
        }

        if ((ch = argv[optind][0]) != '-') {
            /* options must start with '-'  */
            return EOF;
        }

        if (strcmp(argv[optind], "--") == 0) {
            /* to next word */
            ++optind;

            /* -- marks end */
            return EOF;
        }
    }

    optopt = (int) (ch = argv[optind][sp]);    /* flag option  */

    if (ch == ':' || (cp = strchr(opts, ch)) == NULL) {
        if (argv[optind][++sp] == '\0') {
            /* to next word */
            ++optind;
            sp = 1;
        }

        return badopt("illegal option");
    }

    /* ':' option requires argument */
    if (*++cp == ':') {
        /* if same word */
        optarg = &argv[optind][sp + 1];

        /* to next word */
        ++optind;
        sp = 1;

        /* in next word */
        if (*optarg == '\0') {
            /* no more word */
            if (argc <= optind) {
                return badopt("option requires an argument");
            }

            /* to next word */
            optarg = argv[optind++];
        }
    } else {
        /* flag option without argument */
        optarg = NULL;

        if (argv[optind][++sp] == '\0') {
            /* to next word */
            optind++;
            sp = 1;
        }
    }

    return optopt;
}
