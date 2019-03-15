#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include "helper.h"

void child_task(int expect_task, int child_count, int prev_read,
				int **pipe_fd, char *input_file) {
	FILE *fp = fopen(input_file, "rb");
	int threshould = prev_read + expect_task;

	if (fp == NULL) {
		perror("fopen at child_task");
		exit(1);
	}

	struct rec rec_array[expect_task];
	for (int close_no = 0; close_no < prev_read + expect_task; close_no++) {
		if (close(pipe_fd[close_no][0]) == -1)
		{
			perror("close reading in child_task");
			exit(1);
		}
	}

	fseek(fp, prev_read * sizeof(struct rec), SEEK_SET);
	for (int task_num = 0; task_num < expect_task; task_num++) {
		if (fread(&(rec_array[task_num]), sizeof(struct rec), 1, fp) != 1) {
			fprintf(stderr, "fread at child_task");
			exit(1);
		}
	}

	qsort(rec_array, expect_task, sizeof(struct rec), compare_freq);

	for (int write_in = prev_read; write_in < threshould; write_in++) {
		if (write(pipe_fd[write_in][1], &(rec_array[write_in - prev_read]),
			sizeof(struct rec)) != sizeof(struct rec)) {
			perror("write in child pipe");
			exit(1);
		}

		if (close(pipe_fd[write_in][1]) == -1) {
			perror("close writing in child_task");
			exit(1);
		}
	}

	if (fclose(fp) != 0) {
		fprintf(stderr, "fail to close the file at child_task");
		exit(1);
	}
}

void parent_task(int child_num, int rec_num, int *read_tasks,
				int *threshoulds, int **pipe_fd, char *output_file) {
	FILE *fp = fopen(output_file, "wb");
	if (fp == NULL) {
		perror("fopen at parent task");
		exit(1);
	}

	struct rec *merge_array = malloc(child_num * sizeof(struct rec));
	if (merge_array == NULL) {
		perror("malloc at parent_task");
		exit(1);
	}

	int *task_count = generate_task_count(child_num, read_tasks, threshoulds);
	int write_rec_num = rec_num;

	while (write_rec_num > 0) {
		if (write_rec_num == rec_num) {
			for (int child_count = 0; child_count < child_num; child_count++) {
				pmerge(task_count, threshoulds, merge_array, pipe_fd, child_count);
			}
		}

		int min_frec_index = find_minimum(merge_array, child_num);
		if (fwrite(&(merge_array[min_frec_index]),
					sizeof(struct rec), 1, fp) != 1) {
			fprintf(stderr, "fwrite at parent_task");
			exit(1);
		}

		pmerge(task_count, threshoulds, merge_array, pipe_fd, min_frec_index);
		write_rec_num--;
	}

	free(merge_array);
	free_task_count(task_count);

	if (fclose(fp) != 0) {
		fprintf(stderr, "fail to close file at parent_task");
		exit(1);
	}
}

void divide_task(char *input_file, char *output_file, int child_num) {

	int rec_num = get_file_size(input_file) / sizeof(struct rec);

	int rec_avg = rec_num / child_num;
	int rec_remain = rec_num % child_num;

	int *read_tasks = malloc(sizeof(int) * child_num);
	if (read_tasks == NULL) {
		perror("malloc for read_tasks at divide_task");
		exit(1);
	}

	int *threshoulds = malloc(sizeof(int) * child_num);
	if (threshoulds == NULL) {
		perror("malloc for threshoulds at divide_task");
		exit(1);
	}

	int **pipe_fd = alloc_pipe_fd(rec_num);

	for (int child_count = 0; child_count < child_num; child_count++) {
		int expect_task = 0;

		if (rec_remain > 0) {
			expect_task = rec_avg + 1;
			rec_remain--;
		} else {
			expect_task = rec_avg;
		}

		read_tasks[child_count] = expect_task;

		int prev_read = get_prev_read(read_tasks, child_count);
		threshoulds[child_count] = prev_read + expect_task;

		for (int pipe_up = prev_read; pipe_up < threshoulds[child_count]; pipe_up++) {
			if (pipe(pipe_fd[pipe_up]) == -1) {
				perror("pipe");
				exit(1);
			}
		}

		int divide = fork();

		if (divide < 0) {
			perror("fork at divide_task");
			exit(1);
		}

		else if (divide == 0) {
			child_task(expect_task, child_count, prev_read, pipe_fd, input_file);
			dealloc_arrays(read_tasks, threshoulds, pipe_fd, rec_num);
			exit(0);
		}

		else {
			for (int pipe_up = prev_read; pipe_up < threshoulds[child_count]; pipe_up++) {
				if (close(pipe_fd[pipe_up][1]) == -1) {
					perror("close write in parent");
					exit(1);
				}
			}
		}
	}

	parent_task(child_num, rec_num, read_tasks, threshoulds, pipe_fd, output_file);

	call_wait(child_num);
	dealloc_arrays(read_tasks, threshoulds, pipe_fd, rec_num);
}

int main(int argc, char *argv[]) {
	if (argc != 7) {
		fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
		exit(1);
	}

	struct timeval starttime, endtime;
	double timediff;

	int option;
	int child_num;
	char *input_file;
	char *output_file;

	if ((gettimeofday(&starttime, NULL)) == -1) {
		perror("gettimeofday");
		exit(1);
	}
	
	// code you want to time
	while ((option = getopt(argc, argv, "n:f:o:")) != -1) {
		switch (option) {
		case 'n':
			child_num = strtol(optarg, NULL, 10);
			break;
		case 'f':
			input_file = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		default:
			fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
			exit(1);
		}
	}

	if (child_num <= 0) {
		fprintf(stderr, "arithmetic error, there is at least one child");
		exit(0);
	}

	divide_task(input_file, output_file, child_num);
	
	if ((gettimeofday(&endtime, NULL)) == -1) {
		perror("gettimeofday");
		exit(1);
	}

	timediff = (endtime.tv_sec - starttime.tv_sec) +
		(endtime.tv_usec - starttime.tv_usec) / 1000000.0;
	fprintf(stdout, "%.4f\n", timediff);

	return 0;
}