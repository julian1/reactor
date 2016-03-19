/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h> // EAGAIN
#include <assert.h> // EAGAIN

#include <reactor.h>

typedef struct Context 
{
    int sockfd;
}
Context;



void error(char *msg)
{
  // this is horrid
    perror(msg);
    exit(1);
}

int start_listening()
{
    int portno = 8000;
    int sockfd;
    struct sockaddr_in serv_addr; 

    // sock
    sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    // reuse
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");

    // bind
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    // listen
    int backlog = 5;
    listen(sockfd, backlog);

    // accept
    // struct sockaddr_in cli_addr;
    // socklen_t  clilen;
    // clilen = sizeof(cli_addr);
    // int ret = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    int ret = accept(sockfd, NULL, 0);//(struct sockaddr *) &cli_addr, &clilen);

    if (ret != -1 && errno != EAGAIN ) {
        error("ERROR on accept");
    }

    return sockfd;
}



void do_accept( int sockfd )
{
    char buffer[256];
    int n;

    int newsockfd = -1;

    fprintf(stdout, "here2\n");

    //bzero(buffer,256);
    memset(&buffer, 0, 256); 
    n = read(newsockfd,buffer,255);
    if (n < 0) 
        error("ERROR reading from socket");

    printf("Here is the message: %s\n",buffer);
    n = write(newsockfd,"I got your message",18);
    if (n < 0) 
        error("ERROR writing to socket");
}

/*
http://www.linuxquestions.org/questions/linux-software-2/what-is-the-difference-in-using-o_nonblock-in-fcntl-and-sock_nonblock-in-socket-779678/
*/

static void on_accept_ready(Context *context, Event *e)
{
    fprintf(stdout, "on_accept_ready \n");

    struct sockaddr_in cli_addr;
    socklen_t  clilen;

    clilen = sizeof(cli_addr);
    int ret = accept(context->sockfd, (struct sockaddr *) &cli_addr, &clilen);

    fprintf(stdout, "ret is %d\n", ret);

    reactor_on_read_ready(e->reactor, context->sockfd, -1, context, (Reactor_callback) on_accept_ready);
    // reactor_rebind(e); // in callback...
}


static void signal_callback(Context *context, Event *e)
{
    switch(e->type) {
        case OK: {
            fprintf(stdout, "caught signal %d cancel all\n", e->signal);
            reactor_cancel_all(e->reactor);

            fprintf(stdout, "closing listening sock\n");
            if(close(context->sockfd) != 0) { 
                error("Could not close");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
            break;
        }
        default:
            assert(0);
        }
}



int main(int argc, char *argv[])
{
    Reactor *r = reactor_create( );
    // Reactor *reactor_create_with_log_level(LOG_DEBUG );
 
    Context context;
    context.sockfd =  start_listening( ); 

    reactor_register_signal(r, 2);  // SIGINT, ctrl-c
    reactor_on_signal(r, -1, &context, (Reactor_callback)signal_callback);

    reactor_on_read_ready(r, context.sockfd, -1, &context, (Reactor_callback)on_accept_ready);
    reactor_run(r);

     return 0; 
}
