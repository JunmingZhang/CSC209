#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "family.h"

/* Number of word pointers allocated for a new family.
   This is also the number of word pointers added to a family
   using realloc, when the family is full.
*/
static int family_increment = 0;


/* Set family_increment to size, and initialize random number generator.
   The random number generator is used to select a random word from a family.
   This function should be called exactly once, on startup.
*/
void init_family(int size) {
    family_increment = size;
    srand(time(0));
}


/* Given a pointer to the head of a linked list of Family nodes,
   print each family's signature and words.

   Do not modify this function. It will be used for marking.
*/
void print_families(Family* fam_list) {
    int i;
    Family *fam = fam_list;
    
    while (fam) {
        printf("***Family signature: %s Num words: %d\n",
               fam->signature, fam->num_words);
        for(i = 0; i < fam->num_words; i++) {
            printf("     %s\n", fam->word_ptrs[i]);
        }
        printf("\n");
        fam = fam->next;
    }
}


/* Return a pointer to a new family whose signature is 
   a copy of str. Initialize word_ptrs to point to 
   family_increment+1 pointers, numwords to 0, 
   maxwords to family_increment, and next to NULL.
*/
Family *new_family(char *str) {
    /* initialize a struct of family list with the requirement above
       return a pointer to the struct 
    */

    //initialize the node,
    //allocate space for the family node, do error checking for malloc
    Family *fam_list = malloc(sizeof(Family));
    if (fam_list == NULL) {
        perror("invalid, func new_family, fam_list");
        exit(1);
    }

    //set the input signature as the signature of the node,
    //allocate space for the signature of the family node, do error checking for malloc
    fam_list->signature = malloc(strlen(str) + 1);
    if (fam_list->signature == NULL) {
        perror("invalid, func new_family, fam_list->signature");
        exit(1);
    }
    strcpy(fam_list->signature, str);

    //initialize word pointers,
    //allocate space for the word pointers of the family node, do error checking for malloc
    fam_list->word_ptrs = malloc((family_increment + 1) * sizeof(char*));
    if (fam_list->word_ptrs == NULL) {
        perror("invalid, func new_family, fam_list->word_ptrs");
        exit(1);
    }

    //initialize num_words, max_words and next node with the requirement above
    fam_list->num_words = 0;
    fam_list->max_words = family_increment;
    fam_list->next = NULL;
    return fam_list;
}


/* Add word to the next free slot fam->word_ptrs.
   If fam->word_ptrs is full, first use realloc to allocate family_increment
   more pointers and then add the new pointer.
*/
void add_word_to_family(Family *fam, char *word) {
    //error checking, function works if the input fam and string word are not NULL
    if (fam != NULL && word != NULL) {
        //if # of contents of input word array reaches max, enlarge its capacity,
        //do error checking after enlarging
        if (fam->num_words == fam->max_words) {
            fam->max_words += family_increment;
            fam->word_ptrs = realloc(fam->word_ptrs, (fam->max_words + 1) * sizeof(char*));
            if (fam->word_ptrs == NULL) {
                perror("invalid, func add_word_to_family, fam->word_ptrs");
                exit(1);
            }
        }
        //insert the word into the word array
        (fam->word_ptrs)[fam->num_words] = word;
        (fam->word_ptrs)[fam->num_words + 1] = NULL;
        fam->num_words++;
    }
}


/* Return a pointer to the family whose signature is sig;
   if there is no such family, return NULL.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_family(Family *fam_list, char *sig) {
    Family *curr = fam_list;
    //search for the curr if the end of the linked list is not reached
    //and the family owns the signature is not found
    while(curr && (strcmp(curr->signature, sig) != 0)) {
        curr = curr->next;
    }
    
    return curr;
}


/* Return a pointer to the family in the list with the most words;
   if the list is empty, return NULL. If multiple families have the most words,
   return a pointer to any of them.
   fam_list is a pointer to the head of a list of Family nodes.
*/
Family *find_biggest_family(Family *fam_list) {
    Family *curr = fam_list;
    int max_count = 0;
    Family *max_curr = NULL;
    while(curr){
        //record the node with maximum number of words
        //if next node has more words, change to this node
        if (curr->num_words >= max_count) {
            max_curr = curr;
            max_count = curr->num_words;
        }
        curr = curr->next;
    }
    return max_curr;
}


/* Deallocate all memory rooted in the List pointed to by fam_list. */
void deallocate_families(Family *fam_list) {
    Family *curr = fam_list;
    //traverse through the linked list to free components in struct and struct itself
    while (curr) {
        Family *prev = curr;
        curr = curr->next;
        free(prev->signature);
        free(prev->word_ptrs);
        free(prev);
    }
}


/* Generate and return a linked list of all families using words pointed to
   by word_list, using letter to partition the words.

   Implementation tips: To decide the family in which each word belongs, you
   will need to generate the signature of each word. Create only the families
   that have at least one word from the current word_list.
*/
Family *generate_families(char **word_list, char letter) {
    Family *head = NULL;
    //traverse through each word to put them to the proper family
    //or initialize a new family
    for (int start = 0; word_list[start]; start++){
        //generate a signature of the word by the letter partition input
        char sig[strlen(word_list[start]) + 1];
        sig[strlen(word_list[start])] = '\0';
        for(int unit = 0; unit < strlen(word_list[start]); unit++) {
            if ((word_list[start])[unit] != letter) {
                sig[unit] = '-';
            } else {
                sig[unit] = letter;
            }
        }
        //after generating the signature, find the family node by the
        //signature, if not found, create a new family node, set this
        //node as head and add the word to the family
        Family *curr = find_family(head, sig);
        if (curr == NULL) {
            curr = new_family(sig);
            curr->next = head;
            head = curr;
        }
        add_word_to_family(curr, word_list[start]);
    }
    return head;
}


/* Return the signature of the family pointed to by fam. */
char *get_family_signature(Family *fam) {
    char *sig = NULL;
    //extract the signature of the node if the input node is not NULL
    if (fam) {
        sig = fam->signature;
    }
    return sig;
}


/* Return a pointer to word pointers, each of which
   points to a word in fam. These pointers should not be the same
   as those used by fam->word_ptrs (i.e. they should be independently malloc'd),
   because fam->word_ptrs can move during a realloc.
   As with fam->word_ptrs, the final pointer should be NULL.
*/
char **get_new_word_list(Family *fam) {
    char **new_list = NULL;
    //if ithe input node is not NULL, initialize the new list by
    //allocating space for it and do error checking
    if (fam) {
        new_list = malloc((fam->max_words + 1) * sizeof(char*));
        if (new_list == NULL) {
            perror("invalid, func get_new_word_list, new_list");
            exit(1);
        }
        //assign the pointer of each word in the word list of input node
        //to each spot in the new list, set the spot at the tail to NULL
        for (int word_ind = 0; word_ind < fam->num_words; word_ind++) {
            new_list[word_ind] = (fam->word_ptrs)[word_ind];
        }
        new_list[fam->num_words] = NULL;
    }
    return new_list;
}


/* Return a pointer to a random word from fam. 
   Use rand (man 3 rand) to generate random integers.
*/
char *get_random_word_from_family(Family *fam) {
    char *word = NULL;
    if (fam) {
        //get a random number in the interval [0, num_words - 1]
        //and pick the node at the index of thi number
        int pick = rand() % (fam->num_words);
        word = (fam->word_ptrs)[pick];
    }
    return word;
}
