#ifndef HASH_H
#define HASH_H

#include <malloc.h>

struct hash_table_entry {
	void* data;
	struct hash_table_entry* next;
};

struct hash_table {
	int num_buckets;
	int (*hash_fun)(const void* data);
	int (*cmp_fun)(const void* a, const void* b);
	struct hash_table_entry** buckets;
};

static inline int hash_table_create(struct hash_table* ht,
				    int num_buckets,
				    int (*hash_fun)(const void* data),
				    int (*cmp_fun)(const void* a, const void* b))
{
	ht->num_buckets = num_buckets;
	ht->hash_fun = hash_fun;
	ht->cmp_fun = cmp_fun;

	if(!(ht->buckets = malloc(num_buckets * sizeof(struct hash_table_entry*))))
		return 1;

	for(int i = 0; i < num_buckets; i++)
		ht->buckets[i] = NULL;

	return 0;
}

static inline void* hash_table_lookup(struct hash_table* ht, void* data)
{
	int bucket = ht->hash_fun(data) % ht->num_buckets;

	for(struct hash_table_entry* curr = ht->buckets[bucket]; curr; curr = curr->next)
		if(ht->cmp_fun(data, curr->data))
			return curr->data;

	return NULL;
}

static inline int hash_table_insert(struct hash_table* ht, void* data)
{
	struct hash_table_entry* e = malloc(sizeof(struct hash_table_entry));
	int bucket = ht->hash_fun(data) % ht->num_buckets;

	if(!e)
		return 1;

	e->data = data;
	e->next = ht->buckets[bucket];
	ht->buckets[bucket] = e;

	return 0;
}

static inline void hash_table_destroy(struct hash_table* ht, void (*free_fun)(void* d))
{
	for(int i = 0; i < ht->num_buckets; i++) {
		while(ht->buckets[i]) {
			struct hash_table_entry* curr = ht->buckets[i];
			struct hash_table_entry* next = curr->next;

			if(free_fun)
				free_fun(curr->data);

			free(curr);
			ht->buckets[i] = next;
		}
	}

	free(ht->buckets);
}

int hash_table_hash_str(const void* pstr);
int hash_table_cmp_str(const void* pa, const void* pb);

#endif
