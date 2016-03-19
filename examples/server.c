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

    sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK  , 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");

    // bzero((char *) &serv_addr, sizeof(serv_addr));
    memset(&serv_addr, 0, sizeof(serv_addr));

    // portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd,5);


    struct sockaddr_in cli_addr;
    socklen_t  clilen;

/*
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) 
        error("ERROR on accept");
*/

    clilen = sizeof(cli_addr);
    int ret = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

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


int main(int argc, char *argv[])
{
    Reactor *r = reactor_create( );
    // Reactor *reactor_create_with_log_level(LOG_DEBUG );
 
    Context context;
    context.sockfd =  start_listening( ); 

    reactor_on_read_ready(r, context.sockfd, -1, &context, (void *)on_accept_ready);
    reactor_run(r);

     return 0; 
}
