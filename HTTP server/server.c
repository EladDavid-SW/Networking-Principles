#include "threadpool.c"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

typedef struct sendVars
{
    //init variabled:
    int fd;
    char *path;
    struct stat fs;
    int timeArraysLen, responseLen, pathLen;
    char *lastModifyDate;
    char *response;
    time_t currTime;
} sendVars;

void error(char *str);
void errorAndFree(char *str, void *toFree);
void usageError();
int isAnum(const char *str);
int handler(void *s_fd);
char **reqValid(char *req, int fd);
bool check_path_valid(char **tokens, int fd);
char *get_path(char *path, int fd);
void free_tokens(char **tokens);
void init_send_vars(sendVars *send);
void free_send(sendVars *send);
bool check_path_permission(char *path);
bool send_response(sendVars *send);
bool send_dir_content(sendVars *send);
int writeToClient(char *str, int fd);
bool check_sub_path_permission(char *subPath);
bool send_file_to_client(sendVars *send);
bool file_to_client(char *file_path, int fd);
void error_to_client(int err_type, int Efd);
void error302(char *path_err, int Efd);
char *get_current_time();
int fd_is_valid(int fd);
char *get_mime_type(char *name);

int main(int argc, char const *argv[])
{
    //check if args are valid
    if (argc != 4)
        usageError();
    for (int i = 1; i < argc; i++)
    {
        if (isAnum(argv[i]) < 0)
            usageError();
    }

    //Init variables:
    int portNum = atoi(argv[1]), poolSize = atoi(argv[2]), maxNumOfRequest = atoi(argv[3]), s_fd;
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET; //use the Internet addr family
    serv_addr.sin_port = htons(portNum);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //bind: a client may connect to any of my addresses

    //Creating socket:
    s_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (s_fd < 0)
        error("Socket Failed");

    //creating thread pool:
    threadpool *thread_pool = create_threadpool(poolSize);
    if (thread_pool == NULL)
    {
        error("Createing Thread Pool Failed");
    }

    //Bind socket:
    int res = bind(s_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res < 0)
    {
        destroy_threadpool(thread_pool);
        error("Bind Failed");
    }

    //Define how many can be in the queue at the same time:
    res = listen(s_fd, 5);
    if (res < 0)
    {
        destroy_threadpool(thread_pool);
        error("Listen Failed");
    }

    //Array of pointers to send as argument to dispatch:
    int n = sizeof(int) * maxNumOfRequest;
    int *req_num = (int *)malloc(n);
    if (req_num == NULL)
    {
        destroy_threadpool(thread_pool);
        error("Malloc Failed");
    }
    memset(req_num, '\0', n);

    //Everything ready - start to accept requests:
    struct sockaddr_in cli_addr;
    socklen_t clientAddrLen = sizeof(cli_addr);
    for (int i = 0; i < maxNumOfRequest; i++)
    {
        req_num[i] = accept(s_fd, (struct sockaddr *)&cli_addr, &(clientAddrLen));
        if (req_num[i] < 0)
        {
            destroy_threadpool(thread_pool);
            errorAndFree("Accept failed", req_num);
        }
        dispatch(thread_pool, handler, &req_num[i]);
    }

    destroy_threadpool(thread_pool);
    close(s_fd);
    free(req_num);

    return 0;
}

void error(char *str)
{
    char err[500];
    memset(err, '\0', 500);
    strcpy(err, str);
    strcat(err, "\n");
    perror(err);
    exit(EXIT_FAILURE);
}

void errorAndFree(char *str, void *toFree)
{
    if (toFree != NULL)
        free(toFree);
    strcat(str, "\n");

    error(str);
}

void usageError()
{
    char *usage_error = "Usage: server <port> <poolsize> <max-number-of-request>\n";
    printf("%s", usage_error);
    exit(EXIT_FAILURE);
}

int isAnum(const char *str)
{ //check if str is a number
    int n = strlen(str);
    for (int i = 0; i < n; i++)
    {
        if (str[i] == '\0')
            return 0;
        if (str[i] < '0' || str[i] > '9')
            return -1; //It's not a num
    }
    return 0;
}

int handler(void *s_fd)
{ //Handler function for threads

    //Read request from client:
    int fd = *((int *)s_fd), readLen = 1, read_res = -1;
    int firtsLineMaxLen = 4000; //Exe assumptions

    char *req = (char *)malloc(sizeof(char) * firtsLineMaxLen);
    if (req == NULL)
    {
        perror("Malloc Failed");
        return -1;
    }
    memset(req, '\0', firtsLineMaxLen);

    char read_buffer[readLen + 1];
    memset(read_buffer, '\0', readLen + 1);

    bool req_valid = false;
    char *t = (char *)malloc(sizeof(char) * 2);
    if (t == NULL)
    {
        free(req);
        perror("Malloc Failed");
        return -1;
    }
    memset(t, '\0', 2);
    char *compareTo = "\n";
    while (read_res != 0)
    {
        read_res = read(fd, read_buffer, readLen);

        strncpy(t, read_buffer, 1);
        if (strcmp(t, compareTo) == 0)
        {
            req_valid = true;
            break;
        }
        strcat(req, t);
    }
    free(t);
    if (!req_valid)
    {
        free(req);
        printf("Request missing \\r\\n");
        error_to_client(500, fd);
        return -1;
    }
    char **tokens = reqValid(req, fd);
    if (tokens == NULL)
    {
        if (fd_is_valid(fd) == 0)
            error_to_client(500, fd);
        free(req);
        return -1;
    }

    //we got here when we checked that the requst is valid and now we sending response to client
    sendVars *send = (sendVars *)malloc(sizeof(sendVars));
    if (send == NULL)
    {
        free_tokens(tokens);
        free(req);
        if (fd_is_valid(fd) == 0)
            error_to_client(500, fd);
        perror("Malloc Failed");
        return -1;
    }
    init_send_vars(send);

    if (fd_is_valid(fd) == 0)
    { //in case we dont sent file index.html and closed the connection
        send->path = tokens[1];
        send->fd = fd;
        send->pathLen = strlen(tokens[1]);
        if (!send_response(send))
        {
            if (fd_is_valid(fd) == 0)
                error_to_client(500, fd);
            free_send(send);
            free_tokens(tokens);
            return -1;
        }
    }

    free_send(send);
    free(req);
    free_tokens(tokens);
    return 0;
}

char **reqValid(char *req, int fd)
{ //return NULL if the request is invalid. return all tokens if they are valid.

    int n = sizeof(req) + 20;
    char *req_dup = (char *)malloc(sizeof(char) * n);
    if (req_dup == NULL)
        error("Malloc Failed");
    memset(req_dup, '\0', n);
    strncpy(req_dup, req, n - 1);

    int tokens_count = 0;
    char *token = strtok(req_dup, " ");
    char last_token[300];
    memset(last_token, '\0', 300);

    while (token != NULL)
    { //check if req containes at list 3 tokens
        ++tokens_count;
        token = strtok(NULL, " ");
    }
    //validate there are 3 tokens and last token is for http version
    if (tokens_count != 3)
    {
        free(req_dup);
        error_to_client(400, fd);
        return NULL;
    }
    free(req_dup);

    char **tokens = (char **)malloc(sizeof(char *) * tokens_count);
    if (tokens == NULL)
    {
        free(req_dup);
        perror("Malloc Failed");
        return NULL;
    }

    char *req_dup2 = (char *)malloc(sizeof(char) * n);
    if (req_dup2 == NULL)
        error("Malloc Failed");
    memset(req_dup2, '\0', n);
    strncpy(req_dup2, req, n - 1);

    tokens[0] = (char *)malloc(sizeof(char) * 100);
    if (tokens[0] == NULL)
    {
        free(tokens);
        free(req_dup2);
        perror("Malloc failed");
        return NULL;
    }
    memset(tokens[0], '\0', 100);

    strcpy(tokens[0], strtok(req_dup2, " "));

    int len = 0;
    for (int i = 1; i < tokens_count; i++)
    {
        if (i == 1)
            len = 2000;
        else
            len = 200;

        tokens[i] = (char *)malloc(sizeof(char) * len);
        if (tokens[i] == NULL)
        {
            free(req_dup2);
            free_tokens(tokens);
            perror("Malloc failed");
            return NULL;
        }
        memset(tokens[i], '\0', len);

        strcpy(tokens[i], strtok(NULL, " "));
    }

    if ((strncmp(tokens[tokens_count - 1], "HTTP/1.1\r\n", 8) != 0) && (strncmp(tokens[tokens_count - 1], "HTTP/1.0\r\n", 8) != 0))
    { //if the last token is not http vresion
        free(req_dup2);
        free_tokens(tokens);
        error_to_client(400, fd);
        return NULL;
    }

    if (strcmp(tokens[0], "GET") != 0)
    {
        free(req_dup2);
        free_tokens(tokens);
        error_to_client(501, fd);
        return NULL;
    }

    if (!check_path_valid(tokens, fd))
    {
        free(req_dup2);
        free_tokens(tokens);
        return NULL;
    }
    free(req_dup2);

    return tokens;
}

bool check_path_valid(char **tokens, int fd)
{ //check that the input path is valid path to file
    char *t_path = get_path(tokens[1], fd);
    if (t_path == NULL)
    {
        return false;
    }
    char *toFree = t_path;
    t_path++;
    char path[1000];
    memset(path, '\0', sizeof(char) * 1000);
    strcat(path, t_path);
    free(toFree);
    int pathLen = strlen(path);
    strcpy(tokens[1], path);

    if (access(path, F_OK) < 0)
    { //trying to access file. throw error in case of failare
        if (fd_is_valid(fd) == 0)
            error_to_client(404, fd);
        return false;
    }

    //check file properties using stat func
    struct stat fs;
    stat(path, &fs); //fill the struct with deatails

    if (S_IFDIR != (fs.st_mode & S_IFMT))
    {                             //in case it is not a dir
        if (!S_ISREG(fs.st_mode)) // file is not regular
        {
            error_to_client(403, fd);
            return false;
        }
        if (!check_path_permission(path))
        {
            error_to_client(403, fd);
            return false;
        }
    }
    else if ((S_IFMT & fs.st_mode) == S_IFDIR && path[pathLen - 1] != '/')
    { //in case of dir that doesnt end with /
        error302(path, fd);
        return false;
    }
    else if (S_IFDIR == (fs.st_mode & S_IFMT) && !check_path_permission(path))
    { //it is a dir without right permissions
        error_to_client(403, fd);
        return false;
    }
    else if (S_IFDIR == (fs.st_mode & S_IFMT))
    {
        // we got here if its a dir that end with /
        // in that case we will try to find file called index.html
        char *new_path = (char *)malloc(1100);
        if (new_path == NULL)
        {
            error_to_client(500, fd);
            return false;
        }
        memset(new_path, '\0', 1100);
        strcat(new_path, path);
        strcat(new_path, "index.html");
        if (access(new_path, F_OK) == 0)
        { //the file exist!
            strcpy(path, new_path);
            free(new_path);
            if (check_path_permission(path) && (fs.st_mode & S_IROTH))
            { //we got the right permissions!
                if (file_to_client(path, fd))
                    return true;
                else
                {
                    error_to_client(500, fd);
                    return false;
                }
            }

            //no right permisions
            error_to_client(403, fd);
            return false;
        }
        free(new_path);
    }

    return true;
}

char *get_path(char *path, int fd)
{
    if (path[0] != '/')
    { //validate that path start with /. if not we can't find it in directory
        if (fd_is_valid(fd) == 0)
            error_to_client(404, fd);
        return NULL;
    }

    int pathLen = strlen(path);
    for (int i = 0; i < pathLen - 1; i++)
        if (path[i] == '/' && path[i + 1] == '/') //in case path with //
        {
            error_to_client(404, fd);
            return NULL;
        }

    char *ret_path = (char *)malloc(sizeof(char) * 4000);
    if (ret_path == NULL)
    {
        error("Malloc Failed");
        return NULL;
    }
    memset(ret_path, '\0', 1000);

    if (pathLen == 1) //case of / only
    {                 //in that case we want to get this dir. which means the dir .
                      //so we change the path to /./
        strcat(ret_path, "/./");
    }
    else
        strcat(ret_path, path);

    return ret_path;
}

void free_tokens(char **tokens)
{
    if (tokens == NULL)
        return;
    for (int i = 0; i < 3; i++)
        if (tokens[i] != NULL)
            free(tokens[i]);
    free(tokens);
}

void init_send_vars(sendVars *send)
{
    send->lastModifyDate = NULL;
    send->path = NULL;
    send->response = NULL;
}

void free_send(sendVars *send)
{
    if (send == NULL)
        return;
    if (send->lastModifyDate != NULL)
        free(send->lastModifyDate);

    // if (send->path != NULL)
    //     free(send->path);

    if (send->response != NULL)
        free(send->response);

    free(send);
}

bool check_path_permission(char *path)
{ //return true if the file has read permission for everyone and executing permissions for all dir
    int n = sizeof(path) + 1000;
    char *dup_path = (char *)malloc(n);
    if (dup_path == NULL)
    {
        perror("Malloc Failed");
        return false;
    }
    memset(dup_path, '\0', n);
    strncpy(dup_path, path, n - 50);

    char *subPath = (char *)malloc(n);
    if (subPath == NULL)
    {
        free(dup_path);
        perror("Malloc Failed");
        return false;
    }
    memset(subPath, '\0', n);

    char *word;
    word = strtok(dup_path, "/");
    while (word != NULL)
    {
        strcat(subPath, word);
        if (!check_sub_path_permission(subPath))
        {
            free(dup_path);
            free(subPath);
            return false;
        }
        strcat(subPath, "/");
        word = strtok(NULL, "/");
    }

    free(subPath);
    free(dup_path);

    return true;
}

bool send_response(sendVars *send)
{ //sending response to client

    //init variabled:
    stat(send->path, &(send->fs));
    send->timeArraysLen = 200;
    send->responseLen = 10000;
    send->response = (char *)malloc(send->responseLen);
    if (send->response == NULL)
    {
        perror("Malloc Failed");
        return false;
    }
    memset(send->response, '\0', send->responseLen);
    send->lastModifyDate = (char *)malloc(send->timeArraysLen);
    if (send->lastModifyDate == NULL)
    {
        perror("Malloc Failed");
        return false;
    }
    memset(send->lastModifyDate, '\0', send->timeArraysLen);
    struct stat fs;
    stat(send->path, &fs);
    strftime(send->lastModifyDate, send->timeArraysLen, RFC1123FMT, gmtime(&(send->fs.st_mtime))); //????????????????????????????e
    if ((S_IFMT & send->fs.st_mode) == S_IFDIR)
    { //in case of dir - send dir content format
        return send_dir_content(send);
    }

    //in case of file:
    //sending response to client base on file format:
    return send_file_to_client(send);
}

bool send_dir_content(sendVars *send)
{ //returning to client all the content of the dir format
    if (fd_is_valid(send->fd) == -1)
    {
        printf("send_dir_content out\n");
        return false;
    }
    char path[4000];
    memset(path, '\0', 4000);
    strcpy(path, send->path);
    char *htmlPart = (char *)malloc(sizeof(char) * 10000); //check free memset
    if (htmlPart == NULL)
    {
        perror("Malloc Failed");
        return false;
    }
    memset(htmlPart, '\0', 10000);
    sprintf(htmlPart, "<HTML>\r\n<HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of %s</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n\r\n", path, path);

    //scan dir content
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        error("Open Dir Failed");
        return false;
    }

    struct dirent *dentry;
    dentry = readdir(dir);
    while (dentry != NULL)
    {
        int n = send->pathLen + strlen(dentry->d_name) + 5;
        char *entity = (char *)malloc(n);
        if (entity == NULL)
        {
            perror("Malloc Failed");
            return false;
        }
        memset(entity, '\0', n);
        strcpy(entity, path);
        strcat(entity, dentry->d_name);

        stat(entity, &send->fs);
        strftime(send->lastModifyDate, sizeof(send->lastModifyDate), RFC1123FMT, gmtime(&send->fs.st_mtime));

        char first_str[1000];
        char second_str[500];
        memset(first_str, '\0', 1000);
        memset(first_str, '\0', 500);

        if (!((send->fs.st_mode & S_IFMT) == S_IFDIR))
        { //entity is a file
            sprintf(first_str, "</tr><td><A HREF=\"%s\">%s</A></td><td>%s</td>\n", dentry->d_name, dentry->d_name, send->lastModifyDate);
            sprintf(second_str, "<td>%ld</td></tr>\n", send->fs.st_size);
        }
        else if ((send->fs.st_mode & S_IFMT) == S_IFDIR)
        { //dir case (adding / after dir name)
            sprintf(first_str, "</tr><td><A HREF=\"%s/\">%s</A></td><td>%s</td>\n", dentry->d_name, dentry->d_name, send->lastModifyDate);
            sprintf(second_str, "<td></td>\r\n");
        }
        strcat(htmlPart, first_str);
        strcat(htmlPart, second_str);

        free(entity);
        dentry = readdir(dir);
    }
    strcat(htmlPart, "</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>");

    //writing ok message:
    stat(path, &send->fs);
    strftime(send->lastModifyDate, sizeof(send->lastModifyDate), RFC1123FMT, gmtime(&send->fs.st_mtime));

    char ok_message[500];
    char *curr_time = (char *)malloc(sizeof(char) * 200);
    if (curr_time == NULL)
    {
        perror("Malloc Failed");
        return NULL;
    }
    memset(curr_time, '\0', 200);
    char *buff = get_current_time();
    strncpy(curr_time, buff, 200);
    free(buff);
    int htmlPartLen = strlen(htmlPart);

    sprintf(ok_message, "HTTP/1.1 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n", curr_time, htmlPartLen, send->lastModifyDate);

    if (writeToClient(ok_message, send->fd) != 0)
    {
        perror("Write Filed");
        return false;
    }
    if (writeToClient(htmlPart, send->fd) != 0)
    { //send html part
        perror("Write Filed");
        return false;
    }

    //free and close vars:
    close(send->fd);
    if (closedir(dir) == -1)
    {
        perror("Close dir Failed");
        return false;
    }
    free(curr_time);
    free(dentry);
    free(htmlPart);
    return true;
}

int writeToClient(char *str, int fd)
{ //return 0 if success and negative num else
    int wrote = 0, len = strlen(str);
    while (wrote < len)
    {
        wrote = write(fd, str, strlen(str));
        if (wrote < 0)
            return -1;
    }

    return 0;
}

bool check_sub_path_permission(char *subPath)
{ //check if sub path got permissions to read and if dir check execute
    struct stat fs;
    stat(subPath, &fs);

    if (!(fs.st_mode & S_IROTH) || (S_ISDIR(fs.st_mode) && !(fs.st_mode & S_IXOTH)))
        return false;

    return true;
}

bool send_file_to_client(sendVars *send)
{
    return file_to_client(send->path, send->fd);
}

bool file_to_client(char *file_path, int fd)
{ //sending file to client
   
    if (!check_path_permission(file_path))
    {//validate permmisions 
        error_to_client(404, fd);
        return false;
    }

    FILE *fToSend;
    fToSend = fopen(file_path, "r");
    if (fToSend == NULL)
    {
        perror("Open file failed");
        return false;
    }

    //write to file server confirm - 200 ok format:

    char *ok_message = (char *)malloc(sizeof(char) * 1000);
    if (ok_message == NULL)
    {
        perror("Malloc Failed");
        return NULL;
    }
    memset(ok_message, '\0', 1000);

    char content_type[200];
    memset(content_type, '\0', 200);
    strcpy(content_type, "");

    if (get_mime_type(file_path) != NULL)
    {
        strcat(content_type, "Content-Type: ");
        strcpy(content_type, get_mime_type(file_path));
        strcat(content_type, "\r\n");
    }

    //Time vars:
    char *curr_time = (char *)malloc(sizeof(char) * 200);
    if (curr_time == NULL)
    {
        perror("Malloc Failed");
        return NULL;
    }
    memset(curr_time, '\0', 200);
    char *buff = get_current_time();
    strncpy(curr_time, buff, 200);
    free(buff);
    char last_modify_time[200];
    memset(last_modify_time, '\0', 200);
    struct stat fs;
    stat(file_path, &fs);
    strftime(last_modify_time, sizeof(last_modify_time), RFC1123FMT, gmtime(&fs.st_mtime));

    //buileding final ok message:
    sprintf(ok_message, "HTTP/1.1 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\n%sContent-Length: %ld\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n", curr_time, content_type, fs.st_size, last_modify_time);

    if (writeToClient(ok_message, fd) < 0)
    {
        perror("Write failed");
        return false;
    }
    int readLen = 4000;
    char *read_buffer = (char *)malloc(readLen);
    if (read_buffer == NULL)
    {
        perror("Malloc Failed");
        return false; //for 500 error
    }
    memset(read_buffer, '\0', readLen);

    //reading from file and sending to client:
    int rc = -1;
    while (rc != 0)
    {
        rc = fread(read_buffer, sizeof(char), readLen, fToSend);

        // if (write(fd, read_buffer, readLen) < 0)
        // {
        //     perror("Write failed");
        //     return false;
        // }
        write(fd, read_buffer, readLen);
        memset(read_buffer, '\0', readLen);
    }
    
    //free and close var:
    close(fd);
    if (fclose(fToSend) < 0)
    {
        perror("Close file failed");
        return false;
    }
    free(read_buffer);
    free(curr_time);
    free(ok_message);
    return true;
}

void error_to_client(int err_type, int Efd)
{ //get error type and send it to the client on the right format
    if (fd_is_valid(Efd) != 0)
        return;

    char *err = (char *)malloc(2000);
    if (err == NULL)
    {
        perror("Malloc Failed");
        return;
    }
    memset(err, '\0', 2000);
    char *curr_time = (char *)malloc(sizeof(char) * 200);
    if (curr_time == NULL)
    {
        perror("Malloc Failed");
        return;
    }
    memset(curr_time, '\0', 200);
    char *buff = get_current_time();
    strncpy(curr_time, buff, 200);
    free(buff);
    char *err_structcher = (char *)malloc(sizeof(char) * 500);
    if (err_structcher == NULL)
    {
        perror("Malloc Failed");
        return;
    }
    memset(err_structcher, '\0', 500);

    strcpy(err_structcher, "HTTP/1.1 %s\r\nServer: webserver/1.0\r\nDate:%s\r\nContent-Type: text/html\r\nContent-Length: %s\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>");

    switch (err_type)
    {
    case 400:
        sprintf(err, err_structcher, "400 Bad Request", curr_time, "113", "400 Bad Request", "400 Bad Request", "Bad Request.");
        break;

    case 403:
        sprintf(err, err_structcher, "403 Forbidden", curr_time, "111", "403 Forbidden", "403 Forbidden", "Access denied.");
        break;

    case 404:
        sprintf(err, err_structcher, "404 Not Found", curr_time, "112", "404 Not Found", "404 Not Found", "File not found.");
        break;

    case 500:
        sprintf(err, err_structcher, "500 Internal Server Error", curr_time, "144", "500 Internal Server Error", "500 Internal Server Error", "Some server side error.");
        break;

    case 501:
        sprintf(err, err_structcher, "501 Not supported", curr_time, "129", "501 Not supported", "501 Not supported", "Method is not supported.");
        break;

    default:
        //to make sure we got valid error type
        close(Efd);
        fprintf(stderr, "Error type invalid\n");
        free(err_structcher);
        free(err);
        free(curr_time);
        return;
    }

    if (write(Efd, err, 2000) < 0)
    {
        free(err_structcher);
        free(curr_time);
        close(Efd);
        free(err);
        perror("Write Failed (from error to client)");
        return;
    }

    close(Efd);
    free(curr_time);
    free(err);
    free(err_structcher);
}

void error302(char *path_err, int Efd)
{
    if (fd_is_valid(Efd) == -1)
        return;

    char *path = (char *)malloc(2000);
    if (path == NULL)
    {
        perror("Malloc Failed");
        return;
    }
    memset(path, '\0', 2000);
    strcpy(path, path_err);
    strcat(path, "\\");

    char *err = (char *)malloc(2000);
    if (err == NULL)
    {
        perror("Malloc Failed");
        return;
    }
    memset(err, '\0', 2000);
    char *curr_time = (char *)malloc(sizeof(char) * 200);
    if (curr_time == NULL)
    {
        perror("Malloc Failed");
        return;
    }
    memset(curr_time, '\0', 200);
    char *buff = get_current_time();
    strncpy(curr_time, buff, 200);
    free(buff);

    sprintf(err, "HTTP/1.1 302 Found\r\nServer: webserver/1.0\r\nDate: %s\r\nLocation: %s\r\nContent-Type: text/html\r\nContent-Length: 123\r\nConnection: close\r\n\r\nHTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\nBODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>", curr_time, path);

    if (write(Efd, err, 2000) < 0)
    {
        free(curr_time);
        close(Efd);
        free(err);
        free(path);
        perror("Write Failed (from error to client)");
        return;
    }

    close(Efd);
    free(path);
    free(curr_time);
    free(err);
}

char *get_current_time()
{ //return the current time in string type
    time_t curr_time;
    char *toReturn = (char *)malloc(sizeof(char) * 200);
    if (toReturn == NULL)
    {
        perror("Malloc Failed");
        return NULL;
    }
    memset(toReturn, '\0', 200);
    curr_time = time(NULL);
    strftime(toReturn, 200, RFC1123FMT, gmtime(&curr_time));

    return toReturn;
}

int fd_is_valid(int fd)
{ //return 0 if the fd is open
    if (fcntl(fd, F_GETFD) != -1)
        return 0;
    return -1;
}

char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext)
        return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".au") == 0)
        return "audio/basic";
    if (strcmp(ext, ".wav") == 0)
        return "audio/wav";
    if (strcmp(ext, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0)
        return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0)
        return "audio/mpeg";
    return NULL;
}
