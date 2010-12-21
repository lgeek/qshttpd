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

#define BACKLOG 10

#include "conf.c"
#include "util.c"
#include "http.c"

//Sockets stuff
int sockfd, new_fd;
struct sockaddr_in their_addr;
socklen_t sin_size;


//Other global variables
int buffer_counter;
char * buffer;
FILE *openfile;

void read_chunk() {
    fread (buffer,1,1048576,openfile);
    buffer_counter++;
}

int main(void)
{
    char in[3000],  sent[500], code[50], file[200], mime[100], moved[200], length[100], start[100], end[100];
    char *ext=NULL, *extf;
    int buffer_chunks;
    long filesize, range=0;
    Request request;

    Conf configuration = get_conf();
    sockfd = create_and_bind(configuration);

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
