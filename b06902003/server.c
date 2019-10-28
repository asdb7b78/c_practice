#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct{
    int id;
    int balance;
} Account;

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
	int item;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

// Forwards

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error

int main(int argc, char** argv) {
    int i, ret;

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;
    Account acc;
    
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    fd_set master, read_fds, write_fds;
    FD_ZERO(&write_fds);
    maxfd = svr.listen_fd;
    for (int i = 0; i <= maxfd; i++) {
        FD_SET(i, &master);
    }
    
    while (1) {
        // TODO: Add IO multiplexing
        
        read_fds = write_fds = master;
        select(maxfd+1, &read_fds, &write_fds, NULL, NULL);
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                //printf("r = %d\n", i);
            }
            if (FD_ISSET(i, &write_fds)) {
                //printf("w = %d\n", i);
            }
        }
        
        
        
        
        
        
        
        // Check new connection
        clilen = sizeof(cliaddr);
        conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
        if (conn_fd < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;  // try again
            if (errno == ENFILE) {
                (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                continue;
            }
            ERR_EXIT("accept")
        }
        requestP[conn_fd].conn_fd = conn_fd;
        strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
        fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
		ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
		if (ret < 0) {
			fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
			continue;
		}

#ifdef READ_SERVER
        
		sprintf(buf,"%s : %s\n",accept_read_header,requestP[conn_fd].buf);
        int account = atoi(requestP[conn_fd].buf);
        if (account > 0 && account <= 20) {
            file_fd = open("account_list", O_RDONLY);
            //lock
            struct flock lock;
            lock.l_type = F_RDLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = sizeof(Account) * (account - 1);
            lock.l_len = sizeof(Account);
            if (fcntl(file_fd, F_SETFD, lock) == EAGAIN){
                sprintf(buf, "This account is locked.\n");
            }
            else {
                lseek(file_fd, sizeof(Account) * (account - 1), SEEK_SET);
                read(file_fd, &acc, sizeof(Account));
                sprintf(buf, "%d %d\n", acc.id, acc.balance);
                lock.l_type = F_UNLCK;
                fcntl(file_fd, F_SETFD, lock);
            }
            close(file_fd);
        }
		write(requestP[conn_fd].conn_fd, buf, strlen(buf));
#else
		sprintf(buf,"%s : %s\n",accept_write_header,requestP[conn_fd].buf);
        int account = atoi(requestP[conn_fd].buf);
        if (account > 0 && account <= 20) {
            file_fd = open("account_list", O_RDWR);
            struct flock lock;
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = sizeof(Account) * (account - 1);
            lock.l_len = sizeof(Account);
            if (fcntl(file_fd, F_SETFD, lock) == EAGAIN){
                sprintf(buf, "This account is locked.\n");
            }
            else {
                write(requestP[conn_fd].conn_fd, "This account is modifiable.\n", 28);
                lseek(file_fd, sizeof(Account) * (account - 1), SEEK_SET);
                read(file_fd, &acc, sizeof(Account));
                handle_read(&requestP[conn_fd]);
                char command[20];
                sscanf(requestP[conn_fd].buf, "%s", command);
                if (strcmp(command, "save") == 0) {
                    int n;
                    sscanf(requestP[conn_fd].buf, "%s%d", command, &n);
                    if (n < 0) {
                        sprintf(buf, "Operation failed.\n");
                    }
                    else{
                        acc.balance += n;
                    }
                }
                else if (strcmp(command, "withdraw") == 0) {
                    int n;
                    sscanf(requestP[conn_fd].buf, "%s%d", command, &n);
                    if (n < 0 || acc.balance - n < 0) {
                        sprintf(buf, "Operation failed.\n");
                    }
                    else{
                        acc.balance -= n;
                    }
                }
                else if (strcmp(command, "transfer") == 0) {
                    int a2, n;
                    sscanf(requestP[conn_fd].buf, "%s%d%d", command, &a2, &n);
                    Account acc2;
                    lseek(file_fd, sizeof(Account) * (a2 - 1), SEEK_SET);
                    read(file_fd, &acc2, sizeof(Account));
                    if (n < 0 || acc.balance - n < 0) {
                        sprintf(buf, "Operation failed.\n");
                    }
                    else{
                        acc.balance -= n;
                        acc2.balance += n;
                        lseek(file_fd, sizeof(Account) * (a2 - 1), SEEK_SET);
                        write(file_fd, &acc2, sizeof(Account));
                    }
                }
                lseek(file_fd, sizeof(Account) * (account - 1), SEEK_SET);
                write(file_fd, &acc, sizeof(Account));
                lock.l_type = F_UNLCK;
                fcntl(file_fd, F_SETFD, lock);
            }
            close(file_fd);
        }
        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
#endif
		close(requestP[conn_fd].conn_fd);
		free_request(&requestP[conn_fd]);
    }
    free(requestP);
    return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void* e_malloc(size_t size);


static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->item = 0;
    reqP->wait_for_write = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
static int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
	char* p1 = strstr(buf, "\015\012");
	int newline_len = 2;
	// be careful that in Windows, line ends with \015\012
	if (p1 == NULL) {
		p1 = strstr(buf, "\012");
		newline_len = 1;
		if (p1 == NULL) {
			ERR_EXIT("this really should not happen...");
		}
	}
	size_t len = p1 - buf + 1;
	memmove(reqP->buf, buf, len);
	reqP->buf[len - 1] = '\0';
	reqP->buf_len = len-1;
    return 1;
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }
}

static void* e_malloc(size_t size) {
    void* ptr;

    ptr = malloc(size);
    if (ptr == NULL) ERR_EXIT("out of memory");
    return ptr;
}

