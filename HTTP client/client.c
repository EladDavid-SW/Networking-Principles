//Elad David 206760274
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define READLEN 5

//save all the command variables in struct (chunck of memory)
typedef struct
{
    //useful variables:
    char *host;
    char *path;
    int portNum;
    char *port;
    bool p;
    char *pText;
    bool r;
    int rLength; //num of parameters
    char *paramText;
    char *finalRequest;
    int finalReauestLen;
} commandVar;

//function decleretion:
void freeVars(commandVar *i);
void error(char *str, commandVar *toFree);
void wrong_command_err(commandVar *toFree);
int isAnum(char *str);
void initInput(commandVar *input);
void AnalizeInput(int argc, char **argv, commandVar *input);
int parameters(commandVar *input, int argc, char **argv, int i);
int postRequest(commandVar *input, int argc, char **argv, int i);
bool isUrl(char *str);
int urlCase(char *url, commandVar *input);
void initRequest(commandVar *input);
void addLength(char *str, int *n, int len);
void freeVars(commandVar *i);

void error(char *str, commandVar *toFree)
{
    if (toFree != NULL)
    {
        freeVars(toFree);
        free(toFree);
    }

    fprintf(stderr, "%s\n", str);
    exit(EXIT_FAILURE);
}

void wrong_command_err(commandVar *toFree)
{
    fprintf(stderr, "Usage: client [-p <text>] [-r n <pr1=value1 pr2=value2 …>] <URL>\n");
    if (toFree != NULL)
    {
        freeVars(toFree);
        free(toFree);
    }
    exit(EXIT_FAILURE);
}

int isAnum(char *str)
{ //check if str is a number
    int n = strlen(str);
    for (int i = 0; i < n; i++)
    {
        if (i == 0 && str[i] == '-')
            continue;
        if (str[i] == '\0')
            return 0;
        if (str[i] < '0' || str[i] > '9')
            return -1; //It's not a num
    }
    return 0;
}

int main(int argc, char *argv[])
{
    //init all the input variables:
    commandVar *input = (commandVar *)malloc(sizeof(commandVar) * 1);
    if (input == NULL)
        error("Malloc Failed", input);

    if (argc < 2)
        wrong_command_err(input);

    initInput(input);

    //Anlyzing the text:
    AnalizeInput(argc, argv, input);

    //Construct an HTTP request based on the options specified in the command line:

    //init the http requiest:
    initRequest(input);
    char *request = input->finalRequest;
    printf("HTTP request =\n%s\nLEN = %d\n", request, (int)strlen(request));

    //Creating socket:
    struct sockaddr_in serv_addr;
    struct hostent *server;

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("Socket Failed", input);

    //Connecting to server:
    server = gethostbyname(input->host);
    if (server == NULL)
    {
        herror("Host not found\n");
        freeVars(input);
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(input->portNum);

    int conn = connect(sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (conn < 0)
        error("Connection Failed", input);

    // Send HTTP request to the server
    int wLen = 0, requestLen = input->finalReauestLen;
    while (wLen < requestLen)
    {
        int wrote = write(sockfd, input->finalRequest + wLen, requestLen - wLen);
        if (wrote < 0)
            error("Error in sending request", input);
        if (wrote == 0)
            break;
        wLen += wrote;
    }

    //Receive and print an HTTP response
    int readCounter = 0, readThisItr = -1;
    char buf[READLEN];
    while (readThisItr != 0)
    {
        readThisItr = read(sockfd, buf, READLEN);
        if (readThisItr < 0)
            error("Read Failed", input);
        if (readThisItr == 0)
            break;

        readCounter += readThisItr;
        printf("%s", buf);
    }

    close(sockfd);
    printf("\n Total received response bytes: %d\n", readCounter);

    freeVars(input);
    free(input);

    return EXIT_SUCCESS;
}

void initInput(commandVar *input)
{ //initialize all struct variables
    input->path = NULL;
    input->portNum = -1;
    input->port = NULL;
    input->p = false;
    input->pText = NULL;
    input->r = false;
    input->rLength = -1;
    input->paramText = NULL;
    input->host = NULL;
    input->finalRequest = NULL;
    input->finalReauestLen = 0;
}

void freeVars(commandVar *i)
{ //free all allocations
    if (i->finalRequest != NULL)
        free(i->finalRequest);
    if (i->host != NULL)
        free(i->host);
    if (i->paramText != NULL)
        free(i->paramText);
    if (i->path != NULL)
        free(i->path);
    if (i->port != NULL)
        free(i->port);
    if (i->pText != NULL)
        free(i->pText);
}

void AnalizeInput(int argc, char **argv, commandVar *input)
{ //devides input into the right variables
    bool urlFlag = false;

    //calling to fuction due cases:
    for (int i = 1; i < argc; i++)
    { // i=0 --> program name
        if (urlFlag)
        { //error case
            wrong_command_err(input);
        }

        if (strcmp(argv[i], "-r") == 0)
        { //parameters case
            if (parameters(input, argc, argv, i) < 0)
            { //error case
                wrong_command_err(input);
            }
            i += input->rLength + 1;
        }
        else if (strcmp(argv[i], "-p") == 0)
        { //post case
            if (postRequest(input, argc, argv, i) < 0)
            { //error case
                wrong_command_err(input);
            }
            i++;
        }
        else if (isUrl(argv[i]))
        { //url case
            if (urlCase(argv[i], input) < 0)
            { //error case
                wrong_command_err(input);
            }
            urlFlag = true;
        }
        else
        { //error case - not in pattern
            wrong_command_err(input);
        }
    }
    if (!urlFlag)
    {
        wrong_command_err(input);
    }
}
int parameters(commandVar *input, int argc, char **argv, int i)
{ //hendeling parameters case
    //check if -r flag allready added and if rLength exists after -r
    if (input->r || ((i + 1) == argc))
        return -1;

    input->r = true;

    if (isAnum(argv[i + 1]) || (atoi(argv[i + 1]) + i + 1) > argc) //check if the Length valid
        return -1;

    int *rLength = &input->rLength;
    *rLength = atoi(argv[i + 1]);

    if (rLength < 0)
        return -1;

    if (*rLength == 0) //edge case
        return 0;

    //make sure all parameters are valid (writen on the format: "text"+"="+"text")
    int fParam = i + 2;
    for (int j = fParam; j < fParam + *rLength; j++)
    {
        char *param = argv[j];
        if (param[0] == '\0' || param[0] == '=') //check if the param starts with text
            return -1;

        char *res = strstr(argv[j], "=");
        if (res == NULL) //check for "=" sign
            return -1;

        if (res[1] == '\0') //check that text writen after "="
            return -1;
    }

    //allocate params text
    input->paramText = (char *)malloc(sizeof(char) * 450);
    if (input->paramText == NULL)
        error("Malloc failed", input);
    memset(input->paramText, '\0', 450);

    char *paramText = input->paramText;

    //init params text
    paramText[0] = '?';
    strncat(paramText, argv[fParam], strlen(argv[fParam]));

    //concatenate all params to text
    for (int j = fParam + 1; j < fParam + *rLength; j++)
    {
        strcat(paramText, "&");
        strncat(paramText, argv[j], strlen(argv[j]));
    }

    return 0;
}

int postRequest(commandVar *input, int argc, char **argv, int i)
{ //post request case
    //check if -p flag allready added and if text exists after -p
    if (input->p || i + 1 == argc)
        return -1;
    input->p = true;

    int n = strlen(argv[i + 1]) + 2;
    input->pText = (char *)malloc(sizeof(char) * n);
    if (input->pText == NULL)
        error("Malloc failed", input);
    memset(input->pText, '\0', n);

    strncpy(input->pText, argv[i + 1], n);

    return 0;
}

bool isUrl(char *str)
{ //return true if str is url
    char *res = strstr(str, "http://");
    if (res == NULL)
        return false;
    return true;
}

int urlCase(char *url, commandVar *input)
{ //hendeling url case. -1 if failed
    char *res = strstr(url, "http://");
    if (res == NULL)
        return -1;

    //extract path:
    res = res + 7;
    char *path = strchr(res, '/');
    int pathLen = 0;
    if (path != NULL)
    {
        pathLen = strlen(path);
        input->path = (char *)malloc(sizeof(char) * (pathLen + 2));
        if (input->path == NULL)
            error("Malloc faild", input);
        memset(input->path, '\0', (pathLen + 2));
        strncpy(input->path, path, pathLen + 1);
    }
    else
    {
        input->path = (char *)malloc(sizeof(char) * 2);
        if (input->path == NULL)
            error("Malloc faild", input);
        memset(input->path, '\0', 2);
        input->path[0] = '/';
    }

    //extract port: (if exist)
    char *port = strchr(res, ':');
    int portLen = 0;
    if (port != NULL)
    {
        portLen = strlen(port) - pathLen;
        input->port = (char *)malloc(sizeof(char) * (portLen + 1));
        if (input->port == NULL)
            error("Malloc Failed", input);
        memset(input->port, '\0', portLen + 1);
        strncpy(input->port, port + 1, portLen);
        input->port[portLen - 1] = '\0';
        if (isAnum(input->port) != 0)
            return -1;

        input->portNum = atoi(input->port);
    }
    else
        input->portNum = 80;
    if (input->portNum <= 0)
    {
        fprintf(stderr, "Invalid port num");
        freeVars(input);
        free(input);
        exit(EXIT_FAILURE);
    }

    //extract host:
    int hostLength = strlen(res) - pathLen - portLen;
    if (hostLength == 0) //host must be satisfy
        return -1;
    input->host = (char *)malloc(sizeof(char) * (hostLength + 1));
    if (input->host == NULL)
        error("Malloc failed", input);
    memset(input->host, '\0', hostLength + 1);

    strncpy(input->host, res, hostLength);

    return 0;
}

void addLength(char *str, int *n, int len)
{ //add to total length of the request
    if (len == 0)
        return;
    *n += len;
}

void initRequest(commandVar *input)
{ //init http request
    //important variables:
    bool post = input->p, get = !post, isParams = input->r;
    char *path = input->path;
    char *paramText = input->paramText;
    char *host = input->host;
    char *postText = input->pText;
    input->finalRequest = (char *)malloc(sizeof(char) * 450);
    if (input->finalRequest == NULL)
        error("Malloc failed", input);
    memset(input->finalRequest, '\0', 450);

    char *finalRequest = input->finalRequest;
    int len = 0; //request len
    int *pLen = &len;

    if (get)
    {
        addLength(finalRequest, pLen, strlen("GET "));
        strncpy(finalRequest, "GET ", strlen("GET "));
    }
    else if (post)
    {
        addLength(finalRequest, pLen, strlen("POST "));
        strncpy(finalRequest, "POST ", strlen("POST "));
    }

    if (path != NULL)
    {
        addLength(finalRequest, pLen, strlen(path));
        strncat(finalRequest, path, strlen(path));
    }

    if (isParams)
    {
        int paramTextLen = 0;
        if(paramText != NULL)
            paramTextLen = strlen(paramText);
        addLength(finalRequest, pLen, paramTextLen);
        strncat(finalRequest, paramText, paramTextLen);
    }

    addLength(finalRequest, pLen, strlen(" HTTP/1.0\r\nHost: "));
    strcat(finalRequest, " HTTP/1.0\r\nHost: ");
    addLength(finalRequest, pLen, strlen(host));
    strncat(finalRequest, host, strlen(host));

    if (post)
    {
        //parse int to string
        int pTextLen = strlen(postText);
        char pTextLenStr[1000]; 
        sprintf(pTextLenStr, "%d", pTextLen);

        char *add = (char *)malloc(sizeof(char) * 50);
        if (add == NULL)
            error("Malloc Failed", input);
        memset(add, '\0', 50);
        strcat(add, "\r\nContent-length:");
        strncat(add, pTextLenStr, strlen("\r\nContent-length:"));
        addLength(finalRequest, pLen, strlen(add));
        strncat(finalRequest, add, strlen(add));
        free(add);
        addLength(finalRequest, pLen, strlen("\r\n\r\n"));
        strncat(finalRequest, "\r\n\r\n", strlen("\r\n\r\n"));
        addLength(finalRequest, pLen, strlen(postText));
        strncat(finalRequest, postText, strlen(postText));
    }
    else
    {
        addLength(finalRequest, pLen, strlen("\r\n\r\n"));
        strncat(finalRequest, "\r\n\r\n", strlen("\r\n\r\n"));
    }

    addLength(finalRequest, pLen, strlen("\0"));
    strcat(finalRequest, "\0");

    input->finalReauestLen = *pLen;
}
