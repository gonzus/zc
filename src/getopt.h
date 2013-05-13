#ifndef GETOPT_H_
#define GETOPT_H_

// Global exported variables:
extern char* optarg; // option argument if ':' in opts
extern int optind;   // next argv index
extern int opterr;   // show error message if not 0
extern int optopt;   // last option (export dubious)

// Handle arguments in command line.
int getopt(int argc, char* argv[], const char* opts);

#endif
