#include "stdafx.h"
#include "proxy_error.h"
#include "proxy.h"

//-----------------------------------------
int main (int argc, char **argv)
{
  int result = 0;

  /* Считывание конфигурации сервера с параметров командной строки */
  if ( parse_console_parameters (argc, argv, &server_conf) )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    result = 1;
    goto MARK_FREE;
  }
  server_config_print (&server_conf, stdout);
  
  /* Создание слушающего сокета */
  int  MasterSocket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if ( MasterSocket == -1 )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    result = 1;
    goto MARK_FREE;
  }
  
  int  so_reuseaddr = 1; /* Манипулируем флагами, установленными на сокете */
  if ( setsockopt (MasterSocket, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof (so_reuseaddr)) )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    result = 1;
    goto MARK_FREE;
  }

  struct sockaddr_in  SockAddr = {0};
  SockAddr.sin_family      = AF_INET;
  SockAddr.sin_port        = htons     (server_conf.port);
  SockAddr.sin_addr.s_addr = inet_addr (server_conf.ip);
  
  int  Result = bind (MasterSocket, (struct sockaddr*) &SockAddr, sizeof (SockAddr));
  if ( Result == -1 )
  {
    fprintf (stderr, "%s\n", strerror (errno)); 
    result = 1;
    goto MARK_FREE;
  }
  
  set_nonblock (MasterSocket);
  
  Result = listen (MasterSocket, SOMAXCONN);
  if ( Result == -1 )
  {
    fprintf (stderr, "%s\n", strerror (errno)); 
    result = 1;
    goto MARK_FREE;
  }

  /*  Cервер, который обслуживает одновременно несколько клиентов.
   *  Понадобится слушающий сокет, который может асинхронно принимать
   *  подключения, обработчик подключения и логика работы с клиентом.
   */
  
  /* Initialize new event_base to do not overlay the global context */
  struct event_base  *base = event_base_new ();
  if ( !base )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    result = 1;
    goto MARK_FREE;
  }
  /* Init events */
  struct event  ev;
  /* Create a new event, that react to read drom server's socket - event of connecting the new client */
  /* Set connection callback (on_connect()) to read event on server socket */
  event_set (&ev, MasterSocket, EV_READ | EV_PERSIST, proxy_accept_cb /* on_accept */, base);
  /* Attach ev_connect event to my event base */
  event_base_set (base, &ev);
  /* Add server event without timeout */
  event_add (&ev, NULL);  
  /* Dispatch events */
  event_base_loop (base, 0);
  //-------------------------------------------

MARK_FREE:
  //-------------------------------------------
  event_base_free (base);

  shutdown (MasterSocket, SHUT_RDWR);
  close    (MasterSocket);

  server_config_free (&server_conf);
  //-------------------------------------------
  return result;
}
