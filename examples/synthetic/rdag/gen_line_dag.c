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
#include "rand.h"

void print_usage_and_die(char** argv)
{
	fprintf(stderr, "Usage: %s -l num_lines -n num_nodes_per_line [-w weight] [-i min_cross_links] [-a max_cross_links] [-d]\n", argv[0]);
	exit(1);
}

int main(int argc, char** argv)
{
	int num_lines = -1;
	int num_nodes_per_line = -1;
	int weight = 1;
	int min_cross_links = 0;
	int max_cross_links = 0;
	int option;
	int dot = 0;

	while ((option = getopt(argc, argv, "l:n:w:i:a:d")) != -1)
	{
		switch(option)
		{
			case 'l':
				num_lines = atoi(optarg);
				break;
			case 'n':
				num_nodes_per_line = atoi(optarg);
				break;
			case 'w':
				weight = atoi(optarg);
				break;
			case 'i':
				min_cross_links = atoi(optarg);
				break;
			case 'a':
				max_cross_links = atoi(optarg);
				break;
			case 'd':
				dot = 1;
				break;
			case '?':
				print_usage_and_die(argv);
				break;
		}
	}

	if(num_lines == -1 || num_nodes_per_line == -1)
		print_usage_and_die(argv);

	if(min_cross_links > max_cross_links) {
		fprintf(stderr, "Minimal number of cross links greater than maximum.\n");
		exit(1);
	}

	if(dot)
		printf("digraph {\n");

	for(int i = 0; i < num_lines; i++)
		if(dot)
			printf("0 -> \"%d_0\" [label = \"1\"]\n", i);
		else
			printf("0 ->[1] %d_0\n", i);

	for(int i = 0; i < num_lines; i++)
		for(int j = 0; j < num_nodes_per_line-1; j++)
			if(dot)
				printf("\"%d_%d\" -> \"%d_%d\" [label = \"%d\"]\n", i, j, i, j+1, weight);
			else
				printf("%d_%d ->[%d] %d_%d\n", i, j, weight, i, j+1);

	for(int i = 0; i < num_lines; i++) {
		int cross_links = rand_interval_int32(min_cross_links, max_cross_links);
		int src_node;
		int dst_node;
		int dst_line;

		for(int lnk = 0; lnk < cross_links; lnk++) {
			do {
				src_node = rand_interval_int32(0, num_nodes_per_line-1);
				dst_node = src_node + (rand_interval_int32(0, num_nodes_per_line-1) % (num_nodes_per_line - src_node));
				dst_line = rand_interval_int32(0, num_lines-1);
			} while(src_node == dst_node && dst_line == i);

			if(dot)
				printf("\"%d_%d\" -> \"%d_%d\" [label = \"%d\"]\n", i, src_node, dst_line, dst_node, weight);
			else
				printf("%d_%d ->[%d] %d_%d\n", i, src_node, weight, dst_line, dst_node);
		}
	}

	if(dot)
		printf("}\n");

	return 0;
}
