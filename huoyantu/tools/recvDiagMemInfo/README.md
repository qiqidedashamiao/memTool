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
