#ifndef CONFIG_H
#define CONFIG_H

#define PATHSIZE 256

typedef struct {
   int  port;                   /* port number                  */
   int  maxconns;               /* maximum nuber of connection  */
   char user[PATHSIZE];         /* user name                    */
   char root_dir[PATHSIZE];     /* html root directory          */
   char err_dir[PATHSIZE];      /* error html root directory    */
   char cgi_cmd[PATHSIZE];      /* cgi command                  */
   char cgi_dir[PATHSIZE];      /* cgi root directory           */
   char syslog_name[PATHSIZE];  /* syslog name                  */
   int  dns;                    /* dns resolution               */
} config;

int load_config(const char *filename, config *conf);

#endif