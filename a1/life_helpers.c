#include <stdio.h>

void print_state(char *state, int size) {
    //print each character in one state
    for(int ind = 0; ind < size; ind++) {
        printf("%c", state[ind]);
    }
    printf("\n");
}

void  update_state(char *state, int size) {
    //array of update signs
    //each sign indicates '.' or 'X' for the next state
    //define the length of the array by size - 2 because we neglect
    //the beginning and the tail
    int update[size - 2];
    for(int ind = 1; ind < size - 1; ind++) {
        if (state[ind - 1] != state[ind + 1]) {
            // 1 means change to 'X'
            update[ind - 1] = 1;
        } else {
            //0 means change to '.'
            update[ind - 1] = 0;
        }
    }

    for(int check = 0; check < size - 2; check++) {
        //update the state by the signal
        if (update[check] == 1) {
            state[check + 1] = 'X';
        } else if (update[check] == 0) {
            state[check + 1] = '.';
        }
    }
}