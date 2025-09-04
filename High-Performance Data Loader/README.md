# 高性能数据加载与预处理库 (High-Performance Data Loader)

这是一个使用现代C++实现的多线程、高吞吐数据加载与预处理库，专为AI训练和推理中的数据预处理流水线设计。

## 项目特点

- **多线程并行处理**：使用生产者-消费者模型实现数据加载和预处理的并行化
- **高性能文件I/O**：支持标准文件I/O和内存映射(mmap)方式读取文件
- **智能缓存机制**：实现LRU缓存策略，避免重复加载相同的数据
- **分布式存储支持**：支持从S3和HDFS等分布式存储系统加载数据
- **灵活的数据类型**：支持不同类型的数据（如图像、文本）
- **可自定义的处理流程**：允许用户定义自己的数据加载和预处理函数
- **批处理支持**：自动组织数据为批次，方便机器学习模型训练
- **现代C++特性**：充分利用C++11/14/17/20的特性，如智能指针、lambda表达式、移动语义等

## 项目结构

```
High-Performance Data Loader/
├── thread_pool.h       # 线程池实现
├── data_loader.h       # 数据加载器核心实现
├── file_io.h           # 高性能文件I/O工具
├── example.cpp         # 使用示例
├── CMakeLists.txt      # CMake构建配置（待添加）
└── README.md           # 项目文档
```

## 核心组件

### 1. ThreadPool 类

线程池是整个库的基础组件，它提供了以下功能：
- 自动管理工作线程的创建和销毁
- 任务队列管理
- 异步任务提交和结果获取
- 线程安全的任务调度

### 2. LRUCache 类

LRU (Least Recently Used) 缓存实现，提供了：
- 线程安全的缓存操作
- 自动淘汰最久未使用的缓存项
- 支持获取、插入、删除缓存项
- 支持缓存容量动态调整
- 支持get_or_load模式，缓存未命中时自动加载数据

### 3. DataLoader 类

数据加载器是库的核心组件，它实现了：
- 多线程数据加载和预处理
- 数据缓冲区管理
- 批处理功能
- 可自定义的数据加载和预处理函数
- 集成缓存机制，支持配置缓存容量和清除缓存

### 4. FileIO 类

文件I/O工具类提供了高性能的文件读取功能：
- 标准文件读取
- 内存映射(mmap)文件读取（跨平台支持）
- 文件存在性检查
- 文件大小获取
- 文本文件读取

### 5. Storage 接口及实现

存储接口提供了统一的文件访问抽象，支持不同的存储后端：

- **Storage**：抽象接口类，定义统一的文件操作方法
- **LocalStorage**：本地文件系统实现
- **DistributedStorage**：分布式存储接口基类
- **S3Storage**：Amazon S3存储实现
- **HDFSStorage**：Hadoop分布式文件系统实现
- **StorageFactory**：工厂类，用于创建适当的存储实例

这些实现支持无缝切换不同的存储后端，使数据加载器可以从本地文件系统、S3或HDFS等分布式存储系统加载数据。

### 5. DataItem 及其派生类

数据项基类及其派生类用于表示不同类型的数据：
- `DataItem`：抽象基类
- `ImageData`：图像数据类
- `TextData`：文本数据类

## 使用方法

### 1. 包含头文件

```cpp
#include "data_loader.h"
#include "file_io.h"
```

### 2. 定义数据加载和预处理函数

```cpp
// 图像加载函数
std::unique_ptr<DataItem> loadImage(const std::string& path) {
    // 实现图像加载逻辑
    // ...
    return std::make_unique<ImageData>(width, height, channels, std::move(data));
}

// 图像预处理函数
std::unique_ptr<DataItem> preprocessImage(std::unique_ptr<DataItem> item) {
    // 实现图像预处理逻辑
    // ...
    return item;
}
```

### 3. 创建数据加载器并设置处理函数

```cpp
// 创建数据文件路径列表
std::vector<std::string> image_paths = {"image1.jpg", "image2.jpg", ...};

// 创建带有缓存的数据加载器
DataLoader data_loader(
    image_paths,           // 数据文件路径
    32,                    // 批次大小
    4,                     // 加载线程数
    4,                     // 预处理线程数
    100,                   // 缓冲区大小
    200                    // 缓存容量（0表示不使用缓存）
);

// 设置加载和预处理函数
data_loader.setLoaderFunction(loadImage);
data_loader.setProcessorFunction(preprocessImage);

// 也可以在运行时设置或修改缓存容量
data_loader.setCacheCapacity(500);
```

### 4. 使用分布式存储

```cpp
// 示例1：使用S3存储
// 创建S3存储实例
auto s3_storage = StorageFactory::createS3Storage(
    "my-bucket",         // S3存储桶名称
    "access-key",        // AWS访问密钥（可选）
    "secret-key",        // AWS密钥（可选）
    "us-east-1"          // AWS区域（可选）
);

// 连接到S3存储
s3_storage->connect();

// 创建S3文件路径列表
std::vector<std::string> s3_image_paths = {"s3://my-bucket/images/img1.jpg", "s3://my-bucket/images/img2.jpg"};

// 创建数据加载器
DataLoader s3_loader(s3_image_paths, 32, 4, 4, 100);

// 设置使用S3存储
s3_loader.setStorage(std::move(s3_storage));

// 示例2：使用HDFS存储
// 创建HDFS存储实例
auto hdfs_storage = StorageFactory::createHDFSStorage(
    "hdfs-namenode",     // HDFS名称节点
    9000                  // HDFS端口
);

// 连接到HDFS
hdfs_storage->connect();

// 创建HDFS文件路径列表
std::vector<std::string> hdfs_image_paths = {"hdfs://hdfs-namenode:9000/images/img1.jpg"};

// 创建数据加载器
DataLoader hdfs_loader(hdfs_image_paths, 32, 4, 4, 100);

// 设置使用HDFS存储
hdfs_loader.setStorage(std::move(hdfs_storage));

// 示例3：自动检测存储类型
// 数据加载器会根据文件路径前缀自动选择合适的存储实现
auto auto_loader = DataLoader({
    "s3://my-bucket/data/file1.jpg",
    "hdfs://hdfs-namenode:9000/data/file2.jpg",
    "local_file.jpg"
}, 32, 4, 4, 100);
```

### 4. 获取数据批次

```cpp
// 循环获取数据批次
while (true) {
    auto batch = data_loader.getNextBatch();
    if (!batch) {
        break;  // 数据加载完成
    }
    
    // 使用批次数据进行训练或推理
    // ...
}
```

## 性能优化建议

1. **调整线程数量**：根据系统硬件和数据特性调整加载和预处理线程的数量
2. **合理设置缓冲区大小**：缓冲区太小可能导致线程等待，太大会占用过多内存
3. **使用内存映射**：对于大文件或频繁访问的文件，使用`FileIO::mmapFile`可以提高性能
4. **批量处理**：合理设置批次大小可以提高GPU利用率（在深度学习场景下）
5. **避免频繁内存分配**：在预处理函数中尽量重用内存
6. **优化缓存配置**：
   - 根据可用内存和数据规模设置适当的缓存容量
   - 对于重复访问的数据，启用缓存可以显著提高性能
   - 对于一次性访问的大规模数据集，考虑禁用缓存以节省内存
7. **监控缓存性能**：使用`getCacheSize()`方法监控缓存使用情况，根据实际效果调整缓存策略
8. **分布式存储优化**：
   - 对于分布式存储，考虑增加加载线程数量以充分利用网络带宽
   - 使用批量请求API（如S3的批量操作）减少网络往返次数
   - 对于S3存储，配置适当的区域以减少延迟
   - 对于HDFS，考虑增加RPC连接超时时间以适应网络波动

## 扩展建议

1. **集成第三方库**：可以集成OpenCV、stb_image等库进行更复杂的图像处理
2. **支持更多数据格式**：可以扩展DataItem派生类以支持音频、视频等更多数据类型
3. **添加数据增强功能**：实现随机裁剪、翻转、旋转等数据增强功能

## 编译说明

### 使用CMake编译

1. 创建构建目录
```bash
mkdir build && cd build
```

2. 运行CMake
```bash
cmake ..
```

3. 编译项目
```bash
cmake --build .
```

### 直接使用编译器编译

```bash
g++ -std=c++17 -O3 example.cpp -o data_loader_example -pthread
```

## 注意事项

- 需要C++17或更高版本的编译器
- 在Windows平台上，某些功能可能需要特殊处理
- 多线程编程需要注意线程安全问题
- 内存映射文件在使用后需要正确释放资源

## 许可证

MIT License

## 联系方式

如有任何问题或建议，请随时联系项目维护者。