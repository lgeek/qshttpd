
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
