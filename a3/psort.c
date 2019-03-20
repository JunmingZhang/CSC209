#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include "helper.h"

/* 
 * the task of each child process: sort
 * indicated number of records from minimum to maximum
 */
void child_task(int expect_task, int child_count, int prev_read,
				int **pipe_fd, char *input_file) {
	FILE *infp = fopen(input_file, "rb");

	if (infp == NULL) {
		perror("fopen at child_task");
		exit(1);
	}

	// close all writing file dexcriptors before the child_count inclusively
	for (int close_no = 0; close_no <= child_count; close_no++) {
		if (close(pipe_fd[close_no][0]) == -1) {
			perror("close reading in child_task");
			exit(1);
		}
	}

	// read out all records this single child need to merge from the input file
	struct rec *rec_array = array_to_write(expect_task, prev_read, infp);

	// sort the recs in the array from minimum to maximum
	qsort(rec_array, expect_task, sizeof(struct rec), compare_freq);

	// write all recs sorted into the pipe
	// after each writing, close that file descriptor
	write_to_pipe(child_count, expect_task, pipe_fd, rec_array);

	free_rec_array(rec_array);
	if (fclose(infp) != 0) {
		fprintf(stderr, "fail to close the file at child_task");
		exit(1);
	}
}

/*
 * the task of the parent process: merge all sorted sub chunk of
 * children and sort them from minimum to maximum
 */
void parent_task(int child_num, int rec_num, int *read_tasks, int **pipe_fd, char *output_file) {
	FILE *outfp = fopen(output_file, "wb");
	if (outfp == NULL) {
		perror("fopen at parent task");
		exit(1);
	}

	// initialize the array used to store each rec read from each child chunk
	struct rec *merge_array = malloc(child_num * sizeof(struct rec));
	if (merge_array == NULL) {
		perror("malloc at parent_task");
		exit(1);
	}

	// generate an array to count the number of recs each child has writen
	int *task_count = generate_task_count(child_num);

	// write a rec into the file for each iteration, iterate the number of rec times
	int write_rec_num = rec_num;
	while (write_rec_num > 0) {
		// initialize the merge array at the first iteration by pmerge in helper.c
		if (write_rec_num == rec_num) {
			for (int child_count = 0; child_count < child_num; child_count++) {
				pmerge(task_count, read_tasks, merge_array, pipe_fd, child_count);
			}
		}

		// merge the merge_array by calling ther merge function
		merge(child_num, task_count, read_tasks, pipe_fd, outfp, merge_array);
		write_rec_num--;
	}

	// free all allocate space for parent process
	free(merge_array);
	free_task_count(task_count);

	// close all reading descriptors
	for (int child_count = 0; child_count < child_num; child_count++) {
		if (close(pipe_fd[child_count][0]) == -1) {
			perror("close reading in parent_task");
			exit(1);
		}
	}

	if (fclose(outfp) != 0) {
		fprintf(stderr, "fail to close file at parent_task");
		exit(1);
	}
}

/*
 * central regulation and control function,
 * divide parent task and children tasks in this functiom
 */
int divide_task(char *input_file, char *output_file, int child_num) {

	// check if child process returns normally
	int return_val;

	// calculate the number of records in the input file
	int rec_num = get_file_size(input_file) / sizeof(struct rec);

	// compute the number of recs each child need to process
	int rec_avg = rec_num / child_num;
	int rec_remain = rec_num % child_num;

	// initialize the array to store the number of recs
	// each child need to process
	int *read_tasks = malloc(sizeof(int) * child_num);
	if (read_tasks == NULL) {
		perror("malloc for read_tasks at divide_task");
		exit(1);
	}

	// initialize the pipe for file descriptors
	int **pipe_fd = alloc_pipe_fd(child_num);

	// execute each child task
	for (int child_count = 0; child_count < child_num; child_count++) {
		// the number of recs a child need to process
		int expect_task = 0;

		// calculate the number of recs a child to process
		if (rec_remain > 0) {
			expect_task = rec_avg + 1;
			rec_remain--;
		} else {
			expect_task = rec_avg;
		}

		// put the number of tasks each child need to read to read_tasks
		read_tasks[child_count] = expect_task;

		// initialize the chunk of pipes the current child need
		if (pipe(pipe_fd[child_count]) == -1) {
			perror("pipe");
			exit(1);
		}

		int divide = fork();

		// fork fails if return value < 0 (-1), terminates this child
		if (divide < 0) {
			perror("fork at divide_task");
			exit(1);
		}

		// fork returns 0 for child process, call child_task function to do what a child does
		// and then deallocate all space allocated in the child process
		else if (divide == 0) {
			// calculate the number of tasks for previous children to read
			int prev_read = get_prev_read(read_tasks, child_count);

			child_task(expect_task, child_count, prev_read, pipe_fd, input_file);
			dealloc_arrays(read_tasks, pipe_fd, child_num);
			exit(0);
		}

		// for parent process in each iteration, close the current writing file descriptors
		else {
			if (close(pipe_fd[child_count][1]) == -1) {
				perror("close write in parent");
				exit(1);
			}
		}
	}

	// call parent_task function to do merging and writing
	parent_task(child_num, rec_num, read_tasks, pipe_fd, output_file);

	// wait for each child and deallocate space for parent process allocated
	// if child exits abnormally, return 1, otherwise return 0
	return_val = call_wait(child_num);
	dealloc_arrays(read_tasks, pipe_fd, child_num);

	return return_val;
}

int main(int argc, char *argv[]) {
	if (argc != 7) {
		fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
		exit(1);
	}

	/*
	//add a timer to count running time for curiosity
	struct timeval starttime, endtime;
	double timediff;
	*/

	// return 0 if everything is normal otherwise return 0
	int return_val;
	
	// process three inputs from the user by the flag given
	int option;
	int child_num;
	char *input_file;
	char *output_file;

	/*
	// start timing
	if ((gettimeofday(&starttime, NULL)) == -1) {
		perror("gettimeofday");
		exit(1);
	}
	*/
	
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

	// terminate the program if the number of child processes provided is at most 0
	if (child_num <= 0) {
		fprintf(stderr, "arithmetic error, there is at least one child\n");
		exit(0);
	}

	// call divide_task for regulating children and parent process
	return_val = divide_task(input_file, output_file, child_num);
	
	/*
	// end timing
	if ((gettimeofday(&endtime, NULL)) == -1) {
		perror("gettimeofday");
		exit(1);
	}

	// print timing result to standard out
	timediff = (endtime.tv_sec - starttime.tv_sec) +
		(endtime.tv_usec - starttime.tv_usec) / 1000000.0;
	fprintf(stdout, "%.4f\n", timediff);
	*/

	return return_val;
}