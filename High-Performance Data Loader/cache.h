#ifndef CACHE_H
#define CACHE_H

#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>
#include <functional>
#include <memory>
#include <iostream>

/**
 * LRU (Least Recently Used) 缓存类
 * 实现了线程安全的LRU缓存策略
 * 
 * @tparam Key 缓存键的类型
 * @tparam Value 缓存值的类型
 */
template<typename Key, typename Value>
class LRUCache {
public:
    /**
     * 构造函数
     * @param capacity 缓存容量
     */
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}
    
    /**
     * 禁止拷贝构造函数
     */
    LRUCache(const LRUCache&) = delete;
    
    /**
     * 禁止赋值操作符
     */
    LRUCache& operator=(const LRUCache&) = delete;
    
    /**
     * 移动构造函数
     */
    LRUCache(LRUCache&& other) noexcept
        : capacity_(other.capacity_),
          cache_(std::move(other.cache_)),
          list_(std::move(other.list_)) {}
    
    /**
     * 移动赋值操作符
     */
    LRUCache& operator=(LRUCache&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(mutex_);
            capacity_ = other.capacity_;
            cache_ = std::move(other.cache_);
            list_ = std::move(other.list_);
        }
        return *this;
    }
    
    /**
     * 析构函数
     */
    ~LRUCache() = default;
    
    /**
     * 获取缓存中的值
     * @param key 缓存键
     * @return 缓存值，如果不存在则返回空
     */
    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        
        // 将访问的元素移动到链表头部（表示最近使用）
        list_.splice(list_.begin(), list_, it->second);
        
        return it->second->second;
    }
    
    /**
     * 插入或更新缓存
     * @param key 缓存键
     * @param value 缓存值
     */
    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        
        // 如果键已存在，更新值并移动到链表头部
        if (it != cache_.end()) {
            list_.splice(list_.begin(), list_, it->second);
            it->second->second = value;
            return;
        }
        
        // 如果缓存已满，删除最久未使用的元素（链表尾部）
        if (cache_.size() >= capacity_) {
            auto& last = list_.back();
            cache_.erase(last.first);
            list_.pop_back();
        }
        
        // 插入新元素到链表头部
        list_.emplace_front(key, value);
        cache_[key] = list_.begin();
    }
    
    /**
     * 插入或更新缓存（移动语义）
     * @param key 缓存键
     * @param value 缓存值（右值引用）
     */
    void put(const Key& key, Value&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        
        // 如果键已存在，更新值并移动到链表头部
        if (it != cache_.end()) {
            list_.splice(list_.begin(), list_, it->second);
            it->second->second = std::move(value);
            return;
        }
        
        // 如果缓存已满，删除最久未使用的元素（链表尾部）
        if (cache_.size() >= capacity_) {
            auto& last = list_.back();
            cache_.erase(last.first);
            list_.pop_back();
        }
        
        // 插入新元素到链表头部
        list_.emplace_front(key, std::move(value));
        cache_[key] = list_.begin();
    }
    
    /**
     * 检查键是否存在于缓存中
     * @param key 缓存键
     * @return 如果存在则返回true
     */
    bool contains(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.find(key) != cache_.end();
    }
    
    /**
     * 移除缓存中的元素
     * @param key 缓存键
     * @return 如果成功移除则返回true
     */
    bool remove(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return false;
        }
        
        list_.erase(it->second);
        cache_.erase(it);
        return true;
    }
    
    /**
     * 清空缓存
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        list_.clear();
    }
    
    /**
     * 获取当前缓存大小
     * @return 缓存中元素的数量
     */
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
    /**
     * 获取缓存容量
     * @return 缓存容量
     */
    size_t capacity() const {
        return capacity_;
    }
    
    /**
     * 设置缓存容量
     * @param new_capacity 新的缓存容量
     */
    void set_capacity(size_t new_capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        capacity_ = new_capacity;
        
        // 如果新容量小于当前大小，删除多余的元素
        while (cache_.size() > capacity_) {
            auto& last = list_.back();
            cache_.erase(last.first);
            list_.pop_back();
        }
    }
    
    /**
     * 尝试获取缓存中的值，如果不存在则使用提供的函数加载并缓存
     * @param key 缓存键
     * @param loader 加载函数，用于在缓存未命中时加载数据
     * @return 缓存值
     */
    Value get_or_load(const Key& key, std::function<Value(const Key&)> loader) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // 缓存命中，将元素移动到链表头部
            list_.splice(list_.begin(), list_, it->second);
            return it->second->second;
        }
        
        // 缓存未命中，使用loader加载数据
        Value value = loader(key);
        
        // 如果缓存已满，删除最久未使用的元素
        if (cache_.size() >= capacity_) {
            auto& last = list_.back();
            cache_.erase(last.first);
            list_.pop_back();
        }
        
        // 插入新元素到链表头部
        list_.emplace_front(key, value);
        cache_[key] = list_.begin();
        
        return value;
    }
    
private:
    // 缓存容量
    size_t capacity_;
    
    // 缓存数据结构：unordered_map + list 实现LRU策略
    using ListType = std::list<std::pair<Key, Value>>;
    using ListIterator = typename ListType::iterator;
    
    // 存储缓存项的链表，链表头部是最近使用的，尾部是最久未使用的
    ListType list_;
    
    // 键到链表迭代器的映射
    std::unordered_map<Key, ListIterator> cache_;
    
    // 用于线程同步的互斥锁
    mutable std::mutex mutex_;
};

#endif // CACHE_H