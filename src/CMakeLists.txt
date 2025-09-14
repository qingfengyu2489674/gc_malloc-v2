# src/CMakeLists.txt

add_library(gc_malloc STATIC
    gc_malloc/CentralHeap/AlignedChunkAllocatorByMmap.cpp
    gc_malloc/CentralHeap/FreeChunkListCache.cpp
    gc_malloc/CentralHeap/CentralHeap.cpp
    gc_malloc/ThreadHeap/Bitmap.cpp
    gc_malloc/ThreadHeap/MemSubPool.cpp
)


# 告诉 CMake 这个库的头文件在哪里。
# 使用 PUBLIC 意味着，任何链接了 mylib 的目标（比如我们的测试程序）
# 也会自动获得这个头文件搜索路径。
target_include_directories(gc_malloc PUBLIC
    ../include
)