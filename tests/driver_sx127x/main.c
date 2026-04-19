/*
 * Copyright (C) 2016 Unwired Devices <info@unwds.com>
 *               2017 Inria Chile
 *               2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 * @file
 * @brief       Test application for SX127X modem driver
 *
 * @author      Eugene P. <ep@unwds.com>
 * @author      José Ignacio Alamos <jose.alamos@inria.cl>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "thread.h"
#include "shell.h"
// #include "shell_commands.h"

#include "net/netdev.h"
#include "net/netdev/lora.h"
#include "net/lora.h"

#include "board.h"

#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include "fmt.h"

#include "users.h"
#include "message.h"
#define SX127X_LORA_MSG_QUEUE (16U)
#ifndef SX127X_STACKSIZE
#define SX127X_STACKSIZE (THREAD_STACKSIZE_DEFAULT)
#endif

#define MSG_FIFO_SIZE 16
#define MSG_TYPE_ISR (0x3456)
#define MAX_GROUP 32
static char stack[SX127X_STACKSIZE];
static kernel_pid_t _recv_pid;
static chat_message_t my_mess;
static int msg_count = 0;
static uint32_t my_uid = 777;
static sx127x_t sx127x;
static int listenmode; // 0 if non hex 1 otherwise

uint32_t my_groups[MAX_GROUP];
uint8_t group_number = 0;
uint32_t current_group = 0;
uint32_t current_target = 0;
typedef struct {
    chat_message_t msg;
    int16_t rssi;
    int8_t  snr;
} rx_entry_t;

static rx_entry_t  msg_fifo[MSG_FIFO_SIZE];
static int         fifo_head  = 0;
static int         fifo_count = 0;
int lora_setup_cmd(int argc, char **argv)
{

    if (argc < 4)
    {
        puts("usage: setup "
             "<bandwidth (125, 250, 500)> "
             "<spreading factor (7..12)> "
             "<code rate (5..8)>");
        return -1;
    }

    /* Check bandwidth value */
    int bw = atoi(argv[1]);
    uint8_t lora_bw;

    switch (bw)
    {
    case 125:
        puts("setup: setting 125KHz bandwidth");
        lora_bw = LORA_BW_125_KHZ;
        break;

    case 250:
        puts("setup: setting 250KHz bandwidth");
        lora_bw = LORA_BW_250_KHZ;
        break;

    case 500:
        puts("setup: setting 500KHz bandwidth");
        lora_bw = LORA_BW_500_KHZ;
        break;

    default:
        puts("[Error] setup: invalid bandwidth value given, "
             "only 125, 250 or 500 allowed.");
        return -1;
    }

    /* Check spreading factor value */
    uint8_t lora_sf = atoi(argv[2]);

    if (lora_sf < 7 || lora_sf > 12)
    {
        puts("[Error] setup: invalid spreading factor value given");
        return -1;
    }

    /* Check coding rate value */
    int cr = atoi(argv[3]);

    if (cr < 5 || cr > 8)
    {
        puts("[Error ]setup: invalid coding rate value given");
        return -1;
    }
    uint8_t lora_cr = (uint8_t)(cr - 4);

    /* Configure radio device */
    netdev_t *netdev = &sx127x.netdev;

    netdev->driver->set(netdev, NETOPT_BANDWIDTH,
                        &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
                        &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE,
                        &lora_cr, sizeof(lora_cr));

    puts("[Info] setup: configuration set with success");

    return 0;
}

int random_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    netdev_t *netdev = &sx127x.netdev;
    uint32_t rand;

    netdev->driver->get(netdev, NETOPT_RANDOM, &rand, sizeof(rand));
    printf("random: number from sx127x: %u\n",
           (unsigned int)rand);

    /* reinit the transceiver to default values */
    sx127x_init_radio_settings(&sx127x);

    return 0;
}

int register_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        puts("usage: register <get | set>");
        return -1;
    }

    if (strstr(argv[1], "get") != NULL)
    {
        if (argc < 3)
        {
            puts("usage: register get <all | allinline | regnum>");
            return -1;
        }

        if (strcmp(argv[2], "all") == 0)
        {
            puts("- listing all registers -");
            uint8_t reg = 0, data = 0;
            /* Listing registers map */
            puts("Reg   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
            for (unsigned i = 0; i <= 7; i++)
            {
                printf("0x%02X ", i << 4);

                for (unsigned j = 0; j <= 15; j++, reg++)
                {
                    data = sx127x_reg_read(&sx127x, reg);
                    printf("%02X ", data);
                }
                puts("");
            }
            puts("-done-");
            return 0;
        }
        else if (strcmp(argv[2], "allinline") == 0)
        {
            puts("- listing all registers in one line -");
            /* Listing registers map */
            for (uint16_t reg = 0; reg < 256; reg++)
            {
                printf("%02X ", sx127x_reg_read(&sx127x, (uint8_t)reg));
            }
            puts("- done -");
            return 0;
        }
        else
        {
            long int num = 0;
            /* Register number in hex */
            if (strstr(argv[2], "0x") != NULL)
            {
                num = strtol(argv[2], NULL, 16);
            }
            else
            {
                num = atoi(argv[2]);
            }

            if (num >= 0 && num <= 255)
            {
                printf("[regs] 0x%02X = 0x%02X\n",
                       (uint8_t)num,
                       sx127x_reg_read(&sx127x, (uint8_t)num));
            }
            else
            {
                puts("regs: invalid register number specified");
                return -1;
            }
        }
    }
    else if (strstr(argv[1], "set") != NULL)
    {
        if (argc < 4)
        {
            puts("usage: register set <regnum> <value>");
            return -1;
        }

        long num, val;

        /* Register number in hex */
        if (strstr(argv[2], "0x") != NULL)
        {
            num = strtol(argv[2], NULL, 16);
        }
        else
        {
            num = atoi(argv[2]);
        }

        /* Register value in hex */
        if (strstr(argv[3], "0x") != NULL)
        {
            val = strtol(argv[3], NULL, 16);
        }
        else
        {
            val = atoi(argv[3]);
        }

        sx127x_reg_write(&sx127x, (uint8_t)num, (uint8_t)val);
    }
    else
    {
        puts("usage: register get <all | allinline | regnum>");
        return -1;
    }

    return 0;
}

int send_cmd(int argc, char **argv)
{
    if (argc <= 1)
    {
        puts("usage: send <payload>");
        return -1;
    }

    printf("sending \"%s\" payload (%u bytes)\n",
           argv[1], (unsigned)strlen(argv[1]) + 1);

    chat_message_t msg_out;
    memset(&msg_out, 0, sizeof(msg_out));
    strncpy(msg_out.message, argv[1], sizeof(msg_out.message) - 1);
    msg_out.uid         = my_uid;
    msg_out.message_id  = (uint32_t)msg_count++;
    msg_out.group       = current_group;
    msg_out.target_user = current_target;

    iolist_t iolist = {
        .iol_base = &msg_out,
        .iol_len  = sizeof(chat_message_t)
    };
    netdev_t *netdev = &sx127x.netdev;

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP)
    {
        puts("Cannot send: radio is still transmitting");
    }

    return 0;
}

int listen_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (argc > 1)
    {
        if (strcmp(argv[1], "hex") == 0) 
        {
            listenmode = 1; // 1 if we listen in hex mode
        }
    }
    else
    {
        listenmode = 0; // 0 OW
    }
    netdev_t *netdev = &sx127x.netdev;
    /* Switch to continuous listen mode */
    const netopt_enable_t single = false;

    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 0;

    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

    /* Switch to RX state */
    netopt_state_t state = NETOPT_STATE_RX;

    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));

    printf("Listen mode set\n");

    return 0;
}

int syncword_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        puts("usage: syncword <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint8_t syncword;

    if (strstr(argv[1], "get") != NULL)
    {
        netdev->driver->get(netdev, NETOPT_SYNCWORD, &syncword,
                            sizeof(syncword));
        printf("Syncword: 0x%02x\n", syncword);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL)
    {
        if (argc < 3)
        {
            puts("usage: syncword set <syncword>");
            return -1;
        }
        syncword = fmt_hex_byte(argv[2]);
        netdev->driver->set(netdev, NETOPT_SYNCWORD, &syncword,
                            sizeof(syncword));
        printf("Syncword set to %02x\n", syncword);
    }
    else
    {
        puts("usage: syncword <get|set>");
        return -1;
    }

    return 0;
}
int channel_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        puts("usage: channel <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint32_t chan;

    if (strstr(argv[1], "get") != NULL)
    {
        netdev->driver->get(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("Channel: %i\n", (int)chan);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL)
    {
        if (argc < 3)
        {
            puts("usage: channel set <channel>");
            return -1;
        }
        chan = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("New channel set\n");
    }
    else
    {
        puts("usage: channel <get|set>");
        return -1;
    }

    return 0;
}

int rx_timeout_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        puts("usage: channel <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint16_t rx_timeout;

    if (strstr(argv[1], "set") != NULL)
    {
        if (argc < 3)
        {
            puts("usage: rx_timeout set <rx_timeout>");
            return -1;
        }
        rx_timeout = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_RX_SYMBOL_TIMEOUT, &rx_timeout,
                            sizeof(rx_timeout));
        printf("rx_timeout set to %i\n", rx_timeout);
    }
    else
    {
        puts("usage: rx_timeout set");
        return -1;
    }

    return 0;
}

int reset_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    netdev_t *netdev = &sx127x.netdev;

    puts("resetting sx127x...");
    netopt_state_t state = NETOPT_STATE_RESET;

    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(netopt_state_t));
    return 0;
}

static void _set_opt(netdev_t *netdev, netopt_t opt, bool val, char *str_help)
{
    netopt_enable_t en = val ? NETOPT_ENABLE : NETOPT_DISABLE;

    netdev->driver->set(netdev, opt, &en, sizeof(en));
    printf("Successfully ");
    if (val)
    {
        printf("enabled ");
    }
    else
    {
        printf("disabled ");
    }
    printf("%s\n", str_help);
}

int crc_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0)
    {
        printf("usage: %s set <1|0>\n", argv[0]);
        return 1;
    }

    int tmp = atoi(argv[2]);

    _set_opt(netdev, NETOPT_INTEGRITY_CHECK, tmp, "CRC check");
    return 0;
}

int implicit_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0)
    {
        printf("usage: %s set <1|0>\n", argv[0]);
        return 1;
    }

    int tmp = atoi(argv[2]);

    _set_opt(netdev, NETOPT_FIXED_HEADER, tmp, "implicit header");
    return 0;
}

int payload_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0)
    {
        printf("usage: %s set <payload length>\n", argv[0]);
        return 1;
    }

    uint16_t tmp = atoi(argv[2]);

    netdev->driver->set(netdev, NETOPT_PDU_SIZE, &tmp, sizeof(tmp));
    printf("Successfully set payload to %i\n", tmp);
    return 0;
}
static size_t convert_bytes_to_hex(char *dest, uint8_t *src, size_t len)
{
    size_t i;
    if (dest == NULL)
    {
        return -1;
    }
    for (i = 0; i < len; i++)
    {
        dest += sprintf(dest, "%02x", src[i]);
    }
    return i;
}
/** Push a received message into the FIFO. */
static void fifo_push(const chat_message_t *msg, int16_t rssi, int8_t snr)
{
    msg_fifo[fifo_head].msg  = *msg;
    msg_fifo[fifo_head].rssi = rssi;
    msg_fifo[fifo_head].snr  = snr;
    fifo_head = (fifo_head + 1) % MSG_FIFO_SIZE;
    if (fifo_count < MSG_FIFO_SIZE) {
        fifo_count++;
    }
}
/** Return 1 if we belong to the group carried in msg, 0 otherwise. */
static int _am_i_recipient(const chat_message_t *msg)
{
    if (msg->group != 0) {
        for (int i = 0; i < group_number; i++) {
            if (my_groups[i] == msg->group) {
                return 1;
            }
        }
        return 0;
    }
    if (msg->target_user == 0 || msg->target_user == my_uid) {
        return 1;
    }
    return 0;
}
/** Print one chat message in the LoRaChat text format. */
static void _print_chat_message(const chat_message_t *msg, int16_t rssi,
                                int8_t snr)
{
    /* Sender */
    printf("%" PRIu32, msg->uid);

    /* Destination */
    if (msg->group != 0) {
        printf("#%" PRIu32, msg->group);
    } else {
        printf("@");
        if (msg->target_user == 0) {
            printf("*");
        } else {
            printf("%" PRIu32, msg->target_user);
        }
    }

    /* Message id and body */
    printf(":%" PRIu32 ":%s", msg->message_id, msg->message);

    /* Radio info */
    printf("  [RSSI:%d SNR:%d]\n", rssi, snr);
}
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR)
    {
        msg_t msg;

        msg.type = MSG_TYPE_ISR;
        msg.content.ptr = dev;

        if (msg_send(&msg, _recv_pid) <= 0)
        {
            puts("gnrc_netdev: possibly lost interrupt.");
        }
    }
    else
    {
        size_t len;
        netdev_lora_rx_info_t packet_info;
        switch (event)
        {
        case NETDEV_EVENT_RX_STARTED:
            puts("Data reception started");
            break;

        case NETDEV_EVENT_RX_COMPLETE:
              case NETDEV_EVENT_RX_COMPLETE:
        len = dev->driver->recv(dev, NULL, 0, 0);

        if (len > sizeof(chat_message_t)) {
            len = sizeof(chat_message_t);
        }
        dev->driver->recv(dev, &my_mess, len, &packet_info);

        update_user(my_mess.uid, my_mess.message_id);

        if (!_am_i_recipient(&my_mess)) {
            break;
        }

        fifo_push(&my_mess, packet_info.rssi, packet_info.snr);

        if (listenmode == 0) {
            _print_chat_message(&my_mess, packet_info.rssi, packet_info.snr);
        } else {
            char hex_buf[sizeof(my_mess.message) * 2 + 1];
            convert_bytes_to_hex(hex_buf, (uint8_t *)my_mess.message,
                                 len - offsetof(chat_message_t, message));
            hex_buf[sizeof(hex_buf) - 1] = '\0';

            printf("%" PRIu32, my_mess.uid);
            if (my_mess.group != 0) {
                printf("#%" PRIu32, my_mess.group);
            } else {
                printf("@%s", my_mess.target_user == 0
                               ? "*"
                               : "");
                if (my_mess.target_user != 0) {
                    printf("%" PRIu32, my_mess.target_user);
                }
            }
            printf(":%" PRIu32 ":[hex]%s  [RSSI:%d SNR:%d]\n",
                   my_mess.message_id, hex_buf,
                   packet_info.rssi, (int)packet_info.snr);
        }
        break;
        case NETDEV_EVENT_TX_COMPLETE:
            sx127x_set_sleep(&sx127x);
            puts("Transmission completed");
            break;

        case NETDEV_EVENT_CAD_DONE:
            break;

        case NETDEV_EVENT_TX_TIMEOUT:
            sx127x_set_sleep(&sx127x);
            break;

        default:
            printf("Unexpected netdev event received: %d\n", event);
            break;
        }
    }
}

void *_recv_thread(void *arg)
{
    (void)arg;

    static msg_t _msg_q[SX127X_LORA_MSG_QUEUE];

    msg_init_queue(_msg_q, SX127X_LORA_MSG_QUEUE);

    while (1)
    {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR)
        {
            netdev_t *dev = msg.content.ptr;
            dev->driver->isr(dev);
        }
        else
        {
            puts("Unexpected msg type");
        }
    }
}
static size_t convert_hex(uint8_t *dest, const char *src)
{
    size_t i;
    int value;
    size_t count = strlen(src);
    if (dest == NULL)
    {
        return -1;
    }
    for (i = 0; i < count && sscanf(src + i * 2, "%2x", &value) == 1; i++)
    {
        dest[i] = value;
    }
    return i;
}

int sendhex_cmd(int argc, char **argv)
{

    chat_message_t msg_out;

    if (argc < 2)
    {
        puts("usage: sendhex <hexstring>");
        puts("  e.g. sendhex deadbeef");
        return -1;
    }

    size_t hex_len = strlen(argv[1]);
    if (hex_len % 2 != 0)
    {
        puts("[Error] sendhex: hex string must have even length (pairs of characters)");
        return -1;
    }

    size_t len = convert_hex((uint8_t *)msg_out.message, argv[1]);
    if (len == 0)
    {
        puts("[Error] sendhex: invalid or empty hex string");
        return -1;
    }

    printf("sendhex: sending %u bytes: ", (unsigned)len);
    for (size_t i = 0; i < len; i++)
    {
        printf("%02x", msg_out.message[i]);
    }

    msg_out.message_id = msg_count;
    msg_out.uid = my_uid;
    msg_out.group = current_group;
    msg_out.target_user = current_target;
    msg_count++;
    iolist_t iolist = {
        .iol_base = &msg_out,
        .iol_len = sizeof(chat_message_t)};
    netdev_t *netdev = &sx127x.netdev;

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP)
    {
        puts("Cannot send: radio is still transmitting");
    }

    puts("");
    return 0;
}

int init_sx1272_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    sx127x.params = sx127x_params[0];
    netdev_t *netdev = &sx127x.netdev;

    netdev->driver = &sx127x_driver;

    netdev->event_callback = _event_cb;

    //        printf("%8x\n", (unsigned int)netdev->driver);
    //        printf("%8x\n", (unsigned int)netdev->driver->init);

    if (netdev->driver->init(netdev) < 0)
    {
        puts("Failed to initialize SX127x device, exiting");
        return 1;
    }

    _recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
                              THREAD_CREATE_STACKTEST, _recv_thread, NULL,
                              "recv_thread");

    if (_recv_pid <= KERNEL_PID_UNDEF)
    {
        puts("Creation of receiver thread failed");
        return 1;
    }
    puts("5");

    return 0;
}

int userlist_cmd(int argc, char **argv)
{
    if (argc == 1 && strcmp(argv[0], "userlist") == 0)
    {
        puts("User list:");
        list_users();
        return 0;
    }
    return 1;
}
int group_cmd(int argc, char **argv)
{

    if (argc < 2)
    {
        puts("usage: group <get|set|join>");
        return -1;
    }

    if (strcmp(argv[1], "get")==0)
    {
        printf("group: %li\n", current_group);
        return 0;
    }

    if (strcmp(argv[1], "set") == 0)
    {
        if (argc < 3)
        {
            puts("usage: group set <group>");
            return -1;
        }
        else
        {
            uint32_t target_group = (uint32_t)atoi(argv[2]);

            for (int i = 0; i < group_number; i++)
            {
                printf("for loop du set: %i\n", i);
                
                if (my_groups[i] == target_group) 
                {
                    current_group = target_group;
                    printf("successfully transmitting to group %li \n", current_group);
                    return 0;
                }
            }
            
            printf("%li\n", target_group); 
            puts("you must join a group before transmitting to it (group join <group>)");
            return -1;
        }
    }
    if (strcmp(argv[1], "join")==0)
    {
        if (argc < 3)
        {
            puts("usage: group join <group>");
            return -1;
        }
        else
        {
            if (group_number < MAX_GROUP)
            {
                my_groups[group_number] = (uint32_t) atoi(argv[2]);
                group_number++;
            }
        }
    }
    else{if (strcmp(argv[1], "leave") == 0) {
    if (argc < 3) {
        puts("usage: group leave <group>");
        return -1;
    }
    uint32_t g = (uint32_t)atoi(argv[2]);
    for (int i = 0; i < group_number; i++) {
        if (my_groups[i] == g) {
            for (int j = i; j < group_number - 1; j++) {
                my_groups[j] = my_groups[j + 1];
            }
            group_number--;
            if (current_group == g) {
                current_group = 0;
                puts("Left current group, switched to broadcast");
            }
            printf("Left group %" PRIu32 "\n", g);
            return 0;
        }
    }
    puts("Not a member of that group");
    return -1;
}else{
    {
        puts("usage: group <get|set|join>");
        return -1;
    }

    return 0;}}
}
int target_cmd(int argc, char **argv)
{

    if (argc < 2)
    {
        puts("usage: target <get|set>");
        return -1;
    }

    if (strcmp(argv[1], "get")==0)
    {
        printf("target: %li\n", current_target);
        return 0;
    }

    if (strcmp(argv[1], "set")==0)
    {
        if (argc < 3)
        {
            puts("usage: target set <target>");
            return -1;
        }
        else
        {
                    current_target = (uint32_t)atoi(argv[2]);

                    printf("successfully transmitting to target %li \n", current_target);
                    return 0;
        }
    }
    else
    {
        puts("usage: target <get|set>");
        return -1;
    }

    return 0;
}
static const shell_command_t shell_commands[] = {
    {"init", "Initialize SX1272", init_sx1272_cmd},
    {"setup", "Initialize LoRa modulation settings", lora_setup_cmd},
    {"implicit", "Enable implicit header", implicit_cmd},
    {"crc", "Enable CRC", crc_cmd},
    {"payload", "Set payload length (implicit header)", payload_cmd},
    {"random", "Get random number from sx127x", random_cmd},
    {"syncword", "Get/Set the syncword", syncword_cmd},
    {"rx_timeout", "Set the RX timeout", rx_timeout_cmd},
    {"channel", "Get/Set channel frequency (in Hz)", channel_cmd},
    {"register", "Get/Set value(s) of registers of sx127x", register_cmd},
    {"send", "Send raw payload string", send_cmd},
    {"listen", "Start raw payload listener, hex if wanna listen in hexmode", listen_cmd},
    {"reset", "Reset the sx127x device", reset_cmd},
    {"sendhex", "send an hex payload", sendhex_cmd},
    {"userlist", "List all users", userlist_cmd},
    {"group", "Get/Set/Join/leave group", group_cmd},
    {"target", "Get/Set target", target_cmd},
    { "msglist", "List last received messages (FIFO)", msglist_cmd },
{ "uid",     "Show or set our own UID",            uid_cmd     },
    {NULL, NULL, NULL}
};

int main(void)
{

    // init_sx1272_cmd(0,NULL);

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
