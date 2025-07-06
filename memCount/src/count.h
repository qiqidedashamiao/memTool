
#ifndef COUNT_H_
#define COUNT_H_
#include <fstream>  // 包含文件操作的头文件
#include <iostream> // 包含标准输入输出流的头文件
#include <map>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/syscall.h>
//#include <execinfo.h>
#include <ext/malloc_allocator.h>

#if !defined(gettid)
#define gettid() syscall(SYS_gettid)
#endif

#define THREAD_NAME 1
#define READPARAM 1
#define MEMFLAG 0x7


namespace Dahua{
namespace Count{

///#define MINSIZE (0)

#define SIZE 20


class CCount
{
public:
	static CCount *instance();

	bool start();

	bool stop();
	
	static bool readNumberFromFile(const std::string& filename); 

	static void* autoPrint(void* arg);
	
	static void printMemSizeDetail();

	static void printMemDetail();

	static bool startReadParam();

	static void* autoReadParam(void* arg);

	typedef std::map<void*, size_t> MemMap;
	typedef std::map<size_t, MemMap > ThreadMap;
	

	static void insert(void *key, size_t value)
	{
		///size_t realValue = (value + 2*sizeof(size_t) - 1) & ~(2 * sizeof(size_t) - 1);
		//value = ((*(size_t*)((unsigned char*)key - sizeof(size_t))) & (~MEMFLAG));
	    if (sm_minSize != 0 && value < sm_minSize)
		{
	        return;
		}
		if (sm_maxSize != 0 && value >= sm_maxSize)
		{
	        return;
		}
		
		# if 0
		void *buffer[SIZE];
		int nptrs = backtrace(buffer, SIZE);
		char **strings = backtrace_symbols(buffer, nptrs);
		if (strings != NULL) 
		{
			printf("%p baracktrace:\n", key);
			for (int j = 0; j < nptrs; j++)
			{
				printf("%s\n", strings[j]);
			}
			free(strings);
		}
		#endif

		size_t tid = (size_t)syscall(SYS_gettid);

		// if (sm_focSize != 0 && value >=  sm_focSize)
		// {
		// 	printf("tid = %d alloc mem addr = %p size = %d\n", tid, key, value);
		// }
		pthread_mutex_lock(&sm_mutex);
		sm_threadMemMap[tid][key] = value;
		#ifdef THREAD_NAME
		if (sm_threadNameMap.find(tid) == sm_threadNameMap.end())
		{
			char thread_name[128] = {0};
			if (pthread_getname_np(pthread_self(), thread_name, 128) == 0) {
				sm_threadNameMap[tid] = thread_name;
			}
		}
		#endif
    	pthread_mutex_unlock(&sm_mutex);
		return;
		
	}

	static void remove(void *key, size_t value)
	{	
		if (NULL == key)
		{
			return;
		}

		// int value = ((*(size_t*)((unsigned char*)key - sizeof(size_t))) & (~MEMFLAG)) - (2 * sizeof(size_t));
		if (sm_minSize != 0 && value < sm_minSize)
		{
	        return;
		}
		if (sm_maxSize != 0 && value >= sm_maxSize)
		{
	        return;
		}

		bool bFind = false;
		ThreadMap::iterator tItor; 
		MemMap::iterator mItor;
		size_t tid = (size_t)syscall(SYS_gettid);
		// if (sm_focSize != 0 && value >=  sm_focSize)
		// {
		// 	printf("tid = %d free mem addr = %p size = %d\n", tid, key, value);
		// }

		pthread_mutex_lock(&sm_mutex);
		if ((tItor = sm_threadMemMap.find(tid)) != sm_threadMemMap.end() && ((mItor = tItor->second.find(key)) != tItor->second.end()))
		{
			/// 释放是本线程申请的内存。
			bFind = true;
		}
		if (!bFind)
		{
			/// 释放的是其他线程申请的内存。
			for (tItor = sm_threadMemMap.begin(); tItor != sm_threadMemMap.end(); ++tItor)
			{
				if (tid == tItor->first)
				{
					continue;
				}
				mItor = tItor->second.find(key);
				if ((mItor = tItor->second.find(key)) != tItor->second.end())
				{
					bFind = true;
					break;
				}
			}
		}
		if (bFind)
		{
			tItor->second.erase(mItor);
			if (tItor->second.empty())
			{
				sm_threadMemMap.erase(tItor);
				#ifdef THREAD_NAME
				sm_threadNameMap.erase(tItor->first);
				#endif
			}
		}
		pthread_mutex_unlock(&sm_mutex);
		return;
	}

	CCount();


private:
	static ThreadMap sm_threadMemMap;
	#ifdef THREAD_NAME
	static std::map<size_t, std::string> sm_threadNameMap;
	#endif

	static pthread_mutex_t sm_mutex;
	static size_t sm_minSize;
	static size_t sm_maxSize;
	static size_t  sm_focSize;
	static pthread_t sm_tid;
	static int sm_period;
	static int sm_periodParam;
	static bool sm_bStart;
	static std::string sm_basePath;
	static std::string	sm_path;
	static std::string	sm_paramPath;
};

}
}

#endif /* COUNT_H_ */