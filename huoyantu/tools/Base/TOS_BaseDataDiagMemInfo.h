#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
typedef int pid_t;  // Windows 上定义 pid_t
#include <winsock2.h> // Windows 上的网络头文件
#else
#include <sys/types.h>
#include <netinet/in.h> // POSIX 系统的网络头文件
#endif
#include <stdio.h>
#include <inttypes.h>


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef ___TOS_BASEDATADIAGMEMINFO_H__
#define ___TOS_BASEDATADIAGMEMINFO_H__
// Additional code can go here


#ifdef __cplusplus
extern "C" {
#endif

#define TOS_DIAGMEM_UDP_PACKET_MTU 1500
#define TOS_DIAGMEM_UDP_PACKET_MTU_RCVD 1500

#define TOS_DIAGMEM_MAGIC 0x12345678

// item type
#define TOS_DIAGMEM_TYPE_IDENTIFY_CAPSRC 0x10000000
#define TOS_DIAGMEM_TYPE_MAF_ENVRC 0x00000001
#define TOS_DIAGMEM_TYPE_MAF_ENVRC_M64 0x00000002

// memory allocation/free operation codes
#define TOS_DIAGMEM_MAF_DO_ALLOC_UVM 0x00000001
#define TOS_DIAGMEM_MAF_DO_ALLOC_PAGE 0x00000002
#define TOS_DIAGMEM_MAF_DO_ALLOC_SLAB 0x00000003
#define TOS_DIAGMEM_MAF_DO_FREE_UVM 0x00000004
#define TOS_DIAGMEM_MAF_DO_FREE_PAGE 0x00000005
#define TOS_DIAGMEM_MAF_DO_FREE_SLAB 0x00000006


#define TOS_DIAGMEM_CALL_DEPTH_MAX 30



#define TOS_DIAGMEM_FNAME_STRFTIME "%Y%m%d_%H%M%S"
#define TOS_DIAGMEM_FNAME_PREFIX_FMT_PRINT "DiagMem_%s_%s_%s_%u_%s"
#define TOS_DIAGMEM_FNAME_SUFFIX_BINT ".binT"
#define TOS_DIAGMEM_FNAME_PREFIX_MODID "Recv"
#define TOS_DIAGMEM_FNAME_SUFFIX_LOG ".log"

#define TOS_DIAGMEM_FNAME_SUFFIX_HEAP ".heap"


typedef struct TOS_DiagMemHead_T {
    uint32_t Magic; // Magic number to identify the structure
    uint32_t SeqID; // Sequence ID for ordering
    uint32_t BodyType; // Type of the body
    uint32_t BodyLen; // Length of the body
    pid_t PID; // Process ID
    // time_t TvSec; // Timestamp in seconds
} TOS_DiagMemHead_T, *TOS_DiagMemHead_pT;

typedef struct TOS_DiagMemBody_MAF_EnvRc_T {
    uint32_t OpCode; // Operation code (e.g., allocation, free)
    uint32_t OpSize; // Size of the operation
    uint64_t OpAddr; // Address of the operation
    unsigned long CallDepth; 
    unsigned long CallStacks[TOS_DIAGMEM_CALL_DEPTH_MAX]; 
} TOS_DiagMemBody_MAF_EnvRc_T, *TOS_DiagMemBody_MAF_EnvRc_pT;

typedef struct TOS_DiagMemBody_MAF_EnvRcM64_T {
    uint32_t OpCode; // Operation code (e.g., allocation, free)
    uint32_t OpSize; // Size of the operation
    uint64_t OpAddr; // Address of the operation
    unsigned long CallDepth;
    unsigned long CallStacks[TOS_DIAGMEM_CALL_DEPTH_MAX]; // Call stacks for the operation
} TOS_DiagMemBody_MAF_EnvRcM64_T, *TOS_DiagMemBody_MAF_EnvRcM64_pT;

typedef struct TOS_DiagMemBody_CapSrcDesc_T {
    struct in_addr TargetIPAddr; // Target IP address
    char ProgName[64]; // Program name
    // pid_t PID; // Process ID
} TOS_DiagMemBody_CapSrcDesc_T, *TOS_DiagMemBody_CapSrcDesc_pT;

typedef struct TOS_BaseDiagMemInfo_T {
    TOS_DiagMemHead_T Head; // Header of DiagMemInfo
    union {
        TOS_DiagMemBody_MAF_EnvRc_T MAF_EnvRc; // Memory Allocation/Free Environment Return Code
        TOS_DiagMemBody_MAF_EnvRcM64_T MAF_EnvRcM64; // Memory Allocation/Free Environment Return Code for 64-bit
        TOS_DiagMemBody_CapSrcDesc_T CapSrcDesc; // Capture Source Description
        // Other body types can be added here
    } Body;
} TOS_BaseDiagMemInfo_T, *TOS_BaseDiagMemInfo_pT;


void _tos_abort()
{

}


void TOS_DiagMIF_fprintf_AllocFreeStackLogs(FILE *pFile, TOS_BaseDiagMemInfo_pT pDMI)
{
    // This function should implement the logic to print allocation/free stack logs
    // For now, it is a placeholder
    // fprintf(pFile, "Allocation/Free Stack Logs for PID: %d\n", pDMI->Head.PID);
}


#ifdef __cplusplus
}
#endif
#endif // ___TOS_BASEDATADIAGMEMINFO_H__
