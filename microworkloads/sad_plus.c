#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include <time.h>
#include <omp.h>
#include "mimdram.h"

// Number of complete values
#ifndef SIZE
    #define SIZE 1024
#endif

// Data-width of values
#ifndef DWIDTH
    #define DWIDTH 32
#endif

// Select CPU engagement
#ifndef SELECT
    #define SELECT 2
#endif

extern void rowop_and(void *d, void *s1, void *s2);
extern void rowop_or(void *d, void *s1, void *s2);
extern void rowop_xor(void *d, void *s1, void *s2);
extern void rowop_not(void *d, void *s);
extern void rowop_shift(void *d, void *s);
extern void rowop_shift_right(void *d, void *s);

/************/
/* row shift and mask wrapper function
/* Params
/*  d: destination row ptr
/*  s: source row ptr
/*  N: number of times to shift left
/*  mask: bitwise mask row ptr
/************/
void rowop_shift_n_mask(void *d, void *s, int N, void *mask) {
  for (int cnt = 0; cnt < N; cnt++){
    // Left shift 1
    rowop_shift((void *) d, ((cnt==0) ? (void *) s : (void *) s));

    // bitwise AND with mask
    rowop_and((void *) d, (void *) d, (void *) mask);
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
  // Implements Kogge Stone fast-addition algorithm

  // Create bit-mask based on data-width
  unsigned *mask;
  mask = malloc(sizeof(unsigned *));
  posix_memalign((void *) &mask, ALIGNMENT, row_bytes);
  for (int i = 0; i < row_bytes/(N/8)*(N/32); i ++) {
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
    // Create one's-mask based on data-width
    unsigned *ones;    
    ones = malloc(sizeof(unsigned *));
    posix_memalign((void *) &ones, ALIGNMENT, row_bytes);
    for (int i = 0; i < row_bytes/(N/8)*(N/32); i ++) {
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
/* row abs val function
/* Params
/*  d: destination row ptr
/*  s: source row ptr
/*  N: addition data-width
/*  row_bytes: stupid thing from their code
/************/
void row_abs_val(void *d, void *s, int N, int row_bytes) {
    // Create one's-mask based on data-width
    unsigned *mask;    
    mask = malloc(sizeof(unsigned *));
    posix_memalign((void *) &mask, ALIGNMENT, row_bytes);
    for (int i = 0; i < row_bytes/(N/8)*(N/32); i ++) {
        mask[i] = 1 << (N-1); // mask all but MSB
    }

    // Sign Extend
    unsigned *sign_extend;    
    posix_memalign((void *) &sign_extend, ALIGNMENT, row_bytes);
    rowop_and((void *) sign_extend, (void *) s, (void *) mask);
    for (int i = 0; i < N; i++) {
        rowop_shift_right((void *) sign_extend, (void *) sign_extend);
        rowop_or((void *) mask, (void *) mask, (void *) sign_extend);
    }

    // Use mask to capture negative values, then invert
    rowop_and((void *) sign_extend, (void *) s, (void *) mask);
    row_twos_comp((void *) sign_extend, (void *) sign_extend, N, row_bytes);

    // Invert mask, and use mask to write non-negative values
    rowop_not((void *) mask, (void *) mask);
    rowop_and((void *) d, (void *) s, (void *) mask);

    // Copy inverted values
    rowop_or((void *) d, (void *) d, (void *) sign_extend);
   
    free(mask);

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
  unsigned *temp;
  posix_memalign((void *) &temp, ALIGNMENT, row_bytes);

  // Binary Reduction
  for (int shift = N; shift < ROW_SIZE; shift *= 2) {
   if (shift == N) { // If first round take from source, else use running sum
      for (int i = 0; i < shift; i++)
        rowop_shift((void *) temp, ((i==0) ? (void *) s : (void *) temp));
    } else {
      for (int i = 0; i < shift; i++) 
        rowop_shift((void *) temp, ((i==0) ? (void *) d : (void *) temp));
    }
    row_add((void *) d, (void *) d, (void *) temp, N, row_bytes);
  }

  return;
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

// reduce a vector to a single value by summing all elements
uint32_t vec_abs_val_red(uint32_t* vec, uint32_t len) {
    uint32_t sum = 0;
    // loop is executed in parallel, with each thread adding up a portion of the elements
    // http://jakascorner.com/blog/2016/06/omp-for-reduction.html
    #pragma omp parallel for reduction(+:sum)
    for (uint32_t i = 0; i < len; i++) {
        sum += abs(vec[i]);
    }
    return sum;
}

int main(int argc, char **argv) {
    #ifdef DEBUG
    printf("Debugging Mode\n");
    #endif

    srand(121324314);

    /********* SETUP*********/
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
    for (int i = 0; i < row_ints*(DWIDTH/32); i ++) {
        vec1[i] = rand();
        vec2[i] = rand();
    }

    /********* Run Query *********/
    m5_reset_stats(0,0);

    // Difference in RAM
    row_twos_comp((void *) twoComp, (void *) vec2, DWIDTH, row_bytes);
    row_add((void *) diff, (void *) vec1, (void *) twoComp, DWIDTH, row_bytes);

    // Select CPU engagement
    switch ((int) SELECT) {
        case 0:
            row_abs_val((void *) diff, (void *) diff, DWIDTH, row_bytes);
            row_reduce((void *) output, (void *) diff, DWIDTH, row_bytes);
            result = output[0];
            break;
        case 1:
            row_abs_val((void *) diff, (void *) diff, DWIDTH, row_bytes);
            result = vec_red(diff, SIZE);
            break;
        case 2:
            result = vec_abs_val_red(diff, SIZE);
            break;
        default:
            result = vec_abs_val_red(diff, SIZE);
            break;
    }

    m5_dump_stats(0,0);

    printf("Sum of absolute differences: %d\n", result);

    free(vec1);
    free(vec2);
    
    return 0;
}
