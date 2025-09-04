#include "data_loader.h"
#include "file_io.h"
#include "storage.h"
#include <iostream>
#include <chrono>

/**
 * 示例：使用DataLoader加载和预处理数据
 * 注意：这是一个示例实现，实际使用时需要根据具体需求实现数据加载和预处理函数
 */

// 模拟图像加载函数
std::unique_ptr<DataItem> loadImage(const std::string& path) {
    // 在实际应用中，这里会使用OpenCV或其他图像处理库来加载图像
    std::cout << "Loading image: " << path << std::endl;
    
    // 模拟图像数据（实际应用中会从文件读取）
    int width = 640;
    int height = 480;
    int channels = 3;
    auto data = std::make_unique<unsigned char[]>(width * height * channels);
    
    // 填充一些随机数据作为示例
    for (int i = 0; i < width * height * channels; ++i) {
        data[i] = static_cast<unsigned char>(rand() % 256);
    }
    
    return std::make_unique<ImageData>(width, height, channels, std::move(data));
}

// 模拟文本加载函数
std::unique_ptr<DataItem> loadText(const std::string& path) {
    // 在实际应用中，这里会使用FileIO::readTextFile来读取文本文件
    std::cout << "Loading text: " << path << std::endl;
    
    // 模拟文本数据
    std::string text = "This is a sample text content from file: " + path;
    
    return std::make_unique<TextData>(text);
}

// 模拟图像预处理函数
std::unique_ptr<DataItem> preprocessImage(std::unique_ptr<DataItem> item) {
    // 将DataItem转换为ImageData
    auto* image_data = dynamic_cast<ImageData*>(item.get());
    if (!image_data) {
        throw std::runtime_error("Expected ImageData but got different type");
    }
    
    std::cout << "Preprocessing image of size: " 
              << image_data->getWidth() << "x" 
              << image_data->getHeight() << "x" 
              << image_data->getChannels() << std::endl;
    
    // 在实际应用中，这里会进行resize、crop、normalization等操作
    // 这里简单返回原图像（实际应用中可能会返回处理后的新图像）
    return item;
}

// 模拟文本预处理函数
std::unique_ptr<DataItem> preprocessText(std::unique_ptr<DataItem> item) {
    // 将DataItem转换为TextData
    auto* text_data = dynamic_cast<TextData*>(item.get());
    if (!text_data) {
        throw std::runtime_error("Expected TextData but got different type");
    }
    
    std::cout << "Preprocessing text with length: " 
              << text_data->getText().size() << std::endl;
    
    // 在实际应用中，这里会进行tokenization、lowercase等操作
    // 这里简单返回原文本
    return item;
}

int main() {
    std::cout << "=== High-Performance Data Loader Example ===" << std::endl;
    
    // 创建模拟的数据文件路径列表
    std::vector<std::string> image_paths;
    std::vector<std::string> text_paths;
    
    // 生成一些模拟文件路径
    for (int i = 0; i < 20; ++i) {
        image_paths.push_back("image_" + std::to_string(i) + ".jpg");
        text_paths.push_back("text_" + std::to_string(i) + ".txt");
    }
    
    // 测试图像数据加载
    std::cout << "\n--- Testing Image Data Loading ---" << std::endl;
    
    // 创建带有缓存的图像数据加载器
    DataLoader image_loader(image_paths, 4, 4, 4, 20, 50); // 缓存容量设为50
    image_loader.setLoaderFunction(loadImage);
    image_loader.setProcessorFunction(preprocessImage);
    
    // 计时开始
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 第一次遍历数据（不使用缓存或缓存命中率低）
    std::cout << "First pass (without cache or with low cache hit rate):" << std::endl;
    int batch_count = 0;
    while (true) {
        auto batch = image_loader.getNextBatch();
        if (!batch) {
            break;
        }
        
        std::cout << "Processing image batch " << ++batch_count 
                  << " with " << batch->size() << " items" << std::endl;
        
        // 在这里可以使用批次数据进行训练或推理
        // ...
    }
    
    // 计时结束第一次遍历
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_first_pass = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "First pass processed " << image_paths.size() << " images in " 
              << duration_first_pass.count() << " ms" << std::endl;
    std::cout << "Current cache size: " << image_loader.getCacheSize() << " items" << std::endl;
    
    // 重置数据加载器，准备第二次遍历
    image_loader.reset();
    
    // 计时开始第二次遍历
    start_time = std::chrono::high_resolution_clock::now();
    
    // 第二次遍历数据（应该有较高的缓存命中率）
    std::cout << "\nSecond pass (with cache hits):" << std::endl;
    batch_count = 0;
    while (true) {
        auto batch = image_loader.getNextBatch();
        if (!batch) {
            break;
        }
        
        std::cout << "Processing image batch " << ++batch_count 
                  << " with " << batch->size() << " items" << std::endl;
        
        // 在这里可以使用批次数据进行训练或推理
        // ...
    }
    
    // 计时结束第二次遍历
    end_time = std::chrono::high_resolution_clock::now();
    auto duration_second_pass = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Second pass processed " << image_paths.size() << " images in " 
              << duration_second_pass.count() << " ms" << std::endl;
    std::cout << "Cache speedup: " << static_cast<double>(duration_first_pass.count()) / duration_second_pass.count() 
              << "x faster" << std::endl;
    std::cout << "Final cache size: " << image_loader.getCacheSize() << " items" << std::endl;
    
    // 清空缓存示例
    image_loader.clearCache();
    std::cout << "After clearing cache: " << image_loader.getCacheSize() << " items" << std::endl;
    
    // 测试分布式存储数据加载
    std::cout << "\n--- Testing Distributed Storage Data Loading ---" << std::endl;
    
    // 测试S3存储
    std::cout << "\nTesting S3 Storage..." << std::endl;
    
    // 创建S3存储实例
    auto s3_storage = StorageFactory::createS3Storage("my-bucket", "access-key", "secret-key", "us-east-1");
    
    // 连接到S3存储
    if (s3_storage->connect()) {
        std::cout << "Successfully connected to S3 storage" << std::endl;
        
        // 创建S3文件路径列表
        std::vector<std::string> s3_image_paths;
        for (int i = 0; i < 5; ++i) {
            s3_image_paths.push_back("s3://my-bucket/images/image_" + std::to_string(i) + ".jpg");
        }
        
        // 创建使用S3存储的数据加载器
        DataLoader s3_loader(s3_image_paths, 2, 2, 2, 10);
        s3_loader.setLoaderFunction(loadImage);
        s3_loader.setProcessorFunction(preprocessImage);
        s3_loader.setStorage(std::move(s3_storage)); // 设置使用S3存储
        
        // 使用S3加载器加载数据
        std::cout << "Loading data from S3..." << std::endl;
        int s3_batch_count = 0;
        while (true) {
            auto batch = s3_loader.getNextBatch();
            if (!batch) {
                break;
            }
            
            std::cout << "Processing S3 batch " << ++s3_batch_count 
                      << " with " << batch->size() << " items" << std::endl;
        }
    } else {
        std::cout << "Failed to connect to S3 storage" << std::endl;
    }
    
    // 测试HDFS存储
    std::cout << "\nTesting HDFS Storage..." << std::endl;
    
    // 创建HDFS存储实例
    auto hdfs_storage = StorageFactory::createHDFSStorage("hdfs-namenode", 9000);
    
    // 连接到HDFS存储
    if (hdfs_storage->connect()) {
        std::cout << "Successfully connected to HDFS" << std::endl;
        
        // 创建HDFS文件路径列表
        std::vector<std::string> hdfs_image_paths;
        for (int i = 0; i < 5; ++i) {
            hdfs_image_paths.push_back("hdfs://hdfs-namenode:9000/images/image_" + std::to_string(i) + ".jpg");
        }
        
        // 创建使用HDFS存储的数据加载器
        DataLoader hdfs_loader(hdfs_image_paths, 2, 2, 2, 10);
        hdfs_loader.setLoaderFunction(loadImage);
        hdfs_loader.setProcessorFunction(preprocessImage);
        hdfs_loader.setStorage(std::move(hdfs_storage)); // 设置使用HDFS存储
        
        // 使用HDFS加载器加载数据
        std::cout << "Loading data from HDFS..." << std::endl;
        int hdfs_batch_count = 0;
        while (true) {
            auto batch = hdfs_loader.getNextBatch();
            if (!batch) {
                break;
            }
            
            std::cout << "Processing HDFS batch " << ++hdfs_batch_count 
                      << " with " << batch->size() << " items" << std::endl;
        }
    } else {
        std::cout << "Failed to connect to HDFS" << std::endl;
    }
    
    // 测试文本数据加载
    std::cout << "\n--- Testing Text Data Loading ---" << std::endl;
    
    // 创建文本数据加载器
    DataLoader text_loader(text_paths, 5, 3, 3, 15);
    text_loader.setLoaderFunction(loadText);
    text_loader.setProcessorFunction(preprocessText);
    
    // 计时开始
    start_time = std::chrono::high_resolution_clock::now();
    
    // 获取并处理数据批次
    batch_count = 0;
    while (true) {
        auto batch = text_loader.getNextBatch();
        if (!batch) {
            break;
        }
        
        std::cout << "Processing text batch " << ++batch_count 
                  << " with " << batch->size() << " items" << std::endl;
        
        // 在这里可以使用批次数据进行训练或推理
        // ...
    }
    
    // 计时结束
    end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Processed " << text_paths.size() << " text files in " 
              << duration.count() << " ms" << std::endl;
    
    std::cout << "\n=== Example Completed ===" << std::endl;
    
    return 0;
}