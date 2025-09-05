# mmap-hugetlb

Demonstrates memory mapping on Linux using 1GB huge TLB to avoid large numbers of 4KiB (or other small sizes on non-x86-based platforms) page entries

To exercise, run [`./test.bash`](./test.bash).

To see the examples, look at [`./main.cpp`](./main.cpp).
