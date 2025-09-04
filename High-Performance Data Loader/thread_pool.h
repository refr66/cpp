#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <stdexcept>

/**
 * 线程池类 - 使用现代C++实现的高效线程池
 * 支持任务提交、异步结果获取、动态线程管理
 */
class ThreadPool {
public:
    /**
     * 构造函数
     * @param num_threads 线程池中的线程数量，默认为硬件线程数
     */
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
        : stop_(false) {
        if (num_threads == 0) {
            num_threads = 1; // 确保至少有一个线程
        }
        
        // 创建指定数量的工作线程
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                // 工作线程函数
                while (true) {
                    std::function<void()> task;
                    
                    { // 锁作用域开始
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        
                        // 等待任务或停止信号
                        this->condition_.wait(lock, [this] {
                            return this->stop_ || !this->tasks_.empty();
                        });
                        
                        // 如果线程池停止且任务队列为空，则退出线程
                        if (this->stop_ && this->tasks_.empty()) {
                            return;
                        }
                        
                        // 从队列中获取一个任务
                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    } // 锁作用域结束
                    
                    // 执行任务
                    task();
                }
            });
        }
    }
    
    /**
     * 禁止拷贝构造函数
     */
    ThreadPool(const ThreadPool&) = delete;
    
    /**
     * 禁止赋值操作符
     */
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    /**
     * 禁止移动构造函数
     */
    ThreadPool(ThreadPool&&) = delete;
    
    /**
     * 禁止移动赋值操作符
     */
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    /**
     * 析构函数 - 停止所有工作线程
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        // 通知所有线程
        condition_.notify_all();
        
        // 等待所有线程完成
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    /**
     * 提交任务到线程池
     * @tparam F 任务函数类型
     * @tparam Args 任务函数参数类型
     * @param f 任务函数
     * @param args 任务函数参数
     * @return 任务结果的future对象
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        
        // 创建一个包装了任务的shared_ptr
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        // 获取任务的future对象
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // 如果线程池已停止，则不能添加新任务
            if (stop_) {
                throw std::runtime_error("Cannot enqueue task into stopped ThreadPool");
            }
            
            // 添加任务到队列
            tasks_.emplace([task]() {
                (*task)();
            });
        }
        
        // 通知一个线程执行任务
        condition_.notify_one();
        
        return result;
    }
    
    /**
     * 获取线程池中的线程数量
     * @return 线程数量
     */
    size_t size() const {
        return workers_.size();
    }
    
    /**
     * 获取当前任务队列的大小
     * @return 任务队列大小
     */
    size_t queue_size() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }
    
private:
    // 工作线程容器
    std::vector<std::thread> workers_;
    
    // 任务队列
    std::queue<std::function<void()>> tasks_;
    
    // 用于保护任务队列的互斥锁
    mutable std::mutex queue_mutex_;
    
    // 条件变量，用于线程同步
    std::condition_variable condition_;
    
    // 线程池停止标志
    std::atomic<bool> stop_;
};

#endif // THREAD_POOL_H