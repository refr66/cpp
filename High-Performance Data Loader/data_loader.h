#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include "thread_pool.h"
#include "cache.h"
#include "storage.h"
#include <vector>
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>
#include <future>
#include <optional>

/**
 * 数据项基类 - 所有可加载数据的抽象基类
 */
class DataItem {
public:
    virtual ~DataItem() = default;
};

/**
 * 图像数据项 - 用于存储图像数据
 */
class ImageData : public DataItem {
public:
    ImageData(int width, int height, int channels, std::unique_ptr<unsigned char[]> data)
        : width_(width), height_(height), channels_(channels), data_(std::move(data)) {}
    
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getChannels() const { return channels_; }
    unsigned char* getData() { return data_.get(); }
    const unsigned char* getData() const { return data_.get(); }
    
private:
    int width_;
    int height_;
    int channels_;
    std::unique_ptr<unsigned char[]> data_;
};

/**
 * 文本数据项 - 用于存储文本数据
 */
class TextData : public DataItem {
public:
    explicit TextData(std::string text) : text_(std::move(text)) {}
    
    const std::string& getText() const { return text_; }
    
private:
    std::string text_;
};

/**
 * 数据加载器类 - 实现多线程、高吞吐的数据加载和预处理
 */
class DataLoader {
public:
    /**
     * 构造函数
     * @param data_paths 数据文件路径列表
     * @param batch_size 批处理大小
     * @param num_loader_threads 数据加载线程数量
     * @param num_processor_threads 数据预处理线程数量
     * @param buffer_size 缓冲区大小
     * @param cache_capacity 缓存容量，0表示不使用缓存
     */
    DataLoader(
        const std::vector<std::string>& data_paths,
        size_t batch_size,
        size_t num_loader_threads = 4,
        size_t num_processor_threads = 4,
        size_t buffer_size = 100,
        size_t cache_capacity = 100
    ) : 
        data_paths_(data_paths),
        batch_size_(batch_size),
        current_index_(0),
        done_loading_(false),
        buffer_size_(buffer_size),
        cache_capacity_(cache_capacity),
        loader_pool_(num_loader_threads),
        processor_pool_(num_processor_threads),
        storage_(StorageFactory::createStorageForPath(data_paths.empty() ? "" : data_paths[0]))
    {
        if (cache_capacity > 0) {
            data_cache_ = std::make_unique<LRUCache<std::string, std::unique_ptr<DataItem>>>(cache_capacity);
        }
        
        // 启动数据加载过程
        startLoading();
    }
    
    /**
     * 禁止拷贝构造函数
     */
    DataLoader(const DataLoader&) = delete;
    
    /**
     * 禁止赋值操作符
     */
    DataLoader& operator=(const DataLoader&) = delete;
    
    /**
     * 禁止移动构造函数
     */
    DataLoader(DataLoader&&) = delete;
    
    /**
     * 禁止移动赋值操作符
     */
    DataLoader& operator=(DataLoader&&) = delete;
    
    /**
     * 析构函数
     */
    ~DataLoader() {
        stop();
    }
    
    /**
     * 设置数据加载函数
     * @param loader_fn 数据加载函数
     */
    void setLoaderFunction(std::function<std::unique_ptr<DataItem>(const std::string&)> loader_fn) {
        loader_fn_ = std::move(loader_fn);
    }

    /**
     * 设置存储接口
     * @param storage 存储接口实例
     */
    void setStorage(std::unique_ptr<Storage> storage) {
        storage_ = std::move(storage);
    }

    /**
     * 获取当前使用的存储接口
     * @return 存储接口指针
     */
    Storage* getStorage() {
        return storage_.get();
    }
    
    /**
     * 设置缓存容量
     * @param capacity 缓存容量，0表示不使用缓存
     */
    void setCacheCapacity(size_t capacity) {
        cache_capacity_ = capacity;
        if (capacity > 0) {
            if (!data_cache_) {
                data_cache_ = std::make_unique<LRUCache<std::string, std::unique_ptr<DataItem>>>(capacity);
            } else {
                data_cache_->set_capacity(capacity);
            }
        } else {
            data_cache_.reset();
        }
    }
    
    /**
     * 获取当前缓存大小
     * @return 缓存中元素的数量
     */
    size_t getCacheSize() {
        if (!data_cache_) {
            return 0;
        }
        return data_cache_->size();
    }
    
    /**
     * 清空缓存
     */
    void clearCache() {
        if (data_cache_) {
            data_cache_->clear();
        }
    }
    
    /**
     * 设置数据预处理函数
     * @param processor_fn 数据预处理函数
     */
    void setProcessorFunction(std::function<std::unique_ptr<DataItem>(std::unique_ptr<DataItem>)> processor_fn) {
        processor_fn_ = std::move(processor_fn);
    }
    
    /**
     * 获取下一个批次的数据
     * @return 数据批次，如果没有更多数据则返回空
     */
    std::optional<std::vector<std::unique_ptr<DataItem>>> getNextBatch() {
        std::vector<std::unique_ptr<DataItem>> batch;
        batch.reserve(batch_size_);
        
        for (size_t i = 0; i < batch_size_; ++i) {
            auto item = getNextItem();
            if (!item) {
                // 如果已经没有更多数据，但批次中已有部分数据，也返回该批次
                if (!batch.empty()) {
                    return batch;
                }
                return std::nullopt;
            }
            batch.push_back(std::move(*item));
        }
        
        return batch;
    }
    
    /**
     * 停止数据加载器
     */
    void stop() {
        done_loading_ = true;
        // 通知所有等待的线程
        loaded_condition_.notify_all();
        processed_condition_.notify_all();
    }
    
    /**
     * 重置数据加载器，重新开始加载数据
     */
    void reset() {
        stop();
        
        { // 清空缓冲区
            std::lock_guard<std::mutex> lock_loaded(loaded_mutex_);
            std::lock_guard<std::mutex> lock_processed(processed_mutex_);
            
            while (!loaded_queue_.empty()) {
                loaded_queue_.pop();
            }
            while (!processed_queue_.empty()) {
                processed_queue_.pop();
            }
        }
        
        current_index_ = 0;
        done_loading_ = false;
        
        // 重新启动加载过程
        startLoading();
    }
    
    /**
     * 获取数据总量
     * @return 数据项总数
     */
    size_t size() const {
        return data_paths_.size();
    }
    
private:
    // 数据文件路径列表
    std::vector<std::string> data_paths_;
    
    // 批处理大小
    size_t batch_size_;
    
    // 当前处理的索引
    std::atomic<size_t> current_index_;
    
    // 是否已完成加载
    std::atomic<bool> done_loading_;
    
    // 缓冲区大小
    size_t buffer_size_;
    
    // 数据加载线程池
    ThreadPool loader_pool_;
    
    // 数据预处理线程池
    ThreadPool processor_pool_;
    
    // 加载后的数据队列
    std::queue<std::unique_ptr<DataItem>> loaded_queue_;
    
    // 预处理后的数据队列
    std::queue<std::unique_ptr<DataItem>> processed_queue_;
    
    // 用于保护加载队列的互斥锁
    std::mutex loaded_mutex_;
    
    // 用于保护预处理队列的互斥锁
    std::mutex processed_mutex_;
    
    // 加载队列的条件变量
    std::condition_variable loaded_condition_;
    
    // 预处理队列的条件变量
    std::condition_variable processed_condition_;
    
    // 数据加载函数
    std::function<std::unique_ptr<DataItem>(const std::string&)> loader_fn_;
    
    // 数据预处理函数
    std::function<std::unique_ptr<DataItem>(std::unique_ptr<DataItem>)> processor_fn_;
    
    // 数据缓存
    size_t cache_capacity_;
    std::unique_ptr<LRUCache<std::string, std::unique_ptr<DataItem>>> data_cache_;
    
    // 存储接口
    std::unique_ptr<Storage> storage_;
    
    /**
     * 开始数据加载过程
     */
    void startLoading() {
        // 提交加载任务到加载线程池
        for (size_t i = 0; i < data_paths_.size(); ++i) {
            const auto& path = data_paths_[i];
            loader_pool_.enqueue([this, path]() {
                this->loadData(path);
            });
        }
        
        // 启动预处理任务
        for (size_t i = 0; i < processor_pool_.size(); ++i) {
            processor_pool_.enqueue([this]() {
                this->processData();
            });
        }
    }
    
    /**
     * 加载数据
     * @param path 数据文件路径
     */
    void loadData(const std::string& path) {
        if (!loader_fn_) {
            throw std::runtime_error("Loader function not set");
        }
        
        std::unique_ptr<DataItem> data;
        
        // 检查是否有缓存可用且数据在缓存中
        if (data_cache_) {
            auto cached_data = data_cache_->get(path);
            if (cached_data) {
                // 缓存命中，使用缓存的数据
                data = std::make_unique<DataItem>(**cached_data);
            } else {
                // 缓存未命中，加载数据并放入缓存
                data = loader_fn_(path);
                
                // 注意：这里需要创建数据的副本放入缓存，因为原始数据会被移动到队列中
                // 在实际应用中，可能需要根据数据类型实现深拷贝
                if (auto* image_data = dynamic_cast<ImageData*>(data.get())) {
                    // 为图像数据创建副本
                    auto cached_image = std::make_unique<ImageData>(
                        image_data->getWidth(),
                        image_data->getHeight(),
                        image_data->getChannels(),
                        std::make_unique<unsigned char[]>(image_data->getWidth() * image_data->getHeight() * image_data->getChannels())
                    );
                    memcpy(cached_image->getData(), image_data->getData(), 
                           cached_image->getWidth() * cached_image->getHeight() * cached_image->getChannels());
                    data_cache_->put(path, std::move(cached_image));
                } else if (auto* text_data = dynamic_cast<TextData*>(data.get())) {
                    // 为文本数据创建副本
                    auto cached_text = std::make_unique<TextData>(text_data->getText());
                    data_cache_->put(path, std::move(cached_text));
                } else {
                    // 对于未知类型，直接缓存（如果支持拷贝）
                    try {
                        data_cache_->put(path, std::make_unique<DataItem>(*data));
                    } catch (const std::exception&) {
                        // 如果拷贝失败，忽略缓存
                    }
                }
            }
        } else {
            // 没有使用缓存，直接加载数据
            data = loader_fn_(path);
        }
        
        {
            std::unique_lock<std::mutex> lock(loaded_mutex_);
            
            // 如果缓冲区已满，等待
            loaded_condition_.wait(lock, [this] {
                return this->loaded_queue_.size() < this->buffer_size_ || this->done_loading_;
            });
            
            if (!done_loading_) {
                loaded_queue_.push(std::move(data));
            }
        }
        
        // 通知预处理线程有新数据可用
        loaded_condition_.notify_one();
    }
    
    /**
     * 处理数据
     */
    void processData() {
        while (!done_loading_ || !loaded_queue_.empty()) {
            std::unique_ptr<DataItem> data;
            
            { // 获取加载的数据
                std::unique_lock<std::mutex> lock(loaded_mutex_);
                
                loaded_condition_.wait(lock, [this] {
                    return !this->loaded_queue_.empty() || this->done_loading_;
                });
                
                if (loaded_queue_.empty()) {
                    continue;
                }
                
                data = std::move(loaded_queue_.front());
                loaded_queue_.pop();
            }
            
            loaded_condition_.notify_one();
            
            // 进行数据预处理
            if (processor_fn_) {
                data = processor_fn_(std::move(data));
            }
            
            { // 将处理后的数据放入队列
                std::unique_lock<std::mutex> lock(processed_mutex_);
                
                processed_condition_.wait(lock, [this] {
                    return this->processed_queue_.size() < this->buffer_size_ || this->done_loading_;
                });
                
                if (!done_loading_) {
                    processed_queue_.push(std::move(data));
                }
            }
            
            processed_condition_.notify_one();
        }
    }
    
    /**
     * 获取下一个数据项
     * @return 下一个数据项，如果没有更多数据则返回空
     */
    std::optional<std::unique_ptr<DataItem>> getNextItem() {
        std::unique_lock<std::mutex> lock(processed_mutex_);
        
        processed_condition_.wait(lock, [this] {
            return !this->processed_queue_.empty() || this->done_loading_;
        });
        
        if (processed_queue_.empty()) {
            return std::nullopt;
        }
        
        auto item = std::move(processed_queue_.front());
        processed_queue_.pop();
        
        processed_condition_.notify_one();
        
        return item;
    }
};

#endif // DATA_LOADER_H