好的，我们继续！

### 第三次讲解：TCP服务器与客户端代码示例

理论知识讲得再多，不如一个实际的代码示例来得直观。这次我们将编写一个最简单的TCP服务器和客户端程序，它们之间可以发送和接收短消息。通过这个例子，你可以看到之前介绍的Socket API函数是如何在实际中被使用的。

在动手编写代码之前，请确保你有一个Linux环境（虚拟机、WSL、或者云服务器都可以），并且安装了GCC编译器。

---

### 1. 简单的TCP服务器 (server.c)

这个服务器会绑定到本地的`127.0.0.1`地址和端口`8080`，然后监听并接受一个客户端连接，接收客户端发来的消息，并回复一个“Hello from server!”。

```c
#include <stdio.h>          // 标准输入输出
#include <stdlib.h>         // 标准库函数，例如exit()
#include <string.h>         // 字符串处理函数，例如memset()
#include <unistd.h>         // Unix标准函数，例如close()

#include <sys/socket.h>     // socket()函数和相关数据结构
#include <netinet/in.h>     // sockaddr_in结构体和in_addr结构体
#include <arpa/inet.h>      // inet_addr()函数

#define PORT 8080           // 服务器监听的端口
#define MAX_BUFFER_SIZE 1024 // 缓冲区大小

int main() {
    int server_fd, new_socket; // server_fd: 监听套接字; new_socket: 已连接套接字
    struct sockaddr_in address; // 服务器地址结构
    int opt = 1;                // 用于setsockopt，端口复用
    int addrlen = sizeof(address); // 地址结构体大小
    char buffer[MAX_BUFFER_SIZE] = {0}; // 数据缓冲区
    char *hello = "Hello from server!"; // 服务器回复消息

    // 1. 创建套接字文件描述符
    // AF_INET: IPv4协议族
    // SOCK_STREAM: 流式套接字 (TCP)
    // 0: 默认协议 (TCP)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    printf("Server: Socket created successfully.\n");

    // 可选：设置套接字选项，允许地址和端口复用
    // 这样可以避免在服务器崩溃或重启后出现"Address already in use"错误
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    printf("Server: Socket options set.\n");

    // 准备服务器地址结构
    address.sin_family = AF_INET; // IPv4
    // INADDR_ANY表示监听所有可用的网络接口
    // inet_addr("127.0.0.1") 可以指定监听特定的IP地址
    address.sin_addr.s_addr = INADDR_ANY; // 或 inet_addr("127.0.0.1");
    address.sin_port = htons(PORT); // 端口号转换为网络字节序

    // 2. 将套接字绑定到指定的IP地址和端口
    // 注意：将(struct sockaddr *)&address进行类型转换
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Server: Socket bound to port %d.\n", PORT);

    // 3. 监听传入连接
    // 10: 允许的最大等待连接队列长度
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server: Listening on 0.0.0.0:%d...\n", PORT);

    // 4. 接受客户端连接
    // 阻塞等待，直到有客户端连接
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    printf("Server: Client connected.\n");

    // 5. 接收客户端消息
    // read()函数从已连接套接字读取数据
    ssize_t valread = read(new_socket, buffer, MAX_BUFFER_SIZE);
    if (valread < 0) {
        perror("read failed");
    } else if (valread == 0) {
        printf("Server: Client disconnected.\n");
    } else {
        printf("Server: Received from client: %s\n", buffer);
    }
    
    // 清空缓冲区，避免下次读取混淆
    memset(buffer, 0, MAX_BUFFER_SIZE);

    // 6. 向客户端发送消息
    // send()函数向已连接套接字发送数据
    if (send(new_socket, hello, strlen(hello), 0) < 0) {
        perror("send failed");
    } else {
        printf("Server: Sent to client: %s\n", hello);
    }

    // 7. 关闭已连接套接字
    close(new_socket);
    printf("Server: Client socket closed.\n");

    // 8. 关闭监听套接字
    close(server_fd);
    printf("Server: Listening socket closed. Server shutting down.\n");

    return 0;
}
```

**编译服务器代码:**
```bash
gcc server.c -o server
```

---

### 2. 简单的TCP客户端 (client.c)

这个客户端会连接到`127.0.0.1:8080`的服务器，发送一条消息，然后接收服务器的回复。

```c
#include <stdio.h>          // 标准输入输出
#include <stdlib.h>         // 标准库函数
#include <string.h>         // 字符串处理
#include <unistd.h>         // Unix标准函数

#include <sys/socket.h>     // socket()函数和相关数据结构
#include <arpa/inet.h>      // inet_pton()函数
#include <netinet/in.h>     // sockaddr_in结构体

#define PORT 8080           // 服务器端口
#define SERVER_IP "127.0.0.1" // 服务器IP地址
#define MAX_BUFFER_SIZE 1024 // 缓冲区大小

int main() {
    int sock = 0; // 客户端套接字
    struct sockaddr_in serv_addr; // 服务器地址结构
    char buffer[MAX_BUFFER_SIZE] = {0}; // 数据缓冲区
    char *message = "Hello from client!"; // 客户端发送消息

    // 1. 创建套接字文件描述符
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation error");
        exit(EXIT_FAILURE);
    }
    printf("Client: Socket created successfully.\n");

    // 准备服务器地址结构
    serv_addr.sin_family = AF_INET; // IPv4
    serv_addr.sin_port = htons(PORT); // 端口号转换为网络字节序

    // 将IP地址从字符串转换为网络字节序的二进制形式
    // inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) 可以处理点分十进制的IP字符串
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }
    printf("Client: Server address prepared.\n");

    // 2. 连接到服务器
    // connect()函数会尝试与服务器建立连接
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
    printf("Client: Connected to server %s:%d.\n", SERVER_IP, PORT);

    // 3. 向服务器发送消息
    // send()函数向已连接套接字发送数据
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("send failed");
    } else {
        printf("Client: Sent to server: %s\n", message);
    }

    // 4. 接收服务器回复
    // read()函数从已连接套接字读取数据
    ssize_t valread = read(sock, buffer, MAX_BUFFER_SIZE);
    if (valread < 0) {
        perror("read failed");
    } else if (valread == 0) {
        printf("Client: Server disconnected.\n");
    } else {
        printf("Client: Received from server: %s\n", buffer);
    }

    // 5. 关闭套接字
    close(sock);
    printf("Client: Socket closed. Client exiting.\n");

    return 0;
}
```

**编译客户端代码:**
```bash
gcc client.c -o client
```

---

### 3. 如何运行

1.  **打开第一个终端窗口，运行服务器：**
    ```bash
    ./server
    ```
    你应该会看到类似这样的输出：
    ```
    Server: Socket created successfully.
    Server: Socket options set.
    Server: Socket bound to port 8080.
    Server: Listening on 0.0.0.0:8080...
    ```
    服务器现在正在监听，等待客户端连接。

2.  **打开第二个终端窗口，运行客户端：**
    ```bash
    ./client
    ```
    客户端的输出：
    ```
    Client: Socket created successfully.
    Client: Server address prepared.
    Client: Connected to server 127.0.0.1:8080.
    Client: Sent to server: Hello from client!
    Client: Received from server: Hello from server!
    Client: Socket closed. Client exiting.
    ```

3.  **回到服务器终端，你会看到：**
    ```
    Server: Client connected.
    Server: Received from client: Hello from client!
    Server: Sent to client: Hello from server!
    Server: Client socket closed.
    Server: Listening socket closed. Server shutting down.
    ```
    服务器在处理完一个客户端后就关闭了。这是一个单连接服务器的例子。

---

### 代码解释与注意事项：

*   **头文件:** 包含了所有必需的系统调用和数据结构。
*   **`perror()` 和 `exit()`:** 在系统调用失败时，`perror()`会打印错误信息，`exit(EXIT_FAILURE)`会终止程序。这是C语言中处理错误的常见方式。
*   **`setsockopt(SO_REUSEADDR | SO_REUSEPORT)`:** 这两个选项非常重要。`SO_REUSEADDR`允许你绑定一个处于`TIME_WAIT`状态的地址，这在服务器快速重启时非常有用，避免了“Address already in use”错误。`SO_REUSEPORT`（Linux 3.9+）允许多个套接字绑定到相同的地址和端口，这在某些高性能服务器设计中（如多进程或多线程服务器）很有用。
*   **`INADDR_ANY`:** 表示服务器将监听所有可用的本地IP地址上的`PORT`端口。如果你有多个网卡，它会监听所有网卡上的请求。对于客户端，通常你需要指定服务器的具体IP地址。
*   **字节序转换 (`htons()`, `htonl()`, `inet_pton()`):** 再次强调，这些函数用于将主机字节序的整数值（如端口号）和IP地址转换为网络字节序，反之亦然。这是确保不同系统间数据正确交换的关键。`inet_pton()`比老旧的`inet_addr()`更好，因为它支持IPv6，并且可以正确处理错误。
*   **`read()` / `send()`:** 在这个简单的例子中，我们使用了`read()`和`send()`。它们在TCP套接字上的行为与`recv(sockfd, buf, len, 0)`和`send(sockfd, buf, len, 0)`基本相同。
*   **`close(new_socket)` 和 `close(server_fd)`:** 服务器端在处理完客户端请求后，会先关闭与该客户端通信的`new_socket`。然后，在服务器打算停止服务时，才会关闭监听套接字`server_fd`。
*   **单连接服务器:** 这个例子中的服务器只能处理一个客户端连接。一旦处理完一个客户端，它就关闭并退出了。在实际应用中，服务器通常需要同时处理多个客户端连接，这需要用到多进程（`fork()`)、多线程（`pthread`）或者I/O多路复用（`select`, `poll`, `epoll`）技术。

---

### 下一步的思考：

这个例子是网络编程的“Hello World”。它展示了最基础的连接建立和数据传输。但实际的网络应用要复杂得多。下次我们将探讨：

*   **并发服务器设计:** 如何让服务器同时处理多个客户端？我们将讨论`fork()`和`pthread`（多进程和多线程）的基本方法。
*   **I/O多路复用:** 解决C10K问题的关键技术——`select()`, `poll()`, `epoll()`。

请消化这些代码，尝试修改端口号，或者修改发送/接收的消息，加深理解。我们下次再见！