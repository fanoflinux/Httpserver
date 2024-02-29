#include<stdio.h>
#include"Server.h"
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<errno.h>
#include<strings.h>
#include<string.h>
#include<sys/stat.h>//判断文件的属性
#include<assert.h>
#include<sys/sendfile.h>//Linux自带的发送文件函数
#include<dirent.h>
#include<unistd.h>
#include<stdlib.h>
#include <ctype.h>
#include<pthread.h>

//封装数据提供给线程函数进行使用
struct FdInfo
{
    int fd;
    int epfd;
    pthread_t tid;//线程id
};

//初始化监听套接字，监听的文件描述符只需要一个因此在主线程中被创建出来一个就行了
int initListenFD(unsigned short port)
{
    printf("initListenFd\n");
    //创建监听套接字
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd==-1)
    {
        perror("socket");
        return -1;
    }
    //设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(ret==-1)
    {
        perror("setsockopt");
        return -1;
    }
    //绑定
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;
    ret = bind(lfd,(struct sockaddr*)&addr,sizeof(addr));
    if(ret==-1)
    {
        perror("bind");
        return -1;
    }
    //设置监听
    ret = listen(lfd,128);
    if(ret==-1)
    {
        perror("listen");
        return -1;
    }
    //返回fd
    return lfd;
}
//启动epoll
int epollRun(int lfd)
{
    printf("epollRun\n");
    //1.创建epoll红黑树的根节点
    int epfd = epoll_create(1);
    if(epfd==-1)
    {
        perror("epoll_create");
        return -1;
    }
    //2.上树
    struct epoll_event ev;
    ev.data.fd=lfd;
    ev.events=EPOLLIN;
    int ret = epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&ev);
    if(ret==-1)
    {
        perror("epoll_ctl");
        return -1;
    }
    //3.检测需要进行一个持续的检测
    struct epoll_event evs[1024];//结构体数组
    int size = sizeof(evs)/sizeof(struct epoll_event);//计算事件的数量
    /*单线程的操作逻辑
    while(1)
    {
        int num = epoll_wait(epfd,evs,size,-1);//-1将会一直阻塞
        for(int i =0;i<num;i++)//处理出现变化的事件,产生变化则会触发num的值发生变化。
        {
            int fd = evs[i].data.fd;
            if(fd==lfd)//是监听套接字则要进行新的通信建立
            {//建立了新的连接使用accept函数。accept单纯调用是会发生阻塞的
                accpetClient(lfd,epfd);
            }
            else//不是监听的描述符，说明产生了原有描述符的读写事件
            {
                //主要是接受端的数据(HTTP协议相关内容)，接受HTTP数据
                recvHttpRequest(fd,epfd);
            }
        }
    }
    */
   //由于数据处理可以分别给不同线程的函数处理，因此可以创建子线程分别处理不同客户端的数据
   while(1)
    {
        int num = epoll_wait(epfd,evs,size,-1);//-1将会一直阻塞
        for(int i =0;i<num;i++)//处理出现变化的事件,产生变化则会触发num的值发生变化。
        {
            //将结构体进行实例化以后才能进行使用
            struct FdInfo* info=(struct FdInfo*)malloc(sizeof(struct FdInfo));
            int fd = evs[i].data.fd;
            //结构体的赋值操作
            info->epfd=epfd;
            info->fd=fd;
            if(fd==lfd)//是监听套接字则要进行新的通信建立
            {//建立了新的连接使用accept函数。accept单纯调用是会发生阻塞的
                //accpetClient(lfd,epfd);
                //这个pthread_create函数的第一个参数是一个传出参数，因此直接给地址就行
                pthread_create(&info->tid,NULL,accpetClient,info);//由于pthread_create参数数目的限制，因此将参数封装到一个结构体中进行使用
                //注意这里要修改函数原型的原因是因为pthread_create要求的传入参数的类型应该是(void *)类型的函数指针
            }
            else//不是监听的描述符，说明产生了原有描述符的读写事件
            {
                //主要是接受端的数据(HTTP协议相关内容)，接受HTTP数据
                //recvHttpRequest(fd,epfd);
                pthread_create(&info->tid,NULL,recvHttpRequest,info);
            }
        }
    }
    return 0;
}
//与客户端建立新的连接函数两个参数
//（监听的文件描述符、epoll红黑树的根节点，谁要添加到哪里去）
//int accpetClient(int lfd,int epfd)
void* accpetClient(void *arg)
{
    //将结构体类型进行转换，转换成支持线程操作的FdInfo类型
    struct FdInfo* info =(struct FdInfo*)arg;
    //1.与客户端建立新连接
    int cfd = accept(info->fd,NULL,NULL);
    if(cfd==-1)
    {
        perror("accept");
        //return -1;
        //注意修改返回值的问题
        return NULL;
    }
    //2.由于边缘非阻塞效率最高
    int flag = fcntl(cfd,F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd,F_SETFL,flag);
    //3.cfd添加到epoll中
    struct epoll_event ev;
    ev.data.fd=cfd;
    ev.events=EPOLLIN |EPOLLET;//添加边缘模式，此处只处理读入事件而不处理写出事件
    int ret = epoll_ctl(info->epfd,EPOLL_CTL_ADD,cfd,&ev);
    if(ret==-1)
    {
        perror("epoll_ctl");
        //return -1;
        return NULL;
    }
    //释放内存的时间，当接受新连接任务完成的时候就能进行释放了
    free(info);
    //return 0;
    return NULL;
}
//接受http的请求,服务端发送的是get的请求
//int recvHttpRequest(int cfd,int epfd)
void* recvHttpRequest(void* arg)
{//由于定义成为边缘模式，每次消息更新只会给通知一次，因此要一次读取完
    //定义结构体指针满足多线程需求
    struct FdInfo* info=(struct FdInfo*)arg;
    printf("开始接受数据了……\n");
    int len=0;//读取数据的长度
    int totle =0;//每次读取到的数据长度记录符
    char buf[4096]={0};//存储的是整个客户端发送过来的数据，因此每次都存在buf会造成问题
    char tmp[1024]={0};//每次写数据都放入一个新的缓冲区中
    while((len=recv(info->fd,tmp,sizeof(tmp),0))>0)
    {
        if(totle+len<sizeof(buf))//长度小于指派的内容，放入缓冲区中断
        {
            memcpy(buf+totle,tmp,len);//将临时缓冲区的数据拷贝到长时缓冲区中
        }
        totle+=len;
    }
    //数据读取完毕以及数据读取失败都会返回-1
    if(len==-1&&errno==EAGAIN)
    {
        //解析请求行-->使用另一个函数进行解析操作
        char* pt=strstr(buf,"\r\n");
        int reqLen = pt-buf;//获得结束的位置
        buf[reqLen]='\0';
        parseRequestLine(buf,info->fd);
    }
    else if(len==0)//没有消息了就断开连接嘛
    {
        //客户端已经断开了连接
        epoll_ctl(info->epfd,EPOLL_CTL_DEL,info->fd,NULL);//删除文件描述符不需要指定事件
        close(info->fd);//关闭文件描述符
    }
    else
    {
        perror("recv");
    }
    //通信接受后释放内心
    free(info);
    //return 0;
    return NULL;
}
//解析请求行
int parseRequestLine(const char* line,int cfd)
{//将请求进行分行处理
    //解析请求行 get /XX/1.jpg http/1.1
    char method[12];//解析get，post这类方法
    char path[1024];//进行的资源
    sscanf(line,"%[^ ] %[^ ]",method,path);
    if(strcasecmp(method,"get")!=0)//strcasecmp不区分大小写
    {
        return -1;//不处理get以外的请求
    }
    //将得到的请求的内容进行一个解码的操作，得到正确的中文文件内容
    decodeMsg(path,path);//转换从path到path中，覆盖不会出现溢出的问题
    //处理客户端请求的静态资源（目录或者文件）
    //进行相对路劲的转换
    char* file =NULL;
    if(strcmp(path,"/")==0)//判断是不是资源根目录
    {
        file="./";//将绝对路径切换为相对路径
    }
    else
    {
        file=path+1;
    }
    //获取文件的属性
    struct stat st;
    int ret=stat(file,&st);
    if(ret==-1)
    {
        //文件不存在--回复404页面
        sendHeadMsg(cfd,404,"Not Found",getFileType(".html"),-1);//-1就是不知道
        sendFile("404.html",cfd);
        return 0;
    }
    //判断文件的类型
    if(S_ISDIR(st.st_mode))//1表示是一个目录0表示是一个文件
    {
        //把这个目录的内容发送给客户端
        sendHeadMsg(cfd,200,"OK",getFileType(".html"),-1);//-1就是不知道
        sendDir(file,cfd);
    }
    else
    {
        //把文件的内容发送给客户端
        sendHeadMsg(cfd,200,"OK",getFileType(file),st.st_size);//-1就是不知道
        sendFile(file,cfd);
    }
    return 0;
}
//判断文件的类型
const char* getFileType(const char* name)
{
    // a.jpg a.mp4 a.html
    // 自右向左查找‘.’字符, 如不存在返回NULL
    const char* dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";	// 纯文本
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
//html的基本格式
/*
<html>
    <head>
        <title>test</title>
    </head>
    <body>
        <table>
            <tr>//行的标识符
                <td></td>//表示列
                <td></td>
            </tr>
            <tr>
                <td></td>
                <td></td>
            </tr>
        </table>
    </body>
</html>
*/
//发送目录
int sendDir(const char* dirName,int cfd)
{//以html的方式进行组织处理再发送给客户端
    char buf[4096]={0};
    sprintf(buf,"<html><head><title>%s</title></head><body><table>",dirName);
    struct dirent** namelist;
    int num=scandir(dirName,&namelist,NULL,alphasort);
    for(int i=0;i<num;i++)
    {
        //去除文件
        char *name=namelist[i]->d_name;//这个二级指针指向一个指针数组
        struct stat st;
        char subpath[1024]={0};
        sprintf(subpath,"%s/%s",dirName,name);
        stat(name,&st);
        if(S_ISDIR(st.st_mode))
        {//拼接的是一个目录的名字
            //跳转功能a标签<a href="要跳转的位置">namse</a>
            sprintf(buf+strlen(buf),
            "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>",
            name,name,st.st_size);
        }
        else
        {
            sprintf(buf+strlen(buf),
            "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
            name,name,st.st_size);
        }
        send(cfd,buf,strlen(buf),0);
        memset(buf,0,sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf,"</table></body></html>");
    send(cfd,buf,strlen(buf),0);
    free(namelist);
    return 0;
}
//发送数据（读一部分发送一部分，使用的是TCP协议，没有边界）
int sendFile(const char* filename,int cfd)
{
    //1.打开文件
    int fd=open(filename,O_RDONLY);
    //2.判断文件是否被打开成功，使用断言
    assert(fd>0);//一旦断言失败程序将直接挂掉
#if 0
    while(1)
    {
        char buf[1024];
        int len=read(fd,buf,sizeof(buf));
        if(len>0)
        {
            send(cfd,buf,len,0);//发送给客户端
            usleep(10);//避免发送端发送数据过快，在此处休眠一会，防止接收端崩溃
        }
        else if(len==0)
        {
            break;//跳出循环
        }
        else
        {
            perror("read");
        }
    }
#else//以上代码有些许冗余，进行重构，拷贝次数过多
    off_t offset=0;//偏移量用来接收大量数据
    int size=lseek(fd,0,SEEK_END);
    //SEEK_END使用这步操作将会把文件的指针移动到文件的尾部，可能导致文件不能够正确读出
    //使用SEEK_SET进行设置操作(就是一个移动到头部再移动到尾部的操作)
    lseek(fd,0,SEEK_SET);
    while(offset<size)//大文件的处理
    {
        int ret=sendfile(cfd,fd,&offset,size);
        printf("ret value:%d\n",ret);
        if(ret==-1&&errno==EAGAIN)
        {
            printf("没有数据……\n");
        }
    }
#endif
    close(fd);
    return 0;
}
int sendHeadMsg(int cfd,int status,const char* descr,const char* type,int length)
{//状态行，提供一个数组，将格式化后的数据放入到这个数组中，然后再发送出去
    char buf[4096]={0};
    sprintf(buf,"http/1.1 %d %s\r\n",status,descr);
    //响应头
    sprintf(buf+strlen(buf),"content-type: %s\r\n",type);
    sprintf(buf+strlen(buf),"content-type: %d\r\n\r\n",length);
    //发送数据块
    send(cfd,buf,sizeof(buf),0);
    return 0;
}
//将十六进制的字符转换成一个十进制的整形数
int hexToDec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';//字符之间是允许进行字符运算的
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;//添加上
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}
//将utf8转换为中文字符的方法，将HTTP中的奇怪的字符串
void decodeMsg(char* to, char* from)
{
    for (; *from != '\0'; ++to, ++from)
    {
        // isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
        // Linux%E5%86%85%E6%A0%B8.jpg
        //isxdigit判断这个字符是否是一个十六进制的字符
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // 将16进制的数 -> 十进制 将这个数值赋值给了字符 int -> char
            // B2 == 178
            // 将3个字符, 变成了一个字符, 这个字符就是原始数据
            *to = hexToDec(from[1]) * 16 + hexToDec(from[2]);
            //整型与字符之间可以进行隐式的类型转换，打印会的到一个字符
            // 跳过 from[1] 和 from[2] 因此在当前循环中已经处理过了
            from += 2;
        }
        else//不是奇奇怪怪的字符就直接拷贝到要传输的指针中
        {
            // 字符拷贝, 赋值
            *to = *from;
        }

    }
    *to = '\0';
}