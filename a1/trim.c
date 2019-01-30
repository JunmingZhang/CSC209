#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Reads a trace file produced by valgrind and an address marker file produced
 * by the program being traced. Outputs only the memory reference lines in
 * between the two markers
 */

int main(int argc, char **argv) {
    
    if(argc != 3) {
         fprintf(stderr, "Usage: %s tracefile markerfile\n", argv[0]);
         exit(1);
    }

    // Addresses should be stored in unsigned long variables
    unsigned long start_marker, end_marker;
    
    FILE *trace_fp;
    FILE *marker_fp;
    trace_fp = fopen (argv[1], "r");
    marker_fp = fopen(argv[2], "r");
    
    fscanf(marker_fp, "%lx %lx", &start_marker, &end_marker);

    /* For printing output, use this exact formatting string where the
     * first conversion is for the type of memory reference, and the second
     * is the address
     */
    
    //define reference, address and size variable to store info in each line
    char reference;
    unsigned long address;
    int size;
    
    //set a signal to determine whether to print
    //start == 1, print
    //start == 0, do not print
    int start = 0;
    
    //scan the file input
    while (fscanf(trace_fp, " %c %lx, %d", &reference, &address, &size) != EOF) {
        //if the end marker is read, stop printing
        if (address == end_marker) {
            start = 0;
        }
        
        //print if start signal is on (start == 1)
        if (start == 1) {
            printf("%c,%#lx\n", reference, address);
        }
        
        //set start to 1 if the start_marker is read
        if (address == start_marker) {
            start = 1;
        }
    }
    
    //close the file
    fclose(trace_fp);
    fclose(marker_fp);

    //if there is no problem with the argument, and no other exceptions
    //program returns 0
    return 0;
}
