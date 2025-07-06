#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <malloc.h>
#include <map>

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


#define gettid() syscall(SYS_gettid)

typedef std::map<void*, size_t> MemMap;

typedef struct ThreadInfo
{
	long long int  size = 0;
	MemMap memMap;
} ThreadInfo;

static int s_init = 0;
static const char* sg_path = "/root/mount/share/gdb/heap_memory";
static __thread int gst_in_malloc = 0;	
static size_t sg_threadid = 0;

typedef std::map<size_t, ThreadInfo> ThreadMap;
static ThreadMap sg_threadMemMap;
static pthread_mutex_t	sg_threadMemMapMutex = PTHREAD_MUTEX_INITIALIZER;


#define TIME_INTERVAL 30

void* threadFunction(void* arg)
 {
    //int id = *(int*)arg; // 获取传递的参数
	sg_threadid = gettid();
	const int len128 = 128; 
	char temp[len128];
	memset(temp, 0, len128);
    const int len = 128;
    /*
	static char *string = NULL;
	
			if (string == NULL)
			{
				fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:string--.\n", __FUNCTION__, __LINE__, getpid(), gettid());
				string = (char *)__libc_calloc(len, 1);
				fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:string:%p--.\n", __FUNCTION__, __LINE__, getpid(), gettid(), string);
				if (string == NULL)
				{
					fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:malloc 4096 failed--.\n", __FUNCTION__, __LINE__, getpid(), gettid());
					return NULL;
				}
			}
            */
	struct tm t;
    time_t now;
    time(&now);
    localtime_r(&now, &t);
	uint offset = 0;
	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:sg_path:%s--.\n", __FUNCTION__, __LINE__, getpid(), gettid(), sg_path);
	offset = snprintf(temp, len128-1, "%s/memcount_%d-%02d-%02d-%02d%02d%02d", sg_path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	temp[offset] = '\0';
	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:temp:%s--.\n", __FUNCTION__, __LINE__, getpid(), gettid(), temp);
	FILE *pFile = fopen(temp, "a+");
	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:pFile:%p--.\n", __FUNCTION__, __LINE__, getpid(), gettid(), pFile);
	if (pFile == NULL)
	{
		//free(string);
		return NULL;
	}
    	
	//fclose(pFile1);
	//memset(string, 0, len);
	s_init = 1;
	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:s_init:%d, sg_threadid:%zu--.\n", __FUNCTION__, __LINE__, getpid(), gettid(), s_init, sg_threadid);
	
	while(1)
	{
		sleep(TIME_INTERVAL);
        size_t size = 0;
        pthread_mutex_lock(&sg_threadMemMapMutex);
        size = sg_threadMemMap.size();
        pthread_mutex_unlock(&sg_threadMemMapMutex);
        fprintf(stdout, "[%s:%d][pid:%d][tid:%ld]--zl:sg_threadMemMap.size():%zu--.\n", __FUNCTION__, __LINE__, getpid(), gettid(), size);

			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--nosleep--.\n", __FUNCTION__, __LINE__, getpid(), gettid());
			
			//pFile = fopen(temp, "a+");
			//if (pFile == NULL)
			//{
			//	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--pFile is NULL--.\n", __FUNCTION__, __LINE__, getpid(), gettid());
			//	break;
			//}
			//struct tm t;
        	//time_t now;
    		time(&now);
        	localtime_r(&now, &t);

			//char timeBuf[128] = {0};
			
			offset = snprintf(temp, len-1, "\n------------------%04d-%02d-%02d_%02d:%02d:%02d------------------\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
			temp[offset] = '\0';
			fputs(temp, pFile);

			long long int totalSize = 0;
			uint totalNum = 0;
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:1--.\n", __FUNCTION__, __LINE__, getpid(), gettid());
			offset = 0;
            std::map<size_t, std::pair<size_t, long long int> >  threadMap;
			pthread_mutex_lock(&sg_threadMemMapMutex);
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:2--.\n", __FUNCTION__, __LINE__, getpid(), gettid());
			for (ThreadMap::iterator tItor = sg_threadMemMap.begin(); tItor != sg_threadMemMap.end(); ++tItor)
			{
                threadMap[tItor->first] = std::make_pair(tItor->second.memMap.size(), tItor->second.size);
                // offset = snprintf(temp, len, "	thread[%lu],	memNum[%lu],	memSize[%lld]\n", tItor->first, tItor->second.memMap.size(), tItor->second.size);
                // temp[offset] = '\0';
                // fputs(temp, pFile);
                // fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]	thread[%lu],	memNum[%lu],	memSize[%lld]\n\n", __FUNCTION__, __LINE__, getpid(), gettid(), tItor->first, tItor->second.memMap.size(), tItor->second.size);
				// totalNum += tItor->second.memMap.size();
				// totalSize += tItor->second.size;
			}
			//fprintf(stdout,"[pid:%d][tid:%ld]--zl:3--.\n", getpid(), gettid());
			pthread_mutex_unlock(&sg_threadMemMapMutex);
            for (const auto& item : threadMap) 
            {
                offset = snprintf(temp, len, "	thread[%lu],	memNum[%lu],	memSize[%lld]\n", item.first, item.second.first, item.second.second);
                temp[offset] = '\0';
                fputs(temp, pFile);
                fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]	thread[%lu],	memNum[%lu],	memSize[%lld]\n\n", __FUNCTION__, __LINE__, getpid(), gettid(), item.first, item.second.first, item.second.second);
				totalNum += item.second.first;
				totalSize += item.second.second;
            }
			//temp[offset] = '\0';
			//fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]string:\n%s\n", __FUNCTION__, __LINE__, getpid(), gettid(),temp);
			//fputs(temp, pFile);
			//fprintf(stdout,"[pid:%d][tid:%ld]--zl:4--.\n", getpid(), gettid());
			offset = snprintf(temp, len-1, "	totalThread[%lu],	totalNum[%u],	totolSize[%lld]\n", sg_threadMemMap.size(), totalNum, totalSize);
			temp[offset] = '\0';
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]string:%s\n", __FUNCTION__, __LINE__, getpid(), gettid(),temp);
			fputs(temp, pFile);

			fflush(pFile);
			//fclose(pFile);

			// 30s读取一次  config.cfg中配置
			//memset(string, 0, len);
			sleep(TIME_INTERVAL);

	}
	
	fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]--zl:end read param--.\n", __FUNCTION__, __LINE__, getpid(), gettid());
	
	fclose(pFile);
    pthread_exit(NULL);

	return NULL;
}

void createThread(int sonit_pid)
{
	fprintf(stdout,"[pid:%d][tid:%ld]--zl:createThread--.\n", getpid(), gettid());
	pthread_t thread;
	int ret = pthread_create(&thread, NULL, threadFunction, NULL);
    if (ret != 0) {
		fprintf(stdout,"[pid:%d][tid:%ld]zl:Failed to create thread.\n", getpid(), gettid());
        //std::cerr << "Failed to create thread." << std::endl;
        return ;
    }

    //ret = pthread_join(thread, NULL);
    //if (ret != 0) {
	//	fprintf(stdout,"zl:Failed to join thread.\n");
    //    //std::cerr << "Failed to join thread." << std::endl;
    //    return ;
    //}
	
	if (pthread_detach(thread) != 0)
    {
        fprintf(stdout, "[pid:%d][tid:%ld]zl:Failed to detach thread\n", getpid(), gettid());
    }
	fprintf(stdout,"[pid:%d][tid:%ld]--zl:createThread end--.\n", getpid(), gettid());
}

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
	fprintf(stdout,"[pid:%d][tid:%ld]zl: malloctest init argc:%d\n",getpid(), gettid(), argc);
	char path[1024];
	memset(path, 0 ,sizeof(path));
	if (readlink("/proc/self/exe", path, sizeof(path) - 1) > 0)
	{
		char *pName = strrchr(path, '/');
		if (pName != NULL)
		{
			fprintf(stdout,"[pid:%d][tid:%ld]zl:pname:%s\n",getpid(), gettid(), pName);
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
						char thread_name[128];
						if (pthread_getname_np(pthread_self(), thread_name, 128) == 0) {
							fprintf(stdout,"[pid:%d][tid:%ld]zl: Thread name:%s\n",getpid(), gettid(),thread_name);
						} else {
							fprintf(stdout,"[pid:%d][tid:%ld]zl: pthread_getname_np is fail\n",getpid(), gettid());
						}
						createThread(getpid());
						//s_init = 1;
						fprintf(stdout,"[pid:%d][tid:%ld]zl: s_init:%d\n",getpid(), gettid(),s_init);
						break;
					}
				}
			}
		}
	}
}

/**
 * 根据申请的内存指针地址，获取内存申请的大小
*/
size_t getMallocSize(void *ptr)
{
	size_t size = 0u;
	if (ptr == NULL)
	{
		return size;
	}
	size = malloc_usable_size(ptr);
	return size;
}

void zl_insert(void *ptr)
{
	size_t tid = gettid();
	if (tid == sg_threadid)
	{
		return;
	}
	gst_in_malloc = 1;
	size_t size = malloc_usable_size(ptr);
	pthread_mutex_lock(&sg_threadMemMapMutex);
	ThreadInfo &threadInfo = sg_threadMemMap[tid];
	threadInfo.size += size;
	threadInfo.memMap[ptr] = size;
	pthread_mutex_unlock(&sg_threadMemMapMutex);
	gst_in_malloc = 0;
}

void zl_del(void *ptr)
{
	size_t tid = gettid();
	if (tid == sg_threadid)
	{
		return;
	}
	gst_in_malloc = 1;
	bool bFind = false;
	//查找当前线程是否存在ptr
	ThreadMap::iterator tItor; 
	MemMap::iterator mItor;
	pthread_mutex_lock(&sg_threadMemMapMutex);
	if ((tItor = sg_threadMemMap.find(tid)) != sg_threadMemMap.end() && ((mItor = tItor->second.memMap.find(ptr)) != tItor->second.memMap.end()))
	{
		/// 释放是本线程申请的内存。
		bFind = true;
	}
	if (!bFind)
	{
		for (tItor = sg_threadMemMap.begin(); tItor != sg_threadMemMap.end(); ++tItor)
		{
			if (tid == tItor->first)
			{
				continue;
			}
			//mItor = tItor->second.memMap.find(ptr);
			if ((mItor = tItor->second.memMap.find(ptr)) != tItor->second.memMap.end())				
			{
				bFind = true;
				break;
			}
		}
	}
	if (bFind)
	{
		tItor->second.memMap.erase(mItor);
		if (tItor->second.memMap.empty())
		{
			sg_threadMemMap.erase(tItor);
		}
		else
		{
			size_t size = malloc_usable_size(ptr);
			tItor->second.size -= size;
		}
	}
	pthread_mutex_unlock(&sg_threadMemMapMutex);
	gst_in_malloc = 0;
}

extern "C" void * malloc(size_t size)
{
	void* result = NULL;
	
	result = (void*)__libc_malloc(size);
	//if (s_init != 0)fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
	if (s_init != 0 && gst_in_malloc == 0)
	{
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc1:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
		zl_insert(result);
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc2:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
	}
	
	return result;
}


extern "C" void free(void *ptr)
{
	if (ptr == NULL)
	{
		return;
	}
    //if (s_init != 0)fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc);
	if (s_init != 0 && gst_in_malloc == 0)
	{
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc1:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, 0);
		zl_del(ptr);
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc2:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, 0);
	}
	__libc_free(ptr);
	return;
}


extern "C" void *calloc(size_t nmemb, size_t size)
{
	void* result = NULL;
	result = (void*)__libc_calloc(nmemb, size);
    //if (s_init != 0)fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
	if (s_init != 0 && gst_in_malloc == 0)
	{
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc1:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
		zl_insert(result);
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc2:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
	}
	return result;
}

extern "C" void *realloc(void *ptr, size_t size)
{
	void* result = NULL;
    if (ptr != NULL)
    {
		//if (s_init != 0)fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
		if (s_init != 0 && gst_in_malloc == 0)
		{
			//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc1:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
			zl_del(ptr);
			//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc2:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
		}
    }
	result = (void*)__libc_realloc(ptr, size);
    //if (s_init != 0)fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
	if (s_init != 0 && gst_in_malloc == 0)
	{
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc1:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
		zl_insert(result);
		//fprintf(stdout,"[%s:%d][tid:%ld]gst_in_malloc2:%d size:%d\n", __FUNCTION__, __LINE__, gettid(), gst_in_malloc, size);
	}
	return result;
}