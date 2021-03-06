#ifndef CACHEGRAND_XALLOC_H
#define CACHEGRAND_XALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

void* xalloc_alloc(size_t size);
void* xalloc_alloc_zero(size_t size);
void* xalloc_alloc_aligned(size_t alignment, size_t size);
void* xalloc_alloc_aligned_zero(size_t alignment, size_t size);
void xalloc_free(void *memptr);
size_t xalloc_mmap_align_size(size_t size);
void* xalloc_mmap_alloc(size_t size);
int xalloc_mmap_free(void *memptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_XALLOC_H
