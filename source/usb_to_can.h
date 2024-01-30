/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _USB_TO_CAN_H_
#define _USB_TO_CAN_H_

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define MAX_BUF_SIZE    (140)


#define USB_TO_CAN_IDLE     (0)
#define USB_TO_CAN_CMD      (1)
#define USB_TO_CAN_DATA     (2)


#define BELL 7
#define CR 13
#define LR 10

#define RX_STEP_TYPE 0
#define RX_STEP_ID_EXT 1
#define RX_STEP_ID_STD 6
#define RX_STEP_DLC 9
#define RX_STEP_DATA 10
#define RX_STEP_TIMESTAMP 26
#define RX_STEP_CR 30
#define RX_STEP_FINISHED 0xff

#define FD_RX_STEP_CANFD 0
#define FD_RX_STEP_TYPE 2
#define FD_RX_STEP_ID_EXT 3
#define FD_RX_STEP_ID_STD 8
#define FD_RX_STEP_DLC 11
#define FD_RX_STEP_DATA 12
#define FD_RX_STEP_TIMESTAMP 140
#define FD_RX_STEP_CR 144
#define FD_RX_STEP_FINISHED 0xff

/*******************************************************************************
 * Structures
 ******************************************************************************/
typedef struct
{
    uint32_t    (*uart_send)(uint8_t *buf, uint32_t len);
    uint32_t    (*can_send)(uint32_t id, uint8_t *buf, uint32_t len);
}usb_to_can_sendfunc_t;


/* can message data structure */
typedef struct
{
    unsigned long id; 			/* identifier (11 or 29 bit) */
    struct {
       unsigned char rtr : 1;		/* remote transmit request */
       unsigned char extended : 1;	/* extended identifier */
    } flags;

    unsigned char dlc;                  /* data length code */
    unsigned char data[8];		/* payload data */
    unsigned short timestamp;           /* timestamp */
} canmsg_t;

/* can message data structure */
typedef struct
{
    unsigned long id; 			/* identifier (11 or 29 bit) */
    struct {
       unsigned char rtr : 1;		/* remote transmit request */
       unsigned char extended : 1;	/* extended identifier */
    } flags;

    unsigned char dlc;                  /* data length code */
    unsigned char lenInBytes;           /* data length in bytes */
    unsigned char data[64];		/* payload data */
    unsigned short timestamp;           /* timestamp */
} canfdmsg_t;

/*******************************************************************************
 * Interfaces
 ******************************************************************************/

/*!
 * @brief USB to CAN State Machine.
 *
 * USB to CAN State Machine based on the received commands
 *
 * @return None.
 */
void usb_to_can_input(uint8_t ch);

/*!
 * @brief Initialize USB to CAN Interfaces.
 *
 * Initialize USB to CAN with send functions..
 *
 * @return None.
 */
void usb_to_can_init(const usb_to_can_sendfunc_t *sendfunc);

/*!
 * @brief USB to CAN, Can input.
 *
 * Process received CAN message and sent using UART
 *
 * @return None.
 */
void usb_to_can_interfacez(uint32_t id, uint8_t extId, uint8_t *buf,
                            uint8_t dlc);

/*!
 * @brief USB to CAN, Can FD input.
 *
 * Process received CAN message and sent using UART
 *
 * @return None.
 */
void usb_to_can_fd_interfacez(uint32_t id, uint8_t extId, uint8_t *buf,
                            uint8_t dlc, uint32_t lenInBytes);

#endif /*USB_TO_CAN*/
