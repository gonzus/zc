#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zmq.h>
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

#define MAX_BUF 1024
#define MAX_OPT 100

typedef struct SockOpt {
    char name[MAX_BUF];
    int id;
    char value[MAX_BUF];
} SockOpt;

static char prog_[MAX_BUF];
static int verbose_;
static int bind_;
static int connect_;
static int read_;
static int write_;
static char type_[MAX_BUF];
static char address_[MAX_BUF];
static char delimiter_;
static int iterations_;
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
        zmq_ctx_term(ctxt_);
        ctxt_ = 0;
        if (verbose_)
            fprintf(stderr, "Destroyed context\n");
    }

    address_[0] = '\0';
    type_[0] = '\0';
}

void zc_zmq_show_usage(void)
{
    printf("Usage: %s [-hv0rwbc] [-n num] [-o opt=val] TYPE address\n",
           prog_[0] ? prog_ : DEFAULT_PROGRAM_NAME);
    printf("  -h: show this help\n");
    printf("  -v: verbose output; default is quiet\n");
    printf("  -0: use \\0 as delimiter for reading / writing; default is \\n\n");
    printf("  -r: read from socket\n");
    printf("  -w: write to socket\n");
    printf("  -b: bind socket to address\n");
    printf("  -c: connect socket to address\n");
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

    printf("  address: address in ZMQ format ('tcp://127.0.0.1:5000')\n");
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

void zc_zmq_set_address(const char* address)
{
    strcpy(address_, address);
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
    char buf[MAX_BUF];
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
        sopt[nopt].id = ZMQ_IPV4ONLY;
    } else {
        printf("Unknown socket option [%s]\n", buf);
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

    if (! zc_zmq_is_valid())
        return;

    if (verbose_)
        fprintf(stderr, "------\n");

    ctxt_ = zmq_ctx_new();
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

    if (bind_) {
        int ret = zmq_bind(sock_, address_);
        if (verbose_)
            fprintf(stderr, "Socket bound to [%s]: %d\n",
                    address_, ret);
    }
    if (connect_) {
        int ret = zmq_connect(sock_, address_);
        if (verbose_)
            fprintf(stderr, "Socket connected to [%s]: %d\n",
                    address_, ret);
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
    fprintf(stderr, "  socket address: %s\n", address_);
    fprintf(stderr, "       delimiter: %s (%d)\n",
            zc_zmq_get_delimiter(delimiter_, buf),
            (int) delimiter_);
    fprintf(stderr, "      iterations: %d\n", iterations_);

    for (j = 0; j < nopt; ++j) {
        fprintf(stderr, "      option #%2d: %s (%d) = [%s]\n",
                j, sopt[j].name, sopt[j].id, sopt[j].value);
    }
}

static int zc_zmq_is_valid(void)
{
    if (stype_ < 0)
        return 0;

    if (address_ == 0)
        return 0;

    if (!bind_ && !connect_)
        return 0;

    return 1;
}

static void zc_zmq_do_read(void)
{
    char buf[MAX_BUF];
    int n;

    if (! goon_)
        return;

    n = zmq_recv(sock_, buf, MAX_BUF, 0);
    if (n < 0) {
        goon_ = 0;
        return;
    }
    if (verbose_)
        fprintf(stderr, "Read %d:[%*.*s]\n", n, n, n, buf);

    fwrite(buf, 1, n, stdout);
    fputc(DELIMITER_NEWLINE, stdout);
}

static void zc_zmq_do_write(void)
{
    char buf[MAX_BUF];
    int eof = 0;
    int p = 0;

    if (! goon_)
        return;

    while (1) {
        int c = fgetc(stdin);
        if (c == EOF) {
            eof = 1;
            goon_ = 0;
            break;
        }
        if (p >= MAX_BUF ||
            c == delimiter_) {
            break;
        }
        buf[p++] = c;
    }

    if (p > 0 || ! eof) {
        int n;
        buf[p] = '\0';
        if (verbose_)
            fprintf(stderr, "Sending %d:[%*.*s]\n", p, p, p, buf);
        n = zmq_send(sock_, buf, p, 0);
        if (n < 0 || n != p) {
            goon_ = 0;
            return;
        }
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
