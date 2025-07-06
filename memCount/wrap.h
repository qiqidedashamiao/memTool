
#ifndef WRAP_H_
#define WRAP_H_

#include <stddef.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>

//区别动态库静态库分配释放函数声明
#ifdef SHARED_OBJECT
extern "C"
{
typedef void *(*MallocFunctionPtr)(size_t);
typedef void (*FreeFunctionPtr)(void *);
typedef void *(*CallocFunctionPtr)(size_t, size_t);
typedef void *(*ReallocFunctionPtr)(void *, size_t);
}
extern MallocFunctionPtr real_malloc;
extern FreeFunctionPtr real_free;
extern CallocFunctionPtr real_calloc;
extern ReallocFunctionPtr real_realloc;
#define wrap_malloc malloc
#define wrap_free free
#define wrap_calloc calloc
#define wrap_realloc realloc
#else

extern "C"
{
	void* __real_malloc(size_t);
	void __real_free(void*);
	void* __real_calloc(size_t nmemb, size_t size);
	void* __real_realloc(void *ptr, size_t size);
}
#define real_malloc __real_malloc
#define real_free __real_free
#define real_calloc __real_calloc
#define real_realloc __real_realloc

#define wrap_malloc __wrap_malloc
#define wrap_free __wrap_free
#define wrap_calloc __wrap_calloc
#define wrap_realloc __wrap_realloc
#endif


#endif /* WRAP_H_ */