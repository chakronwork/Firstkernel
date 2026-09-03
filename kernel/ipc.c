#include <stdint.h>

#include "ipc.h"
#include "task.h"
#include "uaccess.h"

struct ipc_message
{
    uint32_t sender_id;
    uint32_t receiver_id;
    uint32_t length;

    uint8_t payload[
        IPC_MESSAGE_SIZE
    ];
};

static struct ipc_message queue[
    IPC_MAX_MESSAGES
];

static uint32_t queue_head = 0;
static uint32_t queue_count = 0;

static int task_is_valid_receiver(
    uint32_t task_id
)
{
    struct task *task =
        task_get(task_id);

    if (task == 0)
        return 0;

    if (task->state == TASK_UNUSED)
        return 0;

    if (task->state == TASK_DEAD)
        return 0;

    return 1;
}

int ipc_init(void)
{
    queue_head = 0;
    queue_count = 0;

    for (
        uint32_t i = 0;
        i < IPC_MAX_MESSAGES;
        ++i
    ) {
        queue[i].sender_id = 0;
        queue[i].receiver_id = 0;
        queue[i].length = 0;

        for (
            uint32_t j = 0;
            j < IPC_MESSAGE_SIZE;
            ++j
        ) {
            queue[i].payload[j] = 0;
        }
    }

    return 1;
}

int ipc_send(
    uint32_t receiver_id,
    const void *buffer,
    uint32_t length
)
{
    struct task *sender;
    struct ipc_message *message;

    if (buffer == 0)
        return 0;

    if (length == 0 ||
        length > IPC_MESSAGE_SIZE)
    {
        return 0;
    }

    sender =
        task_current();

    if (sender == 0)
        return 0;

    if (!task_is_valid_receiver(receiver_id))
        return 0;

    if (queue_count >= IPC_MAX_MESSAGES)
        return 0;

    if (!uaccess_verify_read(
            buffer,
            length
        ))
    {
        return 0;
    }

    uint32_t index =
        (queue_head + queue_count) %
        IPC_MAX_MESSAGES;

    message =
        &queue[index];

    message->sender_id =
        sender->id;

    message->receiver_id =
        receiver_id;

    message->length =
        length;

    if (!copy_from_user(
            message->payload,
            buffer,
            length
        ))
    {
        message->sender_id = 0;
        message->receiver_id = 0;
        message->length = 0;

        return 0;
    }

    queue_count++;

    return (int)length;
}

int ipc_receive(
    void *buffer,
    uint32_t capacity
)
{
    struct task *receiver;
    struct ipc_message *message;

    if (buffer == 0)
        return 0;

    if (capacity == 0)
        return 0;

    receiver =
        task_current();

    if (receiver == 0)
        return 0;

    if (queue_count == 0)
        return 0;

    /*
     * Search the queue for the first message
     * addressed to the current task.
     */
    uint32_t found_offset = IPC_MAX_MESSAGES;

    for (
        uint32_t offset = 0;
        offset < queue_count;
        ++offset
    ) {
        uint32_t index =
            (queue_head + offset) %
            IPC_MAX_MESSAGES;

        if (
            queue[index].receiver_id ==
            receiver->id
        ) {
            found_offset = offset;
            break;
        }
    }

    if (found_offset == IPC_MAX_MESSAGES)
        return 0;

    uint32_t index =
        (queue_head + found_offset) %
        IPC_MAX_MESSAGES;

    message =
        &queue[index];

    if (capacity < message->length)
        return 0;

    if (!uaccess_verify_write(
            buffer,
            message->length
        ))
    {
        return 0;
    }

    uint32_t message_length =
        message->length;

    if (!copy_to_user(
            buffer,
            message->payload,
            message_length
        ))
    {
        return 0;
    }

    /*
     * Remove the message while preserving
     * FIFO ordering for the remaining entries.
     */
    for (
        uint32_t offset = found_offset;
        offset + 1U < queue_count;
        ++offset
    ) {
        uint32_t current =
            (queue_head + offset) %
            IPC_MAX_MESSAGES;

        uint32_t next =
            (queue_head + offset + 1U) %
            IPC_MAX_MESSAGES;

        queue[current] =
            queue[next];
    }

    uint32_t last =
        (queue_head + queue_count - 1U) %
        IPC_MAX_MESSAGES;

    queue[last].sender_id = 0;
    queue[last].receiver_id = 0;
    queue[last].length = 0;

    for (
        uint32_t i = 0;
        i < IPC_MESSAGE_SIZE;
        ++i
    ) {
        queue[last].payload[i] = 0;
    }

    queue_count--;

    if (queue_count == 0)
        queue_head = 0;

    return (int)message_length;
}
