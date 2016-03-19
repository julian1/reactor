/* 
    A simple server, handling multiple parallel connections
    http://www.linuxquestions.org/questions/linux-software-2/what-is-the-difference-in-using-o_nonblock-in-fcntl-and-sock_nonblock-in-socket-779678/
*/
#include <stdio.h>
//#include <sys/types.h> 
//#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h> // EAGAIN
#include <netinet/in.h>

#include <reactor.h>

typedef struct Server 
{
    int fd;
} Server;


typedef struct Conn
{
    Server *server;
    int fd;
} Conn;


void error(char *msg)
{
    // this is horrid
    perror(msg);

    // IMPORTANT instead of calling exit() should we be call reactor_cancel_all() instead
    // these are non reactor errors
    exit(1);
}


static int start_accepting()
{
    int portno = 8000;
    int sockfd;
    struct sockaddr_in serv_addr; 

    // sock
    sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    // sockopt
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
    int ret = accept(sockfd, NULL, 0);
    if (ret != -1 && errno != EAGAIN)
        error("ERROR on accept");

    return sockfd;
}


static Server *server_create()
{
    Server *server = (void *)malloc(sizeof(Server));
    memset(server, 0, sizeof(Server));
    // start_accepting...
    return server;
}

static void server_destroy(Server *server)
{
    close(server->fd);
    memset(server, 0, sizeof(Server));
    free(server);
}


static Conn *conn_create()
{
    Conn *conn = (void *)malloc(sizeof(Conn));
    memset(conn, 0, sizeof(Conn));
    return conn;
}

static void conn_destroy(Conn *conn)
{
    close(conn->fd);
    memset(conn, 0, sizeof(Conn));
    free(conn);
}


static void conn_on_read_ready(Conn *conn, Event *e)
{
    switch(e->type) {
        case OK: {
            fprintf(stdout, "conn ok\n");
            char buf[1000];
            int n = read(e->fd, &buf, 1000);
            if(n > 0)  {
                fprintf(stdout, "conn read %d chars\n", n);
                reactor_on_read_ready(e->reactor, e->fd, -1, conn, (Reactor_callback)conn_on_read_ready);
            } else {
                fprintf(stdout, "conn endpoint hangup\n");
                conn_destroy(conn);
            }
            break;
        }
        case CANCELLED:
            fprintf(stdout, "conn cancelled\n");
            conn_destroy(conn);
            break;
        case EXCEPTION:
            fprintf(stdout, "conn exception\n");
            conn_destroy(conn);
            break;
        case TIMEOUT:
            assert(0);
    }
}


static void server_on_accept_ready(Server *server, Event *e)
{
    switch(e->type) {
        case OK: {
            struct sockaddr_in cli_addr;
            socklen_t  clilen;

            clilen = sizeof(cli_addr);
            int fd = accept(server->fd, (struct sockaddr *) &cli_addr, &clilen);
            if(fd < 0) {
                error("ERROR accept()");
            } 
            else {
                fprintf(stdout, "server accept, spawning conn fd %d\n", fd);
                Conn *conn = conn_create();
                conn->server = server;
                conn->fd = fd;
                reactor_on_read_ready(e->reactor, conn->fd, -1, conn, (Reactor_callback)conn_on_read_ready);
            }
            reactor_on_read_ready(e->reactor, server->fd, -1, server, (Reactor_callback)server_on_accept_ready);
            break;
        }
        case CANCELLED:
            fprintf(stdout, "server cancelled\n");
            server_destroy(server);
            break;
        case EXCEPTION:
            fprintf(stdout, "server exception\n");
            server_destroy(server);
            break;
        case TIMEOUT:
            assert(0);
    }
}


static void signal_callback(Server *server, Event *e)
{
    switch(e->type) {
        case OK: {
            fprintf(stdout, "signal caught %d, cancel all\n", e->signal);
            reactor_cancel_all(e->reactor);
            break;
        }
        default:
            assert(0);
    }
}


int main(int argc, char *argv[])
{
    Reactor *r = reactor_create( );
 
    Server *server = server_create();
    server->fd = start_accepting(); 

    reactor_register_signal(r, 2);  // SIGINT, ctrl-c
    reactor_on_signal(r, -1, server, (Reactor_callback)signal_callback);

    reactor_on_read_ready(r, server->fd, -1, server, (Reactor_callback)server_on_accept_ready);
    reactor_run(r);

     return 0; 
}

