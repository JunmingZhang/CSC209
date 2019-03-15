#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"


int get_file_size(char *filename) {
    struct stat sbuf;

    if ((stat(filename, &sbuf)) == -1) {
       perror("stat");
       exit(1);
    }

    return sbuf.st_size;
}

/* A comparison function to use for qsort */
int compare_freq(const void *rec1, const void *rec2) {

    struct rec *r1 = (struct rec *) rec1;
    struct rec *r2 = (struct rec *) rec2;

    if (r1->freq == r2->freq) {
        return 0;
    } else if (r1->freq > r2->freq) {
        return 1;
    } else {
        return -1;
    }
}

int sum_prev_tasks(int child_count, int read_tasks[]) {
    if (child_count == 0) {
        fprintf(stderr, "shoud not be called when the first child is created");
        exit(1);
    } else if (child_count == 1) {
        return read_tasks[0];
    } else {
        return read_tasks[child_count - 1] + sum_prev_tasks(child_count - 1, read_tasks);
    }
}

int** alloc_pipe_fd(int rec_num) {
    int** pipe_fd = malloc(sizeof(int*) * rec_num);
    if (pipe_fd == NULL) {
        perror("malloc for pipe_fd");
        exit(1);
    }

    for (int pipe_pt = 0; pipe_pt < rec_num; pipe_pt++) {
        //printf("*");
        if ((pipe_fd[pipe_pt] = malloc(sizeof(int) * 2)) == NULL) {
            perror("malloc for file descriptors in pipe_fd");
            exit(1);
        }
    }

    return pipe_fd;
}

void free_fd(int** pipe_fd, int rec_num) {
    for (int clean = 0; clean < rec_num; clean++) {
        free(pipe_fd[clean]);
    }
    free(pipe_fd);
}

void call_wait(int child_num) {
    for (int num_process = 0; num_process < child_num; num_process++) {
        if (wait(NULL) == -1) {
            fprintf(stderr, "Child terminated abnormally\n");
        }
    }
}

int get_prev_read(int *read_task, int child_count) {
    int prev_read = 0;
    if (child_count > 0) {
        prev_read = sum_prev_tasks(child_count, read_task);
    }
    return prev_read;
}

int* generate_task_count(int child_num, int* read_tasks, int* threshoulds) {
    int *task_count = malloc(sizeof(int) * child_num);
    if (task_count == NULL) {
        perror("malloc at generate_task_count");
    }

    for (int count = 0; count < child_num; count++) {
        task_count[count] = threshoulds[count] - read_tasks[count];
    }
    return task_count;
}

void free_task_count(int *task_count) { free(task_count); }

void pmerge(int task_count[], int threshoulds[], struct rec merge_array[], int* pipe_fd[], int child_index) {
    int* rec_index = &(task_count[child_index]);

    if (*rec_index >= threshoulds[child_index]) {
        struct rec over_maximum;
        strncpy(over_maximum.word, "overflow", SIZE);
        over_maximum.freq = OVERUPPER;

        merge_array[child_index] = over_maximum;
    } else {
        if (read(pipe_fd[*rec_index][0], &(merge_array[child_index]), sizeof(struct rec)) == -1) {
            perror("read at parent_task while initializing");
            exit(1);
        }

        if (close(pipe_fd[*rec_index][0]) == -1) {
			perror("close reading in parent_task");
			exit(1);
		}

        (*rec_index)++;
    }
}

int find_minimum(struct rec merge_array[], int child_num) {
    int minimum = OVERUPPER;
    int min_index = 0;

    for (int child_no = 0; child_no < child_num; child_no++) {
        if (minimum > merge_array[child_no].freq) {
            minimum = merge_array[child_no].freq;
            min_index = child_no;
        }
    }
    return min_index;
}

void dealloc_arrays(int* read_tasks, int* threshoulds, int* pipe_fd[], int rec_num) {
    free(read_tasks);
	free(threshoulds);
	free_fd(pipe_fd, rec_num);
}
