#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  Write the main() function of a program that takes exactly two arguments,
  and prints one of the following:
    - "Same\n" if the arguments are the same.
    - "Different\n" if the arguments are different.
    - "Invalid\n" if the program is called with an incorrect number of
      arguments.

  Your main function should return 0, regardless of what is printed.
*/
int main (int argc, char **argv) {
  if (argc != 3) {
    printf("Invalid\n");
  } else {
    char *str1 = argv[1];
    char *str2 = argv[2];
    if (strcmp(str1, str2) == 0) {
      printf("Same\n");
    } else {
      printf("Different\n");
    }
  }
  return 0;
}
