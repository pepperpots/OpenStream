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
#include "../rand.h"

void print_usage_and_die(char** argv)
{
	fprintf(stderr, "Usage: %s -f num_forks -l num_lines -n num_nodes_per_line [-w edge_weight]  [-d]\n", argv[0]);
	exit(1);
}

int main(int argc, char** argv)
{
	int num_lines = -1;
	int num_nodes_per_line = -1;
	int weight = 1;
	int option;
	int dot = 0;
	int num_forks = 2;

	while ((option = getopt(argc, argv, "f:l:n:dw:")) != -1)
	{
		switch(option)
		{
			case 'l':
				num_lines = atoi(optarg);
				break;
			case 'n':
				num_nodes_per_line = atoi(optarg);
				break;
			case 'd':
				dot = 1;
				break;
			case 'w':
				weight = atoi(optarg);
				break;
			case 'f':
				num_forks = atoi(optarg)+1;
				break;
			case '?':
				print_usage_and_die(argv);
				break;
		}
	}

	if(num_lines == -1 || num_nodes_per_line == -1)
		print_usage_and_die(argv);

	if(dot)
		printf("digraph {\n");

	for(int i = 0; i < num_forks-1; i++) {
		for(int j = 0; j < num_lines; j++) {
			if(dot)
				printf("f%d -> \"f%d_%d_0\" [label = \"%d\"]\n", i, i, j, weight);
			else
				printf("f%d ->[%d] f%d_%d_0\n", i, weight, i, j);

			for(int k = 0; k < num_nodes_per_line-1; k++) {
				if(dot)
					printf("f%d_%d_%d -> \"f%d_%d_%d\" [label = \"%d\"]\n", i, j, k, i, j, k+1, weight);
				else
					printf("f%d_%d_%d ->[%d] f%d_%d_%d\n", i, j, k, weight, i, j, k+1);
			}

			if(dot)
				printf("\"f%d_%d_%d\" -> f%d [label = \"%d\"]\n", i, j, num_nodes_per_line-1, i+1, weight);
			else
				printf("f%d_%d_%d ->[%d] f%d\n", i, j, num_nodes_per_line-1, weight, i+1);
		}
	}

	if(dot)
		printf("}\n");

	return 0;
}
