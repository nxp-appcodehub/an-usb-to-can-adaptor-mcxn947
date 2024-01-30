/*
 * Copyright 2022, 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CAN_INTERFACE_H_
#define _CAN_INTERFACE_H_

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define USE_CANFD (1U)

/*******************************************************************************
 * API
 ******************************************************************************/

/*!
 * @brief  CAN Start after receiving parameters from GUI.
 *
 * This function initialize CAN module in the application.
 *
 * @return None.
 */
void APPCanStart(uint8_t *param);

/*!
 * @brief  CAN Initalization Application function.
 *
 * This function initialize CAN module in the application.
 *
 * @return None.
 */
void APPCanInit(void);

/*!
 * @brief  CAN Send Frame function.
 *
 * This function send a CAN frame.
 *
 * @return Transmision status.
 */
uint32_t APPCanSend(uint32_t id, uint8_t *buff, uint32_t len);

/*!
 * @brief FlexCAN  USBTin Call Back function.
 *
 * Callback USBTin function for the CAN interrupts.
 *
 * @return None.
 */
void APPUSBTinCanRxCallback(uint32_t id, uint8_t extId, uint8_t *buf, 
                            uint8_t dlc, uint32_t lenInBytes);

/*!
 * @brief Byte endian Swap.
 *
 * Function to change endian in a byte.
 *
 * @return Byte variable swapeded.
 */
static uint32_t endianSwap4bytes(uint32_t var);

/*!
 * @brief Endian Buffer swap .
 *
 * Function to change endian in a buffer.
 *
 * @return None.
 */
static void endianSwapBuffer(uint8_t * dBuff, uint8_t * sBuff, uint8_t length);

/*!
 * @brief Request CAN reception .
 *
 * Function to enable CAN reception after the a complete reception .
 *
 * @return None.
 */
void AppCANRequestRxTask(void);

/*!
 * @brief Get the byte size from the DLC .
 *
 * this function is used in CAN FD to get the byte size from the DLC
 *
 * @return Length in bytes.
 */
uint8_t CANFD_DLC_TO_BYTE(uint8_t dlc);



#endif /* _CAN_INTERFACE_H_ */
