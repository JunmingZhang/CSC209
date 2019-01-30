#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Constants that determine that address ranges of different memory regions

#define GLOBALS_START 0x400000
#define GLOBALS_END   0x700000
#define HEAP_START   0x4000000
#define HEAP_END     0x8000000
#define STACK_START 0xfff000000

int main(int argc, char **argv) {
    
    FILE *fp = NULL;

    if(argc == 1) {
        fp = stdin;

    } else if(argc == 2) {
        fp = fopen(argv[1], "r");
        if(fp == NULL){
            perror("fopen");
            exit(1);
        }
    } else {
        fprintf(stderr, "Usage: %s [tracefile]\n", argv[0]);
        exit(1);
    }

    /* Complete the implementation */
    
    //define reference and address to store info after reading
    //each line of the file
    char reference;
    unsigned long address;
    
    //count four kinds of references
    int I_ref = 0;
    int M_ref = 0;
    int L_ref = 0;
    int S_ref = 0;
    
    //count three areas of the memory
    int global = 0;
    int heap = 0;
    int stack = 0;
    
    //read and count the info needed
    while (fscanf(fp, "%c,%lx\n", &reference, &address) != EOF) {
        if (reference == 'I') { I_ref++; }
        if (reference == 'M') { M_ref++; }
        if (reference == 'L') { L_ref++; }
        if (reference == 'S') { S_ref++; }
        
        if (address >= GLOBALS_START && address <= GLOBALS_END && reference != 'I') { global++; }
        if (address >= HEAP_START && address <= HEAP_END) { heap++; }
        if (address >= STACK_START) { stack++; }
    }

    /* Use these print statements to print the ouput. It is important that 
     * the output match precisely for testing purposes.
     * Fill in the relevant variables in each print statement.
     * The print statements are commented out so that the program compiles.  
     * Uncomment them as you get each piece working.
     */

    //print the output
    printf("Reference Counts by Type:\n");
    printf("    Instructions: %d\n", I_ref);
    printf("    Modifications: %d\n", M_ref);
    printf("    Loads: %d\n", L_ref);
    printf("    Stores: %d\n", S_ref);
    printf("Data Reference Counts by Location:\n");
    printf("    Globals: %d\n", global);
    printf("    Heap: %d\n", heap);
    printf("    Stack: %d\n", stack);
    
    //close the file
    fclose(fp);

    //if there is no problem with the argument, and no other exceptions
    //program returns 0
    return 0;
}
