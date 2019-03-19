#ifndef _HELPER_H
#define _HELPER_H

#define SIZE 44
#define OVERUPPER 30001 /* used for find_minimum and create a rec with maximum freq if a child chunk read up in pmerge */

struct rec {
    int freq;
    char word[SIZE];
};

int get_file_size(char *filename);
int compare_freq(const void *rec1, const void *rec2);
int sum_prev_tasks(int child_count, int read_tasks[]);
int **alloc_pipe_fd(int child_num);

void free_fd(int **pipe_fd, int child_num);
void call_wait(int child_num);
int get_prev_read(int *read_task, int child_count);
struct rec* array_to_write(int expect_task, int prev_read, FILE *fp);

void free_rec_array(struct rec *rec_array);
void write_to_pipe(int child_count, int expect_task,
                    int** pipe_fd, struct rec* rec_array);
int *generate_task_count(int child_num);
void free_task_count(int *task_count);

void merge(int child_num, int* task_count, int* read_tasks,
			int** pipe_fd, FILE* fp, struct rec *merge_array);
void pmerge(int task_count[], int read_tasks[], struct rec merge_array[],
            int *pipe_fd[], int child_index);
int find_minimum(struct rec merge_array[], int child_num);
void dealloc_arrays(int *read_tasks, int *pipe_fd[], int child_num);

#endif /* _HELPER_H */
