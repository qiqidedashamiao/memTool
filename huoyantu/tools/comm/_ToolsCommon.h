_ToolsCommon.h
// Common for all Tool's internal use only.
// #include <ThinkOS/ArchBase/TOS_Base4ALL.h>
#include <ThinkOS/ArchBase/TOS_BaseDataDiagMemInfo.h>  //BaseData::DiagMemInfo

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef ___TOOL_COMMON_H__
#define ___TOOL_COMMON_H__
#ifdef __cplusplus
extern "C" {
#endif

    // API Prefix::_DiagMemTC_doXYZ

    void _DiagMemTC_printUsage(const char *pPrrgName);

    __attribute_used__ static inline void _DiagMemTC_printVersion(const char *pProgName)
    {
        fprintf(stderr, "%s(Build@%s %s)\n", basename((char *)pProgName), __DATE__, __TIME__);
        exit(0);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef struct
    {
        int VebosLevel;                 // 0<DFT>, 1<ON>: -V, 2<MORE>: -VV
        const char *pFName;             // Set by specific ToolXYZ
        TOS_BaseDiagMemInfo_pT pCurDMI; // Current to be processing DiaMemInfo

        //-------------------------------------------------------------------------
        // Following are set and used by ToolsCommon
        const char *pProcBuf;
        long BufOff;
        long BufLen;

        uint32_t NextSeqID;
        int FileFD;

        bool IsPipeXZ;
        bool IsMmapFile;
    } _TOCM_InputCtx_T, *_ToolCommonCtx_pT;

    typedef void (*_DiagMemTC_procEachDiagMemInfo_F)(_ToolCommonCtx_pT pComCtx, void *pProcPirv);

    typedef struct
    {
        _TOCM_InputCtx_T ComCtx;

    } _TOCM_InputMmapFileCtx_T, *_ToolCommonMmapFileCtx_pT;

    typedef struct
    {
        _TOCM_InputCtx_T ComCtx;

        char RdBuf[1 << 16]; // Pipe<=64KB
        long RdBufOff;       // Offset from RdBuf[0]
        long RdBufLen;       // Valid in RdBuf[0 ~ Len-1]
    } _ToolCommonReadFileCtx_T, *_ToolCommonReadFileCtx_pT;

    typedef struct
    {
        _TOCM_InputCtx_T ComCtx;
    } _TOCM_InputPipeXZCtx_T, *_ToolCommonPipeXZCtx_pT;

    void _DiagMemTC_pipeFromXZ_procEachDiagMemInfo(_ToolCommonPipeXZCtx_pT, _DiagMemTC_procEachDiagMemInfo_F, void *);
    void _DiagMemTC_mmapFile_procEachDiagMemInfo(_ToolCommonMmapFileCtx_pT, _DiagMemTC_procEachDiagMemInfo_F, void *);
    void _DiagMemTC_readFile_procEachDiagMemInfo(_ToolCommonReadFileCtx_pT, _DiagMemTC_procEachDiagMemInfo_F, void *);

    FILE *_DiagMemTC_openFile_pipeFromXZ(char *pFName);

    FILE *_DiagMemTC_openFile_pipeFromXZ(char *pFName);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef enum {
      _StatModeAllDMIs = 0,  //<DFT>, from first to last all DMIs will be stated.
                             // TODO: _STAT_MODE_ALLOC_SIZE_PEAK, //
                             // TODO: _STAT_MODE_INUSE_SIZE_PEAK, //
    } _TOCM_StatMode_T;

    typedef enum {
      _StatSortByAllocSizeInUse = 0,  //<DFT>
      _StatSortByAllocCountInUse,

      _StatSortByAllocSizeTotal,
      _StatSortByAllocCountTotal,

      // TODO: _StatSortByAllocSizeInUsePeak,

    } _TOCM_StatSortParam_T;

    typedef struct _TOCM_HashCallStackStatElmt_stru _TOCM_HashCallStackStatElmt_T;
    typedef _TOCM_HashCallStackStatElmt_T *_TOCM_HashCallStackStatElmt_pT;
    struct _TOCM_HashCallStackStatElmt_stru {
        _TOCM_HashCallStackStatElmt_pT pNext;

        uint64_t HashValue;  //__DiagMemTC_calcHashValue_useMurmurHash64A of CallStackDepth*CallStacks[];

        struct {
          uint64_t AllocCounts, FreeCounts, AllocBytes, FreeBytes;
        } Data;

        unsigned long CallStackDepth;
        void *CallStacks[0];
    };

    typedef struct {
        int VebosLevel;  // 0<DFT>, 1<ON>: -V, 2<MORE>: -VV

        struct {
          _TOCM_StatMode_T Mode;  //
        } Cfg;

        struct {
          unsigned long HashedElmtCnt;  // How many hashed element in pHashTable
        } Data;

        unsigned long HashTableRowNum;
        _TOCM_HashCallStackStatElmt_pT *pHashTable;  //[0 ~ HashTableRowNum-1]

    } _TOCM_HashCallStackStatorCtx_T, *_TOCM_HashCallStackStatorCtx_pT;

    typedef struct {
        unsigned long HashTableRowNum;
    } _TOMC_HashCallStackStatorArgs_T, *_TOMC_HashCallStackStatorArgs_pT;

    void _DiagMemTC_initHashCallStackStator(_TOCM_HashCallStackStatorCtx_pT, _TOMC_HashCallStackStatorArgs_pT);
    void _DiagMemTC_deinitHashCallStackStator(_TOCM_HashCallStackStatorCtx_pT);

    void _DiagMemTC_updateHashCallStackStator(_TOCM_HashCallStackStatorCtx_pT, TOS_BaseDiagMemInfo_pT);

    typedef enum {
      CmpRstLessThan = -1,
      CmpRstEqual = 0,
      CmpRstGreaterThan = 1,

      CmpRstBug = 99,
    } _TOCM_QsortCmpResult_T;

    _TOCM_QsortCmpResult_T _DiagMemTC_cmpHashedCallStackStatElem_usedByQsort(
        const void *pMeberLeft /*_TOCM_HashCallStackStatElmt_pT* */,
        const void *pMeberRight /*_TOCM_HashCallStackStatElmt_pT* */, void *pArgSortParam /*_TOCM_StatSortParam_T*/);
#ifdef __cplusplus
}
#endif
#endif //___TOOL_COMMON_H__
