#include "stdafx.h"
#include "proxy_error.h"
#include "proxy.h"

struct server_config  server_conf = { 0 };
//-----------------------------------------
void  proxy_ac_err_cb (evutil_socket_t fd, short ev, void *arg)
{
  struct event_base *base = (struct event_base*) arg;

  int err = EVUTIL_SOCKET_ERROR ();
  fprintf (stderr, "Got an error %d (%s) on the listener. "
           "Shutting down.\n", err, evutil_socket_error_to_string (err));

  event_base_loopexit (base, NULL);
}
void  proxy_accept_cb (evutil_socket_t fd, short ev, void *arg)
{
  /* A new connection! Set up a bufferevent for it */
  struct event_base  *base = (struct event_base*) arg;
  //----------------------------------------------------------------------
  int  SlaveSocket = accept (fd, 0, 0);
  if ( SlaveSocket == -1 )
  { fprintf (stderr, "%s\n", strerror (errno));
    return;
  }
  
  set_nonblock (SlaveSocket);

#ifdef _DEBUG
  printf ("connection accepted\n");
#endif
  //----------------------------------------------------------------------
  /* Making the new client */
  struct client *Client = (struct client*) calloc (1U, sizeof (*Client));
  if ( !Client )
  { fprintf (stderr, "%s\n", strerror (errno));
    return;
  }

  Client->base = base;
  //----------------------------------------------------------------------
  int srv_id = (rand () % server_conf.remote_servers_count);
  //----------------------------------------------------------------------
  struct sockaddr_in  SockAddr = {0};
  SockAddr.sin_family      = AF_INET;
  SockAddr.sin_port        = htons     (server_conf.remote_servers[srv_id].port);
  SockAddr.sin_addr.s_addr = inet_addr (server_conf.remote_servers[srv_id].ip);

  Client->server = bufferevent_socket_new (base, -1, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb (Client->server, proxy_read_cb, NULL, proxy_error_cb, NULL);
  //----------------------------------------------------------------------
  /*  Аргументы аналогичны аргументам стандартного системного вызова connect().
   *  Если до этого в bufferevent сокет не был определён, то вызов создаст новый
   *  потоковый (stream) сокет и сделает его неблокируемым. Установлевается
   *  соединение, которая может завершиться успешно (0) или неудачно (-1).
   */
  if ( 0 > bufferevent_socket_connect (Client->server, (struct sockaddr*) &SockAddr, sizeof (SockAddr)) )
  {
    fprintf (stderr, "%s\n", strerror (errno));

    /* Попытка установить соединение неудачна */
    bufferevent_free (Client->server);
    free (Client);

    return;
  }

#ifdef _DEBUG
  printf ("connection to server established\n");
#endif
  /*  BEV_OPT_CLOSE_ON_FREE    - при удалении объекта bufferevent автоматически закрыватся сокет.
   *  BEV_OPT_THREADSAFE       - включает защиту операций с данными с помощью блокировок.
   *  BEV_OPT_DEFER_CALLBACKS  - предотвращает переполнение стека при генерации серии взаимозависимых обратных вызовов,
   *                             позволяетющий создать очередь отложенных обратных вызовов.
   *  BEV_OPT_UNLOCK_CALLBACKS - функции обратного вызова объекта bufferevent будут выполняться без установки блокировок.
   */
  //----------------------------------------------------------------------
  /* Create new bufferized event, linked with client's socket */
  Client->b_ev = bufferevent_socket_new (base, SlaveSocket, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb  (Client->b_ev, proxy_read_cb, NULL , proxy_error_cb, Client);
  /* Ready to get data */
  bufferevent_enable (Client->b_ev, EV_READ | EV_WRITE | EV_PERSIST);
  //----------------------------------------------------------------------
  bufferevent_setwatermark (Client->b_ev, EV_WRITE, SRV_BUF_LOWMARK, 0);
}
//-----------------------------------------
void  proxy_error_cb (struct bufferevent *b_ev, short events, void *arg)
{
  /*   Структура event превращается в триггер  таймаута с помощью функции timeout_set().
   *   Само собой триггер необходимо "взвести" - для этого служит функция timeout_add().
   *   Чтобы "разрядить" триггер и отменить событие таймаута, необходимо воспользоваться
   *   функцией timeout_del().
   */
  
  /*
   *   В заголовочном файле <event2/bufferevent.h> определены флаги,
   *   позволяющие получить информацию о причинах возникновения особой ситуации:
   *   1. Событие произошло во время чтения (BEV_EVENT_READING 0x01) или записи (BEV_EVENT_WRITING 0x02).
   *   2. Обнаружен признак конца файла (BEV_EVENT_EOF 0x10), произошёл критический сбой (BEV_EVENT_ERROR 0x20),
   *      истёк заданный интервал (BEV_EVENT_TIMEOUT 0x40), соединение было установлено (BEV_EVENT_CONNECTED 0x80).
   */
       if ( events & BEV_EVENT_CONNECTED )
  {
#ifdef _DEBUG
    printf ("connection established\n");
#endif
  }
  else if ( events & BEV_EVENT_EOF       )
  {
    struct client   *Client = (struct client*) arg;
    
    struct evbuffer *buf_in  = bufferevent_get_input  (b_ev);
    struct evbuffer *buf_out = bufferevent_get_output ((Client->b_ev == b_ev) ? Client->server : Client->b_ev);

    /* Copy all the data from the input buffer to the output buffer */
    evbuffer_remove_buffer (buf_in, buf_out, evbuffer_get_length (buf_in));

#ifdef _DEBUG
    printf ("Got a close. Length = %u\n", evbuffer_get_length (buf_out) );
#endif // _DEBUG
  }
  else if ( events & BEV_EVENT_ERROR     ) // (BEV_EVENT_EOF | BEV_EVENT_ERROR)
  {
    struct client *Client = (struct client*) arg;

    fprintf (stderr, "Error from bufferevent: '%s'\n",
             evutil_socket_error_to_string (EVUTIL_SOCKET_ERROR ()) );
    
    bufferevent_free (Client->server);
    bufferevent_free (Client->b_ev);

    free (Client);
  }
}
void  proxy_read_cb  (struct bufferevent *b_ev, void *arg)
{

#ifdef _DEBUG
  printf ("data reseived\n");
#endif // _DEBUG

  /* This callback is invoked when there is data to read on b_ev_read */
  struct client  *Client = (struct client*) arg;
  
  struct evbuffer *buf_in  = bufferevent_get_input  (b_ev);
  struct evbuffer *buf_out = bufferevent_get_output ((Client->b_ev == b_ev) ? Client->server : Client->b_ev);

  /* Copy all the data from the input buffer to the output buffer. */
  evbuffer_remove_buffer (buf_in, buf_out, evbuffer_get_length (buf_in));
  
#ifdef _DEBUG
  printf ("response ready\n");
#endif // _DEBUG
}
//-----------------------------------------
