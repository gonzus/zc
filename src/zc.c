#include <stdio.h>
#include <stdlib.h>
#include "getopt.h"
#include "zc_zmq.h"

int main(int argc, char* argv[])
{
    zc_zmq_init(argv[0]);

    opterr = 0;
    while (1) {
        int c = getopt(argc, argv, "hbcrw0vn:o:");
        if (c < 0) {
            break;
        }

        switch (c) {
        case 'h':
            zc_zmq_show_usage();
            return 0;

        case 'b':
            zc_zmq_will_bind();
            break;

        case 'c':
            zc_zmq_will_connect();
            break;

        case 'r':
            zc_zmq_will_read();
            break;

        case 'w':
            zc_zmq_will_write();
            break;

        case '0':
            zc_zmq_set_delimiter('\0');
            break;

        case 'v':
            zc_zmq_set_verbose(1);
            break;

        case 'n':
            zc_zmq_set_iterations(atoi(optarg));
            break;

        case 'o':
            zc_zmq_add_option(optarg);
            break;

        default:
            printf("Unknown option '%c'\n", optopt);
            break;
        }
    }

    if ((argc - optind) != 2) {
        zc_zmq_show_usage();
    } else {
        zc_zmq_set_type(argv[optind+0]);
        zc_zmq_set_address(argv[optind+1]);
    }

    zc_zmq_debug();
    zc_zmq_run();
    zc_zmq_cleanup();

    return 0;
}
