#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <atomic>
#include <execinfo.h>
#include <condition_variable>
#include <dlfcn.h>
#include <cstdio>
#include <unistd.h>
#include <sys/syscall.h>
#include <random>

using namespace std;


#define gettid() syscall(SYS_gettid)

// 用于控制线程停止的全局标志和条件变量
std::atomic<bool> running(true);
std::condition_variable cv;
std::mutex cv_m;

class A {
public:
    A() {
        #if PRINT_FLAG
        std::cout << "A()" << std::endl;
        #endif
    }
    ~A() {
        #if PRINT_FLAG
        std::cout << "~A()" << std::endl;
        #endif
    }
private:
    int a[1000];

};

//#define PRINT_FLAG 1
extern int8_t g_count_start;

char cache[1024*1024*100] = {2,2,3,4,5,6,7,8,9,10};
// 每个线程执行的函数
void allocateAndFreeMemory(int threadId) {
    std::srand(static_cast<unsigned int>(std::time(nullptr)) + threadId);
    std::random_device rd;  // 用于生成随机种子
    std::default_random_engine eng(rd());
    std::uniform_int_distribution<int> distr(1, 10000); // 生成1到100之间的随机数
    const int step = 10;
    while (running) {
        // 随机分配内存大小
        //int size = std::rand() + 1; // 分配1到1024字节的内存
        //int size = 1024; // 分配1到1024字节的内存
        int size = distr(eng); // 分配1到100KB之间的内存
        #if PRINT_FLAG
        std::cout << "线程 " << gettid() << " 申请 " << size << " 字节内存" << std::endl;
        #endif
        int circle = 5;
        if (size % circle == 0)
        {
            char* buffer = new char[size];
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " new[]分配了 " << size << " 字节内存" << std::endl;
            #endif

            for (int i = 0; i < size; i++) {
                buffer[i] = i;
            }
            //foo();

            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(1*step));
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " new[]开始释放内存" << std::endl;
            #endif
            // 释放内存
            delete[] buffer;
        }
        else if (size % circle == 1)
        {
            char* buffer = (char *)malloc(size);
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " malloc分配了 " << size << " 字节内存" << std::endl;
            #endif

            for (int i = 0; i < size; i++) {
                buffer[i] = i;
            }
            //foo();

            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(4*step));
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " malloc开始释放内存" << std::endl;
            #endif
            // 释放内存
            free(buffer);
        }
        else if (size % circle == 2)
        {
            char* buffer = (char *)calloc(1, size);
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " calloc分配了 " << size << " 字节内存" << std::endl;
            #endif

            for (int i = 0; i < size; i++) {
                buffer[i] = i;
            }
            //foo();

            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(5*step));
            #if PRINT_FLAG

            std::cout << "线程 " << gettid() << " calloc开始释放内存" << std::endl;
            #endif
            // 释放内存
            free(buffer);
        }
        else if (size % circle == 3)
        {
            char* buffer = (char *)malloc(size);
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " realloc分配了 " << size << " 字节内存" << std::endl;
            #endif
            for (int i = 0; i < size; i++) {
                buffer[i] = i;
            }
            buffer = (char *)realloc(buffer, size + size/2);
            for (int i = size; i < size + size/2; i++) {
                buffer[i] = i;
            }
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " realloc重新分配了 " << size+size/2 << " 字节内存" << std::endl;
            #endif

            //foo();

            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(10*step));
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " realloc开始释放内存" << std::endl;
            #endif
            // 释放内存
            free(buffer);
        }
        else if (size % circle == 4)
        {
            A* buffer = new A();
            size = sizeof(A);
            #if PRINT_FLAG
            std::cout << "线程 " << gettid() << " new分配了 " << size << " 字节内存" << std::endl;
            #endif

            // for (int i = 0; i < size; i++) {
            //     buffer[i] = i;
            // }
            //foo();

            // 模拟一些处理时间
            std::this_thread::sleep_for(std::chrono::milliseconds(6*step));
            #if PRINT_FLAG

            std::cout << "线程 " << gettid() << " new开始释放内存" << std::endl;
            #endif
            // 释放内存
            delete buffer;
        }
        #if PRINT_FLAG
        std::cout << "线程 " << gettid() << " 释放了内存" << std::endl;
        #endif

        // 模拟一些处理时间
        std::this_thread::sleep_for(std::chrono::milliseconds(1*step));
    }
    #if PRINT_FLAG
    std::cout << "线程 " << gettid() << " 结束" << std::endl;
    #endif
}

void printSymbol(void* address) {
    Dl_info info;
    if (dladdr(address, &info)) {
        printf("Function: %s\n", info.dli_sname);
        printf("Library: %s\n", info.dli_fname);
    } else {
        printf("No symbol information available.\n");
    }
}


void printStackTrace() {
    void* array[10];
    //char **strings;
    printf("Stack trace start:\n");
    size_t size = backtrace(array, 10);
    printf("Stack trace size: %zu\n", size);
    //strings = backtrace_symbols(array, size);
    for (size_t i = 0; i < size; i++) {
        //printSymbol(array[i]);
        //printf("Addr: %x\n", array[i]);
        //printf("Symbol: %s\n", strings[i]);
    }
}
void test();
void monitorInput();
void foo()
{
    //printStackTrace();
    printf("[foo] start malloc\n");
    int* ptr = (int*)malloc(10 * sizeof(int));
    printf("[foo] end malloc\n");
    free(ptr);
    printf("[foo] end free\n");
    test();
    monitorInput();
}
// 用于接收用户输入并结束程序的函数
void monitorInput() {
    int input;
    //foo();
    while (true)
    {
        std::cin >> input;
        if (input == 0)
        {
            {
                std::lock_guard<std::mutex> lk(cv_m);
                running = false;
            }
            std::cout << "[tid:" << gettid() << "]" << "input " << input << std::endl;
            cv.notify_all();
            break;
        }
    }
}


void test() {
    printf("start\n");
    const int STACK_DEP = 10;
    int frame_count = 0;
    void *addresses[10];
    char **strings;
    frame_count = backtrace(addresses, STACK_DEP); 
    strings = backtrace_symbols(addresses, frame_count);
    for (int i = 0; i < frame_count; i++) {
        printf("addr %d: %p\n", i, addresses[i]);
        printf("frame %d: %s\n", i, strings[i]);
    }
}

int main(int argc, char** argv) {

    #if PRINT_FLAG
    cout << "[tid:" << gettid() << "]" << "main start" << endl;
    #endif
    //foo();
    //test();
    //return 0;
    const int numThreads = 500; // 线程数量
    std::vector<std::thread> threads;
    #if PRINT_FLAG
    cout << "[tid:" << gettid() << "]" << "main create thread" << endl;
    #endif

    // cout << "请输入数字：0：结束 1：不结束" << endl;

    // while(1)
    // {
    //     int input;
    //     std::cin >> input;
    //     if (input == 0)
    //     {
    //         cout << "结束" << endl;
    //         return 0;
    //     }
    //     else if (input == 1)
    //     {
    //         cout << "不结束" << endl;
    //         continue;
    //     }
    //     else
    //     {
    //         cout << "输入错误" << endl;
    //         continue;
    //     }
    // }

    // 创建线程
    
    //g_count_start = 1;
    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread(allocateAndFreeMemory, i));
    }
    
    //g_count_start = 0;
    #if PRINT_FLAG
    cout << "[tid:" << gettid() << "]" << "main create thread end" << endl;
    #endif
    // 创建监控输入的线程
    std::thread inputThread(monitorInput);
    #if PRINT_FLAG
    cout << "[tid:" << gettid() << "]" << "main create monitor thread end" << endl;
    #endif

    sleep(10);
    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (inputThread.joinable()) {
        inputThread.join();
    }
    //int end = 0;
    //cin >> end;
    //cout << "main end" << endl;

    return 0;
}
