/*
 * ghttp.c -- Implementation of the public interface to http functions
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 21-Aug-1998
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "ghttp.h"
#include "http_uri.h"
#include "http_hdrs.h"
#include "http_trans.h"
#include "http_req.h"
#include "http_resp.h"
#include "http_date.h"
#include "http_global.h"
#include "http_base64.h"

struct _ghttp_request
{
  http_uri           *uri;
  http_uri           *proxy;
  http_req           *req;
  http_resp          *resp;
  http_trans_conn    *conn; 
  const char         *errstr;
  int                 connected;
  ghttp_proc          proc;
  char               *username;
  char               *password;
  char               *authtoken;
  char               *proxy_username;
  char               *proxy_password;
  char               *proxy_authtoken;
  int                 secure_uri;
  int                 nTimeoutInSecond;
  int                 isWs;
#if defined(WITH_OPENSSL) || defined(WITH_WOLFSSL)
  ghttp_ssl_cert_cb   cert_cb;
  void               *cert_cb_data;
#endif
};



static const char *basic_header = "Basic ";

ghttp_request *
ghttp_request_new(void)
{
  struct _ghttp_request *l_return = NULL;

  /* create everything */
  l_return = malloc(sizeof(struct _ghttp_request));
  if (l_return == NULL) {
    return l_return;
  }
  memset(l_return, 0, sizeof(struct _ghttp_request));
  l_return->uri = http_uri_new();
  l_return->proxy = http_uri_new();
  l_return->req = http_req_new();
  l_return->resp = http_resp_new();
  l_return->nTimeoutInSecond = 10;
  l_return->conn = http_trans_conn_new(l_return->nTimeoutInSecond);
  l_return->secure_uri = 0;
  return l_return;
}

void ghttp_set_timeout(ghttp_request *a_request, int nTimeoutInSecond)
{
  a_request->nTimeoutInSecond = nTimeoutInSecond;
  a_request->conn->nTimeoutInSecond = nTimeoutInSecond;
}

int ghttp_is_timeout(ghttp_request *a_request)
{
	switch(a_request->conn->error_type) {
		//case http_trans_err_type_host:
		case http_trans_err_type_errno:
			if (a_request->conn->error == EAGAIN || a_request->conn->error == EWOULDBLOCK)
				return 1;
			else
				return 0;
#if defined(WITH_OPENSSL) || defined(WITH_WOLFSSL)
		case http_trans_err_type_ssl:
			if (a_request->conn->error==SSL_ERROR_WANT_READ || a_request->conn->error==SSL_ERROR_WANT_WRITE)
				return 1;
			else
				return 0;
#endif
		default:
			break;
	}
	return 0;
}

void
ghttp_request_destroy(ghttp_request *a_request)
{
  if (!a_request)
    return;
  /* make sure that the socket was shut down. */
  if (a_request->conn->sock >= 0)
    {
      http_trans_conn_close(a_request->conn);
    }
  
  /* destroy everything else */
  if (a_request->uri)
    http_uri_destroy(a_request->uri);
  if (a_request->proxy)
    http_uri_destroy(a_request->proxy);
  if (a_request->req)
    http_req_destroy(a_request->req);
  if (a_request->resp)
    http_resp_destroy(a_request->resp);
  if (a_request->conn)
    http_trans_conn_destroy(a_request->conn);
  /* destroy username info. */
  if (a_request->username)
    {
      free(a_request->username);
      a_request->username = NULL;
    }
  if (a_request->password)
    {
      free(a_request->password);
      a_request->password = NULL;
    }
  if (a_request->authtoken)
    {
      free(a_request->authtoken);
      a_request->authtoken = NULL;
    }
  /* destroy proxy authentication */
  if (a_request->proxy_username)
    {
      free(a_request->proxy_username);
      a_request->proxy_username = NULL;
    }
  if (a_request->proxy_password)
    {
      free(a_request->proxy_password);
      a_request->proxy_password = NULL;
    }
  if (a_request->proxy_authtoken)
    {
      free(a_request->proxy_authtoken);
      a_request->proxy_authtoken = NULL;
    }
  if (a_request)
    free(a_request);
  return;
}

int
ghttp_uri_validate(char *a_uri)
{
  if (!a_uri)
    return -1;
  /* you can do this... */
  return(http_uri_parse(a_uri, NULL));
}

int
ghttp_set_uri(ghttp_request *a_request, const char *a_uri)
{
  int l_rv = 0;
  http_uri *l_new_uri = NULL;

  if ((!a_request) || (!a_uri))
    return -1;
  /* set the uri */
  l_new_uri = http_uri_new();
  l_rv = http_uri_parse(a_uri, l_new_uri);
  if (l_rv < 0)
    {
      http_uri_destroy(l_new_uri);
      return -1;
    }
  if (a_request->uri)
    {
      /* check to see if this has been set yet. */
      if (a_request->uri->host &&
	  a_request->uri->port &&
	  a_request->uri->resource)
	{
	  /* check to see if we just need to change the resource */
	  if ((!strcmp(a_request->uri->host, l_new_uri->host)) &&
	      (a_request->uri->port == l_new_uri->port))
	    {
	      free(a_request->uri->resource);
	      /* make a copy since we're about to destroy it */
	      a_request->uri->resource = strdup(l_new_uri->resource);
	      http_uri_destroy(l_new_uri);
	    }
	  else
	    {
	      http_uri_destroy(a_request->uri);
	      a_request->uri = l_new_uri;
	    }
	}
      else
	{
	  http_uri_destroy(a_request->uri);
	  a_request->uri = l_new_uri;
	}

#if defined(WITH_OPENSSL) || defined(WITH_WOLFSSL)
      if (!strcmp(a_request->uri->proto, "https"))
        {
          a_request->secure_uri = 1;
        }
#endif
    }
  return 0;
}

int
ghttp_set_proxy(ghttp_request *a_request, char *a_uri)
{
  int l_rv = 0;
  
  if ((!a_request) || (!a_uri))
    return -1;
  /* set the uri */
  l_rv = http_uri_parse(a_uri, a_request->proxy);
  if (l_rv < 0)
    return -1;
  return 0;
}

int
ghttp_set_type(ghttp_request *a_request, ghttp_type a_type)
{
  int l_return = 0;

  /* check to make sure that the args are ok */
  if (!a_request)
    return -1;
  /* switch on all of the supported types */
  switch(a_type)
    {
    case ghttp_type_get:
      a_request->req->type = http_req_type_get;
      break;
    case ghttp_type_options:
      a_request->req->type = http_req_type_options;
      break;
    case ghttp_type_head:
      a_request->req->type = http_req_type_head;
      break;
    case ghttp_type_post:
      a_request->req->type = http_req_type_post;
      break;
    case ghttp_type_put:
      a_request->req->type = http_req_type_put;
      break;
    case ghttp_type_delete:
      a_request->req->type = http_req_type_delete;
      break;
    case ghttp_type_trace:
      a_request->req->type = http_req_type_trace;
      break;
    case ghttp_type_connect:
      a_request->req->type = http_req_type_connect;
      break;
    case ghttp_type_propfind:
      a_request->req->type = http_req_type_propfind;
      break;
    case ghttp_type_proppatch:
      a_request->req->type = http_req_type_proppatch;
      break;
    case ghttp_type_mkcol:
      a_request->req->type = http_req_type_mkcol;
      break;
    case ghttp_type_copy:
      a_request->req->type = http_req_type_copy;
      break;
    case ghttp_type_move:
      a_request->req->type = http_req_type_move;
      break;
    case ghttp_type_lock:
      a_request->req->type = http_req_type_lock;
      break;
    case ghttp_type_unlock:
      a_request->req->type = http_req_type_unlock;
      break;
    default:
      l_return = -1;
      break;
    }
  return l_return;
}

int
ghttp_set_body(ghttp_request *a_request, const char *a_body, int a_len)
{
  /* check to make sure the request is there */
  if (!a_request)
    return -1;
  /* check to make sure the body is there */
  if ((a_len > 0) && (a_body == NULL))
    return -1;
  /* check to make sure that it makes sense */
  if ((a_request->req->type != http_req_type_post) &&
      (a_request->req->type != http_req_type_put) &&
      (a_request->req->type != http_req_type_proppatch) &&
      (a_request->req->type != http_req_type_propfind) &&
      (a_request->req->type != http_req_type_lock))
    return -1;
  /* set the variables */
  a_request->req->body = a_body;
  a_request->req->body_len = a_len;
  return 0;
}

int ghttp_set_body3(ghttp_request *a_request, const char *a_body, int a_len,
                    const char *a1_body, int a1_len, const char *a2_body, int a2_len) {
        /* check to make sure the request is there */
        if (!a_request)
                return -1;
        /* check to make sure the body is there */
        if ((a_len > 0) && (a_body == NULL))
                return -1;
        /* check to make sure that it makes sense */
        if ((a_request->req->type != http_req_type_post) &&
            (a_request->req->type != http_req_type_put) &&
            (a_request->req->type != http_req_type_proppatch) &&
            (a_request->req->type != http_req_type_propfind) &&
            (a_request->req->type != http_req_type_lock))
                return -1;
        /* set the variables */
        a_request->req->body = a_body;
        a_request->req->body_len = a_len;
        a_request->req->body1 = a1_body;
        a_request->req->body1_len = a1_len;
        a_request->req->body2 = a2_body;
        a_request->req->body2_len = a2_len;
        return 0;
}

int
ghttp_set_sync(ghttp_request *a_request,
	       ghttp_sync_mode a_mode)
{
  if (!a_request)
    return -1;
  if (a_mode == ghttp_sync)
    a_request->conn->sync = HTTP_TRANS_SYNC;
  else if (a_mode == ghttp_async)
    a_request->conn->sync = HTTP_TRANS_ASYNC;
  else
    return -1;
  return 0;
}

int
ghttp_prepare(ghttp_request *a_request)
{
  /* only allow http requests if no proxy has been set */
  if (!a_request->proxy->host && a_request->uri->proto &&
      strcmp(a_request->uri->proto, "http") &&
      strcmp(a_request->uri->proto, "https"))        
    return 1;
  
  /* check to see if we have to set up the
     host information */
  if ((a_request->conn->host == NULL) ||
      (a_request->conn->host != a_request->uri->host) ||
      (a_request->conn->port != a_request->uri->port) ||
      (a_request->conn->USE_SSL != a_request->secure_uri) ||
      (a_request->conn->proxy_host != a_request->proxy->host) ||
      (a_request->conn->proxy_port != a_request->proxy->port)) 
    {
      /* reset everything. */
      a_request->conn->host = a_request->uri->host;
      a_request->req->host = a_request->uri->host;
      a_request->req->full_uri = a_request->uri->full;
      a_request->conn->port = a_request->uri->port;
      a_request->conn->proxy_host = a_request->proxy->host;
      a_request->conn->proxy_port = a_request->proxy->port;
      if (a_request->secure_uri && http_trans_conn_set_ssl(a_request->conn, a_request->secure_uri) != 0)
          return HTTP_TRANS_ERR;
      
      /* close the socket if it looks open */
      if (a_request->conn->sock >= 0)
	{
          http_trans_conn_close(a_request->conn);
	  a_request->connected = 0;
	}
    }
  /* check to see if we need to change the resource. */
  if ((a_request->req->resource == NULL) ||
      (a_request->req->resource != a_request->uri->resource))
    {
      a_request->req->resource = a_request->uri->resource;
      a_request->req->host = a_request->uri->host;
    }
  /* set the authorization header */
  if ((a_request->authtoken != NULL) &&
      (strlen(a_request->authtoken) > 0))
    {
      http_hdr_set_value(a_request->req->headers,
			 http_hdr_Authorization,
			 a_request->authtoken);
    }
  else
    {
      http_hdr_set_value(a_request->req->headers,
			 http_hdr_WWW_Authenticate,
			 NULL);
    }
  /* set the proxy authorization header */
  if ((a_request->proxy_authtoken != NULL) &&
      (strlen(a_request->proxy_authtoken) > 0))
    {
      http_hdr_set_value(a_request->req->headers,
			 http_hdr_Proxy_Authorization,
			 a_request->proxy_authtoken);
    }
  http_req_prepare(a_request->req);
  return 0;
}

static int get_fix_random_str(char *buf, int bufLen, int len) {
        int i = 0, val = 0;
        const char *base = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789==========++++++++++";
        int base_len = 82;
        
        srand((unsigned int) time(NULL));
        
        len = bufLen - 1 >= len ? len : bufLen - 1;
        for (; i < len; i++) {
                val = 1 + (int) ((float) (base_len - 1) * rand() / (RAND_MAX + 1.0));
                buf[i] = base[val];
        }
        return len;
}

ghttp_status
ghttp_process_upgrade_websocket(ghttp_request *a_request) {
        char key[21];
        memset(key, 0, sizeof(key));
        get_fix_random_str(key, sizeof(key), sizeof(key) - 1);
        ghttp_set_type(a_request, ghttp_type_get);
        ghttp_set_header(a_request, "Connection", "Upgrade");
        ghttp_set_header(a_request, "Sec-WebSocket-Version", "13");
        ghttp_set_header(a_request, "Upgrade", "websocket");
        ghttp_set_header(a_request, "Sec-WebSocket-Key", key);
        a_request->isWs = 1;
        int ret = ghttp_prepare(a_request);
        if (ret != 0)
                return ret;
        return ghttp_process(a_request);
}

void mask(unsigned char* original, int len, unsigned char* maskKey) {
        int i = 0;
        for (i = 0; i < len; i++) {
                original[i] = (unsigned char) (original[i] ^ maskKey[i % 4]);
        }
        return;
}

int ghttp_websocket_send(ghttp_request *a_request, char *data, int len) {
        unsigned char wh[16];
        memset(wh, 0, sizeof(wh));
        int i = 0;
        wh[i++] = 0x82;// fin, binary
        if (len < 126) {
                wh[i++] = len | 0x80;
        } else if (len < 65536) {
                wh[i++] = 126 | 0x80;
                wh[i++] = len/256;
                wh[i++] = len%256;
        } else {
                wh[i++] = 127 | 0x80;
                wh[i++] = 0;
                wh[i++] = 0;
                wh[i++] = 0;
                wh[i++] = 0;
                wh[i++] = len>>24;
                wh[i++] = len>>16;
                wh[i++] = len>>8;
                wh[i++] = len;
        }
        int r;
        get_fix_random_str((char*)(&r), sizeof(int), sizeof(int));
        memcpy(wh+i, &r, sizeof(int));
        i+=4;
        
        memcpy(data-i, wh, i);
        mask((unsigned char*)data, len, (unsigned char *)(&r));
        return http_req_send_websocket(a_request->req, a_request->conn, data-i, len+i);
}

int  ghttp_websocket_recv(ghttp_request *a_request, char * msg, int len) {
        return http_req_read_websocket(a_request->req, a_request->conn, msg, len);
}

ghttp_status
ghttp_process (ghttp_request *a_request)
{
  int l_rv = 0;

  if (a_request->proc == ghttp_proc_none)
    a_request->proc = ghttp_proc_request;
  if (a_request->proc == ghttp_proc_request)
    {
      if (a_request->connected == 0)
	{
	  if (http_trans_connect(a_request->conn, a_request->isWs) < 0)
	    {
	      if (a_request->conn->error_type == http_trans_err_type_errno)
		a_request->errstr = strerror(a_request->conn->error);
	      else if(a_request->conn->error_type == http_trans_err_type_host)
		a_request->errstr = gai_strerror(a_request->conn->error);
	      return ghttp_error;
	    }
#ifdef WITH_OPENSSL
          /* call callback to verify certificate if it's an SSL connection*/
          if(a_request->conn->USE_SSL)
            { 
              if(a_request->conn->ssl_cert &&
                 ((a_request->cert_cb == NULL) ||
                  (*a_request->cert_cb)(a_request, a_request->conn->ssl_cert, 
                                        a_request->cert_cb_data))) 
                {
                  a_request->connected = 1;     
                }
              else 
                {
                  return ghttp_error;
                }
            }
          else 
            a_request->connected = 1;

#elif defined (WITH_WOLFSSL)
          /* call callback to verify certificate if it's an SSL connection*/
          if(a_request->conn->USE_SSL)
            {
              if(a_request->cert_cb == NULL)
              {
                a_request->connected = 1;
              }
              else
                {
                  return ghttp_error;
                }
            }
          else
            a_request->connected = 1;
#else 
          a_request->connected = 1;
#endif	  
	}
      l_rv = http_req_send(a_request->req, a_request->conn);
      if (l_rv == HTTP_TRANS_ERR)
	return ghttp_error;
      if (l_rv == HTTP_TRANS_NOT_DONE)
	return ghttp_not_done;
      if (l_rv == HTTP_TRANS_DONE)
	{
	  a_request->proc = ghttp_proc_response_hdrs;
	  if (a_request->conn->sync == HTTP_TRANS_ASYNC)
	    return ghttp_not_done;
	}
    }
  if (a_request->proc == ghttp_proc_response_hdrs)
    {
      l_rv = http_resp_read_headers(a_request->resp, a_request->conn);
      if (l_rv == HTTP_TRANS_ERR)
	return ghttp_error;
      if (l_rv == HTTP_TRANS_NOT_DONE)
	return ghttp_not_done;
      if (l_rv == HTTP_TRANS_DONE)
	{
                if (a_request->isWs)
                        return ghttp_not_done;
	  a_request->proc = ghttp_proc_response;
	  if (a_request->conn->sync == HTTP_TRANS_ASYNC)
	    return ghttp_not_done;
	}
    }
  if (a_request->proc == ghttp_proc_response)
    {
      l_rv = http_resp_read_body(a_request->resp,
				 a_request->req,
				 a_request->conn);
      if (l_rv == HTTP_TRANS_ERR)
	{
	  /* make sure that the connected flag is fixed and stuff */
	  if (a_request->conn->sock == -1)
	    a_request->connected = 0;
	  return ghttp_error;
	}
      if (l_rv == HTTP_TRANS_NOT_DONE)
	return ghttp_not_done;
      if (l_rv == HTTP_TRANS_DONE)
	{
	/* make sure that the connected flag is fixed and stuff */
	  if (a_request->conn->sock == -1)
	    a_request->connected = 0;
	  a_request->proc = ghttp_proc_none;
	  return ghttp_done;
	}
    }
  return ghttp_error;
}

ghttp_current_status
ghttp_get_status(ghttp_request *a_request)
{
  ghttp_current_status l_return;

  l_return.proc = a_request->proc;
  if (a_request->proc == ghttp_proc_request)
    {
      l_return.bytes_read = a_request->conn->io_buf_io_done;
      l_return.bytes_total = a_request->conn->io_buf_alloc;
    }
  else if (a_request->proc == ghttp_proc_response_hdrs)
    {
      l_return.bytes_read = 0;
      l_return.bytes_total = 0;
    }
  else if (a_request->proc == ghttp_proc_response)
    {
      if (a_request->resp->content_length > 0)
	{
          l_return.bytes_read = a_request->resp->body_len +
            a_request->conn->io_buf_alloc +
            a_request->resp->flushed_length;
	  l_return.bytes_total = a_request->resp->content_length;
	}
      else
	{
	  l_return.bytes_read = a_request->resp->body_len +
	    a_request->conn->io_buf_alloc +
            a_request->resp->flushed_length;
	  l_return.bytes_total = -1;
	}
    }
  else
    {
      l_return.bytes_read = 0;
      l_return.bytes_total = 0;
    }
  return l_return;
}

void
ghttp_flush_response_buffer(ghttp_request *a_request)
{
  http_resp_flush(a_request->resp, a_request->conn);
}

int
ghttp_close(ghttp_request *a_request)
{
  if (!a_request)
    return -1;

  http_trans_conn_close(a_request->conn);

  a_request->connected = 0;
  return 0;
}

void
ghttp_clean(ghttp_request *a_request)
{
  http_resp_destroy(a_request->resp);
  a_request->resp = http_resp_new();
  http_req_destroy(a_request->req);
  a_request->req = http_req_new();
  http_trans_buf_reset(a_request->conn);
  a_request->proc = ghttp_proc_none;
  return;
}

void
ghttp_set_chunksize(ghttp_request *a_request, int a_size)
{
  if (a_request && (a_size > 0))
    a_request->conn->io_buf_chunksize = a_size;
}

void
ghttp_set_header(ghttp_request *a_request,
		 const char *a_hdr, const char *a_val)
{
  http_hdr_set_value(a_request->req->headers,
		     a_hdr, a_val);
}

const char *
ghttp_get_header(ghttp_request *a_request,
		 const char *a_hdr)
{
  return http_hdr_get_value(a_request->resp->headers,
			    a_hdr);
}

int
ghttp_get_header_names(ghttp_request *a_request,
		       char ***a_hdrs, int *a_num_hdrs)
{
  return http_hdr_get_headers(a_request->resp->headers,
			      a_hdrs, a_num_hdrs);
}

const char *
ghttp_get_error(ghttp_request *a_request)
{
  if (a_request->errstr)
      return a_request->errstr;

    switch(a_request->conn->error_type) {
        case http_trans_err_type_errno:
            a_request->errstr = strerror(a_request->conn->error);
            return a_request->errstr;
            if (a_request->conn->error == EAGAIN || a_request->conn->error == EWOULDBLOCK)
#ifdef WITH_OPENSSL
        case http_trans_err_type_ssl:
            a_request->errstr = ERR_reason_error_string(a_request->conn->error);
            return a_request->errstr;

#elif defined (WITH_WOLFSSL)
        case http_trans_err_type_ssl:
            a_request->errstr = wolfSSL_ERR_reason_error_string(a_request->conn->error);
            return a_request->errstr;

#endif
	default:
	    break;
    }

    return "Unknown Error.";
}

time_t
ghttp_parse_date(char *a_date)
{
  if (!a_date)
    return 0;
  return (http_date_to_time(a_date));
}

int
ghttp_status_code(ghttp_request *a_request)
{
  if (!a_request)
    return 0;
  return(a_request->resp->status_code);
}

const char *
ghttp_reason_phrase(ghttp_request *a_request)
{
  if (!a_request)
    return 0;
  return(a_request->resp->reason_phrase);
}

int
ghttp_get_socket(ghttp_request *a_request)
{
  if (!a_request)
    return -1;
  return(a_request->conn->sock);
}

char *
ghttp_get_body(ghttp_request *a_request)
{
  if (!a_request)
    return NULL;
  if (a_request->proc == ghttp_proc_none)
    return (a_request->resp->body);
  if (a_request->proc == ghttp_proc_response)
    {
      if (a_request->resp->content_length > 0)
	{
	  if (a_request->resp->body_len)
	    return a_request->resp->body;
	  else
	    return a_request->conn->io_buf;
	}
      else
	{
	  return a_request->resp->body;
	}
    }
  return NULL;
}

int
ghttp_get_body_len(ghttp_request *a_request)
{
  if (!a_request)
    return 0;
  if (a_request->proc == ghttp_proc_none)
    return (a_request->resp->body_len);
  if (a_request->proc == ghttp_proc_response)
    {
      if (a_request->resp->content_length > 0)
	{
	  if (a_request->resp->body_len)
	    return a_request->resp->body_len;
	  else
	    return a_request->conn->io_buf_alloc;
	}
      else
	{
	  return a_request->resp->body_len;
	}
    }
  return 0;
}

int
ghttp_set_authinfo(ghttp_request *a_request,
		   const char *a_user,
		   const char *a_pass)
{
  char *l_authtoken = NULL;
  char *l_final_auth = NULL;
  char *l_auth64 = NULL;

  /* check our args */
  if (!a_request)
    return -1;
  /* if we have a NULL or zero length string in the username
     or password field, blitz the authinfo */
  if ((!a_user) || (strlen(a_user) < 1) ||
      (!a_pass) || (strlen(a_pass)< 1))
    {
      if (a_request->username)
	{
	  free(a_request->username);
	  a_request->username = NULL;
	}
      if (a_request->password)
	{
	  free(a_request->password);
	  a_request->password = NULL;
	}
      if (a_request->authtoken)
	{
	  free(a_request->authtoken);
	  a_request->authtoken = NULL;
	}
      return 0;
    }
  /* encode the string using base64.  Usernames and passwords
     for basic authentication are encoded like this:
     username:password
     That's it.  Easy, huh?
  */
  /* enough for the trailing \0 and the : */
  l_authtoken = malloc(strlen(a_user) + strlen(a_pass) + 2);
  memset(l_authtoken, 0, (strlen(a_user) + strlen(a_pass) + 2));
  sprintf(l_authtoken, "%s:%s", a_user, a_pass);
  l_auth64 = http_base64_encode(l_authtoken);
  if (!l_auth64)
    {
      free(l_authtoken);
      return -1;
    }
  /* build the final header */
  l_final_auth = malloc(strlen(l_auth64) + strlen(basic_header) + 1);
  memset(l_final_auth, 0, (strlen(l_auth64) + strlen(basic_header) + 1));
  strcat(l_final_auth, basic_header);
  strcat(l_final_auth, l_auth64);
  free(l_auth64);
  free(l_authtoken);
  /* copy the strings into the request */

  if (a_request->username) free(a_request->username);
  if (a_request->password) free(a_request->password);
  if (a_request->authtoken) free(a_request->authtoken);
  a_request->username = strdup(a_user);
  a_request->password = strdup(a_pass);
  a_request->authtoken = l_final_auth;

  return 0;
}


int
ghttp_set_proxy_authinfo(ghttp_request *a_request,
			 const char *a_user,
			 const char *a_pass)
{
  char *l_authtoken = NULL;
  char *l_final_auth = NULL;
  char *l_auth64 = NULL;
  
  /* check our args */
  if (!a_request)
    return -1;
  /* if we have a NULL or zero length string in the username
     or password field, blitz the authinfo */
  if ((!a_user) || (strlen(a_user) < 1) ||
      (!a_pass) || (strlen(a_pass)< 1))
    {
      if (a_request->proxy_username)
	{
	  free(a_request->proxy_username);
	  a_request->proxy_username = NULL;
	}
      if (a_request->proxy_password)
	{
	  free(a_request->proxy_password);
	  a_request->proxy_password = NULL;
	}
      if (a_request->proxy_authtoken)
	{
	  free(a_request->proxy_authtoken);
	  a_request->proxy_authtoken = NULL;
	}
      return 0;
    }
  /* encode the string using base64.  Usernames and passwords
     for basic authentication are encoded like this:
     username:password
     That's it.  Easy, huh?
  */
  /* enough for the trailing \0 and the : */
  l_authtoken = malloc(strlen(a_user) + strlen(a_pass) + 2);
  memset(l_authtoken, 0, (strlen(a_user) + strlen(a_pass) + 2));
  sprintf(l_authtoken, "%s:%s", a_user, a_pass);
  l_auth64 = http_base64_encode(l_authtoken);
  if (!l_auth64)
    {
      free(l_authtoken);
      return -1;
    }
  /* build the final header */
  l_final_auth = malloc(strlen(l_auth64) + strlen(basic_header) + 1);
  memset(l_final_auth, 0, (strlen(l_auth64) + strlen(basic_header) + 1));
  strcat(l_final_auth, basic_header);
  strcat(l_final_auth, l_auth64);
  free(l_auth64);
  free(l_authtoken);
  /* copy the strings into the request */
  if (a_request->proxy_username) free(a_request->proxy_username);
  if (a_request->proxy_password) free(a_request->proxy_password);
  if (a_request->proxy_authtoken) free(a_request->proxy_authtoken);
  a_request->proxy_username = strdup(a_user);
  a_request->proxy_password = strdup(a_pass);
  a_request->proxy_authtoken = l_final_auth;
  
  return 0;
}

void
ghttp_set_ssl_certificate_callback(ghttp_request     *a_request,
                                   ghttp_ssl_cert_cb callback,
                                   void              *user_data) 
{
#if defined(WITH_OPENSSL) || defined(WITH_WOLFSSL)
  a_request->cert_cb      = callback;
  a_request->cert_cb_data = user_data;
#endif
}
