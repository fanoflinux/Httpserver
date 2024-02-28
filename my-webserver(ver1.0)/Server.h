#pragma once

//初始化监听的套接字
int initListenFD(unsigned short port);
//启动epoll的函数
int epollRun(int lfd);
//与客户端建立新的连接函数两个参数（监听的文件描述符、epoll红黑树的根节点，谁要添加到哪里去）
int accpetClient(int lfd,int epfed);
//接受http的请求
int recvHttpRequest(int cfd,int epfd);//用于通信的文件描述符以及依3完成了通信要断开服务，从树上删除节点
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