好的，既然我们已经深入了解了TCP，现在是时候转向另一种重要的网络传输协议：**UDP（用户数据报协议 - User Datagram Protocol）**。虽然TCP因其可靠性而被广泛使用，但在某些特定场景下，UDP因其轻量和高效的特点而成为更优的选择。

### 第七次讲解：UDP 网络编程基础与实践

### 1. 什么是 UDP？

UDP是TCP/IP协议族中的一个**无连接的**传输层协议。这意味着：

*   **无连接 (Connectionless):** UDP在发送数据之前不需要建立连接。每个UDP数据报都是独立发送的，就像寄信一样，你不需要事先打电话告诉对方你要寄信，直接写地址贴邮票就发出去了。
*   **不可靠 (Unreliable):** UDP不保证数据报的到达顺序、不保证数据报的完整性、不保证数据报不丢失、也不提供流量控制和拥塞控制。这意味着数据报可能会丢失、重复、乱序到达。
*   **基于数据报 (Datagram-based):** UDP传输的基本单位是数据报 (Datagram)。每个数据报都是一个独立的消息，有明确的消息边界。
*   **开销小、速度快 (Low Overhead, Fast):** 由于缺乏连接管理、重传机制和流量控制，UDP的开销非常小，传输效率高，延迟低。

### 2. UDP vs. TCP：何时选择？

| 特性           | TCP (传输控制协议)                                     | UDP (用户数据报协议)                                   |
| :------------- | :----------------------------------------------------- | :----------------------------------------------------- |
| **连接性**     | 面向连接 (三次握手建立，四次挥手断开)                 | 无连接 (直接发送数据)                                 |
| **可靠性**     | 可靠 (确认应答、超时重传、流量控制、拥塞控制)         | 不可靠 (不保证数据到达、顺序、完整)                    |
| **传输单位**   | 字节流 (没有消息边界)                                 | 数据报 (有明确消息边界)                               |
| **传输速度**   | 相对较慢 (因可靠性机制引入开销)                      | 相对较快 (开销小)                                     |
| **头部开销**   | 较大 (20字节固定 + 选项)                                | 较小 (8字节固定)                                       |
| **适用场景**   | 对数据完整性要求高、文件传输、HTTP、FTP、SSH、邮件等 | 对实时性要求高、允许少量数据丢失、DNS、DHCP、NTP、VoIP、在线游戏、视频直播、心跳包 |

### 3. UDP 核心 Socket API 函数

尽管UDP是无连接的，但它仍然使用Socket API进行编程。大部分函数与TCP相似，但有关键区别。

#### 3.1 `socket()` - 创建套接字

*   **功能:** 创建一个UDP套接字。
*   **函数原型:**
    ```c
    int socket(int domain, int type, int protocol);
    ```
*   **参数:**
    *   `domain`: `AF_INET` (IPv4) 或 `AF_INET6` (IPv6)。
    *   `type`: **`SOCK_DGRAM`** (数据报套接字)。
    *   `protocol`: 通常设为0，表示使用默认协议 (UDP)。

#### 3.2 `bind()` - 绑定地址与端口 (服务器端通常需要)

*   **功能:** 将一个本地地址（IP地址和端口号）绑定到UDP套接字上。UDP服务器端必须绑定，以便其他方知道向哪里发送数据。UDP客户端通常不需要显式绑定，系统会自动分配一个临时端口。
*   **函数原型:**
    ```c
    int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    ```
*   **参数:** 与TCP的`bind()`相同。

#### 3.3 `sendto()` - 发送数据报

*   **功能:** 向指定的目的地址发送一个UDP数据报。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen);
    ```
*   **参数:**
    *   `sockfd`: UDP套接字描述符。
    *   `buf`: 要发送数据的缓冲区。
    *   `len`: 要发送的字节数。
    *   `flags`: 通常设为0。
    *   `dest_addr`: 指向`sockaddr`结构体的指针，包含**目的地的IP地址和端口号**。
    *   `addrlen`: `dest_addr`结构体的大小。
*   **返回值:** 成功返回发送的字节数，失败返回 -1。

#### 3.4 `recvfrom()` - 接收数据报

*   **功能:** 从UDP套接字接收一个数据报，并获取发送方的地址信息。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen);
    ```
*   **参数:**
    *   `sockfd`: UDP套接字描述符。
    *   `buf`: 接收数据的缓冲区。
    *   `len`: 缓冲区的大小。
    *   `flags`: 通常设为0。
    *   `src_addr`: 指向`sockaddr`结构体的指针，用于存储**发送方的地址信息**。可以传入`NULL`，表示不关心发送方地址。
    *   `addrlen`: 指向`socklen_t`的指针。调用前设为`src_addr`结构体的大小，返回时更新为实际接收到的地址长度。可以传入`NULL`。
*   **返回值:** 成功返回接收的字节数，失败返回 -1。

#### 3.5 `close()` - 关闭套接字

*   **功能:** 关闭一个UDP套接字。
*   **函数原型:** 与TCP的`close()`相同。

#### 3.6 UDP的 `connect()` (可选，但有用)

你可能会好奇，UDP是无连接的，为什么也有`connect()`？

*   **功能:** UDP的`connect()`函数并不像TCP那样建立一个真正的连接。它只是在本地套接字上**绑定一个固定的目的地址（IP和端口）**。
*   **目的:**
    1.  **简化后续发送:** 一旦`connect()`，后续发送数据可以使用`send()`而不是`sendto()`，因为目的地址已经确定。
    2.  **过滤数据报:** 只有来自`connect()`指定地址的数据报才会被接收，其他源的数据报会被丢弃。
    3.  **获取错误信息:** 对于异步错误（如“端口不可达”），`connect()`ed的UDP套接字可以接收到这些错误，而非`connect()`ed的则不行。
*   **重要提示:** 即使调用了`connect()`，UDP套接字仍然是无连接的。每次`send()`仍然是独立的，不保证可靠性。要更改目的地址，需要再次调用`connect()`。

### 4. UDP 通信流程

#### UDP 服务器端基本流程：

1.  **创建套接字 (`socket()`):** 使用`SOCK_DGRAM`类型。
2.  **绑定地址和端口 (`bind()`):** 将服务器的IP地址和端口绑定到套接字上，以便客户端知道向哪里发送数据。
3.  **循环接收数据 (`recvfrom()`):** 在循环中等待并接收客户端发送的数据报。`recvfrom()`会同时获取发送方地址。
4.  **处理数据并回复 (`sendto()`):** 根据接收到的数据进行处理，并通过`sendto()`将回复发送回发送方地址。
5.  **关闭套接字 (`close()`):** 服务器停止服务时关闭套接字。

```mermaid
graph LR
    A[UDP服务器启动] --> B{socket(AF_INET, SOCK_DGRAM, 0)};
    B --> C{bind()};
    C --> D[进入主循环];
    D --> E{recvfrom()};
    E -- 接收到数据 --> F[处理数据];
    F --> G{sendto(发送方地址)};
    G --> D;
    D -- 服务器退出 --> H{close()};
```

#### UDP 客户端基本流程：

1.  **创建套接字 (`socket()`):** 使用`SOCK_DGRAM`类型。
2.  **(可选) 绑定地址和端口 (`bind()`):** 客户端通常不需显式绑定，系统会自动分配临时端口。
3.  **发送数据 (`sendto()`):** 向服务器的IP地址和端口发送数据报。
4.  **接收回复 (`recvfrom()`):** 等待并接收服务器的回复。
5.  **关闭套接字 (`close()`):** 通信结束后关闭套接字。

```mermaid
graph LR
    A[UDP客户端启动] --> B{socket(AF_INET, SOCK_DGRAM, 0)};
    B --> C{sendto(服务器地址)};
    C --> D{recvfrom()};
    D -- 收到回复 --> E[处理回复];
    E --> F{close()};
```

### 5. UDP Echo 服务器与客户端代码示例

#### 5.1 UDP Echo 服务器 (udp_server.c)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For close()
#include <arpa/inet.h> // For sockaddr_in, inet_ntoa, htons

#define PORT 8080
#define MAX_BUFFER_SIZE 1024

int main() {
    int sockfd; // UDP套接字
    struct sockaddr_in server_addr, client_addr; // 服务器和客户端地址结构
    socklen_t client_addr_len;
    char buffer[MAX_BUFFER_SIZE];

    // 1. 创建UDP套接字
    // AF_INET: IPv4协议族
    // SOCK_DGRAM: 数据报套接字 (UDP)
    // 0: 默认协议 (UDP)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    printf("UDP Server: Socket created successfully.\n");

    // 准备服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有可用IP
    server_addr.sin_port = htons(PORT); // 端口号，转换为网络字节序

    // 2. 绑定套接字到服务器地址和端口
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("UDP Server: Socket bound to port %d.\n", PORT);
    printf("UDP Server: Waiting for messages...\n");

    // 主循环：持续接收和回复数据报
    while (1) {
        client_addr_len = sizeof(client_addr);
        memset(buffer, 0, MAX_BUFFER_SIZE); // 清空缓冲区

        // 3. 接收客户端数据报
        // recvfrom()会阻塞，直到收到数据
        // 同时会填充发送方(client)的地址信息到client_addr
        ssize_t bytes_received = recvfrom(sockfd, (char *)buffer, MAX_BUFFER_SIZE, 0,
                                         (struct sockaddr *)&client_addr, &client_addr_len);
        if (bytes_received == -1) {
            perror("recvfrom failed");
            continue; // 继续等待下一个数据报
        }

        // 在接收到的数据末尾添加字符串结束符，确保printf安全
        buffer[bytes_received] = '\0';

        printf("UDP Server: Received %zd bytes from %s:%d: %s\n",
               bytes_received, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        // 4. 将接收到的数据回显给客户端 (Echo)
        // sendto()使用recvfrom()获取的客户端地址进行回复
        ssize_t bytes_sent = sendto(sockfd, (const char *)buffer, bytes_received, 0,
                                    (const struct sockaddr *)&client_addr, client_addr_len);
        if (bytes_sent == -1) {
            perror("sendto failed");
        } else {
            printf("UDP Server: Echoed %zd bytes back to client.\n", bytes_sent);
        }
    }

    // 5. 关闭套接字 (通常不会执行到这里，除非强制退出)
    close(sockfd);
    printf("UDP Server: Shutting down.\n");

    return 0;
}
```

**编译服务器代码:** `gcc udp_server.c -o udp_server`

#### 5.2 UDP Echo 客户端 (udp_client.c)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For close()
#include <arpa/inet.h> // For sockaddr_in, inet_pton, htons

#define PORT 8080
#define SERVER_IP "127.0.0.1" // 服务器IP地址
#define MAX_BUFFER_SIZE 1024

int main() {
    int sockfd; // UDP套接字
    struct sockaddr_in server_addr; // 服务器地址结构
    char buffer[MAX_BUFFER_SIZE];
    const char *message = "Hello UDP Server!";

    // 1. 创建UDP套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    printf("UDP Client: Socket created successfully.\n");

    // 准备服务器地址结构
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT); // 端口号，转换为网络字节序

    // 将IP地址从字符串转换为网络字节序的二进制形式
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("UDP Client: Server address prepared: %s:%d.\n", SERVER_IP, PORT);

    // 2. 向服务器发送数据报
    // sendto()直接指定目的地址
    ssize_t bytes_sent = sendto(sockfd, message, strlen(message), 0,
                               (const struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_sent == -1) {
        perror("sendto failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("UDP Client: Sent %zd bytes to server: %s\n", bytes_sent, message);

    // 3. 接收服务器回复
    // recvfrom()会阻塞，直到收到数据
    // 对于客户端，通常不关心回复来自哪个地址，可以传入NULL
    // 但这里为了完整性，我们还是接收了
    socklen_t server_addr_len = sizeof(server_addr); // 再次设置长度，以防万一
    memset(buffer, 0, MAX_BUFFER_SIZE); // 清空缓冲区
    ssize_t bytes_received = recvfrom(sockfd, (char *)buffer, MAX_BUFFER_SIZE, 0,
                                     (struct sockaddr *)&server_addr, &server_addr_len);
    if (bytes_received == -1) {
        perror("recvfrom failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[bytes_received] = '\0'; // 添加字符串结束符
    printf("UDP Client: Received %zd bytes from server: %s\n", bytes_received, buffer);

    // 4. 关闭套接字
    close(sockfd);
    printf("UDP Client: Socket closed. Client exiting.\n");

    return 0;
}
```

**编译客户端代码:** `gcc udp_client.c -o udp_client`

### 6. 如何运行

1.  **打开第一个终端窗口，运行UDP服务器：**
    ```bash
    ./udp_server
    ```
    服务器会启动并显示："UDP Server: Waiting for messages..."

2.  **打开第二个终端窗口，运行UDP客户端：**
    ```bash
    ./udp_client
    ```
    客户端会发送消息并接收回复。
    *   客户端输出: `UDP Client: Sent ... Received ...`
    *   服务器输出: `UDP Server: Received ... Echoed ...`

你可以多次运行客户端，服务器会持续响应。

### 7. UDP编程的关键考虑点

*   **数据报大小:** UDP数据报理论上最大可达65535字节（包括IP和UDP头部），但实际中由于MTU（最大传输单元）的限制，通常建议将UDP数据报大小控制在1472字节（IPv4）或1280字节（IPv6）以内，以避免IP分片和重组，这会降低性能和可靠性。
*   **乱序与丢失:** 由于UDP不可靠，应用程序需要自己处理数据报的乱序、丢失、重复问题。这通常通过在应用层添加序列号、确认应答、重传机制等方式来实现“伪可靠性”。
*   **洪水攻击 (Flooding):** UDP服务器容易受到洪水攻击，因为不需要建立连接就可以发送数据。
*   **多播 (Multicast) 与广播 (Broadcast):** UDP非常适合多播和广播通信，因为其无连接的特性天然支持向多个目标发送数据。

UDP虽然“简单粗暴”，但它在对延迟敏感、数据丢失可接受的场景中发挥着不可替代的作用。例如，实时音视频、在线游戏通常会选择UDP，并自己实现一套更轻量级的可靠性保障机制。

至此，我们对Linux网络编程中的TCP和UDP都进行了详细的探讨。下次，我们可以根据你的兴趣，深入到更高级的话题，比如：

*   **套接字选项 (`getsockopt`/`setsockopt`) 的高级用法**
*   **优雅关闭连接 (Linger 选项，半关闭)**
*   **应用层协议与序列化**
*   **高级I/O模型：Unix域套接字**
*   **Linux网络配置与工具**

你希望我们接下来深入哪个方向呢？