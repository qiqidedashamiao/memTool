tos-cvntGProfFmt.c
//RefMore: https://yfgitlab.dahuatech.com/CleanArchitecture/iPSA-Architecture_Definition/-/blob/master/ArchV23/Qualities/DiagMemAllocFree.md

#include <ThinkOS/ArchBase/TOS_BaseDataDiagMemInfo.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <math.h>

static unsigned long _mBeVerbose = false;
static bool _mIsAllocSizeInUsePeakMode = false;

//-----------------------------------------------------------------------------
// MurmurHash2, 64-bit versions, by Austin Appleby
// code from: https://sites.google.com/site/murmurhash/, 64A means 64-bit hash for 64-bit platforms
uint64_t TOS_DiagMIF_Stat_calcHashValue_withMurmurHash64A(const void *key, unsigned long len, unsigned int seed) {
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^ (len * m);
    const uint64_t *data = (const uint64_t *) key;
    const uint64_t *end = data + (len / 8);
    while (data != end) {
        uint64_t k = *data++;
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }

    const unsigned char *data2 = (const unsigned char *) data;

    switch (len & 7) {
        case 7:
            h ^= (uint64_t)(data2[6]) << 48;
        case 6:
            h ^= (uint64_t)(data2[5]) << 40;
        case 5:
            h ^= (uint64_t)(data2[4]) << 32;
        case 4:
            h ^= (uint64_t)(data2[3]) << 24;
        case 3:
            h ^= (uint64_t)(data2[2]) << 16;
        case 2:
            h ^= (uint64_t)(data2[1]) << 8;
        case 1:
            h ^= (uint64_t)(data2[0]);
            h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return h;
}


typedef struct _TOS_DiagMIF_StatHashElement_stru
{
    struct _TOS_DiagMIF_StatHashElement_stru *pNext;

    
    uint64_t HashValue;

    struct 
    {
        uint64_t AllocCounts, FreeCounts, AllocSizeTotal, AllocSizeInUse;
    } Stat;
    
    unsigned long CallDepth;
    void *CallStacks[0];

} TOS_DiagMIF_StatHashElement_T, *TOS_DiagMIF_StatHashElement_pT;

typedef struct _TOS_DiagMIF_StatHashElementCache_stru
{
    union
    {
        uint64_t OpAddr;
    }Key;

    uint32_t OpSize;
    TOS_DiagMIF_StatHashElement_pT pElmt;
    struct _TOS_DiagMIF_StatHashElementCache_stru *pNext;
} TOS_DiagMIF_StatHashElementCache_T, *TOS_DiagMIF_StatHashElementCache_pT;

#define _mDiagMIF_StatHashTableRows 1024

typedef enum _TOS_DiagMIF_StatSort_enum
{
    DiagMIF_StatSortParamMin = 0,

    DiagMIF_StatSortByAllocSizeTotal = DiagMIF_StatSortParamMin,
    DiagMIF_StatSortByAllocCountTotal,

    DiagMIF_StatSortByAllocSizeInUse,
    DiagMIF_StatSortByAllocCountInUse,

    DiagMIF_StatSortByAllocSizeInUsePeak,

    DiagMIF_StatSortParamMax,
} TOS_DiagMIF_StatSortParam_T;

static inline const char* DiagMIF_Stat_getSortParamName(TOS_DiagMIF_StatSortParam_T SortParam)
{
    switch ( SortParam)
    {
    case DiagMIF_StatSortByAllocSizeTotal:      return "byAllocSizeTotal";
    case DiagMIF_StatSortByAllocCountTotal:     return "byAllocCountTotal";
    case DiagMIF_StatSortByAllocSizeInUse:      return "byAllocSizeInUse";
    case DiagMIF_StatSortByAllocCountInUse:     return "byAllocCountInUse";
    case DiagMIF_StatSortByAllocSizeInUsePeak:  return "byAllocSizeInUsePeak";
    
    default: return "byBUG";
    }
}

//Stator=Statistician
typedef struct _TOS_DiagMIF_Stator_stru
{
    struct 
    {
        bool IsAllocSizePeak;//FIXME: REMOVE RefOpt: -P

        TOS_DiagMIF_StatSortParam_T SortParam;//RefOpt: -S 0/1/2/3

        const char* pFNamePrefix;//RefOpt: -O

        int CallDepthMax;//RefOpt: -D

        bool IsCutInUseZeroZero;//RefOpt: -C

        bool IsOutputFileFmtExcelCSV;//RefOpt: -E
    } Cfg;

    struct 
    {
        time_t ProcStartSecond;

        uint64_t AllocSizeTotal, AllocSizeInUse, AllocSizeInUsePeak;
        uint32_t AllocSizeMax, AllocSizeMin;
        
        uint64_t AllocCountTotal, AllocCountInUse;

        uint64_t ElmtCountInHashTable, ElmtCountInHashTableCache, InfoCountTotal;
        uint64_t ForwardSkipSeqIDCountTotal, BackwardSkipSeqIDCountTotal, MissFreeCacheCounts;

        time_t   TvSecBegin, TvSecEnd;
        uint32_t SeqIdFirst, SeqIdLast;
    } Stat;
    
    struct 
    {
        TOS_DiagMIF_StatHashElement_T ElmtRows[_mDiagMIF_StatHashTableRows];
    } HashTable;

    struct 
    {
        TOS_DiagMIF_StatHashElement_pT pElmtRowsSnapshot[_mDiagMIF_StatHashTableRows];
    } HashTableSnapshots;

    #define HashElmtCacheArraySize  65536

    struct 
    {
        long ElmtCachedNum;
        TOS_DiagMIF_StatHashElementCache_T Head;
    } HashElmtCache[HashElmtCacheArraySize];

} TOS_DiagMIF_Stator_T, *TOS_DiagMIF_Stator_pT;

void TOS_DiagMIF_Stat_updateAndRemoveHashItemCache(TOS_DiagMIF_Stator_pT pStator, TOS_BaseDiagMemInfo_pT pDMI)
{
    uint64_t OpAddr = 0;

    switch ( pDMI->Head.BodyType )
    {
    case TOS_DIAGMEM_TYPE_MAF_ENVRC:
    {
        if(( pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_FREE_UVM ) || 
           ( pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_FREE_PAGE ) ||
           ( pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_FREE_SLAB ))
        {
            OpAddr = pDMI->Body.MAF_EnvRc.OpAddr;
        }
        else
        {
            _tos_abort();
        }
    }break;
    
    case TOS_DIAGMEM_TYPE_MAF_ENVRC_M64:
    {
        if(( pDMI->Body.MAF_EnvRcM64.OpCode == TOS_DIAGMEM_MAF_DO_FREE_UVM ) || 
           ( pDMI->Body.MAF_EnvRcM64.OpCode == TOS_DIAGMEM_MAF_DO_FREE_PAGE ) ||
           ( pDMI->Body.MAF_EnvRcM64.OpCode == TOS_DIAGMEM_MAF_DO_FREE_SLAB ))
        {
            OpAddr = pDMI->Body.MAF_EnvRcM64.OpAddr;
        }
        else
        {
            _tos_abort();
        }
    }break;

    default: _tos_abort();
    }

    TOS_DiagMIF_StatHashElement_pT pElmt = NULL;
    TOS_DiagMIF_StatHashElementCache_pT pItemCachePrev, pItemCacheCur;
    int Index = (OpAddr / 16) % HashElmtCacheArraySize;
    pItemCachePrev = &pStator->HashElmtCache[Index].Head;
    pItemCacheCur  = pItemCachePrev->pNext;

    do
    {
        if( pItemCacheCur == NULL ){ break; }

        if( pItemCacheCur->Key.OpAddr == OpAddr )
        {
            pElmt = pItemCacheCur->pElmt;
            pElmt->Stat.FreeCounts++;
            if (pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_FREE_PAGE)
            {
                pElmt->Stat.AllocSizeInUse   -= 4096*(size_t)pow(2, pItemCacheCur->OpSize);
            }
            else
            {
                pElmt->Stat.AllocSizeInUse   -= pItemCacheCur->OpSize;
            }

            // if( (unsigned long)pElmt->CallStacks[0] >= (10<<20) )
            // {
            //     fprintf(stderr, "[DEBUG]: OpAddr(%p)/OpSize(%u) @ CallStack[0]=%p\n", 
            //                 (void*)OpAddr, pItemCacheCur->OpSize, pElmt->CallStacks[0]);
            // }

            pStator->Stat.AllocCountInUse--;
            if (pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_FREE_PAGE)
            {
                pStator->Stat.AllocSizeInUse -= 4096*(size_t)pow(2, pItemCacheCur->OpSize);
            }
            else
            {
                pStator->Stat.AllocSizeInUse -= pItemCacheCur->OpSize;
            }

            pItemCachePrev->pNext = pItemCacheCur->pNext;
            free(pItemCacheCur);

            pStator->Stat.ElmtCountInHashTableCache--;
            pStator->HashElmtCache[Index].ElmtCachedNum--;
            break;
        }
        else
        {
            pElmt = NULL;
            pItemCachePrev = pItemCacheCur;
            pItemCacheCur  = pItemCacheCur->pNext;
        }
    } while ( 0x20230904 );

    if( !pElmt )
    {
        // fprintf(stderr, "Can't find StatHashItemCache:\nBEGIN===>\n\t");
        //     TOS_DiagMIF_fprintf_AllocFreeStackLogs(stderr, pDMI);
        // fprintf(stderr, "<===END\n");

        pStator->Stat.MissFreeCacheCounts++;
    }

    return;
}

void TOS_DiagMIF_Stat_writeHashItemCache
    ( TOS_DiagMIF_Stator_pT pStator, TOS_BaseDiagMemInfo_pT pDMI, TOS_DiagMIF_StatHashElement_pT pElmt)
{
    uint64_t OpAddr = 0; uint32_t OpSize = 0;

    switch ( pDMI->Head.BodyType )
    {
    case TOS_DIAGMEM_TYPE_MAF_ENVRC:
    {
        if(( pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_ALLOC_UVM ) || 
           ( pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_ALLOC_PAGE ) ||
           ( pDMI->Body.MAF_EnvRc.OpCode == TOS_DIAGMEM_MAF_DO_ALLOC_SLAB ))
        {
            OpAddr = pDMI->Body.MAF_EnvRc.OpAddr;
            OpSize = pDMI->Body.MAF_EnvRc.OpSize;
        }
        else
        {
            _tos_abort();
        }
    }break;
    
    case TOS_DIAGMEM_TYPE_MAF_ENVRC_M64:
    {
        if(( pDMI->Body.MAF_EnvRcM64.OpCode == TOS_DIAGMEM_MAF_DO_ALLOC_UVM ) || 
           ( pDMI->Body.MAF_EnvRcM64.OpCode == TOS_DIAGMEM_MAF_DO_ALLOC_PAGE ) ||
           ( pDMI->Body.MAF_EnvRcM64.OpCode == TOS_DIAGMEM_MAF_DO_ALLOC_SLAB ))
        {
            OpAddr = pDMI->Body.MAF_EnvRcM64.OpAddr;
            OpSize = pDMI->Body.MAF_EnvRcM64.OpSize;
        }
        else
        {
            _tos_abort();
        }

    }break;

    default: _tos_abort();
    }

    TOS_DiagMIF_StatHashElementCache_pT pElmtCache = calloc(1, sizeof(TOS_DiagMIF_StatHashElementCache_T));
    if( NULL == pElmtCache ){ _tos_abort(); }

    pStator->Stat.ElmtCountInHashTableCache++;

    pElmtCache->Key.OpAddr = OpAddr;

    pElmtCache->OpSize = OpSize;
    pElmtCache->pElmt  = pElmt;

    // if( (unsigned long)pElmt->CallStacks[0] >= (10<<20) )
    // {
    //     fprintf(stderr, "[DEBUG]: OpAddr(%p)/OpSize(%u) @ CallStack[0]=%p\n", 
    //                 (void*)OpAddr, OpSize, pElmt->CallStacks[0]);
    // }
    int Index = (OpAddr / 16) % HashElmtCacheArraySize;
    pElmtCache->pNext  = pStator->HashElmtCache[Index].Head.pNext;
    pStator->HashElmtCache[Index].Head.pNext = pElmtCache;
    pStator->HashElmtCache[Index].ElmtCachedNum++;
}

void TOS_DiagMIF_Stat_updateDiagMemInfo( TOS_DiagMIF_Stator_pT pStator, TOS_BaseDiagMemInfo_pT pDMI )
{
    static int PageSize = 0;
    if( !PageSize ){ PageSize = getpagesize(); };

    TOS_DiagMemHead_pT pHead = &pDMI->Head;

    //Only TYPE=MemAllocFree support now.
    if( !((TOS_DIAGMEM_TYPE_MAF_ENVRC == pHead->BodyType)
            || (TOS_DIAGMEM_TYPE_MAF_ENVRC_M64 == pHead->BodyType)) )
    {
        return;
    }

    bool IsOpMemFree = false;
    size_t MemBlockSize = 0;

    unsigned long CallDepth = 0; 
    unsigned long CallStackSiz = 0;
    void *CallStacks[TOS_DIAGMEM_CALL_DEPTH_MAX] = {NULL,};

    switch( pHead->BodyType )
    {
    case TOS_DIAGMEM_TYPE_MAF_ENVRC:
    {
        TOS_DiagMemBody_MAF_EnvRc_pT pBody = &pDMI->Body.MAF_EnvRc;
        CallDepth = pBody->CallDepth < pStator->Cfg.CallDepthMax 
                        ? pBody->CallDepth : pStator->Cfg.CallDepthMax;

        for( int dps=0; dps<CallDepth && dps<TOS_DIAGMEM_CALL_DEPTH_MAX; dps++ )
        {
            CallStacks[dps] = (void*)(unsigned long)pBody->CallStacks[dps];
        }

        if(( pBody->OpCode == TOS_DIAGMEM_MAF_DO_FREE_UVM ) || 
           (pBody->OpCode == TOS_DIAGMEM_MAF_DO_FREE_PAGE ) ||
           (pBody->OpCode == TOS_DIAGMEM_MAF_DO_FREE_SLAB ))
        {
            IsOpMemFree = true;
        }
        else
        {
            if (pBody->OpCode == TOS_DIAGMEM_MAF_DO_ALLOC_PAGE)
            {
                MemBlockSize = PageSize * (size_t)pow(2, pBody->OpSize);
            }
            else
            {
                MemBlockSize = pBody->OpSize;
            }
        }
    }
    break;

    case TOS_DIAGMEM_TYPE_MAF_ENVRC_M64:
    {
        TOS_DiagMemBody_MAF_EnvRcM64_pT pBodyM64 = &pDMI->Body.MAF_EnvRcM64;
        CallDepth = pBodyM64->CallDepth < pStator->Cfg.CallDepthMax 
                        ? pBodyM64->CallDepth : pStator->Cfg.CallDepthMax;

        for( int dps=0; dps<CallDepth && dps<TOS_DIAGMEM_CALL_DEPTH_MAX; dps++ )
        {
            CallStacks[dps] = (void*)pBodyM64->CallStacks[dps];
        }

        if(( pBodyM64->OpCode == TOS_DIAGMEM_MAF_DO_FREE_UVM ) || 
           ( pBodyM64->OpCode == TOS_DIAGMEM_MAF_DO_FREE_PAGE ) ||
           ( pBodyM64->OpCode == TOS_DIAGMEM_MAF_DO_FREE_SLAB ))
        {
            IsOpMemFree = true;
        }
        else
        {
            MemBlockSize = pBodyM64->OpSize;
        }
    }
    break;

    default: _tos_abort();
    }

    TOS_DiagMIF_StatHashElement_pT pElmt = NULL;
    CallStackSiz = CallDepth * sizeof(void*);

    if( IsOpMemFree )
    {
        TOS_DiagMIF_Stat_updateAndRemoveHashItemCache(pStator, pDMI);
        return;
    }
    else
    {
        uint64_t HashValue = TOS_DiagMIF_Stat_calcHashValue_withMurmurHash64A(CallStacks, CallStackSiz, pHead->BodyType);
        pElmt = pStator->HashTable.ElmtRows[HashValue % _mDiagMIF_StatHashTableRows].pNext;

        do
        {
            if( NULL == pElmt ){ break; }

            if( (pElmt->HashValue == HashValue)
                    && (pElmt->CallDepth == CallDepth)
                    && (!memcmp(pElmt->CallStacks, CallStacks, CallStackSiz)) )
            {
                break;
            }

            pElmt = pElmt->pNext;
        } while ( 0x20230904 );
        
        if( NULL == pElmt )
        {
            size_t ItemSiz = sizeof(TOS_DiagMIF_StatHashElement_T) + CallStackSiz;
            pElmt = calloc(1, ItemSiz); if( NULL == pElmt ){ _tos_abort(); }

            pElmt->HashValue = HashValue;
            pElmt->CallDepth = CallDepth;
            memcpy(pElmt->CallStacks, CallStacks, CallStackSiz);

            pElmt->pNext = pStator->HashTable.ElmtRows[HashValue % _mDiagMIF_StatHashTableRows].pNext;
            pStator->HashTable.ElmtRows[HashValue % _mDiagMIF_StatHashTableRows].pNext = pElmt;

            pStator->Stat.ElmtCountInHashTable++;
        }

        //:::===pElmt->Stat
        pElmt->Stat.AllocCounts++;
        pElmt->Stat.AllocSizeInUse += MemBlockSize;

        if( pStator->Cfg.IsAllocSizePeak )
        {
            if( MemBlockSize > pElmt->Stat.AllocSizeTotal )
            {
                pElmt->Stat.AllocSizeTotal = MemBlockSize;
            }
        }
        else
        {
            pElmt->Stat.AllocSizeTotal += MemBlockSize;
        }

        //:::===pStator->Stat
        pStator->Stat.AllocCountTotal++;
        pStator->Stat.AllocSizeTotal += MemBlockSize;

        pStator->Stat.AllocCountInUse++;
        pStator->Stat.AllocSizeInUse += MemBlockSize;
        if( pStator->Stat.AllocSizeInUse > pStator->Stat.AllocSizeInUsePeak )
        {
            pStator->Stat.AllocSizeInUsePeak = pStator->Stat.AllocSizeInUse;
        }


        if( MemBlockSize > pStator->Stat.AllocSizeMax )
        {
            pStator->Stat.AllocSizeMax = MemBlockSize;
        }

        if( (MemBlockSize < pStator->Stat.AllocSizeMin)
                || (!pStator->Stat.AllocSizeMin) )
        {
            pStator->Stat.AllocSizeMin = MemBlockSize;
        }

        TOS_DiagMIF_Stat_writeHashItemCache(pStator, pDMI, pElmt);
    }
}

int TOS_DiagMIF_StatHashItemQsortCompare(const void *p1, const void *p2, void *p3) 
{
    TOS_DiagMIF_Stator_pT pStator = (TOS_DiagMIF_Stator_pT)p3;
    TOS_DiagMIF_StatHashElement_pT pElmtLeft = (TOS_DiagMIF_StatHashElement_pT)(*(void**)p1);
    TOS_DiagMIF_StatHashElement_pT pElmtRight = (TOS_DiagMIF_StatHashElement_pT)(*(void**)p2);

    uint64_t CmpValLeft = 0;
    uint64_t CmpValueRight = 0;

    switch ( pStator->Cfg.SortParam )
    {
    case DiagMIF_StatSortByAllocCountInUse:
        CmpValLeft    = pElmtLeft->Stat.AllocCounts - pElmtLeft->Stat.FreeCounts;
        CmpValueRight = pElmtRight->Stat.AllocCounts - pElmtRight->Stat.FreeCounts;
    break;

    case DiagMIF_StatSortByAllocCountTotal:
        CmpValLeft    = pElmtLeft->Stat.AllocCounts;
        CmpValueRight = pElmtRight->Stat.AllocCounts;
    break;

    case DiagMIF_StatSortByAllocSizeInUse:
    case DiagMIF_StatSortByAllocSizeInUsePeak:
        CmpValLeft    = pElmtLeft->Stat.AllocSizeInUse;
        CmpValueRight = pElmtRight->Stat.AllocSizeInUse;
    break;
    
    default:
        CmpValLeft    = pElmtLeft->Stat.AllocSizeTotal;
        CmpValueRight = pElmtRight->Stat.AllocSizeTotal;
        break;
    }


    if( CmpValLeft < CmpValueRight )
    {
        return 1;
    }
    else if( CmpValLeft == CmpValueRight ) 
    {
        return 0;
    }
    else 
    {
        return -1;
    }
}

void TOS_DiagMIF_Stat_fprintfGProfFmt_ofHashTable
    ( TOS_DiagMIF_Stator_pT pStator, TOS_DiagMIF_StatHashElement_pT pHashTable, int RowCount, FILE *pFile )
{
    TOS_DiagMIF_StatHashElement_pT pAllElmts [pStator->Stat.ElmtCountInHashTable];
    int ItemNum = 0;

    for( int rid=0; (rid<RowCount) && (ItemNum<pStator->Stat.ElmtCountInHashTable); rid++ )
    {
        TOS_DiagMIF_StatHashElement_pT pItem = pHashTable[rid].pNext;
        do 
        {
            if( NULL == pItem ){ break; }

            pAllElmts[ItemNum++] = pItem;
            pItem = pItem->pNext;
        } while( 0x20230904 );
    }

    qsort_r(pAllElmts, ItemNum, sizeof(TOS_DiagMIF_StatHashElement_pT), 
            TOS_DiagMIF_StatHashItemQsortCompare, (void*)pStator);

    fprintf(pFile, "heap profile: %6" PRIu64 ": %8" PRIu64 " [%6" PRIu64 ": %8" PRIu64 "] @ heapprofile\n",
        pStator->Stat.AllocCountInUse, pStator->Stat.AllocSizeInUse,
        pStator->Stat.AllocCountTotal, pStator->Stat.AllocSizeTotal);

    for( int ndx=0; ndx<ItemNum; ndx++ )
    {
        TOS_DiagMIF_StatHashElement_pT pItem = pAllElmts[ndx];
        uint64_t AllocCountInUse = pItem->Stat.AllocCounts - pItem->Stat.FreeCounts;

        if( ((pStator->Cfg.SortParam == DiagMIF_StatSortByAllocCountInUse) 
                || (pStator->Cfg.SortParam == DiagMIF_StatSortByAllocSizeInUse))
            && (pStator->Cfg.IsCutInUseZeroZero && !AllocCountInUse && !pItem->Stat.AllocSizeInUse) )
        {
            continue;
        }

        //-------------------------------------------------------------------------------------------------------------
        fprintf(pFile, "%6" PRIu64 ": %8" PRIu64 " [%8" PRIu64 ": %9" PRIu64 "] @",
                AllocCountInUse, pItem->Stat.AllocSizeInUse, pItem->Stat.AllocCounts, pItem->Stat.AllocSizeTotal);

        for( int idx=0; idx<pItem->CallDepth; idx++ )
        {
            fprintf(pFile, " %p", pItem->CallStacks[idx]);
        }

        fprintf(pFile, "\n");

        free(pItem);
    }
}

void TOS_DiagMIF_Stat_fprintfExcelCSV_ofHashTable
    ( TOS_DiagMIF_Stator_pT pStator, TOS_DiagMIF_StatHashElement_pT pHashTable, int RowCount, FILE *pFile )
{
    TOS_DiagMIF_StatHashElement_pT pAllElmts [pStator->Stat.ElmtCountInHashTable];
    int ItemNum = 0;

    for( int rid=0; (rid<RowCount) && (ItemNum<pStator->Stat.ElmtCountInHashTable); rid++ )
    {
        TOS_DiagMIF_StatHashElement_pT pItem = pHashTable[rid].pNext;
        do 
        {
            if( NULL == pItem ){ break; }

            pAllElmts[ItemNum++] = pItem;
            pItem = pItem->pNext;
        } while( 0x20230904 );
    }

    qsort_r(pAllElmts, ItemNum, sizeof(TOS_DiagMIF_StatHashElement_pT), 
            TOS_DiagMIF_StatHashItemQsortCompare, (void*)pStator);

    fprintf(pFile, "AllocCountInUse,AllocSizeInUse,AllocCountTotal,AllocSizeTotal,CallStacks\n");

    for( int ndx=0; ndx<ItemNum; ndx++ )
    {
        TOS_DiagMIF_StatHashElement_pT pItem = pAllElmts[ndx];
        uint64_t AllocCountInUse = pItem->Stat.AllocCounts - pItem->Stat.FreeCounts;

        if( ((pStator->Cfg.SortParam == DiagMIF_StatSortByAllocCountInUse) 
                || (pStator->Cfg.SortParam == DiagMIF_StatSortByAllocSizeInUse))
            && (pStator->Cfg.IsCutInUseZeroZero && !AllocCountInUse && !pItem->Stat.AllocSizeInUse) )
        {
            continue;
        }

        //-------------------------------------------------------------------------------------------------------------
        fprintf(pFile, "%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",",
                AllocCountInUse, pItem->Stat.AllocSizeInUse, pItem->Stat.AllocCounts, pItem->Stat.AllocSizeTotal);

        for( int idx=0; idx<pItem->CallDepth; idx++ )
        {
            fprintf(pFile, " %p", pItem->CallStacks[idx]);
        }

        fprintf(pFile, "\n");

        free(pItem);
    }
}

static bool _mIsFNameWithTimestamp = true;

void TOS_DiagMIF_Stat_fprintfGProfFmt( TOS_DiagMIF_Stator_pT pStator )
{
    char FName[512] = "";
    FILE *pFile = stdout;

    char TickNowBuf[64] = ""; time_t TickNow = time(NULL);
    strftime(TickNowBuf, sizeof(TickNowBuf), "%Y.%m.%d-%H.%M.%S", localtime(&TickNow));

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    const char *pFNameSuffix = TOS_DIAGMEM_FNAME_SUFFIX_HEAP;
    if( pStator->Cfg.IsOutputFileFmtExcelCSV )
    {
        pFNameSuffix = ".csv";
    }

    if( pStator->Cfg.IsAllocSizePeak )
    {
        if( pStator->Cfg.pFNamePrefix )
        {
            snprintf(FName, sizeof(FName), "%s(%s)_AllocSizePeak%s", 
                pStator->Cfg.pFNamePrefix, TickNowBuf, pFNameSuffix);
            
            fprintf(stderr, "Writing file: %s\n", FName);

            pFile = fopen(FName, "w");
            if( NULL == pFile ){ _tos_abort(); }
        }

        pStator->Cfg.SortParam = DiagMIF_StatSortByAllocSizeTotal;//Force byAllocSizeTotal
    }
    else
    {
        if( pStator->Cfg.pFNamePrefix )
        {
            if( !_mIsFNameWithTimestamp )
            {
                snprintf(FName, sizeof(FName), "%s_%s%s", pStator->Cfg.pFNamePrefix, 
                                DiagMIF_Stat_getSortParamName(pStator->Cfg.SortParam), pFNameSuffix);
            }
            else 
            {
                snprintf(FName, sizeof(FName), "%s(%s)_%s%s", pStator->Cfg.pFNamePrefix, TickNowBuf , 
                    DiagMIF_Stat_getSortParamName(pStator->Cfg.SortParam), pFNameSuffix);
            }
            
            fprintf(stderr, "Writing file: %s\n", FName);

            pFile = fopen(FName, "w");
            if( NULL == pFile ){ _tos_abort(); }
        }
    }

    if( pStator->Cfg.IsOutputFileFmtExcelCSV )
    {
        TOS_DiagMIF_Stat_fprintfExcelCSV_ofHashTable
            (pStator, &pStator->HashTable.ElmtRows[0], _mDiagMIF_StatHashTableRows, pFile);
    }
    else
    {
        TOS_DiagMIF_Stat_fprintfGProfFmt_ofHashTable
            (pStator, &pStator->HashTable.ElmtRows[0], _mDiagMIF_StatHashTableRows, pFile);
    }

    if( stdout != pFile ){ fclose(pFile); }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////    
    fprintf(stderr, "Using: %ld-Seconds processed %lu DiagMemInfos, skipped [+%lu(%.2f%%),-%lu(%.2f%%)], use HashElmts=%lu[Cached=%lu]\n", 
            TickNow - pStator->Stat.ProcStartSecond, pStator->Stat.InfoCountTotal, 
            pStator->Stat.ForwardSkipSeqIDCountTotal, (float)((float)pStator->Stat.ForwardSkipSeqIDCountTotal/(float)pStator->Stat.InfoCountTotal),
            pStator->Stat.BackwardSkipSeqIDCountTotal, (float)((float)pStator->Stat.BackwardSkipSeqIDCountTotal/(float)pStator->Stat.InfoCountTotal), 
            pStator->Stat.ElmtCountInHashTable, pStator->Stat.ElmtCountInHashTableCache);
    
    // fprintf(stderr, "More Stats: Duration=%u-Seconds(%s ~ %s)\n",
    //         pStator->Stat.TvSecEnd - pStator->Stat.TvSecBegin, 
    //         TvSecStartBuf, TvSecEndBuf);

    fprintf(stderr, "\tAllocCountInUse=%lu : AllocSizeInUse=%lu<Peak=%lu>" 
                    " [AllocCountTotal=%lu : AllocSizeTotal=%lu]\n",
            pStator->Stat.AllocCountInUse, pStator->Stat.AllocSizeInUse, pStator->Stat.AllocSizeInUsePeak,
            pStator->Stat.AllocCountTotal, pStator->Stat.AllocSizeTotal);
            
    fprintf(stderr, "\tAllocSize[Min=%u,Max=%u], SeqID[%u,%u], MissFreeCacheCounts=%lu\n",
            pStator->Stat.AllocSizeMin, pStator->Stat.AllocSizeMax,
            pStator->Stat.SeqIdFirst, pStator->Stat.SeqIdLast, 
            pStator->Stat.MissFreeCacheCounts);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
static TOS_DiagMIF_Stator_T _mDiagMIF_Stator = 
{
    .Cfg = 
    {
        .IsAllocSizePeak = false,
        .SortParam = DiagMIF_StatSortByAllocSizeInUse,
        .CallDepthMax = 10,
    },
};

static TOS_DiagMIF_Stator_T _mDiagMIF_Stator_forAllocSizeInUsePeak = 
{
    .Cfg = 
    {
        .SortParam = DiagMIF_StatSortByAllocSizeInUsePeak,
        .CallDepthMax = 10,
    },
};

typedef struct _ContextCommonDiagMemInfo_stru
{
    const char *pProcBuf;
    long BufOff;
    long BufLen;

    TOS_BaseDiagMemInfo_pT pCurDMI;
    TOS_DiagMIF_Stator_pT pCurStator;

    uint32_t NextSeqID;
} _ContextCommonDiagMemInfo_T, *_ContextCommonDiagMemInfo_pT;

typedef void (*_OpContextCommon_procCurDiagMemInfo_F)(_ContextCommonDiagMemInfo_pT);

void __CtxCommon_procCurDiagMemInfo_intoStator(_ContextCommonDiagMemInfo_pT pCtxDMI)
{
    TOS_BaseDiagMemInfo_pT pDMI = pCtxDMI->pCurDMI;

    if( (TOS_DIAGMEM_TYPE_MAF_ENVRC == pDMI->Head.BodyType)
            || (TOS_DIAGMEM_TYPE_MAF_ENVRC_M64 == pDMI->Head.BodyType) )
    {
        TOS_DiagMIF_Stator_pT pStator = pCtxDMI->pCurStator;

        if( _mIsAllocSizeInUsePeakMode && (pStator->Stat.AllocSizeInUsePeak > 0) 
            && (pStator->Stat.AllocSizeInUse == pStator->Stat.AllocSizeInUsePeak) )
        {
            return;//IS inPeakMode AND reachAllocSizeInusePeak, STOP updateDiagMemInfo
        }
        else
        {
            if( !_mIsAllocSizeInUsePeakMode && (_mBeVerbose >= 2) )
            {
                TOS_DiagMIF_fprintf_AllocFreeStackLogs(stderr, pDMI);
            }

            TOS_DiagMIF_Stat_updateDiagMemInfo(pStator, pDMI);
        }
    }
}

void __CtxCommon_procEachDiagMemInfo_inProcBuf_useProcCurDMI_F
    ( _ContextCommonDiagMemInfo_pT pCtxDMI, 
      _OpContextCommon_procCurDiagMemInfo_F ProcCurDMI_F )
{
    const char *pProcBufHead = &pCtxDMI->pProcBuf[0];
    TOS_BaseDiagMemInfo_pT pCurDMI = (TOS_BaseDiagMemInfo_pT)pProcBufHead;

    do
    {
        pCtxDMI->BufOff = (const char*)pCurDMI - pProcBufHead;
        if( pCtxDMI->BufOff == pCtxDMI->BufLen){ break;/*Current buffer fully processed*/ }
        if( pCtxDMI->BufOff > pCtxDMI->BufLen){ _tos_abort(); }

        unsigned long NotProcBufLen = pCtxDMI->BufLen - pCtxDMI->BufOff;
        if( NotProcBufLen < sizeof(TOS_DiagMemHead_T) ){ break;/*Read more data into buffer*/ }

        TOS_DiagMemHead_pT pHead = &pCurDMI->Head;
        if( TOS_DIAGMEM_MAGIC != pHead->Magic )
        {
            fprintf(stderr, "[ERROR]UnRecongnize DiagMemInfo::NextSeqID(%u)'s Magic(%x), MUST BE %lx\n",
                        pCtxDMI->NextSeqID, pHead->Magic, TOS_DIAGMEM_MAGIC);
            break;/*_tos_abort();*/ 
        }

        NotProcBufLen = NotProcBufLen - sizeof(TOS_DiagMemHead_T);
        if( NotProcBufLen < pHead->BodyLen ){ break;/*Read more data into buffer*/ }

        //---------------------------------------------------------------------
        //Process@SeqID
        if( !pCtxDMI->NextSeqID )
        {
            pCtxDMI->NextSeqID = pHead->SeqID + 1;//INITIAL
            pCtxDMI->pCurStator->Stat.SeqIdFirst = pHead->SeqID;
        }
        else if( pCtxDMI->NextSeqID != pHead->SeqID )
        {
            if( pHead->SeqID > pCtxDMI->NextSeqID )
            {
                //Problem: 
                //  [1]Miss some Alloc will cause AllocSizeTotal--, and some invalid Free
                //  [2]Miss some Free will cause AllocSizeTotal++,AllocSizeInUse++
                if( _mBeVerbose )
                {
                    fprintf(stderr, "[Warning]pCtxDMI->NextSeqID=%u, BUT pHead->SeqID=%u, Delta=+%u\n", 
                        pCtxDMI->NextSeqID, pHead->SeqID, pHead->SeqID - pCtxDMI->NextSeqID );
                }
                
                pCtxDMI->pCurStator->Stat.ForwardSkipSeqIDCountTotal += (pHead->SeqID - pCtxDMI->NextSeqID);
                pCtxDMI->NextSeqID = pHead->SeqID + 1;
                pCtxDMI->pCurStator->Stat.SeqIdLast = pHead->SeqID;
            }
            else
            {
                //Problem:
                //  [1]Disorder some Alloc will cause AllocSizeTotal++ because some Free may before Alloc
                if( _mBeVerbose )
                {
                    fprintf(stderr, "[Warning]pCtxDMI->NextSeqID=%u, BUT pHead->SeqID=%u, Delta=-%u\n", 
                        pCtxDMI->NextSeqID, pHead->SeqID, pCtxDMI->NextSeqID - pHead->SeqID );
                }

                pCtxDMI->pCurStator->Stat.BackwardSkipSeqIDCountTotal += (pCtxDMI->NextSeqID - pHead->SeqID);
            }
        }
        else
        {
            pCtxDMI->NextSeqID++; //FINE
            pCtxDMI->pCurStator->Stat.SeqIdLast = pHead->SeqID;
        }

        //---------------------------------------------------------------------
        if( TOS_DIAGMEM_TYPE_IDENTIFY_CAPSRC == pCurDMI->Head.BodyType )
        {
            if( INADDR_ANY == pCurDMI->Body.CapSrcDesc.TargetIPAddr.s_addr )
            {
                //IF capture source does not set TargetIPAddr, update with received IPAddr
                //pCurDMI->Body.CapSrcDesc.TargetIPAddr = pCtxDMI->CliSockAddr.sin_addr;
            }
        }

        //---------------------------------------------------------------------
        //TOS_DiagMIF_fprintf_AllocFreeStackLogs(stderr, pCurDMI);
        pCtxDMI->pCurStator->Stat.InfoCountTotal++;
        pCtxDMI->pCurDMI = pCurDMI;

        ProcCurDMI_F(pCtxDMI);

        //---------------------------------------------------------------------
        //Next DiagMemInfo:
        pCurDMI = (TOS_BaseDiagMemInfo_pT)((const char*)pCurDMI 
                        + sizeof(TOS_DiagMemHead_T) + pCurDMI->Head.BodyLen);
    } while ( 0x20230904 );
}

typedef struct _ContextMmapDiagMemInfo_stru
{
    _ContextCommonDiagMemInfo_T Common;

    char *pCurFName;

} _ContextMmapDiagMemInfo_T, *_ContextMmapDiagMemInfo_pT;


#include <sys/mman.h>
void __mmapFromNormalFile_procDiagMemInfo( _ContextMmapDiagMemInfo_pT pCtxMmapDMI )
{
    int FileFD = open(pCtxMmapDMI->pCurFName, O_RDONLY);
    if( FileFD < 0 ){ 
        fprintf(stderr, "open(%s) fail, %d(%s)\n", pCtxMmapDMI->pCurFName, errno, strerror(errno));
        _tos_abort(); 
    }

    struct stat stbuf;
    int RetPSX = fstat(FileFD, &stbuf);
    if( RetPSX < 0 ){ _tos_abort(); }

    void* pMmapAddr = mmap(NULL, stbuf.st_size, PROT_READ, MAP_PRIVATE, FileFD, 0);
    if( NULL == pMmapAddr ){ _tos_abort(); }

    pCtxMmapDMI->Common.BufOff      = 0;
    pCtxMmapDMI->Common.pProcBuf    = pMmapAddr;
    pCtxMmapDMI->Common.BufLen      = stbuf.st_size;
    __CtxCommon_procEachDiagMemInfo_inProcBuf_useProcCurDMI_F(&pCtxMmapDMI->Common, __CtxCommon_procCurDiagMemInfo_intoStator);

    munmap(pMmapAddr, stbuf.st_size);
    close(FileFD);
}

typedef struct _ContextReadDiagMemInfo_stru
{
    _ContextCommonDiagMemInfo_T Common;

    int fd;
    const char *pFName;

    //char  RdBuf[1<<12];//4K-PageSize
    char  RdBuf[1<<16];//Pipe<=64KB
    long   RdBufOff;//Offset from RdBuf[0]
    long   RdBufLen;//Valid in RdBuf[0 ~ Len-1]
} _ContextReadDiagMemInfo_T, *_ContextReadDiagMemInfo_pT;


void __readFromFileFD_procDiagMemInfo( _ContextReadDiagMemInfo_pT pCtxReadDMI )
{
    if( !(pCtxReadDMI->fd > 0) )
    {
        pCtxReadDMI->fd = open(pCtxReadDMI->pFName, O_RDONLY);
        if( pCtxReadDMI->fd < 0 ){ 
            fprintf(stderr, "open(%s) fail, %d(%s)\n", pCtxReadDMI->pFName, errno, strerror(errno));
            _tos_abort(); 
        }
    }

    pCtxReadDMI->RdBufOff = 0;
    pCtxReadDMI->RdBufLen = 0;

    do
    {
        char *pRdBufHead = &pCtxReadDMI->RdBuf[0] + pCtxReadDMI->RdBufOff;
        unsigned long  RdBufSiz = sizeof(pCtxReadDMI->RdBuf) - pCtxReadDMI->RdBufOff;
        if( RdBufSiz < sizeof(TOS_BaseDiagMemInfo_T) ){ _tos_abort(); }

        memset(pRdBufHead, 0, RdBufSiz);

        ssize_t RdLen = read(pCtxReadDMI->fd, pRdBufHead, RdBufSiz);
        if( RdLen <= 0 )
        {
            if( RdLen < 0 )
            {
                fprintf(stderr, "Reading(%s) fail, %d(%s)\n", pCtxReadDMI->pFName, errno, strerror(errno));
            }
            break;
        }

        pCtxReadDMI->RdBufLen = RdLen + pCtxReadDMI->RdBufOff;

        pCtxReadDMI->Common.BufOff      = 0;
        pCtxReadDMI->Common.pProcBuf    = &pCtxReadDMI->RdBuf[0];
        pCtxReadDMI->Common.BufLen      = pCtxReadDMI->RdBufLen;

        __CtxCommon_procEachDiagMemInfo_inProcBuf_useProcCurDMI_F(&pCtxReadDMI->Common, __CtxCommon_procCurDiagMemInfo_intoStator);
        
        if( pCtxReadDMI->Common.BufOff < pCtxReadDMI->Common.BufLen )
        {
            pCtxReadDMI->RdBufOff = pCtxReadDMI->Common.BufLen - pCtxReadDMI->Common.BufOff;

            memmove(pCtxReadDMI->RdBuf, 
                pCtxReadDMI->RdBuf + pCtxReadDMI->Common.BufOff, 
                    pCtxReadDMI->RdBufOff);
        }
        else
        {
            pCtxReadDMI->RdBufOff = 0;
        }

    } while ( 0x20230904 );
    

    close(pCtxReadDMI->fd);
    pCtxReadDMI->fd = -1;
}

void __pipeFromDecompressorXZ_procDiagMemInfo( char* const pFName )
{
    int PipeFD[2]; if( pipe(PipeFD) < 0 ){ _tos_abort(); }

    pid_t PID_XZ = vfork();
    if( (0) == PID_XZ )
    {
        close(PipeFD[0]);//close read end

        dup2(PipeFD[1], STDOUT_FILENO);//write to tos-cvntGProfFmt
        close(PipeFD[1]);

        //-----------------------------------------------------------------------------------------
        char* const argv[] = { "/bin/xz", "-d", "-c", "-k", pFName, NULL };
        if( execv("/bin/xz", argv) < 0 ){ _tos_abort(); }
    }
    else if ( (0) < PID_XZ )
    {
        close(PipeFD[1]);//close write end

        _ContextReadDiagMemInfo_T CtxReadDMI = {};
        CtxReadDMI.fd = PipeFD[0];//read from xz's stdout

        if( !_mIsAllocSizeInUsePeakMode )
        {
            CtxReadDMI.Common.pCurStator = &_mDiagMIF_Stator;
        }
        else 
        {
            CtxReadDMI.Common.pCurStator = &_mDiagMIF_Stator_forAllocSizeInUsePeak;
        }
        
        __readFromFileFD_procDiagMemInfo(&CtxReadDMI);

        close(PipeFD[0]);
    }
    else 
    {
        _tos_abort();
    }
}

void __printUsage( const char *pProgName )
{
    printf("%s Usage:\n", pProgName);

    printf("Examples:\n");
    printf("\t%s -F $FName [-X] [-O] [-S 0/1/2/3]\n", pProgName);
    //printf("\t%s -F $FName -P [-X] [-O]\n", pProgName);
    //printf("\t%s -F $FName -U [-X] [-O]\n", pProgName);
    
    printf("Options:\n");
    printf("\t-F: Filename(s) to read and convert to GProfFmt.\n");
        printf("\t\t-X: Use xz to decompress file and pipe stdin.\n");
    printf("\t-O: Output to file instand of stdout, MUST after -F.\n");

    printf("More Options:\n");
    printf("\t-D: Call stack depth, default 32.\n");
    //FIXME: REMOVE printf("\t-P: Use AllocSizePeak of each call stack instand of AllocSizeTotal.\n");
    printf("\t-S: Sort by 0=AllocSizeTotal,1=AllocCountTotal,2=AllocSizeInUse<DFT>,3=AllocCountInUse.\n");
    printf("\t-V: Verbose Warning/Debug messages to stderr.\n");
    printf("\t\t-VV: print each DiagMemInfo in DiagMIF_AllocFreeStackLogs to stderr\n");
    printf("\t-C: Cut InUse ZeroZero which means AllocSizeInUse==0 && AllocCountInUse==0\n");
    printf("\t-E: Output file in Excel/CSV format, MUST after -O\n");

    exit(-1);
}


#define _FNAME_MAX 2
static int   _mFNameNum = 0;
static char* _mFNames[_FNAME_MAX] = { NULL, };
static bool _mIsPipeFromDecomprossorXZ = false;

static void __parseArgs(int argc, char *argv[])
{
    int opt = 0;
    while( (opt=getopt(argc, argv, "F:XOD:S:VCUEt")) != -1 )
    {
        switch (opt)
        {
        case 'F':
        {
            if( _mFNameNum >= _FNAME_MAX ){ _tos_abort(); }
            _mFNames[_mFNameNum] = strdup(optarg);
            _mFNameNum++;
        }break;

        case 'X':
        {
            _mIsPipeFromDecomprossorXZ = true;
        }break;

        case 'O':
        {
            if( !_mFNameNum ){ _tos_abort(); }

            char *pSuffix = strstr(_mFNames[0], TOS_DIAGMEM_FNAME_SUFFIX_LOG);
            if( NULL == pSuffix )
            {
                pSuffix = strstr(_mFNames[0], TOS_DIAGMEM_FNAME_SUFFIX_BINT);
            }

            if( NULL == pSuffix ){ _tos_abort(); }

            //-----------------------------------------------------------------
            long FNamePrefixLen = pSuffix - _mFNames[0];
            char FNamePrefix[FNamePrefixLen + 1]; 
            memset(FNamePrefix, 0, sizeof(FNamePrefix));

            strncpy(FNamePrefix, _mFNames[0], FNamePrefixLen);
            
            _mDiagMIF_Stator.Cfg.pFNamePrefix = strdup(FNamePrefix);
            if( NULL == _mDiagMIF_Stator.Cfg.pFNamePrefix ){ _tos_abort(); }
        }break;

        case 'D':
        {
            _mDiagMIF_Stator.Cfg.CallDepthMax = atoi(optarg);
            _mDiagMIF_Stator_forAllocSizeInUsePeak.Cfg.CallDepthMax = atoi(optarg);
        }break;

        case 'S':
        {
            _mDiagMIF_Stator.Cfg.SortParam = atoi(optarg);
            if( !(_mDiagMIF_Stator.Cfg.SortParam >= DiagMIF_StatSortParamMin 
                    && _mDiagMIF_Stator.Cfg.SortParam < DiagMIF_StatSortParamMax) )
            {
                _tos_abort();
            }
        }break;

        case 'V':
        {
            _mBeVerbose++;
        }break;

        case 'C':
        {
            _mDiagMIF_Stator.Cfg.IsCutInUseZeroZero = true;
        }break;

        case 'E':
        {
            if( _mDiagMIF_Stator.Cfg.pFNamePrefix )
            {
                _mDiagMIF_Stator.Cfg.IsOutputFileFmtExcelCSV = true;
            }
            else
            {
                __printUsage(argv[0]);
            }
        }break;

        case 't':
        {
            _mIsFNameWithTimestamp = false;
        }break;;

        default:
            __printUsage(argv[0]);
        }
    }
}

static void __cleanupBeforeExit(void)
{
    TOS_DiagMIF_Stat_fprintfGProfFmt(&_mDiagMIF_Stator);

    for( int Idx = 0; Idx < _mFNameNum; Idx++ )
    {
        free(_mFNames[Idx]);
    }
}

#include <signal.h>
static void __processSignal(int SigNum)
{
    printf("Signal %d received.\n", SigNum);

    __cleanupBeforeExit();
    exit(-1);
}

static void __installSignalHandler(void)
{
    struct sigaction SigAct;

    SigAct.sa_handler = __processSignal;
    SigAct.sa_flags = SA_ONSTACK;
    sigemptyset(&SigAct.sa_mask);

    sigaction(SIGINT, &SigAct, NULL);
    sigaction(SIGTERM, &SigAct, NULL);
    sigaction(SIGQUIT, &SigAct, NULL);
    sigaction(SIGSEGV, &SigAct, NULL);

}

int main( int argc, char *argv[] )
{
    _mDiagMIF_Stator.Stat.ProcStartSecond = time(NULL);

    __parseArgs(argc, argv);
    __installSignalHandler();

    if( _mFNameNum )
    {
        fprintf(stderr, "%s(Build@%s %s)\n", argv[0], __DATE__, __TIME__);
        for( int idx = 0; idx < _mFNameNum; idx++ )
        {
            fprintf(stderr, "Processing File: %s\n", _mFNames[idx]);

            if( _mIsPipeFromDecomprossorXZ )
            {
                __pipeFromDecompressorXZ_procDiagMemInfo(_mFNames[idx]);
            }
            else
            {
                _ContextMmapDiagMemInfo_T CtxMmapDMI = {};
                CtxMmapDMI.Common.pCurStator = &_mDiagMIF_Stator;
                CtxMmapDMI.pCurFName = _mFNames[idx];

                __mmapFromNormalFile_procDiagMemInfo(&CtxMmapDMI);
            }

            //SET _mIsAllocSizeInUsePeakMode, THEN reProcDiagMemInfo and output file with '_byAllocSizeInUsePeak' surffix
            _mIsAllocSizeInUsePeakMode = true;
            {
                _mDiagMIF_Stator_forAllocSizeInUsePeak.Cfg.pFNamePrefix         = _mDiagMIF_Stator.Cfg.pFNamePrefix;
                _mDiagMIF_Stator_forAllocSizeInUsePeak.Stat.AllocSizeInUsePeak  = _mDiagMIF_Stator.Stat.AllocSizeInUsePeak;

                if( _mIsPipeFromDecomprossorXZ )
                {
                    __pipeFromDecompressorXZ_procDiagMemInfo(_mFNames[idx]);
                }
                else
                {
                    _ContextMmapDiagMemInfo_T CtxMmapDMI = 
                    {
                        .pCurFName          = _mFNames[idx],
                        .Common.pCurStator  = &_mDiagMIF_Stator_forAllocSizeInUsePeak,
                    };

                    __mmapFromNormalFile_procDiagMemInfo(&CtxMmapDMI);
                }
            }
        }
    }
    else 
    {
        __printUsage(argv[0]);
    }

    //pause();
    __cleanupBeforeExit();

    if( (_mDiagMIF_Stator.Cfg.SortParam == DiagMIF_StatSortByAllocSizeInUse) && _mDiagMIF_Stator.Cfg.pFNamePrefix)
    {
        TOS_DiagMIF_Stat_fprintfGProfFmt(&_mDiagMIF_Stator_forAllocSizeInUsePeak);
    }
    return 0;
}
