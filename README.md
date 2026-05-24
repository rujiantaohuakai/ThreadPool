# ThreadPool

## 项目简介

这是一个基于 C++17 的线程池项目，使用 CMake 组织工程。

当前版本是 v2.1，实现的是一个可作为库复用的固定大小线程池，支持：

- 提交任意可调用对象和参数
- 使用 `std::future` 获取任务结果
- 安全关闭线程池
- 查询任务运行状态
- 使用 `waitAll()` 等待当前任务完成
- 可配置日志目录
- CMake 安装与 `find_package` 集成

## 功能特性

- 固定大小工作线程
- 线程安全任务队列
- `std::condition_variable` 阻塞等待任务
- `enqueue` 返回 `std::future`
- `shutdown` 后拒绝新任务提交
- 析构函数自动关闭线程池
- 已提交任务在关闭时允许执行完毕
- 提供任务统计接口：
  - 空闲线程数
  - 等待执行任务数
  - 正在执行任务数
  - 总任务数
- 提供 `waitAll()`：
  - 等待当前队列清空且没有任务仍在执行
  - 返回后线程池仍可继续使用
- 提供日志控制能力：
  - 开关日志
  - 配置日志目录
  - 默认目录为 `logs`
  - 日志失败不会影响线程池核心逻辑

## 目录结构

```text
.
├── CMakeLists.txt
├── include
│   └── thread_pool.h
├── src
│   ├── thread_pool.cpp
│   └── main.cpp
├── test
│   └── test_thread_pool.cpp
├── docs
│   └── design.md
├── backup
├── CURRENT_STATUS_AND_PLAN.md
└── README.md
```

## 构建方式

```bash
cmake -S . -B build
cmake --build build --config Debug
```

## 运行方式

运行 demo：

```bash
./build/Debug/ThreadPool
```

运行测试：

```bash
./build/Debug/ThreadPoolTest
```

## 安装方式

当前项目支持 CMake install。可以把库、头文件和 CMake 包配置安装到指定目录：

```bash
cmake -S . -B build
cmake --build build --config Debug
cmake --install build --config Debug --prefix D:/libs/ThreadPool
```

安装后其他项目可以通过 `find_package(ThreadPool CONFIG REQUIRED)` 使用。
当前导出的推荐链接目标是 `ThreadPool::ThreadPool`。

## 核心接口

```cpp
ThreadPool pool(4);

auto future = pool.enqueue([](int a, int b) {
    return a + b;
}, 20, 22);

pool.waitAll();

int result = future.get();
pool.shutdown();
```

当前主要接口包括：

- `enqueue(...)`
- `shutdown()`
- `waitAll()`
- `getIdleThreadCount()`
- `getPendingTasksCount()`
- `getActiveTasksCount()`
- `getTasksCount()`
- `stopped()`
- `setLogEnabled(bool)`
- `isLogEnabled()`
- `setLogDirectory(...)`
- `getLogDirectory()`

## 作为库使用

### add_subdirectory 方式

如果其他项目可以直接引用本项目源码，可以在其他项目的 `CMakeLists.txt` 中使用：

```cmake
add_subdirectory(third_party/ThreadPool)

target_link_libraries(YourTarget
    PRIVATE ThreadPool::ThreadPool
)
```

### find_package 方式

如果已经执行过 `cmake --install`，其他项目可以使用：

```cmake
find_package(ThreadPool CONFIG REQUIRED)

target_link_libraries(YourTarget
    PRIVATE ThreadPool::ThreadPool
)
```

配置其他项目时指定安装路径：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:/libs/ThreadPool
```

更完整的教程见 [docs/library_usage.md](docs/library_usage.md)。

## `waitAll()` 语义

`waitAll()` 的语义是：

- 等待当前队列中没有待执行任务
- 等待当前没有任务仍在工作线程中执行
- 不要求线程池关闭
- 返回后线程池仍然可以继续提交新任务

它内部使用条件变量实现，不依赖 `sleep` 轮询。

## 测试覆盖

当前测试覆盖：

- 基础任务执行
- `future` 返回值正确性
- 任务异常通过 `future` 传播
- `shutdown` 后继续提交任务抛异常
- 大量任务提交后可正常退出
- `waitAll()` 等待行为
- `waitAll()` 返回后线程池仍可继续使用
- 日志目录配置
- 日志失败降级

## 后续优化方向

- 支持优先级任务
- 支持动态扩缩容
- 支持任务取消或超时控制
- 优化日志实现性能
