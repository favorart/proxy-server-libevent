#include "stdafx.h"

#ifndef _PROXY_H_
#define _PROXY_H_
/*
 *  Создать proxy-сервер.
 *  ! LINUX АРХИТЕКТУРА !
 *
 *  v   Сборка через make.
 *  +   Запуск:  ./proxy -c/--conf <filename>
 *               Config: <proxy_port>,<ip1:port1>, <ip2:port2>, ...
 *  +   Сервер исходящего соединения выбирается случайно
 *      из списка серверов в конфигурационном файле.
 *  v   Чтение и запись буферизированы
 *  +   EOF (nc Ctrl+D) --> отправить на сервер все буф.данные
 *                          продолжать посылать клиенту данные сервера
 *                          aнологично обрабатывать данные сервера
 *      При закрытие соединения или ошибки - обрывать оба соединения
 */
//-----------------------------------------
#define  SRV_SERVER_NAME   "proxy-server-pract3"
#define  SRV_CONFDEFNAME   "config.txt"
#define  SRV_CONFMAXSERV   25U
#define  SRV_IPSTRLENGTH   16U
#define  SRV_MAX_WORKERS   16U
#define  SRV_BUF_LOWMARK  128U

typedef struct server_config  srv_conf;
struct  server_config
{
  //-----------------------
  char *ptr_server_name;
  //-----------------------
  uint16_t    port;
  char        ip[SRV_IPSTRLENGTH];
  //-----------------------
  /* defines FILENAME_MAX in <stdio.h> */
  char  config_path[FILENAME_MAX];
  char  server_path[FILENAME_MAX];
  //-----------------------
  struct
  { uint16_t  port;
    char      ip[SRV_IPSTRLENGTH];
  }         *remote_servers;
  uint16_t   remote_servers_count;
  //-----------------------
};
extern struct server_config  server_conf;
//-----------------------------------------
int   server_config_init  (srv_conf *conf, char *path);
void  server_config_print (srv_conf *conf, FILE *stream);
void  server_config_free  (srv_conf *conf);
//-----------------------------------------
int   parse_console_parameters (int argc, char **argv, srv_conf *conf);
//-----------------------------------------
int   set_nonblock (evutil_socket_t fd);
//-----------------------------------------
struct client
{
  /* bufferevent already has separate
   * two buffers for input and output.
   */
  struct bufferevent     *b_ev;
  struct event_base      *base;
  //---------------------------
  struct bufferevent     *server;
  //---------------------------
  int flag_close;
};
//-----------------------------------------
void  proxy_accept_cb (evutil_socket_t fd, short ev, void *arg);
void  proxy_ac_err_cb (evutil_socket_t fd, short ev, void *arg);
//-----------------------------------------
void  proxy_read_cb   (struct bufferevent *b_ev, void *arg);
void  proxy_error_cb  (struct bufferevent *b_ev, short events, void *arg);
//-----------------------------------------
#endif // _PROXY_H_
