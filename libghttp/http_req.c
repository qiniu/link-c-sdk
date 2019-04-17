/*
 * http_req.c -- Functions for making http requests
 * Created: Christopher Blizzard <blizzard@appliedtheory.com>, 6-Aug-1998
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

#include <string.h>
#include <stdlib.h>
#include "http_req.h"
#include "http_trans.h"
#include "http_global.h"
#include "config.h"
#include "timeoutconn.h"
#include <errno.h>

#ifndef LINK_C_SDK_RELEASE_VERSION
#define LINK_C_SDK_RELEASE_VERSION
#endif
#ifndef LINK_C_SDK_BUILD_VERSION
#define LINK_C_SDK_BUILD_VERSION
#endif
const char *gVersionAgent = "version:" LINK_C_SDK_RELEASE_VERSION " build:" LINK_C_SDK_BUILD_VERSION;


const char *
http_req_type_char[] = {
  "GET",
  "OPTIONS",
  "HEAD",
  "POST",
  "PUT",
  "DELETE",
  "TRACE",
  "CONNECT",
  "PROPFIND",
  "PROPPATCH",
  "MKCOL",
  "COPY",
  "MOVE",
  "LOCK",
  "UNLOCK",
  NULL
};

http_req *
http_req_new(void)
{
  http_req *l_return = NULL;
  
  l_return = (http_req *)malloc(sizeof(http_req));
  memset(l_return, 0, sizeof(http_req));
  /* default to 1.1 */
  l_return->http_ver = 1.1;
  l_return->headers = http_hdr_list_new();
  return l_return;
}

void
http_req_destroy(http_req *a_req)
{
  if (!a_req)
    return;
  if (a_req->headers)
    http_hdr_list_destroy(a_req->headers);
  free(a_req);
}

int
http_req_prepare(http_req *a_req)
{
  int l_return = 0;
  char l_buf[30];

  if (!a_req)
    return -1;
  memset(l_buf, 0, 30);
  /* set the host header */
  http_hdr_set_value(a_req->headers,
		     http_hdr_Host,
		     a_req->host);
  /* check to see if we have an entity body */
  if ((a_req->type == http_req_type_post) ||
      (a_req->type == http_req_type_put) ||
      (a_req->type == http_req_type_trace))
    {
      if (a_req->body_len > 0) {
      sprintf(l_buf, "%d", a_req->body_len+a_req->body1_len+a_req->body2_len);
      http_hdr_set_value(a_req->headers,
			 http_hdr_Content_Length,
			 l_buf);
      }
    }
  /* if the user agent isn't set then set a default */
  if (http_hdr_get_value(a_req->headers, http_hdr_User_Agent) == NULL)
    http_hdr_set_value(a_req->headers, http_hdr_User_Agent,
		       gVersionAgent);
  return l_return;
}

#include <sys/time.h>
static inline int64_t getCurMilliSec() {
        struct  timeval    tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec*(int64_t)(1000)+tv.tv_usec/(int64_t)(1000);
}

static void restoreConn(http_trans_conn *a_conn, http_trans_conn *bak) {
        a_conn->io_buf = bak->io_buf;
        a_conn->io_buf_len = bak->io_buf_len;
        a_conn->io_buf_alloc = bak->io_buf_alloc;
        a_conn->io_buf_io_left = bak->io_buf_io_left;
        a_conn->io_buf_io_done = bak->io_buf_io_done;
}

int
http_req_send(http_req *a_req, http_trans_conn *a_conn)
{
  char       *l_request = NULL;
  int         l_request_len = 0;
  int         i = 0;
  int         l_len = 0;
  int         l_headers_len = 0;
  int         l_rv = 0;
  char       *l_content = NULL;

  /* see if we need to jump into the function somewhere */
  if (a_conn->sync == HTTP_TRANS_ASYNC)
    {
      if (a_req->state == http_req_state_sending_request)
	goto http_req_state_sending_request_jump;
      if (a_req->state == http_req_state_sending_headers)
	goto http_req_state_sending_headers_jump;
      if (a_req->state == http_req_state_sending_body)
	goto http_req_state_sending_body_jump;
    }
  /* enough for the request and the other little headers */
  l_request = malloc(30 + strlen(a_req->resource) + (a_conn->proxy_host ?
						     (strlen(a_req->host) + 20) : 0));
  memset(l_request, 0, 30 + strlen(a_req->resource) + (a_conn->proxy_host ?
						       (strlen(a_req->host) + 20) : 0));
  /* copy it into the buffer */
  if (a_conn->proxy_host)
    {
      l_request_len = sprintf(l_request,
			      "%s %s HTTP/%01.1f\r\n",
			      http_req_type_char[a_req->type],
			      a_req->full_uri,
			      a_req->http_ver);
    }
  else
    {
      l_request_len = sprintf(l_request,
			      "%s %s HTTP/%01.1f\r\n",
			      http_req_type_char[a_req->type],
			      a_req->resource,
			      a_req->http_ver);
    }
  /* set the request in the connection buffer */
  http_trans_append_data_to_buf(a_conn, l_request, l_request_len);
  /* free up the request - we don't need it anymore */
  free(l_request);
  l_request = NULL;
  /* set the state */
  a_req->state = http_req_state_sending_request;
 http_req_state_sending_request_jump:
  /* send the request */
  do {
    l_rv = http_trans_write_buf(a_conn);
    if ((a_conn->sync == HTTP_TRANS_ASYNC) && (l_rv == HTTP_TRANS_NOT_DONE))
      return HTTP_TRANS_NOT_DONE;
    if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0))
      return HTTP_TRANS_ERR;
  } while (l_rv == HTTP_TRANS_NOT_DONE);
  /* reset the buffer */
  http_trans_buf_reset(a_conn);
  /* set up all of the headers */
  for (i = 0; i < HTTP_HDRS_MAX; i++)
    {
      l_len = 0;
      if (a_req->headers->header[i])
	{
	  l_len = strlen(a_req->headers->header[i]);
	  if (l_len > 0)
	    {
	      http_trans_append_data_to_buf(a_conn, a_req->headers->header[i], l_len);
	      l_headers_len += l_len;
	      http_trans_append_data_to_buf(a_conn, ": ", 2);
	      l_headers_len += 2;
	      /* note, it's ok to have no value for a request */
	      if ((l_len = strlen(a_req->headers->value[i])) > 0)
		{
		  http_trans_append_data_to_buf(a_conn, a_req->headers->value[i], l_len);
		  l_headers_len += l_len;
		}
	      http_trans_append_data_to_buf(a_conn, "\r\n", 2);
	      l_headers_len += 2;
	    }
	}
    }
  http_trans_append_data_to_buf(a_conn, "\r\n", 2);
  l_headers_len += 2;
  /* set the state */
  a_req->state = http_req_state_sending_headers;
 http_req_state_sending_headers_jump:
  /* blast that out to the network */
  do {
    l_rv = http_trans_write_buf(a_conn);
    if ((a_conn->sync == HTTP_TRANS_ASYNC) && (l_rv == HTTP_TRANS_NOT_DONE))
      return HTTP_TRANS_NOT_DONE;
    if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0))
      return HTTP_TRANS_ERR;
  } while (l_rv == HTTP_TRANS_NOT_DONE);
  /* reset the buffer */
  http_trans_buf_reset(a_conn);
  l_content = http_hdr_get_value(a_req->headers, http_hdr_Content_Length);
  if (l_content)
    {
      /* append the information to the buffer */
            /*
      http_trans_append_data_to_buf(a_conn, a_req->body, a_req->body_len);
      if (a_req->body1)
        http_trans_append_data_to_buf(a_conn, a_req->body1, a_req->body1_len);
      if (a_req->body2)
        http_trans_append_data_to_buf(a_conn, a_req->body2, a_req->body2_len);
             */
      a_req->state = http_req_state_sending_body;
    http_req_state_sending_body_jump:
            if (a_conn->sync == HTTP_TRANS_ASYNC)
            {
                    
      do {
	l_rv = http_trans_write_buf(a_conn);
	if ((a_conn->sync == HTTP_TRANS_ASYNC) && (l_rv == HTTP_TRANS_NOT_DONE))
	  return HTTP_TRANS_NOT_DONE;
	if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0))
	  return HTTP_TRANS_ERR;
      } while (l_rv == HTTP_TRANS_NOT_DONE);
      /* reset the buffer */
      http_trans_buf_reset(a_conn);
                    
    }else {
            http_trans_conn bakConn = *a_conn;
            
            const char *dataArr[3];
            int dataLens[3];
            dataArr[0] = a_req->body;
            dataArr[1] = a_req->body1;
            dataArr[2] = a_req->body2;
            dataLens[0] = a_req->body_len;
            dataLens[1] = a_req->body1_len;
            dataLens[2] = a_req->body2_len;
            
            int i, remain, offset, l;
            
            remain=0;
            for (i = 0; i < 3; i++) {
                    if (dataArr[i] && dataLens[i] > 0) {
                            remain += dataLens[i];
                    }
            }
            a_conn->nRemainMilliTime += (remain/(256*1024))*5300;
            
            for (i = 0; i < 3; i++) {
                    remain = dataLens[i];
                    offset = 0;
                    
                    while (dataArr[i] && remain > 0) {
                            l = remain > 256 * 1024 ? 256 * 1024 : remain;
                            a_conn->io_buf = (char *)dataArr[i]+offset;
                            a_conn->io_buf_len = l;
                            a_conn->io_buf_alloc = l;
                            a_conn->io_buf_io_left = l;
                            a_conn->io_buf_io_done = 0;
                            do {
                                    l_rv = http_trans_write_buf(a_conn);
                                    if (l_rv == HTTP_TRANS_ERR) {
                                            restoreConn(a_conn, &bakConn);
                                            return HTTP_TRANS_ERR;
                                    } else {
                                            if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0)) {
                                                    restoreConn(a_conn, &bakConn);
                                                    return HTTP_TRANS_ERR;
                                            }
                                            if (a_conn->error == EAGAIN || a_conn->error == EWOULDBLOCK) {
                                                    restoreConn(a_conn, &bakConn);
                                                    return HTTP_TRANS_ERR;
                                            }
                                    }
                            } while (l_rv == HTTP_TRANS_NOT_DONE);
                            remain -= l;
                            offset += l;
                    }
            }
            restoreConn(a_conn, &bakConn);
      }
    }
  return HTTP_TRANS_DONE;
}

int
http_req_send_websocket(http_req *a_req, http_trans_conn *a_conn, char *data,int len) {
        
        http_trans_conn bakConn = *a_conn;
        
        int remain, offset, l, l_rv;
        remain = len;
        offset = 0;
        a_conn->nRemainMilliTime = a_conn->nTimeoutInSecond * 1000;
        while (data && remain > 0) {
                l = remain;
                a_conn->io_buf = (char *)data+offset;
                a_conn->io_buf_len = l;
                a_conn->io_buf_alloc = l;
                a_conn->io_buf_io_left = l;
                a_conn->io_buf_io_done = 0;
                do {
                        l_rv = http_trans_write_buf(a_conn);
                        if (l_rv == HTTP_TRANS_ERR) {
                                restoreConn(a_conn, &bakConn);
                                return HTTP_TRANS_ERR;
                        } else {
                                if ((l_rv == HTTP_TRANS_DONE) && (a_conn->last_read == 0)) {
                                        restoreConn(a_conn, &bakConn);
                                        return HTTP_TRANS_ERR;
                                }
                                if (a_conn->error == EAGAIN || a_conn->error == EWOULDBLOCK) {
                                        restoreConn(a_conn, &bakConn);
                                        return HTTP_TRANS_ERR;
                                }
                        }
                } while (l_rv == HTTP_TRANS_NOT_DONE);
                remain -= l;
                offset += l;
        }
        
        restoreConn(a_conn, &bakConn);
        a_conn->nRemainMilliTime = a_conn->nTimeoutInSecond * 1000;
        return 0;
}

static void set_rcv_timeout(int fd, int tm) {
        struct timeval tv;
        if (tm <= 0) {
                tv.tv_sec = 0;
                tv.tv_usec = 2000;
        } else {
                tv.tv_sec = tm/1000;
                tv.tv_usec = (tm%1000)*1000;
        }
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

static inline int64_t reqCurMilliSec() {
        struct  timeval    tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec*(int64_t)(1000)+tv.tv_usec/(int64_t)(1000);
}

int http_req_read_websocket(http_req *a_req, http_trans_conn *a_conn, char * msg, int len) {
        
        unsigned char mask[4];
        int i = 0;
        a_conn->nRemainMilliTime = 1000;
        set_rcv_timeout(a_conn->sock, a_conn->nRemainMilliTime);
        
        int64_t startT = reqCurMilliSec();
        a_conn->io_buf_io_left = 2;
        a_conn->io_buf_alloc = 0;
        int ret = http_trans_read_into_buf(a_conn);
        if (ret != HTTP_TRANS_DONE)
                return -1;
        int64_t endT = reqCurMilliSec();
        
        int isMask = a_conn->io_buf[1] & 0x80;
        if (isMask) {
                isMask = 4;
        }
        
        int lenLen = 0;
        int payloadLen = a_conn->io_buf[1] & 0x7F;
        if (payloadLen == 126) {
                payloadLen = 0;
                lenLen = 2;
        } else if (payloadLen == 127) {
                payloadLen = 0;
                lenLen = 8;
        }
        
        a_conn->nRemainMilliTime -= (endT - startT);
        if (a_conn->nRemainMilliTime <= 0)
                a_conn->nRemainMilliTime = 10;
        set_rcv_timeout(a_conn->sock, a_conn->nRemainMilliTime);
        
        if (lenLen + isMask > 0) {
                startT = reqCurMilliSec();
                
                a_conn->io_buf_io_left = lenLen + isMask;
                a_conn->io_buf_alloc = 0;
                
                ret = http_trans_read_into_buf(a_conn);
                if (ret != HTTP_TRANS_DONE)
                        return -2;
                endT = reqCurMilliSec();
                if (isMask)
                        memcpy(mask, a_conn->io_buf+lenLen, isMask);
                
                for(i = 0; i < lenLen; i++) {
                        payloadLen = payloadLen << 8;
                        payloadLen += (unsigned char)a_conn->io_buf[i];
                }
                
                a_conn->nRemainMilliTime -= (endT - startT);
                if (a_conn->nRemainMilliTime <= 0)
                        a_conn->nRemainMilliTime = 10;
                set_rcv_timeout(a_conn->sock, a_conn->nRemainMilliTime);
        }
        
        a_conn->io_buf_io_left = (int)payloadLen;
        a_conn->io_buf_alloc = 0;
        ret = http_trans_read_into_buf(a_conn);
        if (ret != HTTP_TRANS_DONE)
                return -3;
        
        int t = len - 1;
        if ((int)payloadLen < t)
                t = (int)payloadLen;
        if (isMask) {
                for (i = 0; i < t; i++) {
                        msg[i] = (unsigned char) (a_conn->io_buf[i] ^ mask[i % 4]);
                }
        } else {
                memcpy(msg, a_conn->io_buf, t);
        }
        msg[len - 1] = 0;
        return 0;
}
