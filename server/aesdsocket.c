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

volatile sig_atomic_t keep_running = 1;

void signal_handler(int signal){
    syslog(LOG_USER, "Caught signal, exiting");
    unlink("/var/tmp/aesdsocketdata");
    keep_running = 0;
}



int main(int argc, char *argv[]){
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
        exit(-1);
    }

    //create socket
    int sockID = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockID == -1){
        perror("socket");
        exit(-1);
    }

    //Change socket option to allow re-binding
    int yes = 1;
    if (setsockopt(sockID, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0){
        perror("setsockopt");
        exit(-1);
    }

    //bind socket
    status = bind(sockID, servinfo->ai_addr, servinfo->ai_addrlen);
    if(status == -1){
        perror("bind");
        exit(-1);
    }

    //Daemon fork
    if (daemon_mode){
        pid_t pid = fork();
        if (pid < 0){
            perror("fork");
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
    status = listen(sockID, 5);
    if (status == -1){
        perror("listen");
        exit(-1);
    }
    // syslog(LOG_USER, "Server started on port 9000%s", daemon_mode ? " (daemon mode)": "");
    //Infinite loop until we receive a signal
    while (keep_running){
    struct sockaddr_storage client_addr;
    socklen_t address_len = sizeof(client_addr);
    int clientFD = accept(sockID, (struct sockaddr *)&client_addr, &address_len);
    if (clientFD == -1){
        perror("accept");
        exit(-1);
    }

    //Need to convert to human readable ip for syslog
    char ipstring[INET6_ADDRSTRLEN];
    void *addr;
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

    // Receive data over connection and append to file (or create file)
    int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    // char packet[1024];
    // size_t packet_len = 0;
    ssize_t bytes_received;
    //Open or create file
    FILE *fp1 = fopen("/var/tmp/aesdsocketdata", "a+");
    if (!fp1){
        perror("fopen");
        close(clientFD);
        exit(-1);
    }

    // Receive loop
    bool packet_complete = false;
    while(!packet_complete && keep_running){
        bytes_received = recv(clientFD, buffer, BUFFER_SIZE-1, 0);
        if(bytes_received == -1){
            perror("receive");
            break;
        }else if (bytes_received == 0){
            break;
        }

        buffer[bytes_received] = '\0';

        char *newline = strchr(buffer, '\n');
        if (newline != NULL){
            size_t to_write = (newline - buffer) + 1;
            if (fwrite(buffer,1,to_write, fp1) < to_write){
                perror("fwrite");
                break;
            }
            fflush(fp1);
            break;
        } else {
            if (fwrite(buffer, 1, bytes_received, fp1) < (size_t)bytes_received){
                perror("fwrite");
                break;
            }
        }
        // fflush(fp1);
  
    }
    fclose(fp1);

    //Return content written to file back to sender
    // Must return in chunks and not all at once since file may be too large
    FILE *fp2 = fopen("/var/tmp/aesdsocketdata", "r");
    if (!fp2){
        perror("fopen");
        close(clientFD);
        exit(-1);
    }
    char sendbuf[BUFFER_SIZE];
    size_t bytes_read;
    ssize_t bytes_sent;

    while((bytes_read = fread(sendbuf, 1, BUFFER_SIZE, fp2)) > 0){
        size_t total_sent = 0;
        while (total_sent < bytes_read){
            bytes_sent = send(clientFD, sendbuf + total_sent, bytes_read - total_sent, 0); //TCP may not send all bytes so iterate through the buffer
            if (bytes_sent == -1){
                perror("send");
                break;
            }
            total_sent += bytes_sent;
        }
    }
    fclose(fp2);
    close(clientFD);
    syslog(LOG_USER,"Closed connection from %s", ipstring);
}
    close(sockID);
    unlink("/var/tmp/aesdsocketdata");
    freeaddrinfo(servinfo);
    return 0;
}
