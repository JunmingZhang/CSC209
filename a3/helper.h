#ifndef _HELPER_H
#define _HELPER_H

#define SIZE 44
#define OVERUPPER 30001 /*used for find_minimum and create a rec with maximum freq if a child chunk read up in pmerge*/

struct rec {
    int freq;
    char word[SIZE];
};

int get_file_size(char *filename);
int compare_freq(const void *rec1, const void *rec2);
int sum_prev_tasks(int child_count, int read_tasks[]);
int **alloc_pipe_fd(int rec_num);

void free_fd(int **pipe_fd, int rec_num);
void call_wait(int child_num);
int get_prev_read(int *read_task, int child_count);
int *generate_task_count(int child_num, int *read_tasks, int *threshould);

void free_task_count(int *task_count);
void pmerge(int task_count[], int threshoulds[], struct rec merge_array[],
            int *pipe_fd[], int child_index);
int find_minimum(struct rec merge_array[], int child_num);
void dealloc_arrays(int *read_tasks, int *threshoulds,
                    int *pipe_fd[], int rec_num);

#endif /* _HELPER_H */
