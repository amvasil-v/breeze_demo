//
// Created by HISONA on 2016. 2. 29..
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "https.h"
#include "ca_cert.h"

/*---------------------------------------------------------------------*/
static int _error;

/*---------------------------------------------------------------------*/
char *strtoken(char *src, char *dst, int size);

static int parse_url(char *src_url, int *https, char *host, char *port, char *url);
static int http_header(HTTP_INFO *hi, char *param);
static int http_parse(HTTP_INFO *hi);

static int https_init(HTTP_INFO *hi, BOOL https, BOOL verify, netio_t *io);
static int https_close(HTTP_INFO *hi);
static int https_connect(HTTP_INFO *hi, char *host, char *port);
static int https_write(HTTP_INFO *hi, char *buffer, int len);
static int https_read(HTTP_INFO *hi, char *buffer, int len);

/*---------------------------------------------------------------------------*/
char *strtoken(char *src, char *dst, int size)
{
    char *p, *st, *ed;
    int  len = 0;

    // l-trim
    p = src;

    while(TRUE)
    {
        if((*p == '\n') || (*p == 0)) return NULL; /* value is not exists */
        if((*p != ' ') && (*p != '\t')) break;
        p++;
    }

    st = p;
    while(TRUE)
    {
        ed = p;
        if(*p == ' ') {
            p++;
            break;
        }
        if((*p == '\n') || (*p == 0)) break;
        p++;
    }

    // r-trim
    while(TRUE)
    {
        ed--;
        if(st == ed) break;
        if((*ed != ' ') && (*ed != '\t')) break;
    }

    len = (int)(ed - st + 1);
    if((size > 0) && (len >= size)) len = size - 1;

    strncpy(dst, st, len);
    dst[len]=0;

    return p;
}

/*---------------------------------------------------------------------*/
static int parse_url(char *src_url, int *https, char *host, char *port, char *url)
{
    char *p1, *p2;
    char str[1024];

    memset(str, 0, 1024);

    if(strncmp(src_url, "http://", 7)==0) {
        p1=&src_url[7];
        *https = 0;
    } else if(strncmp(src_url, "https://", 8)==0) {
        p1=&src_url[8];
        *https = 1;
    } else {
        p1 = &src_url[0];
        *https = 0;
    }

    if((p2=strstr(p1, "/")) == NULL)
    {
        sprintf(str, "%s", p1);
        sprintf(url, "/");
    }
    else
    {
        strncpy(str, p1, p2-p1);
        snprintf(url, 256, "%s", p2);
    }

    if((p1=strstr(str, ":")) != NULL)
    {
        *p1=0;
        snprintf(host, 256, "%s", str);
        snprintf(port, 5, "%s", p1+1);
    }
    else
    {
        snprintf(host, 256, "%s", str);

        if(*https == 0)
            snprintf(port, 5, "80");
        else
            snprintf(port, 5, "443");
    }

    return 0;

}

/*---------------------------------------------------------------------*/
static int http_header(HTTP_INFO *hi, char *param)
{
    char *token;
    char t1[256], t2[256];
    int  len;

    token = param;

    if((token=strtoken(token, t1, 256)) == 0) return -1;
    if((token=strtoken(token, t2, 256)) == 0) return -1;

    if(strncasecmp(t1, "HTTP", 4) == 0)
    {
        hi->response.status = atoi(t2);
    }
    else if(strncasecmp(t1, "set-cookie:", 11) == 0)
    {
        snprintf(hi->response.cookie, 512, "%s", t2);
    }
    else if(strncasecmp(t1, "location:", 9) == 0)
    {
        len = (int)strlen(t2);
        strncpy(hi->response.location, t2, len);
        hi->response.location[len] = 0;
    }
    else if(strncasecmp(t1, "content-length:", 15) == 0)
    {
        hi->response.content_length = atoi(t2);
    }
    else if(strncasecmp(t1, "transfer-encoding:", 18) == 0)
    {
        if(strncasecmp(t2, "chunked", 7) == 0)
        {
            hi->response.chunked = TRUE;
        }
    }
    else if(strncasecmp(t1, "connection:", 11) == 0)
    {
        if(strncasecmp(t2, "close", 5) == 0)
        {
            hi->response.close = TRUE;
        }
    }
    else if(strncasecmp(t1, "content-range:", 14) == 0)
    {
        if(strncasecmp(t2, "bytes", 5) == 0)
        {
            if ((token = strtoken(token, t2, 256)) == 0) {
                printf("Failed to parse content-range\n");
            }
            else {
                int range_start = 0;
                int range_end = 0;
                int content_len = 0;

                int res = sscanf(t2, "%d-%d/%d", &range_start, &range_end, &content_len);
                if (res == 3 && hi->range_start == range_start && hi->range_end == range_end) {
                    hi->content_length = content_len;
                } else {
                    hi->content_length = 0;
                }
            }
        }
    }

    return 1;
}

/*---------------------------------------------------------------------*/
static int http_parse(HTTP_INFO *hi)
{
    char    *p1, *p2;
    long    len;


    if(hi->r_len <= 0) return -1;

    p1 = hi->r_buf;

    while(1)
    {
        if(hi->header_end == FALSE)     // header parser
        {
            if((p2 = strstr(p1, "\r\n")) != NULL)
            {
                len = (long)(p2 - p1);
                *p2 = 0;

                if(len > 0)
                {
                    // printf("header: %s(%ld)\n", p1, len);

                    http_header(hi, p1);
                    p1 = p2 + 2;    // skip CR+LF
                }
                else
                {
                    hi->header_end = TRUE; // reach the header-end.

                    // printf("header_end .... \n");

                    p1 = p2 + 2;    // skip CR+LF

                    if(hi->response.chunked == TRUE)
                    {
                        len = hi->r_len - (p1 - hi->r_buf);
                        if(len > 0)
                        {
                            if((p2 = strstr(p1, "\r\n")) != NULL)
                            {
                                *p2 = 0;

                                if((hi->length = strtol(p1, NULL, 16)) == 0)
                                {
                                    hi->response.chunked = FALSE;
                                }
                                else
                                {
                                    hi->response.content_length += hi->length;
                                }
                                p1 = p2 + 2;    // skip CR+LF
                            }
                            else
                            {
                                // copy the data as chunked size ...
                                strncpy(hi->r_buf, p1, len);
                                hi->r_buf[len] = 0;
                                hi->r_len = len;
                                hi->length = -1;

                                break;
                            }
                        }
                        else
                        {
                            hi->r_len = 0;
                            hi->length = -1;

                            break;
                        }
                    }
                    else
                    {
                        hi->length = hi->response.content_length;
                    }
                }

            }
            else
            {
                len = hi->r_len - (p1 - hi->r_buf);
                if(len  > 0)
                {
                    // keep the partial header data ...
                    strncpy(hi->r_buf, p1, len);
                    hi->r_buf[len] = 0;
                    hi->r_len = len;
                }
                else
                {
                    hi->r_len = 0;
                }

                break;
            }
        }
        else    // body parser ...
        {
            if(hi->response.chunked == TRUE && hi->length == -1)
            {
                len = hi->r_len - (p1 - hi->r_buf);
                if(len > 0)
                {
                    if ((p2 = strstr(p1, "\r\n")) != NULL)
                    {
                        *p2 = 0;

                        if((hi->length = strtol(p1, NULL, 16)) == 0)
                        {
                            hi->response.chunked = FALSE;
                        }
                        else
                        {
                            hi->response.content_length += hi->length;
                        }

                        p1 = p2 + 2;    // skip CR+LF
                    }
                    else
                    {
                        // copy the remain data as chunked size ...
                        strncpy(hi->r_buf, p1, len);
                        hi->r_buf[len] = 0;
                        hi->r_len = len;
                        hi->length = -1;

                        break;
                    }
                }
                else
                {
                    hi->r_len = 0;

                    break;
                }
            }
            else
            {
                if(hi->length > 0)
                {
                    len = hi->r_len - (p1 - hi->r_buf);

                    if(len > hi->length)
                    {
                        // copy the data for response ..
                        if(hi->body_len < hi->body_size-1)
                        {
                            if (hi->body_size > (hi->body_len + hi->length))
                            {
                                memcpy(&(hi->body[hi->body_len]), p1, hi->length);
                                hi->body_len += hi->length;
                                hi->body[hi->body_len] = 0;
                            }
                            else
                            {
                                memcpy(&(hi->body[hi->body_len]), p1, hi->body_size - hi->body_len - 1);
                                hi->body_len = hi->body_size - 1;
                                hi->body[hi->body_len] = 0;
                            }
                        }

                        p1 += hi->length;
                        len -= hi->length;

                        if(hi->response.chunked == TRUE && len >= 2)
                        {
                            p1 += 2;    // skip CR+LF
                            hi->length = -1;
                        }
                        else
                        {
                            return -1;
                        }
                    }
                    else
                    {
                        // copy the data for response ..
                        if(hi->body_len < hi->body_size-1)
                        {
                            if (hi->body_size > (hi->body_len + len))
                            {
                                memcpy(&(hi->body[hi->body_len]), p1, len);
                                hi->body_len += len;
                                hi->body[hi->body_len] = 0;
                            }
                            else
                            {
                                memcpy(&(hi->body[hi->body_len]), p1, hi->body_size - hi->body_len - 1);
                                hi->body_len = hi->body_size - 1;
                                hi->body[hi->body_len] = 0;
                            }
                        }

                        hi->length -= len;
                        hi->r_len = 0;

                        if(hi->response.chunked == FALSE && hi->length <= 0) return 1;

                        break;
                    }
                }
                else
                {
                    if(hi->response.chunked == FALSE) return 1;

                    // chunked size check ..
                    if((hi->r_len > 2) && (memcmp(p1, "\r\n", 2) == 0))
                    {
                        p1 += 2;
                        hi->length = -1;
                    }
                    else
                    {
                        hi->length = -1;
                        hi->r_len = 0;
                    }
                }
            }
        }
    }

    return 0;
}

/*---------------------------------------------------------------------*/
static int https_init(HTTP_INFO *hi, BOOL https, BOOL verify, netio_t *io)
{
    ptrdiff_t zero_len = (void *)&hi->ranged - (void *)hi;
    memset(hi, 0, zero_len);

    if(https == TRUE)
    {
        mbedtls_ssl_init( &hi->tls.ssl );
        mbedtls_ssl_config_init( &hi->tls.conf );
        mbedtls_x509_crt_init( &hi->tls.cacert );
        mbedtls_ctr_drbg_init( &hi->tls.ctr_drbg );
    }

    hi->tls.io = io;

    mbedtls_ssl_set_bio(&hi->tls.ssl, io, io->send, NULL, io->recv_timeout);

    hi->tls.verify = verify;
    hi->url.https = https;

//  printf("https_init ... \n");

    return 0;
}

/*---------------------------------------------------------------------*/
static int https_close(HTTP_INFO *hi)
{
    if(hi->url.https == 1)
    {
        mbedtls_ssl_close_notify(&hi->tls.ssl);
    }

    if(hi->url.https == 1)
    {
        mbedtls_x509_crt_free(&hi->tls.cacert);
        mbedtls_ssl_free(&hi->tls.ssl);
        mbedtls_ssl_config_free(&hi->tls.conf);
        mbedtls_ctr_drbg_free(&hi->tls.ctr_drbg);
        mbedtls_entropy_free(&hi->tls.entropy);
    }

//  printf("https_close ... \n");

    return 0;
}

/*---------------------------------------------------------------------*/
static int https_connect(HTTP_INFO *hi, char *host, char *port)
{
    int ret, https;


    https = hi->url.https;

    if(https == 1)
    {
        mbedtls_entropy_init( &hi->tls.entropy );

        ret = mbedtls_ctr_drbg_seed( &hi->tls.ctr_drbg, mbedtls_entropy_func, &hi->tls.entropy, NULL, 0);
        if( ret != 0 )
        {
            return ret;
        }

        ret = mbedtls_x509_crt_parse(&hi->tls.cacert, (uint8_t *)ca_cert_globalsign, strlen(ca_cert_globalsign) + 1);
        if( ret != 0 )
        {
            return ret;
        }

        ret = mbedtls_ssl_config_defaults( &hi->tls.conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT );
        if( ret != 0 )
        {
            return ret;
        }

        /* OPTIONAL is not optimal for security,
         * but makes interop easier in this simplified example */
        mbedtls_ssl_conf_authmode( &hi->tls.conf, MBEDTLS_SSL_VERIFY_REQUIRED );
        mbedtls_ssl_conf_ca_chain( &hi->tls.conf, &hi->tls.cacert, NULL );
        mbedtls_ssl_conf_rng( &hi->tls.conf, mbedtls_ctr_drbg_random, &hi->tls.ctr_drbg );
        mbedtls_ssl_conf_read_timeout( &hi->tls.conf, 5000 );

        ret = mbedtls_ssl_setup( &hi->tls.ssl, &hi->tls.conf );
        if( ret != 0 )
        {
            return ret;
        }

        ret = mbedtls_ssl_set_hostname( &hi->tls.ssl, host );
        if( ret != 0 )
        {
            return ret;
        }
    }

    ret = hi->tls.io->connect(hi->tls.io, host, port);
    if( ret != 0 )
    {
        return ret;
    }

    if(https == 1)
    {
        //mbedtls_ssl_set_bio(&hi->tls.ssl, &hi->tls.ssl_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

        while ((ret = mbedtls_ssl_handshake(&hi->tls.ssl)) != 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                return ret;
            }
        }

        /* In real life, we probably want to bail out when ret != 0 */
        if( hi->tls.verify && (mbedtls_ssl_get_verify_result(&hi->tls.ssl) != 0) )
        {
            return -1;
        }
    }

    return 0;
}

/*---------------------------------------------------------------------*/
static int https_write(HTTP_INFO *hi, char *buffer, int len)
{
    int ret, slen = 0;

    while(1)
    {
        if(hi->url.https == 1)
            ret = mbedtls_ssl_write(&hi->tls.ssl, (u_char *)&buffer[slen], (size_t)(len-slen));
        else
            ret = hi->tls.io->send(hi->tls.io, (u_char *)&buffer[slen], (size_t)(len-slen));

        if(ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        else if(ret <= 0) return ret;

        slen += ret;

        if(slen >= len) break;
    }

    return slen;
}

/*---------------------------------------------------------------------*/
static int https_read(HTTP_INFO *hi, char *buffer, int len)
{
    if(hi->url.https == 1)
    {
        return mbedtls_ssl_read(&hi->tls.ssl, (u_char *)buffer, (size_t)len);
    }
    else
    {
        return hi->tls.io->recv_timeout(hi->tls.io, (u_char *)buffer, (size_t)len, 5000);
    }
}

/*---------------------------------------------------------------------*/
int http_init(HTTP_INFO *hi, BOOL verify, netio_t *io)
{
    return https_init(hi, 0, verify, io);
}

/*---------------------------------------------------------------------*/
int http_close(HTTP_INFO *hi)
{
    return https_close(hi);
}

/*---------------------------------------------------------------------*/
static int http_get_ranged_impl(HTTP_INFO *hi, char *url, char *response, netio_t *io)
{
    char        request[1024], err[100];
    char        host[256], port[10], dir[1024];
    int         https, verify;
    int         ret, len;


    if(NULL == hi) return -1;

    verify = hi->tls.verify;

    parse_url(url, &https, host, port, dir);

    if( !hi->tls.io->opened(hi->tls.io) || (hi->url.https != https) ||
        (strcmp(hi->url.host, host) != 0) || (strcmp(hi->url.port, port) != 0) )
    {
        https_close(hi);

        https_init(hi, https, verify, io);

        if((ret=https_connect(hi, host, port)) < 0)
        {
            https_close(hi);

            mbedtls_strerror(ret, err, 100);
            snprintf(response, 256, "socket error: %s(%d)", err, ret);
            return -1;
        }
    }
    else
    {
        if(!hi->tls.io->connected(hi->tls.io))
        {
            https_close(hi);

            https_init(hi, https, verify, io);

            if((ret=https_connect(hi, host, port)) < 0)
            {
                https_close(hi);

                mbedtls_strerror(ret, err, 100);
                snprintf(response, 256, "socket error: %s(%d)", err, ret);
                return -1;
            }
        }
    }

    char range_buf[256];
    if (hi->ranged) {
        snprintf(range_buf, 256, "Range: bytes=%u-%u\r\n",
            hi->range_start, hi->range_end);
    }    

    /* Send HTTP request. */
    len = snprintf(request, 1024,
            "GET %s HTTP/1.1\r\n"
            "User-Agent: Mozilla/4.0\r\n"
            "Host: %s:%s\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "%s"
            "Connection: Keep-Alive\r\n"
            "%s\r\n",
            dir, host, port,
            (hi->ranged ? range_buf : ""),
            hi->request.cookie);

    if((ret = https_write(hi, request, len)) != len)
    {
        https_close(hi);

        mbedtls_strerror(ret, err, 100);

        snprintf(response, 256, "socket error: %s(%d)", err, ret);

        return -1;
    }

    //printf("request: %s \r\n\r\n", request);

    hi->response.status = 0;
    hi->response.content_length = 0;
    hi->response.close = 0;

    hi->r_len = 0;
    hi->header_end = 0;

    hi->body = response;
    hi->body_size = hi->range_end - hi->range_start + 2;
    hi->body_len = 0;

    while(1)
    {
        ret = https_read(hi, &hi->r_buf[hi->r_len], (int)(H_READ_SIZE - hi->r_len));
        if(ret == MBEDTLS_ERR_SSL_WANT_READ) continue;
        else if(ret < 0)
        {
            https_close(hi);

            mbedtls_strerror(ret, err, 100);

            snprintf(response, 256, "socket error: %s(%d)", err, ret);

            return -1;
        }
        else if(ret == 0)
        {
            https_close(hi);
            break;
        }

        hi->r_len += ret;
        hi->r_buf[hi->r_len] = 0;

        // printf("read(%ld): |%s| \n", hi->r_len, hi->r_buf);
        // printf("read(%ld) ... \n", hi->r_len);

        if(http_parse(hi) != 0) break;
    }

    if(hi->response.close == 1)
    {
        https_close(hi);
    }
    else
    {
        strncpy(hi->url.host, host, strlen(host));
        strncpy(hi->url.port, port, strlen(port));
        strncpy(hi->url.path, dir, strlen(dir));
    }

    /*
    printf("status: %d \n", hi->response.status);
    printf("cookie: %s \n", hi->response.cookie);
    printf("location: %s \n", hi->response.location);
    printf("referrer: %s \n", hi->response.referrer);
    printf("length: %ld \n", hi->response.content_length);
    printf("body: %ld \n", hi->body_len);
    */

    return hi->response.status;

}

int http_get(HTTP_INFO *hi, char *url, char *response, int size, netio_t *io)
{
    hi->ranged = 0;
    hi->range_start = 0;
    hi->range_end = size - 1;
    return http_get_ranged_impl(hi, url, response, io);
}

int http_get_ranged(HTTP_INFO *hi, char *url, char *response, 
                    size_t range_start, size_t range_end, size_t *content_len,
                    netio_t *io)
{
    hi->ranged = 1;
    hi->range_start = range_start;
    hi->range_end = range_end;
    int ret = http_get_ranged_impl(hi, url, response, io);
    *content_len = hi->content_length;
    return ret;
}

/*---------------------------------------------------------------------*/
void http_strerror(char *buf, int len)
{
    mbedtls_strerror(_error, buf, len);
}

/*---------------------------------------------------------------------*/
int http_open(HTTP_INFO *hi, char *url, netio_t *io)
{
    char        host[256], port[10], dir[1024];
    int         https, verify;
    int         ret;


    if (NULL == hi) return -1;

    verify = hi->tls.verify;

    parse_url(url, &https, host, port, dir);

    if (!hi->tls.io->opened(hi->tls.io) || (hi->url.https != https) ||
        (strcmp(hi->url.host, host) != 0) || (strcmp(hi->url.port, port) != 0))
    {
        if (hi->tls.io->opened(hi->tls.io))
            https_close(hi);

        https_init(hi, https, verify, io);

        if ((ret = https_connect(hi, host, port)) < 0)
        {
            https_close(hi);

            _error = ret;

            return -1;
        }
    }
    else
    {
        if (!hi->tls.io->connected(hi->tls.io))
        {
            https_close(hi);

            https_init(hi, https, verify, io);

            if ((ret = https_connect(hi, host, port)) < 0)
            {
                https_close(hi);

                _error = ret;

                return -1;
            }
        }
//      else
//          printf("socket reuse: %d \n", sock_fd);
    }

    strncpy(hi->url.host, host, strlen(host));
    strncpy(hi->url.port, port, strlen(port));
    strncpy(hi->url.path, dir, strlen(dir));

    return 0;
}

/*---------------------------------------------------------------------*/
int http_write_header(HTTP_INFO *hi)
{
    char        request[4096];
    int         ret, len;


    if (NULL == hi) return -1;

    /* Send HTTP request. */
    len = snprintf(request, 1024,
                   "%s %s HTTP/1.1\r\n"
                   "User-Agent: Mozilla/4.0\r\n"
                   "Host: %s:%s\r\n"
                   "Content-Type: %s\r\n",
                   hi->request.method, hi->url.path,
                   hi->url.host, hi->url.port,
                   hi->request.content_type);


    if(hi->request.referrer[0] != 0)
    {
        len += snprintf(&request[len], H_FIELD_SIZE,
                        "Referer: %s\r\n", hi->request.referrer);
    }

    if(hi->request.chunked == TRUE)
    {
        len += snprintf(&request[len], H_FIELD_SIZE,
                        "Transfer-Encoding: chunked\r\n");
    }
    else
    {
        len += snprintf(&request[len], H_FIELD_SIZE,
                        "Content-Length: %ld\r\n", hi->request.content_length);
    }

    if(hi->request.close == TRUE)
    {
        len += snprintf(&request[len], H_FIELD_SIZE,
                        "Connection: close\r\n");
    }
    else
    {
        len += snprintf(&request[len], H_FIELD_SIZE,
                        "Connection: Keep-Alive\r\n");
    }

    if(hi->request.cookie[0] != 0)
    {
        len += snprintf(&request[len], H_FIELD_SIZE,
                        "Cookie: %s\r\n", hi->request.cookie);
    }

    len += snprintf(&request[len], H_FIELD_SIZE, "\r\n");


    printf("%s", request);

    if ((ret = https_write(hi, request, len)) != len)
    {
        https_close(hi);

        _error = ret;

        return -1;
    }

    return 0;
}

/*---------------------------------------------------------------------*/
int http_write(HTTP_INFO *hi, char *data, int len)
{
    char        str[10];
    int         ret, l;


    if(NULL == hi || len <= 0) return -1;

    if(hi->request.chunked == TRUE)
    {
        l = snprintf(str, 10, "%x\r\n", len);

        if ((ret = https_write(hi, str, l)) != l)
        {
            https_close(hi);
            _error = ret;

            return -1;
        }
    }

    if((ret = https_write(hi, data, len)) != len)
    {
        https_close(hi);
        _error = ret;

        return -1;
    }

    if(hi->request.chunked == TRUE)
    {
        if ((ret = https_write(hi, "\r\n", 2)) != 2)
        {
            https_close(hi);
            _error = ret;

            return -1;
        }
    }

    return len;
}

/*---------------------------------------------------------------------*/
int http_write_end(HTTP_INFO *hi)
{
    char        str[10];
    int         ret, len;

    if (NULL == hi) return -1;

    if(hi->request.chunked == TRUE)
    {
        len = snprintf(str, 10, "0\r\n\r\n");
    }
    else
    {
        len = snprintf(str, 10, "\r\n\r\n");
    }

    if ((ret = https_write(hi, str, len)) != len)
    {
        https_close(hi);
        _error = ret;

        return -1;
    }

    return len;
}

/*---------------------------------------------------------------------*/
int http_read_chunked(HTTP_INFO *hi, char *response, int size)
{
    int ret;


    if (NULL == hi) return -1;

//  printf("request: %s \r\n\r\n", request);

    hi->response.status = 0;
    hi->response.content_length = 0;
    hi->response.close = 0;

    hi->r_len = 0;
    hi->header_end = 0;

    hi->body = response;
    hi->body_size = size;
    hi->body_len = 0;

    hi->body[0] = 0;

    while(1)
    {
        ret = https_read(hi, &hi->r_buf[hi->r_len], (int)(H_READ_SIZE - hi->r_len));
        if(ret == MBEDTLS_ERR_SSL_WANT_READ) continue;
        else if(ret < 0)
        {
            https_close(hi);
            _error = ret;

            return -1;
        }
        else if(ret == 0)
        {
            https_close(hi);
            break;
        }

        hi->r_len += ret;
        hi->r_buf[hi->r_len] = 0;

//        printf("read(%ld): %s \n", hi->r_len, hi->r_buf);
//        printf("read(%ld) \n", hi->r_len);

        if(http_parse(hi) != 0) break;
    }

    if(hi->response.close == 1)
    {
        https_close(hi);
    }

/*
    printf("status: %d \n", hi->status);
    printf("cookie: %s \n", hi->cookie);
    printf("location: %s \n", hi->location);
    printf("referrer: %s \n", hi->referrer);
    printf("length: %d \n", hi->content_length);
    printf("body: %d \n", hi->body_len);
*/

    return hi->response.status;
}

