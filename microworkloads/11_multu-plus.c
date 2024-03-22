#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <immintrin.h>
#include <m5op.h>
#include "mimdram.h"

int main(int argc, char **argv) {
    init_ambit();
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

    // generate col data
    unsigned **vals1 = random_vector_array(per_col_bytes, col_length);
    unsigned **vals2 = random_vector_array(per_col_bytes, col_length);

    // allocate output
    unsigned **output = allocate_vector_array(per_col_bytes, 2*col_length);

	// run some iterations of the algorithm
    for (int iter = 0; iter < 2; iter ++) {
        m5_reset_stats(0,0);

		FOR_ALL_VECTORS {
			// initialize output
			unsigned *v1 = VECTOR(vals1[0]);
			for(int k = 0; k < col_length; k ++) {
				unsigned *v2 = VECTOR(vals2[k]);
				unsigned *out = VECTOR(output[k]);

				AAP_VECTORS (B_T0, v1        )
				AAP_VECTORS (B_T1, v2        )
				AAP_VECTORS (B_T2, C_0       )
				AAP_VECTORS (out , B_T0_T1_T2)
			}
			
			AAP_VECTORS (VECTOR(output[col_length]), C_0);
			
			// Compute the rest of the multiplication
			for(int j = 1; j < col_length; j ++) {
				unsigned *v1 = VECTOR(vals1[j]);
				
				AAP_VECTORS (B_DCC1, C_0)
				for(int k = 0; k < col_length; k ++) {
					unsigned *v2 = VECTOR(vals2[k]);
					unsigned *out = VECTOR(output[j + k]);
					
					AAP_VECTORS (B_T0        , v1        )
					AAP_VECTORS (B_T1        , v2        )
					AAP_VECTORS (B_T2        , C_0       )
					AP_VECTOR   (B_T0_T1_T2              )
					AAP_VECTORS (B_T2_T3     , B_DCC1    )
					AAP_VECTORS (B_DCC1      , out       )
					AP_VECTOR   (B_DCC1_T0_T3            )
					AAP_VECTORS (B_T0_T3     , B_DCC1N   )
					AP_VECTOR   (B_T0_T1_T2              )
					AAP_VECTORS (B_T1        , out       )
					AAP_VECTORS (out         , B_T1_T2_T3)
				}
				AAP_VECTORS (VECTOR(output[j + col_length]), B_DCC1)
			}
		
					
		}

    }
    m5_dump_stats(0,0);

    return 0;
}
