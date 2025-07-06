# -*- coding: utf-8 -*-
'''
检测内存是否正在被使用，有没有被踩内存，或者踩别人的内存
'''
import os
import re
import sys
import gdb
# 添加当前目录到模块搜索路径
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# 用户自定义命令需要继承自gdb.Command类
class Memory(gdb.Command):
     # docstring里面的文本是不是很眼熟？gdb会提取该类的__doc__属性作为对应命令的文档
    """Move breakpoint
    Usage: Check if the current heap memory address is normal and if there is any out-of-bounds access before or after it."
    Example:
        (gdb) memory 0x1234567
    """
    # __init__方法是Python类的构造方法，用于初始化对象的属性
    def __init__(self):
        # 调用父类的构造方法
        super(self.__class__, self).__init__("memory", gdb.COMMAND_USER)
    
    # invoke方法是gdb.Command类的一个抽象方法，用于处理用户输入的命令
    def invoke(self, arg, from_tty):
        # 获取用户输入的参数
        args = gdb.string_to_argv(arg)
        if len(args) != 1:
            print("Usage: memory [heap address]")
            return
        # 获取用户输入的地址
        addr = args[0]
        # 获取当前用户输入的地址的前8个字节的内容，打印出在内存地址里存储的实际申请的长度
        res = gdb.execute('x/8x {}'.format(addr), to_string=True)
        print(res)

Memory()