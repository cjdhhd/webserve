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



#define SERVER_STRING "Server: LiZinan's http/0.1.0\r\n"//定义个人server名称

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

void headers(int client, const char *filename)
{
    char buf[1024];
    /* could use filename to determine file type */
    (void)filename;     
    //发送HTTP头
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void not_found(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void bad_request(int client)
{
    char buf[1024];
    //发送400
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}


void cannot_execute(int client)
{
	 char buf[1024];
	//发送500
	 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "Content-type: text/html\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "\r\n");
	 send(client, buf, strlen(buf), 0);
	 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
	 send(client, buf, strlen(buf), 0);
}

void unimplemented(int client)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void cat(int client, FILE *resource)
{
	//发送文件的内容
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void error_die(const char *sc)
{
    perror(sc);
    exit(1);
}

//执行cgi动态解析
void execute_cgi(int client, const char *path, 
                const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];

    pid_t pid;
    int status;

    int i;
    char c;

    int numchars = 1;
    int content_length = -1;
    //默认字符
    buf[0] = 'A';
    buf[1] = '\0';
    if(strcasecmp(method, "GET") == 0)
    {
        while((numchars > 0) && strcmp("\n", buf))
        {
            numchars = get_line(client, buf, sizeof(buf));
        }
    }
    else
    {
        //获取文件长度
        numchars = get_line(client, buf, sizeof(buf));
        while((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if(strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi((&buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }

        if(content_length == -1)
        {
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    //设置两个管道（无名管道是半双工的，所以要两个，实现双向传输）
    if(pipe(cgi_output) < 0)
    {
        cannot_execute(client);
        return;
    }
    if(pipe(cgi_input) < 0)
    {
        cannot_execute(client);
        return;
    }
    if((pid = fork()) < 0)
    {
        cannot_execute(client);
    }
    //子进程运行cgi脚本
    if(pid == 0)
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        //dup重定向
        //标准输出为1，标准输入为0
        //此处将标准输出重定向到cgi_output的写通道
        dup2(cgi_output[1], 1);

        //此处将标准输入重定向到cgi_input的读通道
        dup2(cgi_input[0], 0);

        //关闭了cgi_output中的读通道
        close(cgi_output[0]);
        //关闭了cgi_input中的写通道
        close(cgi_input[1]);

        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        //设置环境变量，只在程序运行期间有效
        putenv(meth_env);

        if(strcasecmp(method, "GET") == 0)
        {
            //存储QUERY_STRING
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else
        {
            //POST
            //存储CONTENT_LENGTH
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }

        //执行CGI脚本
        execl(path, path, NULL);
        exit(0);
    }
    else
    {
        close(cgi_output[1]);
        close(cgi_input[0]);
        if(strcasecmp(method, "POST") == 0)
        {
            for(i = 0; i < content_length; i++)
            {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }
        while(read(cgi_output[0], &c, 1) > 0)
        {
            send(client, &c, 1, 0);
        }

        close(cgi_output[0]);
        close(cgi_input[1]);

        //阻塞直到子进程结束，类似join
        waitpid(pid, &status, 0);
    }
}

//解析一行http报文
//读到/r/n为结束标志
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);

        if(n > 0)
        {
            if(c == '\r')
            {
                //只peek，不读取
                n = recv(sock, &c, 1, MSG_PEEK);
                if((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return(i);
}

//如果不是CGI文件，也就是静态文件，直接读取文件返回给请求的http客户端即可
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];
    buf[0] = 'A';
    buf[1] = '\0';
    while((numchars > 0) && strcmp("\n", buf))
    {
        numchars = get_line(client, buf, sizeof(buf));
    }

    //打开文件
    resource = fopen(filename, "r");
    if(resource == NULL)
    {
        not_found(client);
    }
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

//启动服务端
int startup(u_short *port)
{
    int httpd = 0, option;
    struct sockaddr_in name;
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if(httpd == -1)
        //连接失败
        error_die("socket");

    socklen_t optlen;
    optlen = sizeof(option);
    option = 1;
    setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (void *)&option, optlen);

    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        //绑定失败
        error_die("bind");
    //动态分配一个端口
    if(*port == 0)
    {
        socklen_t namelen = sizeof(name);
        if(getsockname(httpd, (struct  sockaddr *)&name, &namelen) == -1)
        {
            error_die("getsockname");
        }
        *port = ntohs(name.sin_port);
    }

    if(listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

int main(int argc, char *argv[])
{
    int server_sock = -1;
    u_short port = 6379;
    int client_sock = -1;

    return 0;
}
