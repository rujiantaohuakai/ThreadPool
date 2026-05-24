# ThreadPool Design

## 项目目标

本项目实现一个基于 C++17 的固定大小线程池，目标是提供一个可复用、线程安全、易于理解的并发基础组件。

当前版本不再只是 v1 的基础线程池，而是已经补上了状态查询、等待能力、日志控制和 CMake 库集成能力，适合作为后续继续扩展的基础版本。

## 当前工程结构

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
└── docs
    ├── design.md
    └── library_usage.md
```

## 核心类设计

线程池核心类为 `ThreadPool`，职责包括：

- 创建并管理固定数量工作线程
- 维护待执行任务队列
- 负责线程同步与关闭流程
- 提供任务提交、状态查询和等待接口
- 提供可选的日志记录能力

核心成员可以分为四组：

### 线程与任务

- `workers`
  保存所有工作线程
- `taskque`
  保存待执行任务，类型为 `std::queue<std::function<void()>>`

### 同步原语

- `mtx`
  保护任务队列、停止标志和任务统计数据
- `cv`
  工作线程等待任务时使用
- `doneCv`
  `waitAll()` 等待“所有任务完成”时使用

### 运行状态

- `isstop`
  表示线程池是否停止
- `idleThreadCount`
  当前空闲线程数
- `pendingTasks`
  当前还在队列中等待执行的任务数
- `activeTasks`
  当前正在执行的任务数

### 日志配置

- `logEnabled`
  是否开启日志
- `logDirectory`
  日志目录，默认值为 `logs`
- `logMtx`
  保护日志配置与日志写入

## 线程模型

线程池在构造时接收线程数 `threadNums`：

1. 校验线程数必须大于 0
2. 创建固定数量工作线程
3. 每个工作线程启动后进入 `worker()` 循环

每个工作线程的逻辑如下：

1. 加锁
2. 使用 `cv.wait(lock, predicate)` 等待：
   - 线程池停止，或
   - 任务队列非空
3. 如果线程池已停止且任务队列为空，则退出线程
4. 从队列中取出一个任务
5. 更新统计数据：
   - `idleThreadCount--`
   - `pendingTasks--`
   - `activeTasks++`
6. 释放锁并执行任务
7. 任务执行结束后重新加锁，恢复统计：
   - `activeTasks--`
   - `idleThreadCount++`
8. 如果此时队列为空且没有任务仍在执行，则通知 `doneCv`

这种模型保证：

- 无任务时线程不会忙等
- 执行任务时不会长期持有任务锁
- 多个工作线程可以并发执行不同任务

## 任务提交设计

任务提交接口 `enqueue` 是模板函数，因此放在头文件中实现。

任务提交流程如下：

1. 用 `std::invoke_result` 推导返回值类型
2. 用 `std::packaged_task` 封装用户任务
3. 获取对应的 `std::future`
4. 将任务包装成 `std::function<void()>`
5. 加锁后放入 `taskque`
6. 更新 `pendingTasks`
7. `notify_one()` 唤醒一个工作线程
8. 返回 `future`

这样设计有两个好处：

- 工作线程只执行统一类型的任务，不关心返回值类型
- 调用方可以通过 `future.get()` 获取结果或异常

## `waitAll()` 设计

`waitAll()` 是当前 v2 最重要的增强能力之一。

语义定义为：

- 等待当前线程池内部“没有排队任务，且没有任务仍在执行”
- 不关闭线程池
- 返回后线程池仍可继续接收新任务

实现方式：

1. 加锁
2. 调用 `doneCv.wait(lock, predicate)`
3. 谓词为：

```cpp
taskque.empty() && activeTasks == 0
```

这样设计的关键点是：

- 不能只看 `taskque.empty()`，因为任务可能已经被 worker 取走但尚未执行完
- 不能用 `sleep` 轮询，必须用条件变量等待
- 每次任务执行结束都要检查是否满足“全部完成”，若满足则通知 `doneCv`

## 关闭策略

关闭接口为 `shutdown()`，析构函数中也会调用它。

关闭逻辑为：

1. 加锁检查 `isstop`
2. 如果已经停止，直接返回
3. 否则将 `isstop` 设为 `true`
4. `cv.notify_all()` 唤醒所有工作线程
5. `doneCv.notify_all()` 唤醒可能正在等待的 `waitAll()`
6. 对所有可联结线程执行 `join`

这种策略属于优雅关闭：

- 不再接收新任务
- 已经提交的任务仍会继续执行
- 所有线程结束后再返回

## 线程安全策略

共享状态主要分两类：

- 任务调度状态：`taskque`、`isstop`、`idleThreadCount`、`pendingTasks`、`activeTasks`
- 日志状态：`logEnabled`、`logDirectory`

线程安全策略如下：

- 任务相关状态统一由 `mtx` 保护
- 日志相关状态由 `logMtx` 保护
- 条件变量始终和对应状态锁配合使用
- 执行任务时释放 `mtx`，避免把线程池退化为串行

## 日志设计

当前日志是一个可选能力，不是线程池主流程的一部分。

接口包括：

- `setLogEnabled(bool)`
- `isLogEnabled()`
- `setLogDirectory(const std::filesystem::path&)`
- `getLogDirectory()`

设计约束：

- 默认日志目录为 `logs`
- 日志写入采用追加模式
- 如果目录创建失败、文件打开失败或文件系统异常，日志直接降级为“不写”
- 日志失败不能影响 `enqueue`、`waitAll`、`shutdown` 等核心流程

这使日志能力成为 best-effort，而不是故障传播源。

## CMake 库集成设计

当前工程将线程池核心实现构建为静态库 `ThreadPoolLib`，并额外提供别名目标：

```cmake
ThreadPool::ThreadPool
```

这样其他项目可以通过两种方式使用：

1. `add_subdirectory`
2. `find_package(ThreadPool CONFIG REQUIRED)`

安装规则会导出：

- 静态库文件
- `include/thread_pool.h`
- `ThreadPoolConfig.cmake`
- `ThreadPoolConfigVersion.cmake`
- `ThreadPoolTargets.cmake`

对外推荐链接目标统一使用：

```cmake
target_link_libraries(YourTarget
    PRIVATE ThreadPool::ThreadPool
)
```

为了保证头文件在外部项目中独立可用，公共头文件需要显式包含模板实现所依赖的标准库头文件，并在模板内部使用 `this->log(...)` 访问成员函数，避免未限定名称在外部编译环境中被错误解析。

## 异常处理策略

当前异常策略如下：

- `threadNums == 0` 时，构造函数抛出 `std::invalid_argument`
- 在线程池已停止后调用 `enqueue`，抛出 `std::runtime_error`
- 用户任务内部抛出的异常由 `std::packaged_task` 传递给 `future`
- 日志相关异常在 `log()` 内部被吞掉，不向外传播

当前测试已经验证任务异常会通过 `future.get()` 重新抛给调用者，并且任务抛异常后线程池仍然可以继续执行后续任务。

## Demo 与 Test 的职责

- `src/main.cpp`
  用于展示线程池的基本使用方式
- `test/test_thread_pool.cpp`
  用于验证线程池行为正确性

当前测试覆盖：

- 基础任务执行
- `future` 返回值
- `shutdown` 后拒绝提交
- 任务异常通过 `future` 传播
- 大量任务提交后正常退出
- `waitAll()` 等待行为
- `waitAll()` 返回后线程池继续可用
- 日志目录默认值和自定义配置
- 日志失败不影响线程池功能

## 后续扩展方向

- 支持优先级任务队列
- 支持动态扩缩容
- 支持任务取消、超时等待等高级能力
- 如果后续日志量变大，可再设计更高性能的日志方案
