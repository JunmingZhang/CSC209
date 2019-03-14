/* The purpose of this program is to practice writing signal handling
 * functions and observing the behaviour of signals.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

/* Message to print in the signal handling function. */
#define MESSAGE "%ld reads were done in %ld seconds.\n"

/* Global variables to store number of read operations and seconds elapsed. 
 */
long num_reads, seconds;

void handler(int code) {
  fprintf(stdout, MESSAGE, num_reads, seconds);
  exit(0);
}


/* The first command-line argument is the number of seconds to set a timer to run.
 * The second argument is the name of a binary file containing 100 ints.
 * Assume both of these arguments are correct.
 */

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: time_reads s filename\n");
        exit(1);
    }
    seconds = strtol(argv[1], NULL, 10);

    FILE *fp;
    if ((fp = fopen(argv[2], "r")) == NULL) {
      perror("fopen");
      exit(1);
    }
    
    // Declare a struct to be used by the sigaction function:
    struct sigaction newact;

    // Specify that we want the handler function to handle the
    // signal:
    newact.sa_handler = handler;

    // Use default flags:
    newact.sa_flags = 0;

    // Specify that we don't want any signals to be blocked during
    // execution of handler:
    sigemptyset(&newact.sa_mask);

    // Modify the signal table so that handler is called when
    // signal SIGPROF is received:
    sigaction(SIGPROF, &newact, NULL);

    //setup the timer struct
    struct itimerval timeact;

    //set current count down seconds to be seconds input, other to be 0
    //next micro seconds
    timeact.it_interval.tv_usec = 0;

    //next seconds
    timeact.it_interval.tv_sec = 0;

    //current micro seconds
    timeact.it_value.tv_usec = 0;

    //current seconds
    timeact.it_value.tv_sec = seconds;

    //set up the timer to send the signal when timer stops
    if(setitimer(ITIMER_PROF, &timeact, NULL) == -1) {
      perror("setitimer");
      exit(1);
    }

    /* In an infinite loop, read an int from a random location in the file,
     * and print it to stderr.
     */

    num_reads = 0;

    for (;;) {
      int rand_loc = random() % 100;
      int num;
      fseek(fp, sizeof(int) * rand_loc, SEEK_SET);
      fread(&num, sizeof(int), 1, fp);
      fprintf(stderr, "%d\n", num);
      num_reads++;
    }
    return 1; // something is wrong if we ever get here!
}
