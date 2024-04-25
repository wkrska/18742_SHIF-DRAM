#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "mimdram.h"

#ifndef SIZE
    #define SIZE 8
#endif

#ifndef DWIDTH
    #define DWIDTH 8
#endif

#define ROW_SIZE 8192
#define BANK_COUNT 16
#define RANK_COUNT 2
#define ALIGNMENT (ROW_SIZE * BANK_COUNT * RANK_COUNT)

#define FOR_ALL_ROWS for(int i = 0; i < per_col_rows; i ++)
#define ROW(ptr) ((void *)(ptr) + i*ROW_SIZE)

extern void rowop_and(void *d, void *s1, void *s2);
extern void rowop_or(void *d, void *s1, void *s2);
extern void rowop_not(void *d, void *s);

/************/
/* row shift and mask wrapper function
/* Params
/*  d: destination row ptr
/*  s: source row ptr
/*  N: number of times to shift left
/*  mask: bitwise mask row ptr
/************/
void shift_n_mask(void *d, void *s, int N, void *mask) {
    for (int cnt = 0; cnt < N; cnt++){
        // Left shift 1
        rowop_shift((void *) d, (void *) s);
        // bitwise AND with mask
        rowop_and((void *) d, (void *) d, (void *) mask);
    }
    return;
}

/************/
/* row xor function
/* Params
/*  d: destination row ptr
/*  s1: source row ptr
/*  s2: source 2 row ptr
/*  per_col_bytes: stupid thing from their code
/************/
void rowop_xor(void *d, void *s1, void *s2, int per_col_bytes) {
    // A ^ B = (!A & B) | (A & !B)
    unsigned *scratchA, *scratchB;
    posix_memalign((void *) &scratchA, ALIGNMENT, per_col_bytes);
    posix_memalign((void *) &scratchB, ALIGNMENT, per_col_bytes);

    // !A
    rowop_not((void *) scratchA, (void *) s1);
    // !A & B
    rowop_and((void *) scratchA, (void *) scratchA, (void *) s2);
    // !B
    rowop_not((void *) scratchB, (void *) s2);
    // A & !B
    rowop_and((void *) scratchB, (void *) scratchB, (void *) s1);
    // XOR
    rowop_or((void *) d, (void *) scratchA, (void *) scratchB);

    free(scratchA);
    free(scratchB);
    return;    
}

/************/
/* row addition function
/* Params
/*  d: destination row ptr
/*  s1: source 1 row ptr
/*  s2: source 2 row ptr
/*  N: addition data-width
/*  per_col_bytes: stupid thing from their code
/************/
void row_add(void *d, void *s1, void *s2, int N, int per_col_bytes) {
    // Implements Kogge Stone fast-addition algorithm

    // Create bit-mask based on data-width
    unsigned *mask;
    posix_memalign((void *) &mask, ALIGNMENT, per_col_bytes);
    // ugh how to do this

    // "Front porch" pre processing step
    unsigned *and0, *xor0;
    posix_memalign((void *) &and0, ALIGNMENT, per_col_bytes);
    posix_memalign((void *) &xor0, ALIGNMENT, per_col_bytes);
    rowop_and((void *) and0, (void *) s1, (void *) s2);
    rowop_xor((void *) xor0, (void *) s1, (void *) s2, per_col_bytes);

    // Body operations
    unsigned *andN, *orN, *andNshift, *orNshift, *andTemp, *andNCopy;
    posix_memalign((void *) &andN, ALIGNMENT, per_col_bytes);
    posix_memalign((void *) &orN, ALIGNMENT, per_col_bytes);
    posix_memalign((void *) &andNshift, ALIGNMENT, per_col_bytes);
    posix_memalign((void *) &orNshift, ALIGNMENT, per_col_bytes);
    posix_memalign((void *) &andTemp, ALIGNMENT, per_col_bytes);
    posix_memalign((void *) &andNCopy, ALIGNMENT, per_col_bytes);
    for (int shift = 1; shift < N; shift*=2)
    {
        // left shift prior step results;
        (if shift == 1) { // special treatment for first itr
            shift_n_mask((void *) orNshift, (void *) and0, shift, (void *) mask);
            shift_n_mask((void *) andNshift, (void *) xor0, shift, (void *) mask);
        } else {
            shift_n_mask((void *) orNshift, (void *) orN, shift, (void *) mask);
            shift_n_mask((void *) andNshift, (void *) andN, shift, (void *) mask);
        }

        // Perform or_{N+1} = or_N | (or_shift_N & and_N)
        // Perform and_{N+1} = and_N & and_shift_N)

        (if shift == 1) { // special treatment for first itr
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
    shift_n_mask((void *) orNshift, (void *) orN, 1, (void *) mask);

    // "Back Porch" post processing steps
    rowop_xor((void *) d, (void *) xor0, (void *) orNshift, per_col_bytes);
    return;
}

int main(int argc, char **argv) {
    srand(121324314);

    int col_length = atoi(argv[1]);
    int num_vals = 1024 << (atoi(argv[2]));

    int total_bits = col_length * num_vals;
    int total_bytes = total_bits / 8;
    int total_ints = total_bits / 32;
    int total_vecs = total_bits / 128;

    int per_col_bits = num_vals;
    int per_col_bytes = num_vals / 8;
    int per_col_ints = num_vals / 32;
    int per_col_vecs = num_vals / 128;
    int per_col_rows = (per_col_bytes + ROW_SIZE - 1) / ROW_SIZE;

    unsigned c1, c2;
    int dummy;

    // generate c1 and c2;
    c1 = rand();
    c2 = rand();

    // allocate col data
    unsigned **vals;
    vals = malloc(col_length * sizeof(unsigned *));
    for (int i = 0; i < col_length; i ++)
        dummy = posix_memalign((void *) &(vals[i]), ALIGNMENT, per_col_bytes);

    // allocate output
    unsigned *output;
    dummy += posix_memalign((void *) &output, ALIGNMENT, per_col_bytes);

    unsigned result = 0;
    
    // allocate intermediate vectors
    unsigned *meq1, *meq2, *mlt, *mgt, *neg, *tmp;
    dummy += posix_memalign((void *) &meq1, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &meq2, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &mlt, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &mgt, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &neg, ALIGNMENT, per_col_bytes);
    dummy += posix_memalign((void *) &tmp, ALIGNMENT, per_col_bytes);
    
    // initialize col data
    for (int j = 0; j < col_length; j ++) {
        for (int i = 0; i < per_col_ints; i ++) {
            vals[j][i]  = rand();
        }
    }

    // set meq1 and meq2 to 1
    for (int i = 0; i < per_col_ints; i ++) {
        meq1[i] = 1;
        meq2[i] = 1;
        mlt[i] = 0;
        mgt[i] = 0;
    }
    
    // run the query
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

        
        for (int j = 0; j < col_length; j ++) {
            unsigned *vec = vals[j];
            FOR_ALL_ROWS { rowop_not(ROW(neg), ROW(vec)); }
            int test1 = !!(c1 & (1 << j));
            int test2 = !!(c2 & (1 << j));

            if (test1 == 0 && test2 == 0) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(tmp), ROW(meq1), ROW(vec));
                    rowop_or(ROW(mgt), ROW(mgt), ROW(tmp));
                    rowop_and(ROW(meq1), ROW(meq1), ROW(neg));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(neg));
                }
            }
            else if (test1 == 1 && test2 == 0) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(meq1), ROW(meq1), ROW(vec));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(neg));
                }
            }
            else if (test1 == 0 && test2 == 1) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(tmp), ROW(meq1), ROW(vec));
                    rowop_or(ROW(mgt), ROW(mgt), ROW(tmp));
                    rowop_and(ROW(tmp), ROW(meq2), ROW(neg));
                    rowop_or(ROW(mlt), ROW(mlt), ROW(tmp));
                    rowop_and(ROW(meq1), ROW(meq1), ROW(neg));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(vec));
                }
            }
            else if (test1 == 1 && test2 == 1) {
                FOR_ALL_ROWS {
                    rowop_and(ROW(tmp), ROW(meq2), ROW(neg));
                    rowop_or(ROW(mlt), ROW(mlt), ROW(tmp));
                    rowop_and(ROW(meq1), ROW(meq1), ROW(vec));
                    rowop_and(ROW(meq2), ROW(meq2), ROW(vec));
                }
            }
        }

        result = 0;
        FOR_ALL_ROWS {
            rowop_and(ROW(output), ROW(mlt), ROW(mgt));
            unsigned *vals = (unsigned *)(ROW(output));
            for (int j = 0; j < ROW_SIZE / 4; j ++)
                result += upopcount(vals[j]);
        }
    }
    m5_dump_stats(0,0);

    printf("%u\n", result);
    
    return 0;
}
