#include <hwloc.h>
#include <hwloc/glibc-sched.h>
#include <stdbool.h>
#include <stdlib.h>

#include "error.h"
#include "hwloc-support.h"
#include "hwloc/bitmap.h"
#include "config.h"

static hwloc_topology_t machine_topology;
unsigned num_numa_nodes;
unsigned topology_depth;
static unsigned *cpuid_to_closest_numa_node;

static void populate_closest_numa_nodes(void) {
  for (hwloc_obj_t pu =
           hwloc_get_next_obj_by_type(machine_topology, HWLOC_OBJ_PU, NULL);
       pu;
       pu = hwloc_get_next_obj_by_type(machine_topology, HWLOC_OBJ_PU, pu)) {
    hwloc_obj_t closest_numa_node = NULL;
    unsigned node_index;
    hwloc_bitmap_foreach_begin(node_index, pu->nodeset) hwloc_obj_t currNode =
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

  unsigned num_distances = 5;
  struct hwloc_distances_s *distancesMatrices[5];

  if (hwloc_distances_get_by_type(machine_topology, HWLOC_OBJ_PU,
                                  &num_distances, distancesMatrices, 0,
                                  0) != 0) {
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

#ifdef HWLOC_VERBOSE
  fprintf(stdout,
          "[HWLOC Info] The machine has a depth of %d\n"
          "[HWLOC Info] The machine has %d numa node(s)\n"
          "[HWLOC Info] Retrieved distance matrice(s): %u\n",
          topology_depth, num_numa_nodes, num_distances);
  fprintf(stdout, "\n[HWLOC Info] Machine topology:\n");
  print_topology_tree(stdout);
#endif

  return true;
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
  bool retval = hwloc_topology_restrict(machine_topology, hwlocset, 0) == 0;
  populate_closest_numa_nodes();
  hwloc_bitmap_free(hwlocset);
  return retval;
}

bool distribute_worker_on_topology(unsigned num_workers,
                                   hwloc_obj_t **processing_units) {
  hwloc_cpuset_t *distrib_sets = malloc(num_workers * sizeof(*distrib_sets));
  if (posix_memalign((void **)processing_units, 64,
                     num_workers * sizeof(**processing_units)))
    wstream_df_fatal("Out of memory ...");
  for (unsigned i = 0; i < num_workers; ++i) {
    distrib_sets[i] = hwloc_bitmap_alloc();
  }
  hwloc_obj_t topo_root = hwloc_get_root_obj(machine_topology);
  hwloc_distrib(machine_topology, &topo_root, 1u, distrib_sets, num_workers,
                INT_MAX, 0);

  hwloc_cpuset_t restricted_set = hwloc_bitmap_alloc();
  for (unsigned i = 0; i < num_workers; ++i) {
    hwloc_bitmap_singlify(distrib_sets[i]);
    hwloc_bitmap_or(restricted_set, restricted_set, distrib_sets[i]);
    (*processing_units)[i] = hwloc_get_next_obj_inside_cpuset_by_type(
        machine_topology, distrib_sets[i], HWLOC_OBJ_PU, NULL);
#ifdef HWLOC_VERBOSE
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
