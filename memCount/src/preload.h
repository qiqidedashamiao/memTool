#ifndef PRELOAD_H_
#define PRELOAD_H_

// #include <stddef.h>
// #include <dlfcn.h>
// #include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif
void __libc_free(void*);
void * __libc_malloc(size_t);
void *__libc_calloc(size_t, size_t);
void *__libc_realloc(void*, size_t);

#ifdef __cplusplus
}
#endif

#endif /* PRELOAD_H_ */