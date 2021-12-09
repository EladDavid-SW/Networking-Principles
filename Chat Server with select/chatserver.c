#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdbool.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/select.h>

#define MAX_LEN 4096

//createing client queue:
typedef struct Client
{
    int s_fd;
    char *name;
    struct Client *NextCli;
} Client;

typedef struct List
{
    Client **head_of_clients;
    int num_of_clients;
    int welcome_socket_fd;
} List;

typedef struct Message
{
    char *message;
    int to_fd;
    struct Message *NextM;

} Message;

typedef struct Messages_List
{
    Message **head_of_message;
    int num_of_messages;
} Messages_List;

//Global vars:
List *clients = NULL;
Messages_List *messeges = NULL;

//function decleretion:
int read_first_line(int fd, char *read_buff);
int get_max_fd(List *clients);
void error(char *str);
void MtoErr(char *str);
void usageError();
int isAnum(const char *str);
Client *createCli(List *list, int fd);
void freeClient(Client *cli);
List *createList(int welcomeSocket_fd);
void freeList(List *list);
Client *get_last(List *list);
void add_client(List *list, int fd);
Client *find_client(List *clients, int fd);
void disable_client(List *clients, int i);
Message *create_message(Messages_List *list, char *message, int fd, char *);
void freeMessage(Message *m);
Messages_List *createMessagesList();
void freeMessagesList(Messages_List *list);
Message *get_last_messasge(Messages_List *list);
void add_message(Messages_List *list, char *message, int to_fd, char *senderID);
void send_messages(Messages_List *list, fd_set write_fd);
void removeMessage(Messages_List *list, int fd);
void handler(int sig_num);

int main(int argc, char *argv[])
{
    //check if args are valid
    if (argc != 2)
        usageError();
    for (int i = 1; i < argc; i++)
        if (isAnum(argv[i]) < 0)
            usageError();

    signal(SIGINT, handler); //in case of Ctrl+C end the program

    //Init variables:
    int portNum = atoi(argv[1]);
    if (portNum < 0)
        usageError();
    int welcome_socket_fd;
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET; //use the Internet addr family
    serv_addr.sin_port = htons(portNum);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //Creating welcome socket:
    welcome_socket_fd = socket(PF_INET, SOCK_STREAM, 0); //TCP
    if (welcome_socket_fd < 0)
        error("Socket Failed");

    //Binding socket:
    int res = bind(welcome_socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res < 0)
        error("Bind Failed");

    //Define how many clients can be in the welcome socket queue at the same time:
    res = listen(welcome_socket_fd, 10);
    if (res < 0)
        error("Listen Failed");

    //init variables for accepting requests using select()
    int read_len = -1, cli_fd, max_fd = welcome_socket_fd;
    fd_set read_fd, write_fd, c_rfd;
    FD_ZERO(&read_fd);
    FD_ZERO(&write_fd);
    FD_SET(welcome_socket_fd, &read_fd);
    clients = createList(welcome_socket_fd);
    messeges = createMessagesList();

    while (1)
    {
        FD_ZERO(&write_fd);
        c_rfd = read_fd;
        if (messeges->num_of_messages != 0) //if there is message in queue
            write_fd = read_fd;

        res = select(max_fd + 1, &c_rfd, &write_fd, NULL, NULL);
        if (res < 0)
        {
            freeMessagesList(messeges);
            freeList(clients);
            error("Select Failed");
        }

        //check if case of a new client:
        if (FD_ISSET(welcome_socket_fd, &c_rfd))
        {
            cli_fd = accept(welcome_socket_fd, NULL, NULL);
            if (cli_fd < 0)
            {
                freeMessagesList(messeges);
                freeList(clients);
                error("Accept Failed");
            }
            add_client(clients, cli_fd);
            FD_SET(cli_fd, &read_fd); //set new fd client
            printf("server is ready to read from socket %d\n", cli_fd);
        }

        //write to other clients:
        send_messages(messeges, write_fd);

        //check if there is client request (read needed):
        max_fd = get_max_fd(clients);
        for (int i = welcome_socket_fd + 1; i < max_fd + 1; i++)
        {
            if (FD_ISSET(i, &c_rfd))
            {
                int n = sizeof(char) * MAX_LEN + 1;
                char *message = (char *)malloc(n);
                if (message == NULL)
                {
                    freeMessagesList(messeges);
                    freeList(clients);
                    error("Malloc Failed");
                }
                memset(message, '\0', n);

                read_len = read_first_line(i, message);
                printf("server is ready to read from socket %d\n", i);
                if (read_len < 0)
                {
                    free(message);
                    freeMessagesList(messeges);
                    freeList(clients);
                    error("Read Failed");
                }
                else if (read_len == 0)
                { //in case the client want to close connection
                    free(message);
                    disable_client(clients, i);
                    FD_CLR(i, &read_fd);
                    max_fd = get_max_fd(clients);
                    if (max_fd == -1) //the welcome socket fd is the minimum
                        max_fd = clients->welcome_socket_fd;
                }
                else
                { //read succssed
                    char *senderID = (find_client(clients, i))->name;
                    //add the messages to queue
                    Client *p = *(clients->head_of_clients);
                    while (p != NULL)
                    {
                        if (p->s_fd != i && p->s_fd != -1)
                            add_message(messeges, message, p->s_fd, senderID);
                        p = p->NextCli;
                    }
                    free(message);
                }
            }
        }
    }

    return EXIT_SUCCESS;
}

int read_first_line(int fd, char *read_buff)
{ //read from fd and return len of the read
    int read_res;
    read_res = read(fd, read_buff, MAX_LEN);

    return read_res - 2;
}

int get_max_fd(List *clients)
{
    if (clients == NULL || clients->head_of_clients == NULL)
        return -1;

    Client *p = *(clients->head_of_clients);
    int max = p->s_fd;
    p = p->NextCli;

    while (p != NULL)
    {
        if (p->s_fd > max)
            max = p->s_fd;
        p = p->NextCli;
    }
    return max;
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

void MtoErr(char *str)
{
    fprintf(stderr, "%s", str);
    fprintf(stderr, "%s", "\n");
    exit(EXIT_FAILURE);
}

void usageError()
{
    char *usage_error = "Usage: chatserver <port number>\n";
    MtoErr(usage_error);
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

Client *createCli(List *list, int fd)
{ //return a client with init vars
    Client *toAdd = (Client *)malloc(sizeof(Client));
    if (toAdd == NULL)
    {
        freeList(list);
        error("Malloc Failed");
    }
    toAdd->s_fd = fd;
    toAdd->NextCli = NULL;

    toAdd->name = (char *)malloc(sizeof(char) * 100);
    if (toAdd->name == NULL)
    {
        freeList(list);
        error("Malloc Failed");
    }
    memset(toAdd->name, '\0', 100);

    char tempName[100];
    memset(tempName, '\0', 100);
    sprintf(tempName, "guest%d: ", fd);

    strcpy(toAdd->name, tempName);

    return toAdd;
}

void freeClient(Client *cli)
{ //free client
    if (cli == NULL)
        return;
    if (cli->s_fd != -1)
        close(cli->s_fd);
    if (cli->name != NULL)
        free(cli->name);
    free(cli);
}

List *createList(int welcomeSocket_fd)
{
    //alocate heap memory beacose I want to use the table after the func body end
    List *L = (List *)malloc(sizeof(List));
    if (L == NULL)
        MtoErr("Malloc failed");

    //initialize data members
    L->num_of_clients = 0;
    L->welcome_socket_fd = welcomeSocket_fd;

    L->head_of_clients = (Client **)malloc(sizeof(Client *)); //allocate the pointer to head of clients list
    if (L->head_of_clients == NULL)
        MtoErr("Malloc failed");
        
    *(L->head_of_clients) = NULL;

    return L;
}

void freeList(List *list)
{ //Releases all the allocated members of struct List
    if (list->head_of_clients == NULL)
        return;

    //free clients:
    Client *p = *(list->head_of_clients);
    while (p != NULL)
    {
        Client *toFree = p;
        p = p->NextCli;
        freeClient(toFree);
    }

    //close and free 
    close(list->welcome_socket_fd);
    free(list->head_of_clients);
    free(list);
}

Client *get_last(List *list)
{
    if (list == NULL)
        return NULL;
    Client *p = *(list->head_of_clients);
    Client *prev = NULL;

    while (p != NULL)
    {
        prev = p;
        p = p->NextCli;
    }
    return prev;
}

void add_client(List *list, int fd)
{ //add new client to list

    //check input validation
    if (list == NULL)
        MtoErr("Invalid input");

    //creating an element and init vars
    Client *toAdd = createCli(list, fd);

    //add the client to queue
    Client *last_client_in_queue = get_last(list);
    if (last_client_in_queue == NULL)
        *(list->head_of_clients) = toAdd;
    else
        last_client_in_queue->NextCli = toAdd;
    list->num_of_clients++;
}

Client *find_client(List *clients, int fd)
{ //return clilent that founded by socket fd
    Client *p = *(clients->head_of_clients);
    while (p != NULL)
    {
        if (p->s_fd == fd)
            return p;
        p = p->NextCli;
    }
    return NULL;
}

void disable_client(List *clients, int i)
{ //replace fd with -1 to disable this socket
    Client *p = find_client(clients, i);
    p->s_fd = -1; //disable this fd
    close(i);
}

Message *create_message(Messages_List *list, char *message, int fd, char *senderID)
{ //return a message with init vars
    Message *newM = (Message *)malloc(sizeof(Message));
    if (newM == NULL)
    {
        freeMessagesList(list);
        error("Malloc Failed");
    }
    int n = sizeof(char) * strlen(message) + 400;
    newM->message = (char *)malloc(n);
    if (newM->message == NULL)
    {
        freeMessagesList(list);
        error("Malloc Failed");
    }
    memset(newM->message, '\0', n);
    newM->NextM = NULL;
    newM->to_fd = fd;

    if (senderID != NULL)
        strcpy(newM->message, senderID);
    strcat(newM->message, message);

    return newM;
}

void freeMessage(Message *m)
{ //free message
    if (m == NULL)
        return;
    if (m->message != NULL)
        free(m->message);
    free(m);
}

Messages_List *createMessagesList()
{

    Messages_List *M = (Messages_List *)malloc(sizeof(Messages_List));
    if (M == NULL)
        MtoErr("Malloc failed");
        
    //initialize data members
    M->num_of_messages = 0;
    //allocate the pointer to head of messages list:
    M->head_of_message = (Message **)malloc(sizeof(Message *));
    if (M->head_of_message == NULL)
        MtoErr("Malloc failed");
    *(M->head_of_message) = NULL;

    return M;
}

void freeMessagesList(Messages_List *list)
{ //Releases all the allocated members of struct Messages_List
    if (list->head_of_message == NULL)
        return;

    //free messages:
    Message *p = *(list->head_of_message);
    while (p != NULL)
    {
        Message *toFree = p;
        p = p->NextM;
        freeMessage(toFree);
    }

    //free the pointer to head of the list
    free(list->head_of_message);
    free(list);
}

Message *get_last_messasge(Messages_List *list)
{ //return last message in the queue
    if (list == NULL)
        return NULL;

    Message *p = *(list->head_of_message);
    Message *prev = NULL;
    while (p != NULL)
    {
        prev = p;
        p = p->NextM;
    }
    return prev;
}

void add_message(Messages_List *list, char *message, int to_fd, char *senderID)
{ //add new message to list

    //check input validation
    if (list == NULL)
        MtoErr("Invalid input");

    //creating an element and init vars
    Message *toAdd = create_message(list, message, to_fd, senderID);

    //add the message to queue
    Message *last_message = get_last_messasge(list);
    if (last_message == NULL)
        *(list->head_of_message) = toAdd;
    else
        last_message->NextM = toAdd;
    list->num_of_messages++;
}

void send_messages(Messages_List *list, fd_set write_fd)
{ //trying to sent all messages in queue
    if (list == NULL || list->head_of_message == NULL)
        return;

    int fd, wrote;
    Message *toSend = *(list->head_of_message);
    int counter = 0, numOfMessages = list->num_of_messages;
    while (toSend != NULL && counter < numOfMessages)
    {
        fd = toSend->to_fd;
        if (FD_ISSET(fd, &write_fd))
        {
            wrote = write(fd, toSend->message, strlen(toSend->message));
            if (wrote > 0)
            {
                printf("Server is ready to write to socket %d\n", fd);
                toSend = toSend->NextM;
                removeMessage(list, fd);
            }
        }
        else
        { //move message to tail
            Message *toTail = create_message(list, toSend->message, fd, NULL);
            toSend = toSend->NextM;
            removeMessage(list, fd);
            Message *tail = get_last_messasge(list);
            if (tail == NULL)
                *(list->head_of_message) = toTail;
            else
                tail->NextM = toTail;
            list->num_of_messages++;
        }
        counter++;
    }
}
void removeMessage(Messages_List *list, int fd)
{ //remove Message from list
    if (list->head_of_message == NULL)
        return;
    Message *toRemove = *(list->head_of_message);
    if (toRemove->to_fd == fd)
    { //if head
        *(list->head_of_message) = toRemove->NextM;
        freeMessage(toRemove);
        return;
    }
    Message *prev = toRemove;
    toRemove = toRemove->NextM;

    while (toRemove != NULL)
    {
        if (toRemove->to_fd == fd)
        {
            prev->NextM = toRemove->NextM;
            freeMessage(toRemove);
            list->num_of_messages--;
            return;
        }

        prev = toRemove;
        toRemove = toRemove->NextM;
    }
}

void handler(int sig_num)
{
    if (sig_num == SIGINT)
    {
        freeMessagesList(messeges);
        freeList(clients);
        exit(EXIT_SUCCESS);
    }
}
