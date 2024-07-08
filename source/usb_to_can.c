/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "usb_to_can.h"
#include "stdio.h"
#include "can_interface.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef struct
{
    const usb_to_can_sendfunc_t     *sendfunc;
    uint8_t                 		state;
    uint8_t                 		recv_buf[MAX_BUF_SIZE];
    uint8_t                 		send_buf[MAX_BUF_SIZE];
    uint8_t                 		send_buf_len;
    uint8_t                 		recv_buf_cnt;
}usb_to_can_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
static usb_to_can_t usb2can;
unsigned char time_stamping = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief Initialize USB to CAN Interfaces.
 *
 * Initialize USB to CAN with send functions..
 *
 * @return None.
 */
void usb_to_can_init(const usb_to_can_sendfunc_t *sendfunc)
{
	usb2can.state = USB_TO_CAN_IDLE;
	usb2can.sendfunc = sendfunc;
}

/*!
 * @brief Parse hex values.
 *
 * Parse Hex value.
 *
 * @return Status.
 */
unsigned char parse_hex(char * line, unsigned char len, unsigned long * value)
{
    *value = 0;
    while (len--) {
        if (*line == 0) return 0;
        *value <<= 4;
        if ((*line >= '0') && (*line <= '9')) {
           *value += *line - '0';
        } else if ((*line >= 'A') && (*line <= 'F')) {
           *value += *line - 'A' + 10;
        } else if ((*line >= 'a') && (*line <= 'f')) {
           *value += *line - 'a' + 10;
        } else return 0;
        line++;
    }
    return 1;
}

/*!
 * @brief Parse transmit command.
 *
 * Parse transmit command.
 *
 * @return None.
 */
unsigned char parse_cmd_transmit(uint8_t *line, uint8_t len)
{
#if (defined(USE_CANFD) && USE_CANFD)
	canfdmsg_t canmsg;
#else
	canmsg_t canmsg;
#endif

    unsigned long temp;
    unsigned char idlen;

 //   canmsg.flags.rtr = ((line[0] == 'r') || (line[0] == 'R'));

    /* upper case -> extended identifier */
    if ((line[0] == 'e') || (line[0] == 'E')) {
        canmsg.flags.extended = true;
        idlen = 8;
    } else {
        canmsg.flags.extended = false;
        idlen = 3;
    }

    if (!parse_hex((char*)&line[1], idlen, &temp)) return 0;
    canmsg.id = temp;

    if (!parse_hex((char*)&line[1 + idlen], 1, &temp)) return 0;
    canmsg.dlc = temp;

    if (!canmsg.flags.rtr) {
        unsigned int i;
        unsigned char length = CANFD_DLC_TO_BYTE(canmsg.dlc);
        for (i = 0; i < length; i++) {
            if (!parse_hex((char*)&line[idlen + 2 + i*2], 2, &temp)) return 0;
            canmsg.data[i] = temp;
        }
    }

    usb2can.sendfunc->can_send(canmsg.id, canmsg.flags.extended, canmsg.data, canmsg.dlc);

    return 1;
}

/*!
 * @brief Parse transmit command.
 *
 * Parse transmit command.
 *
 * @return None.
 */
char canmsg_to_ascii_getnextchar(canmsg_t * canmsg, unsigned char * step) {

    char ch = BELL;
    char newstep = *step;

    if (*step == RX_STEP_TYPE) {

            /* type */

            if (canmsg->flags.extended) {
                newstep = RX_STEP_ID_EXT;
                ch = 'e';
            } else {
                newstep = RX_STEP_ID_STD;
                ch = 's';
            }

    } else if (*step < RX_STEP_DLC) {

	    /* id */

        unsigned char i = *step - 1;
        unsigned char * id_bp = (unsigned char*) &canmsg->id;
        ch = id_bp[3 - (i / 2)];
        if ((i % 2) == 0) ch = ch >> 4;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        newstep++;

    } else if (*step < RX_STEP_DATA) {

	    /* length */

        ch = canmsg->dlc;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        if ((canmsg->dlc == 0) || canmsg->flags.rtr) newstep = RX_STEP_TIMESTAMP;
        else newstep++;

    } else if (*step < RX_STEP_TIMESTAMP) {

        /* data */

        unsigned char i = *step - RX_STEP_DATA;
        ch = canmsg->data[i / 2];
        if ((i % 2) == 0) ch = ch >> 4;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        newstep++;
        if (newstep - RX_STEP_DATA == canmsg->dlc*2) newstep = RX_STEP_TIMESTAMP;

    } else if (time_stamping && (*step < RX_STEP_CR)) {

        /* time stamp */

        unsigned char i = *step - RX_STEP_TIMESTAMP;
        if (i < 2) ch = (canmsg->timestamp >> 8) & 0xff;
        else ch = canmsg->timestamp & 0xff;
        if ((i % 2) == 0) ch = ch >> 4;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        newstep++;

    } else {

        /* line break */

        ch = CR;
        newstep = RX_STEP_FINISHED;
    }

    *step = newstep;
    return ch;
}

/*!
 * @brief Parse transmit command.
 *
 * Parse transmit command.
 *
 * @return None.
 */
char canfdmsg_to_ascii_getnextchar(canfdmsg_t * canfdmsg, unsigned char * step) {

    char ch = BELL;
    char newstep = *step;

    if (*step < FD_RX_STEP_TYPE){
        /*CAN FD identifier */
        if(*step == 0) ch='F';
        if(*step == 1) ch='D';

        newstep++;

    }else if(*step == FD_RX_STEP_TYPE) {

            /* type */
            if (canfdmsg->flags.extended) {
                newstep = FD_RX_STEP_ID_EXT;
                ch = 'e';
            } else {
                newstep = FD_RX_STEP_ID_STD;
                ch = 's';
            }

    }else if (*step < FD_RX_STEP_DLC) {

	    /* id */
        unsigned char i = *step - FD_RX_STEP_ID_EXT;
        unsigned char * id_bp = (unsigned char*) &canfdmsg->id;
        ch = id_bp[3 - (i / 2)];
        if ((i % 2) == 0) ch = ch >> 4;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        newstep++;

    } else if (*step < FD_RX_STEP_DATA) {

	    /* length */
        ch = canfdmsg->dlc;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        if ((canfdmsg->dlc == 0) || canfdmsg->flags.rtr) newstep = FD_RX_STEP_TIMESTAMP;
        else newstep++;

    } else if (*step < FD_RX_STEP_TIMESTAMP) {

        /* data */
        unsigned char i = *step - FD_RX_STEP_DATA;
        ch = canfdmsg->data[i / 2];
        if ((i % 2) == 0) ch = ch >> 4;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        newstep++;
        if (newstep - FD_RX_STEP_DATA == canfdmsg->lenInBytes*2) newstep = FD_RX_STEP_TIMESTAMP;

    } else if (time_stamping && (*step < FD_RX_STEP_CR)) {

        /* time stamp */
        unsigned char i = *step - FD_RX_STEP_TIMESTAMP;
        if (i < 2) ch = (canfdmsg->timestamp >> 8) & 0xff;
        else ch = canfdmsg->timestamp & 0xff;
        if ((i % 2) == 0) ch = ch >> 4;

        ch = ch & 0xF;
        if (ch > 9) ch = ch - 10 + 'A';
        else ch = ch + '0';

        newstep++;

    } else {

        /* line break */
        ch = CR;
        newstep = FD_RX_STEP_FINISHED;
    }

    *step = newstep;
    return ch;
}



/*!
 * @brief USB to CAN State Machine.
 *
 * USB to CAN State Machine based on the received commands
 *
 * @return None.
 */
void usb_to_can_input(uint8_t ch)
{
    switch(usb2can.state)
    {
        case USB_TO_CAN_IDLE:
            usb2can.recv_buf_cnt = 0;
            if(ch == 's' || ch == 'S' )
            {
                usb2can.state = USB_TO_CAN_DATA;
                usb2can.recv_buf[usb2can.recv_buf_cnt++] = ch;
                usb2can.send_buf_len = 2;
                usb2can.send_buf[0] = 'z';
                usb2can.send_buf[1] = 0x0D;
            }else if(ch == 'e' || ch == 'E' )
			{
                usb2can.state = USB_TO_CAN_DATA;
                usb2can.recv_buf[usb2can.recv_buf_cnt++] = ch;
                usb2can.send_buf_len = 2;
                usb2can.send_buf[0] = 'Z';
                usb2can.send_buf[1] = 0x0D;
			}
            break;
        case USB_TO_CAN_DATA:
            usb2can.recv_buf[usb2can.recv_buf_cnt++] = ch;
            if(ch == 0x0D || ch == 0x0A)
            {
                usb2can.state = USB_TO_CAN_IDLE;
                parse_cmd_transmit(usb2can.recv_buf, usb2can.recv_buf_cnt);
            }
            break;
        case USB_TO_CAN_CMD:
            if(ch == 0x0D || ch == 0x0A)
            {
                usb2can.state = USB_TO_CAN_IDLE;
            }
            break;

        default:
            printf("unknown");
            break;
    }
}

/*!
 * @brief convert CAN FD Frame into Serial
 *
 * convert CAN FD Frame into Serial
 *
 * @return None.
 */
void can_to_cdc_interfacez(uint32_t id, uint8_t extId, uint8_t *buf, uint8_t dlc)
{
    int i, cdc_len;
    uint8_t rxstep = 0;
    uint8_t cdc_buf[64];
    canmsg_t msg;

    msg.dlc = dlc;
    msg.id = id;
    msg.flags.extended = extId;
    msg.flags.rtr = 0;
    msg.timestamp = 0;
    for(i=0; i<dlc; i++)
    {
        msg.data[i] = buf[i];
    }

    cdc_len = 0;
    // printf("usart rx:%c\r\n", data);
    while(rxstep != RX_STEP_FINISHED)
    {
        cdc_buf[cdc_len++] = canmsg_to_ascii_getnextchar(&msg, &rxstep);
    }
    rxstep = 0;

    usb2can.sendfunc->uart_send(cdc_buf, cdc_len);
}

/*!
 * @brief convert CAN FD Frame into Serial
 *
 * Process received CAN message and sent using UART
 *
 * @return None.
 */
void can_fd_to_cdc_interfacez(uint32_t id, uint8_t extId, uint8_t *buf,
                            uint8_t dlc, uint32_t lenInBytes)
{
    int i, cdc_len;
    uint8_t rxstep = 0;
    uint8_t cdc_buf[150];
    canfdmsg_t msg;

    msg.dlc = dlc;
    msg.lenInBytes = lenInBytes;
    msg.id = id;
    msg.flags.extended = extId;
    msg.flags.rtr = 0;
    msg.timestamp = 0;
    for(i=0; i<lenInBytes; i++)
    {
        msg.data[i] = buf[i];
    }

    cdc_len = 0;
    while(rxstep != FD_RX_STEP_FINISHED)
    {
        cdc_buf[cdc_len++] = canfdmsg_to_ascii_getnextchar(&msg, &rxstep);
    }
    rxstep = 0;

    usb2can.sendfunc->uart_send(cdc_buf, cdc_len);
}




