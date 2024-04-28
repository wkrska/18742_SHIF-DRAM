#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <omp.h>

#ifndef SIZE
#define SIZE 1024
#endif

#if DWIDTH==32
    typedef uint32_t my_int;
#elif DWIDTH==16
    typedef uint16_t my_int;
#elif DWIDTH==8
    typedef uint8_t my_int;
#else
    typedef uint32_t my_int;
#endif

void right_circular_shift(my_int* vec, uint32_t len) {
    my_int temp = vec[len - 1];
    for (uint32_t i = len - 1; i > 0; i--) {
        vec[i] = vec[i - 1];
    }
    vec[0] = temp;
}

void left_circular_shift(my_int* vec, uint32_t len) {
    my_int temp = vec[0];
    for (uint32_t i = 0; i < len - 1; i++) {
        vec[i] = vec[i + 1];
    }
    vec[len - 1] = temp;
}

// create a vector that is the absolute difference of two vectors
void vec_abs_diff(my_int* vec1, my_int* vec2, my_int* res, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        res[i] = abs(vec1[i] - vec2[i]);
    }
}

// reduce a vector to a single value by summing all elements
my_int vec_red(my_int* vec, uint32_t len) {
    my_int sum = 0;
    // loop is executed in parallel, with each thread adding up a portion of the elements
    // http://jakascorner.com/blog/2016/06/omp-for-reduction.html
    #pragma omp parallel for reduction(+:sum)
    for (uint32_t i = 0; i < len; i++) {
        sum += vec[i];
    }
    return sum;
}

my_int sum_abs_diff(my_int* vec1, my_int* vec2, uint32_t len) {
    my_int* res = malloc(len * sizeof(my_int));
    vec_abs_diff(vec1, vec2, res, len);
    my_int sum = vec_red(res, len);
    free(res);
    return sum;
}

int main() {
    srand(time(NULL));
    // two vectors that are almost the same except shifted off by one
    my_int* vec1 = malloc(SIZE * sizeof(my_int));
    my_int* vec2 = malloc(SIZE * sizeof(my_int));
    
    for (uint32_t i = 0; i < SIZE; i++) {
        vec1[i] = rand() % 100;
        vec2[i] = vec1[i];
    }

    printf("Sum of absolute differences: %d\n", sum_abs_diff(vec1, vec2, SIZE));

    free(vec1);
    free(vec2);
    return 0;
}

// gcc -std=c99 -fopenmp -o sad sad.c