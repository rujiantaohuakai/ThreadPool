# 指定最低CMake版本
cmake_minimum_required(VERSION 3.10.0)

# 项目名称与版本
project(ThreadPool VERSION 2.1.0 LANGUAGES CXX)
include(CMakePackageConfigHelpers)


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

add_library(ThreadPool::ThreadPool ALIAS ThreadPoolLib)

set_target_properties(ThreadPoolLib
    PROPERTIES
        EXPORT_NAME ThreadPool
)

target_include_directories(ThreadPoolLib
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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

# 安装线程池库、头文件和 CMake 包配置，方便其他项目通过 find_package 使用。
install(TARGETS ThreadPoolLib
    EXPORT ThreadPoolTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

install(EXPORT ThreadPoolTargets
    FILE ThreadPoolTargets.cmake
    NAMESPACE ThreadPool::
    DESTINATION lib/cmake/ThreadPool
)

configure_package_config_file(
    cmake/ThreadPoolConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolConfig.cmake
    INSTALL_DESTINATION lib/cmake/ThreadPool
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ThreadPoolConfigVersion.cmake
    DESTINATION lib/cmake/ThreadPool
)
