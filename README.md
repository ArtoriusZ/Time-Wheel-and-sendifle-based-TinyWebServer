

基于Time Wheel和sendifle改进的TinyWebServer
===============
Linux下基于C++的轻量级Web服务器注释、修改版本.
原项目来自:https://github.com/qinguoyi/TinyWebServer



**项目特点**
* 原项目
    * 使用 线程池 + 非阻塞socket + epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现) 的并发模型。
    * 使用状态机解析HTTP请求报文，支持解析GET和POST请求。
    * 访问服务器数据库实现web端用户注册、登录功能，可以请求服务器图片和视频文件。
    * 实现同步/异步日志系统，记录服务器运行状态。
    * 经Webbench压力测试可以实现上万的并发连接数据交换
* 本项目改进
  * 改进了原项目的定时器实现方式。原项目采用升序链表存储定时器，而本项目采用时间轮实现定时器，将定时器添加的复杂度从O(n)降至O(1)
  * 改进了文件传输的效率。原项目采用mmap+writev进行零拷贝，而本项目采用sendfile进行零拷贝，提高了传输文件时的I/O效率。

  

快速运行
------------
请参考原项目。


个性化运行
------
本项目增加了一个参数。

* -z，选择零拷贝方式，默认mmap+wrtiev
	* 0，mmap+wrtiev
	* 1，sendfile

# 改进思路

**以下分析都是个人见解，不一定准确**

## 定时器改进
首先从添加定时器的效率来看，原项目采用升序链表实现定时器，即将所有定时器存放在一个升序链表中，其顺序由定时器的超时时间(timeout)决定，timeout最小的在链表头，最大的在链表尾。这种实现方式下，想添加一个定时器，需要遍历整个链表，才能找到合适的插入位置，即添加的时间复杂度为O(n)。而如果采用时间轮实现，则可以将所有定时器散列到不同的slot上，此时添加一个定时器，只需要根据其timeout计算其对应应该存放的slot(也可以理解为计算hash值,slot=hash(timeout))，之后直接插入该slot即可，即添加的时间复杂度为O(1)。其次，从执行定时器任务的效率来看，虽然时间轮和升序链表都是O(n)，但实际上时间轮的效率会比O(n)要好一些，因为时间轮将不同定时器散列到不同链表上，随着散列入口(slot数量)增多，每条链表上的定时器数量会越少，如果使用多级时间轮，其执行任务效率能够接近O(1)。

|    | 添加定时器 | 删除定时器|执行定时器任务|
| -------| ------| ------ | -------|
| 升序链表 | O(n) | O(1) | O(n) |
| 时间轮  | O(1) | O(1) | O(n) |

参考： 《Linux高性能服务器编程》游双

## 零拷贝方式改进
当服务器返回客户请求的文件时，一般的做法是read+write，这种做法需要进行: 2次系统调用+2次DMA拷贝(内存和磁盘之间拷贝)+2次CPU拷贝(用户空间和内核空间之间拷贝)。
改进的做法是采用零拷贝技术，减少CPU拷贝，零拷贝的实现方法有两种——

* **① mmap+write**: 先将传输的文件mmap到内存中，此操作涉及1次系统调用和1次DMA拷贝，随后将映射到内存中的文件用write写到socket中，此操作涉及一次系统调用，一次CPU拷贝和一次DMA拷贝。综合起来，此方式共需要2次系统调用+2次DMA拷贝+1次CPU拷贝。
* **② sendfile**:直接将传输的文件从磁盘发送到socket上，此操作的具体实现分两种情况：
    * 网卡不支持SG-DMA：涉及一次系统调用+2次DMA拷贝+1次CPU拷贝。
    * 网卡支持SG-DMA：涉及一次系统调用+2次DMA拷贝。

综上，采用支持SG-DMA的sendfile，在传输文件时能够比mmap+write少一次系统调用和一次CPU拷贝。

接下来介绍在本项目中的sendfile改进的具体实现，首先需要注意的是，HTTP响应报文包含两部分呢：HTTP响应行和响应头，HTTP响应体（响应文件）。其中响应行和响应头是在内存中生成的，而响应文件则存储在磁盘上。而sendfile只能用于传输存储在磁盘上的文件（不能传输socket或者管道），因此如果要用sendfile进行HTTP响应，其响应方式应该结合write使用。

**本项目的具体实现为**：先用write将HTTP响应行和响应头写入到socket中，再用sendfile将响应文件写入到socket中。

**性能分析**：
* 首先定义**HTTP响应报文总字节数为 A**,**HTTP响应行和响应头部分的字节数为 B**，**响应文件部分的字节数为 C**，其中 **A=B+C**。
* **传输小文件时（即可以一次传输完所有数据的情况）**：**mmap+write需要2次系统调用+2次DMA拷贝（A+C字节）+1次CPU拷贝（A字节）**，**而sendfile需要2次系统调用+3次DMA拷贝（A+C字节）+1次CPU拷贝（B字节）**（其中响应头和响应行用write传输，需要1次系统调用+1次CPU拷贝（B字节）+1次DMA拷贝（B字节），其余为sendfile的消耗）。上面括号中的数据表示拷贝操作涉及的字节数。注意write涉及的CPU拷贝和DMA拷贝只需要拷贝字节数较小的响应头和响应行部分，即B字节。此外虽然sendfile要多一次DMA拷贝，但总体来说sendfile和mmap+write两种方式的DMA拷贝总字节数是相同的，只不过在拷贝socket缓冲区数据到网卡时，sendfile分了两次，而mmap+write只需要一次。此外sendfile涉及的CPU拷贝只需要拷贝响应头和响应行部分，而mmap+write需要拷贝整个响应报文。
* **传输大文件时（即需要分多次才能传输完所有数据）**：当传输文件较大时，可能发生传到一半，socket的send_buf就满了的情况，此时会重新注册该socket上的EPOOL_OUT事件，等待之后事件循环再次触发写事件时继续传输剩下的数据。**这里假设C远大于B，且一共要传n次才能传完整个响应文件。** 首先分析mmap+write的情况，mmap需要1次系统调用，1次DMA拷贝（C字节），而write需要n次系统调用+n次CPU拷贝（A字节）+n次DMA拷贝（A字节）。**综上mmap+write需要(n+1)次系统调用+n次CPU拷贝（A字节）+(n+1)次DMA拷贝（A+C字节）**，其次分析sendfile的情况，首先用write发送响应行和响应头，需要1次系统调用+1次CPU拷贝（B字节）+1次DMA拷贝（B字节），接下来用sendfile发送响应文件，需要n次系统调用+2n次DMA拷贝(2*C字节)，**综上sendfile需要(n+1)次系统调用+1次CPU拷贝（B字节）+(2n+1)次DMA拷贝（A+C字节）**。可以看出，**在传输大文件时，sendfile和mmap+wirte的系统调用次数相同，DMA拷贝字节数相同，而sendfile的CPU拷贝字节数只需要B字节，mmap+write却需要A字节，注意大文件的情况下A远大于B，说明sendfile的性能应该比mmap+write要好很多。**

 参考：[零拷贝（Zero-copy）和mmap](https://zhuanlan.zhihu.com/p/616105519)



# 改进性能测试

**！！webbench测试时，要记得带上-2选项（表示使用HTTP 1.1），此项目只支持HTTP 1.1连接 ！！**

## 1. 时间轮效果测试
**实验设置：** 一开始是用webbench进行压测，结果发现时间轮和升序链表的性能相差不大，个人认为原因是：webbench模拟大量client并发访问，导致所有连接的超时时间都挤在同一个slot内（哈希冲突严重），使得时间轮无法很好的进行散列，从而退化成了一个普通链表。因此我另写了一个测试文件(./tiemr/test_timer.cpp)，用于测设大量client分批次访问server时，两种定时器的添加性能。测试文件中模拟5000个client分5批访问webserver，每批包含1000人且并发地进行访问, 每批访问间隔为1个TIMESLOT, 且这些client都请求长连接，且均为活跃连接，这意味着这些连接不会被close，其timer将一直存在。

**实验结果：**
| |添加5000个定时器的所需时间(us)|
|---|:---:|
|时间轮| 588.9|
升序链表| 220492|

大概是374倍

## 2. sendfile效果测试

### 2.1 webbench压测

以mmap+write的方式启动服务器
```
./server -l 1 -m 1 -p 9008 -z 0 -c 1
```

以sendifle的方式启动服务器
```
./server -l 1 -m 1 -p 9007 -z 1 -c 1
```

**请求小文件（请求log.html,1KB）**

mmap+write webbench测试：
![webbench_small](/root/webbench_small_mmap.png "webbench结果")


sendfile webbench测试：
![webbench_small](/root/webbench_small_sendfile.png "webbench结果")


**请求大文件(请求vidio.html,包含一个66MB的视频)**

mmap+write webbench测试：
![webbench_small](/root/webbench_large_mmap.png "webbench结果")

sendfile webbench测试：
![webbench_small](/root/webbench_large_sendfile.png "webbench结果")

### 2.2 运行时间测试

**实验设置：** 用日志记录每次HTTP请求中，工作线程处理请求的耗时**process time**(具体为httpconn.cpp中的process()函数的运行时间)以及服务器执行响应发送的耗时**I/O time**（具体为webserver.cpp中dealwithwrite()函数的运行时间）。其中process time包含解析HTTP请求报文以及生成HTTP响应报文的时间，I/O time包含服务器将生成好的HTTP响应报文写入socket的时间。

**实验结果：**
详情见日志文件2023_06_02_ServerLog_new_9007和2023_06_02_ServerLog_new_9008，这里取其中部分信息进行分析。

**首先请求一个小文件judge.html**
```
mmap+write的日志信息为：

#请求报文
2023-06-02 21:31:42.065740 [info]: GET / HTTP/1.1
2023-06-02 21:31:42.065808 [info]: Host: 10.249.43.11:9008

#响应信息
2023-06-02 21:31:42.066191 [info]: process time cost: 452
2023-06-02 21:31:42.066204 [info]: send file: /home/dell/menglun/WebServer-master/root/judge.html
2023-06-02 21:31:42.066287 [info]: send data to the client(10.250.48.157)
2023-06-02 21:31:42.066312 [info]: I/O time cost: 80

sendfile的日志信息为：

#请求报文
2023-06-02 21:31:45.272433 [info]: GET / HTTP/1.1
2023-06-02 21:31:45.272503 [info]: Host: 10.249.43.11:9007

#响应信息
2023-06-02 21:31:45.272835 [info]: process time cost: 404
2023-06-02 21:31:45.272935 [info]: send file: /home/dell/menglun/WebServer-master/root/judge.html
2023-06-02 21:31:45.273048 [info]: send data to the client(10.250.48.157)
2023-06-02 21:31:45.273075 [info]: I/O time cost: 110
```


可以看出，sendfile的process time较低，这是因为sendfile在准备响应报文时，只需要打开对应的文件描述符即可，而mmap+write还需要进一步将文件用mmap映射到内存中。而两者的I/O time相差不大。

**接下来请求一个大文件video.html**

这部分我就不贴对应的日志信息了，太长了，简单说一下。

两者都是先请求video.html，这个文件较小，情况和上面一致，接下来会进一步请求video.html中包含的Solar_System.mp4文件（66MB），此时mmap+write的响应日志信息对应文件2023_06_02_ServerLog_new_9008中的70-117行，sendfile的响应日志信息对应文件2023_06_02_ServerLog_new_9008中的70-109行。

具体来说，mmap+write会分12次传输完整个mp4文件，其总的I/O time是5184us，而sendfile则会分10次传输完整个mp4文件，其总的I/O time是2266us，差不多只需要mmap+write一半的时间。

可以看出，在传输大文件时，sendfile的I/O时间要显著优于mmap+write。