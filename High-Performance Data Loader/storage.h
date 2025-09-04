#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <vector>
#include <memory>
#include <cstddef>
#include <stdexcept>
#include <functional>

/**
 * 存储接口 - 定义统一的文件访问操作，支持本地和分布式存储
 */
class Storage {
public:
    virtual ~Storage() = default;

    /**
     * 读取文件内容到内存
     * @param file_path 文件路径
     * @return 文件内容的内存缓冲区
     */
    virtual std::vector<unsigned char> readFile(const std::string& file_path) = 0;

    /**
     * 检查文件是否存在
     * @param file_path 文件路径
     * @return 如果文件存在则返回true
     */
    virtual bool fileExists(const std::string& file_path) = 0;

    /**
     * 获取文件大小
     * @param file_path 文件路径
     * @return 文件大小（字节）
     */
    virtual size_t getFileSize(const std::string& file_path) = 0;

    /**
     * 读取文本文件
     * @param file_path 文件路径
     * @return 文件内容的字符串
     */
    virtual std::string readTextFile(const std::string& file_path) = 0;

    /**
     * 列出目录下的文件
     * @param dir_path 目录路径
     * @return 文件路径列表
     */
    virtual std::vector<std::string> listFiles(const std::string& dir_path) = 0;
};

/**
 * 本地文件存储实现
 */
class LocalStorage : public Storage {
public:
    std::vector<unsigned char> readFile(const std::string& file_path) override;
    bool fileExists(const std::string& file_path) override;
    size_t getFileSize(const std::string& file_path) override;
    std::string readTextFile(const std::string& file_path) override;
    std::vector<std::string> listFiles(const std::string& dir_path) override;
};

/**
 * 分布式存储接口 - 为分布式存储系统提供额外的功能
 */
class DistributedStorage : public Storage {
public:
    /**
     * 连接到分布式存储系统
     * @return 连接是否成功
     */
    virtual bool connect() = 0;

    /**
     * 断开与分布式存储系统的连接
     */
    virtual void disconnect() = 0;

    /**
     * 检查连接状态
     * @return 是否已连接
     */
    virtual bool isConnected() const = 0;
};

/**
 * S3存储实现
 */
class S3Storage : public DistributedStorage {
public:
    /**
     * 构造函数
     * @param bucket S3存储桶名称
     * @param access_key AWS访问密钥
     * @param secret_key AWS密钥
     * @param region AWS区域
     */
    S3Storage(
        const std::string& bucket,
        const std::string& access_key = "",
        const std::string& secret_key = "",
        const std::string& region = "us-east-1"
    );

    ~S3Storage() override;
    
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    
    std::vector<unsigned char> readFile(const std::string& file_path) override;
    bool fileExists(const std::string& file_path) override;
    size_t getFileSize(const std::string& file_path) override;
    std::string readTextFile(const std::string& file_path) override;
    std::vector<std::string> listFiles(const std::string& dir_path) override;

private:
    std::string bucket_;
    std::string access_key_;
    std::string secret_key_;
    std::string region_;
    bool connected_;
    // 注意：在实际实现中，这里应该包含S3客户端库的实例
};

/**
 * HDFS存储实现
 */
class HDFSStorage : public DistributedStorage {
public:
    /**
     * 构造函数
     * @param namenode HDFS名称节点
     * @param port HDFS端口
     */
    HDFSStorage(
        const std::string& namenode = "localhost",
        int port = 9000
    );

    ~HDFSStorage() override;
    
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    
    std::vector<unsigned char> readFile(const std::string& file_path) override;
    bool fileExists(const std::string& file_path) override;
    size_t getFileSize(const std::string& file_path) override;
    std::string readTextFile(const std::string& file_path) override;
    std::vector<std::string> listFiles(const std::string& dir_path) override;

private:
    std::string namenode_;
    int port_;
    bool connected_;
    // 注意：在实际实现中，这里应该包含HDFS客户端库的实例
};

/**
 * 存储工厂类 - 创建不同类型的存储实例
 */
class StorageFactory {
public:
    /**
     * 创建本地存储实例
     * @return 本地存储实例
     */
    static std::unique_ptr<Storage> createLocalStorage();

    /**
     * 创建S3存储实例
     * @param bucket S3存储桶名称
     * @param access_key AWS访问密钥
     * @param secret_key AWS密钥
     * @param region AWS区域
     * @return S3存储实例
     */
    static std::unique_ptr<DistributedStorage> createS3Storage(
        const std::string& bucket,
        const std::string& access_key = "",
        const std::string& secret_key = "",
        const std::string& region = "us-east-1"
    );

    /**
     * 创建HDFS存储实例
     * @param namenode HDFS名称节点
     * @param port HDFS端口
     * @return HDFS存储实例
     */
    static std::unique_ptr<DistributedStorage> createHDFSStorage(
        const std::string& namenode = "localhost",
        int port = 9000
    );

    /**
     * 根据文件路径创建合适的存储实例
     * @param path 文件路径
     * @return 适合该路径的存储实例
     */
    static std::unique_ptr<Storage> createStorageForPath(const std::string& path);
};

#endif // STORAGE_H