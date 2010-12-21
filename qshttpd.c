/*  qshttpd is a lightweight HTTP server.
    Copyright (C) 2007, 2010 Cosmin Gorgovan <cosmin AT linux-geek.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

/* Version 0.3.1 - alpha software
See qshttpd.conf for a configuration example. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>

#include "conf.c"

#define BACKLOG 10

//Sockets stuff
int sockfd, new_fd;
struct sockaddr_in their_addr;
socklen_t sin_size;
struct sigaction sa;

//Other global variables
int buffer_counter;
char * buffer;
FILE *openfile;

void read_chunk() {
    fread (buffer,1,1048576,openfile);
    buffer_counter++;
}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

//Chroot and change user and group to nobody. Got this function from Simple HTTPD 1.0.
void drop_privileges(Conf configuration) {
    struct passwd *pwd;
    struct group *grp;

    if ((pwd = getpwnam(configuration.user)) == 0) {
        fprintf(stderr, "User not found in /etc/passwd\n");
        exit(EXIT_FAILURE);
    }

    if ((grp = getgrnam(configuration.group)) == 0) {
        fprintf(stderr, "Group not found in /etc/group\n");
        exit(EXIT_FAILURE);
    }
    
    if (chdir(configuration.root) != 0) {
        fprintf(stderr, "chdir(...) failed\n");
        exit(EXIT_FAILURE);
    }

    if (chroot(configuration.root) != 0) {
        fprintf(stderr, "chroot(...) failed\n");
        exit(EXIT_FAILURE);
    }

    if (setgid(grp->gr_gid) != 0) {
        fprintf(stderr, "setgid(...) failed\n");
        exit(EXIT_FAILURE);
    }

    if (setuid(pwd->pw_uid) != 0) {
        fprintf(stderr, "setuid(...) failed\n");
        exit(EXIT_FAILURE);
    }
}



void create_and_bind(Conf configuration) {
    int yes=1;
    struct sockaddr_in my_addr;

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(configuration.port);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr.sin_zero), '\0', 8);

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    drop_privileges(configuration);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

struct request {
    char *get;
    long resume;
    char *host;
};
typedef struct request Request;

Request process_request (char *buffer) {
    Request request;
    
    char *line = strtok(buffer, "\n\r");
    do {
        if (strncmp(line, "GET", 3) == 0) {
            line += 4;
            int path_length = strpbrk(line, " ") - line;
            request.get = calloc(1, path_length + 1);
            strncpy(request.get, line, path_length);
        } else if (strncmp(line, "Range: bytes=", 13) == 0){
            line += 13;
            request.resume = atoi(line);
        } else if (strncmp(line, "Host:", 5) == 0) {
            line += 6;
            request.host = malloc(strlen(line) + 1);
            strncpy(request.host, line, strlen(line) + 1);
            printf("%s\n\n", request.host);
        }
    } while ((line = strtok(NULL, "\n\r")) != NULL);
    
    return request;
}

int main(void)
{
    char in[3000],  sent[500], code[50], file[200], mime[100], moved[200], length[100], auth[200], auth_dir[500], start[100], end[100];
    char *result=NULL, *hostname, *hostnamef, *lines, *ext=NULL, *extf, *auth_dirf=NULL, *authf=NULL, *rangetmp;
    int buffer_chunks;
    long filesize, range=0;
    Request request;

    Conf configuration = get_conf();
    create_and_bind(configuration);

    //Important stuff happens here.

    while(1) {
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }

        if (!fork()) {
            close(sockfd);
            if (read(new_fd, in, 3000) == -1) {
                perror("receive");
            } else {
                request = process_request(in);

                // If requested path is a directory
                if (opendir(request.get)) {
                    // Last char is /
                    if (request.get[strlen(request.get)-1] == '/'){
                        // The user knows this is a directory so we serve the directory index
                        strcat(request.get, "index.html");
                        openfile=fopen (request.get, "r");
                        if (openfile){
                            strcpy(code, "200 OK");
                        } else {
                            //Here should be some kind of directory listing
                            strcpy(file, "/404.html");
                            openfile = fopen (request.get, "r");
                            strcpy(code, "404 Not Found");
                        }
                    // Last char isn't / so we redirect the browser to the directory
                    } else {
                        strcpy(code, "301 Moved Permanently");
                        strcpy(moved, "Location: http://");
                        strcat(moved, request.host);
                        strcat(moved, request.get);
                        strcat(moved, "/");
                    }

                // Requested path isn't a directory
                } else {
                    openfile=fopen (request.get, "rb");
                    if (openfile){
                        if (strlen(code) < 1) {
                            strcpy (code, "200 OK");
                        }
                    } else {
                        strcpy(request.get, "/404.html");
                        openfile = fopen (file, "r");
                        strcpy(code, "404 Not Found");
                    }
                }
            }
            if (strcmp(code, "301 Moved Permanently") != 0) {
                fseek (openfile , 0 , SEEK_END);
                filesize = ftell (openfile);
                rewind (openfile);
                if (range > 0) {
                    sprintf(end, "%ld", filesize);
                    filesize = filesize - range;
                    sprintf(start, "%ld", range);
                    fseek (openfile , range , SEEK_SET);
                }
                buffer_chunks = filesize/1048576;
                if(filesize%1048576 > 0){
                    buffer_chunks++;
                }
                sprintf(length, "%ld", filesize);
                buffer_counter = 0;
                buffer = (char*) malloc (sizeof(char)*1048576);
            }

            if (strcmp(code, "404 Not Found") != 0 && strcmp(code, "301 Moved Permanently") !=0) {
                ext = strtok(request.get, ".");
                while(ext != NULL){
                    ext = strtok(NULL, ".");
                    if (ext != NULL){
                        extf = ext;
                    }
                }
            } else {
                extf="html";
            }

            /* Maybe I should read mime types from a file. At least for now, add here what you need.*/

            if (strcmp(extf, "html") == 0){
                strcpy (mime, "text/html");
            } else if(strcmp(extf, "jpg") == 0){
                strcpy (mime, "image/jpeg");
            } else if(strcmp(extf, "gif") == 0){
                strcpy (mime, "image/gif");
            } else if(strcmp(extf, "css") == 0){
                strcpy (mime, "text/css");
            } else {
                strcpy(mime, "application/octet-stream");
            }

            strcpy(sent, "HTTP/1.1 ");
            strcat(sent, code);
            strcat(sent, "\nServer: qshttpd 0.3.0\n");
            if(strcmp(code, "301 Moved Permanently") == 0){
                strcat(sent, moved);
                strcat(sent, "\n");
            }

            strcat(sent, "Content-Length: ");
            if(strcmp(code, "301 Moved Permanently") != 0){
                strcat(sent, length);
            } else {
                strcat(sent, "0");
            }

            if(strcmp(code, "206 Partial Content") == 0) {
                strcat(sent, "\nContent-Range: bytes ");
                strcat(sent, start);
                strcat(sent, "-");
                strcat(sent, end);
                strcat(sent, "/");
                strcat(sent, end);
            }
            strcat(sent, "\nConnection: close\nContent-Type: ");
            strcat(sent, mime);
            strcat(sent, "; charset=");
            strcat(sent, configuration.charset);
            strcat(sent, "\n\n");
            write(new_fd, sent, strlen(sent));
            while (buffer_counter < buffer_chunks) {
                read_chunk();
                write(new_fd, buffer, 1048576);
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    return 0;
}
