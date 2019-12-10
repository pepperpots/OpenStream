#if defined(UNIFORM_MEMORY_ACCESS) && MAX_NUMA_NODES != 1
#error "UNIFORM_MEMORY_ACCESS defined, but MAX_NUMA_NODES != 1"
#endif

#if defined(MATRIX_PROFILE) && !WQUEUE_PROFILE
#error "MATRIX_PROFILE defined, but WQUEUE_PROFILE != 1"
#endif // defined(MATRIX_PROFILE) && !defined(WQUEUE_PROFILE)
