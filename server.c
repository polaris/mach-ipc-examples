#include <mach/mach.h>
#include <stdio.h>
#include <servers/bootstrap.h>

#define SERVICE_NAME "com.foo.bar"

static int32_t receive(mach_port_t recv_port, int32_t *n) {
    kern_return_t err;

    struct {
        mach_msg_header_t header;
        mach_msg_body_t body;
        int32_t n;
        mach_msg_trailer_t trailer;
    } msg;

    err = mach_msg(&msg.header,
                   MACH_RCV_MSG,
                   0,
                   sizeof msg,
                   recv_port,
                   MACH_MSG_TIMEOUT_NONE,
                   MACH_PORT_NULL);
    if (err != KERN_SUCCESS) {
        fprintf(stderr, "mach_msg failed: %s\n", mach_error_string(err));
        return -1;
    }

    (*n) = msg.n;

    return 0;
}

static int32_t register_bootstrap_service() {
    kern_return_t err;
    mach_port_t service_receive_port;

    err = bootstrap_check_in(bootstrap_port, (char *)SERVICE_NAME, &service_receive_port);
    if (err != KERN_SUCCESS) {
        fprintf(stderr, "bootstrap_check_in failed: %s\n", bootstrap_strerror(err));
        mach_error("bootstrap_check_in", err);
        return -1;
    } 

    int32_t n = 0;
    receive(service_receive_port, &n);
    printf("%d\n", n);

    return 0;
}

int main() {
    kern_return_t err;

    err = register_bootstrap_service();
    if (err != 0) {
        return -1;
    }

    return 0;
}
