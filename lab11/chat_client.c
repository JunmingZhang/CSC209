#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#ifndef PORT
  #define PORT 30000
#endif
#define BUF_SIZE 128

int main(void) {
    // Create the socket FD.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("client: socket");
        exit(1);
    }

    // Set the IP and port of the server to connect to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) < 1) {
        perror("client: inet_pton");
        close(sock_fd);
        exit(1);
    }

    // Connect to the server.
    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("client: connect");
        close(sock_fd);
        exit(1);
    }

    // get the username
    char username_buf[BUF_SIZE + 1];
    
    printf("Please enter your username: \n");
    int num_name_read = read(STDIN_FILENO, username_buf, BUF_SIZE);
    if (num_name_read == -1) {
        perror("client: username");
        close(sock_fd);
        exit(1);
    }


    for(int username_newline = 0; username_newline < BUF_SIZE + 1; username_newline++) {
        if (username_buf[username_newline] == '\n' || username_newline == BUF_SIZE) {
            username_buf[username_newline] = '\0';
            break;
        } 
    }

    if ((write(sock_fd, username_buf, num_name_read)) != num_name_read) {
        perror("client: write username");
        close(sock_fd);
        exit(1);
    }

    // use for selecting, point max_fd to sock_fd
    // stdin/out is also a fd, can be added to FD_SET

    // file descriptor tips:
    // STDIN_FILENO: 0
    // STDOUT_FILENO: 1
    // STDERR_FILENO: 2
    int max_fd = sock_fd;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);
    FD_SET(STDIN_FILENO, &all_fds);

    // Read input from the user, send it to the server, and then accept the
    // echo that returns. Exit when stdin is closed.
    char buf[BUF_SIZE + 1];
    while (1) {

        // use for updating all_fds in a loop
        // all_fds must be updated in a loop
        fd_set listen_fds = all_fds;
        
        // select the file descriptor to operator
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (nready == -1) {
            perror("client: select");
            exit(1);
        }

        // if stdin is selected, send message to server
        if (FD_ISSET(STDIN_FILENO, &listen_fds)) {
            int num_read = read(STDIN_FILENO, buf, BUF_SIZE);
            if (num_read == 0) {
                break;
            }
            buf[num_read] = '\0';         

            int num_written = write(sock_fd, buf, num_read);
            if (num_written != num_read) {
                perror("client: write");
                close(sock_fd);
                exit(1);
            }
        }
        
        // if sock_fd is selected, receive message from server
        if (FD_ISSET(sock_fd, &listen_fds)) {
            int num_read = read(sock_fd, buf, BUF_SIZE);
            if (num_read == 0) {
                break;
            }
            buf[num_read] = '\0';
            printf("Received from server: %s", buf);
        }
    }

    close(sock_fd);
    return 0;
}
