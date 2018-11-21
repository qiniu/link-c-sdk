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

#include "http_trans.h"
#include "http_global.h"

#ifdef USE_SSL
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static int          ssl_initialized = 0;
static SSL_METHOD * ssl_method = NULL;
static SSL_CTX    * ssl_context = NULL;
#endif

static int http_trans_buf_free(http_trans_conn *a_conn);

int
http_trans_connect(http_trans_conn *a_conn)
{
  int err_ret;

  if ((a_conn == NULL) || (a_conn->host == NULL))
    goto ec;
  if (a_conn->hostinfo == NULL)
    {
      /* look up the name of the proxy if it's there. */
      if (a_conn->proxy_host)
	{
	  if ((a_conn->hostinfo = gethostbyname(a_conn->proxy_host)) == NULL)
	    {
	      a_conn->error_type = http_trans_err_type_host;
	      a_conn->error = h_errno;
	      goto ec;
	    }
	}
      else
	{
	  /* look up the name */
	  if ((a_conn->hostinfo = gethostbyname(a_conn->host)) == NULL)
	    {
	      a_conn->error_type = http_trans_err_type_host;
	      a_conn->error = h_errno;
	      goto ec;
	    }
	}
      /* set up the saddr */
      a_conn->saddr.sin_family = AF_INET;
      /* set the proxy port */
      if (a_conn->proxy_host)
	a_conn->saddr.sin_port = htons(a_conn->proxy_port);
      else
	a_conn->saddr.sin_port = htons(a_conn->port);
      /* copy the name info */
      memcpy(&a_conn->saddr.sin_addr.s_addr,
	     a_conn->hostinfo->h_addr_list[0],
	     sizeof(unsigned long));
    }
  /* set up the socket */
  if ((a_conn->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      a_conn->error_type = http_trans_err_type_errno;
      a_conn->error = errno;
      goto ec;
    }
  /* set up the socket */
  if (connect(a_conn->sock,
	      (struct sockaddr *)&a_conn->saddr,
	      sizeof(struct sockaddr)) < 0)
    {
      a_conn->error_type = http_trans_err_type_errno;
      a_conn->error = errno;
      goto ec;
    }
#ifdef USE_SSL
  /* initialize the SSL data structures */
  if (a_conn->use_ssl) 
    {
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
#endif
  
  return 0;
 ec:
  return -1;
}

http_trans_conn *
http_trans_conn_new(void)
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
  l_return->use_ssl = 0;
#ifdef USE_SSL
  l_return->ssl_conn = NULL;
  l_return->ssl_cert = NULL;
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
  
#ifdef USE_SSL
  if(a_conn->use_ssl) 
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
      a_conn->use_ssl = 0;
    }
#endif
  
  if (a_conn->sock != -1)
    {
      close(a_conn->sock);
      a_conn->sock = -1;
    }
}

void
http_trans_conn_set_ssl(http_trans_conn * a_conn, int use_ssl) 
{
  if(a_conn == NULL)
    return;
  
  if(use_ssl == a_conn->use_ssl) 
    return;

#ifdef USE_SSL
  if(use_ssl) {
    a_conn->use_ssl = 1;

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
            return;
            ssl_initialized = 0;
          }
        else 
          {
            //SSL_CTX_set_verify(ssl_context, SSL_VERIFY_NONE, 0);
            if(SSL_CTX_load_verify_locations(ssl_context, "/etc/ssl/certs/ca-certificates.crt", NULL) != 0) 
                ssl_initialized = 1;
          }
      }    
  }
  
#else
  a_conn->use_ssl = 0;
#endif
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
			      char *a_data,
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
  if(a_conn->use_ssl) 
    {
#ifdef USE_SSL
      if ((a_conn->last_read = l_read = 
           SSL_read(a_conn->ssl_conn,
                    &a_conn->io_buf[a_conn->io_buf_alloc],
                    l_bytes_to_read)) < 0)
        {
          long int sslerr = SSL_get_error(a_conn->ssl_conn, l_read);
          if((sslerr == SSL_ERROR_WANT_READ) ||
             (sslerr == SSL_ERROR_WANT_WRITE)) 
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
  if(a_conn->use_ssl) 
    {
#ifdef USE_SSL
      if ((a_conn->last_read = l_written = 
           SSL_write(a_conn->ssl_conn, 
                     &a_conn->io_buf[a_conn->io_buf_io_done],
                     a_conn->io_buf_io_left)) <= 0) 
        {
          long int sslerr = SSL_get_error(a_conn->ssl_conn, l_written);
          if ((sslerr == SSL_ERROR_WANT_READ) ||
              (sslerr == SSL_ERROR_WANT_WRITE)) 
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
