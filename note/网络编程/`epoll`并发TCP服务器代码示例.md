好的，我们来进一步巩固对`epoll`的理解，通过一个实际的并发TCP服务器代码示例来演示其用法。

### 第六次讲解：`epoll`并发TCP服务器代码示例

这个例子将实现一个单线程的、基于`epoll`的并发TCP服务器。它能够同时处理多个客户端连接，接收它们发送的消息，并回复一个简单的响应。

**代码目标:**
*   创建一个监听套接字，并将其添加到`epoll`实例中。
*   主循环中使用`epoll_wait`等待事件。
*   如果监听套接字有事件，表示有新连接到来，`accept`并将其添加到`epoll`实例中。
*   如果已连接套接字有事件，表示有数据可读，`read`数据并回复。
*   处理客户端断开连接的情况。

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h> // For errno
#include <fcntl.h> // For fcntl and O_NONBLOCK

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h> // epoll 头文件

#define PORT 8080
#define MAX_EVENTS 10 // epoll_wait一次最多返回的事件数量
#define MAX_BUFFER_SIZE 1024

// 设置文件描述符为非阻塞模式
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL)");
        return -1;
    }
    return 0;
}

int main() {
    int listen_sock, conn_sock; // 监听套接字和已连接套接字
    struct sockaddr_in server_addr, client_addr; // 服务器和客户端地址结构
    socklen_t client_addr_len;
    int epoll_fd; // epoll 实例的文件描述符
    struct epoll_event event; // 用于注册事件的结构体
    struct epoll_event events[MAX_EVENTS]; // 存储epoll_wait返回的就绪事件

    char buffer[MAX_BUFFER_SIZE];

    // 1. 创建监听套接字
    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    printf("Server: Listening socket created successfully.\n");

    // 允许地址和端口复用，避免"Address already in use"
    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }
    printf("Server: Socket options set.\n");

    // 设置监听套接字为非阻塞模式 (虽然在LT模式下不是强制，但养成习惯更好，尤其是在ET模式下是必须的)
    if (set_nonblocking(listen_sock) == -1) {
        close(listen_sock);
        exit(EXIT_FAILURE);
    }
    printf("Server: Listening socket set to non-blocking.\n");

    // 准备服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有可用IP
    server_addr.sin_port = htons(PORT); // 端口号

    // 2. 绑定套接字
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }
    printf("Server: Socket bound to port %d.\n", PORT);

    // 3. 开始监听
    if (listen(listen_sock, SOMAXCONN) == -1) { // SOMAXCONN是系统定义的连接队列最大值
        perror("listen failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }
    printf("Server: Listening on 0.0.0.0:%d...\n", PORT);

    // 4. 创建epoll实例
    epoll_fd = epoll_create1(0); // epoll_create1(0) 等同于 epoll_create(size)，但推荐使用
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }
    printf("Server: Epoll instance created with fd %d.\n", epoll_fd);

    // 5. 将监听套接字添加到epoll实例中，关注EPOLLIN事件 (可读事件，表示有新连接到来)
    event.events = EPOLLIN; // 关注可读事件
    event.data.fd = listen_sock; // 将监听套接字添加到data中
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event) == -1) {
        perror("epoll_ctl: EPOLL_CTL_ADD listen_sock failed");
        close(listen_sock);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server: Listening socket added to epoll.\n");

    // 主事件循环
    printf("Server: Entering main event loop...\n");
    while (1) {
        // 6. 等待I/O事件发生
        // 阻塞等待，直到有事件发生，或者超时(这里是-1，表示永远阻塞)
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            if (errno == EINTR) { // 被信号中断，继续循环
                continue;
            }
            perror("epoll_wait failed");
            break; // 出现其他错误则退出
        }

        // 7. 处理所有就绪事件
        for (int i = 0; i < num_events; i++) {
            // 如果是监听套接字上的事件，表示有新客户端连接
            if (events[i].data.fd == listen_sock) {
                client_addr_len = sizeof(client_addr);
                conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
                if (conn_sock == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 如果监听套接字是非阻塞的，并且没有新连接了，会返回这些错误码
                        // 可以忽略，或记录日志
                        // printf("Server: accept returned EAGAIN/EWOULDBLOCK.\n");
                        continue;
                    } else {
                        perror("accept failed");
                        continue;
                    }
                }

                // 打印客户端信息
                printf("Server: New client connected from %s:%d (fd: %d)\n",
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), conn_sock);

                // 将新连接的套接字设置为非阻塞模式
                if (set_nonblocking(conn_sock) == -1) {
                    close(conn_sock);
                    continue;
                }

                // 将新连接的套接字添加到epoll实例中，关注EPOLLIN事件
                event.events = EPOLLIN; // 关注可读事件
                event.data.fd = conn_sock;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &event) == -1) {
                    perror("epoll_ctl: EPOLL_CTL_ADD conn_sock failed");
                    close(conn_sock);
                }

            } else { // 否则，是已连接套接字上的事件 (通常是数据可读)
                int current_sock = events[i].data.fd; // 获取当前发生事件的套接字

                // 检查是否是EPOLLIN (可读事件)
                if (events[i].events & EPOLLIN) {
                    memset(buffer, 0, MAX_BUFFER_SIZE); // 清空缓冲区
                    ssize_t bytes_read = read(current_sock, buffer, MAX_BUFFER_SIZE);

                    if (bytes_read == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 非阻塞I/O下，表示暂时没有数据可读了
                            // 如果是ET模式，需要循环读直到这个错误
                            printf("Server (fd: %d): No more data to read (EAGAIN/EWOULDBLOCK).\n", current_sock);
                        } else {
                            perror("read failed");
                            // 发生错误，关闭连接
                            printf("Server: Closing connection for fd %d due to read error.\n", current_sock);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_sock, NULL);
                            close(current_sock);
                        }
                    } else if (bytes_read == 0) {
                        // 客户端关闭了连接 (FIN 包)
                        printf("Server (fd: %d): Client disconnected gracefully.\n", current_sock);
                        // 从epoll实例中移除并关闭套接字
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_sock, NULL);
                        close(current_sock);
                    } else {
                        // 成功读取到数据
                        printf("Server (fd: %d): Received: %s\n", current_sock, buffer);
                        // 回复客户端
                        char *response = "Hello from epoll server!";
                        if (send(current_sock, response, strlen(response), 0) == -1) {
                            perror("send failed");
                            printf("Server: Closing connection for fd %d due to send error.\n", current_sock);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_sock, NULL);
                            close(current_sock);
                        } else {
                            printf("Server (fd: %d): Sent: %s\n", current_sock, response);
                        }
                    }
                }
                
                // 检查其他事件类型，例如EPOLLOUT (可写) 或错误
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    printf("Server (fd: %d): Error or hangup event occurred. Closing connection.\n", current_sock);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_sock, NULL);
                    close(current_sock);
                }
            }
        }
    }

    // 清理资源
    close(listen_sock);
    close(epoll_fd);
    printf("Server: Shutting down.\n");

    return 0;
}
```

**编译命令:**
```bash
gcc epoll_server.c -o epoll_server
```

**运行方式:**
1.  **在第一个终端运行服务器:** `./epoll_server`
2.  **在第二个、第三个...终端运行客户端:** (使用上次讲解的`client.c`，或自己编写一个简单的TCP客户端连接到`127.0.0.1:8080`)
    `./client`
    你可以多开几个客户端终端，同时连接服务器，观察服务器是如何并发处理的。

---

### 代码解释与注意事项：

1.  **`set_nonblocking(int fd)` 函数:**
    *   这是一个辅助函数，用于将给定的文件描述符设置为非阻塞模式。
    *   **重要性:** 虽然在`epoll`的**水平触发(LT)**模式下，监听套接字和已连接套接字不强制要求非阻塞，但在实践中，为了避免`accept()`、`read()`、`write()`等操作阻塞整个事件循环，**强烈建议将所有添加到`epoll`的套接字设置为非阻塞模式。**尤其是在使用**边缘触发(ET)**模式时，非阻塞是**必须的**。

2.  **`epoll_create1(0)`:**
    *   创建一个新的`epoll`实例，返回一个文件描述符`epoll_fd`。这个FD代表了内核中维护的事件表。

3.  **`epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event)`:**
    *   将监听套接字`listen_sock`添加到`epoll_fd`所管理的事件列表中。
    *   `event.events = EPOLLIN;` 表示我们关注`listen_sock`上的**可读事件**。对于监听套接字，这意味着有新的客户端连接请求。
    *   `event.data.fd = listen_sock;` 将套接字本身作为用户数据存储在`epoll_event`结构中，方便`epoll_wait`返回后识别。

4.  **`epoll_wait(epoll_fd, events, MAX_EVENTS, -1)`:**
    *   这是`epoll`的核心循环。它会阻塞，直到`epoll_fd`管理的某个或某些FD上发生我们感兴趣的事件。
    *   `-1` 表示永远阻塞，直到有事件发生。可以设置为`0`实现非阻塞轮询，或指定一个正数进行超时等待。
    *   `events` 数组用于存储所有就绪的事件，`num_events` 是实际返回的就绪事件数量。

5.  **事件处理循环:** `for (int i = 0; i < num_events; i++)`
    *   遍历`epoll_wait`返回的所有就绪事件。

6.  **`if (events[i].data.fd == listen_sock)` (处理新连接):**
    *   如果事件发生在监听套接字上，说明有新的客户端请求连接。
    *   `accept()`: 接受新连接，返回一个新的已连接套接字`conn_sock`。
    *   **将`conn_sock`设置为非阻塞：** 这非常重要，否则如果`read`或`send`阻塞，整个事件循环将停滞。
    *   **将`conn_sock`添加到`epoll`：** `epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &event);` 同样关注其`EPOLLIN`事件，以便后续可以读取客户端发送的数据。

7.  **`else` (处理已连接客户端数据):**
    *   如果事件发生在已连接套接字上，通常表示有数据可读。
    *   `read()`: 从`current_sock`读取数据到`buffer`。
    *   **`bytes_read == 0`:** 这表示客户端优雅地关闭了连接（发送了FIN包）。此时，服务器应该从`epoll`实例中移除该FD并关闭它。
    *   **`bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)`:** 在非阻塞模式下，如果`read()`返回-1且`errno`是`EAGAIN`或`EWOULDBLOCK`，表示缓冲区暂时没有数据了（或者所有数据已被读取完毕）。这是正常情况，尤其是在边缘触发模式下，你需要一直读到这个错误。
    *   **`bytes_read == -1` (其他错误):** 发生了其他读取错误，也应该关闭连接。
    *   **`bytes_read > 0`:** 成功读取到数据，进行处理并回复。
    *   `send()`: 向客户端发送回复。发送失败也需要关闭连接。

8.  **错误和挂断事件 (`EPOLLERR | EPOLLHUP`):**
    *   `epoll_wait`也可能返回`EPOLLERR`（错误）或`EPOLLHUP`（挂断，通常表示对端关闭了连接的写半部分）。当这些事件发生时，通常也意味着连接已不再可用，需要从`epoll`中移除并关闭套接字。

9.  **资源清理:**
    *   在程序退出前，记得关闭监听套接字和`epoll_fd`。

---

### `epoll` 与 `LT` (水平触发) 模式：

这个示例使用的是`epoll`的默认模式——水平触发 (LT)。在LT模式下：
*   只要文件描述符准备好（例如，缓冲区有数据可读），`epoll_wait()`就会一直报告它可读。
*   这意味着你不需要一次性读取所有数据。每次`epoll_wait`返回后，你只需读取一部分数据，下次`epoll_wait`还会告诉你它可读，直到数据全部读完。
*   这种模式编程相对简单，但可能会有少量重复事件通知。

如果你想使用**边缘触发 (ET)**模式，你需要在`epoll_ctl`中将`event.events`设置为`EPOLLIN | EPOLLET`。但是，在ET模式下，你需要：
1.  **所有套接字必须是非阻塞的。**
2.  **当事件发生时，必须循环地读取/写入数据，直到`read()`/`write()`返回`EAGAIN`或`EWOULDBLOCK`。** 这是因为ET模式只通知一次状态变化，如果你不把所有数据都处理完，下次`epoll_wait`将不会再通知你。

这个`epoll`服务器示例展示了如何高效地在一个线程中处理大量并发连接。它是构建高性能网络应用（如Web服务器、消息队列）的基础。

接下来，我们可以探讨一些更高级的网络编程话题，或者更深入地研究其他方面，例如：

*   **UDP网络编程** (虽然TCP是主流，但UDP在特定场景下很有用)。
*   **非阻塞I/O的更深入探讨** (包括何时以及如何使用`write`的非阻塞特性)。
*   **套接字选项 (`getsockopt`/`setsockopt`) 的高级用法**。
*   **优雅关闭连接 (Linger 选项，半关闭)**。
*   **应用层协议与序列化** (数据如何编码/解码)。
*   **线程池/进程池与`epoll`的结合**。
*   **并发编程中的同步与互斥** (如果在一个进程内处理复杂逻辑)。

你更希望我们接下来探讨哪个主题呢？