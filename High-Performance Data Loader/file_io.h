#ifndef FILE_IO_H
#define FILE_IO_H

#include <string>
#include <vector>
#include <memory>
#include <cstddef>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

/**
 * 文件I/O工具类 - 提供高性能的文件读取功能
 */
class FileIO {
public:
    /**
     * 读取整个文件到内存
     * @param file_path 文件路径
     * @return 文件内容的内存缓冲区
     */
    static std::vector<unsigned char> readFile(const std::string& file_path) {
        FILE* file = fopen(file_path.c_str(), "rb");
        if (!file) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        
        // 获取文件大小
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // 分配缓冲区
        std::vector<unsigned char> buffer(file_size);
        
        // 读取文件内容
        size_t bytes_read = fread(buffer.data(), 1, file_size, file);
        if (bytes_read != static_cast<size_t>(file_size)) {
            fclose(file);
            throw std::runtime_error("Failed to read entire file: " + file_path);
        }
        
        fclose(file);
        return buffer;
    }
    
    /**
     * 使用内存映射读取文件
     * @param file_path 文件路径
     * @return 内存映射的文件内容
     */
    static std::shared_ptr<unsigned char[]> mmapFile(const std::string& file_path, size_t& out_size) {
#ifdef _WIN32
        // Windows实现
        HANDLE hFile = CreateFileA(
            file_path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        
        // 获取文件大小
        DWORD file_size_low = GetFileSize(hFile, NULL);
        if (file_size_low == INVALID_FILE_SIZE) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to get file size: " + file_path);
        }
        
        out_size = static_cast<size_t>(file_size_low);
        
        // 创建文件映射
        HANDLE hMapping = CreateFileMappingA(
            hFile,
            NULL,
            PAGE_READONLY,
            0,
            0,
            NULL
        );
        
        if (!hMapping) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to create file mapping: " + file_path);
        }
        
        // 映射视图
        void* mapped_data = MapViewOfFile(
            hMapping,
            FILE_MAP_READ,
            0,
            0,
            0
        );
        
        if (!mapped_data) {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            throw std::runtime_error("Failed to map view of file: " + file_path);
        }
        
        // 复制映射的数据到堆内存
        auto buffer = std::shared_ptr<unsigned char[]>(new unsigned char[out_size]);
        memcpy(buffer.get(), mapped_data, out_size);
        
        // 清理资源
        UnmapViewOfFile(mapped_data);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        
        return buffer;
#else
        // POSIX实现
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("Failed to open file: " + file_path + " - " + strerror(errno));
        }
        
        // 获取文件大小
        struct stat stat_buf;
        if (fstat(fd, &stat_buf) < 0) {
            close(fd);
            throw std::runtime_error("Failed to get file size: " + file_path + " - " + strerror(errno));
        }
        
        out_size = static_cast<size_t>(stat_buf.st_size);
        
        // 内存映射
        void* mapped_data = mmap(NULL, out_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped_data == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Failed to mmap file: " + file_path + " - " + strerror(errno));
        }
        
        // 复制映射的数据到堆内存
        auto buffer = std::shared_ptr<unsigned char[]>(new unsigned char[out_size]);
        memcpy(buffer.get(), mapped_data, out_size);
        
        // 清理资源
        munmap(mapped_data, out_size);
        close(fd);
        
        return buffer;
#endif
    }
    
    /**
     * 检查文件是否存在
     * @param file_path 文件路径
     * @return 如果文件存在则返回true
     */
    static bool fileExists(const std::string& file_path) {
#ifdef _WIN32
        DWORD dwAttrib = GetFileAttributesA(file_path.c_str());
        return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
                !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
        struct stat buffer;
        return (stat(file_path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode));
#endif
    }
    
    /**
     * 获取文件大小
     * @param file_path 文件路径
     * @return 文件大小（字节）
     */
    static size_t getFileSize(const std::string& file_path) {
#ifdef _WIN32
        DWORD file_size_low, file_size_high;
        HANDLE hFile = CreateFileA(
            file_path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        
        file_size_low = GetFileSize(hFile, &file_size_high);
        CloseHandle(hFile);
        
        if (file_size_low == INVALID_FILE_SIZE) {
            throw std::runtime_error("Failed to get file size: " + file_path);
        }
        
        return (static_cast<size_t>(file_size_high) << 32) + static_cast<size_t>(file_size_low);
#else
        struct stat stat_buf;
        if (stat(file_path.c_str(), &stat_buf) < 0) {
            throw std::runtime_error("Failed to get file size: " + file_path + " - " + strerror(errno));
        }
        
        return static_cast<size_t>(stat_buf.st_size);
#endif
    }
    
    /**
     * 读取文本文件
     * @param file_path 文件路径
     * @return 文件内容的字符串
     */
    static std::string readTextFile(const std::string& file_path) {
        FILE* file = fopen(file_path.c_str(), "r");
        if (!file) {
            throw std::runtime_error("Failed to open file: " + file_path);
        }
        
        // 获取文件大小
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // 分配缓冲区
        std::string buffer(file_size, '\0');
        
        // 读取文件内容
        size_t bytes_read = fread(&buffer[0], 1, file_size, file);
        if (bytes_read != static_cast<size_t>(file_size)) {
            fclose(file);
            throw std::runtime_error("Failed to read entire file: " + file_path);
        }
        
        fclose(file);
        return buffer;
    }
};

#endif // FILE_IO_H