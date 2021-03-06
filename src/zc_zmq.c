#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zmq.h>
#include "buffer.h"
#include "zc_zmq.h"

#define DEFAULT_PROGRAM_NAME "zc"

#define SOCKET_TYPE_PUSH  "PUSH"
#define SOCKET_TYPE_PULL  "PULL"
#define SOCKET_TYPE_PUB   "PUB"
#define SOCKET_TYPE_SUB   "SUB"
#define SOCKET_TYPE_REQ   "REQ"
#define SOCKET_TYPE_REP   "REP"

#define SOCKET_OPTION_SUBSCRIBE   "SUBSCRIBE"
#define SOCKET_OPTION_UNSUBSCRIBE "UNSUBSCRIBE"
#define SOCKET_OPTION_IDENTITY    "IDENTITY"
#define SOCKET_OPTION_SNDHWM      "SNDHWM"
#define SOCKET_OPTION_RCVHWM      "RCVHWM"
#define SOCKET_OPTION_SNDBUF      "SNDBUF"
#define SOCKET_OPTION_RCVBUF      "RCVBUF"
#define SOCKET_OPTION_SNDTIMEO    "SNDTIMEO"
#define SOCKET_OPTION_RCVTIMEO    "RCVTIMEO"
#define SOCKET_OPTION_LINGER      "LINGER"
#define SOCKET_OPTION_BACKLOG     "BACKLOG"
#define SOCKET_OPTION_IPV4ONLY    "IPV4ONLY"

#define DELIMITER_NEWLINE '\n'
#define DELIMITER_NULL    '\0'

#define OPT_SEPARATOR '='

#define MAX_STR 1024
#define MAX_OPT 50
#define MAX_ADD 50

#if ZMQ_VERSION < ZMQ_MAKE_VERSION(3, 0, 0)

#define ZMQ_INIT zmq_init(0)
#define ZMQ_TERM(ctx) zmq_term(ctx)
#define ZMQ_SEND(s, m, f) zmq_send(s, m, f)
#define ZMQ_RECV(s, m, f) zmq_recv(s, m, f)

#define ZMQ_SNDHWM ZMQ_HWM
#define ZMQ_RCVHWM -1
#define ZMQ_IPV4ONLY -2

#else

#define ZMQ_INIT zmq_ctx_new()
#define ZMQ_TERM(ctx) zmq_ctx_term(ctx)
#define ZMQ_SEND(s, m, f) zmq_sendmsg(s, m, f)
#define ZMQ_RECV(s, m, f) zmq_recvmsg(s, m, f)

#endif

typedef struct SockAdd {
    char ep[MAX_STR];
} SockAdd;

typedef struct SockOpt {
    char name[MAX_STR];
    int id;
    char value[MAX_STR];
} SockOpt;

static char prog_[MAX_STR];
static int verbose_;
static int bind_;
static int connect_;
static int read_;
static int write_;
static char type_[MAX_STR];
static char delimiter_;
static int iterations_;

static int nadd;
static SockAdd sadd[MAX_ADD];

static int nopt;
static SockOpt sopt[MAX_OPT];

static void* ctxt_;
static void* sock_;
static int stype_;
static int goon_;

static int zc_zmq_is_valid(void);
static void zc_zmq_do_read(void);
static void zc_zmq_do_write(void);
static const char* zc_zmq_get_delimiter(char d, char* buf);
static int zc_zmq_set_options(void);

void zc_zmq_init(const char* s)
{
    strcpy(prog_, s);
    delimiter_ = DELIMITER_NEWLINE;
    stype_ = -1;
    goon_ = 1;
}

void zc_zmq_cleanup(void)
{
    if (sock_ != 0) {
        zmq_close(sock_);
        sock_ = 0;
        if (verbose_)
            fprintf(stderr, "Closed socket\n");
    }
    if (ctxt_ != 0) {
        ZMQ_TERM(ctxt_);
        ctxt_ = 0;
        if (verbose_)
            fprintf(stderr, "Destroyed context\n");
    }

    buffer_clean();
}

void zc_zmq_show_usage(void)
{
    printf("Usage: %s [-hv0rwbc] [-n num] [-o opt=val] TYPE address ...\n",
           prog_[0] ? prog_ : DEFAULT_PROGRAM_NAME);
    printf("  -h: show this help\n");
    printf("  -v: verbose output; default is quiet\n");
    printf("  -0: use \\0 as delimiter for reading / writing; default is \\n\n");
    printf("  -r: read from socket\n");
    printf("  -w: write to socket\n");
    printf("  -b: bind socket to address(es)\n");
    printf("  -c: connect socket to address(es)\n");
    printf("  -n: read / write at most num records; default is infinite\n");
    printf("  -o: set socket option to given value\n"
           "      %s %s %s %s %s\n"
           "      %s %s %s %s %s %s %s\n",
           SOCKET_OPTION_SUBSCRIBE,
           SOCKET_OPTION_UNSUBSCRIBE,
           SOCKET_OPTION_IDENTITY,
           SOCKET_OPTION_SNDHWM,
           SOCKET_OPTION_RCVHWM,
           SOCKET_OPTION_SNDBUF,
           SOCKET_OPTION_RCVBUF,
           SOCKET_OPTION_SNDTIMEO,
           SOCKET_OPTION_RCVTIMEO,
           SOCKET_OPTION_LINGER,
           SOCKET_OPTION_BACKLOG,
           SOCKET_OPTION_IPV4ONLY);
    printf("  TYPE: socket type\n"
           "        %s %s %s %s %s %s\n",
           SOCKET_TYPE_PUSH,
           SOCKET_TYPE_PULL,
           SOCKET_TYPE_PUB,
           SOCKET_TYPE_SUB,
           SOCKET_TYPE_REQ,
           SOCKET_TYPE_REP);

    printf("  address: one or more addresses in ZMQ format\n"
           "           ('tcp://127.0.0.1:5000', 'inproc://pipe')\n");
}

void zc_zmq_set_verbose(int v)
{
    verbose_ = v;
}

void zc_zmq_will_bind(void)
{
    if (connect_) {
        zc_zmq_show_usage();
    }
    connect_ = 0;
    bind_ = 1;
}

void zc_zmq_will_connect(void)
{
    if (bind_) {
        zc_zmq_show_usage();
    }
    connect_ = 1;
    bind_ = 0;
}

void zc_zmq_will_read(void)
{
    if (write_) {
        zc_zmq_show_usage();
    }
    write_ = 0;
    read_ = 1;
}

void zc_zmq_will_write(void)
{
    if (read_) {
        zc_zmq_show_usage();
    }
    write_ = 1;
    read_ = 0;
}

void zc_zmq_set_type(const char* type)
{
    strcpy(type_, type);

    stype_ = -1;
    if (strcmp(type_, SOCKET_TYPE_PUSH) == 0) {
        stype_ = ZMQ_PUSH;
    } else if (strcmp(type_, SOCKET_TYPE_PULL) == 0) {
        stype_ = ZMQ_PULL;
    } else if (strcmp(type_, SOCKET_TYPE_PUB) == 0) {
        stype_ = ZMQ_PUB;
    } else if (strcmp(type_, SOCKET_TYPE_SUB) == 0) {
        stype_ = ZMQ_SUB;
    } else if (strcmp(type_, SOCKET_TYPE_REQ) == 0) {
        stype_ = ZMQ_REQ;
    } else if (strcmp(type_, SOCKET_TYPE_REP) == 0) {
        stype_ = ZMQ_REP;
    } else {
        if (verbose_)
            fprintf(stderr, "Unknown socket type [%s]\n", type_);
    }
}

void zc_zmq_add_address(const char* address)
{
    if (nadd >= MAX_ADD) {
        printf("Too many addresses (max is %d): [%s]\n",
               MAX_ADD, address);
        return;
    }

    strcpy(sadd[nadd].ep, address);
    ++nadd;
}

void zc_zmq_set_delimiter(char d)
{
    delimiter_ = d;
}

void zc_zmq_set_iterations(int n)
{
    iterations_ = n;
}

void zc_zmq_add_option(const char* opt)
{
    char buf[MAX_STR];
    char* p = 0;
    char* q = 0;

    if (nopt >= MAX_OPT) {
        printf("Too many options (max is %d): [%s]\n",
               MAX_OPT, opt);
        return;
    }

    strcpy(buf, opt);
    for (p = buf, q = 0; *p != '\0'; ++p) {
        if (*p == OPT_SEPARATOR) {
            *p = '\0';
            q = p+1;
            break;
        }
    }

    if (q == 0) {
        printf("Invalid option without a valid separator '%c'\n",
               OPT_SEPARATOR);
        return;
    }

    int ok = 1;
    if (strcmp(buf, SOCKET_OPTION_SUBSCRIBE) == 0) {
        sopt[nopt].id = ZMQ_SUBSCRIBE;
    } else if (strcmp(buf, SOCKET_OPTION_UNSUBSCRIBE) == 0) {
        sopt[nopt].id = ZMQ_UNSUBSCRIBE;
    } else if (strcmp(buf, SOCKET_OPTION_IDENTITY) == 0) {
        sopt[nopt].id = ZMQ_IDENTITY;
    } else if (strcmp(buf, SOCKET_OPTION_SNDHWM) == 0) {
        sopt[nopt].id = ZMQ_SNDHWM;
    } else if (strcmp(buf, SOCKET_OPTION_RCVHWM) == 0) {
        sopt[nopt].id = ZMQ_RCVHWM;
    } else if (strcmp(buf, SOCKET_OPTION_SNDBUF) == 0) {
        sopt[nopt].id = ZMQ_SNDBUF;
    } else if (strcmp(buf, SOCKET_OPTION_RCVBUF) == 0) {
        sopt[nopt].id = ZMQ_RCVBUF;
    } else if (strcmp(buf, SOCKET_OPTION_SNDTIMEO) == 0) {
        sopt[nopt].id = ZMQ_SNDTIMEO;
    } else if (strcmp(buf, SOCKET_OPTION_RCVTIMEO) == 0) {
        sopt[nopt].id = ZMQ_RCVTIMEO;
    } else if (strcmp(buf, SOCKET_OPTION_LINGER) == 0) {
        sopt[nopt].id = ZMQ_LINGER;
    } else if (strcmp(buf, SOCKET_OPTION_BACKLOG) == 0) {
        sopt[nopt].id = ZMQ_BACKLOG;
    } else if (strcmp(buf, SOCKET_OPTION_IPV4ONLY) == 0) {
        if (ZMQ_IPV4ONLY < 0)
            ok = 0;
        else
            sopt[nopt].id = ZMQ_IPV4ONLY;
    } else {
        ok = 0;
    }

    if (!ok) {
        printf("Invalid socket option [%s]\n", buf);
        return;
    }

    strcpy(sopt[nopt].name, buf);
    strcpy(sopt[nopt].value, q);
    ++nopt;
}

void zc_zmq_run(void)
{
    int count = 0;
    int subs = 0;
    int j;

    if (! zc_zmq_is_valid())
        return;

    if (verbose_)
        fprintf(stderr, "------\n");

    buffer_init(verbose_);

    ctxt_ = ZMQ_INIT;
    if (verbose_)
        fprintf(stderr, "Context created: %p\n", ctxt_);

    sock_ = zmq_socket(ctxt_, stype_);
    if (verbose_)
        fprintf(stderr, "Socket type %s (%d) created: %p\n",
                type_, stype_, sock_);

    subs = zc_zmq_set_options();

    if (stype_ == ZMQ_SUB && !subs) {
        int ret = zmq_setsockopt(sock_, ZMQ_SUBSCRIBE, 0, 0);
        if (verbose_)
            fprintf(stderr, "Socket automatically subscribed to all messages: %d\n",
                    ret);
    }

    for (j = 0; j < nadd; ++j) {
        if (bind_) {
            int ret = zmq_bind(sock_, sadd[j].ep);
            if (verbose_) {
                fprintf(stderr, "Socket bound to [%s]: %d", sadd[j].ep, ret);
                if (ret < 0)
                    fprintf(stderr, " (%d)", errno);
                fprintf(stderr, "\n");
            }
            continue;
        }
        if (connect_) {
            int ret = zmq_connect(sock_, sadd[j].ep);
            if (verbose_) {
                fprintf(stderr, "Socket connected to [%s]: %d", sadd[j].ep, ret);
                if (ret < 0)
                    fprintf(stderr, " (%d)", errno);
                fprintf(stderr, "\n");
            }
            continue;
        }
    }

    if (verbose_) {
        fprintf(stderr, "Running loop...\n");
        fprintf(stderr, "------\n");
    }

    count = 0;
    goon_ = 1;
    while (goon_) {
        ++count;
        if (iterations_ > 0 &&
            count > iterations_) {
            if (verbose_)
                fprintf(stderr, "Reached %d iterations, aborting\n", iterations_);
            break;
        }
        if (stype_ == ZMQ_REQ) {
            zc_zmq_do_write();
            zc_zmq_do_read();
        } else if (stype_ == ZMQ_REP) {
            zc_zmq_do_read();
            zc_zmq_do_write();
        } else if (read_) {
            zc_zmq_do_read();
        } else if (write_) {
            zc_zmq_do_write();
        } else {
            if (verbose_)
                fprintf(stderr, "Invalid loop mode\n");
            break;
        }
    }

    zc_zmq_cleanup();
}

void zc_zmq_debug(void)
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    char buf[10];
    int j;

    if (! verbose_)
        return;

    if (! zc_zmq_is_valid())
        return;

    zmq_version(&major, &minor, &patch);
    fprintf(stderr, "ZQM version used: %d.%d.%d\n", major, minor, patch);

    fprintf(stderr, "       will bind: %d\n", bind_);
    fprintf(stderr, "    will connect: %d\n", connect_);
    fprintf(stderr, "       will read: %d\n", read_);
    fprintf(stderr, "      will write: %d\n", write_);
    fprintf(stderr, "     socket type: %s (%d)\n", type_, stype_);
    fprintf(stderr, "       delimiter: %s (%d)\n",
            zc_zmq_get_delimiter(delimiter_, buf),
            (int) delimiter_);
    fprintf(stderr, "      iterations: %d\n", iterations_);

    for (j = 0; j < nadd; ++j) {
        fprintf(stderr, "     address #%2d: %s\n",
                j, sadd[j].ep);
    }

    for (j = 0; j < nopt; ++j) {
        fprintf(stderr, "      option #%2d: %s (%d) = [%s]\n",
                j, sopt[j].name, sopt[j].id, sopt[j].value);
    }
}

static int zc_zmq_is_valid(void)
{
    if (stype_ < 0)
        return 0;

    if (nadd <= 0)
        return 0;

    if (!bind_ && !connect_)
        return 0;

    return 1;
}

static void zc_zmq_do_read(void)
{
    zmq_msg_t msg;
    int n;

    if (! goon_)
        return;

    n = zmq_msg_init(&msg);
    if (n < 0) {
        if (verbose_)
            fprintf(stderr, "Message init returned %d (%d), aborting\n",
                    n, errno);
        goon_ = 0;
        return;
    }

    n = ZMQ_RECV(sock_, &msg, 0);
    if (n < 0) {
        if (verbose_)
            fprintf(stderr, "Receive returned %d (%d), aborting\n",
                    n, errno);
        zmq_msg_close(&msg);
        goon_ = 0;
        return;
    }

    void* p = zmq_msg_data(&msg);
    if (verbose_)
        fprintf(stderr, "Received %d:%p:[%*.*s]\n",
                n, p, n, n, (char*) p);
    fwrite(p, 1, n, stdout);
    fputc(DELIMITER_NEWLINE, stdout);
    zmq_msg_close(&msg);
}

static void zc_zmq_free(void* buf, void* hint)
{
    int b = (int) hint;
    if (verbose_)
        fprintf(stderr, "Freeing buffer #%d:%p\n", b, buf);
    buffer_free(b);
}

static void zc_zmq_do_write(void)
{
    int b = 0;
    char* data = 0;
    int eof = 0;
    int p = 0;

    if (! goon_)
        return;

    b = buffer_alloc(&data);
    if (b < 0 || data == 0) {
        // BAD!!!
        goon_ = 0;
        return;
    }
    while (1) {
        int c = fgetc(stdin);
        if (c == EOF) {
            if (verbose_)
                fprintf(stderr, "Found EOF\n");
            eof = 1;
            goon_ = 0;
            break;
        }
        if (p >= MAX_STR ||
            c == delimiter_) {
            break;
        }
        data[p++] = c;
    }

    if (p > 0 || !eof) {
        zmq_msg_t msg;
        int n;

        n = zmq_msg_init_data(&msg, data, p, zc_zmq_free, (void*) b);
        if (n < 0) {
            if (verbose_)
                fprintf(stderr, "Message init returned %d (%d), aborting\n",
                        n, errno);
            goon_ = 0;
            return;
        }

        if (verbose_)
            fprintf(stderr, "Sending %d:#%d:%p:[%*.*s]\n",
                    p, b, data, p, p, data);

        n = ZMQ_SEND(sock_, &msg, 0);
        if (n < 0) {
            if (verbose_)
                fprintf(stderr, "Send returned %d (%d), aborting\n",
                        n, errno);
            zmq_msg_close(&msg);
            goon_ = 0;
            return;
        }

        zmq_msg_close(&msg);
    }
}

static const char* zc_zmq_get_delimiter(char d, char* buf)
{
    buf[0] = '\0';

    if (isprint((int) d)) {
        sprintf(buf, "%c", d);
    } else {
        switch (d) {
        case '\n':
            sprintf(buf, "\\n");
            break;
        case '\0':
            sprintf(buf, "\\0");
            break;
        case '\t':
            sprintf(buf, "\\t");
            break;
        default:
            sprintf(buf, ".");
            break;
        }
    }

    return buf;
}

static int zc_zmq_set_options(void)
{
    int subs = 0;
    int j;

    for (j = 0; j < nopt; ++j) {
        size_t olen = 0;
        int ival;
        int ret;
        switch (sopt[j].id) {
        case ZMQ_SUBSCRIBE:
        case ZMQ_UNSUBSCRIBE:
        case ZMQ_IDENTITY:
            olen = strlen(sopt[j].value);
            ret = zmq_setsockopt(sock_, sopt[j].id, sopt[j].value, olen);
            if (verbose_)
                fprintf(stderr, "Socket option %s (%d) set to %d:[%s] (%d)\n",
                        sopt[j].name, sopt[j].id,
                        olen, sopt[j].value, ret);
            if (! subs)
                subs = (sopt[j].id == ZMQ_SUBSCRIBE);
            break;

        case ZMQ_SNDHWM:
        case ZMQ_RCVHWM:
        case ZMQ_SNDBUF:
        case ZMQ_RCVBUF:
        case ZMQ_SNDTIMEO:
        case ZMQ_RCVTIMEO:
        case ZMQ_LINGER:
        case ZMQ_BACKLOG:
        case ZMQ_IPV4ONLY:
            ival = atoi(sopt[j].value);
            olen = sizeof(ival);
            ret = zmq_setsockopt(sock_, sopt[j].id, &ival, olen);
            if (verbose_)
                fprintf(stderr, "Socket option %s (%d) set to %d (%d)\n",
                        sopt[j].name, sopt[j].id,
                        ival, ret);
            break;

        default:
            printf("Don't know how to handle socket option %s (%d)\n",
                   sopt[j].name, sopt[j].id);
            break;
        }
    }
    return subs;
}
