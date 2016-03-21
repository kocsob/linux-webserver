/* Library import */
#include <stdio.h>          /* standard input output                */
#include <stdlib.h>         /* standard library                     */
#include <string.h>         /* for string functions                 */

#include <arpa/inet.h>      /* inet_ntop, including <netinet/in.h>  */
#include <unistd.h>         /* miscellaneous functions              */
#include <errno.h>          /* error numbers                        */
#include <pwd.h>            /* for passwd                           */
#include <sys/wait.h>       /* for waitpid                          */
#include <syslog.h>         /* syslog                               */

/* Own headers */
#include "config.h"         /* config header                        */
#include "response.h"       /* response header                      */

/* Main function */
int main(int argc, char **argv)
{
    config conf;                        /* config stucture              */
    struct passwd *pwd;                 /* password stucture            */

    int sockfd;                         /* server socket                */
    int connection;                     /* client connection socket     */
    int conn_cnt = 0;                   /* number of active connections */

    struct sockaddr_in server_addr;     /* server address structure     */
    struct sockaddr_in client_addr;     /* client address structure     */

    /* Check the config file argument */
    if (argc != 2)
    {
        printf("Usage: %s <config_file_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Load config */
    if ( (load_config(argv[1], &conf)) != EXIT_SUCCESS)
    {
        fprintf(stderr, "Loading config from file is failed!\n");
        return(EXIT_FAILURE);
    }

    /* Get the user id */
    pwd = getpwnam(conf.user);
    if (pwd == NULL)
    {
        fprintf(stderr, "User name is not valid!\n");
        return(EXIT_FAILURE);
    }

    /* Create the server socket:
     *  PF_INET         IPv4 protocol (protocol family)
     *  SOCK_STREAM     TCP connection (communication type)
     *  0               default (protocol type)
    */
    if ( (sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "Server socket creating failed!: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }

    /* Set socket options:
     *  sockfd          socket descriptor
     *  SOL_SOCKET      socket options
     *  SO_REUSEADDR    resuse the address
     *  optval          true
     *  optlen          length of optval
    */
    int optval = 1;
    if ( (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) == -1)
    {
        fprintf(stderr, "Server socket options set failed!: %s\n", strerror(errno));
        return(EXIT_FAILURE);
    }

    /* Parametrize the server address */
    memset(&server_addr, 0, sizeof(server_addr)); /* Write zeros to the server_addr struct */
    server_addr.sin_family = AF_INET;  /* Address family */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* IP address */
    server_addr.sin_port = htons(conf.port);  /* Port number */

    /* Bind the server socket
     *  sockfd          socket descriptor
     *  server_addr     address, need to be cast to (struct sockaddr *)
     *  addrlen         length of the address
    */
    if ( (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0)
    {
        fprintf(stderr, "Server socket binding failed!: %s\n", strerror(errno));
        close(sockfd);
        return(EXIT_FAILURE);
    }

    /* Drop the privileges */
    if ( (setgid(pwd->pw_gid)) == -1)
    {
        fprintf(stderr, "Set Group ID failed!: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    if ( (setuid(pwd->pw_uid)) == -1)
    {
        fprintf(stderr, "Set User ID failed!: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    /* Print the config values for checking */
    printf("Port number: %d\n", conf.port);
    printf("Number of clients: %d\n", conf.maxconns);
    printf("User name: %s\n", conf.user);
    printf("Root directory path: %s\n", conf.root_dir);
    printf("Error directory path: %s\n", conf.err_dir);
    printf("CGI command: %s\n", conf.cgi_cmd);
    printf("CGI directory path: %s\n", conf.cgi_dir);
    printf("SysLog name: %s\n", conf.syslog_name);
    printf("DNS resolution: %d\n", conf.dns);
    printf("Webserver started with these paramaters!\n");

    /* Open syslog */
    openlog(conf.syslog_name, LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Webserver started!");

    /* Daemonize
     *  -1              don't change the working directory
     *  0               redirects  standard input, standard output and standard error to /dev/null
    */
    if ( (daemon(-1, 0)) == -1)
    {
        syslog(LOG_ERR, "Daemonize failed!: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
    /* Listen on the server socket
     *  sockfd          socket descriptor
     *  5               Length of the waiting connection list (backlog)
    */
    if ( (listen(sockfd, 5)) < 0)
    {
        syslog(LOG_ERR, "Server socket listening failed!: %s", strerror(errno));
        close(sockfd);
        return(EXIT_FAILURE);
    }

    socklen_t len = sizeof(client_addr);

    /* The main loop of the webserver */
    while (1)
    {
        /* No connection avaliable, wait for one process */
        if (conn_cnt >= conf.maxconns)
        {
            syslog(LOG_NOTICE, "The webserver reach the connection limit");   
            waitpid(-1, NULL, 0);
            --conn_cnt;
        }

        /* Accept the connections on the server socket
         *  sockfd          socket descriptor
         *  client_addr      address, need to be cast to (struct sockaddr *)
         *  addrlen         length of the address
        */
        connection = accept(sockfd, (struct sockaddr *) &client_addr, &len);

        if (connection < 0)
        {
            syslog(LOG_ERR, "Server socket accept failed!: %s", strerror(errno));
        }
        else
        {
            ++conn_cnt;
            /* Child process */
            if (fork() == 0)
            {
                openlog(conf.syslog_name, LOG_PID, LOG_DAEMON);
                response(conf, connection, &client_addr);
                shutdown(connection, SHUT_RDWR); /* close(connection) in all process */
                closelog();
                return EXIT_SUCCESS;
            }

            /* Parent process */
            while ( (waitpid(-1, NULL, WNOHANG)) > 0)  /* Collect the finished process if exists */
            {
                --conn_cnt;
            }
        } /* end else */
    } /* end while */

    /* Close the server socket
     *  sockfd          socket descriptor
    */
    close(sockfd);  /* we never get here */

    closelog();
    return EXIT_SUCCESS;
}