#包含配置文件 config.cfg
#include config.cfg

TARGET = libpreload.so

CROSS=

CPP	= $(CROSS_)g++
CC	= $(CROSS_)gcc
LD	= $(CROSS_)ld
AR  = $(CROSS_)ar
RM	= rm -f
STRIP = $(CROSS_)strip


MEMTRACE_SRCPATH = ./src
MEMTRACE_LIBPATH = ./
OBJ_PATH = ./Obj
BIN_PATH = ./

#源文件
# SRC = threadstat.cpp
SRC = $(wildcard $(MEMTRACE_SRCPATH)/*.cpp)

#CFLAGS += -I./Include 
CFLAGS += -Wall -g --std=c++11
#CFLAGS +=  -ansi
#CFLAGS += -D_REENTRANT -msoft-float 

CFLAGS += -lpthread

#优化FP寄存器，默认开启
#TEST_LDFLAGS += -fomit-frame-pointer  
CFLAGS += -fno-omit-frame-pointer  

#默认情况下，编译器只会将程序中显式导出的符号（例如通过 extern 声明的符号）在最终的可执行文件中保留为可见的符号。其他符号可能会被剥离或只在本地可见。-rdynamic 选项会告诉编译器和链接器将所有符号都保留为全局可见。
#当你使用 backtrace_symbols 打印堆栈信息时，如果不使用 -rdynamic，你可能会看到一些地址而不是可读的函数名称。这是因为这些符号在链接过程中没有被保留。加上 -rdynamic 后，函数名和其他符号会被保留并可以正确解析。
CFLAGS += -rdynamic

#做backtrace最方便的就是使用gcc自带的backtrace功能，编译的时候加上-funwind-tables选项（该选项对性能无影响但是会使可执行文件略微变大），异常处理函数中调用相关函数即可输出函数调用栈，但是这依赖于你所用的编译工具链是否支持
CFLAGS += -funwind-tables

#开启地址空间布局随机化（ASLR）
#TEST_LDFLAGS += -fPIE -pie

#在某些编译器中，可能有选项可以影响 backtrace 的使用。例如，使用 -fno-unwind-tables 或者 -fno-asynchronous-unwind-tables 选项来避免生成某些调试信息。
#-fno-unwind-tables：禁用生成用于异常处理和栈展开的 .eh_frame 节中的解压表。这些表通常用于 C++ 的异常处理机制，帮助在异常抛出时正确展开栈并调用析构函数。
#-fno-asynchronous-unwind-tables：禁用生成用于异步信号处理和调试器的解压表。这些表在异步信号或调试时，帮助定位异常发生时的函数调用位置。
#这些选项减少了用于异常处理和调试的元数据，但并不会影响调用栈的结构。换句话说，它们不会移除返回地址，也不会阻止 backtrace 通过返回地址来获取调用栈信息。
#TEST_LDFLAGS += -fno-unwind-tables
#TEST_LDFLAGS += -fno-asynchronous-unwind-tables

#开启堆栈保护,使用一些编译器选项（如 -fstack-protector-all 或类似的选项）来加强堆栈保护，尽管这些选项本质上不会阻止 backtrace，但可能会在某些情况下导致 backtrace 不再有效。
#TEST_LDFLAGS += -fstack-protector-all

#一些编译器可能启用了栈保护（如 -fstack-protector）或堆保护（如 -D_FORTIFY_SOURCE=2）。这些保护机制可能会影响调用栈的可用性。尝试禁用这些选项以测试是否是这些保护引起的问题。
#CFLAGS += -D_FORTIFY_SOURCE=2

#如果 malloc 在某些情况下被内联，调用栈的路径可能会被优化掉。确保在调用 malloc 的函数没有被优化为内联函数。
#可以通过 -fno-inline 来禁用内联优化。
#CFLAGS += -fno-inline

#如果 USE_BACKTRACK为true，则增加宏USE_BACKTRACK
# ifeq ($(USE_BACKTRACE),true)
# 	CFLAGS += -DUSE_BACKTRACE
# endif

# #如果使用USE_STACK，则增加宏USE_STACK
# ifeq ($(USE_STACK),true)
# 	CFLAGS += -DUSE_STACK
# endif

# #如果USE_ALLOC为true，则增加宏USE_ALLOC
# ifeq ($(USE_ALL),true)
# 	CFLAGS += -DUSE_ALL
# endif

# #如果 USE_SIZE为true，则增加宏USE_SIZE
# ifeq ($(USE_SIZE),true)
# 	CFLAGS += -DUSE_SIZE
# endif

# #如果USE_THREAD为true，则增加宏USE_THREAD
# ifeq ($(USE_THREAD),true)
# 	CFLAGS += -DUSE_THREAD
# endif

# #如果USE_LARGE为true，则增加宏USE_LARGE
# ifeq ($(USE_LARGE),true)
# 	CFLAGS += -DUSE_LARGE
# endif

# #增加默认栈深度的宏STACK_DEPTH
# CFLAGS += -DSTACK_DEPTH=$(STACK_DEPTH)

# #如果PATH_LIST_DIR值不为空，则增加宏PATH_LIST_DIR
# ifneq ($(PATH_LIST_DIR),)
# 	CFLAGS += -DPATH_LIST_DIR=$(PATH_LIST_DIR)
# endif

# #如果PATHMAP值不为空，则增加宏PATHMAP
# ifneq ($(PATH_MAP_DIR),)
# 	CFLAGS += -DPATH_MAP_DIR=$(PATH_MAP_DIR)
# endif

# #如果 PATH_PARAM 值不为空，则增加宏 PATH_PARAM
# ifneq ($(PATH_PARAM),)
# 	CFLAGS += -DPATH_PARAM=$(PATH_PARAM)
# endif

# #增加宏OUTPUT_BUFSIZE_LAST
# CFLAGS += -DOUTPUT_BUFSIZE_LAST=$(OUTPUT_BUFSIZE_LAST)

# #增加宏TIME_INTERVAL
# CFLAGS += -DTIME_INTERVAL=$(TIME_INTERVAL)

# #增加宏TIME_INTERVAL_WRITE
# CFLAGS += -DTIME_INTERVAL_WRITE=$(TIME_INTERVAL_WRITE)


all: $(TARGET)

$(TARGET): clean $(SRC)
	$(CPP) -shared -fpic $(CFLAGS)  -o $(TARGET) $(SRC) 
	#$(STRIP) $(TARGET)

clean:
	rm -rf $(TARGET)

test:
	rm -rf sonia
	g++ -o sonia test_memory.cpp -ldl $(CFLAGS)
	#$(STRIP) sonia



#APP_LDFLAGS += -L./
APP_LDFLAGS += -g --std=c++11

pre:
	#mkdir -p $(OBJ_PATH)

	