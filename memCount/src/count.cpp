#include "count.h"
#include <assert.h>
#include <stdio.h>
#include "time.h"

extern int8_t g_count_start;

namespace Dahua{
namespace Count{

	using namespace std;
	CCount::ThreadMap CCount::sm_threadMemMap;
	#ifdef THREAD_NAME
	std::map<size_t, std::string> CCount::sm_threadNameMap;
	#endif
	pthread_mutex_t CCount::sm_mutex;
	size_t CCount::sm_minSize = 0;
	size_t CCount::sm_maxSize = 0;
	size_t CCount::sm_focSize = 0;
	pthread_t CCount::sm_tid = 0;
	int	CCount::sm_period = 60;    			// 30s
	int	CCount::sm_periodParam = 30000000;  // 10s
	bool CCount::sm_bStart = false;
	// std::string CCount::sm_path = "/share";
	//std::string CCount::sm_paramPath = "/share/memParam";
	std::string CCount::sm_basePath = "/root/mount/share/memleaktest/output";
	std::string CCount::sm_path = "";
	std::string CCount::sm_paramPath = "/root/mount/share/memleaktest/memParam";

	CCount *CCount::instance()
	{
		static CCount _instance;
		return &_instance;
	}

	CCount::CCount()
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&sm_mutex, &attr);
	}

	bool CCount::start()
	{
		pthread_mutex_lock(&sm_mutex);
		if (sm_bStart)
		{
			pthread_mutex_unlock(&sm_mutex);
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:count is already started\n",__FUNCTION__, __LINE__, getpid(), gettid());
			return false;
		}
		sm_bStart = true;
		sm_threadMemMap.clear();
		#ifdef THREAD_NAME
		sm_threadNameMap.clear();
		#endif
		pthread_mutex_unlock(&sm_mutex);
		char string[128] = {0};
		struct tm t;
		time_t now;
		time(&now);
		localtime_r(&now, &t);
		snprintf(string, 128, "%s/%d-%02d-%02d-%2d%2d%2d", sm_basePath.c_str(), t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		sm_path = string;
		fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:sm_path:%s\n",__FUNCTION__, __LINE__, getpid(), gettid(), sm_path.c_str());
		if (sm_tid != 0)
		{
			return true;
		}
		pthread_t tid;
		pthread_create(&tid, NULL,  &CCount::autoPrint, this);
		pthread_detach(tid);
		sm_tid = tid;
		return true;
	}

	bool CCount::stop()
	{
		printMemSizeDetail();
		printMemDetail();
		sm_bStart = false;
		return true;
	}

	bool CCount::startReadParam()
	{
		pthread_t tid;
		pthread_create(&tid, NULL,  &CCount::autoReadParam, NULL);
		pthread_detach(tid);
		return true;
	}

	void* CCount::autoReadParam(void* arg)
	{
		while (true)
		{
			// sleep(sm_periodParam);
			std::string filename = sm_paramPath; // 文件名为number.txt

			// 调用函数读取数字
			if (CCount::readNumberFromFile(filename)) 
			{
				// std::cout << "The number in the file is: "  << std::endl;
			} 
			else 
			{
				std::cout << "Error reading the number from the file!" << std::endl;
			}
			
			// sm_periodParam = 2000000;
			// 休眠50ms
			usleep(sm_periodParam);
		}
		return NULL;
	}

	bool CCount::readNumberFromFile(const std::string& filename) 
	{
		// 读取文件中的参数  启动使能(0|1) 读参数周期(us) 最小内存（0 不检测 >minSize） 最大内存（0 不检测 <maxSize）
		int start = 0;
		int periodParam = 0;
		size_t minSize;
		size_t maxSize;

		FILE * file = NULL;
		file = fopen(filename.c_str(), "r");
		if(file != NULL)
		{
			size_t len = 0;
			char buf[128];
			memset(buf, 0, sizeof(buf));
			len = fread(buf, sizeof(char), sizeof(buf), file);
			if (len > 0)
			{
				sscanf(buf, "%d %d %zu %zu", &start, &periodParam, &minSize, &maxSize);
			}
			fclose(file);
			file = NULL;
			#ifdef READPARAM
			if (start != g_count_start)
			{
				if (start == 1)
				{
					g_count_start = 1;
				}
				else
				{
					g_count_start = 0;
				}
			}
			#endif
			if (periodParam != 0 && periodParam != sm_periodParam)
			{
				sm_periodParam = periodParam;
			}
			if (minSize != 0 && minSize != sm_minSize)
			{
				sm_minSize = minSize;
			}
			if (maxSize != 0 && maxSize != sm_maxSize)
			{
				sm_maxSize = maxSize;
			}
		}
		else
		{
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:open file failed\n",__FUNCTION__, __LINE__, getpid(), gettid());
			return false;
		}
		fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:mem_param:start:%d periodParam:%d minSize:%zu maxSize:%zu\n",__FUNCTION__, __LINE__, getpid(), gettid(), start, periodParam, minSize, maxSize);
		return true;
	}

	void* CCount::autoPrint(void* arg)
	{
		while (true)
		{
			sleep(sm_period);
			static bool bStop = false;
			if (bStop)
			{
				continue;
			}
			char string[128] = {0};
			struct tm t;
        	time_t now;
    		time(&now);
        	localtime_r(&now, &t);

			//snprintf(string, 128, "%s/memCount_%d-%02d-%02d-%2d%2d%2d", sm_path.c_str(), t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
			snprintf(string, 128, "%s_memCount", sm_path.c_str());
			
			FILE *pFile = fopen(string, "a+");
			if (pFile == NULL)
			{
				fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:open file failed:%s\n",__FUNCTION__, __LINE__, getpid(), gettid(), string);
				continue;
			}
			snprintf(string, 128, "\n------------------%04d-%02d-%02d_%02d:%02d:%02d------------------\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
			fputs(string, pFile);

			size_t totalSize = 0;
			size_t totalNum = 0;

			pthread_mutex_lock(&sm_mutex);
			for (ThreadMap::iterator tItor = sm_threadMemMap.begin(); tItor != sm_threadMemMap.end(); ++tItor)
			{
				size_t iSize = 0;
				for (MemMap::iterator mItor = tItor->second.begin(); mItor != tItor->second.end(); ++mItor)
				{
					iSize += mItor->second;
				}
				#ifdef THREAD_NAME
				std::map<size_t, std::string>::iterator nameItor = sm_threadNameMap.find(tItor->first);
				if (nameItor != sm_threadNameMap.end())
				{
					snprintf(string, 128, "	thread[%zu],	threadName[%s],	memSize[%zu],	memNum[%zu]\n", tItor->first, nameItor->second
						.c_str(), iSize, tItor->second.size());
				}
				else
				{
					snprintf(string, 128, "	thread[%zu],	memSize[%zu],	memNum[%zu]\n", tItor->first, iSize, tItor->second.size());
				}
				#else
				snprintf(string, 128, "	thread[%zu],	memSize[%zu],	memNum[%zu]\n", tItor->first, iSize, tItor->second.size());
				#endif
				fputs(string, pFile);

				totalNum += tItor->second.size();
				totalSize += iSize;
			}
			snprintf(string, 128, "	totalThread[%zu],	totolSize[%zu],	totalNum[%zu]\n", sm_threadMemMap.size(), totalSize, totalNum);
			pthread_mutex_unlock(&sm_mutex);

			fputs(string, pFile);
			fflush(pFile);

			fclose(pFile);

			printMemSizeDetail();

			if (!sm_bStart)
			{
				//printMemSizeDetail();
				//printMemDetail();
				bStop = true;
			}
		}
		return NULL;
	}

	/**
	 * @brief 打印sm_threadMemMap中每个线程每个内存块大小的总大小，申请的内存块数目
	 */
	void CCount::printMemSizeDetail()
	{
		char string[128] = {0};
		struct tm t;
		time_t now;
		time(&now);
		localtime_r(&now, &t);

		// char timeBuf[128] = {0};
		snprintf(string, 128, "%s_memSizeDetail", sm_path.c_str());

		size_t totalSize = 0;
		size_t totalNum = 0;

		pthread_mutex_lock(&sm_mutex);
		typedef std::map<size_t, size_t> SizeMap;
		typedef std::map<size_t, SizeMap > ThreadSizeDetail;
		ThreadSizeDetail threadSizeDetail;

		for (ThreadMap::iterator tItor = sm_threadMemMap.begin(); tItor != sm_threadMemMap.end(); ++tItor)
		{
			for (MemMap::iterator mItor = tItor->second.begin(); mItor != tItor->second.end(); ++mItor)
			{
				threadSizeDetail[tItor->first][mItor->second] += 1;
			}
		}
		pthread_mutex_unlock(&sm_mutex);

		static pthread_mutex_t s_fileMutex = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_lock(&s_fileMutex);

		FILE *pFile = fopen(string, "a+");
		if (pFile == NULL)
		{
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:open file failed:%s\n",__FUNCTION__, __LINE__, getpid(), gettid(), string);
			return;
		}
		fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:openfile: %s\n",__FUNCTION__, __LINE__, getpid(), gettid(), string);
		
		snprintf(string, 128, "\n------------------%04d-%02d-%02d_%02d:%02d:%02d------------------\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		fputs(string, pFile);

		for (ThreadSizeDetail::iterator tItor = threadSizeDetail.begin(); tItor != threadSizeDetail.end(); ++tItor)
		{
			size_t threadSize = 0;
			size_t threadSizeTotalNum = 0;

			for (SizeMap::iterator mItor = tItor->second.begin(); mItor != tItor->second.end(); ++mItor)
			{
				snprintf(string, 128, "	thread[%zu],	size[%zu],	num[%zu]\n", tItor->first, mItor->first, mItor->second);
				fputs(string, pFile);
				threadSize += mItor->first * mItor->second;
				threadSizeTotalNum += mItor->second;
			}
			#ifdef THREAD_NAME
			std::map<size_t, std::string>::iterator nameItor = sm_threadNameMap.find(tItor->first);
			if (nameItor != sm_threadNameMap.end())
			{
				snprintf(string, 128, "	thread[%zu],	threadName[%s],	memSize[%zu],	memNum[%zu],	threadSizeNum[%zu]\n\n", tItor->first, nameItor->second.c_str(), threadSize, threadSizeTotalNum, tItor->second.size());
			}
			else
			{
				snprintf(string, 128, "	thread[%zu],	memSize[%zu],	memNum[%zu],	threadSizeNum[%zu]\n\n", tItor->first, threadSize, threadSizeTotalNum, tItor->second.size());
			}
			#else
			snprintf(string, 128, "	thread[%zu],	memSize[%zu],	memNum[%zu],	threadSizeNum[%zu]\n\n", tItor->first, threadSize, threadSizeTotalNum, tItor->second.size());
			#endif
			fputs(string, pFile);
			totalNum += threadSizeTotalNum;
			totalSize += threadSize;
		}
		snprintf(string, 128, "	totalThread[%zu],	totolSize[%zu],	totalNum[%zu]\n", threadSizeDetail.size(), totalSize, totalNum);

		fputs(string, pFile);
		fflush(pFile);

		fclose(pFile);
		pthread_mutex_unlock(&s_fileMutex);

	}

	void CCount::printMemDetail()
	{
		char string[128] = {0};
		struct tm t;
		time_t now;
		time(&now);
		localtime_r(&now, &t);

		// char timeBuf[128] = {0};
		snprintf(string, 128, "%s_memAddrDetail", sm_path.c_str());

		pthread_mutex_lock(&sm_mutex);
		FILE *pFile = fopen(string, "a+");
		if (pFile == NULL)
		{
			fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:open file failed:%s\n",__FUNCTION__, __LINE__, getpid(), gettid(), string);
			pthread_mutex_unlock(&sm_mutex);
			return;
		}
		fprintf(stdout,"[%s:%d][pid:%d][tid:%ld]zl:openfile: %s\n",__FUNCTION__, __LINE__, getpid(), gettid(), string);
		snprintf(string, 128, "\n------------------%04d-%02d-%02d_%02d:%02d:%02d------------------\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		fputs(string, pFile);
		for (ThreadMap::iterator tItor = sm_threadMemMap.begin(); tItor != sm_threadMemMap.end(); ++tItor)
		{
			for (MemMap::iterator mItor = tItor->second.begin(); mItor != tItor->second.end(); ++mItor)
			{
				snprintf(string, 128, "	thread[%zu],	mem addr[%p],	memSize[%zu]\n", tItor->first, mItor->first, mItor->second);
				fputs(string, pFile);
			}
			snprintf(string, 128, "\n");
			fputs(string, pFile);
		}
		pthread_mutex_unlock(&sm_mutex);
		fflush(pFile);
		fclose(pFile);
	}

}
}