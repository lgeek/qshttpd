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

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int create_and_bind(Conf configuration) {
    struct sigaction sa;
    int sockfd, yes=1;
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
    
    return sockfd;
}
