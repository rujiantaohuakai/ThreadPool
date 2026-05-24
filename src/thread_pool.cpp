#include "thread_pool.h"
#include <thread>
#include <mutex>
#include <future>
#include <queue>
#include <functional>
#include <condition_variable>
#include <type_traits>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

ThreadPool::ThreadPool(std::size_t threadNums)
    : isstop(false),
      logEnabled(false),
      logDirectory(defaultLogDirectory()),
      idleThreadCount(0),
      pendingTasks(0),
      activeTasks(0) {
    if (threadNums == 0) {
        throw std::invalid_argument("threadNums must be greater than 0");
    }
    for(std::size_t i = 0; i < threadNums; ++i){
        workers.emplace_back([this](){
            this->worker();
        });
    }
    idleThreadCount = threadNums;
    log("thread pool created with " + std::to_string(threadNums) + " worker threads.");
}

ThreadPool::~ThreadPool(){
    shutdown();
}

void ThreadPool::shutdown() {
    // 更改停止标志
    {
        std::lock_guard<std::mutex> lock(this->mtx);
        if (this->isstop) {
            return;
        }
        isstop = true;
    }
    
    log("shutdown started.");

    // 通知所有阻塞中的线程
    this->cv.notify_all();

    this->doneCv.notify_all();

    // 确保线程执行完成
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    log("shutdown finished.");
}

void ThreadPool::worker(){
    while(true){
        // 定义任务
        std::function<void()> task;

        // 从队列中获取一个任务
        {
            std::unique_lock<std::mutex> lock(this->mtx);
            // 获取锁，同时线程池停止或者任务队列不为空时继续
            cv.wait(lock, [this](){ return this->isstop || !this->taskque.empty(); });
            if(this->isstop && this->taskque.empty()) return; // 线程池停止且任务队列为空，退出线程
            task = std::move(this->taskque.front());
            this->taskque.pop();
            --this->idleThreadCount;
            --this->pendingTasks;
            ++this->activeTasks;
        }

        {
            std::ostringstream oss;
            oss << "worker " << std::this_thread::get_id() << " started a task.";
            log(oss.str());
        }
        // 执行任务
        task();

        {
            std::unique_lock<std::mutex> lock(this->mtx);
            --this->activeTasks;
            ++this->idleThreadCount;
            if(this->taskque.empty() && this->activeTasks == 0){
                this->doneCv.notify_all();
            }
        }
        {
            std::ostringstream oss;
            oss << "worker " << std::this_thread::get_id() << " finished a task.";
            log(oss.str());
        }
    }
}


std::size_t ThreadPool::getIdleThreadCount() const{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->idleThreadCount;
}

std::size_t ThreadPool::getPendingTasksCount() const{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->pendingTasks;
}

std::size_t ThreadPool::getActiveTasksCount() const{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->activeTasks;
}

std::size_t ThreadPool::getTasksCount() const{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->activeTasks + this->pendingTasks;
}

bool ThreadPool::stopped() const{
    std::lock_guard<std::mutex> lock(this->mtx);
    return this->isstop;
}

void ThreadPool::setLogEnabled(bool enabled){
    std::lock_guard<std::mutex> lock(this->logMtx);
    this->logEnabled = enabled;
}

bool ThreadPool::isLogEnabled() const{
    std::lock_guard<std::mutex> lock(this->logMtx);
    return this->logEnabled;
}

void ThreadPool::setLogDirectory(const std::filesystem::path& directory){
    std::lock_guard<std::mutex> lock(this->logMtx);
    if (directory.empty()) {
        this->logDirectory = defaultLogDirectory();
        return;
    }
    this->logDirectory = directory;
}

std::filesystem::path ThreadPool::getLogDirectory() const{
    std::lock_guard<std::mutex> lock(this->logMtx);
    return this->logDirectory;
}

void ThreadPool::waitAll(){
    log("waitAll started.");
    std::unique_lock<std::mutex> lock(this->mtx);
    // 等待直到没有排队的任务且没有任务仍在执行
    doneCv.wait(lock, [this](){ return this->taskque.empty() && this->activeTasks == 0; });
    lock.unlock();
    log("waitAll finished.");
}

void ThreadPool::log(const std::string& message){
    std::lock_guard<std::mutex> lock(this->logMtx);
    if (!this->logEnabled) {
        return;
    }

    try {
        std::filesystem::create_directories(this->logDirectory);
        std::ofstream logFile(this->logDirectory / "thread_pool.log", std::ios::app);
        if (!logFile.is_open()) {
            return;
        }

        const auto now = std::chrono::system_clock::now();
        const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
#ifdef _WIN32
        localtime_s(&localTime, &nowTime);
#else
        localtime_r(&nowTime, &localTime);
#endif

        logFile << "[" << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << "] "
                << message << std::endl;
    } catch (const std::filesystem::filesystem_error&) {
        return;
    } catch (const std::ios_base::failure&) {
        return;
    } catch (...) {
        return;
    }
}

std::filesystem::path ThreadPool::defaultLogDirectory() {
    return "logs";
}
