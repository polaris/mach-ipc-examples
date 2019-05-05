#include <stdio.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>

#define SERVICE_NAME "com.foo.bar"

static int32_t send(mach_port_t remote_port, int32_t n) {
    kern_return_t err;

    struct  {
        mach_msg_header_t header;
        mach_msg_body_t body;
        int32_t n;
    } msg;

    msg.header.msgh_remote_port = remote_port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.header.msgh_size = sizeof msg;
    msg.body.msgh_descriptor_count = 1;
    msg.n = n;

    err = mach_msg_send(&msg.header);
    if (err != KERN_SUCCESS) {
        //mach_error("mach_msg_send", err);
        fprintf(stderr, "mach_msg_send failed: %s\n", mach_error_string(err));
        return -1;
    }

    return 0;
}

int main() {
    kern_return_t err;
    mach_port_t port;

    err = bootstrap_look_up(bootstrap_port, (char *)SERVICE_NAME, &port);
    if (err != KERN_SUCCESS) {
        fprintf(stderr, "bootstrap_look_up failed: %s\n", bootstrap_strerror(err));
        return -1;
    }

    send(port, 123);

    return 0;
}
