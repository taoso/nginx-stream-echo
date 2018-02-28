
/*
 * Copyright (C) Haitao Lv
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


typedef struct {
    ngx_chain_t                 *buf_chain;
} ngx_stream_echo_ctx_t;


static void ngx_stream_echo_handler(ngx_stream_session_t *s);
static void ngx_stream_echo_read_handler(ngx_event_t *ev);
static void ngx_stream_echo_write_handler(ngx_event_t *ev);

static char *ngx_stream_echo(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t  ngx_stream_echo_commands[] = {

    { ngx_string("echo"),
      NGX_STREAM_SRV_CONF|NGX_CONF_NOARGS,
      ngx_stream_echo,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_stream_module_t  ngx_stream_echo_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL                                   /* merge server configuration */
};


ngx_module_t  ngx_stream_echo_module = {
    NGX_MODULE_V1,
    &ngx_stream_echo_module_ctx,           /* module context */
    ngx_stream_echo_commands,              /* module directives */
    NGX_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void
ngx_stream_echo_handler(ngx_stream_session_t *s)
{
    ngx_connection_t              *c;
    ngx_stream_echo_ctx_t       *ctx;

    c = s->connection;

    c->log->action = "echo";

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_echo_module);

    if (ctx == NULL) {

        ctx = ngx_pcalloc(c->pool, sizeof(ngx_stream_echo_ctx_t));

        if (ctx == NULL) {
            ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
            return;
        }

        ngx_stream_set_ctx(s, ctx, ngx_stream_echo_module);
    }

    c->read->handler = ngx_stream_echo_read_handler;
    c->write->handler = ngx_stream_echo_write_handler;

    ngx_stream_echo_read_handler(c->read);

    ngx_add_timer(c->read, 5000);
}

static void
ngx_stream_echo_read_handler(ngx_event_t *ev)
{
    ngx_chain_t           *buf_chain;
    ngx_chain_t                 **lb;
    ngx_buf_t                   *buf;
    ngx_int_t                     rc;
    ngx_connection_t              *c;
    ngx_stream_session_t          *s;
    ngx_stream_echo_ctx_t       *ctx;
    ssize_t                     size;
    ssize_t                        n;

    rc = NGX_AGAIN;

    c = ev->data;
    s = c->data;

    if (ev->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "connection timed out");
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_echo_module);

    lb = &ctx->buf_chain;
    while (rc == NGX_AGAIN) {

        if (c->read->eof) {
            rc = NGX_STREAM_OK;
            break;
        }

        if (!c->read->ready) {
            if (ngx_handle_read_event(c->read, 0) != NGX_OK) {
                rc = NGX_ERROR;
            }

            break;
        }

        buf = ngx_create_temp_buf(c->pool, 1);

        if (buf == NULL) {
            rc = NGX_ERROR;
            break;
        }

        size = buf->end - buf->last;

        n = c->recv(c, buf->last, size);

        if (n == NGX_ERROR) {
            rc = NGX_STREAM_OK;
            break;
        }

        if (n > 0) {
            buf->last += n;

            buf_chain = ngx_alloc_chain_link(c->pool);
            if (buf_chain == NULL) {
                ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
                return;
            }

            buf_chain->buf = buf;
            buf_chain->next = NULL;

            if (*lb == NULL) {
                *lb = buf_chain;
            } else {
                (*lb)->next = buf_chain;
            }

            lb = &(*lb)->next;
        }
    }

    ngx_add_timer(ev, 5000);

    ngx_stream_echo_write_handler(c->write);

    ctx->buf_chain = NULL;
}


static void
ngx_stream_echo_write_handler(ngx_event_t *ev)
{
    ngx_connection_t         *c;
    ngx_stream_session_t     *s;
    ngx_stream_echo_ctx_t  *ctx;

    c = ev->data;
    s = c->data;

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_echo_module);

    if (ctx->buf_chain == NULL) {
        return;
    }

    if (ngx_stream_top_filter(s, ctx->buf_chain, 1) == NGX_ERROR) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    if (ngx_handle_write_event(ev, 0) != NGX_OK) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }
}


static char *
ngx_stream_echo(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_core_srv_conf_t          *cscf;
    cscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_core_module);

    if (cscf->handler != NULL) {
        return "is duplicate";
    }

    cscf->handler = ngx_stream_echo_handler;

    return NGX_CONF_OK;
}
