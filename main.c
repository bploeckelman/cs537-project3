/*
Main program for CS537 - Project 3 - RAID
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk-array.h"

#define DEBUG


disk_array_t disk_array = NULL;


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


// Command values -------------------------------------------------------------
typedef enum { INVALID, READ, WRITE, FAIL, RECOVER, END } ECommandType;
struct command {
    ECommandType cmd;
    int lba;
    int size;
    int value;
    int disk;
};
struct command cmd;


// Function declarations ------------------------------------------------------
int parseCmdLine(int argc, char *argv[]);
void parseLine(char *line);
void printCommand();
int processCommand();


// Entry point ----------------------------------------------------------------
int main( int argc, char *argv[] )
{
    // Parse the command line arguments
	if (parseCmdLine(argc, argv) == -1) {
		fprintf(stderr, "Usage: %s <-level> [0|10|4|5] <-strip> n <-disks> n <-size> n <-trace> traceFile [-verbose]\n", argv[0]);
		return 1;
    }

#ifdef DEBUG
    // Print parsed command line aguments
    printf("args: \n  level = %d\n  strip = %d\n  disks = %d\n  size = %d\n  trace = %s\n  verbose = %d\n",
        args.level, args.strip, args.disks, args.size, args.trace, verbose);
#endif

    // Create a new disk array
    const char *diskArrayName = "disk-array";
    disk_array = disk_array_create(diskArrayName, args.disks, args.size);
    if (disk_array == NULL) {
        fprintf(stderr, "Failed to create disk array.");
        return 1;
    }

    // Setup trace file descriptor
    FILE *fd = stdin;
#ifndef DEBUG
    if (args.trace == NULL) {
        fprintf(stderr, "Error: no trace file specified.\n");
        return 1;
    }
    // Try to open args.traceFile, switching fd from stdin to that file
    if ((fd = fopen(args.trace, "r")) == NULL) {
        char errString[1024];
        sprintf(errString, "Error: Unable to open trace file \"%s\"", args.trace);
        perror(errString);
        return 1;
    }
#endif

    // Parse each line from the input file
    char line[1000];
    printf("Enter command [ctrl-d to quit]: ");
    while (fgets(line, 1000, fd) != NULL) {
        parseLine(line);

#ifdef DEBUG
        printCommand();
#endif

        if (processCommand() != 0) {
            break;
        }

        printf("Enter command [ctrl-d to quit]: ");
    }

    // Print statistics
    disk_array_print_stats(disk_array);

    // Cleanup
    disk_array_close(disk_array);

	return 0;
}


// Parse Command Line Arguments -----------------------------------------------
// Note: this could be safer by using strol or bsd strtonum instead of atoi,
// but that seems excessive for a class project,  we could also use 
// getopt_long_only() instead of this, but this gets the job done as well
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

    return 0;
}


// Parse Command Line ---------------------------------------------------------
void parseLine(char *line) {
    // Clear command structure
    memset(&cmd, 0, sizeof(struct command));
    cmd.cmd = INVALID;
    strtok(line, "\n"); // hack to remove newline

    // Determine actual command
    char *tok = strtok(line, " ");
    if (strcmp(tok, "READ") == 0) {
        cmd.cmd = READ;
    } else if (strcmp(tok, "WRITE") == 0) {
        cmd.cmd = WRITE;
    } else if (strcmp(tok, "FAIL") == 0) {
        cmd.cmd = FAIL;
    } else if (strcmp(tok, "RECOVER") == 0) {
        cmd.cmd = RECOVER;
    } else if (strcmp(tok, "END") == 0) {
        cmd.cmd = END;
        return;
    } else {
        fprintf(stderr, "Invalid command: %s, expected READ, WRITE, FAIL, RECOVER, or END\n", tok);
        return;
    }

    // Get and store next tokens...
    int tokNum = 0;
    while (tok != NULL) {
        ++tokNum;
        tok = strtok(NULL, " ");

        // READ LBA SIZE
        if (cmd.cmd == READ) {
            switch (tokNum) {
                case 1: cmd.lba  = atoi(tok); break;
                case 2: cmd.size = atoi(tok); break;
            }
            if (tokNum == 2) break;
        }
        // WRITE LBA SIZE VALUE
        else if (cmd.cmd == WRITE) {
            switch (tokNum) {
                case 1: cmd.lba   = atoi(tok); break;
                case 2: cmd.size  = atoi(tok); break;
                case 3: cmd.value = atoi(tok); break;
            }
            if (tokNum == 3) break;
        }
        // FAIL DISK
        else if (cmd.cmd == FAIL) {
            cmd.disk = atoi(tok);
            break;
        }
        // RECOVER DISK
        else if (cmd.cmd == RECOVER) {
            cmd.disk = atoi(tok);
            break;
        }
        // Shouldn't get here...
        else {
            printf("Warning: unknown command, shouldn't get here\n");
            break;
        }
    } // end tokenizing
}


// Print command details (debug) ----------------------------------------------
void printCommand() {
    char *cmdString = "";
    switch (cmd.cmd) {
        case INVALID: cmdString = "Invalid"; break;
        case READ:    cmdString = "Read";    break;
        case WRITE:   cmdString = "Write";   break;
        case FAIL:    cmdString = "Fail";    break;
        case RECOVER: cmdString = "Recover"; break;
        case END:     cmdString = "End";     break;
        default:      cmdString = "Unknown"; break;
    }

    printf("Command:\n  cmd = %s\n  lba = %d\n  size = %d\n  value = %d\n  disk = %d\n",
        cmdString, cmd.lba, cmd.size, cmd.value, cmd.disk);
}


// Process the current command ------------------------------------------------
int processCommand() {
    switch (cmd.cmd) {
        case INVALID:
            fprintf(stderr, "Warning: unable to process invalid command\n");
        case READ:
            // TODO
            break;
        case WRITE:
            // TODO
            break;
        case FAIL:
            if (disk_array_fail_disk(disk_array, cmd.disk) == -1) {
                fprintf(stderr, "Problem failing disk #%d\n", cmd.disk);
            }
            break;
        case RECOVER:
            if (disk_array_recover_disk(disk_array, cmd.disk) == -1) {
                fprintf(stderr, "Problem recovering disk #%d\n", cmd.disk);
            }
            break;
        case END:
            return 1;
    }

    return 0;
}

