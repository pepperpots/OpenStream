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
#include <malloc.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include "rdag-common.h"
#include "../../common/common.h"

/* FILE* tasklist; */

struct stream_assign {
	void** streams;
	int curr_stream;
	int num_streams;
};

void assign_stream(struct arc* arc, void* data)
{
	struct stream_assign* sa = (struct stream_assign*)data;

	arc->data = sa->streams[sa->curr_stream];
	//printf("assign stream %d to arc %p\n", sa->curr_stream, arc);
	sa->curr_stream++;
}

void create_task(struct node* n, struct stream_assign* sa)
{
	int in_sz = is_root(n) ? 1 : n->in_arcs[0]->weight;
	char sin[n->num_in_arcs] __attribute__((stream_ref));
	char in[n->num_in_arcs][in_sz];

	int out_sz = is_leaf(n) ? 1 : n->out_arcs[0]->weight;
	int nout = is_leaf(n) ? 1 : n->num_out_arcs;
	char sout[nout] __attribute__((stream_ref));
	char out[nout][out_sz];

	for(int i = 0; i < n->num_in_arcs; i++)
		sin[i] = n->in_arcs[i]->data;

	if(is_leaf(n)) {
		sout[0] = sa->streams[(long)(n->data)];
	} else {
		for(int i = 0; i < n->num_out_arcs; i++)
			sout[i] = n->out_arcs[i]->data;
	}

	/* fprintf(tasklist, "Creating task %p\n", n); */
	/* fflush(tasklist); */

	n->created = 1;

	#pragma omp task input(sin >> in[n->num_in_arcs][in_sz]) output(sout << out[nout][out_sz])
	{
		/* fprintf(tasklist, "Running task %p\n", n); */
		/* fflush(tasklist); */

		int val = 0;

		for(int i = 0; i < n->num_in_arcs; i++)
			for(int j = 0; j < in_sz; j++)
				val ^= in[i][j];

		for(int i = 0; i < nout; i++)
			for(int j = 0; j < out_sz; j++)
				out[i][j] = val;

		for(int i = 0; i < n->num_create_nodes; i++) {
			create_task(n->create_nodes[i], sa);
		}
	}
}

void create_terminal_task(struct dag* g, struct stream_assign* sa)
{
	int in_sz = 1;
	char sin[g->num_leaves] __attribute__((stream_ref));
	char in[g->num_leaves][in_sz];

	for(int i = 0; i < g->num_leaves; i++)
		sin[i] = sa->streams[(long)(g->leaves[i]->data)];

	#pragma omp task input(sin >> in[g->num_leaves][in_sz])
	{
	}
}

void print_usage_and_die(char** argv)
{
	fprintf(stderr, "Usage: %s -i input_file\n", argv[0]);
	exit(1);
}


int main(int argc, char** argv)
{
	struct dag g;
	struct stream_assign sa;
	const char* in_file = NULL;
	int option;

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

	dag_init(&g);

	/* tasklist = fopen("tasks.list", "w+"); */

	printf("Reading DAG... "); fflush(stdout);
	dag_read_file(&g, in_file);
	printf("done\n"); fflush(stdout);

	FILE* fp = fopen("dag-before.dot", "w+");
	dag_dump_dot(fp, &g);
	fclose(fp);

	/* printf("Checking for cycles... "); fflush(stdout); */
	/* if(dag_detect_cycle(&g)) { */
	/* 	printf("\n"); */
	/* 	fprintf(stderr, "Cycle detected\n"); */
	/* 	exit(1); */
	/* } */
	/* printf("done\n"); fflush(stdout); */

	printf("Building creator rels... "); fflush(stdout);
	dag_build_creator_rels(&g);
	printf("done\n"); fflush(stdout);

	fp = fopen("dag.dot", "w+");
	dag_dump_dot(fp, &g);
	fclose(fp);

	fprintf(stderr, "%d nodes, %d arcs.\n", g.num_nodes, g.num_arcs);
	fprintf(stderr, "%d roots found.\n", g.num_roots);
	fprintf(stderr, "%d leaves found.\n", g.num_leaves);

	char streams[g.num_arcs + g.num_leaves] __attribute__((stream));
	sa.num_streams = g.num_arcs + g.num_leaves;
	sa.streams = malloc(sa.num_streams * sizeof(void*));
	sa.curr_stream = 0;
	memcpy(sa.streams, streams, sa.num_streams * sizeof(void*));

	dag_for_each_arc(&g, &sa, assign_stream);
	for(int i = 0; i < g.num_leaves; i++)
		g.leaves[i]->data = (void*)((long)(sa.curr_stream+i));

	gettimeofday (&start, NULL);
	for(int i = 0; i < g.num_roots; i++) {
		create_task(g.roots[i], &sa);

		for(int j = 0; j < g.roots[i]->num_out_arcs; j++) {
			struct node* child = g.roots[i]->out_arcs[j]->dst;

			if(!child->creator && !child->created)
				create_task(child, &sa);
		}
	}

	create_terminal_task(&g, &sa);
	#pragma omp taskwait

	gettimeofday (&end, NULL);
	printf ("%.5f\n", tdiff (&end, &start));fflush(stdout);

	dag_destroy(&g);
	free(sa.streams);
	/* fclose(tasklist); */

	return 0;
}
