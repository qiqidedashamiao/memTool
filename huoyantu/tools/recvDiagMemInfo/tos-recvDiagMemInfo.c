tos-recvDiagMemInfo.c

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



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool _mStandaloneFile = false;
static bool _mRawDiagMemInfoT = false;
static bool _mPipe2DispPushPMS = false;
static bool _mPipe2CompressorXZ = false;
static bool _mVerboseStatReceiving = false;
static bool _mIsProtoTCP = false;

typedef struct _ContextDiagMemInfo_stru
{
    unsigned char RcvBuf[TOS_DIAGMEM_UDP_PACKET_MTU_RCVD] __attribute__((aligned(16)));
    int RcvLen, BufLeftProcLen;
    struct sockaddr_in CliSockAddr;

    TOS_BaseDiagMemInfo_pT pCurDMI;

    struct 
    {
        time_t LatestStatTick;
        uint32_t DiagMemInfoCounts, UDP_PacketCounts;
        uint32_t MemAllocCounts, MemAllocSize, MemAllocSizePeak;
    } StatReceiving;

} _ContextDiagMemInfo_T, *_ContextDiagMemInfo_pT;


static void __doOutput2File_inDiagMIF_DiagMemInfoT
    ( FILE *pFile2Output, _ContextDiagMemInfo_pT pCtxDMI, void *pCtxPriv )
{
    void *pWPtr = (void*)pCtxDMI->pCurDMI;
    int WLen = sizeof(TOS_DiagMemHead_T) + pCtxDMI->pCurDMI->Head.BodyLen;

    if( (1) != fwrite( pWPtr, WLen, 1, pFile2Output) )
    {
        _tos_abort();
    }
}

typedef struct _ContextFileWriter_stru
{
    //IdentifyCapSrc = TargetIPAddr + ProgName/PID + SeqID(=1)
    pid_t PID;
    //char *pProgName;
    struct in_addr TargetIPAddr;

    time_t TickLatestWritten;//TODO: ... 

    FILE *pFile2Write;
    char *pFileName;

    struct _ContextFileWriter_stru *pNext;
} _ContextFileWriter_T, *_ContextFileWriter_pT;

static _ContextFileWriter_T _mCtxFileWriterHead = 
{
    .pFile2Write = NULL,
    .pNext = NULL,
};

static FILE* __createPipe2CompressorXZ( char *pFName )
{
    FILE *pFile = NULL;
    int PipeFD[2];
    if( pipe(PipeFD) < 0 ){ _tos_abort(); }

    pid_t PID_XZ = vfork();
    if( (0) == PID_XZ )
    {
        close(PipeFD[1]);//close write end

        dup2(PipeFD[0], STDIN_FILENO);//Read from tos-recvDiagMemInfo
        close(PipeFD[0]);

        //-----------------------------------------------------------------------------------------
        char NewXZ_FName[strlen(pFName) + 8 ];
        memset(NewXZ_FName, 0, sizeof(NewXZ_FName));
        snprintf(NewXZ_FName, sizeof(NewXZ_FName), "%s.xz", pFName);

        int fd = open(NewXZ_FName, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if( fd < 0 ){ _tos_abort(); } 

        dup2(fd, STDOUT_FILENO);//Write to ${FName}.xz
        close(fd);

        //-----------------------------------------------------------------------------------------
        char *argv[] = { "/bin/xz", "-z", "-c", "-", NULL };
        if( execv("/bin/xz", argv) < 0 ){ _tos_abort(); }
    }
    else if ( (0) < PID_XZ )
    {
        close(PipeFD[0]);//close read end

        pFile = fdopen(PipeFD[1], "w");
        if( NULL == pFile ){ _tos_abort(); }
    }
    else 
    {
        _tos_abort();
    }

    return pFile;
}

static _ContextFileWriter_pT __createStandaloneOutputFile( _ContextDiagMemInfo_pT pCtxDMI )
{
    _ContextFileWriter_pT pCtxFileWriter = NULL;
    TOS_DiagMemHead_pT pHead = &pCtxDMI->pCurDMI->Head;
    if(TOS_DIAGMEM_TYPE_IDENTIFY_CAPSRC != pHead->BodyType ){ _tos_abort(); }

    TOS_DiagMemBody_CapSrcDesc_pT pCapSrcDesc = &pCtxDMI->pCurDMI->Body.CapSrcDesc;

    pCtxFileWriter = calloc(1, sizeof(_ContextFileWriter_T));
    if( NULL == pCtxFileWriter ){ _tos_abort(); }

    pCtxFileWriter->PID = pHead->PID;
    pCtxFileWriter->TargetIPAddr = pCtxDMI->CliSockAddr.sin_addr;
    pCtxFileWriter->TickLatestWritten = time(NULL);

    pCtxFileWriter->pNext = _mCtxFileWriterHead.pNext;
    _mCtxFileWriterHead.pNext = pCtxFileWriter;

    if( _mPipe2DispPushPMS )
    {
        //TODO: pipe to tos-dispPushPMS 
        _tos_abort();
    }
    else
    {
        //fopen
        char TickNowBuf[64] = "";
        strftime(TickNowBuf, sizeof(TickNowBuf), TOS_DIAGMEM_FNAME_STRFTIME,
                 localtime(&pCtxFileWriter->TickLatestWritten));

        char FNameBuf[256] = "";
        if( _mRawDiagMemInfoT )
        {
            // snprintf(FNameBuf, sizeof(FNameBuf), "DiagMem_%s_%s_%u_%s.binT",
            //     inet_ntoa(pCtxFileWriter->TargetIPAddr), pCapSrcDesc->ProgName, pHead->PID, TickNowBuf);

            snprintf(FNameBuf, sizeof(FNameBuf), TOS_DIAGMEM_FNAME_PREFIX_FMT_PRINT TOS_DIAGMEM_FNAME_SUFFIX_BINT,
                     TOS_DIAGMEM_FNAME_PREFIX_MODID, inet_ntoa(pCtxFileWriter->TargetIPAddr), pCapSrcDesc->ProgName,
                     pHead->PID, TickNowBuf);
        }
        else
        {
            // snprintf(FNameBuf, sizeof(FNameBuf), "DiagMem_%s_%s_%u_%s.log",
            //     inet_ntoa(pCtxFileWriter->TargetIPAddr), pCapSrcDesc->ProgName, pHead->PID, TickNowBuf);

            snprintf(FNameBuf, sizeof(FNameBuf), TOS_DIAGMEM_FNAME_PREFIX_FMT_PRINT TOS_DIAGMEM_FNAME_SUFFIX_LOG,
                     TOS_DIAGMEM_FNAME_PREFIX_MODID, inet_ntoa(pCtxFileWriter->TargetIPAddr), pCapSrcDesc->ProgName,
                     pHead->PID, TickNowBuf);
        }

        if( _mPipe2CompressorXZ )
        {
            pCtxFileWriter->pFile2Write = __createPipe2CompressorXZ(FNameBuf);
        }
        else
        {
            pCtxFileWriter->pFile2Write = fopen(FNameBuf, "w");
        }

        if( NULL == pCtxFileWriter->pFile2Write ){ _tos_abort(); }
        else 
        { 
            fprintf(stderr, "New output file: %s\n", FNameBuf); 
            pCtxFileWriter->pFileName = strdup(FNameBuf);
        }
    }

    return pCtxFileWriter;
}

static _ContextFileWriter_pT __getStandaloneOutputFile( _ContextDiagMemInfo_pT pCtxDMI )
{
    _ContextFileWriter_pT pCtxFileWriter = _mCtxFileWriterHead.pNext;

    do
    {
        if( NULL == pCtxFileWriter ){ break; }

        if( (pCtxFileWriter->PID == pCtxDMI->pCurDMI->Head.PID) 
                && (pCtxFileWriter->TargetIPAddr.s_addr 
                        == pCtxDMI->CliSockAddr.sin_addr.s_addr ) )
        {
            time_t TickNow = time(NULL);

            if( abs(TickNow) - abs(pCtxFileWriter->TickLatestWritten) < 10 )
            {
                pCtxFileWriter->TickLatestWritten = TickNow;
                break;
            }
        }

        pCtxFileWriter = pCtxFileWriter->pNext;

    } while ( 0x20230828 );

    return pCtxFileWriter;
}

static void __closeAndFreeTimeoutStandaloneOutputFile( void )
{
    _ContextFileWriter_pT pPrevCtxFileWriter = &_mCtxFileWriterHead;
    _ContextFileWriter_pT pCtxFileWriter = _mCtxFileWriterHead.pNext;

    do
    {
        if( NULL == pCtxFileWriter ){ break; }

        time_t TickNow = time(NULL);

        if( abs(TickNow) - abs(pCtxFileWriter->TickLatestWritten) > 20 )
        {
            pPrevCtxFileWriter->pNext = pCtxFileWriter->pNext;

            fprintf(stderr, "Close output file: %s\n", pCtxFileWriter->pFileName);

            fclose(pCtxFileWriter->pFile2Write);
            free(pCtxFileWriter->pFileName);
            free(pCtxFileWriter);

            pCtxFileWriter = pPrevCtxFileWriter->pNext;
        }
        else
        {
            pPrevCtxFileWriter = pCtxFileWriter;
            pCtxFileWriter = pCtxFileWriter->pNext;
        }        
    } while ( 0x20230829 );
}

    return pCtxFileWriter;
}

static void __closeAndFreeTimeoutStandaloneOutputFile( void )
{
    _ContextFileWriter_pT pPrevCtxFileWriter = &_mCtxFileWriterHead;
    _ContextFileWriter_pT pCtxFileWriter = _mCtxFileWriterHead.pNext;

    do
    {
        if( NULL == pCtxFileWriter ){ break; }

        time_t TickNow = time(NULL);

        if( abs(TickNow) - abs(pCtxFileWriter->TickLatestWritten) > 20 )
        {
            pPrevCtxFileWriter->pNext = pCtxFileWriter->pNext;

            fprintf(stderr, "Close output file: %s\n", pCtxFileWriter->pFileName);

            fclose(pCtxFileWriter->pFile2Write);
            free(pCtxFileWriter->pFileName);
            free(pCtxFileWriter);

            pCtxFileWriter = pPrevCtxFileWriter->pNext;
        }
        else
        {
            pPrevCtxFileWriter = pCtxFileWriter;
            pCtxFileWriter = pCtxFileWriter->pNext;
        }        
    } while ( 0x20230829 );
}

static FILE* __getOrCreate_StandaloneOutputFile( _ContextDiagMemInfo_pT pCtxDMI )
{
    _ContextFileWriter_pT pCtxFileWriter = NULL;
    TOS_DiagMemHead_pT pHead = &pCtxDMI->pCurDMI->Head;

    if( ((1) == pHead->SeqID) && (TOS_DIAGMEM_TYPE_IDENTIFY_CAPSRC == pHead->BodyType) ) //doCreate
    {
        //fprintf(stderr, "[Debug]: New standalone output file for SeqID(%u)\n", pHead->SeqID);
        pCtxFileWriter = __createStandaloneOutputFile(pCtxDMI);
    }
    else//doGet
    {
        pCtxFileWriter = __getStandaloneOutputFile(pCtxDMI);
        if( NULL==pCtxFileWriter )
        {
            if( TOS_DIAGMEM_TYPE_IDENTIFY_CAPSRC == pHead->BodyType )
            {
                fprintf(stderr, "[Warning]: DiagMemInfo from SeqID(%u) may inconsistent!\n", pHead->SeqID);
                pCtxFileWriter = __createStandaloneOutputFile(pCtxDMI);
            }
            else
            {
                //fprintf(stderr, "[Debug]: Use /dev/null SeqID(%u)\n", pHead->SeqID);
                static _ContextFileWriter_T _mCtxFileWriterDevNull = { .pFile2Write = NULL, .pFileName = "/dev/null" };
                if( NULL==_mCtxFileWriterDevNull.pFile2Write )
                {
                    _mCtxFileWriterDevNull.pFile2Write = fopen(_mCtxFileWriterDevNull.pFileName, "w");
                }

                pCtxFileWriter = &_mCtxFileWriterDevNull;
            }
        }

    }

    //-------------------------------------------------------------------------
    if( NULL == pCtxFileWriter ){ _tos_abort(); }
    else return pCtxFileWriter->pFile2Write;
}

static void __doProcAndOutput_StandaloneFile( _ContextDiagMemInfo_pT pCtxDMI, void *pCtxPriv )
{
    FILE *pFile2Output = __getOrCreate_StandaloneOutputFile(pCtxDMI);
    if( NULL == pFile2Output ){ _tos_abort(); } 

    if( _mRawDiagMemInfoT )
    {
        __doOutput2File_inDiagMIF_DiagMemInfoT(pFile2Output, pCtxDMI, pCtxPriv);
    }
    else
    {
        TOS_DiagMIF_fprintf_AllocFreeStackLogs(pFile2Output, pCtxDMI->pCurDMI);
    }
}

static void __doProcAndOutputAll2Stdout( _ContextDiagMemInfo_pT pCtxDMI, void *pCtxPriv )
{
    TOS_DiagMIF_fprintf_AllocFreeStackLogs(stdout, pCtxDMI->pCurDMI);
}

typedef void (*_OpProcAndOutput_F)( _ContextDiagMemInfo_pT pCtxDMI, void *pCtxPriv );
static void __forEachDiagMemInfo( _ContextDiagMemInfo_pT pCtxDMI, _OpProcAndOutput_F doProcAndOutoutEachDMI, void *pCtxPriv )
{
    int RcvLen = pCtxDMI->RcvLen;
    unsigned char *pRcvBuf = &pCtxDMI->RcvBuf[0];
    //First DiagMemInfo:
    TOS_BaseDiagMemInfo_pT pDiagMemInfo = (TOS_BaseDiagMemInfo_pT)pRcvBuf;

    do
    {
        int CurOff = (unsigned char*)pDiagMemInfo - pRcvBuf;
        if( CurOff >= RcvLen ){ pCtxDMI->BufLeftProcLen = 0; break; /*END*/ }
        if( (RcvLen - CurOff) < sizeof(TOS_DiagMemHead_T) )
        {
            pCtxDMI->BufLeftProcLen = RcvLen - CurOff; 
            memcpy(pCtxDMI->RcvBuf, pCtxDMI->RcvBuf + CurOff, pCtxDMI->BufLeftProcLen);            
            break; 
        }

        TOS_DiagMemHead_pT pHead = &pDiagMemInfo->Head;
        if( TOS_DIAGMEM_MAGIC != pHead->Magic )
            { if(_mIsProtoTCP) { _tos_abort(); } else { break; } }

        int LeftValidBufLen = RcvLen - CurOff - sizeof(TOS_DiagMemHead_T);
        if( LeftValidBufLen < pHead->BodyLen )
        { 
            pCtxDMI->BufLeftProcLen = RcvLen - CurOff; 
            memcpy(pCtxDMI->RcvBuf, pCtxDMI->RcvBuf + CurOff, pCtxDMI->BufLeftProcLen);            
            break; 
        }

        //---------------------------------------------------------------------
        if( TOS_DIAGMEM_TYPE_IDENTIFY_CAPSRC == pDiagMemInfo->Head.BodyType )
        {
            if( INADDR_ANY == pDiagMemInfo->Body.CapSrcDesc.TargetIPAddr.s_addr )
            {
                //IF capture source does not set TargetIPAddr, update with received IPAddr
                pDiagMemInfo->Body.CapSrcDesc.TargetIPAddr = pCtxDMI->CliSockAddr.sin_addr;
            }
        }

        if( (TOS_DIAGMEM_TYPE_MAF_ENVRC == pDiagMemInfo->Head.BodyType)
                && (TOS_DIAGMEM_MAF_DO_ALLOC_UVM == pDiagMemInfo->Body.MAF_EnvRc.OpCode) )
        {
            pCtxDMI->StatReceiving.MemAllocCounts++;
            pCtxDMI->StatReceiving.MemAllocSize += pDiagMemInfo->Body.MAF_EnvRc.OpSize;

            if( pDiagMemInfo->Body.MAF_EnvRc.OpSize > pCtxDMI->StatReceiving.MemAllocSizePeak )
            {
                pCtxDMI->StatReceiving.MemAllocSizePeak = pDiagMemInfo->Body.MAF_EnvRc.OpSize;
            }
        }

        if( (TOS_DIAGMEM_TYPE_MAF_ENVRC_M64 == pDiagMemInfo->Head.BodyType)
                && (TOS_DIAGMEM_MAF_DO_ALLOC_UVM == pDiagMemInfo->Body.MAF_EnvRcM64.OpCode) )
        {
            pCtxDMI->StatReceiving.MemAllocCounts++;
            pCtxDMI->StatReceiving.MemAllocSize += pDiagMemInfo->Body.MAF_EnvRcM64.OpSize;

            if( pDiagMemInfo->Body.MAF_EnvRcM64.OpSize > pCtxDMI->StatReceiving.MemAllocSizePeak )
            {
                pCtxDMI->StatReceiving.MemAllocSizePeak = pDiagMemInfo->Body.MAF_EnvRcM64.OpSize;
            }
        }

        //---------------------------------------------------------------------
        pCtxDMI->pCurDMI       = pDiagMemInfo;
        pCtxDMI->StatReceiving.DiagMemInfoCounts++;
        doProcAndOutoutEachDMI(pCtxDMI, pCtxPriv);

        //---------------------------------------------------------------------
        //Next DiagMemInfo:
        pDiagMemInfo = (TOS_BaseDiagMemInfo_pT)((unsigned char*)pDiagMemInfo 
                        + sizeof(TOS_DiagMemHead_T) + pDiagMemInfo->Head.BodyLen);
    } while ( 0x20230823 );
}

static uint32_t __getTailSeqID(unsigned char *pRcvBuf, int BufLen)
{
    unsigned char *pRcvBufTail     = pRcvBuf + BufLen;
    TOS_BaseDiagMemInfo_pT pCurDMI = (TOS_BaseDiagMemInfo_pT)pRcvBuf;
    uint32_t CurSeqID              = pCurDMI->Head.SeqID;

    do
    {
        if ((unsigned char *)pCurDMI >= pRcvBufTail)
        {
            break;
        }

        TOS_BaseDiagMemInfo_pT pNextDMI =
            (TOS_BaseDiagMemInfo_pT)((unsigned char *)pCurDMI + sizeof(TOS_DiagMemHead_T) + pCurDMI->Head.BodyLen);
        if ((unsigned char *)pNextDMI > pRcvBufTail)  //=Means pCurDMI is  the last one in buffer
        {
          break;
        }

        CurSeqID = pCurDMI->Head.SeqID;
        pCurDMI  = pNextDMI;
    } while ( 0x20230925 );

    return CurSeqID;
}

static void __recvAndProcDiagMemInfo( int SrvSockFD )
{
    unsigned long RcvCounts = 0;
    _ContextDiagMemInfo_T CtxDMI = {};
    
    do
    {
        struct pollfd fds = { .fd = SrvSockFD, .events = POLLIN, };
        int RetPSX = poll(&fds, 1, 10000);
        if( RetPSX != 1 )
        {
            //fprintf(stderr, "[Debug]: poll timeout\n");
            __closeAndFreeTimeoutStandaloneOutputFile();
            if(_mIsProtoTCP){ _tos_abort(); } else { continue; }
        }

        socklen_t CliSockAddrLen = sizeof(CtxDMI.CliSockAddr);
        unsigned char RcvBuf[TOS_DIAGMEM_UDP_PACKET_MTU] __attribute__((aligned(16)));

        int RcvLen = recvfrom(SrvSockFD, RcvBuf, sizeof(RcvBuf), MSG_DONTWAIT, 
                                (struct sockaddr*)&CtxDMI.CliSockAddr, &CliSockAddrLen);
        if( RcvLen < 0 )
        { 
            if( EAGAIN == errno )
            {
                if(_mIsProtoTCP){ _tos_abort(); } else { continue; }
            }
            else
            {
                _tos_abort();
            }
        }
        else 
        { 
            ++RcvCounts;
            if( sizeof(CtxDMI.RcvBuf) - CtxDMI.BufLeftProcLen < RcvLen ){ _tos_abort(); }


            memcpy(CtxDMI.RcvBuf + CtxDMI.BufLeftProcLen, RcvBuf, RcvLen);
            CtxDMI.RcvLen  = CtxDMI.BufLeftProcLen + RcvLen;
        }

        TOS_BaseDiagMemInfo_pT pDMI_firstInCtxBuf = (TOS_BaseDiagMemInfo_pT)&CtxDMI.RcvBuf[0];
        if (TOS_DIAGMEM_MAGIC != pDMI_firstInCtxBuf->Head.Magic)
        {
            if (_mIsProtoTCP)
            {
                _tos_abort();
            }
            else
            {
                continue;
            }
        }
        else { CtxDMI.StatReceiving.UDP_PacketCounts++; }

        //-------------------------------------------------------------------------------------------------------------
        uint32_t RecivedCurSeqID = pDMI_firstInCtxBuf->Head.SeqID;

        static uint32_t ExpectNextSeqID = 0;
        if (!ExpectNextSeqID)
        {
            ExpectNextSeqID = RecivedCurSeqID;
        }
        static uint32_t RordSeqID = 0;
        static bool IsForwardReordering =
            false; /*some DMIs received too fast, such as ExpSeqID=100, but SeqID=150 recivd, set this flag expect
                      SeqID=101~149 will recived after SeqID=150, and it always as exepected.*/
        static unsigned char RordBuf[sizeof(CtxDMI.RcvBuf)];
        static int RordBufLen;

        if (ExpectNextSeqID != RecivedCurSeqID)
        {
            if (RecivedCurSeqID > ExpectNextSeqID) /*Some DMIs lost*/
            {
                uint32_t DeltaSeqID = RecivedCurSeqID - ExpectNextSeqID;

                if ((DeltaSeqID < 100 /*JUST AN EMPIRICAL MAGIC VALUE*/) && !IsForwardReordering)
                {
                    RordBufLen = CtxDMI.RcvLen;
                    RordSeqID  = RecivedCurSeqID;
                    memcpy(RordBuf, CtxDMI.RcvBuf, CtxDMI.RcvLen);

                    // fprintf(stderr, "[DEBUG]FwdRord<BUF>: ExpectNextSeqID(%u) -> RordSeqID(%u)\n",
                    //         ExpectNextSeqID, RordSeqID);

                    IsForwardReordering = true;
                    continue;//Receive next and expect its NextSeqID as expected.
                }
                else 
                {
                    if( IsForwardReordering )
                    {
                       fprintf(stderr,
                               "[WARNING]FwdRord<DISCARD>: ExpectNextSeqID(%u) -...%u...-> RordSeqID(%u) -> "
                               "RecivedCurSeqID(%u)\n",
                               ExpectNextSeqID, RordSeqID - ExpectNextSeqID, RordSeqID, RecivedCurSeqID);

                      unsigned char TmpBuf[sizeof(CtxDMI.RcvBuf)];
                      int TmpBufLen;

                      TmpBufLen = CtxDMI.RcvLen;
                      memcpy(TmpBuf, CtxDMI.RcvBuf, CtxDMI.RcvLen);

                      CtxDMI.RcvLen = RordBufLen;
                      memcpy(CtxDMI.RcvBuf, RordBuf, RordBufLen);

                      if( _mStandaloneFile == true ) 
                      {
                        __forEachDiagMemInfo(&CtxDMI, __doProcAndOutput_StandaloneFile, NULL);
                        }
                        else 
                        {
                            __forEachDiagMemInfo(&CtxDMI, __doProcAndOutputAll2Stdout, NULL);
                        }

                        CtxDMI.RcvLen = TmpBufLen;
                        memcpy(CtxDMI.RcvBuf, TmpBuf, TmpBufLen);
                    }
                    else
                    {
                        fprintf(stderr,
                                "[WARNING]FwdRord<DISCARD>: ExpectNextSeqID(%u) -...%u...-> RecivedCurSeqID(%u)\n",
                                ExpectNextSeqID, DeltaSeqID, RecivedCurSeqID);
                    }

                    IsForwardReordering = false;
                }
            }

            if (RecivedCurSeqID < ExpectNextSeqID) /*Some DMIs out-of-order*/
            {
                //Not-Support
                fprintf(stderr, "[WARNING]FwdRord<OUT-OF-ORDER>: RecivedCurSeqID(%u) < ExpectNextSeqID(%u)\n",
                        RecivedCurSeqID, ExpectNextSeqID);

                //if( IsForwardReordering ){ _tos_abort(); }
            }
        }
        else 
        {
            if( IsForwardReordering )
            {
                // fprintf(stderr, "[DEBUG]FwdRord<APPLY>: NextSeqID(%u)/Head.SeqID(%u) -> RordSeqID(%u)\n", 
                //                     NextSeqID, pDiagMemInfo->Head.SeqID, RordSeqID);

                //[1]Proc Late-Arriving 
                if( _mStandaloneFile == true )
                {
                    __forEachDiagMemInfo(&CtxDMI, __doProcAndOutput_StandaloneFile, NULL);
                }
                else 
                {
                    __forEachDiagMemInfo(&CtxDMI, __doProcAndOutputAll2Stdout, NULL);
                }

                ExpectNextSeqID = __getTailSeqID(CtxDMI.RcvBuf, CtxDMI.RcvLen) + 1;
                if ((ExpectNextSeqID < RordSeqID) && (RordSeqID - ExpectNextSeqID < 50))
                {
                    // fprintf(stderr, "[DEBUG]FwdRord<APPLY-HOLE>: Head.SeqID(%u) -> NextSeqID(%u) -...-> RordSeqID(%u)\n", 
                    //                     pDiagMemInfo->Head.SeqID, NextSeqID, RordSeqID);
                    continue; 
                }

                //[2]Proc Fwd-Arriving
                CtxDMI.RcvLen = RordBufLen;
                memcpy(CtxDMI.RcvBuf, RordBuf, RordBufLen);
                
                IsForwardReordering = false;
            }
        }

        ExpectNextSeqID = __getTailSeqID(CtxDMI.RcvBuf, CtxDMI.RcvLen) + 1;

        //-------------------------------------------------------------------------------------------------------------
        if( _mStandaloneFile == true )
        {
            __forEachDiagMemInfo(&CtxDMI, __doProcAndOutput_StandaloneFile, NULL);
        }
        else 
        {
            __forEachDiagMemInfo(&CtxDMI, __doProcAndOutputAll2Stdout, NULL);
        }

        if( !(RcvCounts % 10000) )
        {
            __closeAndFreeTimeoutStandaloneOutputFile();
        }

        if( _mVerboseStatReceiving && !(RcvCounts%1000) )
        {
            time_t TickNow = time(NULL);
            if( !CtxDMI.StatReceiving.LatestStatTick ){ CtxDMI.StatReceiving.LatestStatTick = TickNow; }

            // long ElapsedSeconds = (long)difftime(TickNow, CtxDMI.StatReceiving.LatestStatTick);
            unsigned long ElapsedSeconds = (unsigned long)TickNow - (unsigned long)CtxDMI.StatReceiving.LatestStatTick;
            if( ElapsedSeconds >=  100 )
            {
                fprintf(stderr,
                        "StatReceiving: %" PRIu32 "-DiagMemInfos/s(%" PRIu32 "-MemAllocs,%" PRIu32
                        "-Bytes/PeakMemSiz,%" PRIu32 "-Bytes/AvgMemSiz), %" PRIu32 "-UDP_Packets/s\n",
                        CtxDMI.StatReceiving.DiagMemInfoCounts / 100 /*every second*/,
                        CtxDMI.StatReceiving.MemAllocCounts / 100 /*every second*/,
                        CtxDMI.StatReceiving.MemAllocSizePeak,
                        CtxDMI.StatReceiving.MemAllocCounts
                            ? CtxDMI.StatReceiving.MemAllocSize / CtxDMI.StatReceiving.MemAllocCounts
                            : 0,
                        CtxDMI.StatReceiving.UDP_PacketCounts / 100 /*every second*/);

                CtxDMI.StatReceiving.LatestStatTick = TickNow; 
                CtxDMI.StatReceiving.DiagMemInfoCounts = 0;
                CtxDMI.StatReceiving.UDP_PacketCounts  = 0;
                CtxDMI.StatReceiving.MemAllocCounts    = 0;
                CtxDMI.StatReceiving.MemAllocSize      = 0;
                CtxDMI.StatReceiving.MemAllocSizePeak  = 0;
            }
        }
    } while ( 0x20230823 );
}

static struct sockaddr_in _mSrvSockAddr = 
{ 
    .sin_family      = AF_INET, 
    .sin_addr.s_addr = INADDR_ANY, 
};

static void __printUsage( char *pProgName )
{
    printf("%s Usage:\n", pProgName);

    printf("Examples:\n");
    printf("\t%s [-P $PortNum]\n", pProgName);
    printf("\t%s -S [-D $Path2Save] [-P $PortNum] [-Z]\n", pProgName);
    printf("\t%s -S -R [-D $Path2Save] [-P $PortNum] [-Z]\n", pProgName);

    printf("Options:\n");
    printf("\t-D: Path to output directory\n");
    printf("\t-P: Port number to receive UDP_Packet\n");
    printf("\t-S: Output/Save to file instand of stdout.\n");
    printf("\t\t-R: Save in DiagMIF_DiagMemInfoT but DiagMIF_AllocFreeStackLogs\n");
    printf("\t\t-Z: pipe to compressor XZ and saved using much smaller disk space.\n");

    //TODO: printf("\t\t-I: pipe to tos-dispPushPMS in DiagMIF_AllocFreeStackLogs\n");

    printf("More Options:\n");
    printf("\t-V: Verbose output DiagMemInfo<s>/100-Seconds, UDP_Packet<s>/100-Seconds\n");
    printf("\t-T: Receive TCP_Packet instand of UDP_Packet\n");

    exit(-1);
}

static void __parseArgs(int argc, char *argv[])
{
    int opt;
    while( (opt=getopt(argc, argv, "P:D:SRIZVT")) != -1 )
    {
        switch (opt)
        {
        case 'P':
        {
            _mSrvSockAddr.sin_port = htons(atoi(optarg));
        }break;

        case 'D':
        {
            if( chdir(optarg) < 0 )
            {
                printf("chdir(%s) fail, %d(%s)\n", optarg, errno, strerror(errno));
                exit(-1);
            }
        }break;

        case 'S':
        {
            _mStandaloneFile = true;
        }break; 

        case 'R':
        {
            _mRawDiagMemInfoT = true;
        }break;

        case 'I':
        {
            _mPipe2DispPushPMS = true;
        }break; 
        
        case 'Z':
        {
            _mPipe2CompressorXZ = true;
        }break;

        case 'V':
        {
            _mVerboseStatReceiving = true;
        }break;

        case 'T':
        {
            _mIsProtoTCP = true;
        }break;

        default:
            __printUsage(argv[0]);
        }
    }
}

int main(int argc, char *argv[])
{
    //Default receive port 
    _mSrvSockAddr.sin_port = htons(23925);

    __parseArgs(argc, argv);

    //-------------------------------------------------------------------------
    fprintf(stderr, "%s(Build@%s %s): Listening %s@(%s:%d)\n", argv[0], __DATE__, __TIME__,
        _mIsProtoTCP ? "TCP" : "UDP",
        inet_ntoa(_mSrvSockAddr.sin_addr), ntohs(_mSrvSockAddr.sin_port));
    if( _mStandaloneFile )
    {
        if( _mPipe2DispPushPMS )
        {
            fprintf(stderr, "\tPipe to stdin of tos-dispPushPMS in DiagMIF_AllocFreeStackLogs\n");
        }
        else if ( _mRawDiagMemInfoT )
        {
            fprintf(stderr, "\tOutput to standalone file in DiagMIF_DiagMemInfoT\n");
        }
        else
        {
            fprintf(stderr, "\tOutput to standalone file in DiagMIF_AllocFreeStackLogs\n");
        }
    }
    else
    {
        fprintf(stderr, "\tOutput to stdout in DiagMIF_AllocFreeStackLogs\n");
    }

    //-------------------------------------------------------------------------
    int SrvSockFD = -1;
    if( _mIsProtoTCP )
    {
        SrvSockFD = socket(AF_INET, SOCK_STREAM, 0);
    }
    else
    {
        SrvSockFD = socket(AF_INET, SOCK_DGRAM, 0);
    }
    
    if( SrvSockFD < 0 )
    {
        perror("socket");
        exit(-1);
    }

    int RetPSX = bind(SrvSockFD, (struct sockaddr*)&_mSrvSockAddr, sizeof(_mSrvSockAddr));
    if( RetPSX < 0 )
    {
        perror("bind");
        exit(-1);
    }

    //-------------------------------------------------------------------------
    if( _mIsProtoTCP )
    {
        RetPSX = listen(SrvSockFD, 5);
        if( RetPSX < 0 )
        {
            perror("listen");
            exit(-1);
        }

        do
        {
            int CliSockFD = -1;
            RetPSX = accept(SrvSockFD, NULL, 0);
            if( RetPSX < 0 )
            {
                perror("accept");
                exit(-1);
            }
            else
            {
                CliSockFD = RetPSX;
            }

            pid_t pid = fork();
            if( (0) == pid )//Child
            {
                __recvAndProcDiagMemInfo(CliSockFD);
            }
            else if( (0) < pid )//Parent
            {
                close(CliSockFD);
            } 
            else//Error
            {
                perror("fork");
                exit(-1);
            }
        } while ( 0x20230922 );
        
    }
    else
    {
        __recvAndProcDiagMemInfo(SrvSockFD);
    }

    return 0;
}
readme
[[_TOC_]]

# 关于（About）
* 本文档详细阐述【tos-recvDiagMemInfo】工具的用法（Usage）、示例（Examples）和原理（Internals）。
* 更多工具：[README](../README.md)

# 概述（Overview）
* 用途：从网络接收UDP_Packet(格式：Packed DiagMIF_DiagMemInfoT)，
  * 解析为格式：DiagMIF_AllocFreeStackLogs or DiagMIF_DiagMemInfoT，
    * 输出到：stdout or file（可选直接xz压缩）。
    * RefFMT：

# 用法（Usage）
```shell
 ./tos-recvDiagMemInfo -H
    ./tos-recvDiagMemInfo Usage:
    Examples:
            ./tos-recvDiagMemInfo [-P $PortNum]
            ./tos-recvDiagMemInfo -S [-D $Path2Save] [-P $PortNum] [-Z]
            ./tos-recvDiagMemInfo -S -R [-D $Path2Save] [-P $PortNum] [-Z]
    Options:
            -D: Path to output directory
            -P: Port number to receive UDP_Packet
            -S: Output/Save to file instand of stdout.
                    -R: Save in DiagMIF_DiagMemInfoT but DiagMIF_AllocFreeStackLogs
                    -Z: pipe to compressor XZ and saved using much smaller disk space.
    More Options:
            -V: Verbose output DiagMemInfo<s>/100-Seconds, UDP_Packet<s>/100-Seconds
            -T: Receive TCP_Packet instand of UDP_Packet
```


* 选项：

  * 【默认】：混合输出，将所有收到的信息，全部写出到标准输出（stdout），适合只接收单个设备、单个进程的情景。
  * 【-S/--standalone-file】：独立输出，用“TargetIPAddr+ProgName/PID”为区分边界，每个都写出到独立文件。文件命名规则：
    * DiagMem_${TargetIPAddr}\_${ProgName}\_${PID}\_${YYYY.MM.DD-hh.mm.ss}.log
    * 若打开选项【-R/--raw】，表示写出为“裸数据”，即将DiagMIF_DiagMemInfoT写出到文件。
      * 文件名在独立输出文件名后缀后面再追加字母“T”，如：DiagMAF_~.binT
    * 若打开选项【-Z/--pipe-CompressorXZ】，表示写出的独立文件，自动调用xz工具进行压缩，文件名为：DiagMAF_~[log,binT].xz。
    * 若打开选项【TODO：-I/--pipe-dispPushPMS】，表示独立文件为工具tos-dispPushPMS的标准输入，即每个独立输出，都启动一个对应的tos-dispPushPMS工具进程。
  * 【-D/--directory-output ${/Path/2/Dir}】：指定写出文件的目录，默认为程序当前目录。
  * 【-P/--port-receive ${PortNum}】：指定接收UDP_Packet的端口，默认：23925。

# 示例（Examples）
```shell
$ tos-recvDiagMemInfo -S -Z -R
tos-recvDiagMemInfo(Build@Sep  5 2023 11:37:08): Listening(0.0.0.0:23925)
        Output to standalone file in DiagMIF_DiagMemInfoT #关注格式
New output file: DiagMem_171.5.24.183_sonia_946_2023.09.05-20.55.41.binT

$ tos-recvDiagMemInfo -S -Z
tos-recvDiagMemInfo(Build@Sep  5 2023 11:37:08): Listening(0.0.0.0:23925)
        Output to standalone file in DiagMIF_AllocFreeStackLogs #关注格式
New output file: DiagMem_171.5.24.183_sonia_946_2023.09.06-08.50.39.log
```

# 原理（Internals）
* 设备采集（Capture：KSpace+USpace），传输UDP_Packets，接收后解析并转存到文件。
