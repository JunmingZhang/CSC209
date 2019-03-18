#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "helper.h"


/* get the size of the input file */
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

/* sum up the number of all previous recs processed recursively */
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

/* allocate space for the pipe */
int** alloc_pipe_fd(int child_num) {
    int** pipe_fd = malloc(sizeof(int*) * child_num);
    if (pipe_fd == NULL) {
        perror("malloc for pipe_fd");
        exit(1);
    }

    for (int pipe_pt = 0; pipe_pt < child_num; pipe_pt++) {
        if ((pipe_fd[pipe_pt] = malloc(sizeof(int) * 2)) == NULL) {
            perror("malloc for file descriptors in pipe_fd");
            exit(1);
        }
    }
    return pipe_fd;
}

/* free the allocated pipe */
void free_fd(int** pipe_fd, int child_num) {
    for (int clean = 0; clean < child_num; clean++) {
        free(pipe_fd[clean]);
    }
    free(pipe_fd);
}

/* call wait for each child process in the parent process */
void call_wait(int child_num) {
    for (int num_process = 0; num_process < child_num; num_process++) {
        if (wait(NULL) == -1) {
            fprintf(stderr, "Child terminated abnormally\n");
        }
    }
}

/* get the number of previously-read recs by calling sum_prev_tasks as helper */
int get_prev_read(int *read_task, int child_count) {
    int prev_read = 0;
    if (child_count > 0) {
        prev_read = sum_prev_tasks(child_count, read_task);
    }
    return prev_read;
}

/* read out all records this single child need to merge */
struct rec* array_to_write(int expect_task, int prev_read, FILE *fp) {
	struct rec *rec_array = malloc(sizeof(struct rec) * expect_task);
    if (rec_array == NULL) {
        perror("malloc at array_to_write, child_task");
        exit(1);
    }

	fseek(fp, prev_read * sizeof(struct rec), SEEK_SET);
	for (int task_num = 0; task_num < expect_task; task_num++) {
		if (fread(&(rec_array[task_num]), sizeof(struct rec), 1, fp) != 1) {
			fprintf(stderr, "fread at child_task");
			exit(1);
		}
	}
    return rec_array;
}

/* free the rec_array used for wrting to the pipe for each child */
void free_rec_array(struct rec *rec_array) { free(rec_array); }

/*
 * write all recs sorted into the pipe for child task,
 * after each writing, close that file descriptor
 */
void write_to_pipe(int child_count, int expect_task,
                    int** pipe_fd, struct rec* rec_array) {
    for (int write_in = 0; write_in < expect_task; write_in++) {
		if (write(pipe_fd[child_count][1], &(rec_array[write_in]),
			sizeof(struct rec)) != sizeof(struct rec)) {
			perror("write in child pipe");
			exit(1);
		}
	}

    if (close(pipe_fd[child_count][1]) == -1) {
			perror("close writing in child_task");
			exit(1);
	}
}

/*
 * generate an array of starting indices
 * of the file descriptor each child wrote
 */
int* generate_task_count(int child_num) {
    int *task_count = malloc(sizeof(int) * child_num);
    if (task_count == NULL) {
        perror("malloc at generate_task_count");
        exit(1);
    }

    // each child has not written any recs into the pipe at beginning
    for (int count = 0; count < child_num; count++) {
        task_count[count] = 0;
    }
    return task_count;
}

/* free the array of task_count */
void free_task_count(int *task_count) { free(task_count); }

/*
 * write the struct to the binary file,
 * re-merge the input merge array by finding the struct
 * with the smallest freq at the top of all child chunks
 * of pipes
 */
void merge(int child_num, int* task_count, int* read_tasks,
			int** pipe_fd, FILE* fp, struct rec *merge_array) {
	// find the rec with minimum freq in the merge array
	// and write it to the file in binary form
	int child_index = find_minimum(merge_array, child_num);
	if (fwrite(&(merge_array[child_index]),
		sizeof(struct rec), 1, fp) != 1) {
		fprintf(stderr, "fwrite at parent_task");
		exit(1);
	}

	// after wrting, update the merge array by pmerge
	pmerge(task_count, read_tasks, merge_array, pipe_fd, child_index);
}

/* 
 * update the merge array in the parent_task when initializing
 * the merge array or change the rec at the child_index slot
 * to a new one read from the pipe
 */
void pmerge(int task_count[], int read_tasks[], struct rec merge_array[],
            int* pipe_fd[], int child_index) {
    int* rec_task = &(task_count[child_index]);

    // if the pipe chunk for the child has been read up, change
    // the freq of the rec at the slot to be "over large"
    if (*rec_task >= read_tasks[child_index]) {
        struct rec over_maximum;
        strncpy(over_maximum.word, "overflow", SIZE);
        over_maximum.freq = OVERUPPER;

        merge_array[child_index] = over_maximum;
    } else { // read a rec from a pipe and update the rec at the given slot
        if (read(pipe_fd[child_index][0], &(merge_array[child_index]), sizeof(struct rec)) == -1) {
            perror("read at parent_task while initializing");
            exit(1);
        }

        // increase the number of records have been read
        (*rec_task)++;
    }
}

/* find the rec with minimum freq in the merge array */
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

/* deallocate a series of arrays input */
void dealloc_arrays(int* read_tasks, int* pipe_fd[], int child_num) {
    free(read_tasks);
	free_fd(pipe_fd, child_num);
}
