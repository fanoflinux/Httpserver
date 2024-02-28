#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"Server.h"

int main(int argc,char *argv[])
{
    if(argc<3)
    {
        printf("./a.out port path\n");
        return -1;
    }
    unsigned short port =atoi(argv[1]);//转换得到的端口
    //切换服务器的工作目录
    chdir(argv[2]);//这个函数的参数是一个绝对路径使用切换函数之后生成相对目录，给解析请求行的函数使用
    //初始化用于监听的套接字
    int lfd = initListenFD(port);
    //启动服务
    epollRun(lfd);

    return 0;
}