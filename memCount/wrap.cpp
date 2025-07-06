#include "wrap.h"
#include "stdlib.h"
#include <string.h>

#ifdef SHARED_OBJECT
MallocFunctionPtr real_malloc = NULL;
FreeFunctionPtr real_free = NULL;
CallocFunctionPtr real_calloc = NULL;
ReallocFunctionPtr real_realloc = NULL;
#endif

namespace Dahua
{
namespace Count
{
pthread_key_t isNeedCallRealMalloc = 0;			//是否需用调用真实的malloc
static char firstBuffer[1024] = {0};

extern "C"{
void* wrap_malloc(size_t size)
{
	Dahua::Count::initRealMalloc();
	if(pthread_getspecific(Dahua::Count::isNeedCallRealMalloc))
	{
		return real_malloc(size);
	}
	
	pthread_setspecific(Dahua::Count::isNeedCallRealMalloc, (void*)true);
	void* addr = real_malloc(size);
	if (addr)
	{
		Dahua::Count::insertFunc(addr, size);
	}
	pthread_setspecific(Dahua::Count::isNeedCallRealMalloc, (void*)false);
	return addr;
}

void wrap_free(void *ptr)
{
	Dahua::Count::initRealMalloc();
	if(pthread_getspecific(Dahua::Count::isNeedCallRealMalloc))
	{
		return real_free(ptr);
	}

	pthread_setspecific(Dahua::Count::isNeedCallRealMalloc, (void*)true);
	Dahua::Count::removeFunc(ptr);
	real_free(ptr);
	pthread_setspecific(Dahua::Count::isNeedCallRealMalloc, (void*)false);
}

void* wrap_realloc(void *ptr, size_t size)
{
	Dahua::Count::initRealMalloc();
	if(pthread_getspecific(Dahua::Count::isNeedCallRealMalloc))
	{
		return real_realloc(ptr, size);
	}
	
	pthread_setspecific(Dahua::Count::isNeedCallRealMalloc, (void*)true);
	Dahua::Count::removeFunc(ptr);
	void *addr = real_realloc(ptr, size);
	if (addr)
	{
		Dahua::Count::insertFunc(addr, size);
	}
	pthread_setspecific(Dahua::Count::isNeedCallRealMalloc, (void*)false);
	return addr;
}

void* wrap_calloc(size_t nmemb, size_t size)
{
#ifdef SHARED_OBJECT	
	static bool isFirst = true;
	if(isFirst)
	{
		isFirst = false;
		
		return firstBuffer;
	}
#endif	
	if(nmemb == 0 || size == 0) 
	{ 
		return wrap_malloc(0);
	} 
	void* addr = wrap_malloc(nmemb * size);
	memset(addr, 0x00, nmemb*size); 

	return addr; 
} 

}

void initRealMalloc()
{
    static bool isInit = false;
    if(isInit)
    {
        return;
    }
    isInit = true;
#ifdef SHARED_OBJECT
    if(real_malloc == NULL)
    {
        real_malloc = (MallocFunctionPtr)dlsym(RTLD_NEXT, "malloc");
        printf("real_malloc(%p)\n", real_malloc);
    }
    if(real_realloc == NULL)
	{
		real_realloc = (ReallocFunctionPtr)dlsym(RTLD_NEXT, "realloc");
		printf("real_realloc(%p)\n", real_realloc);
	}
    if(real_free == NULL)
    {
        real_free = (FreeFunctionPtr)dlsym(RTLD_NEXT, "free");
        printf("real_free(%p)\n", real_free);
    }
#endif    
    pthread_key_create(&isNeedCallRealMalloc, NULL);
}

}
}