#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "mimdram.h"

// Number of complete values
#ifndef SIZE
    #define SIZE 1024
#endif

// Data-width of values
#ifndef DWIDTH
    #define DWIDTH 32
#endif

extern void rowop_and(void *d, void *s1, void *s2);
extern void rowop_or(void *d, void *s1, void *s2);
extern void rowop_xor(void *d, void *s1, void *s2);
extern void rowop_not(void *d, void *s);
extern void rowop_shift(void *d, void *s);

/************/
/* row shift and mask wrapper function
/* Params
/*  d: destination row ptr
/*  s: source row ptr
/*  N: number of times to shift left
/*  mask: bitwise mask row ptr
/************/
void rowop_shift_n_mask(void *d, void *s, int N, void *mask) {
    #ifdef DEBUG
    printf("rowop: shift n mask\n");
    #endif
    for (int cnt = 0; cnt < N; cnt++){
        // Left shift 1
        if (cnt==0)
            rowop_shift((void *) d, (void *) s);
        else
            rowop_shift((void *) d, (void *) d);

        // bitwise AND with mask
        rowop_and((void *) d, (void *) d, (void *) mask);
        #ifdef DEBUG
        printf("\titr: %d\n", cnt);
        #endif
    }
    return;
}

/************/
/* row addition function
/* Params
/*  d: destination row ptr
/*  s1: source 1 row ptr
/*  s2: source 2 row ptr
/*  N: addition data-width
/*  row_bytes: stupid thing from their code
/************/
void row_add(void *d, void *s1, void *s2, int N, int row_bytes) {
    #ifdef DEBUG
        printf("Row add\n");
    #endif

    // Implements Kogge Stone fast-addition algorithm

    // Create bit-mask based on data-width
    unsigned *mask;
    mask = malloc(sizeof(unsigned *));
    posix_memalign((void *) &mask, ALIGNMENT, row_bytes);
    for (int i = 0; i < (row_bytes/(N/8)); i ++) {
        mask[i] = ~1;
    }

    // "Front porch" pre processing step
    unsigned *and0, *xor0;
    posix_memalign((void *) &and0, ALIGNMENT, row_bytes);
    posix_memalign((void *) &xor0, ALIGNMENT, row_bytes);
    rowop_and((void *) and0, (void *) s1, (void *) s2);
    rowop_xor((void *) xor0, (void *) s1, (void *) s2);

    // Body operations
    unsigned *andN, *orN, *andNshift, *orNshift, *andTemp, *andNCopy;
    posix_memalign((void *) &andN, ALIGNMENT, row_bytes);
    posix_memalign((void *) &orN, ALIGNMENT, row_bytes);
    posix_memalign((void *) &andNshift, ALIGNMENT, row_bytes);
    posix_memalign((void *) &orNshift, ALIGNMENT, row_bytes);
    posix_memalign((void *) &andTemp, ALIGNMENT, row_bytes);
    posix_memalign((void *) &andNCopy, ALIGNMENT, row_bytes);
    for (int shift = 1; shift < N; shift*=2)
    {
        #ifdef DEBUG
        printf("\tBody %d\n", shift);
        #endif
        // left shift prior step results;
        if (shift == 1) { // special treatment for first itr
            rowop_shift_n_mask((void *) orNshift, (void *) and0, shift, (void *) mask);
            rowop_shift_n_mask((void *) andNshift, (void *) xor0, shift, (void *) mask);
        } else {
            rowop_shift_n_mask((void *) orNshift, (void *) orN, shift, (void *) mask);
            rowop_shift_n_mask((void *) andNshift, (void *) andN, shift, (void *) mask);
        }

        // Perform intermediate steps
        if (shift == 1) { // special treatment for first itr
            // Perform or_{N+1} = and_0 | (and_shift_N & xor_0)
            // Perform and_{N+1} = or_N & or_shift_N)

            rowop_aap((void *) andNCopy, (void *) and0);
            rowop_and((void *) andTemp, (void *) xor0, (void *) andNshift);
            rowop_and((void *) andN, (void *) xor0, (void *) orNshift);
            rowop_or((void *) orN, (void *) andNCopy, (void *) andTemp);
        } else {
            // Perform or_{N+1} = or_N | (or_shift_N & and_N)
            // Perform and_{N+1} = and_N & and_shift_N)

            rowop_and((void *) andTemp, (void *) andN, (void *) orNshift);
            rowop_and((void *) andN, (void *) andN, (void *) andNshift);
            rowop_or((void *) orN, (void *) orN, (void *) orNshift);
        }    
    }
    rowop_shift_n_mask((void *) orNshift, (void *) orN, 1, (void *) mask);

    // "Back Porch" post processing steps
    rowop_xor((void *) d, (void *) xor0, (void *) orNshift);

    free(mask);
    return;
}

/************/
/* two's comlement function
/* Params
/*  d: destination row ptr
/*  s: source row ptr
/*  N: addition data-width
/*  row_bytes: stupid thing from their code
/************/
void row_twos_comp(void *d, void *s, int N, int row_bytes) {
    #ifdef DEBUG
    printf("row two's comp\n");
    #endif
    // Create one's-mask based on data-width
    unsigned *ones;    
    ones = malloc(sizeof(unsigned *));
    posix_memalign((void *) &ones, ALIGNMENT, row_bytes);
    for (int i = 0; i < row_bytes/(N/8); i ++) {
        ones[i] = 1;
    }

    // Invert
    rowop_not((void *) d, (void *) s);

    // Add one
    row_add((void *) d, (void *) d, (void *) ones, N, row_bytes);

    free(ones);

    return;
}

/************/
/* row reduce function
/* Params
/*  d: destination row ptr
/*  s: source row ptr
/*  N: addition data-width
/*  row_bytes: stupid thing from their code
/************/
void row_reduce(void *d, void *s, int N, int row_bytes) {
    #ifdef DEBUG
    printf("Row reduce\n");
    #endif
    // Create bit-mask based on data-width
    unsigned *mask;    
    mask = malloc(sizeof(unsigned *));
    posix_memalign((void *) &mask, ALIGNMENT, row_bytes);
    for (int i = 0; i < row_bytes/(N/8); i ++) {
        mask[i] = ~1;
    }

    unsigned *temp;
    posix_memalign((void *) &temp, ALIGNMENT, row_bytes);

    // Binary Reduction
    for (int shift = N; shift < ROW_SIZE; shift *= 2) {
        #ifdef DEBUG
        printf("\titr: %d\n",shift);
        #endif
        if (shift == N)
            rowop_shift_n_mask((void *) temp, (void *) s, shift, (void *) mask);
        else   
            rowop_shift_n_mask((void *) temp, (void *) d, shift, (void *) mask);
        
        row_add((void *) d, (void *) d, (void *) temp, N, row_bytes);
    }

    free(mask);

    return;
}


int main(int argc, char **argv) {
    #ifdef DEBUG
    printf("Debugging Mode\n");
    #endif

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

    unsigned result = 0;
    
    // allocate intermediate vectors
    unsigned *twoComp, *diff;
    posix_memalign((void *) &twoComp, ALIGNMENT, row_bytes);
    posix_memalign((void *) &diff, ALIGNMENT, row_bytes);
    
    // initialize input data
    for (int i = 0; i < row_ints; i ++) {
        vec1[i] = rand();
        vec2[i] = rand();
    }

    // Run the query
    m5_reset_stats(0,0);

    // Difference
    row_twos_comp((void *) twoComp, (void *) vec2, DWIDTH, row_bytes);
    row_add((void *) diff, (void *) vec1, (void *) twoComp, DWIDTH, row_bytes);

    // Reduce
    row_reduce((void *) output, (void *) diff, DWIDTH, row_bytes);

    // Extract output
    result = output[0];

    m5_dump_stats(0,0);

    free(vec1);
    free(vec2);
    
    return 0;
}
