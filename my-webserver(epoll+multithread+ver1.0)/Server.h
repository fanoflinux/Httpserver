#pragma once

//初始化监听的套接字
int initListenFD(unsigned short port);
//启动epoll的函数
int epollRun(int lfd);
//与客户端建立新的连接函数两个参数（监听的文件描述符、epoll红黑树的根节点，谁要添加到哪里去）
//int accpetClient(int lfd,int epfed);
void* accpetClient(void *arg);//由于传入的数据是一个指针类型的所以要修改函数原型
//接受http的请求
//int recvHttpRequest(int cfd,int epfd);//用于通信的文件描述符以及完成了通信要断开服务，从树上删除节点
void* recvHttpRequest(void *arg);
//解析请求行
int parseRequestLine(const char* line,int cfd);//请求行对应字符串，用于通信的文件描述符
//发送文件
int sendFile(const char* filename,int cfd);//发送的文件内容，发山东的套接字
//发响应头（状态行与响应）
int sendHeadMsg(int cfd,int status,const char* descr,const char* type,int length);//套接字、状态码、状态描述、键值对
//自动获取类型
const char* getFileType(const char*name);
//发送目录
int sendDir(const char* dirName,int cfd);
//HTTPget中并不支持请求行的特殊字符要求可以使用Linux中的unicode命令查看中文字符的utf8编码
//将utf8转换为中文字符的方法，将HTTP中的奇怪的字符串
void decodeMsg(char* to, char* from);
//将十六进制的字符转换成一个十进制的整形数
int hexToDec(char c);