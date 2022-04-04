#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define QUEUE_LENGHT 32
#define BUFFER_SIZE 1024
#define CLIENT_ARR_SIZE 32
#define PORT 2000

int the_number=0;

const char buf_msg[]="Buffer is overcrowed!\n";
const char not_cmd_msg[]="Invalid command.\n";
const char welcome_msg[]="Welcome to the inc/dec game!\nCurrent value ";
const char value_msg[]="New value ";
const char final_msg[]="Good bye.\n";

enum{
    sock_err=1,
    select_err=2,
};

struct client{
    int sd;
    char *buf;
    int buf_used;
};

struct server{
    int lsd; /*listen socket descriptor*/
    struct client **client_arr;
    int arr_size; /*number of clients*/
};

struct client *add_new_client(int sd)
{
    char num_str[BUFFER_SIZE];
    struct client *cl=malloc(sizeof(*cl));
    cl->sd=sd;
    cl->buf=malloc(BUFFER_SIZE*sizeof(*(cl->buf)));
    cl->buf_used=0;
    write(cl->sd,welcome_msg,sizeof(welcome_msg));
    sprintf(num_str,"%d\n",the_number);
    write(cl->sd,num_str,strlen(num_str));
    return cl;
}

int server_init(struct server *serv)
{
    int sock,opt=1,i;
    struct sockaddr_in addr;
    sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(sock==-1){
        perror("socket");
        return sock_err;
    }
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(PORT);
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if(-1==bind(sock,(struct sockaddr*)&addr,sizeof(addr))){
        perror("bind");
        return sock_err;
    }
    listen(sock,QUEUE_LENGHT);
    serv->lsd=sock;
    serv->client_arr=malloc(sizeof(*serv->client_arr)*CLIENT_ARR_SIZE);
    serv->arr_size=CLIENT_ARR_SIZE;
    for(i=0;i< serv->arr_size;i++)
        serv->client_arr[i]=NULL;
    return 0;
}

void server_accept_client(struct server *serv)
{
    int sd;
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    sd=accept(serv->lsd,(struct sockaddr*)&addr,&len);
    if(sd==-1){
        perror("accept");
        return;
    }
    if(sd>=serv->arr_size){
        int i,new_len=serv->arr_size;
        struct client **new_client_arr;
        while(sd>=new_len)
            new_len+=CLIENT_ARR_SIZE;
        new_client_arr=malloc(sizeof(*new_client_arr)*new_len);
        for(i=0;i< serv->arr_size;i++)
            new_client_arr[i]=serv->client_arr[i];
        for(i=serv->arr_size;i<new_len;i++)
            new_client_arr[i]=NULL;
        free(serv->client_arr);
        serv->client_arr=new_client_arr;
        serv->arr_size=new_len;
    }
    serv->client_arr[sd]=add_new_client(sd);
}

void send_the_number(struct server *serv)
{
    int i;
    char num_str[BUFFER_SIZE];
    sprintf(num_str,"%d\n",the_number);
    for(i=0;i< serv->arr_size;i++){
        if(serv->client_arr[i]){
            write(serv->client_arr[i]->sd,value_msg,sizeof(value_msg));
            write(serv->client_arr[i]->sd,num_str,strlen(num_str));
        }
    }
}

void extract_string(struct server *serv,struct client *cl)
{
    int i,j=0,pos=-1;
    char *cmd,*new_buf;
    for(i=0;i< cl->buf_used;i++){
        if(cl->buf[i]=='\n'){
            pos=i;
            break;
        }
    }
    if(pos==-1)
        return;
    cmd=malloc((pos+1)*sizeof(*cmd));
    for(i=0;i<pos;i++)
        cmd[i]=cl->buf[i];
    if(cmd[pos-1]=='\r')
        cmd[pos-1]=0;
    else
        cmd[pos]=0;
    new_buf=malloc((cl->buf_used - (pos+1))*sizeof(*new_buf));
    for(i=pos+1;i< cl->buf_used;i++,j++)
        new_buf[j]=cl->buf[i];
    cl->buf_used-=(pos+1);
    cl->buf=new_buf;
    if(strcmp(cmd,"inc") && strcmp(cmd,"dec")){
        write(cl->sd,not_cmd_msg,sizeof(not_cmd_msg));
        return;
    }
    if(!strcmp(cmd,"inc"))
        the_number++;
    else
        the_number--;
    send_the_number(serv);
}

void close_connection(struct server *serv,struct client *cl)
{
    int fd=cl->sd;
    write(cl->sd,final_msg,sizeof(final_msg));
    close(cl->sd);
    free(serv->client_arr[fd]);
    serv->client_arr[fd]=NULL;
}

int read_from_client(struct server *serv,struct client *cl)
{
    int rd,shift=cl->buf_used;
    rd=read(cl->sd,cl->buf + shift,BUFFER_SIZE - shift);
    if(rd<=0){
        if(rd==-1)
            perror("read");
        close_connection(serv,cl);
        return 0;
    }
    cl->buf_used+=rd;
    if(cl->buf_used >= BUFFER_SIZE){
        write(cl->sd,buf_msg,sizeof(buf_msg));
        close_connection(serv,cl);
        return 0;
    }
    extract_string(serv,cl);
    return 0;
}

int start_server(struct server *serv)
{
    int sel,maxfd,i;
    fd_set readfds;
    for(;;){
        FD_ZERO(&readfds);
        FD_SET(serv->lsd,&readfds);
        maxfd=serv->lsd;
        for(i=0;i< serv->arr_size;i++){
            if(serv->client_arr[i]){
                FD_SET(i,&readfds);
                if(i>maxfd)
                    maxfd=i;
            }
        }
        sel=select(maxfd+1,&readfds,NULL,NULL,NULL);
        if(sel==-1){
            perror("select");
            return select_err;
        }
        if(FD_ISSET(serv->lsd,&readfds))
            server_accept_client(serv);
        for(i=0;i< serv->arr_size;i++){
            if(serv->client_arr[i] && FD_ISSET(i,&readfds))
                read_from_client(serv,serv->client_arr[i]);
        }
    }
    return 0;
}

int main()
{
    struct server serv;
    if(server_init(&serv)==sock_err){
        fputs("Server initialization error\n",stderr);
        return 1;
    }
    printf("Server successfully started, port %d.\n",PORT);
    return start_server(&serv);
}
