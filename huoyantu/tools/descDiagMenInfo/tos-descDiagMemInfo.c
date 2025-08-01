tos-descDiagMemInfo.c
#include "../Common/_ToolsCommon.h"

typedef struct
{
    // MUST: Keep CommonCtx @HEAD_OF_STRUCTURE
    union {
      _TOCM_InputCtx_T ComCtx;
      _TOCM_InputMmapFileCtx_T ComMmapFileCtx;
      _TOCM_InputPipeXZCtx_T ComPipeXZCtx;
    };

    bool IsSummaryMode;
    bool IsDetailMode;

    time_t StartTime, TvSecBase;
    time_t EndTime;

    struct
    {
        uint32_t Total;
        uint32_t AllocUVM, FreeUVM;
        uint32_t AllocPage, FreePage;
        uint32_t AllocSlab, FreeSlab;
        uint32_t IdentifyCapSrc;
    } CNT;

    struct
    {
        uint32_t InUseBytes_ofUVM;
        uint32_t PeakInUseBytes_ofUVM;
        uint32_t PeakAllocBytes_ofUVM;
    } SIZ;
} _ToolDescDiagMemInfoCtx_T, *_ToolDescDiagMemInfoCtx_pT;

// RefDoc: README::Usage
static void __printUsage(const char *pProgName)
{
    pProgName = basename((char *)pProgName);

    printf("Examples:\n");
    printf("\t%s -F $FName\n", pProgName);
    printf("\t%s -F $FName [-d]\n", pProgName);

    printf("Options:\n");
    printf("\t-d: Dump detail DiagMemInfos instand of just the summary.\n");

    _DiagMemTC_printUsage(pProgName);

    printf("RefURL: "
           "https://yfgitlab.dahuatech.com/CleanArchitecture/iPSA-ThinkOS/Diag/MemAllocFree/-/blob/main/Src/Tools/"
           "DescDiagMemInfo/README.md\n");

    exit(-1);
}

static void __parseArgs(_ToolDescDiagMemInfoCtx_pT pDescCtx, int argc, char *argv[]) {
    int opt = -1;

    while ((opt = getopt(argc, argv, "dF:XBV")) != -1)
    {
        switch (opt)
        {
        case 'd': {
          pDescCtx->IsDetailMode = true;
          pDescCtx->IsSummaryMode = false;
        }
        break;

        case 'F': {
          pDescCtx->ComCtx.pFName = optarg;
        }
        break;

        case 'X': {
          pDescCtx->ComCtx.IsPipeXZ = true;
          pDescCtx->ComCtx.IsMmapFile = false;
        }
        break;

        case 'B': {
          pDescCtx->ComCtx.IsPipeXZ = false;
          pDescCtx->ComCtx.IsMmapFile = true;
        }
        break;

        case 'V': {
            _DiagMemTC_printVersion(argv[0]);
        }
        break;

        default:
            __printUsage(argv[0]);
        }
    }

    if (!pDescCtx->ComCtx.pFName) {
        __printUsage(argv[0]);
    }
}

static void __printDescSummary(_ToolDescDiagMemInfoCtx_pT pDescCtx)
{
    printf("%s\n", pDescCtx->ComCtx.pFName);

    char StartTime[128] = "", EndTime[128] = "";
    strftime(StartTime, sizeof(StartTime), "%Y-%m-%d %H:%M:%S", localtime(&pDescCtx->StartTime));
    strftime(EndTime, sizeof(EndTime), "%Y-%m-%d %H:%M:%S", localtime(&pDescCtx->EndTime));
    printf("\tDuration: %s ~ %s\n", StartTime, EndTime);

    uint32_t DurationSeconds = pDescCtx->EndTime - pDescCtx->StartTime;
    if (!DurationSeconds)
    {
        DurationSeconds = 1; // Avoid DIV_BY_ZERO
    }

    printf("\tTotal DiagMemInfo Counts: %u<Avg=%u/S>\n", pDescCtx->CNT.Total, pDescCtx->CNT.Total / DurationSeconds);

    if (pDescCtx->CNT.AllocUVM || pDescCtx->CNT.FreeUVM)
    {
        printf("\t\tAllocUVMs(%u<Avg=%u/S>)\tFreeUVMs(%u<Avg=%u/S>)\n", pDescCtx->CNT.AllocUVM,
               pDescCtx->CNT.AllocUVM / DurationSeconds, pDescCtx->CNT.FreeUVM,
               pDescCtx->CNT.FreeUVM / DurationSeconds);
        printf("\t\tLastInUseSize: %u-Bytes\tPeakInUseSize: %u-Bytes\tPeakAllocSize: %u-Bytes\n",
               pDescCtx->SIZ.InUseBytes_ofUVM, pDescCtx->SIZ.PeakInUseBytes_ofUVM, pDescCtx->SIZ.PeakAllocBytes_ofUVM);
    }

    if (pDescCtx->CNT.AllocPage || pDescCtx->CNT.FreePage)
    {
        printf("\t\tAllocPages(%u) FreePages(%u)\n", pDescCtx->CNT.AllocPage, pDescCtx->CNT.FreePage);
    }

    if (pDescCtx->CNT.AllocSlab || pDescCtx->CNT.FreeSlab)
    {
        printf("\t\tAllocSlabs(%u) FreeSlabs(%u)\n", pDescCtx->CNT.AllocSlab, pDescCtx->CNT.FreeSlab);
    }

    //-----------------------------------------------------------------------------------------------------------------
    printf("\tOther Counts:\n");
    printf("\t\tIdentifyCapSrc: %u\n", pDescCtx->CNT.IdentifyCapSrc);

    printf("\tExtra Notes:\n");
    printf("\t\tUVM: User-Virtual-Memory, memory allocate by POSIX APIs such as "
           "malloc/calloc/realloc/mmap/...\n");
}

static void __procEachDiagMemInfo_toDescSummary(_ToolCommonCtx_pT pComCtx, void *pProcPirv)
{
    _ToolDescDiagMemInfoCtx_pT pDescCtx = (_ToolDescDiagMemInfoCtx_pT)pProcPirv;
    TOS_BaseDiagMemInfo_pT pCurDMI      = pComCtx->pCurDMI;

    //-------------------------------------------------------------------------
    if (!pDescCtx->StartTime)
    {
        pDescCtx->TvSecBase = pCurDMI->Head.TvSec;

        pDescCtx->StartTime = TOS_DiagMIF_getTimestamp_fromFName_asStartSecond(pComCtx->pFName);
        pDescCtx->EndTime   = pDescCtx->StartTime;
    }

    uint32_t DeltaTvSec = 0;
    if (pCurDMI->Head.TvSec >= pDescCtx->TvSecBase) {
      DeltaTvSec = pCurDMI->Head.TvSec - pDescCtx->TvSecBase;
    } else {
      // TOS_DIAGMEM_HEAD_TVSEC_MASK OVERFLOW
      DeltaTvSec = pDescCtx->TvSecBase + pCurDMI->Head.TvSec;
    }

    uint32_t DeltaStartEnd = pDescCtx->EndTime - pDescCtx->StartTime;
    if (DeltaStartEnd < DeltaTvSec)
    {
        pDescCtx->EndTime = pDescCtx->StartTime + DeltaTvSec;
    }

    //-------------------------------------------------------------------------
    pDescCtx->CNT.Total++;
    TOS_DiagMemBodyType_T BodyType = TOS_DiagMIF_getBodyType(pCurDMI);

    if (BodyType == TOS_DIAGMEM_TYPE_MAF_ENVRC || BodyType == TOS_DIAGMEM_TYPE_MAF_ENVRC_M64)
    {

        switch (TOS_DiagMIF_getOpCode(pCurDMI))
        {
        case TOS_DIAGMEM_MAF_DO_ALLOC_UVM: {
            pDescCtx->CNT.AllocUVM++;

            uint32_t OpSize = TOS_DiagMIF_getOpSize(pCurDMI);
            pDescCtx->SIZ.InUseBytes_ofUVM += OpSize;

            if (OpSize > pDescCtx->SIZ.PeakAllocBytes_ofUVM)
            {
                pDescCtx->SIZ.PeakAllocBytes_ofUVM = OpSize;
            }
            if (pDescCtx->SIZ.InUseBytes_ofUVM > pDescCtx->SIZ.PeakInUseBytes_ofUVM)
            {
                pDescCtx->SIZ.PeakInUseBytes_ofUVM = pDescCtx->SIZ.InUseBytes_ofUVM;
            }
        }
        break;

        case TOS_DIAGMEM_MAF_DO_FREE_UVM: {
            pDescCtx->CNT.FreeUVM++;

            uint32_t OpSize = TOS_DiagMIF_getOpSize(pCurDMI);
            if (OpSize > 0)
            {
                pDescCtx->SIZ.InUseBytes_ofUVM -= OpSize;
            }
        }
        break;

        case TOS_DIAGMEM_MAF_DO_ALLOC_PAGE: {
            pDescCtx->CNT.AllocPage++;
            // TODO(@W)
        }
        break;

        case TOS_DIAGMEM_MAF_DO_FREE_PAGE: {
            pDescCtx->CNT.FreePage++;
            // TODO(@W)
        }
        break;

        case TOS_DIAGMEM_MAF_DO_ALLOC_SLAB: {
            pDescCtx->CNT.AllocSlab++;
            // TODO(@W)
        }
        break;

        case TOS_DIAGMEM_MAF_DO_FREE_SLAB: {
            pDescCtx->CNT.FreeSlab++;
            // TODO(@W)
        }
        break;
        }
    }
    else if (BodyType == TOS_DIAGMEM_TYPE_IDENTIFY_CAPSRC)
    {
        pDescCtx->CNT.IdentifyCapSrc++;
    }
    else
    {
        fprintf(stderr, "NOT_SUPPORT: BodyType=%u\n", BodyType);
    }
}

static void __procEachDiagMemInfo_toDescDetail(_ToolCommonCtx_pT pComCtx, void *pProcPirv)
{
    TOS_DiagMIF_fprintf_AllocFreeStackLogs(stdout, pComCtx->pCurDMI);
}

int main(int argc, char *argv[])
{
    _ToolDescDiagMemInfoCtx_T DescCtx = {
        .IsSummaryMode = true,
        .IsDetailMode = false,
        .ComCtx =
            {
                .IsPipeXZ = true,
                .IsMmapFile = false,
            },
    };

    __parseArgs(&DescCtx, argc, argv);

    if (DescCtx.IsSummaryMode) {
        if (DescCtx.ComCtx.IsPipeXZ) {
            _DiagMemTC_pipeFromXZ_procEachDiagMemInfo(&DescCtx.ComPipeXZCtx, __procEachDiagMemInfo_toDescSummary,
                                                      (void *)&DescCtx);
        }

        if (DescCtx.ComCtx.IsMmapFile) {
            _DiagMemTC_mmapFile_procEachDiagMemInfo(&DescCtx.ComMmapFileCtx, __procEachDiagMemInfo_toDescSummary,
                                                    (void *)&DescCtx);
        }

        __printDescSummary(&DescCtx);
    }

    if (DescCtx.IsDetailMode) {
        if (DescCtx.ComCtx.IsPipeXZ) {
            _DiagMemTC_pipeFromXZ_procEachDiagMemInfo(&DescCtx.ComPipeXZCtx, __procEachDiagMemInfo_toDescDetail,
                                                      (void *)&DescCtx);
        }

        if (DescCtx.ComCtx.IsMmapFile) {
            _DiagMemTC_mmapFile_procEachDiagMemInfo(&DescCtx.ComMmapFileCtx, __procEachDiagMemInfo_toDescDetail,
                                                    (void *)&DescCtx);
        }
    }
    return 0;
}
