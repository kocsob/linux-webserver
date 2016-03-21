#include <stdio.h>          /* standard input output                    */
#include <stdlib.h>         /* standard library                         */
#include <string.h>         /* string functions                         */
#include <fcntl.h>          /* for file operations                      */
#include <unistd.h>         /* miscellaneous functions                  */
#include <sys/socket.h>     /* socket handling                          */
#include <arpa/inet.h>      /* for inet_ntop, including <netinet/in.h>  */
#include <sys/sendfile.h>   /* for sendfile                             */
#include <sys/stat.h>       /* for file status                          */
#include <errno.h>          /* error numbers                            */
#include <netdb.h>          /* for gethostname                          */
#include <time.h>           /* for now function                         */
#include <syslog.h>         /* syslog                                   */ 

/* Own headers */
#include "config.h"		    /* config header                            */
#include "http_codes.h"     /* http codes header                        */

#define BUFFSIZE 1024
#define REQUESTSIZE 10240
#define REQLINE 256
#define NOTALLOWEDCHARS " '`"

typedef int bool;
#define true 1
#define false 0

typedef enum {HEAD = 0, GET, POST} req_type;

typedef struct {
   req_type type;  /* http request type    */
   char *route;    /* http request route   */
   char *params;   /* http request params  */
   char *version;  /* http request version */
} request;

/* request parser */
int parse_request(char *req_buffer, request *req);

/* response functions */
int head_response(config conf, int connection, const char *route);
int get_response(config conf, int connection, const char *route);
int post_response(config conf, int connection, const char *route, char *params);

/* error handler function */
void error_handler(config conf, int connection, int status_code, req_type type);

/* response helper functions */
void send_status(int connection, int status_code);
void send_header(int connection, const char *filepath, int filesize);
int send_content(int connection, const char *filepath, int filesize);

/* misc functions */ 
int get_file_size(const char *filepath);
const char * resolve_addr(struct sockaddr_in *addr, bool dns_resolve);
const char * resolve_http_code(int http_code);
const char * get_datetime();


int response(config conf, int connection, struct sockaddr_in *client_addr)
{
    char req_buffer[REQUESTSIZE];
    request req;
    char req_type_str[BUFFSIZE];
    int status_code = 400; /* Bad request */
    int rcvd;

    memset(req_buffer, 0, sizeof(req_buffer));

    rcvd = recv(connection, req_buffer, REQUESTSIZE, 0);
    if (rcvd < 0)
    {
        syslog(LOG_ERR, "Client disconnected unexpectedly.");
        return EXIT_FAILURE;
    }
    
    /* Response */
    if ( (parse_request(req_buffer, &req)) == EXIT_SUCCESS)
    {
        switch (req.type)
        {
            case GET:
                /* for GET request to "/" route give the "/index.html" */
                if (strncmp(req.route, "/", BUFFSIZE) == 0)
                {
                    strncpy(req.route, "/index.html", PATHSIZE);
                }
                status_code = get_response(conf, connection, req.route);
                strncpy(req_type_str, "GET", BUFFSIZE);
                break;
            case HEAD:
                status_code = head_response(conf, connection, req.route);
                strncpy(req_type_str, "HEAD", BUFFSIZE);
                break;
            case POST:
                /* POST request route begins only with /cgi/ */
                if (strncmp(req.route, "/cgi/", 5) == 0)
                {
                    status_code = post_response(conf, connection, req.route + 5, req.params);
                }
                else
                {
                    status_code = 400;  /* Bad request */
                }
                strncpy(req_type_str, "POST", BUFFSIZE);
                break;
            default:
                break;
        } /* end switch */
    } /* end if */

    /* Check the status code */
    if (status_code != 200)
    {
        error_handler(conf, connection, status_code, req.type);
    }

    /* Log the response */
    syslog(LOG_INFO, "%d %s %s (%s)", status_code, req_type_str, req.route,
        resolve_addr(client_addr, true));

    return EXIT_SUCCESS;
}

/* request parser */
int parse_request(char *req_buffer, request *req)
{
    char *reqline[REQLINE];
    int reqline_len = 0;
    req->type = 0;

    /* Split by "\r\n" */
    reqline[reqline_len] = strtok(req_buffer, "\r\n");
    while (reqline[reqline_len] != NULL)
    {
        ++reqline_len;
        reqline[reqline_len] = strtok(NULL, "\r\n");
    }

    /* Get the request type */
    if (strncmp(reqline[0], "GET", 3) == 0)
    {
        req->type = GET;
    }
    else if (strncmp(reqline[0], "HEAD", 4) == 0)
    {
        req->type = HEAD;
    }
    else if (strncmp(reqline[0], "POST", 4) == 0)
    {
        req->type = POST;
    }
    else
    {
        return EXIT_FAILURE;
    }

    /* Parse the first line */
    strtok(reqline[0], " ");                /* Before the first SPACE */
    req->route = strtok(NULL, " ");         /* After the first SPACE, before the second, it can be NULL */
    if (req->route == NULL)
    {
        return EXIT_FAILURE;
    }
    req->version = strtok(NULL, " ");       /* After the second SPACE */

    /* Check the request */
    if (req->route == NULL || req->version == NULL)
    {
        return EXIT_FAILURE;
    }

    if ( (strcmp(req->version, "HTTP/1.0") != 0) &&
          strcmp(req->version, "HTTP/1.1") != 0)
    {
        return EXIT_FAILURE;
    }

    /* Parse the params */
    switch (req->type)
    {
        case GET:
            req->route = strtok(req->route, "?");   /* Before ? */
            req->params = strtok(NULL, "?");        /* After ?, it can be NULL */
            break;
        case POST:
            req->params = reqline[reqline_len - 1];
            break;
        default:
            req->params = NULL;
            break;
    } /* end switch */

    return EXIT_SUCCESS;
}


/* Response functions */
int get_response(config conf, int connection, const char *route)
{
    char filepath[PATHSIZE];
    int filesize = 0;

    strncpy(filepath, conf.root_dir, PATHSIZE);
    strncat(filepath, route, PATHSIZE);

    if (access(filepath, R_OK) != 0)
    {
        return 404; /* Not found */
    }

    filesize = get_file_size(filepath);
    send_status(connection, 200); /* OK */
    send_header(connection, filepath, filesize);
    if ( (send_content(connection, filepath, filesize)) != EXIT_SUCCESS)
    {
        return 500; /* Internal server error */
    }

    return 200; /* OK */
}

int head_response(config conf, int connection, const char *route)
{
    char filepath[PATHSIZE];
        
    strncpy(filepath, conf.root_dir, PATHSIZE);
    strncat(filepath, route, PATHSIZE);

    if (access(filepath, R_OK) != 0)
    {
        return 404; /* Not found */
    }

    send_status(connection, 200); /* OK */
    send_header(connection, filepath, get_file_size(filepath));
    
    return 200; /* OK */
}

int post_response(config conf, int connection, const char *route, char *params)
{
    FILE* fd;
    char buffer[BUFFSIZE];
    int length;

    /* Cut by the first not allowed character */
    strtok(params, NOTALLOWEDCHARS);

    /* Create the command */
    snprintf(buffer, BUFFSIZE, "%s %s/%s '%s'", conf.cgi_cmd, conf.cgi_dir, route, params);
    
    fd = popen(buffer, "r");
    if (fd == NULL)
    {
        syslog(LOG_ERR, "Failed to run command!: %s", strerror(errno));
        return 500; /* Internal server error */
    }

    send_status(connection, 200); /* OK */

    snprintf(buffer, BUFFSIZE, "\r\n");
    write(connection, buffer, strnlen(buffer, BUFFSIZE));

    while ( (length = read(fileno(fd), buffer, BUFFSIZE)) > 0)
    {
        write(connection, buffer, length);
    }
    return 200; /* OK */
}


/* error handler function */
void error_handler(config conf, int connection, int status_code, req_type type)
{
    char filepath[PATHSIZE];
    int filesize = 0;
    
    snprintf(filepath, PATHSIZE, "%s/%d.html", conf.err_dir, status_code);
 
    if (access(filepath, R_OK) != 0)
    {
        /* Short response */
        send_status(connection, status_code);
    }

    /* Long response */
    filesize = get_file_size(filepath);
    send_status(connection, status_code);
    send_header(connection, filepath, filesize);
    
    if (type == GET || type == POST)
    {
        send_content(connection, filepath, filesize);
    }
}


/* response helper functions */
void send_status(int connection, int status_code)
{
    char buffer[BUFFSIZE];

    snprintf(buffer, BUFFSIZE, "%s\r\n", resolve_http_code(status_code));
    write(connection, buffer, strnlen(buffer, BUFFSIZE));
}

void send_header(int connection, const char *filepath, int filesize)
{
    char buffer[BUFFSIZE];

    snprintf(buffer, BUFFSIZE, "Content-Type: %s\r\n", "text/html");
    write(connection, buffer, strnlen(buffer, BUFFSIZE));

    snprintf(buffer, BUFFSIZE, "Content-Length: %d\r\n\r\n", filesize);
    write(connection, buffer, strnlen(buffer, BUFFSIZE));
}

int send_content(int connection, const char *filepath, int filesize)
{
    int fd;

    fd = open(filepath, O_RDONLY);
    if (fd < 0)
    {
        syslog(LOG_ERR, "Cant open file!: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if ( (sendfile(connection, fd, 0, filesize)) < 0)
    {
        syslog(LOG_ERR, "Failed send file!: %s", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);
    return EXIT_SUCCESS;
}


/* MISC functions */ 
int get_file_size(const char *filepath)
{
    struct stat st;
    if (stat(filepath, &st) == -1)
    {
        return 0;
    }
    return st.st_size;
}

const char * resolve_addr(struct sockaddr_in *addr, bool dns_resolve)
{
    static char name[256];
    int err;
    int flags = 0;

    if (dns_resolve == false)
    {
        flags = NI_NUMERICHOST;
    }

    err = getnameinfo((struct sockaddr*) addr, sizeof(*addr), 
        name, sizeof(name), NULL, 0, flags);
    if (err != 0)
    {
        snprintf(name, sizeof(name), "%s", gai_strerror(err));
    }
    return name;
}

const char * resolve_http_code(int http_code)
{
    switch (http_code)
    {
        case 200: return HTTP_200;
        case 201: return HTTP_201;
        case 204: return HTTP_204;
        case 301: return HTTP_301;
        case 302: return HTTP_302;
        case 304: return HTTP_304;
        case 400: return HTTP_400;
        case 401: return HTTP_401;
        case 403: return HTTP_403;
        case 404: return HTTP_404;
        case 500: return HTTP_500;
        case 501: return HTTP_501;
        case 502: return HTTP_502;
        case 503: return HTTP_503;
        default: return "";
    }
}