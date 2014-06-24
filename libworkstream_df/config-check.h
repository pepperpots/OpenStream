#if defined(UNIFORM_MEMORY_ACCESS) && MAX_NUMA_NODES != 1
#error "UNIFORM_MEMORY_ACCESS defined, but MAX_NUMA_NODES != 1"
#endif
