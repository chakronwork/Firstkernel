#ifndef FIRSTOS_IPC_H
#define FIRSTOS_IPC_H

#include <stdint.h>

#define IPC_MAX_MESSAGES 16U
#define IPC_MESSAGE_SIZE 64U

int ipc_init(void);

int ipc_send(
    uint32_t receiver_id,
    const void *buffer,
    uint32_t length
);

int ipc_receive(
    void *buffer,
    uint32_t capacity
);

#endif /* FIRSTOS_IPC_H */
