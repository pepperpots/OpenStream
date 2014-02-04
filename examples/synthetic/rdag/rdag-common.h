#ifndef RDAG_COMMON_H
#define RDAG_COMMON_H

#include <stdio.h>

#define ARC_PREALLOC 5
#define CREATE_PREALLOC 5

struct node;
struct arc {
	int weight;
	void* data;
	struct node* src;
	struct node* dst;
};

struct node {
	int num_in_arcs;
	int num_in_arcs_free;
	int num_out_arcs;
	int num_out_arcs_free;
	int mark;
	struct arc** in_arcs;
	struct arc** out_arcs;
	struct node** create_nodes;
	int num_create_nodes;
	int num_create_nodes_free;
	struct node* creator;
	int depth;
	int created;

	char name[32];
	void* data;
};

struct dag {
	int num_nodes;
	int num_arcs;

	struct node* root;
	int curr_mark;

	struct node** roots;
	int num_roots;

	struct node** leaves;
	int num_leaves;
};

void dag_init(struct dag* g);
int gen_rnd_tree(struct dag* g, int depth, int min_succ, int max_succ, int min_weight, int max_weight);
void dag_destroy(struct dag* g);
int dag_find_leaves(struct dag* g);
int dag_find_roots(struct dag* g);
int dag_build_creator_rels(struct dag* g);
void dag_dump_dot(FILE* fp, struct dag* g);
void dag_for_each_arc(struct dag* g, void* data, void (*fun)(struct arc* arc, void* data));
int dag_read_file(struct dag* g, const char* filename);
int dag_detect_cycle(struct dag* g);

struct node* dag_create_node(struct dag* g, const char* name);

int is_leaf(struct node* root);
int is_root(struct node* root);

#endif
