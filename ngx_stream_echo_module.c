
/*
 * Copyright (C) Haitao Lv
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>


typedef struct {
    ngx_chain_t                 *out;
} ngx_stream_echo_ctx_t;


static ngx_int_t ngx_stream_echo_init(ngx_conf_t *cf);
static void ngx_stream_echo_handler(ngx_stream_session_t *s);
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
    ngx_stream_echo_init,                  /* postconfiguration */

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

    c->log->action = "echo text";

    ctx = ngx_pcalloc(c->pool, sizeof(ngx_stream_echo_ctx_t));
    if (ctx == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_stream_set_ctx(s, ctx, ngx_stream_echo_module);

    ctx->out = ngx_alloc_chain_link(c->pool);
    if (ctx->out == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ctx->out->buf = c->buffer;
    ctx->out->next = NULL;

    c->write->handler = ngx_stream_echo_write_handler;

    ngx_stream_echo_write_handler(c->write);
}


static void
ngx_stream_echo_write_handler(ngx_event_t *ev)
{
    ngx_connection_t         *c;
    ngx_stream_session_t     *s;
    ngx_stream_echo_ctx_t  *ctx;

    c = ev->data;
    s = c->data;

    if (ev->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "connection timed out");
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_echo_module);

    if (ngx_stream_top_filter(s, ctx->out, 1) == NGX_ERROR) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ctx->out = NULL;

    if (!c->buffered) {
        ngx_log_debug0(NGX_LOG_DEBUG_STREAM, c->log, 0,
                       "stream return done sending");
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    if (ngx_handle_write_event(ev, 0) != NGX_OK) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_add_timer(ev, 5000);
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


static ngx_int_t
ngx_stream_echo_preread_handler(ngx_stream_session_t *s)
{
    ngx_connection_t *c;
    c = s->connection;
    if (c->buffer == NULL) {
        return NGX_AGAIN;
    }

    return NGX_OK;
}


static ngx_int_t
ngx_stream_echo_init(ngx_conf_t *cf)
{
    ngx_stream_handler_pt        *h;
    ngx_stream_core_main_conf_t  *cmcf;

    cmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_STREAM_PREREAD_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_stream_echo_preread_handler;

    return NGX_OK;
}
