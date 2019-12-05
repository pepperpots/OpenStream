#ifndef HWLOC_SUPPORT_H_
#define HWLOC_SUPPORT_H_

#include <hwloc.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

extern unsigned num_numa_nodes;
extern unsigned topology_depth;

// Initializes the hwloc support by discovereing the current machine topology
bool discover_machine_topology(void);

// Get the identifier of the numa node for a given processor unit id
unsigned numa_node_of_pu(unsigned pu_id);

const char *level_name_hwloc(unsigned level);

unsigned num_available_processing_units(void);

bool restrict_topology_to_glibc_cpuset(cpu_set_t set);

void check_bond_to_cpu(pthread_t tid, hwloc_obj_t cpu);

cpu_set_t object_glibc_cpuset(hwloc_obj_t obj);

bool distribute_worker_on_topology(unsigned num_workers,
                                   hwloc_obj_t **processing_units);

void print_topology_tree(FILE *where);

int bind_memory_to_numa_node(const void *addr, size_t len, unsigned node_index);

int bind_memory_to_cpu_memspace(const void *addr, size_t len, hwloc_obj_t cpu);

int interleave_memory_on_machine_nodes(const void *addr, size_t len);

hwloc_nodeset_t numa_memlocation_of_memory(const void *addr, size_t len);

unsigned level_of_common_ancestor(const hwloc_obj_t obj1, const hwloc_obj_t obj2);

unsigned numa_node_of_processing_unit(const hwloc_obj_t obj);

#endif // HWLOC_SUPPORT_H_