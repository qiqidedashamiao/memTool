catch load libpreload.so
commands 1
    continue
end

#云台初始化前端点
b test_memory.cpp:297
commands 2
    #source gdb_script.gdb
end

#云台初始化断点后
b test_memory.cpp:313
commands 3
    del 4
    del 5
end

#continue
run