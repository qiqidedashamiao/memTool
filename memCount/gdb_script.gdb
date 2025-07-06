# 设置断点在malloc函数上，只在申请的内存大小大于256字节时执行
#break zl_malloc if $r1 > 262135
#break zl_malloc if $r1 > 1000
break zl_malloc
# 为malloc断点定义命令
commands 4
    # 只在申请的内存大小大于256字节时执行
    #if $r1 > 262136
    # 打印堆栈信息
    bt 10
    # 打印内存指针和大小
    #printf "Malloc ptr: %p, size: %lu\n", $return, $_nbytes
    # 继续执行程序
    continue
    #else
    #    # 如果内存大小不大于256字节，则直接继续执行，不打印信息
    #    continue
    #end
end

# 设置第二个断点，在函数 bar 上
#break zl_free if $r1 > 262135
break zl_free
commands 5
    # 断点触发时执行的命令集
    #print "Breakpoint 2 hit in bar!"
    #info locals  # 显示当前函数的局部变量
	#bt 10
    continue
end

continue