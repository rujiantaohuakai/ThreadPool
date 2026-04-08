# 指定最低CMake版本
cmake_minimum_required(VERSION 3.10.0)

# 项目名称与版本
project(ThreadPool VERSION 1.0.0 LANGUAGES CXX)


#[====[ 
# 设置C++标准
# 必须指定 C++17 或以上，因为 std::invoke_result 等特性需要
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # 禁止使用编译器特有的扩展

# 针对MSVC编译器的优化建议
if(MSVC)
    # 开启最高级别警告 + 严格标准C++ + UTF-8 编码 
    add_compile_options(/W4 /permissive- /utf-8)
    # 关闭 VS 特有 “安全警告”
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()
]====]

# 生成静态库
add_library(ThreadPoolLib STATIC
    src/thread_pool.cpp
)


target_include_directories(ThreadPoolLib
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(ThreadPoolLib
    PUBLIC
        cxx_std_17
)

# 生成可执行文件
add_executable(ThreadPool 
    src/main.cpp
)

# 链接库到可执行文件
# 将 ThreadPoolLib 链接给 ThreadPool 可执行文件
target_link_libraries(ThreadPool 
    PRIVATE ThreadPoolLib
)

# 测试
add_executable(ThreadPoolTest
    test/test_thread_pool.cpp
)

target_link_libraries(ThreadPoolTest
    PRIVATE ThreadPoolLib
)


if(MSVC)
    target_compile_options(ThreadPoolLib PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(ThreadPoolLib PRIVATE _CRT_SECURE_NO_WARNINGS)

    target_compile_options(ThreadPool PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(ThreadPool PRIVATE _CRT_SECURE_NO_WARNINGS)

    target_compile_options(ThreadPoolTest PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(ThreadPoolTest PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
