/*
Main program for CS537 - Project 3 - RAID
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG

// Program arguments ----------------------------------------------------------
struct args {
    int level;
    int strip;
    int disks;
    int size;
    const char *trace;
};
struct args args;
int verbose = 0;

// Note: this could be safer by using strol or bsd strtonum instead of atoi...
// but that seems excessive for a class project
int parseCmdLine(int argc, char *argv[]) {
    // Validate number of arguments
    if (argc < 11 || argc > 12) {
        return -1;
    }

    // Set args struct to known state
    memset(&args, -1, sizeof(struct args));

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-level") == 0) {
            args.level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-strip") == 0) {
            args.strip = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-disks") == 0) {
            args.disks = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-size") == 0) {
            args.size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-trace") == 0) {
            args.trace = argv[++i];
        } else if (strcmp(argv[i], "-verbose") == 0) {
            verbose = 1;
        }
    }

    // Validate arguments
    if (args.level != 0 && args.level != 10 && args.level != 4 && args.level != 5) {
        return -1;
    }
    if (args.strip < 1) {
        return -1;
    }
    if ((args.disks < 1)
     || ((args.level == 10) && (args.disks % 2 != 0))  // Raid 10 requires even # disks
     || ((args.level == 4)  && (args.disks < 3))       // Raid 4 requires more than 2 disks
     || ((args.level == 5)  && (args.disks < 3)) ) {   // Raid 5 requires more than 2 disks
        fprintf(stderr, "Invalid number of disks: RAID 10 requires an even number of disks, RAID 4 and 5 require > 2 disk\n");
        return -1;
    }
    if (args.size < 1) {
        return -1;
    }
    // TODO : try to open file specified by args.trace return -1 if it can't be opened?

    return 0;
}



// Entry point ----------------------------------------------------------------
int main( int argc, char *argv[] )
{
	if (parseCmdLine(argc, argv) == -1) {
		fprintf(stderr, "Usage: %s <-level> [0|10|4|5] <-strip> n <-disks> n <-size> n <-trace> traceFile [-verbose]\n", argv[0]);
		return 1;
    }

    // TODO : run the simulation 
    printf("args: \n  level = %d\n  strip = %d\n  disks = %d\n  size = %d\n  trace = %s\n  verbose = %d\n",
        args.level, args.strip, args.disks, args.size, args.trace, verbose);

	return 0;
}

