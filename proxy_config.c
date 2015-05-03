#include "stdafx.h"
#include "proxy_error.h"
#include "proxy.h"

//-----------------------------------------
const char*  strmyerror ()
{
  const char* strerr;

  switch ( my_errno )
  {
    default: strerr = NULL;                                      break;

    case SRV_ERR_PARAM: strerr = "Invalid function argument."; break;
    case SRV_ERR_INPUT: strerr = "Input incorrect user data."; break;
    case SRV_ERR_LIBEV: strerr = "Incorrect libevent entity."; break;
    case SRV_ERR_RCMMN: strerr = "Incorrect request command."; break;
    case SRV_ERR_RFREE: strerr = "Dispatched a free request."; break;
  }

  my_errno = SRV_ERR_NONE;
  return strerr;
}
//-----------------------------------------
int   set_nonblock (evutil_socket_t fd)
{
  int flags;
#ifdef O_NONBLOCK
  if ( -1 == (flags = fcntl (fd, F_GETFL, 0)) )
    flags = 0;
  return fcntl (fd, F_SETFL, flags | O_NONBLOCK);
#else
  flags = 1;
  return ioctl (fd, FIOBIO, &flags);
#endif
}
//-----------------------------------------
int   server_config_init  (srv_conf *conf, char *path)
{
  bool fail = false;
  //-----------------------
  srand ((unsigned int) clock ());
  //-----------------------
  conf->ptr_server_name = SRV_SERVER_NAME;
  //-----------------------
  strcpy (conf->ip, "0.0.0.0");
  //-----------------------
  size_t filesize = 0;
  struct stat sb;
  if ( path && stat (path, &sb) == 0 )
  {
    strcpy (conf->config_path, path);
    filesize = sb.st_size;
  }
  else
  {
    my_errno = SRV_ERR_INPUT;
    fprintf (stderr, "%s\n", strmyerror ());
    
    if ( stat (SRV_CONFDEFNAME, &sb) == 0 )
    {
      strcpy (conf->config_path, SRV_CONFDEFNAME);
      filesize = sb.st_size;
    }
    else
    {
      my_errno = SRV_ERR_INPUT;
      fprintf (stderr, "%s\n", strmyerror ());
      fail = true;
      goto CONF_FREE;
    }
  }
  //-----------------------  
  int  bytes = 0U;
  if ( 0 >= (bytes = readlink ("/proc/self/exe", conf->server_path, FILENAME_MAX - 1U)) )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    strcpy  (conf->server_path, "");
  }
  else conf->server_path[bytes] = '\0';  
  //-----------------------
  FILE* f_conf = fopen (conf->config_path, "rt");
  if ( !f_conf )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    fail = true;
    goto CONF_FREE;
  }

  char *fileline = malloc ((filesize + 1U) * sizeof (char));
  if ( !fileline )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    fail = true;
    goto CONF_FREE;
  }
  
  fread (fileline, filesize, 1U, f_conf);
  fileline[filesize] = '\0';

  size_t count = 0;
  char* str = fileline;
  while ( (str = strchr (str, ',')) )
  {
    ++str;
    ++count;
  }
  //-----------------------
  conf->remote_servers_count = count;
  conf->remote_servers = calloc (count, sizeof (*conf->remote_servers));
  if ( !conf->remote_servers )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    fail = true;
    goto CONF_FREE;
  }
  //-----------------------

  str = fileline;
  int str_offset = 0;
  const uint16_t  max_port = 65535U;
  if ( 1 != sscanf (fileline, "%hu,%n", &conf->port, &str_offset) || conf->port >= max_port )
  {
    my_errno = SRV_ERR_INPUT;
    fprintf (stderr, "%s\n", strmyerror ());
    conf->port = 8080;
  }
  str += str_offset;
  //-----------------------
  for ( size_t i = 0U; i < count; ++i )
  {
    if ( 0 >= sscanf (str, " %s:%hu%n",
                       conf->remote_servers[i].ip,
                      &conf->remote_servers[i].port,
                      &str_offset) )
    {
      fprintf (stderr, "%s\n", strerror (errno));
      fail = true;
      goto CONF_FREE;
    }

    printf ("%s:%hu\n", conf->remote_servers[i].ip, conf->remote_servers[i].port);
    str += str_offset;
  }
  
  //-----------------------
CONF_FREE:;
  fclose (f_conf);
  if ( fail )
    server_config_free (conf);
  //-----------------------
  return fail;
}
void  server_config_print (srv_conf *conf, FILE *stream)
{
  fprintf (stream, ">>> http server config:\n\n\tname: '%s'   server path: '%s'\n"
                   "\tconf: '%s'\n\tport: %u   ip: '%s'\n\n", conf->ptr_server_name,
                   conf->server_path, conf->config_path, conf->port, conf->ip);
}
void  server_config_free  (srv_conf *conf)
{
  //-----------------------
  free   (conf->remote_servers);
  memset (conf, 0, sizeof (*conf));
}
//-----------------------------------------
#include <getopt.h>   /* for getopt_long finction */
int   parse_console_parameters (int argc, char **argv, srv_conf *conf)
{
  char  *conf_opt = NULL;
  static struct option long_options[] =
  {
    { "conf", required_argument, 0, 'c' },
    { 0, 0, 0, 0 }
  };
  //-----------------------
  int c, option_index = 0;
  while ( -1 != (c = getopt_long (argc, argv, "c:",
                                  long_options, &option_index)) )
  {
    switch ( c )
    {
      case 0:
        printf ("option %s", long_options[option_index].name);
        break;

      case 'c':
        conf_opt = optarg;
        break;

      case '?':
        /* getopt_long already printed an error message. */
        printf ("using:\n./wwwd -c|--conf <conf>\n\n");
        break;

      default:
        printf ("?? getopt returned character code 0%o ??\n", c);
        break;
    }
  }
  //-----------------------
  if ( optind < argc )
  {
    printf ("non-option ARGV-elements: ");
    while ( optind < argc )
      printf ("%s ", argv[optind++]);
    printf ("\n");
  }
  //-----------------------
  return  server_config_init (conf, conf_opt);
}
//-----------------------------------------
