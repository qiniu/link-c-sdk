/*
 * http_trans.c -- Functions for doing transport related stuff including
 * automatically extending buffers and whatnot.
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 5-Aug-1998
 *
 * Copyright (C) 1998 Free Software Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "http_trans.h"
#include "http_global.h"
#include "qupload.h"

#ifdef USE_OPENSSL
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static int          ssl_initialized = 0;
static SSL_METHOD * ssl_method = NULL;
static SSL_CTX    * ssl_context = NULL;

#elif defined (USE_WOLFSSL)
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include "timeoutconn.h"

static int          ssl_initialized = 0;
static WOLFSSL_METHOD * ssl_method = NULL;
static WOLFSSL_CTX    * ssl_context = NULL;
#endif

static int http_trans_buf_free(http_trans_conn *a_conn);

static char cert_file[256]={"/etc/ssl/certs/ca-certificates.crt"};
static char cert_path[256];

void ghttp_set_global_cert_file_path(const char *file, const char *path)
{
    int lenf = strlen(file);
    int lenp = strlen(path);
    assert(lenf < sizeof(cert_file));
    assert(lenp < sizeof(cert_path));
    memcpy(cert_file, file, lenf);
    cert_file[lenf] = 0;
    memcpy(cert_path, path, lenp);
    cert_path[lenp] = 0;
    return;
}

static int get_host_by_name(const char *host, struct sockaddr_in *sinp)
{
    struct addrinfo        *ailist, *aip;
    struct addrinfo        hint;
    int                 err;

    hint.ai_flags = AI_CANONNAME;
    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_addrlen = 0;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;

    if ((err = getaddrinfo(host, NULL, &hint, &ailist)) != 0)
            return err;

    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        if (aip->ai_family == AF_INET) {
            memcpy(sinp, (struct sockaddr_in *)aip->ai_addr, sizeof(struct sockaddr_in));
            break;
        }
    }
    freeaddrinfo(ailist);
    return 0;
}

int
http_trans_connect(http_trans_conn *a_conn)
{
  int dnserr = 0;

  if ((a_conn == NULL) || (a_conn->host == NULL))
    goto ec;
      /* look up the name of the proxy if it's there. */
      if (a_conn->proxy_host)
	{
          char *tmp = strrchr(a_conn->host, ':');
          if (tmp) 
            *tmp = 0;
	  if ((dnserr = get_host_by_name(a_conn->proxy_host, &a_conn->saddr)) != 0)
	    {
	      a_conn->error_type = http_trans_err_type_host;
	      a_conn->error = dnserr;
              if (tmp) 
                *tmp = ':';
	      goto ec;
	    }
            if (tmp) 
              *tmp = ':';
	}
      else
	{
	  /* look up the name */
          char *tmp = strrchr(a_conn->host, ':');
          if (tmp) 
            *tmp = 0;
	  if ((dnserr = get_host_by_name(a_conn->host, &a_conn->saddr)) != 0)
	    {
	      a_conn->error_type = http_trans_err_type_host;
	      a_conn->error = dnserr;
              if (tmp) 
                *tmp = ':';
	      goto ec;
	    }
            if (tmp) 
              *tmp = ':';
	}
      /* set up the saddr */
      a_conn->saddr.sin_family = AF_INET;
      /* set the proxy port */
      if (a_conn->proxy_host)
	a_conn->saddr.sin_port = htons(a_conn->proxy_port);
      else
	a_conn->saddr.sin_port = htons(a_conn->port);
  /* set up the socket */
  if ((a_conn->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      a_conn->error_type = http_trans_err_type_errno;
      a_conn->error = errno;
      goto ec;
    }

  struct timeval tv;
  tv.tv_sec = a_conn->nTimeoutInSecond;
  tv.tv_usec = 0;
  setsockopt(a_conn->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  tv.tv_sec = 5;
  setsockopt(a_conn->sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  /* set up the socket */
  int connBeginTime = time(NULL);
  if (timeout_connect(a_conn->sock,
	      &a_conn->saddr,
	      a_conn->nTimeoutInSecond) < 0)
    {
      int connEndTime = time(NULL);
      int notok = 1;
      int errnobak = errno;
      const char * blkMode = "blksock";
      if (errno == EINPROGRESS) {
              if (socket_is_nonblock(a_conn->sock)) {
                      blkMode = "nonblksock";
              }
      }
      char connErr[128]={0};
      unsigned char * sockip = (unsigned char *)&a_conn->saddr.sin_addr.s_addr;
            snprintf(connErr, sizeof(connErr), "######%s:%d connect fail:en1:%d en2:%d time:%d-%d=%d notok:%d ip:%d.%d.%d.%d\n",
               blkMode, a_conn->sock, errnobak, errno, connEndTime, connBeginTime, connEndTime - connBeginTime, notok,
               sockip[0], sockip[1], sockip[2], sockip[3]);
      LinkGhttpLogger(connErr);
     if (notok) {
      a_conn->error_type = http_trans_err_type_errno;
      a_conn->error = errno;
      goto ec;
     }
    }
#ifdef USE_OPENSSL
  /* initialize the SSL data structures */
  if (a_conn->USE_SSL)
    {
        int err_ret;

      if(a_conn->ssl_conn) 
        {
          SSL_free(a_conn->ssl_conn);
          a_conn->ssl_conn = NULL;
        }
      
      a_conn->ssl_conn = SSL_new(ssl_context);
      if(a_conn->ssl_conn == NULL) {
        a_conn->error_type = http_trans_err_type_ssl;
        a_conn->error = ERR_get_error();
        goto ec;
      }

      SSL_set_fd(a_conn->ssl_conn, a_conn->sock);
      if((err_ret = SSL_connect(a_conn->ssl_conn)) < 1) {
        a_conn->error_type = http_trans_err_type_ssl;
        a_conn->error = SSL_get_error(a_conn->ssl_conn, err_ret);        
        goto ec;
      }
      
      if(a_conn->ssl_cert) 
        {
          X509_free(a_conn->ssl_cert);
          a_conn->ssl_cert = NULL;      
        }
      
      a_conn->ssl_cert = SSL_get_peer_certificate(a_conn->ssl_conn);
      if(a_conn->ssl_cert == NULL) {
        a_conn->error_type = http_trans_err_type_ssl;
        a_conn->error = SSL_get_error(a_conn->ssl_conn, err_ret);        
        goto ec;
      }
  }

#elif defined (USE_WOLFSSL)
  /* initialize the SSL data structures */
  if (a_conn->USE_SSL)
    {
        int err_ret;

      if(a_conn->ssl_conn)
        {
          wolfSSL_free(a_conn->ssl_conn);
          a_conn->ssl_conn = NULL;
        }

      a_conn->ssl_conn = wolfSSL_new(ssl_context);
      if(a_conn->ssl_conn == NULL) {
        a_conn->error_type = http_trans_err_type_ssl;
        goto ec;
      }

      wolfSSL_set_fd(a_conn->ssl_conn, a_conn->sock);
      if((err_ret = wolfSSL_connect(a_conn->ssl_conn)) < 1) {
        a_conn->error_type = http_trans_err_type_ssl;
        a_conn->error = wolfSSL_get_error(a_conn->ssl_conn, err_ret);
        goto ec;
      }
  }

#endif
  
  return 0;
 ec:
  return -1;
}

http_trans_conn *
http_trans_conn_new(int nTimeoutInSecond)
{
  http_trans_conn *l_return = NULL;

  /* allocate a new connection struct */
  l_return = (http_trans_conn *)malloc(sizeof(http_trans_conn));
  memset(l_return, 0, sizeof(http_trans_conn));
  /* default to 80 */
  l_return->port = 80;
  /* default to 1000 bytes at a time */
  l_return->io_buf_chunksize = 1024;
  /* allocate a new trans buffer */
  l_return->io_buf = malloc(l_return->io_buf_chunksize);
  memset(l_return->io_buf, 0, l_return->io_buf_chunksize);
  l_return->io_buf_len = l_return->io_buf_chunksize;
  /* make sure the socket looks like it's closed */
  l_return->sock = -1;
  /* don't use SSL until told to */
  l_return->USE_SSL = 0;
  l_return->nTimeoutInSecond = nTimeoutInSecond;
#ifdef USE_OPENSSL
  l_return->ssl_conn = NULL;
  l_return->ssl_cert = NULL;
#elif defined (USE_WOLFSSL)
  l_return->ssl_conn = NULL;
#endif  
  return l_return;
}

void
http_trans_conn_destroy(http_trans_conn *a_conn)
{
  /* destroy the connection structure. */
  if (a_conn == NULL)
    return;

  /* close the connection */
  http_trans_conn_close(a_conn);

  if (a_conn->io_buf)
    free(a_conn->io_buf);
  
  free(a_conn);
  return;
}

void
http_trans_conn_close(http_trans_conn * a_conn) 
{
  if(a_conn == NULL) 
    return;
  
#ifdef USE_OPENSSL
  if(a_conn->USE_SSL)
    {
      if(a_conn->ssl_conn) 
        {
          SSL_shutdown(a_conn->ssl_conn);
          if(a_conn->sock != -1) 
            {
              close(a_conn->sock);
              a_conn->sock = -1;
            }
          SSL_free(a_conn->ssl_conn);
          a_conn->ssl_conn = NULL;
        }
      if (a_conn->ssl_cert) 
        {
          X509_free(a_conn->ssl_cert);
          a_conn->ssl_cert = NULL;
        }
      a_conn->USE_SSL = 0;
    }
#elif defined (USE_WOLFSSL)
  if(a_conn->USE_SSL)
    {
      if(a_conn->ssl_conn)
        {
          wolfSSL_shutdown(a_conn->ssl_conn);
          if(a_conn->sock != -1)
            {
              close(a_conn->sock);
              a_conn->sock = -1;
            }
          wolfSSL_free(a_conn->ssl_conn);
          a_conn->ssl_conn = NULL;
        }
      a_conn->USE_SSL = 0;
    }
#endif
  
  if (a_conn->sock != -1)
    {
      close(a_conn->sock);
      a_conn->sock = -1;
    }
}

int
http_trans_conn_set_ssl(http_trans_conn * a_conn, int USE_SSL)
{
  if(a_conn == NULL)
    return -1;
  
  if(USE_SSL == a_conn->USE_SSL)
    return -2;

#ifdef USE_OPENSSL
  if(USE_SSL) {
    a_conn->USE_SSL = 1;

    if (ssl_initialized == 0) 
      {
        /* initialize OpenSSL */
        SSLeay_add_ssl_algorithms();
        ssl_method = SSLv23_client_method();    
        SSL_load_error_strings();
        ssl_context = SSL_CTX_new(ssl_method);
        if(ssl_context == NULL) 
          {
            a_conn->error_type = http_trans_err_type_ssl;
            a_conn->error = ERR_get_error();        
            return -3;
          }
        else 
          {
            //SSL_CTX_set_verify(ssl_context, SSL_VERIFY_NONE, 0);
            if(SSL_CTX_load_verify_locations(ssl_context, cert_file, NULL) != 0) {
                ssl_initialized = 1;
                return 0;
            } else {
                a_conn->error_type = http_trans_err_type_local_ca;
                a_conn->error = ERR_get_error();        
                return -4;
            }
          }
      }    
  }

#elif defined (USE_WOLFSSL)
  if(USE_SSL) {
    a_conn->USE_SSL = 1;

    if (ssl_initialized == 0)
      {
        /* initialize OpenSSL */
        wolfSSL_Init();
        ssl_method = wolfSSLv23_client_method();
        wolfSSL_load_error_strings();
        ssl_context = wolfSSL_CTX_new(ssl_method);
        if(ssl_context == NULL)
          {
            a_conn->error_type = http_trans_err_type_ssl;
            return -3;
          }
        else
          {
            //wolfSSL_CTX_set_verify(ssl_context, WOLFSSL_VERIFY_NONE, 0);
            if(wolfSSL_CTX_load_verify_locations(ssl_context, cert_file, NULL) != 0) {
                ssl_initialized = 1;
                return 0;
            } else {
                a_conn->error_type = http_trans_err_type_local_ca;
                return -4;
            }
          }
      }
  }
  
#else
  a_conn->USE_SSL = 0;
#endif
  return -6;
}

const char *
http_trans_get_host_error(int a_herror)
{
  switch (a_herror)
    {
    case HOST_NOT_FOUND:
      return "Host not found";
    case NO_ADDRESS:
      return "An address is not associated with that host";
    case NO_RECOVERY:
      return "An unrecoverable name server error occured";
    case TRY_AGAIN:
      return "A temporary error occurred on an authoritative name server.  Please try again later.";
    default:
      return "No error or error not known.";
    }
}

int
http_trans_append_data_to_buf(http_trans_conn *a_conn,
			      const char *a_data,
			      int   a_data_len)
{
  if (http_trans_buf_free(a_conn) < a_data_len)
    {
      a_conn->io_buf = realloc(a_conn->io_buf, a_conn->io_buf_len + a_data_len);
      a_conn->io_buf_len += a_data_len;
    }
  memcpy(&a_conn->io_buf[a_conn->io_buf_alloc], a_data, a_data_len);
  a_conn->io_buf_alloc += a_data_len;
  return 1;
}

int
http_trans_read_into_buf(http_trans_conn *a_conn)
{
  int l_read = 0;
  int l_bytes_to_read = 0;

  /* set the length if this is the first time */
  if (a_conn->io_buf_io_left == 0)
    {
      a_conn->io_buf_io_left = a_conn->io_buf_chunksize;
      a_conn->io_buf_io_done = 0;
    }
  /* make sure there's enough space */
  if (http_trans_buf_free(a_conn) < a_conn->io_buf_io_left)
    {
      a_conn->io_buf = realloc(a_conn->io_buf,
			       a_conn->io_buf_len + a_conn->io_buf_io_left);
      a_conn->io_buf_len += a_conn->io_buf_io_left;
    }
  /* check to see how much we should try to read */
  if (a_conn->io_buf_io_left > a_conn->io_buf_chunksize)
    l_bytes_to_read = a_conn->io_buf_chunksize;
  else
    l_bytes_to_read = a_conn->io_buf_io_left;
  
  /* read in some data */
  if(a_conn->USE_SSL)
    {
#ifdef USE_OPENSSL
      if ((a_conn->last_read = l_read = 
           SSL_read(a_conn->ssl_conn,
                    &a_conn->io_buf[a_conn->io_buf_alloc],
                    l_bytes_to_read)) < 0)
        {
          long int sslerr = SSL_get_error(a_conn->ssl_conn, l_read);
          a_conn->error_type = http_trans_err_type_ssl;
          a_conn->error = sslerr;
          if((sslerr == SSL_ERROR_WANT_READ) ||
             (sslerr == SSL_ERROR_WANT_WRITE)) 
            l_read = 0;
          else
            return HTTP_TRANS_ERR;
        }
      else if (l_read == 0) {
        return HTTP_TRANS_DONE;
      }

#elif defined (USE_WOLFSSL)
      if ((a_conn->last_read = l_read =
           wolfSSL_read(a_conn->ssl_conn,
                    &a_conn->io_buf[a_conn->io_buf_alloc],
                    l_bytes_to_read)) < 0)
        {
          long int sslerr = wolfSSL_get_error(a_conn->ssl_conn, l_read);
          a_conn->error_type = http_trans_err_type_ssl;
          a_conn->error = sslerr;
          if((sslerr == WOLFSSL_ERROR_WANT_READ) ||
             (sslerr == WOLFSSL_ERROR_WANT_WRITE))
            l_read = 0;
          else
            return HTTP_TRANS_ERR;
        }
      else if (l_read == 0) {
        return HTTP_TRANS_DONE;
      }

#else 
      return HTTP_TRANS_ERR;
#endif
    }
  else if ((a_conn->last_read = l_read = 
            read(a_conn->sock,
                 &a_conn->io_buf[a_conn->io_buf_alloc],
                 l_bytes_to_read)) < 0)
    {
      a_conn->error_type = http_trans_err_type_errno;
      a_conn->error = errno;
      if (errno == EINTR)
	l_read = 0;
      else
	return HTTP_TRANS_ERR;
    }
  else if (l_read == 0)
    return HTTP_TRANS_DONE;
  
  /* mark the buffer */
  a_conn->io_buf_io_left -= l_read;
  a_conn->io_buf_io_done += l_read;
  a_conn->io_buf_alloc += l_read;
  
  /* generate the result */
  if (a_conn->io_buf_io_left == 0)
    return HTTP_TRANS_DONE;
  
  return HTTP_TRANS_NOT_DONE;
}

int
http_trans_write_buf(http_trans_conn *a_conn)
{
  int l_written = 0;

  if (a_conn->io_buf_io_left == 0)
    {
      a_conn->io_buf_io_left = a_conn->io_buf_alloc;
      a_conn->io_buf_io_done = 0;
    }
  /* write out some data */
  if(a_conn->USE_SSL)
    {
#ifdef USE_OPENSSL
      if ((a_conn->last_read = l_written = 
           SSL_write(a_conn->ssl_conn, 
                     &a_conn->io_buf[a_conn->io_buf_io_done],
                     a_conn->io_buf_io_left)) <= 0) 
        {
          long int sslerr = SSL_get_error(a_conn->ssl_conn, l_written);
          a_conn->error_type = http_trans_err_type_ssl;
          a_conn->error = sslerr;
          if ((sslerr == SSL_ERROR_WANT_READ) ||
              (sslerr == SSL_ERROR_WANT_WRITE)) 
            l_written = 0;
          else
            return HTTP_TRANS_ERR;
        }

#elif defined (USE_WOLFSSL)
      if ((a_conn->last_read = l_written =
           wolfSSL_write(a_conn->ssl_conn,
                     &a_conn->io_buf[a_conn->io_buf_io_done],
                     a_conn->io_buf_io_left)) <= 0)
        {
          long int sslerr = wolfSSL_get_error(a_conn->ssl_conn, l_written);
          a_conn->error_type = http_trans_err_type_ssl;
          a_conn->error = sslerr;
          if ((sslerr == WOLFSSL_ERROR_WANT_READ) ||
              (sslerr == WOLFSSL_ERROR_WANT_WRITE))
            l_written = 0;
          else
            return HTTP_TRANS_ERR;
        }

#else 
      return HTTP_TRANS_ERR;
#endif
    }
  else if ((a_conn->last_read = l_written = write (a_conn->sock,
                                                   &a_conn->io_buf[a_conn->io_buf_io_done],
                                                   a_conn->io_buf_io_left)) <= 0)
    {
      a_conn->error_type = http_trans_err_type_errno;
      a_conn->error = errno;
      if (errno == EINTR)
        l_written = 0;
      else
        return HTTP_TRANS_ERR;
    }
  
  if (l_written == 0)
    return HTTP_TRANS_DONE;
  /* advance the counters */
  a_conn->io_buf_io_left -= l_written;
  a_conn->io_buf_io_done += l_written;
  if (a_conn->io_buf_io_left == 0)
    return HTTP_TRANS_DONE;
  return HTTP_TRANS_NOT_DONE;
}

void
http_trans_buf_reset(http_trans_conn *a_conn)
{
  if (a_conn->io_buf)
    free(a_conn->io_buf);
  a_conn->io_buf = malloc(a_conn->io_buf_chunksize);
  memset(a_conn->io_buf, 0, a_conn->io_buf_chunksize);
  a_conn->io_buf_len = a_conn->io_buf_chunksize;
  a_conn->io_buf_alloc = 0;
  a_conn->io_buf_io_done = 0;
  a_conn->io_buf_io_left = 0;
}

void
http_trans_buf_clip(http_trans_conn *a_conn, char *a_clip_to)
{
  int l_bytes = 0;
  
  /* get the number of bytes to clip off of the front */
  l_bytes = a_clip_to - a_conn->io_buf;
  if (l_bytes > 0)
    {
      memmove(a_conn->io_buf, a_clip_to, a_conn->io_buf_alloc - l_bytes);
      a_conn->io_buf_alloc -= l_bytes;
    }
  a_conn->io_buf_io_done = 0;
  a_conn->io_buf_io_left = 0;
}

char *
http_trans_buf_has_patt(char *a_buf, int a_len,
			char *a_pat, int a_patlen)
{
  int i = 0;
  for ( ; i <= ( a_len - a_patlen ); i++ )
    {
      if (a_buf[i] == a_pat[0])
	{
	  if (memcmp(&a_buf[i], a_pat, a_patlen) == 0)
	    return &a_buf[i];
	}
    }
  return NULL;
}

/* static functions */

static int
http_trans_buf_free(http_trans_conn *a_conn)
{
  return (a_conn->io_buf_len - a_conn->io_buf_alloc);
}
