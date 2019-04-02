#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 58982
#endif
#define MAX_QUEUE 5


void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);

/* These are some of the function prototypes that we used in our solution 
 * You are not required to write functions that match these prototypes, but
 * you may find the helpful when thinking about operations in your program.
 */
/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf);
void announce_turn(struct game_state *game);
void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);


/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;

int name_check(char* name, struct client *player_list, int cur_fd) {
    if (strlen(name) > MAX_NAME) {
        free(name);
        char msg[] = "Your name is too long\r\n";
        if(write(cur_fd, msg, sizeof(msg)) == -1) {
            printf("User %d has disconnected\n", cur_fd);
            return 2;
        }
        return 1;
    }

    if (strcmp(name, "") == 0) {
        free(name);
        char msg[] = "Your name should not be empty\r\n";
        if(write(cur_fd, msg, sizeof(msg)) == -1) {
            printf("User %d has disconnected\n", cur_fd);
            return 2;
        }
        return 1;
    }
    struct client *player;
    
    for(player = player_list; player != NULL; player = player->next) {
        if (strcmp(player->name, name) == 0) {
            free(name);
            char msg[] = "This name has been\r\n";
            if (write(cur_fd, msg, sizeof(msg)) == -1) {
                printf("User %d has disconnected\n", cur_fd);
                return 2;
            }
            return 1;
        }
    }
    
    return 0;
}

int find_network_line(char* buf, int length) {
    for (int ind = 0; ind < length; ind++) {
        if (buf[ind] == '\n' && buf[ind - 1] == '\r') {
            return ind + 1;
        }
    }

    return -1;
}

char *read_msg(int fd, struct client *player) {
    char buf[MAX_BUF] = {'\0'};
    char* return_buf = malloc(sizeof(char) * MAX_BUF);

    if (return_buf == NULL) {
        perror("read_msg, malloc");
        exit(1);
    }

    int nbytes = read(fd, buf, MAX_BUF);
    if (nbytes == -1) {
        fprintf(stderr, "read at read_msg\n");
        exit(1);
    }

    printf("[%d] Read %d bytes\n", fd, nbytes);
    if (nbytes == 0) {
        printf("Disconnect from %s\n", inet_ntoa(player->ipaddr));
        remove_player(&player, fd);
        free(return_buf);
        return NULL;
    }

    int where = find_network_line(buf, MAX_BUF);
    buf[where - 2] = '\0';

    printf("[%d] Found newline %s\n", fd, buf);

    strncpy(return_buf, buf, MAX_BUF);
    return return_buf;
}

void announce_exit(char* name, struct client **game_head) {
    char msg[MAX_BUF];
    sprintf(msg, "Good bye %s", name);
    struct client **curr = game_head;

    while (*curr) {
        if (write((*curr)->fd, msg, strlen(msg)) == -1) {
            perror("write at anounce_exit");
            exit(1);
        }
        curr = &((*curr)->next);
    }
}

void broadcasting(char* msg, struct client **game_head) {
    struct client **curr = game_head;

    while (*curr) {
        if (write((*curr)->fd, msg, strlen(msg)) == -1) {
            printf("%s has left\n", (*curr)->name);
            remove_player(game_head, (*curr)->fd);
            announce_exit((*curr)->name, game_head);
        }
        curr = &((*curr)->next);
    }
}

/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));

    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

void take_turn(struct game_state *game) {
    if (!game->has_next_turn->next) {
        game->has_next_turn = game->head;
    } else if ((game->has_next_turn)->fd == -1) {
        while (game->has_next_turn && (game->has_next_turn)->fd == -1) {
            game->has_next_turn = (game->has_next_turn)->next;
        }
    } else {
        game->has_next_turn = (game->has_next_turn)->next;
    }
}

int is_valid(char* input) {
    if (strlen(input) > 1) {
        return 1;
    }

    char ch = *input;
    if (ch>='a' && ch<='z') {
        return 0;
    } else {
        return 1;
    }
}

char *is_win(char* sol, char guess) {
    return strchr(sol, guess);
}

int is_guessed(int* guessed, char guess) {
    int ind;
    for (ind = 0; ind < 26; ind++) {
        if (guess == ((char) 'a' + ind)) {
            break;
        }
    }

    return guessed[ind];
}

void announce_guessed(struct game_state *game, char guess) {
    struct client *head = game->head;
    struct client *turn = game->has_next_turn;

    char *msg = malloc(sizeof(char) * MAX_BUF);
    if (msg == NULL) {
        perror("malloc at announced_guessed");
        exit(1);
    }

    sprintf(msg, "%s guesses: %c\r\n", turn->name, guess);

    broadcasting(msg, &head);
    free(msg);
}

void announce_state(struct game_state *game) {
    struct client *head = game->head;

    char *update_msg = malloc(sizeof(char) * MAX_BUF);
    if (update_msg == NULL) {
        perror("malloc at announced_state");
        exit(1);
    }

    char *msg = status_message(update_msg, game);
    
    broadcasting(msg, &head);
    free(msg);
}

void announce_turn(struct game_state *game) {
    struct client *head = game->head;
    struct client *turn = game->has_next_turn;

    char my_msg[MAX_BUF];
    sprintf(my_msg, "Your guess?\r\n");

    char others_msg[MAX_BUF];
    sprintf(others_msg, "It's %s's turn.\r\n", turn->name);

    struct client* curr = head;
    while (curr) {
        if (strcmp(curr->name, turn->name) == 0) {
            if (write(curr->fd, my_msg, strlen(my_msg)) == -1) {
                perror("write at annoucne_turn\n");
                exit(1);
            }
        } else {
            if (write(curr->fd, others_msg, strlen(others_msg)) == -1) {
                perror("write at annoucne_turn\n");
                exit(1);
            }
        }
        curr = curr->next;
    }
}

void inform_new(struct game_state *game) {
    struct client *new = game->head;

    char *update_msg = malloc(sizeof(char) * MAX_BUF);
    if (update_msg == NULL) {
        perror("malloc at announced_state");
        exit(1);
    }

    char *state_msg = status_message(update_msg, game);
    if (write(new->fd, state_msg, strlen(state_msg)) == -1) {
        perror("write at inform_new\n");
        exit(1);
    }
    free(state_msg);
}

void reveal(struct game_state *game, char guess) {
    for (int ind = 0; ind < strlen(game->word); ind++) {
        if (game->word[ind] == guess) {
            game->guess[ind] = guess;
        }
    }
}

void add_guesses_left(struct game_state *game, char guess) {
    for (int ind = 0; ind < NUM_LETTERS; ind++) {
        char ch = (char) 'a' + ind;
        if (guess == ch) {
            game->letters_guessed[ind] = 1;
            break;
        }
    }
}

void announce_win(struct game_state *game) {
    char winner_msg[MAX_BUF];
    char others_msg[MAX_BUF];
    char all_msg[MAX_BUF];

    struct client *turn = game->has_next_turn;

    sprintf(winner_msg, "Game over! You win!\r\n");
    sprintf(others_msg, "Game over! %s won!\r\n", turn->name);
    sprintf(all_msg, "The word was %s.\r\n", game->word);

    printf("Game over! %s won!\n", turn->name);
    
    struct client* curr = game->head;
    while (curr) {
        if (write(curr->fd, all_msg, strlen(all_msg)) == -1) {
                perror("write at annoucne_win\n");
                exit(1);
        }

        if (strcmp(curr->name, turn->name) == 0) {
            if (write(curr->fd, winner_msg, strlen(winner_msg)) == -1) {
                perror("write at annoucne_win\n");
                exit(1);
            }
        } else {
            if (write(curr->fd, others_msg, strlen(others_msg)) == -1) {
                perror("write at annoucne_win\n");
                exit(1);
            }
        }
        curr = curr->next;
    }
}

void announce_fail(struct game_state *game) {
    char* msg = malloc(sizeof(char) * MAX_BUF);
    if (msg == NULL) {
        perror("malloc at announce fail\n");
        exit(1);
    }
    sprintf(msg, "No more guesses.  The word was %s\r\n", game->word);
    broadcasting(msg, &(game->head));
    printf("Evaluating for game_over\n");

    free(msg);
}

void restart(struct game_state *game, char* dict_name) {
    char* msg = malloc(sizeof(char) * MAX_BUF);
    if (msg == NULL) {
        perror("malloc at announce restart\n");
        exit(1);
    }
    sprintf(msg, "\r\nLet's start a new game.\r\n");
    broadcasting(msg, &(game->head));
    free(msg);

    init_game(game, dict_name);

    printf("New game\n");
    printf("It's %s's turn.\n", game->has_next_turn->name);

    announce_state(game);
    announce_turn(game);
}

int main(int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;
    
    if(argc != 2) {
        fprintf(stderr,"Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }
    
    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to 
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);
    
    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;
    
    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;
    
    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);
    
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);

            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            if(write(clientfd, greeting, strlen(greeting)) == -1) {
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&new_players, p->fd);
            };
        }
        
        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be 
         * valid.
         */
        int cur_fd;
        for(cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if(FD_ISSET(cur_fd, &rset)) {
                // Check if this socket descriptor is an active player
                for(p = game.head; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        //TODO - handle input from an active client
                        char *content = read_msg(cur_fd, p);
                        if (strlen(content) == 0) {
                            free(content);
                            continue;
                        }

                        if (strcmp(game.has_next_turn->name, p->name) == 0) {
                            if (is_valid(content) == 0) {
                                if (is_guessed(game.letters_guessed, *content)) {
                                    char msg[] = "This answer has been guessed!\r\n";
                                    if (write(cur_fd, msg, strlen(msg)) == -1) {
                                        fprintf(stderr, "Fail to tell %s he guessed an existed guess\n",  p->name);
                                        remove_player(&(game.head), p->fd);
                                    }

                                    char another_msg[] = "Your guess?\r\n";
                                    if (write(cur_fd, another_msg, strlen(another_msg)) == -1) {
                                        fprintf(stderr, "Fail to tell %s he to make a guess\n",  p->name);
                                        remove_player(&(game.head), p->fd);
                                    }
                                } else if (is_win(game.word, *content)) {
                                    reveal(&game, *content);
                                    if (strcmp(game.word, game.guess) == 0) {
                                        announce_win(&game);
                                        restart(&game, argv[1]);
                                        continue;
                                    }
                                    add_guesses_left(&game, *content);

                                    announce_guessed(&game, *content);
                                    announce_state(&game);
                                    announce_turn(&game);
                                } else {                                   
                                    char msg[MAX_BUF];
                                    sprintf(msg, "%c is not in the word\r\n", *content);
                                    if (write(cur_fd, msg, strlen(msg)) == -1) {
                                        fprintf(stderr, "Fail to tell %s it is not his turn\n",  p->name);
                                        remove_player(&(game.head), p->fd);
                                    }
                                    printf("%c is not in the word\n", *content);

                                    take_turn(&game);
                                    game.guesses_left--;
                                    if (game.guesses_left == 0) {
                                        announce_fail(&game);
                                        restart(&game, argv[1]);
                                        continue;
                                    }
                                    add_guesses_left(&game, *content);
                                    
                                    announce_guessed(&game, *content);
                                    announce_state(&game);
                                    announce_turn(&game);
                                }
                            } else {
                                char msg[] = "One lowercase character only!\r\n";
                                char another_msg[] = "Your guess?\r\n";
                
                                if (write(cur_fd, msg, strlen(msg)) == -1) {
                                    fprintf(stderr, "Fail to tell %s it is not his turn\n",  p->name);
                                    remove_player(&(game.head), p->fd);
                                }

                                if (write(cur_fd, another_msg, strlen(another_msg)) == -1) {
                                    fprintf(stderr, "Fail to tell %s to make a guess\n",  p->name);
                                    remove_player(&(game.head), p->fd);
                                }
                            }
                        } else {
                            char msg[] = "It is not your turn to guess.\r\n";
                            if (write(cur_fd, msg, strlen(msg)) == -1) {
                                fprintf(stderr, "Fail to tell %s it is not his turn\n",  p->name);
                                remove_player(&(game.head), p->fd);
                            }
                        }
                        printf("It's %s's turn.\n", game.has_next_turn->name);                       
                        free(content);
                    }
                }
        
                // Check if any new players are entering their names
                struct client *prev = NULL;
                for(p = new_players; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        // TODO - handle input from an new client who has
                        // not entered an acceptable name.
                        char *name = read_msg(cur_fd, p);
                        int check_ret = name_check(name, game.head, cur_fd);

                        if (check_ret == 1) {
                             continue;
                        } else if (check_ret == 2) {
                            remove_player(&new_players, cur_fd);
                        } else {
                            struct client temp;
                            (&temp)->next = p->next;

                            if (prev) {
                            prev->next = p->next;
                            } else {
                                new_players = new_players->next;
                            }

                            p->next = game.head;
                            game.head = p;
                            strncpy(game.head->name, name, MAX_NAME);
                            p = &temp;

                            char* msg = malloc(sizeof(char) * MAX_MSG);
                            if (msg == NULL) {
                                perror("malloc at processing new_players");
                                exit(1);
                            }

                            printf("%s has just joined.\n", game.head->name);
                            sprintf(msg, "%s has just joined.\r\n", game.head->name);
                            broadcasting(msg, &(game.head));

                            if (!game.has_next_turn) {
                                game.has_next_turn = game.head;

                                announce_state(&game);
                                announce_turn(&game);
                            } else {
                                inform_new(&game);
                                announce_turn(&game);
                            }
                            printf("It's %s's turn.\n", game.has_next_turn->name);
                            free(msg);
                        }
                        free(name);
                    } else {
                        prev = p;
                    }
                }
            }
        }
    }
    return 0;
}
