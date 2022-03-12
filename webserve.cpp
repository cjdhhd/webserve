#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))



#define SERVER_STRING "Server: SongHao's http/0.1.0\r\n"//定义个人server名称

void *accept_request(void* client);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

void *accept_request(void* from_client)
{
    int client = *(int *)from_client;
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    //文件块属性
    struct stat st;

    int cgi = 0;
    char *query_string = NULL;
    numchars = get_line(client, buf, sizeof(buf));
    i = 0;
    j = 0;

    while(!ISspace(buf[j]) && i < sizeof(method) - 1)
    {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';

    //strcasecmp 比较时忽略大小写
    if(strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return NULL;
    }

    //如果method != POST 则一定有 method == GET, cgi的值不变，为0
    if(strcasecmp(method, "POST") == 0)
        cgi = 1;

    //滤去get或post后的空格
    while(ISspace(buf[j]) && j < sizeof(buf))
        j++;
    
    i = 0;
    while(!ISspace(buf[j]) && i < sizeof(url) - 1 && j < sizeof(buf))
    {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    //GET请求url可能会带有?,有查询参数
    if(strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while(*query_string != '?' &&  *query_string != '\0')
            query_string++;

        /* 如果有?表明是动态请求, 开启cgi */
        if(*query_string == '?')
        {
            cgi  = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "httpdocs%s", url);

    //url以'/'结尾
    if(path[strlen(path) - 1] == '/')
    {
        //url后追加test.html
        strcat(path, "test.html");
    }

    //获取文件信息失败
    if(stat(path, &st) == -1)
    {
        while(numchars > 0 && strcmp("\n", buf))
        {
            numchars = get_line(client, buf, sizeof(buf));
        }

        not_found(client);
    }
    else
    {
        //判断路径是否为目录， S_IFMT起掩码作用
        //如果请求参数为目录, 自动打开test.html
        if((st.st_mode & S_IFMT) == S_IFDIR) 
        {
            strcat(path, "/test.html");
        }

        //文件可执行
        if((st.st_mode & S_IXUSR) ||
           (st.st_mode & S_IXGRP) ||
           (st.st_mode & S_IXOTH))
           //S_IXUSR:文件所有者具可执行权限
		   //S_IXGRP:用户组具可执行权限
		   //S_IXOTH:其他用户具可读取权限
        {
            cgi = 1;
        }

        if(!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }

    close(client);
    //printf("connection close....client: %d \n",client);
    return NULL;
}


int main(int argc, char *argv[])
{
    return 0;
}
