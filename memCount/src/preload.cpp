
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <unistd.h>
#include <malloc.h>
#include <new>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>

#include "count.h"
#include "preload.h"


//如果定义宏USE_BACKTRACK，则包含头文件<execinfo.h>
#ifdef USE_BACKTRACE
#include <execinfo.h>
#endif

#if !defined(gettid)
#define gettid() syscall(SYS_gettid)
#endif


// backtrace_symbols()
// backtrace_symbols_fd()


static void initRealMalloc();

// #ifdef READPARAM
int8_t g_count_start = 0;
// #else
// extern int8_t g_count_start;
// #endif

static int8_t sg_count_init = 0;

static int s_init = 0;
static size_t sg_threadid = 0;

static pthread_key_t sg_isNeedCallRealMalloc = 0;			//是否需用调用真实的malloc

// 清理函数（如果需要）
__attribute__((destructor)) void cleanup() {
    // ... 在dlclose时执行的清理代码（如果有）
}

/**
 * purpose: 进程启动前初始化内存跟踪信息
 * param:
 * return:
*/
__attribute__ ((constructor))
void _main(int argc, char** argv)
{
	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: malloctest init argc:%d\n", __FUNCTION__, __LINE__, getpid(), gettid(), argc);
	char path[1024];
	memset(path, 0 ,sizeof(path));
	if (readlink("/proc/self/exe", path, sizeof(path) - 1) > 0)
	{
		char *pName = strrchr(path, '/');
		if (pName != NULL)
		{
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:pname:%s\n", __FUNCTION__, __LINE__,getpid(), gettid(), pName);
			int len = strlen(pName);
			for( int i = 0; i < len-4; ++i)
			{
				if (pName[i] == 's')
				{
					if (pName[i+1] == 'o'
					&& pName[i+2] == 'n'
					&& pName[i+3] == 'i'
					&& pName[i+4] == 'a')
					{
						// MemTraceInit();
						// updateParam();
						char thread_name[128];
						if (pthread_getname_np(pthread_self(), thread_name, 128) == 0) {
							fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: Thread name: :%s\n", __FUNCTION__, __LINE__,getpid(), gettid(),thread_name);
							sg_threadid = gettid();
						} else 
						{
							fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: pthread_getname_np is fail\n", __FUNCTION__, __LINE__,getpid(), gettid());
						}
						s_init = 1;
						//#ifdef READPARAM
						Dahua::Count::CCount::startReadParam();
						//#endif
						fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: s_init:%d\n", __FUNCTION__, __LINE__,getpid(), gettid(),s_init);
					}
				}
			}
		}
	}	
}

// 假设元数据在分配的内存块之前，并且包含一个 size_t 类型的大小字段
// 最后3位用于其他目的
size_t get_allocated_size(void* ptr) {
    if (ptr == NULL) {
        return 0;
    }

    // 根据特定的内存分配器实现调整偏移量
    size_t* size_ptr = (size_t*)((uint8_t*)ptr - sizeof(size_t));
    // 屏蔽掉最后3位
    return *size_ptr & ~((size_t)0x7);
}

size_t getMallocSize(void *ptr)
{
	size_t size = 0u;
	if (ptr == NULL)
	{
		return size;
	}
	size = malloc_usable_size(ptr);
	return size;
	// return ((*(size_t*)((unsigned char*)ptr - sizeof(size_t))) & (~MEMFLAG)) - sizeof(size_t);
	// return ((*(size_t*)((unsigned char*)ptr - sizeof(size_t))) & (~MEMFLAG)) ;
}
extern "C" void zl_malloc(void *ptr, size_t size, size_t sizereal)
{
    Dahua::Count::CCount::insert(ptr, sizereal);
}
extern "C" void zl_malloc_b(void *ptr, size_t size)
{
	if (ptr == NULL)
	{
		return;
	}
    if (g_count_start != 0)
    {
        if (sg_count_init == 0)
        {
            //Dahua::CCount::start();
            Dahua::Count::CCount::instance()->start();
            sg_count_init = 1;
        }
		size_t sizereal = ((*(size_t*)((unsigned char*)ptr - sizeof(size_t))) & (~MEMFLAG));
        zl_malloc(ptr, size, sizereal);
        
    }
}

extern "C" void zl_free(void *ptr, size_t size)
{
    Dahua::Count::CCount::remove(ptr, size);
}

extern "C" void zl_free_b(void *ptr)
{
	if (ptr == NULL)
	{
		return;
	}
    if (sg_count_init != 0)
    {
        if (g_count_start != 0)
        {
            size_t size = ((*(size_t*)((unsigned char*)ptr - sizeof(size_t))) & (~MEMFLAG));
            zl_free(ptr, size);
        }
        else
        {
            sg_count_init = 0;
            Dahua::Count::CCount::instance()->stop();
        }
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
    pthread_key_create(&sg_isNeedCallRealMalloc, NULL);
}

extern "C" void * malloc(size_t size)
{
	initRealMalloc();
	if(pthread_getspecific(sg_isNeedCallRealMalloc))
	{
		void* result = __libc_malloc(size);
		if (result != NULL)
		{
			// size_t newSize = getMallocSize(result);
			// fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: size:%zu realsize:%zu\n", __FUNCTION__, __LINE__,getpid(), gettid(),size, newSize);
			// if (newSize > 1024)
			// {
			// 	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: size:%zu realsize:%zu\n", __FUNCTION__, __LINE__,getpid(), gettid(),size, newSize);
			// }
			memset(result, 0x00, getMallocSize(result));
		}
		return result;
		// return __libc_malloc(size);
	}
	
	pthread_setspecific(sg_isNeedCallRealMalloc, (void*)true);
	void* result = __libc_malloc(size);
	if (result != NULL)
	{
		// size_t newSize = getMallocSize(result);
			// fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: size:%zu realsize:%zu\n", __FUNCTION__, __LINE__,getpid(), gettid(),size, newSize);
			// if (newSize > 1024)
			// {
			// 	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl: size:%zu realsize:%zu\n", __FUNCTION__, __LINE__,getpid(), gettid(),size, newSize);
			// }
		memset(result, 0x00, getMallocSize(result));
	}
	zl_malloc_b(result, size);
	pthread_setspecific(sg_isNeedCallRealMalloc, (void*)false);
	return result;
}

extern "C" void free(void *ptr)
{
	if (ptr == NULL)
	{
		return;
	}

	initRealMalloc();
	if(pthread_getspecific(sg_isNeedCallRealMalloc))
	{
		return __libc_free(ptr);
	}

	pthread_setspecific(sg_isNeedCallRealMalloc, (void*)true);
	zl_free_b(ptr);
	__libc_free(ptr);
	pthread_setspecific(sg_isNeedCallRealMalloc, (void*)false);
	return;
}


extern "C" void *calloc(size_t nmemb, size_t size)
{
	if(nmemb == 0 || size == 0) 
	{ 
		return malloc(0);
	}
	void *result = malloc(nmemb * size);
	if (result != NULL)
	{
		memset(result, 0x00, nmemb*size);
	}
    
	return result;
}

extern "C" void *realloc(void *ptr, size_t size)
{
	size_t oldSize = 0;
	if (ptr != NULL)
	{
		oldSize = getMallocSize(ptr);
	}
	initRealMalloc();
	if(pthread_getspecific(sg_isNeedCallRealMalloc))
	{
		void *result = __libc_realloc(ptr, size);
		size_t newSize = getMallocSize(result);
		if (result != NULL && newSize > oldSize)
		{
			memset((char *)result+oldSize, 0x00, newSize - oldSize);
		}
		return result;
		// return __libc_realloc(ptr, size);
	}
	
	pthread_setspecific(sg_isNeedCallRealMalloc, (void*)true);
	zl_free_b(ptr);
	void *result = __libc_realloc(ptr, size);
	size_t newSize = getMallocSize(result);
	if (result != NULL && newSize > oldSize)
	{
		memset(result, 0x00, newSize - oldSize);
	}
	zl_malloc_b(result, size);
	pthread_setspecific(sg_isNeedCallRealMalloc, (void*)false);

	return result;
}

