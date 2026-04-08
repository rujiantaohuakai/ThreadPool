# ThreadPool

## 项目简介

这是一个基于 C++17 的通用线程池命令行项目，使用 CMake 组织工程。线程池支持固定大小工作线程、线程安全任务队列、条件变量阻塞等待、任务结果异步获取以及安全关闭。

## 功能特性

- 固定大小线程池
- 构造时创建指定数量工作线程
- 使用 `std::queue` 存储任务
- 使用 `std::condition_variable` 阻塞等待任务
- 支持提交任意可调用对象和参数
- `enqueue` 返回 `std::future`
- `shutdown` 后拒绝新任务提交
- 析构函数自动关闭线程池
- 已提交任务在关闭时允许执行完毕

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
├── CURRENT_STATUS_AND_PLAN.md
└── README.md
```

## 构建方式

```bash
cmake -S . -B build
cmake --build build --config Debug
```

## 运行说明

运行 demo：

```bash
./build/Debug/ThreadPool
```

运行测试：

```bash
./build/Debug/ThreadPoolTest
```

## 核心实现思路

1. 在线程池构造时创建固定数量工作线程。
2. 工作线程在 `worker` 中循环等待条件变量。
3. `enqueue` 使用 `std::packaged_task` 封装任务，并通过 `std::future` 返回结果。
4. 任务统一包装为 `std::function<void()>` 后进入队列。
5. `shutdown` 设置停止标志、唤醒所有线程，并在任务完成后 `join` 全部工作线程。

## 测试覆盖

- 基础任务执行
- future 返回值正确性
- shutdown 后提交任务抛异常
- 大量任务提交后可正常退出

## 后续优化方向

- 增加运行状态查询接口
- 增加等待全部任务完成接口
- 增加任务数量统计
- 增加优先级任务支持
- 增加动态扩缩容能力
