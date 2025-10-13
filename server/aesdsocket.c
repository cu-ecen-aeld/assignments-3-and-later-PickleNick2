#define _POSIX_C_SOURCE 200112L
// definition required from linux man pages
//   getaddrinfo(), freeaddrinfo(), gai_strerror():
//        Since glibc 2.22: _POSIX_C_SOURCE >= 200112L
//        Glibc 2.21 and earlier: _POSIX_C_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h> //Allows conversion of ip from binary to human readable
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/queue.h>

#define FILE_PATH "/var/tmp/aesdsocketdata"
#define PORT "9000"
#define BUFFER_SIZE 1024

volatile sig_atomic_t keep_running = 1;
int server_fd = -1;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

struct thread_node {
    pthread_t thread_id;
    int client_fd;
    bool complete;
    SLIST_ENTRY(thread_node) entries;
};

//Head definition
SLIST_HEAD(thread_list_head, thread_node);
struct thread_list_head thread_head = SLIST_HEAD_INITIALIZER(thread_head);

/*HELPERS*/
void signal_handler(int signal){
    syslog(LOG_USER, "Caught signal, exiting");
    unlink("/var/tmp/aesdsocketdata");
    keep_running = 0;
    if (server_fd != -1){
        close(server_fd);
    }
}

//Helper to write data from buffer to file
ssize_t write_buffer_to_file(const char *buf, size_t len){
    if (pthread_mutex_lock(&list_mutex) != 0)
        return -1;
    FILE *fp = fopen(FILE_PATH, "a");
    if (!fp){
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    size_t written = fwrite(buf, 1, len, fp);
    fflush(fp);
    fclose(fp);
    pthread_mutex_unlock(&list_mutex);
    return (ssize_t)written;
}

//Helper to send whole file back to client
int send_file_to_client(int client_fd){
    if(pthread_mutex_lock(&file_mutex) != 0)
        return -1;
    FILE *fp = fopen(FILE_PATH, "r");
    if(!fp){
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    char buf[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0){
        size_t total_sent = 0;
        while (total_sent < bytes_read){
            ssize_t sent = send(client_fd, buf + total_sent, bytes_read - total_sent, 0);
            if (sent == -1){
                fclose(fp);
                pthread_mutex_unlock(&file_mutex);
                return -1;
            }
            total_sent += sent;
        }
    }
    fclose(fp);
    pthread_mutex_unlock(&file_mutex);
    return 0;
}

//Helper the handle threading of single client
void *client_thread(void *arg){
    struct thread_node *node = (struct thread_node *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while(keep_running){
        bytes_received = recv(node->client_fd, buffer, sizeof(buffer)-1, 0);
        if(bytes_received <= 0)
            break;

        buffer[bytes_received] = '\0';
        char *newline = strchr(buffer, '\n');

        if(newline){
            size_t to_write = (newline - buffer) +1;
            write_buffer_to_file(buffer, to_write);
            break;
        }else{
            write_buffer_to_file(buffer, bytes_received);
        }
    }
    send_file_to_client(node->client_fd);
    close(node->client_fd);

    pthread_mutex_lock(&list_mutex);
    node->complete = true;
    pthread_mutex_unlock(&list_mutex);
    return NULL;
}

//Helper to handle the timestamping
void timestamp_thread(union sigval sv){
    (void)sv;
   
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char timestr[128];
    strftime(timestr, sizeof(timestr), "timestamp:%a, %d %b %Y %H: %M:%S %z\n", &tm);
    write_buffer_to_file(timestr, strlen(timestr));

}

/*END OF HELPERS*/

int main(int argc, char *argv[]){
    openlog(NULL, 0, LOG_USER);
    //Check for daemon mode from cmd
    bool daemon_mode = false;
    int opt;
    while((opt = getopt(argc, argv, "d")) != -1){
        switch (opt)
        {
        case 'd':
            daemon_mode = true;
            break;
        
        default:
            fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    //Register the signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;

    memset(&hints, 0, sizeof(hints)); //make struct empty filled with 0s
    hints.ai_family = AF_UNSPEC; //IPV6 or V4, no preference
    hints.ai_socktype = SOCK_STREAM; //Stream socket
    hints.ai_flags = AI_PASSIVE; //Set IP for me

    //get info
    status = getaddrinfo(NULL, "9000", &hints, &servinfo);
    if (status != 0){
        fprintf(stderr, "Error with getting addrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    //create socket
    server_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (server_fd == -1){
        perror("socket");
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    //Change socket option to allow re-binding
    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
        perror("setsockopt");
        close(server_fd);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    //bind socket
    status = bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen);
    if(status == -1){
        perror("bind");
        close(server_fd);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    //Daemon fork
    if (daemon_mode){
        pid_t pid = fork();
        if (pid < 0){
            perror("fork");
            close(server_fd);
            exit(EXIT_FAILURE);
        } else if (pid > 0){
            exit(EXIT_SUCCESS);
        }

        if (setsid() < 0){
            perror("setsid");
            exit(EXIT_FAILURE);
        }

        if (chdir("/") < 0){
            perror("chdir");
            exit(EXIT_FAILURE);
        }
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
    }

    //listen
    status = listen(server_fd, 5);
    if (status == -1){
        perror("listen");
        close(server_fd);
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_USER, "Server started on port 9000");

    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timestamp_thread;
    sev.sigev_value.sival_ptr = &timerid;

    if(timer_create(CLOCK_REALTIME, &sev, &timerid) == -1){
        perror("timer_create");
    }else{
        its.it_value.tv_sec = 10;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = 10;
        its.it_interval.tv_nsec = 0;

        if( timer_settime(timerid, 0, &its, NULL) == -1){
            perror("timer_settime");
        }
    }

    //Infinite loop until we receive a signal
    while (keep_running){
    struct sockaddr_storage client_addr;
    socklen_t address_len = sizeof(client_addr);
    int clientFD = accept(server_fd, (struct sockaddr *)&client_addr, &address_len);
    if (clientFD == -1){
        perror("accept");
        break;
    }

    //create_thread then for threads in list is thread complete flag set? No, back to accept connection. Yes, pthread_join() then back to accept connection
    // Receive on socket -> send on socket -> set complete flag and exit
    //Threading pitfall tips: Deallocate memory only on main process thread. Only access linked list from main process thread. Remember pthread_join()

    //Need to convert to human readable ip for syslog
    char ipstring[INET6_ADDRSTRLEN];
    void *addr = NULL;
    if (client_addr.ss_family == AF_INET){
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)&client_addr;
        addr = &(ipv4->sin_addr);
    }else if (client_addr.ss_family == AF_INET6){
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&client_addr;
        addr = &(ipv6->sin6_addr);
    }
    //addr now contains the binary representation of the ip address, now convert to human readable
    inet_ntop(client_addr.ss_family, addr, ipstring, sizeof(ipstring));

    syslog(LOG_USER, "Accepted connection from %s", ipstring);

    //Now that we have accepted a connection. Allocate node and add to list
    struct thread_node *node = calloc(1, sizeof(*node));
    if (!node){
        close(clientFD);
        continue;
    }
    node->client_fd = clientFD;
    pthread_mutex_lock(&list_mutex);
    SLIST_INSERT_HEAD(&thread_head, node, entries);
    pthread_mutex_unlock(&list_mutex);

    //create thread
    if(pthread_create(&node->thread_id, NULL, client_thread, node) != 0){
        syslog(LOG_USER, "Failed to create client thread");
        close(clientFD);
        free(node);
        continue;
    }

    //join completed threads
    pthread_mutex_lock(&list_mutex);
    struct thread_node *curr, *temp;
     for(curr = SLIST_FIRST(&thread_head); curr != NULL; curr = temp){
        temp = SLIST_NEXT(curr, entries);
        if (curr->complete){
            pthread_join(curr->thread_id, NULL);
            SLIST_REMOVE(&thread_head, curr, thread_node, entries);
            free(curr);
        }
    }

    pthread_mutex_unlock(&list_mutex);
}
    
    /*SHUTDOWN*/
    close(server_fd);

    pthread_mutex_lock(&list_mutex);
    struct thread_node *curr, *temp;
    
    for(curr = SLIST_FIRST(&thread_head); curr != NULL; curr = temp){
        temp = SLIST_NEXT(curr, entries);
        if (curr->complete){
            pthread_join(curr->thread_id, NULL);
            SLIST_REMOVE(&thread_head, curr, thread_node, entries);
            free(curr);
        }
    }

    pthread_mutex_unlock(&list_mutex);

    timer_delete(timerid);
    unlink(FILE_PATH);

    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&list_mutex);
    closelog();
    return 0;
}
