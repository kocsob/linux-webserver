#include <stdio.h>      /* standard input output    */
#include <stdlib.h>     /* standard library import  */
#include <string.h>     /* string functions         */

/* Own header */
#include "config.h"     /* config header */

#define CONFIG_PORT "PORT"
#define CONFIG_MAXCONNS "MAXCONNS"
#define CONFIG_USER "USER"
#define CONFIG_ROOT_DIR "ROOT_DIR"
#define CONFIG_ERR_DIR "ERR_DIR"
#define CONFIG_CGI_CMD "CGI_CMD"
#define CONFIG_CGI_DIR "CGI_DIR"
#define CONFIG_SYSLOG_NAME "SYSLOG_NAME"
#define CONFIG_DNS "DNS"

/* Function declarations */
int parse_line(const char *line, config *conf);
int check_config(config conf);

int load_config(const char *filename, config *conf)
{
    FILE* fp;
    char line[1024];

    memset(conf, 0, sizeof(config));

    /* Open config file */
    fp = fopen(filename, "r+");
    if (fp == NULL)
    {
        perror("Config file open failed!");
        return EXIT_FAILURE;
    }

    /* Read config file */
    while (fgets(line, 1024, fp))
    {
        if ( (parse_line(line, conf)) != EXIT_SUCCESS)
        {
            fprintf(stderr, "Config file structure is not appropriate");
            return EXIT_FAILURE;
        }
    } /* end while */

    fclose(fp);

    return check_config(*conf);
}

int parse_line(const char *line, config *conf)
{
    char key[PATHSIZE];
    char value[PATHSIZE];

    /* Comment in config, nothing to do */
    if (line[0] == '#')
    {
        return EXIT_SUCCESS;
    }

    /* Key value pairs sparated with ' = ' */
    if ( (sscanf(line, "%255s = %255s\n", key, value)) >= 2)
    {
        /* Port */
        if (strncmp(key, CONFIG_PORT, PATHSIZE) == 0)
        {
            conf->port = atoi(value);
            if (conf->port == 0)
            {
                fprintf(stderr, "The given port number config value is not an integer");
                return EXIT_FAILURE;
            }
        }
        /* Maximum number of connection */
        else if (strncmp(key, CONFIG_MAXCONNS, PATHSIZE) == 0)
        {
            conf->maxconns = atoi(value);
            if(conf->maxconns == 0)
            {
                fprintf(stderr, "The given client number config value is not an integer");
                return EXIT_FAILURE;
            }
        }
        /* User name */
        else if (strncmp(key, CONFIG_USER, PATHSIZE) == 0)
        {
            strncpy(conf->user, value, PATHSIZE);
        }
        /* HTML root directory */
        else if (strncmp(key, CONFIG_ROOT_DIR, PATHSIZE) == 0)
        {
            strncpy(conf->root_dir, value, PATHSIZE);
        }
        /* Error HTML root directory */
        else if (strncmp(key, CONFIG_ERR_DIR, PATHSIZE) == 0)
        {
            strncpy(conf->err_dir, value, PATHSIZE);
        }
        /* CGI command */
        else if (strncmp(key, CONFIG_CGI_CMD, PATHSIZE) == 0)
        {
            strncpy(conf->cgi_cmd, value, PATHSIZE);
        }
        /* CGI root directory */
        else if (strncmp(key, CONFIG_CGI_DIR, PATHSIZE) == 0)
        {
            strncpy(conf->cgi_dir, value, PATHSIZE);
        }
        /* SysLog name */
        else if (strncmp(key, CONFIG_SYSLOG_NAME, PATHSIZE) == 0)
        {
            strncpy(conf->syslog_name, value, PATHSIZE);
        }
        /* DNS resolution */
        else if (strncmp(key, CONFIG_DNS, PATHSIZE) == 0)
        {
            conf->dns = atoi(value);
        }
    }
    return EXIT_SUCCESS;
}

int check_config(config conf)
{
    if (conf.port <= 0)
    {
        return EXIT_FAILURE;
    }
    else if (conf.maxconns <= 0)
    {
        return EXIT_FAILURE;
    }
    else if (conf.user == NULL)
    {
        return EXIT_FAILURE;
    }
    else if (conf.root_dir == NULL)
    {
        return EXIT_FAILURE;
    }
    else if (conf.err_dir == NULL)
    {
        return EXIT_FAILURE;
    }
    else if (conf.cgi_dir == NULL)
    {
        return EXIT_FAILURE;
    }
    else if (conf.syslog_name == NULL)
    {
        return EXIT_FAILURE;
    }
    else if (conf.dns != 0 && conf.dns != 1)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}