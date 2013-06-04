#include "../libworkstream_df/config.h"
#include <stdio.h>

int main(int argc, char** argv)
{
	#ifdef WS_PAPI_PROFILE
	printf("yes\n");
	return 0;
	#else
	printf("no\n");
	return 1;
	#endif
}
