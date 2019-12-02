#include <stdlib.h>

extern "C" {

int wstream_df_interleave_data(void* p, size_t size);
int wstream_df_alloc_on_node(void* p, size_t size, int node);

}
