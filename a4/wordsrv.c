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


/* my own helpers */
int name_check(char* name, struct client *player_list, int cur_fd);
int find_network_line(char* buf, int length); // from lab 10
char *read_msg(int fd, struct client **player); // from lab 10
void announce_exit(char* name, struct game_state *game);
void broadcasting(char* msg, struct client **game_head, struct game_state *game);

struct client** process_exit(struct game_state *game, struct client **curr, struct client** player_list, int from_new);
void take_turn(struct game_state *game);
int is_turn(struct client *turn, struct client *player);
int is_valid(char* input);
char *is_win(char* sol, char guess);

int is_guessed(int* guessed, char guess);
void announce_guessed(struct game_state *game, char guess);
void announce_state(struct game_state *game);
void announce_next_turn(struct game_state *game, struct client** player_list);
void inform_new(struct game_state *game, struct client** player_list);

void reveal(struct game_state *game, char guess);
void add_guesses_left(struct game_state *game, char guess);
void announce_win(struct game_state *game, struct client **player_list);
void announce_fail(struct game_state *game, struct client **player_list);
void restart(struct game_state *game, struct client **player_list, char* dict_name);

/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;

/*
 * check whether the name given is valid
 * return 0, the name is valid
 * return 1, the name is invalid
 * return 2, the user in the player_list loses connection (write returns -1)
 */
int name_check(char* name, struct client *player_list, int cur_fd) {
    // the name is invalid if it is too long
    if (strlen(name) > MAX_NAME) {
        printf("Name %s is too long\n", name);
        free(name);

        char msg[] = "Your name is too long\r\nPlese change a new name:\r\n";
        if (write(cur_fd, msg, sizeof(msg)) == -1) {
            return 2;
        }
        return 1;
    }

    // the name is invalid if it is an empty string
    if (strcmp(name, "") == 0) {
        printf("Provided name is empty\n");
        free(name);

        char msg[] = "Your name should not be empty\r\nPlese change a new name:\r\n";
        if (write(cur_fd, msg, sizeof(msg)) == -1) {
            return 2;
        }
        return 1;
    }

    // check all names in current players
    // the name is invalid if it is used by some player in the game
    struct client *player;
    for(player = player_list; player != NULL; player = player->next) {
        if (strcmp(player->name, name) == 0) {
            printf("Name %s is has been used\n", name);
            free(name);

            char msg[] = "This name has been used\r\nPlese change a new name:\r\n";
            if (write(cur_fd, msg, sizeof(msg)) == -1) {
                return 2;
            }
            return 1;
        }
    }
    
    return 0;
}

/* find the index of "/r/n" in the given string from network (from lab 10) */
int find_network_line(char* buf, int length) {
    for (int ind = 0; ind < length; ind++) {
        if (buf[ind] == '\n' && buf[ind - 1] == '\r') {
            return ind + 1;
        }
    }

    return -1;
}

/*
 * read messages from the given message read from network
 * and return the message by cancelling off "\r\n" or NULL
 * if there is a disconnection (read 0 bytes from the file descriptor)
 */
char *read_msg(int fd, struct client **player) {
    // initialize the buf to accept the message and
    // malloc space for the message to be returned
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
    // if 0 bytes are read out, then someone loses the connection
    if (nbytes == 0) {
        printf("Disconnect from %s\n", inet_ntoa((*player)->ipaddr));
        free(return_buf);
        return NULL;
    }

    // cancel off "\r\n", network newline
    int where = find_network_line(buf, MAX_BUF);
    buf[where - 2] = '\0';

    printf("[%d] Found newline %s\n", fd, buf);

    strncpy(return_buf, buf, MAX_BUF);
    return return_buf;
}

/* announce to all players in the game that someone has exited */
void announce_exit(char* name, struct game_state *game) {
    char* msg = malloc(sizeof(char) * MAX_BUF);
    if (!msg) {
        perror("malloc at announce_exit\n");
        exit(1);
    }

    sprintf(msg, "Good bye %s\r\n", name);

    broadcasting(msg, &(game->head), game);
    free(msg);
}

/* broadcast the given message to all players */
void broadcasting(char* msg, struct client **game_head, struct game_state *game) {
    struct client **curr = game_head;

    while (*curr) {
        if (write((*curr)->fd, msg, strlen(msg)) == -1) {
            curr = process_exit(game, curr, game_head, 0);
        }
        curr = &((*curr)->next);
    }
}

/*
 * process all cases of disconnection,
 * include disconnection during reading and writing,
 * and disconnection of the client from new_list or players in the game
 * curr -> pointer to the client loses connection
 * player_list -> new_list or game.head (players in the game)
 * from_new = 0 -> from game.head; from_new = 1 -> from new_players
 * return the pointer to the struct which holds the next client of
 * the disconnected client
 */
struct client** process_exit(struct game_state *game, struct client **curr, struct client** player_list, int from_new) {
    // struct pointer used to store the pointer to the next client of the
    // client who has lost the connection, use this pointer, the loop in the main
    // block can work normally
    struct client del_temp;
    struct client *del_temp_ptr = &del_temp;
    struct client **del_ptr_ptr = &del_temp_ptr;

   // get the name of the disconnected client
   char *name = malloc(sizeof(char) * strlen((*curr)->name));
    if (!name) {
        perror("malloct at process exit");
        exit(1);
    }

    strncpy(name, (*curr)->name, strlen((*curr)->name));

    // store the next pointer of the disconnected client to the del_tem pointer
    del_temp_ptr->next = (*curr)->next;

    // if the player is from new_list, print the folloing info
    // and remove him from the new_players
    if (from_new == 1) {
        printf("%d has disconnected\n", (*curr)->fd);
        remove_player(player_list, (*curr)->fd);
    }

    // if the player is in the game
    if (from_new == 0) {
        // find and copy the name of the head of the clients in the game->head
        char head_name[MAX_NAME];
        strncpy(head_name, game->head->name, strlen(game->head->name));

        // find and copy the name of the client of the turn
        char turn_name[MAX_NAME];
        strncpy(turn_name, game->has_next_turn->name, strlen(game->has_next_turn->name));

        struct client *head_next = game->head->next;

        if (strcmp(name, turn_name) == 0) {
            take_turn(game);
        }

        // check if there is only one player in the game
        int single = (game->has_next_turn->next == NULL && game->head->next == NULL);

        remove_player(&(game->head), (*curr)->fd);

        if (strcmp(head_name, name) == 0) {
            game->head = head_next;
        }

        if (single == 1) {
            game->head = NULL;
            game->has_next_turn = NULL;
        }

        printf("%s has left!\n", name);
        announce_exit(name, game);
        if (from_new == 0 && game->head) {
            announce_next_turn(game, &(game->head));
        }
    }

    free(name);
    return del_ptr_ptr;
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
    } else {
        game->has_next_turn = (game->has_next_turn)->next;
    }
}

int is_turn(struct client *turn, struct client *player) {
    return strcmp(turn->name, player->name);
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

    broadcasting(msg, &head, game);
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
    
    broadcasting(msg, &head, game);
    free(msg);
}

void announce_next_turn(struct game_state *game, struct client** player_list) {
    struct client *head = game->head;
    struct client *turn = game->has_next_turn;

    char my_msg[MAX_BUF];
    sprintf(my_msg, "Your guess?\r\n");

    char others_msg[MAX_BUF];
    sprintf(others_msg, "It's %s's turn.\r\n", turn->name);

    struct client* curr = head;
    while (curr) {
        if (is_turn(turn, curr) == 0) {
            if (write(curr->fd, my_msg, strlen(my_msg)) == -1) {
                curr = *(process_exit(game, &curr, player_list, 0));
            }
        } else {
            if (write(curr->fd, others_msg, strlen(others_msg)) == -1) {
                curr = *(process_exit(game, &curr, player_list, 0));
            }
        }
        curr = curr->next;
    }
}

void inform_new(struct game_state *game, struct client** player_list) {
    struct client *new = game->head;

    char *update_msg = malloc(sizeof(char) * MAX_BUF);
    if (update_msg == NULL) {
        perror("malloc at announced_state");
        exit(1);
    }

    char *state_msg = status_message(update_msg, game);
    if (write(new->fd, state_msg, strlen(state_msg)) == -1) {
        game->head = (*(process_exit(game, &new, player_list, 0)))->next;
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

void announce_win(struct game_state *game, struct client **player_list) {
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
            curr = *(process_exit(game, &curr, player_list, 0));
        }

        if (is_turn(turn, curr) == 0) {
            if (write(curr->fd, winner_msg, strlen(winner_msg)) == -1) {
                curr = *(process_exit(game, &curr, player_list, 0));
            }
        } else {
            if (write(curr->fd, others_msg, strlen(others_msg)) == -1) {
                curr = *(process_exit(game, &curr, player_list, 0));
            }
        }
        curr = curr->next;
    }
}

void announce_fail(struct game_state *game, struct client **player_list) {
    char* msg = malloc(sizeof(char) * MAX_BUF);
    if (msg == NULL) {
        perror("malloc at announce fail\n");
        exit(1);
    }
    sprintf(msg, "No more guesses.  The word was %s\r\n", game->word);
    broadcasting(msg, &(game->head), game);
    printf("Evaluating for game_over\n");

    free(msg);
}

void restart(struct game_state *game, struct client **player_list, char* dict_name) {
    char* msg = malloc(sizeof(char) * MAX_BUF);
    if (msg == NULL) {
        perror("malloc at announce restart\n");
        exit(1);
    }

    init_game(game, dict_name);

    sprintf(msg, "\r\nLet's start a new game.\r\n");
    broadcasting(msg, &(game->head), game);
    free(msg);

    printf("New game\n");
    printf("It's %s's turn.\n", game->has_next_turn->name);

    announce_state(game);
    announce_next_turn(game, player_list);
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
                        char *content = read_msg(cur_fd, &(game.head));

                        if (content == NULL) {
                            struct client **del_ptr = process_exit(&game, &p, &(game.head), 0);
                            p = *del_ptr;             
                            continue;
                        }

                        if (strlen(content) == 0) {
                            free(content);
                            continue;
                        }

                        if (is_turn(game.has_next_turn, p) == 0) {
                            if (is_valid(content) == 0) {
                                if (is_guessed(game.letters_guessed, *content)) {
                                    printf("Answer %c has been guessed!\n", *content);
                                    char msg[] = "This answer has been guessed!\r\n";

                                    if (write(cur_fd, msg, strlen(msg)) == -1) {                                      
                                        p = *(process_exit(&game, &p, &(game.head), 0));
                                        free(content);
                                        continue;
                                    }

                                    char another_msg[] = "Your guess?\r\n";
                                    if (write(cur_fd, another_msg, strlen(another_msg)) == -1) {
                                        p = *(process_exit(&game, &p, &(game.head), 0));
                                        free(content);
                                        continue;
                                    }
                                } else if (is_win(game.word, *content)) {
                                    printf("Given letter %c is in the word\n", *content);
                                    reveal(&game, *content);

                                    if (strcmp(game.word, game.guess) == 0) {
                                        announce_win(&game, &(game.head));
                                        restart(&game, &(game.head), argv[1]);
                                    }
                                    add_guesses_left(&game, *content);

                                    announce_guessed(&game, *content);
                                    announce_state(&game);
                                    announce_next_turn(&game, &(game.head));
                                } else {                                   
                                    char msg[MAX_BUF];
                                    sprintf(msg, "%c is not in the word\r\n", *content);
                                    if (write(cur_fd, msg, strlen(msg)) == -1) {
                                        p = *(process_exit(&game, &p, &(game.head), 0));
                                        free(content);
                                        continue;
                                    }
                                    printf("%c is not in the word\n", *content);

                                    take_turn(&game);
                                    game.guesses_left--;
                                    if (game.guesses_left == 0) {
                                        announce_fail(&game, &(game.head));
                                        restart(&game, &(game.head), argv[1]);
                                    }
                                    add_guesses_left(&game, *content);
                                    
                                    announce_guessed(&game, *content);
                                    announce_state(&game);
                                    announce_next_turn(&game, &(game.head));
                                }
                            } else {
                                printf("Stop %s from entering the invalid answer, %s\n.", game.has_next_turn->name, content);

                                char msg[] = "One lowercase character only!\r\n";
                                char another_msg[] = "Your guess?\r\n";
                
                                if (write(cur_fd, msg, strlen(msg)) == -1) {
                                    p = *(process_exit(&game, &p, &(game.head), 0));
                                    free(content);
                                    continue;
                                }

                                if (write(cur_fd, another_msg, strlen(another_msg)) == -1) {
                                    p = *(process_exit(&game, &p, &(game.head), 0));
                                    free(content);
                                    continue;
                                }
                            }
                        } else {
                            printf("Stop %s from moving in wrong turn.\n", p->name);

                            char msg[] = "It is not your turn to guess.\r\n";
                            if (write(cur_fd, msg, strlen(msg)) == -1) {
                                p = *(process_exit(&game, &p, &(game.head), 0));
                                free(content);
                                continue;
                            }
                        }                     
                        free(content);
                    }
                }
        
                // Check if any new players are entering their names
                struct client *prev = NULL;
                for(p = new_players; p != NULL; p = p->next) {
                    if(cur_fd == p->fd) {
                        // TODO - handle input from an new client who has
                        // not entered an acceptable name.

                        char *name = read_msg(cur_fd, &(new_players));
                        if (name == NULL) {
                            p = *(process_exit(&game, &p, &new_players, 1));
                            continue;
                        }

                        int check_ret = name_check(name, game.head, cur_fd);

                        if (check_ret == 1) {
                             continue;
                        } else if (check_ret == 2) {
                            p = *(process_exit(&game, &p, &new_players, 1));
                            free(name);
                            continue;
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
                            broadcasting(msg, &(game.head), &(game));

                            if (!game.has_next_turn) {
                                game.has_next_turn = game.head;

                                announce_state(&game);
                                announce_next_turn(&game, &(game.head));
                            } else {
                                inform_new(&game, &(game.head));
                                announce_next_turn(&game, &(game.head));
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
