#include "storage.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstddef>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

// LocalStorage实现

std::vector<unsigned char> LocalStorage::readFile(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + file_path);
    }

    return buffer;
}

bool LocalStorage::fileExists(const std::string& file_path) {
    return fs::exists(file_path) && fs::is_regular_file(file_path);
}

size_t LocalStorage::getFileSize(const std::string& file_path) {
    if (!fileExists(file_path)) {
        throw std::runtime_error("File does not exist: " + file_path);
    }
    return fs::file_size(file_path);
}

std::string LocalStorage::readTextFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    return content;
}

std::vector<std::string> LocalStorage::listFiles(const std::string& dir_path) {
    std::vector<std::string> files;
    
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        throw std::runtime_error("Directory does not exist: " + dir_path);
    }
    
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (fs::is_regular_file(entry)) {
            files.push_back(entry.path().string());
        }
    }
    
    return files;
}

// S3Storage实现

S3Storage::S3Storage(
    const std::string& bucket,
    const std::string& access_key,
    const std::string& secret_key,
    const std::string& region
) : bucket_(bucket), access_key_(access_key), secret_key_(secret_key), region_(region), connected_(false) {
    // 注意：在实际实现中，这里应该初始化S3客户端库
    std::cout << "S3Storage initialized for bucket: " << bucket << " (region: " << region << ")" << std::endl;
}

S3Storage::~S3Storage() {
    if (connected_) {
        disconnect();
    }
}

bool S3Storage::connect() {
    // 注意：在实际实现中，这里应该连接到S3存储系统
    std::cout << "Connecting to S3 storage..." << std::endl;
    connected_ = true;
    std::cout << "Connected to S3 storage successfully." << std::endl;
    return true;
}

void S3Storage::disconnect() {
    // 注意：在实际实现中，这里应该断开与S3存储系统的连接
    std::cout << "Disconnecting from S3 storage..." << std::endl;
    connected_ = false;
    std::cout << "Disconnected from S3 storage." << std::endl;
}

bool S3Storage::isConnected() const {
    return connected_;
}

std::vector<unsigned char> S3Storage::readFile(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to S3 storage");
    }

    // 注意：在实际实现中，这里应该使用S3客户端库读取文件
    std::cout << "Reading file from S3: " << file_path << " in bucket: " << bucket_ << std::endl;
    
    // 示例实现：返回空数据
    // 在实际应用中，应该使用如AWS SDK for C++等库来读取S3文件
    return {};
}

bool S3Storage::fileExists(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to S3 storage");
    }

    // 注意：在实际实现中，这里应该使用S3客户端库检查文件是否存在
    std::cout << "Checking if file exists in S3: " << file_path << " in bucket: " << bucket_ << std::endl;
    
    // 示例实现：假设文件存在
    return true;
}

size_t S3Storage::getFileSize(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to S3 storage");
    }

    // 注意：在实际实现中，这里应该使用S3客户端库获取文件大小
    std::cout << "Getting file size from S3: " << file_path << " in bucket: " << bucket_ << std::endl;
    
    // 示例实现：返回0
    return 0;
}

std::string S3Storage::readTextFile(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to S3 storage");
    }

    // 注意：在实际实现中，这里应该使用S3客户端库读取文本文件
    std::cout << "Reading text file from S3: " << file_path << " in bucket: " << bucket_ << std::endl;
    
    // 示例实现：返回空字符串
    return "";
}

std::vector<std::string> S3Storage::listFiles(const std::string& dir_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to S3 storage");
    }

    // 注意：在实际实现中，这里应该使用S3客户端库列出目录下的文件
    std::cout << "Listing files in S3 directory: " << dir_path << " in bucket: " << bucket_ << std::endl;
    
    // 示例实现：返回空列表
    return {};
}

// HDFSStorage实现

HDFSStorage::HDFSStorage(
    const std::string& namenode,
    int port
) : namenode_(namenode), port_(port), connected_(false) {
    // 注意：在实际实现中，这里应该初始化HDFS客户端库
    std::cout << "HDFSStorage initialized for namenode: " << namenode << " (port: " << port << ")" << std::endl;
}

HDFSStorage::~HDFSStorage() {
    if (connected_) {
        disconnect();
    }
}

bool HDFSStorage::connect() {
    // 注意：在实际实现中，这里应该连接到HDFS存储系统
    std::cout << "Connecting to HDFS..." << std::endl;
    connected_ = true;
    std::cout << "Connected to HDFS successfully." << std::endl;
    return true;
}

void HDFSStorage::disconnect() {
    // 注意：在实际实现中，这里应该断开与HDFS存储系统的连接
    std::cout << "Disconnecting from HDFS..." << std::endl;
    connected_ = false;
    std::cout << "Disconnected from HDFS." << std::endl;
}

bool HDFSStorage::isConnected() const {
    return connected_;
}

std::vector<unsigned char> HDFSStorage::readFile(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to HDFS");
    }

    // 注意：在实际实现中，这里应该使用HDFS客户端库读取文件
    std::cout << "Reading file from HDFS: " << file_path << std::endl;
    
    // 示例实现：返回空数据
    return {};
}

bool HDFSStorage::fileExists(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to HDFS");
    }

    // 注意：在实际实现中，这里应该使用HDFS客户端库检查文件是否存在
    std::cout << "Checking if file exists in HDFS: " << file_path << std::endl;
    
    // 示例实现：假设文件存在
    return true;
}

size_t HDFSStorage::getFileSize(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to HDFS");
    }

    // 注意：在实际实现中，这里应该使用HDFS客户端库获取文件大小
    std::cout << "Getting file size from HDFS: " << file_path << std::endl;
    
    // 示例实现：返回0
    return 0;
}

std::string HDFSStorage::readTextFile(const std::string& file_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to HDFS");
    }

    // 注意：在实际实现中，这里应该使用HDFS客户端库读取文本文件
    std::cout << "Reading text file from HDFS: " << file_path << std::endl;
    
    // 示例实现：返回空字符串
    return "";
}

std::vector<std::string> HDFSStorage::listFiles(const std::string& dir_path) {
    if (!connected_) {
        throw std::runtime_error("Not connected to HDFS");
    }

    // 注意：在实际实现中，这里应该使用HDFS客户端库列出目录下的文件
    std::cout << "Listing files in HDFS directory: " << dir_path << std::endl;
    
    // 示例实现：返回空列表
    return {};
}

// StorageFactory实现

std::unique_ptr<Storage> StorageFactory::createLocalStorage() {
    return std::make_unique<LocalStorage>();
}

std::unique_ptr<DistributedStorage> StorageFactory::createS3Storage(
    const std::string& bucket,
    const std::string& access_key,
    const std::string& secret_key,
    const std::string& region
) {
    return std::make_unique<S3Storage>(bucket, access_key, secret_key, region);
}

std::unique_ptr<DistributedStorage> StorageFactory::createHDFSStorage(
    const std::string& namenode,
    int port
) {
    return std::make_unique<HDFSStorage>(namenode, port);
}

std::unique_ptr<Storage> StorageFactory::createStorageForPath(const std::string& path) {
    // 根据路径前缀判断使用哪种存储
    if (path.substr(0, 5) == "s3://") {
        // 解析S3路径 s3://bucket/path/to/file
        size_t bucket_start = 5;
        size_t bucket_end = path.find('/', bucket_start);
        if (bucket_end == std::string::npos) {
            bucket_end = path.length();
        }
        std::string bucket = path.substr(bucket_start, bucket_end - bucket_start);
        return createS3Storage(bucket);
    } else if (path.substr(0, 7) == "hdfs://") {
        // 解析HDFS路径 hdfs://namenode:port/path/to/file
        size_t nn_start = 7;
        size_t nn_end = path.find('/', nn_start);
        if (nn_end == std::string::npos) {
            nn_end = path.length();
        }
        std::string nn_part = path.substr(nn_start, nn_end - nn_start);
        
        std::string namenode = "localhost";
        int port = 9000;
        
        size_t port_pos = nn_part.find(':');
        if (port_pos != std::string::npos) {
            namenode = nn_part.substr(0, port_pos);
            try {
                port = std::stoi(nn_part.substr(port_pos + 1));
            } catch (...) {
                // 端口解析失败，使用默认端口
            }
        } else {
            namenode = nn_part;
        }
        
        return createHDFSStorage(namenode, port);
    } else {
        // 默认使用本地存储
        return createLocalStorage();
    }
}