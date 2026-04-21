# mem_alloc — A Simple 3-Layer Memory Allocator

A lightweight custom memory allocator in C, inspired by TCMalloc. It uses `mmap` to manage memory across three caching layers for efficient small and large allocations.

## How It Works

Allocations are split into two paths:

- **Small (≤ 1024 bytes):** Routed through a 3-layer cache hierarchy.
- **Large (> 1024 bytes):** Allocated directly from the OS via `mmap`.

### The 3 Layers

| Layer | Name | Description |
|-------|------|-------------|
| 1 | **Thread Cache** | Per-thread free lists — fast, lock-free access |
| 2 | **Transfer Cache** | Shared between threads; refills the thread cache in batches of 32 |
| 3 | **OS** | Maps 4096-byte pages via `mmap`, chopped into fixed-size blocks |

### Size Classes

Allocations are rounded up to one of 8 size classes:

`8, 16, 32, 64, 128, 256, 512, 1024` (bytes)

Each block is prefixed with a `BlockHeader` that stores its size.

## Usage

```c
void* allocate(size_t size);
```

```c
int*  p = (int*)allocate(20);    // small allocation
int*  q = (int*)allocate(2000);  // large allocation (goes directly to OS)
```

## Build & Run

```bash
gcc mem_alloc.c -o mem_alloc && ./mem_alloc
```

## Limitations

- No `free()` — memory is never returned (intentional for this demo).
- Transfer cache is shared but not protected by a mutex — not thread-safe as-is.
- No error handling on `mmap` failures.
