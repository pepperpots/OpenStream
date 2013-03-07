#include "../libworkstream_df/config.h"
#include <stdio.h>

int main(int argc, char** argv)
{
	for(int level = 0; level < MEM_NUM_LEVELS; level++) {
		puts(mem_level_name(level));
	}

	return 0;
}
