tos-cutDiagMemInfo.c
#include "../Common/_ToolsCommon.h"

typedef struct {
  // MUST: Keep CommonCtx @HEAD_OF_STRUCTURE
  union {
    _TOCM_InputCtx_T ComInCtx;
    _TOCM_InputMmapFileCtx_T ComInMmapFileCtx;
    _TOCM_InputPipeXZCtx_T ComInPipeXZCtx;
  };

  time_t StartSecond, BaseSecond;
  time_t FromSecond, ToSecond;

  char OutFName[PATH_MAX];
  FILE *pFile2Write;
  bool IsOutputXZ;

  unsigned long TotalNumbers, TotalLength;
} _ToolCutDiagMemInfoCtx_T, *_ToolCutDiagMemInfoCtx_pT;

// RefDoc: README::Usage
static void __printUsage(const char *pProgName) {
  pProgName = basename((char *)pProgName);

  printf("Examples:\n");
  printf("\t%s: -F $FName -f $'YYYY.MM.DD-HH.MM.SS' -t $'YYYY.MM.DD-HH.MM.SS'\n", pProgName);
  printf("\t%s: -F $FName -f $'YYYY.MM.DD-HH.MM.SS' -d $Seconds\n", pProgName);

  printf("Options:\n");
  printf("\t-f/-t: From NYRSFM -> To NYRSFM.\n");
  printf("\t-d: From NYRSFM -> Duration of +Seconds.\n");

  _DiagMemTC_printUsage(pProgName);

  printf(
      "\tRefURL: "
      "https://yfgitlab.dahuatech.com/CleanArchitecture/iPSA-ThinkOS/Diag/MemAllocFree/-/blob/main/Src/Tools/"
      "CutDiagMemInfo/README.md\n");

  exit(-1);
}

static void __parseArgs(_ToolCutDiagMemInfoCtx_pT pCutCtx, int argc, char *argv[]) {
  int opt = -1;

  while ((opt = getopt(argc, argv, "f:t:d:F:XZBV")) != -1) {
    switch (opt) {
      case 'f': {
        // 用sscanf解析optarg，格式为‘YYYY.MM.DD-HH.MM.SS’，转换时间格式为time_t、并保持到FrameSecond变量
        struct tm TMVAL;
        if (sscanf(optarg, TOS_DIAGMEM_FNAME_SCANFTIME, &TMVAL.tm_year, &TMVAL.tm_mon, &TMVAL.tm_mday, &TMVAL.tm_hour,
                   &TMVAL.tm_min, &TMVAL.tm_sec) != 6) {
          printf("Error: Invalid time format.\n");
          __printUsage(argv[0]);
        }

        TMVAL.tm_year = TMVAL.tm_year - 1900;
        TMVAL.tm_mon--;

        pCutCtx->FromSecond = mktime(&TMVAL);
      } break;

      case 't': {
        struct tm TMVAL;
        if (sscanf(optarg, TOS_DIAGMEM_FNAME_SCANFTIME, &TMVAL.tm_year, &TMVAL.tm_mon, &TMVAL.tm_mday, &TMVAL.tm_hour,
                   &TMVAL.tm_min, &TMVAL.tm_sec) != 6) {
          printf("Error: Invalid time format.\n");
          __printUsage(argv[0]);
        }

        TMVAL.tm_year = TMVAL.tm_year - 1900;
        TMVAL.tm_mon--;

        pCutCtx->ToSecond = mktime(&TMVAL);
      } break;

      case 'd': {
        long duration = -1;
        char *endptr = NULL;
        duration = strtol(optarg, &endptr, 10);
        if (*endptr != '\0' || duration < 0) {
          printf("Error: Invalid duration.\n");
          __printUsage(argv[0]);
        }

        pCutCtx->ToSecond = pCutCtx->StartSecond + duration;
      } break;

      case 'F': {
        pCutCtx->ComInCtx.pFName = optarg;
        pCutCtx->StartSecond = TOS_DiagMIF_getTimestamp_fromFName_asStartSecond(optarg);
        if (!pCutCtx->StartSecond) {
          printf("Error: Invalid start time.\n");
          __printUsage(argv[0]);
        }
      } break;

      case 'X': {
        pCutCtx->ComInCtx.IsPipeXZ = true;
        pCutCtx->ComInCtx.IsMmapFile = false;
      } break;

      case 'Z': {
        pCutCtx->IsOutputXZ = true;
      } break;

      case 'B': {
        pCutCtx->ComInCtx.IsPipeXZ = false;
        pCutCtx->ComInCtx.IsMmapFile = true;
      } break;

      case 'V': {
        _DiagMemTC_printVersion(argv[0]);
      } break;

      default:
        __printUsage(argv[0]);
    }
  }
}

__attribute_used__ static time_t TOS_DiagMIF_getTimestamp_withStartBaseSencond(TOS_BaseDiagMemInfo_pT pDMI,
                                                                               time_t StartSecond, time_t BaseSecond) {
  time_t DeltaSencods = 0;
  if (pDMI->Head.TvSec >= BaseSecond) {
    DeltaSencods = pDMI->Head.TvSec - BaseSecond;
  } else {
    // TOS_DIAGMEM_HEAD_TVSEC_MASK OVERFLOW ONCE ONLY NOW
    DeltaSencods = BaseSecond + pDMI->Head.TvSec;
  }

  return StartSecond + DeltaSencods;
}

static void __saveCurDMI(_ToolCutDiagMemInfoCtx_pT pCutCtx, TOS_BaseDiagMemInfo_pT pCurDMI) {
  void *pMemBlock = (void *)pCurDMI;
  unsigned long MemBlockLen = sizeof(TOS_DiagMemHead_T) + pCurDMI->Head.BodyLen;

  pCutCtx->TotalNumbers++;
  pCutCtx->TotalLength += MemBlockLen;

  fwrite(pMemBlock, MemBlockLen, 1, pCutCtx->pFile2Write);
}

static void __inputProcEachDiagMemInfo_toCut(_ToolCommonCtx_pT pComCtx, void *pProcPirv) {
  _ToolCutDiagMemInfoCtx_pT pCutCtx = (_ToolCutDiagMemInfoCtx_pT)pProcPirv;
  TOS_BaseDiagMemInfo_pT pCurDMI = pComCtx->pCurDMI;

  if (!pCutCtx->BaseSecond) {
    pCutCtx->BaseSecond = pCurDMI->Head.TvSec;
  }

  time_t CurSecond = TOS_DiagMIF_getTimestamp_withStartBaseSencond(pCurDMI, pCutCtx->StartSecond, pCutCtx->BaseSecond);

  if (pCutCtx->FromSecond <= CurSecond && CurSecond <= pCutCtx->ToSecond) {
    __saveCurDMI(pCutCtx, pCurDMI);
  }
}

int main(int argc, char *argv[]) {
  static _ToolCutDiagMemInfoCtx_T CutCtx = {
      .ComInCtx =
          {
              .IsPipeXZ = true,
              .IsMmapFile = false,
          },
      //.IsOutputXZ = true,
  };

  __parseArgs(&CutCtx, argc, argv);

  if (!CutCtx.ComInCtx.pFName) {
    __printUsage(argv[0]);
  }

  if (!CutCtx.FromSecond || !CutCtx.ToSecond) {
    __printUsage(argv[0]);
  } else {
    // format output FName with input FName's prefix and From/ToSecond
    char OutFNamePfx[128] = "";

    char *pInFNameBase = basename((char *)CutCtx.ComInCtx.pFName);
    const char *pInFNameLastUnderline = strrchr(pInFNameBase, '_');
    if (!pInFNameLastUnderline) {
      printf("Error: Invalid filename(%s)\n", CutCtx.ComInCtx.pFName);
      __printUsage(argv[0]);
    } else {
      long OutFNamePfxLen = pInFNameLastUnderline - pInFNameBase;
      if (OutFNamePfxLen < sizeof(OutFNamePfx)) {
        memcpy(OutFNamePfx, pInFNameBase, OutFNamePfxLen);
      } else {
        printf("Error: FNamePfxLen(%ld)>sizeof(FNamePfx(%ld))\n", OutFNamePfxLen, sizeof(OutFNamePfx));
        __printUsage(argv[0]);
      }
    }

    char FromSecBuf[64] = "", ToSecBuf[64] = "";
    strftime(FromSecBuf, sizeof(FromSecBuf), TOS_DIAGMEM_FNAME_STRFTIME, localtime(&CutCtx.FromSecond));
    strftime(ToSecBuf, sizeof(ToSecBuf), TOS_DIAGMEM_FNAME_STRFTIME, localtime(&CutCtx.ToSecond));

    snprintf(CutCtx.OutFName, sizeof(CutCtx.OutFName), "%s_%s(~%s)%s", OutFNamePfx, FromSecBuf, ToSecBuf,
             TOS_DIAGMEM_FNAME_SUFFIX_BINT);
    // printf("OutFName=%s\n", OutFName);
  }

  //-------------------------------------------------------------------------------------------------------------------
  if (CutCtx.IsOutputXZ) {
    CutCtx.pFile2Write = _DiagMemTC_openFile_pipeFromXZ(CutCtx.OutFName);
  } else {
    CutCtx.pFile2Write = fopen(CutCtx.OutFName, "w+");
  }

  if (!CutCtx.pFile2Write) {
    _tos_abort();
  }

  //-------------------------------------------------------------------------------------------------------------------
  if (CutCtx.ComInCtx.IsPipeXZ) {
    _DiagMemTC_pipeFromXZ_procEachDiagMemInfo(&CutCtx.ComInPipeXZCtx, __inputProcEachDiagMemInfo_toCut,
                                              (void *)&CutCtx);
  }

  if (CutCtx.ComInCtx.IsMmapFile) {
    _DiagMemTC_mmapFile_procEachDiagMemInfo(&CutCtx.ComInMmapFileCtx, __inputProcEachDiagMemInfo_toCut,
                                            (void *)&CutCtx);
  }

  fclose(CutCtx.pFile2Write);

  printf("Cut(%ld-DiagMemInfos, %ld-Bytes) -> %s\n", CutCtx.TotalNumbers, CutCtx.TotalLength, CutCtx.OutFName);
  return 0;
}
readm[[_TOC_]]

# 关于（About）
* 本文档详细阐述【tos-cutDiagMemInfo】工具的用法（Usage）、示例（Examples）和原理（Internals）。
* 更多工具：[README](../README.md)

# 概述（Overview）
* 【tos-cutDiagMemInfo】工具用于【剪切】工具tos-recvDiagMemInfo保存的格式为【[DiagMIF](https://yfgitlab.dahuatech.com/CleanArchitecture/iPSA-Architecture_Definition/-/blob/master/ArchV23/Qualities/DiagMemAllocFree.md#%E6%A0%BC%E5%BC%8Fdiagmifdiag-memory-info-format)】的文件中的某个时间段内的信息，并另存为独立文件。



# 用法（Usage）
```shell
Examples:
        tos-cutDiagMemInfo: -F $FName -f $'YYYY-MM-DD HH:MM:SS' -t $'YYYY-MM-DD HH:MM:SS'
        tos-cutDiagMemInfo: -F $FName -f $'YYYY-MM-DD HH:MM:SS' -d $Seconds
Options:
        -f/-t: From NYRSFM -> To NYRSFM.
        -d: From NYRSFM -> Duration of +Seconds.
Tools Common Options: 
        -F: filename to read or write.
        -Z/-X: use xz to compress after write or decompress before read.
                -B: use binary mode when reading or writing, a.k.a DiagMIF_DiagMemInfoT.
```

# 示例（Examples）
```shell

```

# 原理（Internals）
* 采用Tools Common里的Context框架，逐个处理"DiagMIF_DiagMemInfoT"。
