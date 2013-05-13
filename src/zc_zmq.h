#ifndef ZMQ_HH_
#define ZMQ_HH_

void zc_zmq_init(const char* s);
void zc_zmq_cleanup(void);

void zc_zmq_show_usage(void);

void zc_zmq_will_bind(void);
void zc_zmq_will_connect(void);
void zc_zmq_will_read(void);
void zc_zmq_will_write(void);

void zc_zmq_set_verbose(int v);

void zc_zmq_set_type(const char* type);
void zc_zmq_set_address(const char* address);
void zc_zmq_set_delimiter(char d);
void zc_zmq_set_iterations(int n);
void zc_zmq_add_option(const char* opt);

void zc_zmq_run(void);
void zc_zmq_debug(void);

#endif
