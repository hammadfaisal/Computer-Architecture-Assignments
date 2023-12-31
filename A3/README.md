# Cache Simulator
Simulator for a multilevel memory heirarchy having L1, L2 caches and main memory. It follows a write-back write allocate policy with LRU replacement policy and L1, L2 caches are inclusive.
The simulator takes the following inputs: L1 Cache size, L1 cache associativity, L2 cache size, L2 cache associativity, block size.

Read a file containing memory access trace. Each line of the file contains a memory access request eg "R 0x12345678"

The simulator prints the following statistics:
1. Number of L1 cache reads, L1 cache writes, L2 cache reads, L2 cache writes, main memory reads, main memory writes.
2. Number of L1 cache read misses, L1 cache write misses, L2 cache read misses, L2 cache write  misses.
3. Number of L1 cache writebacks, L2 cache writebacks.
