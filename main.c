/*
Main program for CS537 - Project 3 - RAID
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
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
    char value[4];
    int disk;
};
struct command cmd;


// Function declarations ------------------------------------------------------
int parseCmdLine(int argc, char *argv[]);
void parseLine(char *line);
void printCommand();
int processCommand();
void readCommand();
void writeCommand();
void recoverCommand();


// Entry point ----------------------------------------------------------------
int main( int argc, char *argv[] )
{
    // Parse the command line arguments
    if (parseCmdLine(argc, argv) == -1) {
        fprintf(stderr, "Usage: %s <-level> [0|10|4|5] <-strip> n <-disks> n <-size> n <-trace> traceFile [-verbose]\n", argv[0]);
        return 1;
    }

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
#ifdef DEBUG
    printf("Enter command [ctrl-d to quit]: ");
#endif
    char line[1000];
    while (fgets(line, 1000, fd) != NULL) {
        parseLine(line);
        if (processCommand() != 0) {
            break;
        }
#ifdef DEBUG
        printf("Enter command [ctrl-d to quit]: ");
#endif
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
             if (strcmp(argv[i], "-level")   == 0) args.level = atoi(argv[++i]);
        else if (strcmp(argv[i], "-strip")   == 0) args.strip = atoi(argv[++i]);
        else if (strcmp(argv[i], "-disks")   == 0) args.disks = atoi(argv[++i]);
        else if (strcmp(argv[i], "-size")    == 0) args.size  = atoi(argv[++i]);
        else if (strcmp(argv[i], "-trace")   == 0) args.trace = argv[++i];
        else if (strcmp(argv[i], "-verbose") == 0) verbose = 1;
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

#ifdef DEBUG
    // Print parsed command line aguments
    printf("args: \n  level = %d\n  strip = %d\n  disks = %d\n  size = %d\n  trace = %s\n  verbose = %d\n",
        args.level, args.strip, args.disks, args.size, args.trace, verbose);
#endif


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
         if (strcmp(tok, "READ")    == 0) cmd.cmd = READ;
    else if (strcmp(tok, "WRITE")   == 0) cmd.cmd = WRITE;
    else if (strcmp(tok, "FAIL")    == 0) cmd.cmd = FAIL;
    else if (strcmp(tok, "RECOVER") == 0) cmd.cmd = RECOVER;
    else if (strcmp(tok, "END")     == 0) {
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
                case 3: memcpy(cmd.value, tok, 4); break;
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

#ifdef DEBUG
        printCommand();
#endif
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

    printf("Command:\n  cmd = %s\n  lba = %d\n  size = %d\n  value = %c%c%c%c\n  disk = %d\n",
        cmdString, cmd.lba, cmd.size, cmd.value[0], cmd.value[1], cmd.value[2], cmd.value[3], cmd.disk);
}


// Process the current command ------------------------------------------------
int processCommand() {
    switch (cmd.cmd) {
        case INVALID: fprintf(stderr, "Warning: Invalid command\n"); break;
        case READ:    readCommand();    break;
        case WRITE:   writeCommand();   break;
        case RECOVER: recoverCommand(); break;
        case END:     return 1;
        case FAIL:
            if (disk_array_fail_disk(disk_array, cmd.disk) == -1) {
                fprintf(stderr, "Problem failing disk #%d\n", cmd.disk);
            }
            break;
    }

    return 0;
}


// ----------------------------------------------------------------------------
// RAID Read functions --------------------------------------------------------
// ----------------------------------------------------------------------------


// Read RAID 0 : striped data -------------------------------------------------
void readRaid0() {
    int lba = cmd.lba;
    int blocksToRead = cmd.size;
    int blocksPerStripe = args.strip;

    while (blocksToRead-- > 0) {
        int disk  = (lba / blocksPerStripe) % args.disks;
        int block = (lba % blocksPerStripe) + ((lba / blocksPerStripe) / args.disks) * blocksPerStripe;

        char readBuffer[BLOCK_SIZE];
        if (disk_array_read(disk_array, disk, block, readBuffer) == -1) {
#ifdef DEBUG
            fprintf(stderr, "Error: Failed to read block %d from disk %d\n", block, disk);
#endif
            // Print ERROR instead of first 4 btyes
            printf("ERROR ");
        } else {
            // Print first 4 bytes from block that was read 
            printf("%c%c%c%c ", readBuffer[0], readBuffer[1], readBuffer[2], readBuffer[3]);
        }
        
        ++lba;
    }
    puts("");
}

// Read RAID 10 : striped and mirrored data ------------------------------------
void readRaid10() {
    // TODO
}

// Read RAID 4 : striped on disks 0..(n-1), parity on disk n -------------------
void readRaid4() {
    // TODO
}

// Read RAID 5 : striped, with parity on different disk for each stripe --------
void readRaid5() {
    // TODO
}


// RAID Read Command -----------------------------------------------------------
void readCommand() {
    if (cmd.cmd != READ) return;

    // READ LBA SIZE
    switch (args.level) {
        case 0:  readRaid0();  break;
        case 10: readRaid10(); break;
        case 4:  readRaid4();  break;
        case 5:  readRaid5();  break;
        default: fprintf(stderr, "Warning: attempted to read using invalid raid level\n");
    }
}


// ----------------------------------------------------------------------------
// RAID Write functions -------------------------------------------------------
// ----------------------------------------------------------------------------


// Write RAID 0 : striped data ------------------------------------------------
void writeRaid0() {
    int lba = cmd.lba;
    int blocksToWrite = cmd.size;
    int blocksPerStripe = args.strip;

    while (blocksToWrite-- > 0) {
        int disk  = (lba / blocksPerStripe) % args.disks;
        int block = (lba % blocksPerStripe) + ((lba / blocksPerStripe) / args.disks) * blocksPerStripe;

        // Fill write buffer with the repeated 4 byte pattern from value
        char writeBuffer[BLOCK_SIZE];
        memset(writeBuffer, 0, BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE; ++i) {
            writeBuffer[i] = cmd.value[i % 4];
        }

        if (disk_array_write(disk_array, disk, block, writeBuffer) == -1) {
            fprintf(stderr, "Error: Failed to write block %d to disk %d\n", block, disk);
        }

#ifdef DEBUG
        // Print first 4 bytes from block that was written 
        printf("%c%c%c%c ", writeBuffer[0], writeBuffer[1], writeBuffer[2], writeBuffer[3]);
#endif
        
        ++lba;
    }
    puts("");
}

// Write RAID 10 : striped and mirrored data -----------------------------------
void writeRaid10() {
    // TODO
}

// Write RAID 4 : striped on disks 0..(n-1), parity on disk n ------------------
void writeRaid4() {
    // TODO
}

// Write RAID 5 : striped, with parity on different disk for each stripe -------
void writeRaid5() {
    // TODO
}


// Write command ---------------------------------------------------------------
void writeCommand() {
    if (cmd.cmd != WRITE) return;

    // WRITE LBA SIZE VALUE
    switch (args.level) {
        case 0:  writeRaid0();  break;
        case 10: writeRaid10(); break;
        case 4:  writeRaid4();  break;
        case 5:  writeRaid5();  break;
        default: fprintf(stderr, "Warning: attempted to write using invalid raid level\n");
    }
}


// -----------------------------------------------------------------------------
// RAID Recover functions ------------------------------------------------------
// -----------------------------------------------------------------------------


// Recover RAID 0 : striped data -----------------------------------------------
void recoverRaid0() {
    // uhh... can't really recover a failed disk's data in raid 0
    printf("Error: cannot recover data from disk #%d in RAID 0 configuration\n", cmd.disk);
}

// Recover RAID 10 : striped and mirrored data ---------------------------------
void recoverRaid10() {

}

// Recover RAID 4 : striped on disks 0..(n-1), parity on disk n ----------------
void recoverRaid4() {

}

// Recover RAID 5 : striped, with parity on different disk for each stripe -----
void recoverRaid5() {

}


// Recover command -------------------------------------------------------------
void recoverCommand() {
    if (cmd.cmd != RECOVER) return;

    // Zero out the specified disk in preparation for recovery
    if (disk_array_recover_disk(disk_array, cmd.disk) == -1) {
        fprintf(stderr, "Problem recovering disk #%d\n", cmd.disk);
    }

    switch (args.level) {
        case 0:  recoverRaid0();  break;
        case 10: recoverRaid10(); break;
        case 4:  recoverRaid4();  break;
        case 5:  recoverRaid5();  break;
        default: fprintf(stderr, "Warning: attempted to recover using invalid raid level\n");
    }
}

