/**
 * Copyright (C) 2014 Andi Drebes <andi.drebes@lip6.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include "rdag-common.h"
#include "../../common/common.h"

void print_usage_and_die(char** argv)
{
	fprintf(stderr, "Usage: %s -i input_file\n", argv[0]);
	exit(1);
}

int main(int argc, char** argv)
{
	int option;
	const char* in_file = NULL;

	struct timeval start;
	struct timeval end;

	while ((option = getopt(argc, argv, "i:")) != -1)
	{
		switch(option)
		{
			case 'i':
				in_file = optarg;
				break;
			case '?':
				print_usage_and_die(argv);
				break;
		}
	}

	if(!in_file)
		print_usage_and_die(argv);

	struct dag g;
	dag_init(&g);

	gettimeofday (&start, NULL);
	printf("Reading DAG... "); fflush(stdout);
	dag_read_file(&g, in_file);
	printf("done\n"); fflush(stdout);

	gettimeofday (&end, NULL);
	printf ("%.5f\n", tdiff (&end, &start));fflush(stdout);

	/* dag_dump_dot(stdout, &g); */
	dag_destroy(&g);

	return 0;
}
