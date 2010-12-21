int is_option(char *buffer, char *option_name) {
    // Find the first uppercase alphabetic char
    while (*buffer < 65 || *buffer > 90) {
        buffer++;
    }
    
    // key=value must be longer than key
    if (strlen(buffer) > strlen(option_name)) {
        return (strncmp(buffer, option_name, strlen(option_name)) == 0);
    } else {   
        return 0;
    }
}

char * get_value(char * buffer/*, int max_length*/) {
    /*if (strlen(buffer) > max_length) {
        fprintf(stderr, "Error parsing configuration directive: %s\n", buffer);
        exit(EXIT_FAILURE);
    }*/
    buffer = strchr(buffer, '=') + 1;
    while (*buffer == ' ' || *buffer == '\t') {
        buffer++;
    }
    
    return buffer;
}

struct conf {
    char *root;
    int port;
    char *charset;
    char *user;
    char *group;
};
typedef struct conf Conf;

Conf get_conf() {
    FILE *conffile;
    char buffer[500];
    char *tmp;
    Conf conf;
    
    conffile = fopen ("/etc/qshttpd.conf", "r");

    while (fgets (buffer , 500, conffile)) {
        if (is_option(buffer, "ROOT")) {
            tmp = get_value(buffer);
            conf.root = calloc(1, strlen(tmp));
            strncpy(conf.root, tmp, strlen(tmp) -1);
        } else if (is_option(buffer, "PORT")) {
            conf.port = atoi(get_value(buffer));
        } else if (is_option(buffer, "USER")) {
            tmp = get_value(buffer);
            conf.user = calloc(1, strlen(tmp));
            strncpy(conf.user, tmp, strlen(tmp) -1);
        } else if (is_option(buffer, "GROUP")) {
            tmp = get_value(buffer);
            conf.group = calloc(1, strlen(tmp));
            strncpy(conf.group, tmp, strlen(tmp) -1);
        } else if (is_option(buffer, "CHARSET")) {
            tmp = get_value(buffer);
            conf.charset = calloc(1, strlen(tmp) + 1);
            strncpy(conf.charset, tmp, strlen(tmp) -1);
        }
    }
    
    fclose (conffile);
    
    return conf;
}
