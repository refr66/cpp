好的，我们继续深入讲解！

### 第二次讲解：核心Socket API函数与TCP通信流程

在理解了Socket的基本概念和地址结构之后，接下来我们将学习Linux网络编程中最重要的Socket API函数。这些函数是构建任何网络应用的基础。

### 1. 核心Socket API函数详解

以下是进行网络编程时最常用的几个系统调用（函数）：

#### 1.1 `socket()` - 创建套接字

*   **功能:** 创建一个套接字，并返回一个套接字描述符（一个整数，类似于文件描述符）。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    int socket(int domain, int type, int protocol);
    ```
*   **参数:**
    *   `domain`: **协议族 (Protocol Family)**，指定通信的域。
        *   `AF_INET`: IPv4协议。
        *   `AF_INET6`: IPv6协议。
        *   `AF_UNIX` 或 `AF_LOCAL`: 本地通信（Unix域套接字）。
    *   `type`: **套接字类型 (Socket Type)**。
        *   `SOCK_STREAM`: 流式套接字（TCP）。
        *   `SOCK_DGRAM`: 数据报套接字（UDP）。
        *   `SOCK_RAW`: 原始套接字。
    *   `protocol`: **具体协议 (Protocol)**。通常设为0，表示使用给定协议族和套接字类型下的默认协议。
        *   对于 `AF_INET` 和 `SOCK_STREAM`，默认是TCP（`IPPROTO_TCP`）。
        *   对于 `AF_INET` 和 `SOCK_DGRAM`，默认是UDP（`IPPROTO_UDP`）。
*   **返回值:** 成功返回新的套接字描述符，失败返回 -1。

#### 1.2 `bind()` - 绑定地址与端口

*   **功能:** 将一个本地地址（IP地址和端口号）绑定到已创建的套接字上。服务器端必须绑定，客户端通常不需要显式绑定，系统会自动分配一个临时端口。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    ```
*   **参数:**
    *   `sockfd`: 要绑定的套接字描述符（由`socket()`返回）。
    *   `addr`: 指向`sockaddr`结构体的指针，包含了要绑定的IP地址和端口号。需要将`sockaddr_in`或`sockaddr_in6`结构体强制类型转换为`sockaddr*`。
    *   `addrlen`: `addr`结构体的大小。
*   **返回值:** 成功返回0，失败返回 -1。

#### 1.3 `listen()` - 监听传入连接（仅限服务器TCP）

*   **功能:** 将一个套接字设置为监听状态，准备接受客户端的连接请求。此函数仅用于TCP流式套接字。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    int listen(int sockfd, int backlog);
    ```
*   **参数:**
    *   `sockfd`: 处于监听状态的套接字描述符。
    *   `backlog`: 队列中允许的最大未完成连接请求数（等待`accept()`接受的连接）。这个值通常设为一个较小的正整数，例如`SOMAXCONN`（系统定义的上限）。
*   **返回值:** 成功返回0，失败返回 -1。

#### 1.4 `accept()` - 接受连接（仅限服务器TCP）

*   **功能:** 从`listen()`的队列中接受一个传入的连接请求。如果队列中没有连接，`accept()`会阻塞，直到有新的连接到来。成功后，它会返回一个新的套接字描述符，这个新的套接字专门用于与这个客户端进行通信，而原来的监听套接字继续监听新的连接。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
    ```
*   **参数:**
    *   `sockfd`: 监听套接字描述符。
    *   `addr`: 指向`sockaddr`结构体的指针，用于存储连接客户端的地址信息。可以传入`NULL`，表示不关心客户端地址。
    *   `addrlen`: 指向`socklen_t`的指针，在调用前设为`addr`结构体的大小，函数返回时会更新为实际存储的客户端地址的长度。可以传入`NULL`。
*   **返回值:** 成功返回一个新的套接字描述符（**已连接套接字**），用于与客户端进行通信；失败返回 -1。

#### 1.5 `connect()` - 建立连接（仅限客户端TCP）

*   **功能:** 客户端使用此函数向服务器发起连接请求。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    ```
*   **参数:**
    *   `sockfd`: 客户端的套接字描述符。
    *   `addr`: 指向`sockaddr`结构体的指针，包含服务器的IP地址和端口号。
    *   `addrlen`: `addr`结构体的大小。
*   **返回值:** 成功返回0，失败返回 -1。

#### 1.6 `send()` / `recv()` - 发送/接收数据（TCP/UDP）

*   **功能:** 用于在已连接的套接字（TCP）或指定的对端（UDP）上发送和接收数据。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    ssize_t send(int sockfd, const void *buf, size_t len, int flags);
    ssize_t recv(int sockfd, void *buf, size_t len, int flags);
    ```*   **参数:**
    *   `sockfd`: 套接字描述符。
    *   `buf`: 指向要发送/接收数据的缓冲区的指针。
    *   `len`: 要发送/接收的字节数。
    *   `flags`: 控制发送/接收行为的标志，通常设为0。
        *   `MSG_WAITALL`: 等待所有数据都被接收或发生错误。
        *   `MSG_PEEK`: 查看数据，但不从缓冲区中移除。
*   **返回值:** 成功返回实际发送/接收的字节数；如果连接关闭，`recv()`返回0；失败返回 -1。

*   **`read()` / `write()`**：对于TCP流式套接字，`read()`和`write()`也可以用于发送和接收数据，它们与`send()`/`recv()`在很多情况下是等价的，但在错误处理和某些高级特性上有所不同。在TCP中，通常推荐使用`read()`/`write()`，因为它们更通用，且符合Unix的“一切皆文件”哲学。

#### 1.7 `close()` - 关闭套接字

*   **功能:** 关闭一个套接字描述符，释放其占用的资源。
*   **函数原型:**
    ```c
    #include <unistd.h>

    int close(int fd);
    ```
*   **参数:** `fd`: 要关闭的套接字描述符。
*   **返回值:** 成功返回0，失败返回 -1。

#### 1.8 `shutdown()` - 控制连接关闭方式

*   **功能:** 提供比`close()`更精细的关闭控制，可以部分关闭连接（例如，只禁止发送或只禁止接收）。
*   **函数原型:**
    ```c
    #include <sys/socket.h>

    int shutdown(int sockfd, int how);
    ```
*   **参数:**
    *   `sockfd`: 套接字描述符。
    *   `how`: 关闭方式。
        *   `SHUT_RD`: 禁止进一步的接收操作。
        *   `SHUT_WR`: 禁止进一步的发送操作。
        *   `SHUT_RDWR`: 禁止进一步的发送和接收操作。
*   **返回值:** 成功返回0，失败返回 -1。

### 2. TCP 服务器端基本流程

一个典型的TCP服务器会按照以下步骤工作：

1.  **创建套接字 (`socket()`):** 创建一个监听套接字，指定`AF_INET`/`AF_INET6`和`SOCK_STREAM`。
2.  **绑定地址和端口 (`bind()`):** 将服务器的IP地址和端口绑定到监听套接字上。通常IP地址绑定为`INADDR_ANY`（或`IN6ADDR_ANY`），表示监听所有可用网络接口。
3.  **开始监听 (`listen()`):** 将监听套接字设置为监听模式，等待客户端连接。
4.  **接受连接 (`accept()`):** 阻塞等待并接受客户端的连接请求。当一个连接建立时，`accept()`会返回一个新的已连接套接字描述符。此时，父进程（监听进程）通常会派生一个子进程或创建一个线程来处理这个新的连接，而父进程继续循环调用`accept()`来处理新的连接请求。
5.  **数据传输 (`read()`/`write()` 或 `send()`/`recv()`):** 使用新的已连接套接字与客户端进行双向数据通信。
6.  **关闭连接 (`close()`):** 当数据传输完成或发生错误时，关闭已连接套接字。服务器继续监听新的连接。

```mermaid
graph LR
    A[服务器启动] --> B{socket()};
    B --> C{bind()};
    C --> D{listen()};
    D --> E[等待客户端连接];
    E -- 接收到连接请求 --> F{accept()};
    F --> G[新连接套接字];
    G -- 与客户端通信 --> H{read()/write()};
    H -- 通信结束 --> I{close(新连接套接字)};
    I --> E;
    D --> J[客户端连接队列];
    J -- accept从队列中取出连接 --> F;
```

### 3. TCP 客户端基本流程

一个典型的TCP客户端会按照以下步骤工作：

1.  **创建套接字 (`socket()`):** 创建一个客户端套接字，指定`AF_INET`/`AF_INET6`和`SOCK_STREAM`。
2.  **建立连接 (`connect()`):** 向服务器的指定IP地址和端口发起连接请求。
3.  **数据传输 (`read()`/`write()` 或 `send()`/`recv()`):** 连接建立后，客户端与服务器进行双向数据通信。
4.  **关闭连接 (`close()`):** 数据传输完成或发生错误时，关闭客户端套接字。

```mermaid
graph LR
    A[客户端启动] --> B{socket()};
    B --> C{connect()};
    C -- 连接成功 --> D[已连接套接字];
    D -- 与服务器通信 --> E{read()/write()};
    E -- 通信结束 --> F{close()};
    F --> G[客户端退出];
```

### 4. 字节序转换函数

在网络通信中，一个非常重要且容易出错的地方是字节序问题。不同的CPU架构可能使用不同的字节序（大端序或小端序）。网络协议通常规定使用**网络字节序**（大端序）。因此，在将本地主机的整数（如端口号、IP地址）发送到网络之前，需要将其转换为网络字节序；从网络接收到的数据，也需要从网络字节序转换回主机字节序。

*   **`htons()`**: host to network short (16位，用于端口号)
*   **`htonl()`**: host to network long (32位，用于IPv4地址)
*   **`ntohs()`**: network to host short
*   **`ntohl()`**: network to host long

这些函数通常定义在`<arpa/inet.h>`头文件中。

例如，设置端口号：
`serv_addr.sin_port = htons(8080);`

设置IP地址：
`serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);`
或者使用`inet_addr()`/`inet_pton()`将字符串IP地址转换为网络字节序的二进制格式。

### 总结

本次讲解详细介绍了TCP网络编程中使用的核心Socket API函数的功能、参数和返回值，并概述了TCP服务器和客户端的基本工作流程图，还强调了字节序转换的重要性。

在下一次讲解中，我将提供一个完整的、可编译运行的C语言TCP服务器和客户端的简单示例代码，并通过代码进一步解释这些函数的实际应用。

继续学习，下一次我们将看到这些理论知识如何转化为实际的代码！