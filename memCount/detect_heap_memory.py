# -*- coding: utf-8 -*-
import json
import os
import subprocess
import sys
import threading

# 添加当前目录到模块搜索路径
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# 获取上一级目录的路径
parent_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# 将上一级目录添加到 sys.path
sys.path.insert(0, parent_dir)


from tool import run_command, run_command_nowait, set_breakpoints, start_gdb
from tool_core import start_gdb_core

# gdb_process = None
dict_addr = {}
end = ""

# 触发断点
# 假设返回结果包含类似 "Breakpoint 1, func (b=0x603010) at zilei.cpp:30" 的信息
def trigger_break(output, gdb_process):
    if 'Breakpoint' in output:
        # addr = run_command(gdb_process, 'print re_ptr')
        # print(addr)
        bt = run_command(gdb_process, 'bt')
        print(bt)
        # dict_addr[addr] = bt
        if end == 1:
            run_command_nowait(gdb_process, 'continue')
            run_command_nowait(gdb_process, 'gcore 2.core')
            # run_command_nowait(gdb_process, '0')
            # run_command_nowait(gdb_process, 'quit')
        else:
            res = run_command(gdb_process, 'continue')
            print(res)
            trigger_break(res, gdb_process)


def start_exe():
    print("start")
    gdb_process = start_gdb('gdb', 'a.out')
    print("gdb start success")

    # Set a breakpoint
    breakpoints = ['allocateAndFreeMemory']
    set_breakpoints(gdb_process, breakpoints)

    # Start the program
    res = run_command(gdb_process, 'r')
    print(res)
    trigger_break(res, gdb_process)

    # #格式化输出dict
    # format_dict = json.dumps(dict_addr)
    # print(format_dict)

def wait_end():
    end = input("请输入结束标志:1")
    
if __name__ == "__main__":
    thread_consumer = threading.Thread(target=wait_end, args=())
    thread_consumer.start()
    start_exe()

