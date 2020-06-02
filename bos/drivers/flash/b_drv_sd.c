/**
 *!
 * \file        b_drv_sd.c
 * \version     v0.0.1
 * \date        2020/06/01
 * \author      Bean(notrynohigh@outlook.com)
 *******************************************************************************
 * @attention
 * 
 * Copyright (c) 2020 Bean
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************
 */
   
/*Includes ----------------------------------------------*/
#include "b_drv_sd.h"
/** 
 * \addtogroup B_DRIVER
 * \{
 */

/** 
 * \addtogroup SD
 * \{
 */

/** 
 * \defgroup SD_Private_TypesDefinitions
 * \{
 */
 

/**
 * \}
 */
   
/** 
 * \defgroup SD_Private_Defines
 * \{
 */
#ifdef HAL_SD_CS_PORT
#define SD_CS_SET()     bHalGPIO_WritePin(HAL_SD_CS_PORT, HAL_SD_CS_PIN, 1)
#define SD_CS_RESET()   bHalGPIO_WritePin(HAL_SD_CS_PORT, HAL_SD_CS_PIN, 0)
#else
#define SD_CS_SET()
#define SD_CS_RESET()
#endif
/**
 * \}
 */
   
/** 
 * \defgroup SD_Private_Macros
 * \{
 */
   
/**
 * \}
 */
   
/** 
 * \defgroup SD_Private_Variables
 * \{
 */

bSD_Driver_t bSD_Driver;
/**
 * \}
 */
   
/** 
 * \defgroup SD_Private_FunctionPrototypes
 * \{
 */

/**
 * \}
 */
   
/** 
 * \defgroup SD_Private_Functions
 * \{
 */	

/************************************************************************************************************driver interface*******/
static int _bSD_WaitReady()
{
    uint32_t tick = bUtilGetTick();
    uint8_t tmp = 0;

    for(;;)
    {
        tmp = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        if(tmp == 0xff)
        {
            return 0;
        }
        if(bUtilGetTick() - tick > 500)
        {
            break;
        }
    }
    return -1;
}

static void _bSD_SendDump(uint8_t n)
{
    uint8_t tmp = 0xff;
    uint8_t i = 0;
    for(i = 0;i < n;i++)
    {
        bHalSPI_Send(HAL_SD_SPI, &tmp, 1);
    }
}

static int _bSD_PowerON()
{
    uint8_t tmp = 0xff;
    uint8_t cmd[6] = {CMD0, 0, 0, 0, 0, 0X95};
    uint32_t cnt;
    bUtilDelayMS(100);
    bHalSPI_SetSpeed(HAL_SD_SPI, 300000);
    SD_CS_SET();
    _bSD_SendDump(10);
    
    SD_CS_RESET();

    bHalSPI_Send(HAL_SD_SPI, cmd, 6);
    cnt = 0;
    for(;;)
    {
        tmp = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        if(tmp == 0x1 || cnt >= 0x1fff)
        {
            break;
        }
        cnt++;
    }
    SD_CS_SET();
    _bSD_SendDump(1);
    if(cnt >= 0x1fff)
    {
        return -1;
    }
    return 0;
}


static int _bSD_SendCmd(uint8_t cmd, uint32_t param, uint8_t crc)
{
    uint8_t cmd_table[6];
    uint8_t tmp, cnt;
    if(_bSD_WaitReady() < 0)
    {
        return -1;
    }
    cmd_table[0] = cmd;
    cmd_table[1] = (uint8_t)((param >> 24) & 0xff);
    cmd_table[2] = (uint8_t)((param >> 16) & 0xff);
    cmd_table[3] = (uint8_t)((param >> 8) & 0xff);
    cmd_table[4] = (uint8_t)((param >> 0) & 0xff);
    cmd_table[5] = crc;
    bHalSPI_Send(HAL_SD_SPI, cmd_table, 6);
    cnt = 0;
    for(;;)
    {
        tmp = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        if(tmp != 0xff || cnt > 200)
        {
            break;
        }
        cnt++;
    }
    if(cnt > 200)
    {
        b_log_e("cmd%d err..\r\n", cmd);
        return -1;
    }
    return tmp;
}


static int _bSD_Init()
{
    int retval = -1;
    uint32_t cnt = 0;
    uint8_t ocr[4];
    if(_bSD_PowerON() < 0)
    {
        b_log_e("power on err\r\n");
        return -1;
    }
    SD_CS_RESET();
    retval = _bSD_SendCmd(CMD0, 0, 0x95);
    if(retval < 0)
    {
        SD_CS_SET();
        return -1;
    }
      
    if(retval == 1)
    {
        retval = _bSD_SendCmd(CMD8, 0x1AA, 0x87);
        if(retval < 0)
        {
            SD_CS_SET();
            return -1;
        }
    }
    else
    {
        SD_CS_SET();
        return -1;
    }
    
    if(retval == 0x5)
    {
        bSD_Driver._private.v = CT_SD1;
        SD_CS_SET();
        _bSD_SendDump(1);
        cnt = 0;
        SD_CS_RESET();
        do
        {
            retval = _bSD_SendCmd(CMD55, 0, 0);
            if(retval < 0)
            {
                SD_CS_SET();
                return -1;
            }
            retval = _bSD_SendCmd(CMD41, 0, 0);
            if(retval < 0)
            {
                SD_CS_SET();
                return -1;
            }
            cnt++;
        }while((retval != 0) && (cnt < 400));
        if(cnt >= 400)
        {
            SD_CS_SET();
            return -1;
        }
        bHalSPI_SetSpeed(HAL_SD_SPI, 18000000);
        _bSD_SendDump(1);
        retval = _bSD_SendCmd(CMD59, 0, 0x95);
        if(retval < 0)
        {
            SD_CS_SET();
            return -1;
        }
        retval = _bSD_SendCmd(CMD16, 512, 0x95);
        if(retval < 0)
        {
            SD_CS_SET();
            return -1;
        }
    }
    else if(retval == 0x1)
    {
        ocr[0] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        ocr[1] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        ocr[2] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        ocr[3] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        SD_CS_SET();
        _bSD_SendDump(1);
        cnt = 0;
        SD_CS_RESET();
        do
        {
            retval = _bSD_SendCmd(CMD55, 0, 0);
            if(retval < 0)
            {
                SD_CS_SET();
                return -1;
            }
            retval = _bSD_SendCmd(CMD41, 0x40000000, 0);
            if(retval < 0)
            {
                SD_CS_SET();
                return -1;
            }
            cnt++;
        }while((retval != 0) && (cnt < 400));
        if(cnt >= 400)
        {
            SD_CS_SET();
             return -1;
        }
        retval = _bSD_SendCmd(CMD58, 0, 0);
        if(retval != 0)
        {
            SD_CS_SET();
            return -1;
        }
        ocr[0] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        ocr[1] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        ocr[2] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        ocr[3] = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
        if(ocr[0] & 0x40)
        {
            bSD_Driver._private.v = CT_SDHC;
        }
        else
        {
            bSD_Driver._private.v = CT_SD2;
        }
    }
    SD_CS_SET();
    _bSD_SendDump(1);
    bHalSPI_SetSpeed(HAL_SD_SPI, 18000000);
    b_log("sd type:%d\r\n", bSD_Driver._private.v);
    return 0;   
}


//static int _bSD_RxDataBlock(uint8_t *buff, uint16_t len)
//{
//	uint8_t token;
//    uint16_t cnt = 0;
//    SD_CS_RESET();
//	do 
//    {
//		token = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
//        cnt++;
//	} while((token == 0xFF) && cnt < 200);

//	if(token != 0xFE)
//    {
//        SD_CS_SET();
//        return -1;
//    }
//	do 
//    {
//		*buff = bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
//        buff++;
//	} while(len--);
//	bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
//	bHalSPI_SendReceiveByte(HAL_SD_SPI, 0xff);
//    SD_CS_SET();
//    _bSD_SendDump(1);
//	return len;
//}


//static int _bSD_TxDataBlock(const uint8_t *buff, uint8_t token)
//{
//	uint8_t resp;
//	uint8_t i = 0;

//	/* wait SD ready */
//	if (SD_ReadyWait() != 0xFF) return FALSE;

//	/* transmit token */
//	SPI_TxByte(token);

//	/* if it's not STOP token, transmit data */
//	if (token != 0xFD)
//	{
//		SPI_TxBuffer((uint8_t*)buff, 512);

//		/* discard CRC */
//		SPI_RxByte();
//		SPI_RxByte();

//		/* receive response */
//		while (i <= 64)
//		{
//			resp = SPI_RxByte();

//			/* transmit 0x05 accepted */
//			if ((resp & 0x1F) == 0x05) break;
//			i++;
//		}

//		/* recv buffer clear */
//		while (SPI_RxByte() == 0);
//	}

//	/* transmit 0x05 accepted */
//	if ((resp & 0x1F) == 0x05) return TRUE;

//	return FALSE;
//}




/**
 * \}
 */
   
/** 
 * \addtogroup SD_Exported_Functions
 * \{
 */
int bSD_Init()
{          
    if(_bSD_Init() < 0)
    {
        b_log("sd_err\r\n");
        return -1;
    }
    b_log("sd_ok\r\n");
    bSD_Driver.status = 0;
    bSD_Driver.close = NULL;
    bSD_Driver.read = NULL;
    bSD_Driver.ctl = NULL;
    bSD_Driver.open = NULL;
    bSD_Driver.write = NULL;
    return 0;
}


bDRIVER_REG_INIT(bSD_Init);




/**
 * \}
 */

/**
 * \}
 */

/**
 * \}
 */

/************************ Copyright (c) 2020 Bean *****END OF FILE****/
