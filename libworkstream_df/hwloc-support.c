#include <hwloc.h>
#include <hwloc/glibc-sched.h>
#include <stdbool.h>
#include <stdlib.h>

#include "config.h"
#include "error.h"
#include "hwloc-support.h"
#include "hwloc/bitmap.h"

static hwloc_topology_t machine_topology;
unsigned num_numa_nodes;
unsigned topology_depth;
static unsigned *cpuid_to_closest_numa_node;
unsigned pu_latency_matrix_size;
void *pu_latency_distances_arr__;
unsigned pu_bandwidth_matrix_size;
void *pu_bandwidth_distances_arr__;

static void print_topology_node_and_childrens(hwloc_obj_t node,
                                              unsigned indent);

static void populate_closest_numa_nodes(void) {
  for (hwloc_obj_t pu =
           hwloc_get_next_obj_by_type(machine_topology, HWLOC_OBJ_PU, NULL);
       pu;
       pu = hwloc_get_next_obj_by_type(machine_topology, HWLOC_OBJ_PU, pu)) {
    hwloc_obj_t closest_numa_node = NULL;
    unsigned node_index;
    hwloc_bitmap_foreach_begin(node_index, pu->nodeset);
    hwloc_obj_t currNode =
        hwloc_get_numanode_obj_by_os_index(machine_topology, node_index);
    if (closest_numa_node) {
      if (closest_numa_node->depth < currNode->depth)
        closest_numa_node = currNode;
    } else {
      closest_numa_node = currNode;
    }
    hwloc_bitmap_foreach_end();
    cpuid_to_closest_numa_node[pu->logical_index] = closest_numa_node->os_index;
  }
}

static void alloc_distance_matrix(unsigned num_pu, void **arr,
                                  unsigned *size_store) {
  *arr = calloc(1, sizeof(unsigned[num_pu][num_pu]));
  *size_store = num_pu;
}

static void free_distance_matrices(void) {
  pu_latency_matrix_size = 0;
  free(pu_latency_distances_arr__);
  pu_latency_distances_arr__ = NULL;

  pu_bandwidth_matrix_size = 0;
  free(pu_bandwidth_distances_arr__);
  pu_bandwidth_distances_arr__ = NULL;
}

#if HWLOC_PRINT_DISTANCE_MATRICES
static inline unsigned unsignedlog10(unsigned x) {
  unsigned logval = 0u;
  while (x != 0u) {
    x /= 10u;
    logval += 1u;
  }
  return logval;
}

static inline unsigned unsignedmaximum(unsigned x, unsigned y) {
  return x > y ? x : y;
}

static void print_distance_matrix(FILE *out, unsigned size,
                                  unsigned (*matrix)[size]) {
  unsigned biggest_number_pow10 = unsignedlog10(size);
  for (unsigned i = 0; i < size; ++i) {
    for (unsigned j = 0; j < size; ++j) {
      biggest_number_pow10 =
          unsignedmaximum(biggest_number_pow10, unsignedlog10(matrix[i][j]));
    }
  }
  fprintf(out, "%*s  ", biggest_number_pow10, "");
  for (unsigned j = 0; j < size; ++j) {
    if (j > 0)
      fprintf(out, " ");
    fprintf(out, "%*u", biggest_number_pow10, j);
  }
  fprintf(out, "\n%*s -", biggest_number_pow10, "");
  for (unsigned j = 0; j < size; ++j) {
    if (j > 0)
      fprintf(out, "-");
    for (unsigned k = 0; k < biggest_number_pow10; ++k)
      fprintf(out, "-");
  }
  for (unsigned i = 0; i < size; ++i) {
    fprintf(out, "-\n%*u |", biggest_number_pow10, i);
    for (unsigned j = 0; j < size; ++j) {
      if (j > 0)
        fprintf(out, "|");
      fprintf(out, "%*u", biggest_number_pow10, matrix[i][j]);
    }
    fprintf(out, "|\n%*s -", biggest_number_pow10, "");
    for (unsigned j = 0; j < size; ++j) {
      if (j > 0)
        fprintf(out, "-");
      for (unsigned k = 0; k < biggest_number_pow10; ++k)
        fprintf(out, "-");
    }
  }
  fprintf(out, "-\n");
}
#else // !HWLOC_PRINT_DISTANCE_MATRICES
#define print_distance_matrix(a, b, c)                                         \
  do {                                                                         \
  } while (0)
#endif // HWLOC_PRINT_DISTANCE_MATRICES

static void populate_distance_matrix_for_pu_from_numa_distances(
    struct hwloc_distances_s *distances, unsigned num_pu,
    unsigned (*dist_array)[num_pu]) {
  for (unsigned i = 0; i < distances->nbobjs; ++i) {
    for (unsigned j = i; j < distances->nbobjs; ++j) {
      hwloc_obj_t numa_node_i = distances->objs[i];
      unsigned pu_i_os_index;
      hwloc_bitmap_foreach_begin(pu_i_os_index, numa_node_i->cpuset);
      hwloc_obj_t pu_i =
          hwloc_get_pu_obj_by_os_index(machine_topology, pu_i_os_index);
      if (cpuid_to_closest_numa_node[pu_i->logical_index] ==
          numa_node_i->os_index) {
        unsigned pu_j_os_index;
        hwloc_obj_t numa_node_j = distances->objs[j];
        hwloc_bitmap_foreach_begin(pu_j_os_index, numa_node_j->cpuset);
        hwloc_obj_t pu_j =
            hwloc_get_pu_obj_by_os_index(machine_topology, pu_j_os_index);
        if (cpuid_to_closest_numa_node[pu_j->logical_index] ==
            numa_node_j->os_index) {
          dist_array[pu_i->logical_index][pu_j->logical_index] =
              distances->values[i * distances->nbobjs + j];
          dist_array[pu_j->logical_index][pu_i->logical_index] =
              distances->values[j * distances->nbobjs + i];
        }
        hwloc_bitmap_foreach_end();
      }
      hwloc_bitmap_foreach_end();
    }
  }
}

static bool retrieve_numa_distances() {
  unsigned num_distances = 2;
  struct hwloc_distances_s *distancesMatrices[2];

  if (hwloc_distances_get_by_type(machine_topology, HWLOC_OBJ_NUMANODE,
                                  &num_distances, distancesMatrices, 0,
                                  0) != 0) {
    hwloc_topology_destroy(machine_topology);
    return false;
  }
  unsigned nproc = num_available_processing_units();

  if (num_distances) { // NUMA With Latency matrix
    for (unsigned i = 0; i < num_distances; ++i) {
      if (distancesMatrices[i]->kind & HWLOC_DISTANCES_KIND_MEANS_LATENCY) {
#if HWLOC_VERBOSE
        if (distancesMatrices[i]->kind & HWLOC_DISTANCES_KIND_FROM_OS) {
          fprintf(stdout,
                  "[HWLOC] OS provided latencies between NUMA nodes.\n");
        }
#endif // HWLOC_VERBOSE
        alloc_distance_matrix(nproc, &pu_latency_distances_arr__,
                              &pu_latency_matrix_size);
        populate_distance_matrix_for_pu_from_numa_distances(
            distancesMatrices[i], nproc, pu_latency_distances_arr__);
        print_distance_matrix(stdout, nproc, pu_latency_distances_arr__);
      }
      if (distancesMatrices[i]->kind & HWLOC_DISTANCES_KIND_MEANS_BANDWIDTH) {
        if (distancesMatrices[i]->kind & HWLOC_DISTANCES_KIND_FROM_OS) {
#if HWLOC_VERBOSE
          fprintf(stdout,
                  "[HWLOC] OS provided bandwidth between NUMA nodes.\n");
#endif // HWLOC_VERBOSE
        }
        alloc_distance_matrix(nproc, &pu_bandwidth_distances_arr__,
                              &pu_bandwidth_matrix_size);
        populate_distance_matrix_for_pu_from_numa_distances(
            distancesMatrices[i], nproc, pu_bandwidth_distances_arr__);
        print_distance_matrix(stdout, nproc, pu_bandwidth_distances_arr__);
      }
      hwloc_distances_release(machine_topology, distancesMatrices[i]);
    }
  } else { // UMA
#if HWLOC_VERBOSE
    fprintf(stdout, "[HWLOC] No distance matrices provided by the OS, assuming "
                    "UMA machine.\n");
#endif // HWLOC_VERBOSE
    alloc_distance_matrix(nproc, &pu_latency_distances_arr__,
                          &pu_latency_matrix_size);
    for (unsigned i = 0; i < nproc; ++i) {
      for (unsigned j = 0; j < nproc; ++j) {
        pu_latency_distances[i][j] = 1u;
      }
    }
    print_distance_matrix(stdout, nproc, pu_latency_distances_arr__);
  }
  return true;
}

bool discover_machine_topology(void) {
  if (hwloc_topology_init(&machine_topology) != 0) {
    return false;
  }
  // Get the topology available inside this process
  hwloc_topology_set_flags(
      machine_topology, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM |
                            HWLOC_TOPOLOGY_FLAG_THISSYSTEM_ALLOWED_RESOURCES);
  hwloc_topology_set_type_filter(machine_topology,
                                 HWLOC_OBJ_BRIDGE | HWLOC_OBJ_PCI_DEVICE |
                                     HWLOC_OBJ_MISC,
                                 HWLOC_TYPE_FILTER_KEEP_NONE);
  if (hwloc_topology_load(machine_topology) != 0) {
    hwloc_topology_destroy(machine_topology);
    return false;
  }

  num_numa_nodes =
      hwloc_get_nbobjs_by_type(machine_topology, HWLOC_OBJ_NUMANODE);
  topology_depth = hwloc_topology_get_depth(machine_topology);

  unsigned nproc = num_available_processing_units();
  cpuid_to_closest_numa_node =
      malloc(nproc * sizeof(*cpuid_to_closest_numa_node));
  populate_closest_numa_nodes();

#if HWLOC_VERBOSE
  fprintf(stdout,
          "[HWLOC Info] The machine has a depth of %d\n"
          "[HWLOC Info] The machine has %d numa node(s)\n",
          topology_depth, num_numa_nodes);
#endif

  return retrieve_numa_distances();
}

unsigned num_available_processing_units(void) {
  return hwloc_get_nbobjs_by_type(machine_topology, HWLOC_OBJ_PU);
}

// Get the identifier of the numa node for a given processor unit id
unsigned numa_node_of_pu(unsigned pu_id) { return 0; }

const char *level_name_hwloc(unsigned level) {
  return hwloc_obj_type_string(hwloc_get_depth_type(machine_topology, level));
}

bool restrict_topology_to_glibc_cpuset(cpu_set_t set) {
  hwloc_cpuset_t hwlocset = hwloc_bitmap_alloc();
  if (hwloc_cpuset_from_glibc_sched_affinity(machine_topology, hwlocset, &set,
                                             sizeof(cpu_set_t))) {
    hwloc_bitmap_free(hwlocset);
    return false;
  }
  bool retval =
      hwloc_topology_restrict(machine_topology, hwlocset,
                              HWLOC_RESTRICT_FLAG_REMOVE_CPULESS) == 0;
  populate_closest_numa_nodes();
  free_distance_matrices();
  retrieve_numa_distances();
  hwloc_bitmap_free(hwlocset);
  return retval;
}

static void
distrib_minimizing_latency(unsigned num_workers, unsigned wanted_workers,
                           unsigned (*restrict latency_matrix)[num_workers],
                           hwloc_cpuset_t sets[wanted_workers],
                           bool use_hyperthreaded_pu_last) {
  hwloc_const_cpuset_t topology_cpuset =
      hwloc_topology_get_complete_cpuset(machine_topology);

  hwloc_cpuset_t allocated_cpu_set = hwloc_bitmap_alloc();
  hwloc_cpuset_t tmp_set = hwloc_bitmap_alloc();
  int the_chosen_one = hwloc_bitmap_first(topology_cpuset);
  hwloc_bitmap_set(allocated_cpu_set, the_chosen_one);
  // The greedy algorithm should work reasonably well
  for (unsigned i = 1; i < wanted_workers; ++i) {
    unsigned best_cost = UINT_MAX;
    unsigned best_candidate = the_chosen_one;
    hwloc_bitmap_zero(tmp_set);
    hwloc_bitmap_set(tmp_set, the_chosen_one);
    hwloc_obj_t core = hwloc_get_next_obj_covering_cpuset_by_type(
        machine_topology, tmp_set, HWLOC_OBJ_CORE, NULL);
    hwloc_bitmap_and(tmp_set, core->cpuset, allocated_cpu_set);
    int lowest_pu_per_core = hwloc_bitmap_weight(tmp_set);
    unsigned j;
    hwloc_bitmap_foreach_begin(j, topology_cpuset);
    {
      if (!hwloc_bitmap_isset(allocated_cpu_set, j)) {
        hwloc_bitmap_zero(tmp_set);
        hwloc_bitmap_set(tmp_set, j);
        hwloc_obj_t core = hwloc_get_next_obj_covering_cpuset_by_type(
            machine_topology, tmp_set, HWLOC_OBJ_CORE, NULL);
        hwloc_bitmap_and(tmp_set, core->cpuset, allocated_cpu_set);
        int pu_in_considered_core = hwloc_bitmap_weight(tmp_set);
        unsigned one_of_the_chosen;
        unsigned j_cost = 0;
        hwloc_bitmap_foreach_begin(one_of_the_chosen, allocated_cpu_set);
        {
          j_cost += latency_matrix[j][one_of_the_chosen] +
                    latency_matrix[one_of_the_chosen][j];
        }
        hwloc_bitmap_foreach_end();
        if (use_hyperthreaded_pu_last) {
          if (pu_in_considered_core < lowest_pu_per_core ||
              (pu_in_considered_core == lowest_pu_per_core &&
               j_cost < best_cost)) {
            best_candidate = j;
            best_cost = j_cost;
            lowest_pu_per_core = pu_in_considered_core;
          }
        } else {
          if (j_cost < best_cost) {
            best_candidate = j;
            best_cost = j_cost;
          }
        }
      }
    }
    hwloc_bitmap_foreach_end();
    assert(best_candidate != 0);
    hwloc_bitmap_set(allocated_cpu_set, best_candidate);
  }
  unsigned one_of_the_chosen;
  unsigned num_chosen = 0;
  hwloc_bitmap_foreach_begin(one_of_the_chosen, allocated_cpu_set);
  {
    hwloc_bitmap_set(sets[num_chosen], one_of_the_chosen);
    num_chosen++;
  }
  hwloc_bitmap_foreach_end();
  hwloc_bitmap_free(allocated_cpu_set);
  hwloc_bitmap_free(tmp_set);
}

bool distribute_worker_on_topology(
    unsigned num_workers, hwloc_obj_t **processing_units,
    enum hwloc_wstream_worker_distribution_algorithm howto_distribute) {
  if (num_workers <= 1)
    howto_distribute = distribute_maximise_per_worker_resources;
  hwloc_cpuset_t *distrib_sets = malloc(num_workers * sizeof(*distrib_sets));
  if (posix_memalign((void **)processing_units, 64,
                     num_workers * sizeof(**processing_units)))
    wstream_df_fatal("Out of memory ...");
  for (unsigned i = 0; i < num_workers; ++i) {
    distrib_sets[i] = hwloc_bitmap_alloc();
  }
  bool use_hyperthreaded_cores_last = false;
  switch (howto_distribute) {
  case distribute_maximise_per_worker_resources: {
    hwloc_obj_t topo_root = hwloc_get_root_obj(machine_topology);
    hwloc_distrib(machine_topology, &topo_root, 1u, distrib_sets, num_workers,
                  INT_MAX, 0);
  } break;
  case distribute_minimise_worker_communication_hyperthreading_last:
    use_hyperthreaded_cores_last = true;
    // fallthrough
  case distribute_minimise_worker_communication: {
    unsigned nproc = num_available_processing_units();
    distrib_minimizing_latency(nproc, num_workers, pu_latency_distances_arr__,
                               distrib_sets, use_hyperthreaded_cores_last);
  } break;
  default:
    break;
  }

  hwloc_cpuset_t restricted_set = hwloc_bitmap_alloc();
  for (unsigned i = 0; i < num_workers; ++i) {
    hwloc_bitmap_singlify(distrib_sets[i]);
    hwloc_bitmap_or(restricted_set, restricted_set, distrib_sets[i]);
    (*processing_units)[i] = hwloc_get_next_obj_inside_cpuset_by_type(
        machine_topology, distrib_sets[i], HWLOC_OBJ_PU, NULL);
#if HWLOC_VERBOSE
    fprintf(stderr, "Worker %u mapped to processing unit %u (OS index %u)\n", i,
            (*processing_units)[i]->logical_index,
            (*processing_units)[i]->os_index);
#endif
    hwloc_bitmap_free(distrib_sets[i]);
  }
  free(distrib_sets);
  bool retval = true;
  if (num_available_processing_units() > num_workers) {
    retval = hwloc_topology_restrict(machine_topology, restricted_set, 0) == 0;
    populate_closest_numa_nodes();
    free_distance_matrices();
    retrieve_numa_distances();
  }
  if (!retval) {
    *processing_units = NULL;
  }
  hwloc_bitmap_free(restricted_set);
  return retval;
}

static void print_topology_node_and_childrens(hwloc_obj_t node,
                                              unsigned indent) {
  char type[32], attr[1024];
  hwloc_obj_type_snprintf(type, sizeof(type), node, 0);
  printf("%*s%s", indent, "", type);
  if (node->logical_index != (unsigned)-1)
    printf("#%u", node->logical_index);
  hwloc_obj_attr_snprintf(attr, sizeof(attr), node, " ", 0);
  if (*attr)
    printf("(%s)", attr);
  for (hwloc_obj_t mem_child = node->memory_first_child; mem_child;
       mem_child = mem_child->next_sibling) {
    printf(" -- ");
    hwloc_obj_type_snprintf(type, sizeof(type), mem_child, 0);
    printf("%s", type);
    if (mem_child->logical_index != (unsigned)-1)
      printf("#%u", mem_child->logical_index);
    hwloc_obj_attr_snprintf(attr, sizeof(attr), mem_child, " ", 0);
    if (*attr)
      printf("(%s)", attr);
  }
  printf("\n");
  for (unsigned i = 0; i < node->arity; i++) {
    print_topology_node_and_childrens(node->children[i], indent + 2);
  }
}

void print_topology_tree(FILE *out) {
  hwloc_obj_t root = hwloc_get_root_obj(machine_topology);
  print_topology_node_and_childrens(root, 0);
}

cpu_set_t object_glibc_cpuset(hwloc_obj_t obj) {
  cpu_set_t cpuset;
  hwloc_cpuset_to_glibc_sched_affinity(machine_topology, obj->cpuset, &cpuset,
                                       sizeof(cpuset));
  return cpuset;
}

void check_bond_to_cpu(pthread_t tid, hwloc_obj_t cpu) {
  hwloc_cpuset_t hwlocset = hwloc_bitmap_alloc();
  if (hwloc_get_thread_cpubind(machine_topology, tid, hwlocset, 0)) {
    fprintf(stderr, "Impossible to get CPU affinity\n");
    exit(EXIT_FAILURE);
  }
  if (!hwloc_bitmap_isincluded(hwlocset, cpu->cpuset)) {
    fprintf(stderr, "Warning: Worker did not get affected to requested cpu\n");
  }
  int num_set = hwloc_bitmap_weight(hwlocset);
  if (num_set == -1) {
    fprintf(stderr,
            "Warning: A worker affinity spans all possible processing units\n");
  }
  if (num_set > 1) {
    hwloc_obj_t pu = hwloc_get_next_obj_inside_cpuset_by_type(
        machine_topology, hwlocset, HWLOC_OBJ_PU, NULL);
    do {
      fprintf(stderr,
              "Warning: A worker has been affected to unwanted processing unit "
              "%u\n",
              pu->os_index);
    } while (pu != NULL);
  }
  hwloc_bitmap_free(hwlocset);
}

int bind_memory_to_cpu_memspace(const void *addr, size_t len, hwloc_obj_t cpu) {
  return hwloc_set_area_membind(
      machine_topology, addr, len, cpu->cpuset, HWLOC_MEMBIND_BIND,
      HWLOC_MEMBIND_MIGRATE | HWLOC_MEMBIND_NOCPUBIND);
}

int interleave_memory_on_machine_nodes(const void *addr, size_t len) {
  return hwloc_set_area_membind(
      machine_topology, addr, len, hwloc_get_root_obj(machine_topology)->cpuset,
      HWLOC_MEMBIND_INTERLEAVE,
      HWLOC_MEMBIND_MIGRATE | HWLOC_MEMBIND_NOCPUBIND);
}

_Thread_local hwloc_bitmap_t thread_bitmap;

hwloc_nodeset_t numa_memlocation_of_memory(const void *addr, size_t len) {
  if (!thread_bitmap) {
    thread_bitmap = hwloc_bitmap_alloc();
  }
  hwloc_get_area_memlocation(machine_topology, addr, len, thread_bitmap,
                             HWLOC_MEMBIND_BYNODESET);
  return thread_bitmap;
}

int bind_memory_to_numa_node(const void *addr, size_t len,
                             unsigned node_index) {
  if (!thread_bitmap) {
    thread_bitmap = hwloc_bitmap_alloc();
  }
  hwloc_bitmap_set(thread_bitmap, node_index);
  int retval = hwloc_set_area_membind(
      machine_topology, addr, len, thread_bitmap, HWLOC_MEMBIND_BIND,
      HWLOC_MEMBIND_NOCPUBIND | HWLOC_MEMBIND_MIGRATE |
          HWLOC_MEMBIND_BYNODESET);
  return retval;
}

unsigned level_of_common_ancestor(const hwloc_obj_t obj1,
                                  const hwloc_obj_t obj2) {
  hwloc_obj_t common_ancestor =
      hwloc_get_common_ancestor_obj(machine_topology, obj1, obj2);
  return common_ancestor->depth;
}

unsigned closest_numa_node_of_processing_unit(const hwloc_obj_t obj) {
  return cpuid_to_closest_numa_node[obj->logical_index];
}
void openstream_hwloc_cleanup(void) {
  hwloc_topology_destroy(machine_topology);
  free_distance_matrices();
  free(cpuid_to_closest_numa_node);
  cpuid_to_closest_numa_node = NULL;
  num_numa_nodes = 0;
  topology_depth = 0;
}

unsigned hwloc_mem_transfer_cost(unsigned numa_node_a, unsigned numa_node_b) {
  // Best information source should be provided by inter-node bandwith
  if (pu_bandwidth_matrix_size) {
    return pu_bandwidth_distances[numa_node_a][numa_node_b];
  }
  // Second best information source can be extracted by looking at the latency
  if (pu_latency_matrix_size) {
    return pu_latency_distances[numa_node_a][numa_node_b];
  }
  // Assume uniform transfer cost when no other data is available
  return 1;
}