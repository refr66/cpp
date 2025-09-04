# CMakeLists.txt for High-Performance Data Loader

# 指定CMake的最低版本要求
cmake_minimum_required(VERSION 3.15)

# 项目名称和版本
project(HighPerformanceDataLoader VERSION 1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    # Microsoft Visual C++ 编译器选项
    add_compile_options(/W4 /WX /O2 /EHsc)
else()
    # GCC/Clang 编译器选项
    add_compile_options(-Wall -Wextra -Werror -O3)
endif()

# 查找线程库
find_package(Threads REQUIRED)

# 添加示例可执行文件
add_executable(data_loader_example
    example.cpp
    storage.cpp
    # 注意：头文件不需要在这里列出，因为它们会被源文件包含
)

# 链接线程库
target_link_libraries(data_loader_example PRIVATE Threads::Threads)

# 在Windows平台上，添加Windows库
if(WIN32)
    target_link_libraries(data_loader_example PRIVATE kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32)
endif()

# 包含头文件目录
target_include_directories(data_loader_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 安装规则
install(TARGETS data_loader_example
    RUNTIME DESTINATION bin
)

# 可选：添加测试（如果需要）
# enable_testing()
# add_test(NAME example_test COMMAND data_loader_example)

# 可选：如果项目发展为库，添加库的构建规则
# add_library(data_loader_lib STATIC
#     # 库的源文件
# )
# 
# target_link_libraries(data_loader_lib PRIVATE Threads::Threads)
# target_include_directories(data_loader_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})