好的，我们继续深入Linux系统编程，这次我们将聚焦于除了套接字之外的，更加通用的**进程间通信 (IPC)** 机制。这些机制对于构建复杂的分布式系统和多进程应用程序至关重要。

### 第十三次讲解：更复杂的IPC机制 - 管道、消息队列、System V IPC

IPC机制允许相互独立的进程进行数据交换。我们之前探讨的Unix域套接字也是一种IPC方式，但这里我们将介绍其他几种传统且常用的IPC方式。

### 1. 管道 (Pipes)

管道是Unix/Linux中最古老、最简单的一种IPC形式。它本质上是一个内核缓冲区，用于连接两个进程，一个进程写入，另一个进程读取。

#### 1.1 匿名管道 (Anonymous Pipes)

匿名管道是无名的，通常用于**父子进程之间**进行单向通信。它们随进程创建而存在，随进程结束而消失。

*   **创建:** `int pipe(int pipefd[2]);`
    *   成功返回0，并将两个文件描述符存入`pipefd`数组：`pipefd[0]`用于读取，`pipefd[1]`用于写入。
    *   通常，父进程创建管道后，`fork()`子进程。父子进程都会继承这两个文件描述符。然后父进程关闭不需要的读端（或写端），子进程关闭不需要的写端（或读端），从而建立单向通信。
*   **通信:** 使用`read()`和`write()`读写数据。
*   **特性:**
    *   **单向通信:** 数据只能从写端流向读端。
    *   **半双工:** 虽然管道是单向的，但通过创建两个管道（一个用于父到子，一个用于子到父），可以实现双向通信（但不是全双工）。
    *   **缓冲区:** 管道有固定的缓冲区大小（通常几KB，例如64KB），如果写端写入的数据超过缓冲区容量，写操作会阻塞；如果读端读取时没有数据，读操作会阻塞。
    *   **面向字节流:** 没有消息边界。

**示例：父子进程通过匿名管道通信**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For pipe(), fork(), read(), write(), close()
#include <sys/wait.h> // For wait()

int main() {
    int pipefd[2]; // pipefd[0] for read, pipefd[1] for write
    pid_t pid;
    char buffer[256];
    const char *parent_msg = "Hello from parent!";
    const char *child_msg = "Hello from child!";

    // 创建管道
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }
    printf("Pipe created: read_fd = %d, write_fd = %d\n", pipefd[0], pipefd[1]);

    pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // 子进程
        close(pipefd[1]); // 子进程关闭写端 (只读)
        printf("Child process (PID: %d): Reading from pipe...\n", getpid());
        ssize_t bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Child process: Received: %s\n", buffer);
        } else if (bytes_read == 0) {
            printf("Child process: Parent closed pipe.\n");
        } else {
            perror("child read failed");
        }
        close(pipefd[0]); // 关闭读端
        exit(EXIT_SUCCESS);
    } else { // 父进程
        close(pipefd[0]); // 父进程关闭读端 (只写)
        printf("Parent process (PID: %d): Sending to pipe...\n", getpid());
        if (write(pipefd[1], parent_msg, strlen(parent_msg)) == -1) {
            perror("parent write failed");
        }
        printf("Parent process: Sent: %s\n", parent_msg);
        close(pipefd[1]); // 关闭写端 (发送EOF给子进程)
        wait(NULL); // 等待子进程结束
        printf("Parent process: Child finished.\n");
        exit(EXIT_SUCCESS);
    }
}
```
**编译:** `gcc -o anon_pipe anon_pipe.c`

#### 1.2 命名管道 (Named Pipes / FIFO)

命名管道（或称为FIFO，First-In, First-Out）通过文件系统中的一个特殊文件来表示。它们允许**任意不相关的进程**进行通信。

*   **创建:** `int mkfifo(const char *pathname, mode_t mode);`
    *   在文件系统中创建一个FIFO文件。`pathname`是文件路径，`mode`是文件权限。
    *   **注意:** FIFO文件只是一个名称，实际数据仍存储在内核缓冲区，不占用磁盘空间。
*   **通信:** 进程通过`open()`打开FIFO文件，然后使用`read()`和`write()`进行通信。
*   **特性:**
    *   **持久性:** FIFO文件一旦创建，除非显式删除（`unlink()`），否则一直存在。
    *   **单向通信:** 默认仍是单向，但可以通过两个FIFO文件实现双向。
    *   **阻塞性:** 默认情况下，打开FIFO会阻塞，直到另一个进程也打开了FIFO的另一端。`read()`和`write()`也是阻塞的。可以通过`O_NONBLOCK`标志设置为非阻塞。

**示例：通过命名管道通信 (需要两个独立程序)**

**fifo_writer.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // For open()
#include <sys/stat.h> // For mkfifo()
#include <unistd.h> // For write(), close()

#define FIFO_PATH "/tmp/my_fifo"
#define MSG_SIZE 256

int main() {
    int fd;
    const char *message = "Hello from FIFO writer!";

    // 创建FIFO文件 (如果不存在)
    mkfifo(FIFO_PATH, 0666); // 0666 表示 rw-rw-rw- 权限

    printf("FIFO Writer: Opening FIFO '%s' for writing...\n", FIFO_PATH);
    fd = open(FIFO_PATH, O_WRONLY); // 以写模式打开FIFO
    if (fd == -1) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }
    printf("FIFO Writer: FIFO opened, writing message...\n");

    if (write(fd, message, strlen(message) + 1) == -1) { // +1 for null terminator
        perror("write failed");
    } else {
        printf("FIFO Writer: Sent: %s\n", message);
    }

    close(fd);
    printf("FIFO Writer: FIFO closed.\n");
    // unlink(FIFO_PATH); // 可以选择在结束后删除FIFO文件
    return 0;
}
```

**fifo_reader.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIFO_PATH "/tmp/my_fifo"
#define MSG_SIZE 256

int main() {
    int fd;
    char buffer[MSG_SIZE];

    // 创建FIFO文件 (如果不存在)
    mkfifo(FIFO_PATH, 0666);

    printf("FIFO Reader: Opening FIFO '%s' for reading...\n", FIFO_PATH);
    fd = open(FIFO_PATH, O_RDONLY); // 以读模式打开FIFO
    if (fd == -1) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }
    printf("FIFO Reader: FIFO opened, reading message...\n");

    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        printf("FIFO Reader: Received: %s\n", buffer);
    } else if (bytes_read == 0) {
        printf("FIFO Reader: Writer closed FIFO.\n");
    } else {
        perror("read failed");
    }

    close(fd);
    printf("FIFO Reader: FIFO closed.\n");
    unlink(FIFO_PATH); // 通常由读取方在结束后删除FIFO文件
    return 0;
}
```
**编译:** `gcc -o fifo_writer fifo_writer.c` 和 `gcc -o fifo_reader fifo_reader.c`
**运行:** 先运行 `./fifo_reader` (它会阻塞等待写端)，然后在另一个终端运行 `./fifo_writer`。

### 2. 消息队列 (Message Queues)

消息队列是操作系统维护的一个消息链表，允许进程以**消息**的形式发送和接收数据，每个消息都有一个特定的类型。消息队列提供了比管道更结构化的通信方式。

在Linux中，有两种主要的消息队列实现：

#### 2.1 POSIX 消息队列

POSIX消息队列是较新且推荐的接口，通常与线程和`select`/`poll`/`epoll`兼容性更好。它们以`/name`形式命名。

*   **头文件:** `<mqueue.h>`
*   **创建/打开:** `mqd_t mq_open(const char *name, int oflag, ...)`
    *   `name`: 队列名称，以`/`开头。
    *   `oflag`: `O_RDWR`, `O_CREAT`, `O_EXCL`等。
    *   可选参数：`mode_t mode` (权限), `struct mq_attr *attr` (属性)。
*   **发送:** `int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, unsigned int msg_prio);`
*   **接收:** `ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, unsigned int *msg_prio);`
*   **关闭:** `int mq_close(mqd_t mqdes);`
*   **删除:** `int mq_unlink(const char *name);` (在进程退出时通常需要删除)
*   **特性:**
    *   **消息边界:** 消息是独立的，有明确边界。
    *   **消息优先级:** 可以为消息指定优先级，高优先级消息优先被接收。
    *   **可持久化:** 队列可以一直存在，直到显式删除。
    *   **非阻塞/超时:** 可以设置为非阻塞模式或带超时接收。

**示例：POSIX 消息队列 (简版)**

**posix_mq_sender.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h> // For POSIX message queues
#include <fcntl.h>   // For O_RDWR, O_CREAT
#include <sys/stat.h> // For S_IRUSR, S_IWUSR
#include <unistd.h>

#define MQ_NAME "/my_posix_mq"
#define MAX_MSG_SIZE 256
#define MQ_MAX_MSGS 10

int main() {
    mqd_t mq;
    struct mq_attr attr;
    const char *message = "Hello from POSIX MQ Sender!";

    // 配置消息队列属性
    attr.mq_flags = 0; // 阻塞模式
    attr.mq_maxmsg = MQ_MAX_MSGS;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    // 打开/创建消息队列
    mq = mq_open(MQ_NAME, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(EXIT_FAILURE);
    }
    printf("POSIX MQ Sender: Opened message queue '%s'.\n", MQ_NAME);

    // 发送消息 (优先级为1)
    if (mq_send(mq, message, strlen(message) + 1, 1) == -1) {
        perror("mq_send failed");
    } else {
        printf("POSIX MQ Sender: Sent message: '%s' with priority 1.\n", message);
    }

    // 关闭消息队列
    mq_close(mq);
    printf("POSIX MQ Sender: Message queue closed.\n");

    // 不在这里unlink，由接收方unlink
    return 0;
}```

**posix_mq_receiver.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define MQ_NAME "/my_posix_mq"
#define MAX_MSG_SIZE 256

int main() {
    mqd_t mq;
    char buffer[MAX_MSG_SIZE];
    unsigned int priority;
    struct mq_attr attr;

    // 打开消息队列
    mq = mq_open(MQ_NAME, O_RDONLY); // 接收方只需读
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(EXIT_FAILURE);
    }
    printf("POSIX MQ Receiver: Opened message queue '%s'.\n", MQ_NAME);

    // 获取消息队列属性，以知道最大消息大小
    if (mq_getattr(mq, &attr) == -1) {
        perror("mq_getattr failed");
        mq_close(mq);
        mq_unlink(MQ_NAME);
        exit(EXIT_FAILURE);
    }
    printf("POSIX MQ Receiver: Max message size: %ld bytes.\n", attr.mq_msgsize);

    // 接收消息
    ssize_t bytes_read = mq_receive(mq, buffer, attr.mq_msgsize, &priority);
    if (bytes_read == -1) {
        perror("mq_receive failed");
    } else {
        printf("POSIX MQ Receiver: Received message: '%s' (Priority: %u, Length: %zd bytes)\n",
               buffer, priority, bytes_read);
    }

    // 关闭并删除消息队列
    mq_close(mq);
    if (mq_unlink(MQ_NAME) == -1) {
        perror("mq_unlink failed");
    } else {
        printf("POSIX MQ Receiver: Message queue '%s' closed and unlinked.\n", MQ_NAME);
    }

    return 0;
}
```
**编译:** `gcc -o posix_mq_sender posix_mq_sender.c -lrt` 和 `gcc -o posix_mq_receiver posix_mq_receiver.c -lrt` (注意：`mqueue.h`的函数通常需要链接`librt`库)
**运行:** 先运行 `./posix_mq_receiver` (它会阻塞等待)，然后在另一个终端运行 `./posix_mq_sender`。

#### 2.2 System V 消息队列

System V消息队列是Unix/Linux早期提供的IPC机制，使用整数ID来标识队列。

*   **头文件:** `<sys/msg.h>`, `<sys/ipc.h>`
*   **创建/获取:** `int msgget(key_t key, int msgflg);` (通过键值`key`获取队列ID)
*   **发送:** `int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);`
*   **接收:** `ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);`
*   **控制/删除:** `int msgctl(int msqid, int cmd, struct msqid_ds *buf);` (使用`IPC_RMID`删除)

**特点:** 相对POSIX MQ更复杂，通常推荐使用POSIX MQ，除非需要与遗留系统兼容。

### 3. System V IPC (System V Interprocess Communication)

System V IPC是一个IPC家族，包括三类机制：

*   **System V 消息队列 (System V Message Queues):** 上面已简要介绍。
*   **System V 信号量 (System V Semaphores):**
    *   **目的:** 用于进程间的同步和互斥。它与POSIX信号量类似，但更复杂，因为它可以操作一个信号量集，而不是单个信号量。
    *   **头文件:** `<sys/sem.h>`, `<sys/ipc.h>`
    *   **创建/获取:** `int semget(key_t key, int nsems, int semflg);`
    *   **操作:** `int semop(int semid, struct sembuf *sops, size_t nsops);` (可以同时执行多个P/V操作)
    *   **控制/删除:** `int semctl(int semid, int semnum, int cmd, ...);`

*   **System V 共享内存 (System V Shared Memory):**
    *   **目的:** 允许多个进程共享一块物理内存区域。这是**最快的IPC方式**，因为一旦内存映射完成，进程就可以直接读写这块内存，无需通过内核进行数据拷贝。
    *   **头文件:** `<sys/shm.h>`, `<sys/ipc.h>`
    *   **基本操作:**
        *   **创建/获取:** `int shmget(key_t key, size_t size, int shmflg);`
            *   `key`: 共享内存段的键值。
            *   `size`: 共享内存的大小。
        *   **连接到进程地址空间:** `void *shmat(int shmid, const void *shmaddr, int shmflg);`
            *   将共享内存段连接到调用进程的地址空间。
        *   **分离:** `int shmdt(const void *shmaddr);`
            *   将共享内存从进程的地址空间分离。
        *   **控制/删除:** `int shmctl(int shmid, int cmd, struct shmid_ds *buf);`
            *   使用`IPC_RMID`命令删除共享内存段。
    *   **特性:**
        *   **高速:** 直接内存访问，无数据拷贝。
        *   **需要同步:** 共享内存本身不提供同步机制，需要结合信号量或互斥锁（放置在共享内存中）来保护共享数据的访问。

**示例：System V 共享内存 (配合信号量或互斥锁使用)**

**shm_writer.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h> // For ftok
#include <sys/shm.h> // For shmget, shmat, shmdt, shmctl
#include <sys/sem.h> // For System V semaphores (optional, but good for synchronization)

#define SHM_KEY 1234
#define SHM_SIZE 1024
#define SEM_KEY 5678

// Union for semctl (required by some versions)
union semun {
    int              val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};


int main() {
    int shm_id;
    char *shm_ptr;
    int sem_id;

    // 1. 获取共享内存段ID
    shm_id = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }
    printf("SHM Writer: Shared memory ID: %d\n", shm_id);

    // 2. 连接共享内存到进程地址空间
    shm_ptr = (char *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat failed");
        shmctl(shm_id, IPC_RMID, NULL); // 清理
        exit(EXIT_FAILURE);
    }
    printf("SHM Writer: Shared memory attached at %p\n", shm_ptr);

    // --- 使用System V信号量进行同步 (可选但推荐) ---
    // 获取信号量集ID (创建或获取)
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666); // 1个信号量
    if (sem_id == -1) {
        perror("semget failed");
        shmdt(shm_ptr);
        shmctl(shm_id, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }
    printf("SHM Writer: Semaphore ID: %d\n", sem_id);

    // 初始化信号量为1 (表示资源可用)
    union semun arg;
    arg.val = 1;
    if (semctl(sem_id, 0, SETVAL, arg) == -1) { // 0是信号量在集合中的索引
        perror("semctl SETVAL failed");
        shmdt(shm_ptr);
        shmctl(shm_id, IPC_RMID, NULL);
        semctl(sem_id, 0, IPC_RMID); // 清理信号量
        exit(EXIT_FAILURE);
    }
    printf("SHM Writer: Semaphore initialized to 1.\n");

    // 3. 写入数据到共享内存
    struct sembuf sb; // 信号量操作结构体
    sb.sem_num = 0;   // 操作第0个信号量
    sb.sem_flg = SEM_UNDO; // 进程结束时自动撤销操作

    sb.sem_op = -1; // P操作 (等待，减1)
    if (semop(sem_id, &sb, 1) == -1) { // 1表示操作数量
        perror("semop P failed");
        shmdt(shm_ptr); shmctl(shm_id, IPC_RMID, NULL); semctl(sem_id, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }
    printf("SHM Writer: Acquired semaphore.\n");

    const char *message = "Hello from shared memory writer!";
    strncpy(shm_ptr, message, SHM_SIZE - 1);
    shm_ptr[SHM_SIZE - 1] = '\0'; // 确保null终止
    printf("SHM Writer: Wrote '%s' to shared memory.\n", shm_ptr);

    sb.sem_op = 1; // V操作 (释放，加1)
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop V failed");
    }
    printf("SHM Writer: Released semaphore.\n");

    // 4. 分离共享内存
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
    }
    printf("SHM Writer: Shared memory detached.\n");

    // 通常由读取方或守护进程负责清理共享内存和信号量
    // shmctl(shm_id, IPC_RMID, NULL);
    // semctl(sem_id, 0, IPC_RMID);
    return 0;
}
```

**shm_reader.c**

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_KEY 1234
#define SHM_SIZE 1024
#define SEM_KEY 5678

union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
    struct seminfo  *__buf;
};

int main() {
    int shm_id;
    char *shm_ptr;
    int sem_id;

    // 1. 获取共享内存段ID
    shm_id = shmget(SHM_KEY, SHM_SIZE, 0666); // 注意: 不加IPC_CREAT
    if (shm_id == -1) {
        perror("shmget failed (ensure writer runs first)");
        exit(EXIT_FAILURE);
    }
    printf("SHM Reader: Shared memory ID: %d\n", shm_id);

    // 2. 连接共享内存到进程地址空间
    shm_ptr = (char *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (char *)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }
    printf("SHM Reader: Shared memory attached at %p\n", shm_ptr);

    // --- 使用System V信号量进行同步 ---
    sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id == -1) {
        perror("semget failed (ensure writer runs first)");
        shmdt(shm_ptr);
        exit(EXIT_FAILURE);
    }
    printf("SHM Reader: Semaphore ID: %d\n", sem_id);

    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_flg = SEM_UNDO;

    // 3. 读取数据 (等待信号量，确保数据已写入)
    sb.sem_op = -1; // P操作 (等待，减1)
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop P failed");
        shmdt(shm_ptr);
        exit(EXIT_FAILURE);
    }
    printf("SHM Reader: Acquired semaphore.\n");

    printf("SHM Reader: Read from shared memory: '%s'\n", shm_ptr);

    sb.sem_op = 1; // V操作 (释放，加1)
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop V failed");
    }
    printf("SHM Reader: Released semaphore.\n");

    // 4. 分离共享内存
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt failed");
    }
    printf("SHM Reader: Shared memory detached.\n");

    // 5. 清理共享内存和信号量 (通常由其中一个进程或独立的清理脚本完成)
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID failed");
    } else {
        printf("SHM Reader: Shared memory segment removed.\n");
    }

    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID failed");
    } else {
        printf("SHM Reader: Semaphore set removed.\n");
    }

    return 0;
}
```
**编译:** `gcc -o shm_writer shm_writer.c` 和 `gcc -o shm_reader shm_reader.c`
**运行:** 先运行 `./shm_writer`，然后运行 `./shm_reader`。

**注意：** System V IPC资源（共享内存段、消息队列、信号量集）在创建后会一直存在于内核中，直到显式删除（`IPC_RMID`命令）或系统重启。如果程序异常退出，这些资源可能残留，导致后续启动时出现“文件已存在”或`shmget`失败等问题。因此，**务必在程序退出前进行清理**。可以使用`ipcs -m`, `ipcs -q`, `ipcs -s`来查看当前系统中的IPC资源。

### 总结

*   **管道 (Pipe / FIFO):** 最简单，通常用于父子进程或相关进程。FIFO允许不相关进程通信，但仍是字节流，需要自己处理消息边界。
*   **消息队列 (Message Queue):** 提供了消息的抽象，有消息边界和优先级。POSIX消息队列更现代且易用，System V消息队列则更传统。
*   **共享内存 (Shared Memory):** 最快的IPC方式，直接在内存中读写，但需要额外的同步机制（如信号量或互斥锁）来保证数据一致性。

选择哪种IPC机制取决于你的具体需求：

*   **简单单向通信（父子进程）:** 匿名管道。
*   **简单单向通信（不相关进程）:** 命名管道 (FIFO)。
*   **消息结构化、优先级或异步通信:** 消息队列。
*   **追求极致性能，且愿意处理复杂同步问题:** 共享内存。
*   **网络通信、跨机器通信:** TCP/UDP套接字。
*   **本地高性能、文件描述符传递:** Unix域套接字。

至此，我们对Linux网络编程和进程间通信的大部分核心内容都有了详细的讲解。你还有哪些想深入了解的领域吗？例如：

*   **Linux网络配置与工具** (常用命令、内核参数调优)
*   **异步I/O (AIO) 的更深层机制** (Linux Native AIO vs. `io_uring`)
*   **套接字编程中的常见陷阱与调试技巧**
*   **深入理解TCP/IP协议栈的内核行为**