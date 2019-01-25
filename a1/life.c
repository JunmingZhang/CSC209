#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "life_helpers.c"


void print_state(char *state, int size);
void update_state(char *state, int size);


int main(int argc, char **argv) {

    if (argc != 3) {
    	fprintf(stderr, "Usage: USAGE: life initial n\n");
    	return 1;
    }

    int size = strlen(argv[1]);

    //extract the first argument, initial state and the number of
    //states to predict
    char *state = argv[1];
    int total = strtol(argv[2], NULL, 10);

    for (int state_num = 0; state_num < total; state_num++) {
        print_state(state, size);
        //update the state to the next one after printing the
        // current state
        update_state(state, size);
    }
    
    return 0;
}