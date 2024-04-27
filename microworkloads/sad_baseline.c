#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <omp.h>

#ifndef SIZE
#define SIZE 1024
#endif

void right_circular_shift(uint32_t* vec, uint32_t len) {
    uint32_t temp = vec[len - 1];
    for (uint32_t i = len - 1; i > 0; i--) {
        vec[i] = vec[i - 1];
    }
    vec[0] = temp;
}

void left_circular_shift(uint32_t* vec, uint32_t len) {
    uint32_t temp = vec[0];
    for (uint32_t i = 0; i < len - 1; i++) {
        vec[i] = vec[i + 1];
    }
    vec[len - 1] = temp;
}

// create a vector that is the absolute difference of two vectors
void vec_abs_diff(uint32_t* vec1, uint32_t* vec2, uint32_t* res, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        res[i] = abs(vec1[i] - vec2[i]);
    }
}

// reduce a vector to a single value by summing all elements
uint32_t vec_red(uint32_t* vec, uint32_t len) {
    uint32_t sum = 0;
    // loop is executed in parallel, with each thread adding up a portion of the elements
    // http://jakascorner.com/blog/2016/06/omp-for-reduction.html
    #pragma omp parallel for reduction(+:sum)
    for (uint32_t i = 0; i < len; i++) {
        sum += vec[i];
    }
    return sum;
}

uint32_t sum_abs_diff(uint32_t* vec1, uint32_t* vec2, uint32_t len) {
    uint32_t* res = malloc(len * sizeof(uint32_t));
    vec_abs_diff(vec1, vec2, res, len);
    uint32_t sum = vec_red(res, len);
    free(res);
    return sum;
}

int main() {
    srand(time(NULL));
    // two vectors that are almost the same except shifted off by one
    uint32_t* vec1 = malloc(SIZE * sizeof(uint32_t));
    uint32_t* vec2 = malloc(SIZE * sizeof(uint32_t));
    
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