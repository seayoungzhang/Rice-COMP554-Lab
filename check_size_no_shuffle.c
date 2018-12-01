#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#define TICKS_PER_SECOND 1000000.0	/* One million ticks per second i.e. 1 usec per tick */

/*** Key changes:
 1. Changed calculation of bytes per int to bytes per pointer.  Corrects
    cache calculations where the size of a pointer != size of an int.
 2. Added option (FISHER_YATES_SHUFFLE) to scramble cache accesses.  Without this
    option, cache accesses are sequential.
 3. Changed output heading to read "Size (bytes)    Runtime (seconds)"
***/

int main(int argc, char **argv)
{
	/*** This is the start of the variables you can play with... ***/
    int size = 1024;			/* The minimum cache size tested for. */
    int maxSize = pow(2, 27);	/* maximum cache size tested for is 256 MB = 8 bytes per pointer * 2^25 ints */
    int number_of_trials = 10;	/* How many trials to run to look for the minimum time. */
    int iterations = 1000000;	/* perform one million accesses per test */
	int FISHER_YATES_SHUFFLE = 0;   /* 0 = no shuffle, 1 = shuffle */
	/*** ... and this is the end of the variable you can play with. ***/

    int i, j, k, **next;
    struct timeval timer_start, timer_end;
    double start, finish, time, lowest_time;
    int setup_int, *setup_int_ptr, **does_not_equal_next;
	int bytes_per_pointer_to_int;
    int stride = 1; /* The stride is basically how much space we skip between
					 elements in the array.  If we increase the stride, we would
					 reduce the amount of spatial reuse. */

	/* In accessing the cache we will be storing pointers to ints.  So, we need
	   to know the number of bytes per pointer to an int... */
	bytes_per_pointer_to_int = sizeof(int *);

	// printf("Our array holds pointer to ints and they have a width of %d bytes...\n", bytes_per_pointer_to_int);

    /* to get around timer inaccuracies, we perform each test multiple
       times and take the lowest time */

    /* we need to ensure that the optimizer can't eliminate all of this
       as dead code, so we need to set up a test at the end of the code
       that will make it look like the "next" pointer has an important
       value; while a human could reason through the code and figure
       out that it's all useless, most optimizers cannot */
    setup_int_ptr = (&setup_int);
    does_not_equal_next = &setup_int_ptr;

    /* run through all of the sizes of arrays */

	printf("Size,Runtime\n");
	// printf("    (bytes)    (seconds) \n");

    while (size <= maxSize) /* We do this to limit the number of tests we do */
    {
	/* We will be doubling 'size' every iteration.  To help generate several
	 more samples, we split the range from the last_size to this size.  We
	 calculate three points (plus one for the current size) that are the
	 array sizes that we are going to check */
	int last_size = size/2;
	int size_delta = size-last_size;
	int quarter_of_delta = size_delta/4;
	int first_quarter = last_size+(1*quarter_of_delta);
	int half = last_size+(2*quarter_of_delta);
	int three_quarters = last_size+(3*quarter_of_delta);
	int points[] = {first_quarter, half, three_quarters, size};
	int num_array_indices;

	/* test each array size */
for (i = 0; i < 4; i++)
	{
	    int test_size = points[i];

		/* We allocate a piece of memory that is the size of the largest test
		 we will run.  This gives us a large block of valid addresses we can
		 use in our cache experiments. */

	    int ***test_array = malloc(sizeof(int **)*test_size);
		int *test_array_index = malloc(test_size * sizeof (int)); /* Allocate array for the test array indices. */

	    /* first, allocate the array; make each entry point to
	       the next entry; this keeps the optimizer from discovering
	       that this is useless code (which it would delete!) */

		/* Our test array is a portion of the max_test_array */
		/* We also keep track of the indexes j in the test array.  We use	  */
		/* this in the optional scrambler.									  */
	    for (j=0, k=0; j<(test_size);j+=stride, k++)
		{
			test_array[j] = (int **)(&(test_array[(j+stride)%test_size]));
			test_array_index[k] = (j+stride)%test_size; /* This is capturing the test array indices we used. */
		}
		num_array_indices = k;	/* This is how many array indices we have. */

		/* make sure that last entry points to the first to create
		 the circular list of accesses */
	    if (test_array[j-stride] != (int **)(&test_array[0]))
			test_array[j-stride] = (int**)(&test_array[0]);

		/* Scramble the list of test array indices using the Fisher-Yates shuffle */

		if (FISHER_YATES_SHUFFLE)
		{
			int temporary;
			int m_index;
			int random_i_index;

			/* We first shuffle the indices we used in the test_array...*/

			/* 1. Start with a list of elements, 0, ... n-1 and let m = n-1.
			   2. Pick a random number i between 0 and m inclusive.
			   3. Swap the ith and mth numbers in the list.
			   4. Decrease m by one.
			   5. Go to step 2 until m = 0. */

			for (m_index = num_array_indices - 1; m_index > 0; m_index--)
			{
				/* Select a random i index betwenn 0 and m_index...*/
				random_i_index = rand()%m_index;

				/* Swap the random i index and the m_index ...*/
				temporary = test_array_index[m_index];
				test_array_index[m_index] = test_array_index[random_i_index];
				test_array_index[random_i_index] = temporary;
			}

			/* Now we shuffle the test_array according to the shuffled
			   test_array_index[] */

			for (j = 0, k = 0; j < test_size; j += stride, k++)
				test_array[test_array_index[k]] = (int **)(&(test_array[test_array_index[(k+1)%num_array_indices]]));

			/* make sure that last entry points to the first to create
			 the circular list of accesses */
			if (test_array[test_array_index[num_array_indices-1]] != (int **)(&test_array[test_array_index[0]]))
				test_array[test_array_index[num_array_indices-1]] = (int **)(&test_array[test_array_index[0]]);
		}

	    lowest_time = 1000.0;

		/* To help get more stable resuts, we run the test number_of_trials
		 times in order to remove outliers in timing the runs. */
	    for (k=0;k<number_of_trials;k++)
	    {
		/* iterate through the array */
			next = (int **)test_array[0];

		/* get the time we start */
		gettimeofday(&timer_start, NULL);

		/***********************************************************************
		 This is the raison d'etre of the application.  We use the elements in
		 the array as the address to the next element in the array.

		 We expect this to perform at least one memory access per cycle.  The
		 access will be as wide as a pointer to an int.
		 **********************************************************************/
		for (j=0;j<iterations;j++)
		    next = (int **)*next;

		/* get the time we stop */
		gettimeofday(&timer_end, NULL);

		/* convert the time to a number we can understand i.e. seconds */
		start = timer_start.tv_sec + (timer_start.tv_usec/TICKS_PER_SECOND);
		finish = timer_end.tv_sec + (timer_end.tv_usec/TICKS_PER_SECOND);
		time = finish-start;
		if (time < lowest_time)
		    lowest_time = time;
	    }

		/* Print out the cache size 'points[i]' being tested (in bytes) and the time to perform 'iterations' accesses in seconds. */
		fprintf(stdout, "%d,%1.8f\n", points[i]*bytes_per_pointer_to_int, lowest_time);

	    free(test_array);
		free(test_array_index);
	}
	size*=2; /* Let's double the size of the arrays we use on the next pass. */
    }
    if (next == does_not_equal_next)
	fprintf(stderr, "This should never print.\n");
 } /* main */
