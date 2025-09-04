好的，我们已经学习了基于IP地址和端口的网络套接字。现在，我们将转向另一种非常重要的I/O模型，它在本地进程间通信（IPC）中扮演着关键角色：**Unix域套接字 (Unix Domain Sockets - UDS)**。

### 第九次讲解：高级I/O模型 - Unix域套接字 (Unix Domain Sockets)

### 1. 什么是 Unix 域套接字？

Unix域套接字（通常简称为UDS，或IPC套接字）是一种用于在**同一台机器上的不同进程之间进行通信**的机制。与基于IP地址和端口的网络套接字（如AF_INET/AF_INET6）不同，UDS不通过网络协议栈，而是直接利用文件系统路径作为它们的地址。

你可以把UDS看作是套接字API的“本地版”，它提供了与网络套接字相似的接口，但在底层完全不同。

### 2. UDS 的优势

*   **高性能:**
    *   **无网络开销:** UDS不涉及网络协议栈（TCP/IP、UDP/IP），无需进行IP地址解析、端口查找、路由、校验和计算等。数据直接在内核空间中进行复制，避免了网络通信中额外的协议处理开销。
    *   **零拷贝潜力:** 在某些情况下（尤其是在Linux上，当使用`splice()`等系统调用时），甚至可以实现零拷贝或更高效的数据传输。
*   **支持文件描述符传递 (File Descriptor Passing):** 这是UDS独有的一个强大功能。一个进程可以通过UDS将一个打开的文件描述符（可以是文件、管道、另一个套接字等）传递给另一个不相关的进程。这在一些高级IPC场景中非常有用。
*   **支持凭证传递 (Credential Passing):** 可以获取连接对端进程的PID、UID、GID等信息，用于更细粒度的权限控制。
*   **访问控制:** 由于UDS使用文件系统路径作为地址，它们的访问权限可以通过标准的文件系统权限（`chmod`, `chown`）来控制，这比仅仅依赖端口号来控制访问更灵活和安全。
*   **语义与网络套接字相似:** 对于熟悉网络套接字编程的开发者来说，UDS的API非常相似，学习曲线较低。

### 3. UDS 的局限性

*   **仅限于本地通信:** 无法用于跨机器的网络通信。
*   **路径长度限制:** UDS的地址（文件路径）通常有长度限制（`UNIX_PATH_MAX`，通常是108字节），这在某些情况下可能需要注意。

### 4. UDS 的类型

与网络套接字一样，UDS也支持流式和数据报两种类型：

*   **流式 Unix 域套接字 (`SOCK_STREAM`):**
    *   提供可靠的、面向连接的、有序的字节流服务。
    *   行为类似于TCP套接字。
    *   适用于需要可靠数据传输和连接状态的本地通信。
*   **数据报 Unix 域套接字 (`SOCK_DGRAM`):**
    *   提供不可靠的、无连接的、定长数据报服务。
    *   行为类似于UDP套接字，但其可靠性比UDP更高（在本地通常不会丢失或乱序）。
    *   每个数据报都是独立的，有明确的消息边界。
    *   适用于对延迟敏感、消息有明确边界、或者需要广播/多播效果的本地通信。

### 5. UDS 核心 Socket API 函数

创建和使用UDS的API与网络套接字非常相似，主要区别在于`socket()`的`domain`参数和地址结构体。

#### 5.1 `socket()` - 创建套接字

*   **`domain`:** 使用 `AF_UNIX` 或 `AF_LOCAL`。这两个宏是等价的。
*   **`type`:** `SOCK_STREAM` 或 `SOCK_DGRAM`。
*   **`protocol`:** 通常为0。

#### 5.2 `struct sockaddr_un` - UDS 地址结构

```c
#include <sys/un.h> // 包含 sockaddr_un 结构体的定义

struct sockaddr_un {
    sa_family_t sun_family; // 地址族，总是 AF_UNIX
    char        sun_path[108]; // 套接字文件路径
};
```
*   `sun_family`: 总是`AF_UNIX`。
*   `sun_path`: 存储套接字文件在文件系统中的路径名。这个路径名必须是唯一的，且必须是可写的目录。

#### 5.3 其他函数

*   **`bind()`:** 将`sockaddr_un`结构体中的路径绑定到套接字上。
*   **`listen()` / `accept()`:** 用于流式UDS服务器端。
*   **`connect()`:** 用于流式UDS客户端。
*   **`send()` / `recv()` 或 `read()` / `write()`:** 用于流式UDS数据传输。
*   **`sendto()` / `recvfrom()`:** 用于数据报UDS数据传输。
*   **`unlink()` (重要!)**: 在服务器程序退出前，必须使用`unlink()`函数删除`bind()`时创建的套接字文件。如果服务器异常退出，该文件可能会残留，导致下次启动时`bind()`失败。

### 6. UDS 代码示例

#### 6.1 流式 Unix 域套接字示例 (Stream UDS)

**server_uds_stream.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h> // For sockaddr_un

#define UDS_PATH "/tmp/my_uds_socket_stream" // UDS文件路径
#define BACKLOG 5 // listen队列长度

int main() {
    int listen_fd, client_fd;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[256];

    // 1. 创建流式UDS套接字
    if ((listen_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("Server UDS: Socket created.\n");

    // 2. 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, UDS_PATH, sizeof(server_addr.sun_path) - 1);

    // 3. 删除旧的UDS文件 (重要，防止上次异常退出导致文件残留)
    unlink(UDS_PATH);

    // 4. 绑定套接字到文件路径
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server UDS: Bound to %s.\n", UDS_PATH);

    // 5. 监听连接
    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        close(listen_fd);
        unlink(UDS_PATH);
        exit(EXIT_FAILURE);
    }
    printf("Server UDS: Listening for connections...\n");

    // 6. 接受客户端连接
    client_addr_len = sizeof(client_addr);
    if ((client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
        perror("accept");
        close(listen_fd);
        unlink(UDS_PATH);
        exit(EXIT_FAILURE);
    }
    printf("Server UDS: Client connected.\n");

    // 7. 接收数据
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Server UDS: Received from client: %s\n", buffer);
    } else if (bytes_read == 0) {
        printf("Server UDS: Client disconnected.\n");
    } else {
        perror("read");
    }

    // 8. 发送回复
    const char *response = "Hello from UDS Stream Server!";
    if (write(client_fd, response, strlen(response)) == -1) {
        perror("write");
    } else {
        printf("Server UDS: Sent to client: %s\n", response);
    }

    // 9. 关闭套接字并清理UDS文件
    close(client_fd);
    close(listen_fd);
    unlink(UDS_PATH); // 再次强调清理
    printf("Server UDS: Shutting down and cleaned up %s.\n", UDS_PATH);

    return 0;
}
```

**client_uds_stream.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h> // For sockaddr_un

#define UDS_PATH "/tmp/my_uds_socket_stream" // UDS文件路径

int main() {
    int client_fd;
    struct sockaddr_un server_addr;
    char buffer[256];
    const char *message = "Hello UDS Stream Server!";

    // 1. 创建流式UDS套接字
    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("Client UDS: Socket created.\n");

    // 2. 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, UDS_PATH, sizeof(server_addr.sun_path) - 1);

    // 3. 连接到服务器
    printf("Client UDS: Connecting to %s...\n", UDS_PATH);
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client UDS: Connected to server.\n");

    // 4. 发送数据
    if (write(client_fd, message, strlen(message)) == -1) {
        perror("write");
    } else {
        printf("Client UDS: Sent to server: %s\n", message);
    }

    // 5. 接收回复
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Client UDS: Received from server: %s\n", buffer);
    } else if (bytes_read == 0) {
        printf("Client UDS: Server disconnected.\n");
    } else {
        perror("read");
    }

    // 6. 关闭套接字
    close(client_fd);
    printf("Client UDS: Socket closed. Client exiting.\n");

    return 0;
}
```

**编译和运行流式UDS:**
```bash
gcc server_uds_stream.c -o server_uds_stream
gcc client_uds_stream.c -o client_uds_stream

# 在第一个终端运行服务器
./server_uds_stream

# 在第二个终端运行客户端
./client_uds_stream
```
观察输出，你会看到客户端和服务器之间通过`/tmp/my_uds_socket_stream`文件进行通信。

#### 6.2 数据报 Unix 域套接字示例 (Datagram UDS)

数据报UDS不需要`listen()`和`accept()`，客户端也通常需要`bind()`一个自己的UDS文件路径，以便服务器能回复数据。

**server_uds_dgram.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h> // For sockaddr_un

#define UDS_PATH "/tmp/my_uds_socket_dgram_server" // 服务器UDS文件路径
#define MAX_BUFFER_SIZE 256

int main() {
    int sockfd;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[MAX_BUFFER_SIZE];

    // 1. 创建数据报UDS套接字
    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("Server UDS DGRAM: Socket created.\n");

    // 2. 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, UDS_PATH, sizeof(server_addr.sun_path) - 1);

    // 3. 删除旧的UDS文件 (重要)
    unlink(UDS_PATH);

    // 4. 绑定套接字到文件路径
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Server UDS DGRAM: Bound to %s.\n", UDS_PATH);
    printf("Server UDS DGRAM: Waiting for messages...\n");

    // 主循环：接收和回复数据报
    while (1) {
        client_addr_len = sizeof(client_addr);
        memset(buffer, 0, MAX_BUFFER_SIZE);

        // 5. 接收数据报 (同时获取发送方地址)
        ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                         (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_received == -1) {
            perror("recvfrom");
            continue;
        }

        buffer[bytes_received] = '\0';
        printf("Server UDS DGRAM: Received %zd bytes from client %s: %s\n",
               bytes_received, client_addr.sun_path, buffer);

        // 6. 回复数据报给发送方
        const char *response = "Hello from UDS DGRAM Server!";
        if (sendto(sockfd, response, strlen(response), 0,
                   (struct sockaddr *)&client_addr, client_addr_len) == -1) {
            perror("sendto");
        } else {
            printf("Server UDS DGRAM: Sent to client: %s\n", response);
        }
    }

    // 7. 关闭套接字并清理UDS文件
    close(sockfd);
    unlink(UDS_PATH);
    printf("Server UDS DGRAM: Shutting down and cleaned up %s.\n", UDS_PATH);

    return 0;
}
```

**client_uds_dgram.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h> // For sockaddr_un

#define SERVER_UDS_PATH "/tmp/my_uds_socket_dgram_server" // 服务器UDS文件路径
#define CLIENT_UDS_PATH "/tmp/my_uds_socket_dgram_client" // 客户端UDS文件路径 (用于服务器回复)
#define MAX_BUFFER_SIZE 256

int main() {
    int sockfd;
    struct sockaddr_un server_addr, client_addr;
    char buffer[MAX_BUFFER_SIZE];
    const char *message = "Hello UDS DGRAM Server!";

    // 1. 创建数据报UDS套接字
    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    printf("Client UDS DGRAM: Socket created.\n");

    // 2. 配置客户端地址 (用于服务器回复)
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, CLIENT_UDS_PATH, sizeof(client_addr.sun_path) - 1);
    unlink(CLIENT_UDS_PATH); // 清理旧文件

    // 3. 绑定客户端套接字到文件路径
    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) == -1) {
        perror("bind client");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Client UDS DGRAM: Bound to %s.\n", CLIENT_UDS_PATH);

    // 4. 配置服务器地址 (发送数据用)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SERVER_UDS_PATH, sizeof(server_addr.sun_path) - 1);

    // 5. 发送数据报到服务器
    if (sendto(sockfd, message, strlen(message), 0,
               (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("sendto");
        close(sockfd);
        unlink(CLIENT_UDS_PATH);
        exit(EXIT_FAILURE);
    }
    printf("Client UDS DGRAM: Sent to server %s: %s\n", SERVER_UDS_PATH, message);

    // 6. 接收服务器回复
    socklen_t server_addr_len = sizeof(server_addr); // 再次设置长度，以防万一
    memset(buffer, 0, MAX_BUFFER_SIZE);
    ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                     (struct sockaddr *)&server_addr, &server_addr_len);
    if (bytes_received == -1) {
        perror("recvfrom");
    } else {
        buffer[bytes_received] = '\0';
        printf("Client UDS DGRAM: Received from server: %s\n", buffer);
    }

    // 7. 关闭套接字并清理UDS文件
    close(sockfd);
    unlink(CLIENT_UDS_PATH);
    printf("Client UDS DGRAM: Socket closed and cleaned up %s. Client exiting.\n", CLIENT_UDS_PATH);

    return 0;
}
```

**编译和运行数据报UDS:**
```bash
gcc server_uds_dgram.c -o server_uds_dgram
gcc client_uds_dgram.c -o client_uds_dgram

# 在第一个终端运行服务器
./server_uds_dgram

# 在第二个终端运行客户端
./client_uds_dgram
```
注意观察在`/tmp`目录下会生成套接字文件（例如：`my_uds_socket_stream`），这些文件是UDS的“地址”。它们并非普通文件，而是套接字类型的文件，你可以用`ls -l`看到它们的类型以`s`开头。

### 总结

Unix域套接字是Linux（以及其他类Unix系统）中非常高效和灵活的进程间通信方式。它们在需要高性能本地通信、文件描述符传递或基于文件系统权限进行访问控制的场景中非常有用。例如，很多本地服务（如X Window System、Docker daemon、Systemd等）都使用UDS进行内部通信。

希望这次的详细讲解和代码示例能让你对Unix域套接字有深刻的理解！

如果你有兴趣，我们可以继续探讨：
*   **文件描述符传递 (FD Passing) 的具体实现和应用**
*   **优雅关闭连接 (Linger 选项，半关闭) 的更深入探讨**
*   **应用层协议与序列化**
*   **Linux网络配置与工具**
*   **并发编程中的同步与互斥 (针对共享内存等)**

期待你的选择！