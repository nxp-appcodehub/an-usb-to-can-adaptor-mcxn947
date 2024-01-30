/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017, 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "can_interface.h"
#include "usb_to_can.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define EXAMPLE_CAN                CAN0
#define EXAMPLE_FLEXCAN_IRQn       CAN0_IRQn
#define EXAMPLE_FLEXCAN_IRQHandler CAN0_IRQHandler
#define RX_MESSAGE_BUFFER_NUM      (0)
#define TX_MESSAGE_BUFFER_NUM      (1)
#define RX_IDENTIFIER              (0x000)

/* Get frequency of flexcan clock */
#define EXAMPLE_CAN_CLK_FREQ CLOCK_GetFlexcanClkFreq(0U)
/* Set USE_IMPROVED_TIMING_CONFIG macro to use api to calculates the improved CAN / CAN FD timing values. */
#define USE_IMPROVED_TIMING_CONFIG (1U)
/* Fix MISRA_C-2012 Rule 17.7. */
#define LOG_INFO (void)PRINTF

#if (defined(USE_CANFD) && USE_CANFD)
/*
 *    DWORD_IN_MB    DLC    BYTES_IN_MB             Maximum MBs
 *    2              8      kFLEXCAN_8BperMB        64
 *    4              10     kFLEXCAN_16BperMB       42
 *    8              13     kFLEXCAN_32BperMB       25
 *    16             15     kFLEXCAN_64BperMB       14
 *
 * Dword in each message buffer, Length of data in bytes, Payload size must align,
 * and the Message Buffers are limited corresponding to each payload configuration:
 */
#define DWORD_IN_MB (16)
#define DLC         (15)
#define BYTES_IN_MB kFLEXCAN_64BperMB
#else
#define DLC (8)
#endif


/*******************************************************************************
 * Variables
 ******************************************************************************/

flexcan_handle_t flexcanHandle;
volatile bool txComplete = false;
volatile bool rxComplete = false;
flexcan_mb_transfer_t txXfer; 
flexcan_mb_transfer_t rxXfer;
#if (defined(USE_CANFD) && USE_CANFD)
flexcan_fd_frame_t txFrame, rxFrame;
#else
flexcan_frame_t txFrame, rxFrame;
#endif

#if (defined(FSL_FEATURE_FLEXCAN_HAS_EXTENDED_FLAG_REGISTER)) && (FSL_FEATURE_FLEXCAN_HAS_EXTENDED_FLAG_REGISTER > 0)
    uint64_t flag = 1U;
#else
    uint32_t flag = 1U;
#endif
uint8_t swapRxData[64]; /*Create buffer with the maximun expected size*/


/*******************************************************************************
 * Code
 ******************************************************************************/


uint8_t CANFD_DLC_TO_BYTE(uint8_t dlc)
{
  uint8_t byteSize;
  
  switch(dlc)
  {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
      byteSize = dlc;
      break;
    case 9:
    case 10:
    case 11:
    case 12:
      byteSize = ((dlc-8)*4)+8;
      break;
    case 13:
      byteSize = 32;
      break;
    case 14:
      byteSize = 48;
      break;
    case 15:
      byteSize = 64;
      break;      
  }
  
  return(byteSize);
}


/*!
 * @brief FlexCAN Call Back function.
 *
 * Callback function for the CAN interrupts.
 *
 * @return None.
 */
static FLEXCAN_CALLBACK(flexcan_callback)
{
    switch (status)
    {
        case kStatus_FLEXCAN_RxIdle:
            if (RX_MESSAGE_BUFFER_NUM == result)
            {
                /* Set Complete Flag */
                rxComplete = true;
            }
            break;

        case kStatus_FLEXCAN_TxIdle:
            if (TX_MESSAGE_BUFFER_NUM == result)
            {
               /* Set Complete Flag */
               txComplete = true;
            }
            break;

        default:
            break;
    }
}

/*!
 * @brief  CAN Start after receiving parameters from GUI.
 *
 * This function initialize CAN module in the application.
 *
 * @return None.
 */
void APPCanStart(uint8_t *param)
{
	flexcan_timing_config_t newTimingConfig;
	uint32_t fdBitrate = 2000000U;
	uint32_t bitrate = 1000000U;

	char fdBitFrame;

	if(param[9] == '0')
	{
		fdBitFrame = param[12];
	}
	else
	{
		fdBitFrame = param[11];
	}

	switch(param[3])
	{
		case '1':
			if(param[9] == '0')
			{
				bitrate = 1000000U;
			}
			else
			{
				bitrate = 100000U;
			}
			break;
		case '2':
			bitrate = 250000U;
			break;
		case '5':
			bitrate = 500000U;
			break;
		case '8':
			bitrate = 800000U;
			break;
		default:
			bitrate = 1000000U;
			break;
	}

	switch(fdBitFrame)
	{
		case '1':
			fdBitrate = 1000000U;
			break;
		case '2':
			fdBitrate = 2000000U;
			break;
		case '4':
			fdBitrate = 4000000U;
			break;
		case '5':
			fdBitrate = 5000000U;
			break;
	}

#if (defined(USE_CANFD) && USE_CANFD)
    if (FLEXCAN_FDCalculateImprovedTimingValues(EXAMPLE_CAN, bitrate, fdBitrate,
                                                EXAMPLE_CAN_CLK_FREQ, &newTimingConfig))
    {
        /* Update the improved timing configuration*/
        FLEXCAN_SetFDTimingConfig(EXAMPLE_CAN, &newTimingConfig);
    }
    else
    {
        LOG_INFO("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
#else
    if (FLEXCAN_CalculateImprovedTimingValues(EXAMPLE_CAN, bitrate, EXAMPLE_CAN_CLK_FREQ, &newTimingConfig))
    {
        /* Update the improved timing configuration*/
    	FLEXCAN_SetTimingConfig(EXAMPLE_CAN, &newTimingConfig);
    }
    else
    {
        LOG_INFO("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
#endif

    /* Start receive data */
#if (defined(USE_CANFD) && USE_CANFD)
    rxXfer.framefd = &rxFrame;
    (void)FLEXCAN_TransferFDReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &rxXfer);
#else
     rxXfer.frame = &rxFrame;
     (void)FLEXCAN_TransferReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &rxXfer);
#endif
     rxComplete = false;

}

/*!
 * @brief  CAN Initialization Application function.
 *
 * This function initialize CAN module in the application.
 *
 * @return None.
 */
void APPCanInit(void)
{
    flexcan_config_t flexcanConfig;
    flexcan_rx_mb_config_t mbConfig;

  
      /* attach FRO HF to FLEXCAN0 */
    CLOCK_SetClkDiv(kCLOCK_DivFlexcan0Clk, 1u);
    CLOCK_AttachClk(kFRO_HF_to_FLEXCAN0);
    
    LOG_INFO("\r\n== USB Adapter Example -- Start.==\r\n\r\n");

    /* Get FlexCAN module default Configuration. */
    /*
     * flexcanConfig.clkSrc                 = kFLEXCAN_ClkSrc0;
     * flexcanConfig.bitRate                = 1000000U;
     * flexcanConfig.bitRateFD              = 2000000U;
     * flexcanConfig.maxMbNum               = 16;
     * flexcanConfig.enableLoopBack         = false;
     * flexcanConfig.enableSelfWakeup       = false;
     * flexcanConfig.enableIndividMask      = false;
     * flexcanConfig.disableSelfReception   = false;
     * flexcanConfig.enableListenOnlyMode   = false;
     * flexcanConfig.enableDoze             = false;
     */
    FLEXCAN_GetDefaultConfig(&flexcanConfig);
#if defined(EXAMPLE_CAN_CLK_SOURCE)
    flexcanConfig.clkSrc = EXAMPLE_CAN_CLK_SOURCE;
#endif

/* If special quantum setting is needed, set the timing parameters. */
#if (defined(SET_CAN_QUANTUM) && SET_CAN_QUANTUM)
    flexcanConfig.timingConfig.phaseSeg1 = PSEG1;
    flexcanConfig.timingConfig.phaseSeg2 = PSEG2;
    flexcanConfig.timingConfig.propSeg   = PROPSEG;
#if (defined(FSL_FEATURE_FLEXCAN_HAS_FLEXIBLE_DATA_RATE) && FSL_FEATURE_FLEXCAN_HAS_FLEXIBLE_DATA_RATE)
    flexcanConfig.timingConfig.fphaseSeg1 = FPSEG1;
    flexcanConfig.timingConfig.fphaseSeg2 = FPSEG2;
    flexcanConfig.timingConfig.fpropSeg   = FPROPSEG;
#endif
#endif    
    
    
#if (defined(USE_IMPROVED_TIMING_CONFIG) && USE_IMPROVED_TIMING_CONFIG)
    flexcan_timing_config_t timing_config;
    memset(&timing_config, 0, sizeof(flexcan_timing_config_t));
#if (defined(USE_CANFD) && USE_CANFD)
    if (FLEXCAN_FDCalculateImprovedTimingValues(EXAMPLE_CAN, flexcanConfig.bitRate, flexcanConfig.bitRateFD,
                                                EXAMPLE_CAN_CLK_FREQ, &timing_config))
    {
        /* Update the improved timing configuration*/
        memcpy(&(flexcanConfig.timingConfig), &timing_config, sizeof(flexcan_timing_config_t));
    }
    else
    {
        LOG_INFO("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
#else
    if (FLEXCAN_CalculateImprovedTimingValues(EXAMPLE_CAN, flexcanConfig.bitRate, EXAMPLE_CAN_CLK_FREQ, &timing_config))
    {
        /* Update the improved timing configuration*/
        memcpy(&(flexcanConfig.timingConfig), &timing_config, sizeof(flexcan_timing_config_t));
    }
    else
    {
        LOG_INFO("No found Improved Timing Configuration. Just used default configuration\r\n\r\n");
    }
#endif
#endif

#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_FDInit(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ, BYTES_IN_MB, true);
#else
    FLEXCAN_Init(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ);
#endif

    /* Create FlexCAN handle structure and set call back function. */
    FLEXCAN_TransferCreateHandle(EXAMPLE_CAN, &flexcanHandle, flexcan_callback, NULL);

    /* Set Rx Masking mechanism. */
    FLEXCAN_SetRxMbGlobalMask(EXAMPLE_CAN, FLEXCAN_RX_MB_STD_MASK(RX_IDENTIFIER, 0, 0));
           
    /* Setup Rx Message Buffer. */
    mbConfig.format = kFLEXCAN_FrameFormatStandard;
    mbConfig.type   = kFLEXCAN_FrameTypeData;
    mbConfig.id     = FLEXCAN_ID_STD(RX_IDENTIFIER);
#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_SetFDRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
#else
    FLEXCAN_SetRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
#endif

/* Setup Tx Message Buffer. */
#if (defined(USE_CANFD) && USE_CANFD)
    FLEXCAN_SetFDTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);
#else
    FLEXCAN_SetTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);
#endif
    
}

/*!
 * @brief  CAN Send Frame function.
 *
 * This function send a CAN frame.
 *
 * @return Transmission status.
 */
uint32_t APPCanSend(uint32_t id, uint8_t *buff, uint32_t len)
{
    uint32_t txStatus;
    uint8_t i;

    /* Prepare Tx Frame for sending. */
    txFrame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
    txFrame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
    txFrame.id     = FLEXCAN_ID_STD(id);
    txFrame.length = (uint8_t)len;
#if (defined(USE_CANFD) && USE_CANFD)
    txFrame.brs = 1U;
#endif
    
#if(defined(USE_CANFD) && USE_CANFD)
    for (i = 0; i < DWORD_IN_MB; i++)
    {
      txFrame.dataWord[i] = endianSwap4bytes((uint32_t)(*((uint32_t *)buff + i)));
    }
#else
    /* Copy the message doing byte swap */
    txFrame.dataWord0 = endianSwap4bytes((uint32_t)(*((uint32_t *)buff)));
    txFrame.dataWord1 = endianSwap4bytes((uint32_t)(*((uint32_t *)(buff + 4))));
#endif
    
    LOG_INFO("Send message from MB%d to MB%d\r\n", TX_MESSAGE_BUFFER_NUM, RX_MESSAGE_BUFFER_NUM);


#if (defined(USE_CANFD) && USE_CANFD)
    for (i = 0; i < DWORD_IN_MB; i++)
    {
        LOG_INFO("tx word%d = 0x%x\r\n", i, txFrame.dataWord[i]);
    }
#else
    LOG_INFO("tx ID = 0x%x\r\n", id);
    LOG_INFO("tx word0 = 0x%x\r\n", txFrame.dataWord0);
    LOG_INFO("tx word1 = 0x%x\r\n", txFrame.dataWord1);
#endif
    /* Start frame transmission */
    txXfer.mbIdx = (uint8_t)TX_MESSAGE_BUFFER_NUM;
#if (defined(USE_CANFD) && USE_CANFD)
    txXfer.framefd = &txFrame;
    txStatus = (uint32_t)FLEXCAN_TransferFDSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
#else
    txXfer.frame = &txFrame;
    txStatus = (uint32_t)FLEXCAN_TransferSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
#endif
   
    return txStatus;
}

/*!
 * @brief Byte endian Swap.
 *
 * Function to change endian in a byte.
 *
 * @return Byte variable swap.
 */
static uint32_t endianSwap4bytes(uint32_t var)
{

  uint8_t *bytePtr = (uint8_t*)&var;
  uint32_t swappedVar = (uint32_t)((bytePtr[0] << 24) | 
                                   (bytePtr[1] << 16) | 
                                   (bytePtr[2] << 8) | 
                                   (bytePtr[3]));
  
  return (swappedVar);
}

/*!
 * @brief Endian Buffer swap .
 *
 * Function to change endian in a buffer.
 *
 * @return None.
 */
static void endianSwapBuffer(uint8_t * dBuff, uint8_t * sBuff, uint8_t length)
{
  uint8_t ctr = 0;
  uint8_t byteCtr = 0;
  length = length / 4;
  
  for(ctr = 0; ctr < length; ctr++)
  {
    dBuff[0 + byteCtr] = sBuff[3 + byteCtr];
    dBuff[1 + byteCtr] = sBuff[2 + byteCtr];
    dBuff[2 + byteCtr] = sBuff[1 + byteCtr];
    dBuff[3 + byteCtr] = sBuff[0 + byteCtr]; 
    byteCtr = byteCtr + 4;
  }
  
}

/*!
 * @brief FlexCAN  USBTin Call Back function.
 *
 * Callback USBTin function for the CAN interrupts.
 *
 * @return None.
 */
void APPUSBTinCanRxCallback(uint32_t id, uint8_t extId, uint8_t *buf, 
                            uint8_t dlc, uint32_t lenInBytes)
{
#if (defined(USE_CANFD) && USE_CANFD)
	usb_to_can_fd_interfacez(id, extId, buf, dlc, lenInBytes);
#else
  usb_to_can_interfacez(id, extId, buf, dlc);
#endif
  
  
}

void AppCANRequestRxTask(void)
{

	if(rxComplete == true)
	{
	    rxComplete = false;

	    /* Since the adapter does not have an active CAN ID any transmission will also be detected as a reception
	     * the next condition will help to filter messages sent by it self and do not reflect it on the GUI. */
	    if(txFrame.id != rxFrame.id)
	    {

			PRINTF("Reception complete!\r\n\r\n");
#if (defined(USE_CANFD) && USE_CANFD)
			/* Copy the received message doing a 8byte swap to fix endiness */
			endianSwapBuffer((uint8_t *)swapRxData, (uint8_t *)&(rxFrame.dataWord[0]), CANFD_DLC_TO_BYTE(rxFrame.length));
#else
			/* Copy the received message doing a 8byte swap to fix endiness */
			endianSwapBuffer((uint8_t *)swapRxData, (uint8_t *)&(rxFrame.dataWord0), rxFrame.length);
#endif

			/* Call USBTin function to process the message  */
			APPUSBTinCanRxCallback((rxFrame.id >> CAN_ID_STD_SHIFT), rxFrame.format, (uint8_t *)swapRxData, rxFrame.length, CANFD_DLC_TO_BYTE(rxFrame.length));
	    }

		/* Start receive data through Rx Message Buffer. */
		rxXfer.mbIdx = (uint8_t)RX_MESSAGE_BUFFER_NUM;
#if (defined(USE_CANFD) && USE_CANFD)
		rxXfer.framefd = &rxFrame;
		(void)FLEXCAN_TransferFDReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &rxXfer);
#else
		rxXfer.frame = &rxFrame;
		(void)FLEXCAN_TransferReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &rxXfer);
#endif

		txFrame.id = 0x000;
	}
}




