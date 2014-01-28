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

#include "rdag-common.h"
#include "hash.h"
#include "rand.h"
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>

void dag_init(struct dag* g)
{
	g->root = NULL;
	g->curr_mark = 1;

	g->num_roots = 0;
	g->num_leaves = 0;

	g->roots = NULL;
	g->leaves = NULL;

	g->num_arcs = 0;
	g->num_nodes = 0;
}

int gen_rnd_tree_generic(struct dag* g, struct node* root, struct node* parent, struct arc* in_arc, int curr_depth, int depth, int min_succ, int max_succ, int min_weight, int max_weight)
{
	root->num_in_arcs = (parent) ? 1 : 0;
	root->num_out_arcs = (curr_depth == depth) ? 0 : rand_interval_int32(min_succ, max_succ);
	root->out_arcs = NULL;
	root->create_nodes = NULL;
	root->num_create_nodes = 0;
	root->creator = NULL;
	root->mark = 0;

	g->num_nodes++;

	if(in_arc) {
		root->in_arcs = malloc(sizeof(struct arc*));
		root->in_arcs[0] = in_arc;
	} else {
		root->in_arcs = NULL;
	}

	if(root->num_out_arcs) {
		root->out_arcs = malloc(sizeof(struct arc*) * root->num_out_arcs);
		g->num_arcs += root->num_out_arcs;

		for(int i = 0; i < root->num_out_arcs; i++) {
			struct arc* arc = malloc(sizeof(struct arc));
			struct node* succ = malloc(sizeof(struct node));

			root->out_arcs[i] = arc;

			if(!succ)
				return 1;

			root->out_arcs[i]->weight = rand_interval_int32(min_weight, max_weight);
			root->out_arcs[i]->data = NULL;
			root->out_arcs[i]->src = root;
			root->out_arcs[i]->dst = succ;

			if(gen_rnd_tree_generic(g, root->out_arcs[i]->dst, root, root->out_arcs[i], curr_depth+1, depth, min_succ, max_succ, min_weight, max_weight))
				return 1;
		}
	}

	return 0;
}

int dag_build_creator_rels_generic(struct node* root)
{
	int ncreate = 0;

	for(int i = 0; i < root->num_out_arcs; i++) {
		struct node* child = root->out_arcs[i]->dst;

		for(int j = 0; j < child->num_out_arcs; j++) {
			struct node* grandchild = child->out_arcs[j]->dst;

			if(!grandchild->creator)
				ncreate++;
		}
	}

	if(ncreate) {
		if(!(root->create_nodes = malloc(ncreate*sizeof(root->create_nodes[0]))))
			return 1;

		for(int i = 0; i < root->num_out_arcs; i++) {
			struct node* child = root->out_arcs[i]->dst;

			for(int j = 0; j < child->num_out_arcs; j++) {
				struct node* grandchild = child->out_arcs[j]->dst;

				if(!grandchild->creator) {
					root->create_nodes[root->num_create_nodes++] = grandchild;
					grandchild->creator = root;
				}
			}
		}
	}

	for(int i = 0; i < root->num_out_arcs; i++) {
		struct node* child = root->out_arcs[i]->dst;
		if(dag_build_creator_rels_generic(child))
			return 1;
	}

	return 0;
}

int dag_build_creator_rels(struct dag* g)
{
	for(int i = 0; i < g->num_roots; i++)
		if(dag_build_creator_rels_generic(g->roots[i]))
			return 1;

	return 0;
}

int gen_rnd_tree(struct dag* g, int depth, int min_succ, int max_succ, int min_weight, int max_weight)
{
	if(!(g->root = malloc(sizeof(struct node))))
		return 1;

	if(gen_rnd_tree_generic(g, g->root, NULL, NULL, 0, depth, min_succ, max_succ, min_weight, max_weight))
		return 1;

	if(dag_find_roots(g))
		return 1;

	if(dag_find_leaves(g))
		return 1;

	return 0;
}

int is_leaf(struct node* root)
{
	return (root->num_out_arcs == 0);
}

int is_root(struct node* root)
{
	return (root->num_in_arcs == 0);
}

int cmp_nodes_addr(const void* pa, const void* pb)
{
	const struct node** a = (const struct node**)pa;
	const struct node** b = (const struct node**)pb;

	if(*a < *b)
		return -1;
	if(*a == *b)
		return 0;

	return 1;
}

int arr_has_node(struct node** nodes, int num_nodes, struct node* needle)
{
	return (bsearch(&needle, nodes, num_nodes, sizeof(struct node*), cmp_nodes_addr) != NULL);
}

int dag_find_generic(struct dag* g, struct node* root, int (*predicate)(struct node*), int (*rec_fun)(struct dag* g, struct node* node), struct node*** arr, int* ctr)
{
	if(root->mark == g->curr_mark)
		return 0;

	root->mark = g->curr_mark;

	if(predicate(root) && !arr_has_node(*arr, *ctr, root)) {
		if(!(*arr = realloc(*arr, ((*ctr)+1)*sizeof(struct node*))))
			return 1;

		(*arr)[*ctr] = root;
		(*ctr)++;

		qsort(*arr, *ctr, sizeof(struct node*), cmp_nodes_addr);
	}

	for(int i = 0; i < root->num_out_arcs; i++) {
		struct node* child = root->out_arcs[i]->dst;
		if(rec_fun(g, child))
			return 1;
	}

	for(int i = 0; i < root->num_in_arcs; i++) {
		struct node* parent = root->in_arcs[i]->src;
		if(rec_fun(g, parent))
			return 1;
	}

	return 0;
}

int dag_find_roots_generic(struct dag* g, struct node* root)
{
	return dag_find_generic(g, root, is_root, dag_find_roots_generic, &g->roots, &g->num_roots);
}

int dag_find_roots(struct dag* g)
{
	g->curr_mark++;

	return dag_find_roots_generic(g, g->root);
}

int dag_find_leaves_generic(struct dag* g, struct node* root)
{
	return dag_find_generic(g, root, is_leaf, dag_find_leaves_generic, &g->leaves, &g->num_leaves);
}

int dag_find_leaves(struct dag* g)
{
	g->curr_mark++;

	return dag_find_leaves_generic(g, g->root);
}

void dag_dump_dot_generic(FILE* fp, struct dag* g, struct node* root)
{
	if(root->mark == g->curr_mark)
		return;

	root->mark = g->curr_mark;

	for(int i = 0; i < root->num_out_arcs; i++) {
		fprintf(fp, "\t\"%p\" -> \"%p\" [label = \"%d\"]\n", root, root->out_arcs[i]->dst, root->out_arcs[i]->weight);
		dag_dump_dot_generic(fp, g, root->out_arcs[i]->dst);
	}

	for(int i = 0; i < root->num_create_nodes; i++) {
		fprintf(fp, "\t\"%p\" -> \"%p\" [style = \"dotted\"]\n", root, root->create_nodes[i]);
	}
}

void dag_dump_dot(FILE* fp, struct dag* g)
{
	g->curr_mark++;
	fprintf(fp, "digraph {\n");

	for(int i = 0; i < g->num_roots; i++)
		dag_dump_dot_generic(fp, g, g->roots[i]);
	fprintf(fp, "}\n");
}

void dag_destroy_generic(struct dag* g, struct node* root)
{
	for(int i = 0; i < root->num_out_arcs; i++) {
		struct node* child = root->out_arcs[i]->dst;

		if(child)
			dag_destroy_generic(g, child);
	}

	for(int i = 0; i < root->num_in_arcs; i++) {
		root->in_arcs[i]->dst = NULL;
	}

	for(int i = 0; i < root->num_out_arcs; i++)
		free(root->out_arcs[i]);

	free(root->out_arcs);
	free(root->in_arcs);
	free(root->create_nodes);
	free(root);
}

void dag_destroy(struct dag* g)
{
	for(int i = 0; i < g->num_roots; i++)
		dag_destroy_generic(g, g->roots[i]);

	free(g->roots);
	free(g->leaves);
}

void dag_for_each_arc_generic(struct dag* g, struct node* root, void* data, void (*fun)(struct arc* arc, void* data))
{
	if(root->mark == g->curr_mark)
		return;

	root->mark = g->curr_mark;

	for(int i = 0; i < root->num_out_arcs; i++) {
		fun(root->out_arcs[i], data);
		dag_for_each_arc_generic(g, root->out_arcs[i]->dst, data, fun);
	}
}

void dag_for_each_arc(struct dag* g, void* data, void (*fun)(struct arc* arc, void* data))
{
	g->curr_mark++;

	for(int i = 0; i < g->num_roots; i++)
		dag_for_each_arc_generic(g, g->roots[i], data, fun);
}

int get_next_token(FILE* fp, char* token, int* line)
{
	int offs = 0;
	int c;
	int r = 0;

	while(!feof(fp)) {
		c = fgetc(fp);

		if(c == '\n')
			(*line)++;

		if(c == '#') {
			while((c = fgetc(fp)) != '\n' && !feof(fp));
			(*line)++;
		}

		if(!isblank(c) && c != '\n' && c != EOF) {
			r = 1;
			token[offs++] = (char)c;
		} else {
			if(r)
				goto out;
		}
	}

out:
	token[offs++] = '\0';
	return offs != 1;
}

struct node_str {
	char* str;
	int len;
	struct node* node;
};

int hash_table_hash_node_str(const void* pstr)
{
	const struct node_str* nstr = pstr;

	int val = nstr->len << 8;
	const char* str = nstr->str;

	while(*str) {
		val ^= *str;
		str++;
	}

	return val;
}

int hash_table_cmp_node_str(const void* pa, const void* pb)
{
	const struct node_str* nstra = pa;
	const struct node_str* nstrb = pb;

	if(nstra->len == nstrb->len)
		return strcmp(nstra->str, nstrb->str) == 0;

	return 0;
}

void free_node_str(void* d)
{
	const struct node_str* nstr = d;
	free(nstr->str);
	free(d);
}

struct node_str* hash_table_lookup_insert_node_str(struct hash_table* ht, char* token)
{
	struct node_str nstr;
	struct node_str* pnstr;
	int len = strlen(token);

	nstr.str = token;
	nstr.len = len;
	nstr.node = NULL;

	if(!(pnstr = hash_table_lookup(ht, &nstr))) {
		if(!(pnstr = malloc(sizeof(*pnstr))))
			return NULL;

		if(!(pnstr->str = malloc(len+1))) {
			free(pnstr);
			return NULL;
		}

		strcpy(pnstr->str, token);
		pnstr->str[len] = '\0';
		pnstr->len = len;
		pnstr->node = NULL;

		if(hash_table_insert(ht, pnstr)) {
			free_node_str(pnstr);
			return NULL;
		}
	}

	return pnstr;
}

struct node* dag_create_node(struct dag* g)
{
	struct node* n;

	if(!(n = malloc(sizeof(struct node))))
		return NULL;

	n->num_in_arcs = 0;
	n->num_out_arcs = 0;
	n->mark = 0;
	n->in_arcs = NULL;
	n->out_arcs = NULL;
	n->create_nodes = NULL;
	n->num_create_nodes = 0;
	n->creator = NULL;
	n->data = NULL;

	if(g->root == NULL)
		g->root = n;

	g->num_nodes++;

	return n;
}

int dag_create_edge(struct dag* g, struct node* a, struct node* b, int weight)
{
	struct arc** out_arcs_new = realloc(a->out_arcs, (a->num_out_arcs+1)*sizeof(struct arc*));

	if(!out_arcs_new)
		return 1;

	a->out_arcs = out_arcs_new;
	a->out_arcs[a->num_out_arcs] = NULL;

	struct arc** in_arcs_new = realloc(b->in_arcs, (b->num_in_arcs+1)*sizeof(struct arc*));

	if(!in_arcs_new)
		return 1;

	b->in_arcs = in_arcs_new;
	b->in_arcs[b->num_in_arcs] = NULL;

	struct arc* arc = malloc(sizeof(struct arc));

	if(!arc)
		return 1;

	arc->weight = weight;
	arc->data = NULL;
	arc->src = a;
	arc->dst = b;

	a->out_arcs[a->num_out_arcs++] = arc;
	b->in_arcs[b->num_in_arcs++] = arc;

	g->num_arcs++;

	return 0;
}

int dag_read_file(struct dag* g, const char* filename)
{
	char left_token[128];
	char middle_token[128];
	char right_token[128];

	struct hash_table ht;
	struct node_str* pnstr_left;
	struct node_str* pnstr_right;

	int line = 0;
	int err = 1;
	FILE* fp = fopen(filename, "r");
	int weight;

	if(!fp)
		goto out_err;

	if(hash_table_create(&ht, 5000, hash_table_hash_node_str, hash_table_cmp_node_str))
		goto out_fp;

	while(1) {
		weight = 1;

		if(!get_next_token(fp, left_token, &line))
			goto out_ok;

		if(!get_next_token(fp, middle_token, &line))
			goto out_ht;

		if(strncmp(middle_token, "->", 2) != 0)
			goto out_ht;

		int mid_len = strlen(middle_token);

		if(mid_len != 2) {
			if(mid_len < 5 || middle_token[2] != '[' || middle_token[mid_len-1] != ']')
				goto out_ht;
			else
				sscanf(&middle_token[3], "%d", &weight);
		}

		if(!get_next_token(fp, right_token, &line))
			goto out_ht;

		pnstr_left = hash_table_lookup_insert_node_str(&ht, left_token);
		pnstr_right = hash_table_lookup_insert_node_str(&ht, right_token);

		if(!pnstr_left->node)
			if(!(pnstr_left->node = dag_create_node(g)))
				goto out_ht;

		if(!pnstr_right->node)
			if(!(pnstr_right->node = dag_create_node(g)))
				goto out_ht;

		if(dag_create_edge(g, pnstr_left->node, pnstr_right->node, weight))
			goto out_ht;
	}

out_ok:
	dag_find_roots(g);
	dag_find_leaves(g);

	err = 0;

out_ht:
	hash_table_destroy(&ht, free_node_str);
out_fp:
	fclose(fp);
out_err:
	return err;
}
