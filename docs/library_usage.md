# Using ThreadPool As A Library

本文说明如何把当前 `ThreadPool` 项目作为库集成到其他 CMake 项目中。

当前推荐两种方式：

- 方式一：使用 `add_subdirectory`
- 方式二：先 `cmake --install`，再在其他项目中使用 `find_package`

## 方式一：add_subdirectory

如果你的其他项目可以直接访问 `ThreadPool` 源码目录，这是最简单的方式。

目录示例：

```text
ChatServer
├── CMakeLists.txt
├── src
│   └── main.cpp
└── third_party
    └── ThreadPool
```

其中 `third_party/ThreadPool` 放当前线程池项目。

聊天服务器项目的 `CMakeLists.txt` 示例：

```cmake
cmake_minimum_required(VERSION 3.10)

project(ChatServer LANGUAGES CXX)

add_executable(ChatServer
    src/main.cpp
)

add_subdirectory(third_party/ThreadPool)

target_link_libraries(ChatServer
    PRIVATE ThreadPool::ThreadPool
)
```

代码中直接包含头文件：

```cpp
#include "thread_pool.h"

int main() {
    ThreadPool pool(4);

    auto future = pool.enqueue([]() {
        return 42;
    });

    return future.get() == 42 ? 0 : 1;
}
```

## 方式二：install + find_package

如果你希望先把线程池安装到一个独立目录，再让其他项目查找它，可以使用这种方式。

先构建并安装 `ThreadPool`：

```bash
cmake -S . -B build
cmake --build build --config Debug
cmake --install build --config Debug --prefix D:/libs/ThreadPool
```

安装后大致结构如下：

```text
D:/libs/ThreadPool
├── include
│   └── thread_pool.h
└── lib
    ├── ThreadPoolLib.lib
    └── cmake
        └── ThreadPool
            ├── ThreadPoolConfig.cmake
            ├── ThreadPoolConfigVersion.cmake
            ├── ThreadPoolTargets.cmake
            └── ThreadPoolTargets-debug.cmake
```

其他项目的 `CMakeLists.txt` 示例：

```cmake
cmake_minimum_required(VERSION 3.10)

project(ChatServer LANGUAGES CXX)

find_package(ThreadPool CONFIG REQUIRED)

add_executable(ChatServer
    src/main.cpp
)

target_link_libraries(ChatServer
    PRIVATE ThreadPool::ThreadPool
)
```

配置其他项目时，告诉 CMake 去哪里找已安装的线程池：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:/libs/ThreadPool
cmake --build build --config Debug
```

## 在聊天服务器中的建议用法

线程池适合处理已经从网络层读取出来的业务任务，例如：

- 消息解析
- 消息广播
- 数据库写入
- 文件收发处理
- 其他耗时业务逻辑

不建议把永久阻塞的 socket 读取任务直接大量提交到线程池中。更合理的结构是：

1. 网络层负责 `accept`、`recv`、连接管理。
2. 网络层收到完整消息后，把业务处理任务提交到 `ThreadPool`。
3. 线程池完成业务处理后，再由网络层负责发送响应或广播。

## 关闭时的注意点

当线程池已经 `shutdown()` 后，继续调用 `enqueue()` 会抛出 `std::runtime_error`。

服务器关闭流程中应该避免继续提交新任务，或者显式捕获异常：

```cpp
try {
    pool.enqueue([]() {
        // task
    });
} catch (const std::runtime_error& ex) {
    // thread pool is already stopped
}
```

## 任务异常处理

任务内部抛出的异常会通过 `std::future` 传递给调用者：

```cpp
auto future = pool.enqueue([]() {
    throw std::runtime_error("task failed");
});

try {
    future.get();
} catch (const std::runtime_error& ex) {
    // handle task exception
}
```

这种异常不会直接破坏线程池本身。
