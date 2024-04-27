#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "mimdram.h"

#ifndef SIZE
    #define SIZE 1024
#endif

// Data-width of values
#ifndef DWIDTH
    #define DWIDTH 32
#endif

extern void rowop_and(void *d, void *s1, void *s2);
extern void rowop_or(void *d, void *s1, void *s2);
extern void rowop_not(void *d, void *s);
extern void rowop_shift(void *d, void *s);
extern void rowop_xor(void *d, void *s1, void *s2);

int main(int argc, char **argv) {

    srand(121324314);

    int num_bits = (int) SIZE*DWIDTH;
    int row_bytes = num_bits / 8;
    int row_ints = num_bits / DWIDTH;

    // allocate input data
    unsigned *vec1, *vec2;
    vec1 = malloc(sizeof(unsigned *));
    posix_memalign((void *) &vec1, ALIGNMENT, row_bytes);
    vec2 = malloc(sizeof(unsigned *));
    posix_memalign((void *) &vec2, ALIGNMENT, row_bytes);

    // allocate output
    unsigned *output;
    posix_memalign((void *) &output, ALIGNMENT, row_bytes);


    for (int i = 0; i < 16; i ++) {
        vec1[i] = 0xFFFF0000;
        vec2[i] = 0;
    }

    vec1[0] = 0xFFFFFFFF;
    vec1[2] = 0xFFFFFFFF;

    m5_reset_stats(0,0);

    rowop_shift((void *) output, (void *)vec1);

    for (int i = 0; i < 16; i ++) {
        printf("%x\n", output[i]);
    }

    m5_dump_stats(0,0);

    free(vec1);
    free(vec2);

    return 0;
}