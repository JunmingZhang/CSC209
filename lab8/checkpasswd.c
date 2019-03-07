#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXLINE 256
#define MAX_PASSWORD 10

#define SUCCESS "Password verified\n"
#define INVALID "Invalid password\n"
#define NO_USER "No such user\n"

int main(void) {
  char user_id[MAXLINE];
  char password[MAXLINE];

  if(fgets(user_id, MAXLINE, stdin) == NULL) {
      perror("fgets");
      exit(1);
  }
  if(fgets(password, MAXLINE, stdin) == NULL) {
      perror("fgets");
      exit(1);
  }
  
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipeid");
    exit(1);
  }
  
  int result = fork();
  
  if (result == 0) {
    
    if (close(pipefd[1]) == -1) {
        perror("write closed abnormally at child");
        exit(1);
    }
    
    if (dup2(pipefd[0], fileno(stdin)) == -1) {
      perror("dup2");
      exit(1);
    }
    
    if (close(pipefd[0]) == -1) {
      perror("read closed abnormally at child");
      exit(1);
    }
    
    execl("./validate", "validate", NULL);
    perror("execl");
    exit(1);
  }
  if (close(pipefd[0]) == -1) {
    perror("read closed abnormally at parent");
    exit(1);
  }
  
  if (write(pipefd[1], user_id, MAX_PASSWORD) == -1) {
    perror("write does not work properly at parent");
    exit(1);
  }
  
  if (write(pipefd[1], password, MAX_PASSWORD) == -1) {
    perror("write does not work properly at parent");
    exit(1);
  }
  
  if (close(pipefd[1]) == -1) {
    perror("write closed abnormally at parent");
    exit(1);
  }
  
  int status;
  
  if (wait(&status) == -1) {
    perror("wait");
    exit(1);
  } else if (WIFEXITED(status)) {
    if (WEXITSTATUS(status) == 0) {
      printf("%s", SUCCESS);
    } else if (WEXITSTATUS(status) == 1) {
      printf("error");
      exit(1);
    } else if (WEXITSTATUS(status) == 2) {
      printf("%s", INVALID);
      exit(2);
    } else if (WEXITSTATUS(status) == 3) {
        printf("%s", NO_USER);
        exit(3);
    }
  }
  return 0;
}
