#include <stdio.h>
#include <mach/mach.h>
#include <mach/error.h>
#include <mach/message.h>
#include <unistd.h>

static int32_t send_foo(mach_port_t remote_port, int32_t n) {
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
        mach_error("Can't send mach msg\n", err);
        return -1;
    }

    return 0;
}

static int32_t recv_foo(mach_port_t recv_port, int32_t *n) {
    kern_return_t err;

    struct {
        mach_msg_header_t header;
        mach_msg_body_t body;
        int32_t n;
        mach_msg_trailer_t trailer;
    } msg;

    err = mach_msg(&msg.header, MACH_RCV_MSG,
                   0, sizeof msg, recv_port,
                   MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (err != KERN_SUCCESS) {
        mach_error("Can't recieve mach message\n", err);
        return -1;
    }

    (*n) = msg.n;
    return 0;
}

static int32_t send_port(mach_port_t remote_port, mach_port_t port) {
    kern_return_t err;

    struct  {
        mach_msg_header_t header;
        mach_msg_body_t body;
        mach_msg_port_descriptor_t task_port;
    } msg;

    msg.header.msgh_remote_port = remote_port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
    msg.header.msgh_size = sizeof msg;
    msg.body.msgh_descriptor_count = 1;
    msg.task_port.name = port;
    msg.task_port.disposition = MACH_MSG_TYPE_COPY_SEND;
    msg.task_port.type = MACH_MSG_PORT_DESCRIPTOR;

    err = mach_msg_send(&msg.header);
    if (err != KERN_SUCCESS) {
        mach_error("Can't send mach msg\n", err);
        return -1;
    }

    return 0;
}

static int32_t recv_port(mach_port_t recv_port, mach_port_t *port) {
    kern_return_t err;

    struct {
        mach_msg_header_t header;
        mach_msg_body_t body;
        mach_msg_port_descriptor_t task_port;
        mach_msg_trailer_t trailer;
    } msg;

    err = mach_msg(&msg.header, MACH_RCV_MSG,
                   0, sizeof msg, recv_port,
                   MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (err != KERN_SUCCESS) {
        mach_error("Can't recieve mach message\n", err);
        return -1;
    }

    (*port) = msg.task_port.name;
    return 0;
}

static int32_t setup_recv_port(mach_port_t *recv_port) {
    kern_return_t err;
    mach_port_t port = MACH_PORT_NULL;

    err = mach_port_allocate(mach_task_self(),
                             MACH_PORT_RIGHT_RECEIVE, &port);
    if (err != KERN_SUCCESS) {
        mach_error("Can't allocate mach port\n", err);
        return -1;
    }

    err = mach_port_insert_right(mach_task_self(),
                                 port,
                                 port,
                                 MACH_MSG_TYPE_MAKE_SEND);
    if (err != KERN_SUCCESS) {
        mach_error("Can't insert port right\n", err);
        return -1;
    }

    (*recv_port) = port;
    return 0;
}

static int32_t start(mach_port_t port, void *arg) {
    printf("start %d\n", port);
    return 0;
}

static pid_t fork_pass_port(mach_port_t *pass_port, int32_t (*child_start)(mach_port_t port, void *arg), void *arg) {
    pid_t pid = 0;
    int32_t rtrn = 0;
    kern_return_t err;
    mach_port_t special_port = MACH_PORT_NULL;

    /* Allocate the mach port. */
    if (setup_recv_port(pass_port) != 0) {
        printf("Can't setup mach port\n");
        return -1;
    }

    /* Grab our current process's bootstrap port. */
    err = task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, &special_port);
    if (err != KERN_SUCCESS) {
        mach_error("Can't get special port:\n", err);
        return -1;
    }

    /* Set the special port as the parent recv port.  */
    err = task_set_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, (*pass_port));
    if (err != KERN_SUCCESS) {
        mach_error("Can't set special port:\n", err);
        return -1;
    }

    pid = fork();
    if (pid == 0) {
        mach_port_t bootstrap_port = MACH_PORT_NULL;
        mach_port_t port = MACH_PORT_NULL;

        /* In the child process grab the port passed by the parent. */
        err = task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, pass_port);
        if (err != KERN_SUCCESS) {
            mach_error("Can't get special port:\n", err);
            return -1;
        }

        /* Create a port with a send right. */
        if (setup_recv_port(&port) != 0) {
            printf("Can't setup mach port\n");
            return -1;
        }

        /* Send port to parent. */
        rtrn = send_port((*pass_port), port);
        if (rtrn < 0) {
            printf("Can't send port\n");
            return -1;
        }

        /* Receive the real bootstrap port from the parent. */
        rtrn = recv_port(port, &bootstrap_port);
        if (rtrn < 0) {
            printf("Can't receive bootstrap port\n");
            return -1;
        }

        /* Set the bootstrap port back to normal. */
        err = task_set_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, bootstrap_port);
        if (err != KERN_SUCCESS) {
            mach_error("Can't set special port:\n", err);
            return -1;
        }

        /* Now that were done with the port dance, start the function passed by the caller. */
        child_start((*pass_port), arg);

        int32_t n = 0;
        rtrn = recv_foo(port, &n);
        if (rtrn < 0) {
            printf("Can't receive foo\n");
            return -1;
        }
        printf("chile received foo %d\n", n);
        rtrn = send_foo((*pass_port), n + 1);
        if (rtrn < 0) {
            printf("Can't send foo\n");
            return -1;
        }

        /* Exit and clean up the child process. */
        _exit(0);
    } else if (pid > 0) {
        mach_port_t child_port = MACH_PORT_NULL;

        /* Grab the child's recv port. */
        rtrn = recv_port((*pass_port), &child_port);
        if (rtrn < 0) {
            printf("Can't recv port\n");
            return -1;
        }

        /* Send the child the original bootstrap port. */
        rtrn = send_port(child_port, special_port);
        if (rtrn < 0) {
            printf("Can't send bootstrap port\n");
            return -1;
        }

        /* Reset parents special port. */
        err = task_set_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT, special_port);
        if (err != KERN_SUCCESS) {
            mach_error("Can't set special port:\n", err);
            return -1;
        }

        rtrn = send_foo(child_port, 123);
        if (rtrn < 0) {
            printf("Can't send foo\n");
            return -1;
        }
        int32_t n = 0;
        rtrn = recv_foo((*pass_port), &n);
        if (rtrn < 0) {
            printf("Can't receive foo\n");
            return -1;
        }
        printf("parent received foo %d\n", n);

        return 0;
    } else {
        /* Error, so cleanup the mach port. */
        err = mach_port_deallocate(mach_task_self(), (*pass_port));
        if (err != KERN_SUCCESS) {
            mach_error("Can't deallocate mach port\n", err);
            return -1;
        }

        perror("fork");

        return -1;
    }
}

int main() {
    /* Mach port we want to pass to the child. */
    mach_port_t port = MACH_PORT_NULL;

    /* Argument to pass to the child process. */
    char *arg = "argument";

    pid_t pid = fork_pass_port(&port, start, arg);
    if (pid < 0) {
        printf("Can't fork and pass msg port\n");
        return -1;
    }

    return 0;
}
