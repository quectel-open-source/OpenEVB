/*
Copyright (c) 2025, Quectel Wireless Solutions Co., Ltd.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************
  Name: example_lcx9h_spi_nmea.c
  Current support: LC29HAA, LC29HAI, LC29HBA, LC29HCA, LC29HDA
  History:
    Version  Date         Author   Description
    v1.0     2024-0928    Hayden   Create file
*/

#include "example_def.h"
#ifdef __EXAMPLE_LCx9H_SPI_NMEA__
#include "gd32f4xx_usart.h"
#include "ql_spi.h"
#include "ql_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ql_delay.h"
#include "stdbool.h"
#include "ql_log.h"
#define LOG_TAG "spi_nmea"
#define LOG_LVL QL_LOG_DEBUG
#define LOG_UART_NUM (USART5)

#define LCx9H_SPIS_CFG_RD_CMD         		0x0a
#define LCx9H_SPIS_RD_CMD             		0x81
#define LCx9H_SPIS_CFG_WR_CMD         		0x0c
#define LCx9H_SPIS_WR_CMD             		0x0e
#define LCx9H_SPIS_RS_CMD             		0x06
#define LCx9H_SPIS_WS_CMD             		0x08
#define LCx9H_SPIS_PWON_CMD           		0x04
#define LCx9H_SPIS_PWOFF_CMD          		0x02
#define LCx9H_SPIS_CT_CMD             		0x10

#define LCx9H_SPIS_TX_LEN_REG				0x08
#define LCx9H_SPIS_TX_BUF_REG				0x2000
#define LCx9H_SPIS_RX_LEN_REG				0x04
#define LCx9H_SPIS_RX_BUF_REG				0x1000

#define LCx9H_SPIS_STA_ON_OFFSET            (0)
#define LCx9H_SPIS_STA_ON_MASK              (0x1<<LCx9H_SPIS_STA_ON_OFFSET) //0x1

#define LCx9H_SPIS_STA_FIFO_RDY_OFFSET      (2)
#define LCx9H_SPIS_STA_FIFO_RDY_MASK        (0x1<<LCx9H_SPIS_STA_FIFO_RDY_OFFSET)//0x4

#define LCx9H_SPIS_STA_RD_ERR_OFFSET        (3)
#define LCx9H_SPIS_STA_RD_ERR_MASK          (0x1<<LCx9H_SPIS_STA_RD_ERR_OFFSET)

#define LCx9H_SPIS_STA_WR_ERR_OFFSET        (4)
#define LCx9H_SPIS_STA_WR_ERR_MASK          (0x1<<LCx9H_SPIS_STA_WR_ERR_OFFSET)

#define LCx9H_SPIS_STA_RDWR_FINISH_OFFSET   (5)
#define LCx9H_SPIS_STA_RDWR_FINISH_MASK     (0x1<<LCx9H_SPIS_STA_RDWR_FINISH_OFFSET) //0x5c 01011100

#define MAX_ERROR_NUMBER                    100
#define FAIL_MAX_TIME                       5

#define LCx9H_SPIS_TX_SIZE                  0x0800
#define LCx9H_SPIS_RX_SIZE                  0x0800

typedef enum 
{
    HOST_MUX_STATUS_OK,                          /**<  status ok*/
    HOST_MUX_STATUS_ERROR,                       /**<  status error*/
    HOST_MUX_STATUS_ERROR_PARAMETER,             /**<  status error parameter*/
    HOST_MUX_STATUS_ERROR_INIT,                  /**<  status initialization*/
    HOST_MUX_STATUS_ERROR_NOT_INIT,              /**<  status uninitialized*/
    HOST_MUX_STATUS_ERROR_INITIATED,             /**<  status of have initialized*/
}LCx9H_SPIS_STATUS;

void write_cmd(uint8_t cmd);
#define Power_on()  write_cmd(LCx9H_SPIS_PWON_CMD)
#define Power_off() write_cmd(LCx9H_SPIS_PWOFF_CMD)
typedef struct SPI_Receive_Send_struct
{
	uint16_t receive_length;
	uint16_t send_length;
	uint8_t* send_data;
	uint8_t* receive_buffer;
}LCx9H_SPI_Receive_Send;

SemaphoreHandle_t xSPIMutex;
int32_t Ql_LCx9H_PortSPIRead(uint8_t *buffer, uint32_t* size,int32_t timeout);
int32_t Ql_LCx9H_PortSPIWrite(uint8_t *buffer, uint32_t size,int32_t timeout);
/**
 * @brief Initialize the SPI interface for LCx9H.
 *
 * This function initializes the SPI interface by calling Ql_SPI_Init with specific parameters 
 * for the LCx9H device. It configures the SPI mode, transmit buffer size, and receive buffer size.
 *
 * @return SPI_Status_TypeDef - The status of the SPI initialization.
 *         Returns a value indicating whether the initialization was successful or not.
 */
static SPI_Status_TypeDef LCx9H_SPI_Init(void)
{
	SPI_Status_TypeDef spi_status;
	spi_status = Ql_SPI_Init(SPI_MODE_SW_LCx9H,LCx9H_SPIS_TX_SIZE,LCx9H_SPIS_RX_SIZE);
	return spi_status;
}
/**
 * @brief Reinitialize the SPI interface for LCx9H device.
 *
 * This function reinitializes the SPI interface to the LCx9H device. 
 * It first deinitializes the current SPI settings using Ql_SPI_DeInit(),
 * then initializes the SPI interface again with LCx9H_SPI_Init().
 *
 * @return SPI_Status_TypeDef The status of the SPI initialization process.
 */
static SPI_Status_TypeDef LCx9H_SPI_ReInit(void)
{
	Ql_SPI_DeInit();
	return LCx9H_SPI_Init();
}
/**
 * Task function to read data via SPI interface.
 * 
 * This function continuously checks the status of SPI read/write operations and reads data from the SPI port when appropriate.
 * It handles SPI read errors by reinitializing the SPI interface and logs any errors encountered.
 * The received data is printed, and the buffer is cleared after each successful read.
 * 
 * @param Param Task parameter (not used in this function)
 */
void LCx9H_SPI_Read_Task(void *Param)
{
	static uint8_t rx_buf[LCx9H_SPIS_TX_SIZE] = {0};
	int32_t Length = 0;
	uint8_t status = 0;
	while (1)
    {

		if (xSemaphoreTake(xSPIMutex, portMAX_DELAY) == pdTRUE)
        {
			status = Ql_LCx9H_PortSPIRead(rx_buf, &Length, 100);
			if(status != HOST_MUX_STATUS_OK)
			{
				QL_LOG_E("read spi error");
				LCx9H_SPI_ReInit();
				xSemaphoreGive(xSPIMutex);
                vTaskDelay(pdMS_TO_TICKS(10));
				continue;
			}
			//QL_LOG_I("read data from spi, len: %d", Length);
			if (Length <= 0)
			{
				xSemaphoreGive(xSPIMutex);
                vTaskDelay(pdMS_TO_TICKS(10));
				continue;
			}
			Ql_Printf("%s",rx_buf);
			memset(rx_buf,0,LCx9H_SPIS_TX_SIZE);
			xSemaphoreGive(xSPIMutex);
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		// if((LCx9H_RDWR_Switch & 0x01) != 0)//recv data need to send by spi
		// {
		// 	if((LCx9H_RDWR_Switch & 0x02) == 0)
		// 	{
		// 		LCx9H_RDWR_Switch |= 0x01 << 1;
		// 	}
		// 	continue;//suspend task until write spi complete
		// }
		// else
		// {
		// 	LCx9H_RDWR_Switch = 0;
		// }
        
    }
}
/**
 * @brief SPI Write Task function
 * 
 * This function is responsible for reading data from UART and writing it to the SPI interface.
 * It continuously checks for incoming UART data, processes it, and writes it to the SPI bus.
 * If an error occurs during SPI writing, it reinitializes the SPI interface and continues.
 *
 * @param Param Pointer to task parameters (not used in this function)
 */
void LCx9H_SPI_Write_Task(void *Param)
{
	uint8_t send_buf[LCx9H_SPIS_TX_SIZE] = {0};
	uint16_t send_len = 0;
	uint8_t status = 0;
	while (1)
	{
		send_len = Ql_Uart_Read(LOG_UART_NUM,send_buf,LCx9H_SPIS_TX_SIZE,500);
		//send_len = LCx9H_Uart_Read(send_buf);
		if(send_len > 0)
		{
			//QL_LOG_I("recv %d bytes:\r\n%s",send_len,send_buf);
			if (xSemaphoreTake(xSPIMutex, portMAX_DELAY) == pdTRUE)
        	{
				status = Ql_LCx9H_PortSPIWrite(send_buf,send_len,100);
				if(status != HOST_MUX_STATUS_OK)
				{
					QL_LOG_E("write spi error");
					LCx9H_SPI_ReInit();
					xSemaphoreGive(xSPIMutex);
                    vTaskDelay(pdMS_TO_TICKS(10));
					continue;
				}

				memset(send_buf,0,LCx9H_SPIS_TX_SIZE);
				xSemaphoreGive(xSPIMutex);
				vTaskDelay(pdMS_TO_TICKS(10));
			}
			// LCx9H_RDWR_Switch = 1;
			// //QL_LOG_I("recv %d bytes:\r\n%s",send_len,send_buf);
			// while (LCx9H_RDWR_Switch != 0x03)//wait read spi complete
			// {
			// 	;
			// }
			//LCx9H_RDWR_Switch = 0;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
/**
 * @brief Example task function to initialize and create SPI read/write tasks.
 * 
 * @param Param A pointer to the parameter passed to the task (unused in this function).
 */

void Ql_Example_Task(void *Param)
{
    LCx9H_SPI_Init();
	xSPIMutex = xSemaphoreCreateMutex();
    if (xSPIMutex == NULL) {
        // Mutex creation failed, handle error
        QL_LOG_E("Failed to create SPI mutex");
        return;
    }

	xTaskCreate( LCx9H_SPI_Read_Task,
				"LCx9H_SPI_Read_Task task",
				configMINIMAL_STACK_SIZE * 6,
				NULL,
				tskIDLE_PRIORITY + 6,
				NULL);
	xTaskCreate( LCx9H_SPI_Write_Task,
				"LCx9H_SPI_Write_Task task",
				configMINIMAL_STACK_SIZE * 6,
				NULL,
				tskIDLE_PRIORITY + 7,
				NULL);
    (void)Param;
    
    QL_LOG_I("--->lcx9h SPI nmea<---");
	while (1)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	

}
/**
 * @brief Query the status of the SPI slave with retry mechanism .
 * for PWON and PWOFF command
 * This function queries the status of the SPI slave using the SPI interface, 
 * retrying until success or until the maximum number of retries is reached.
 * It is primarily used in SPI communication with LCx9H series chips to ensure 
 * that the slave status meets expectations.
 * 
 * @param bit_mask Bit mask for specifying which status bits to check
 * @param bit_value Expected value of the status bits
 * @param retry_counter Maximum number of retry attempts
 * @return LCx9H_SPIS_STATUS Returns the communication status, either success or error
 */
static LCx9H_SPIS_STATUS HOST_SPI_MASTER_QUERY_SLAVE_STATUS_WITH_RETRY(uint32_t bit_mask, uint32_t bit_value, uint32_t retry_counter)
{
	uint8_t status_cmd[2] = {LCx9H_SPIS_RS_CMD,0x00};
	uint8_t status;
	uint8_t status_receive[2]={0};
	uint8_t clear_cmd_buf[2];
	int i;
	LCx9H_SPI_Receive_Send spi_send_and_receive_config;

	for(i = 0;i<retry_counter;i++)
	{
		status_receive[1] = 0;
		spi_send_and_receive_config.receive_length = 2;
		spi_send_and_receive_config.send_length = 1;
		spi_send_and_receive_config.send_data = status_cmd;
		spi_send_and_receive_config.receive_buffer = status_receive;
		
		Ql_SPI_ReadWrite(spi_send_and_receive_config.send_data,\
		spi_send_and_receive_config.receive_buffer,spi_send_and_receive_config.receive_length,10);
		status = status_receive[1];
		if (status != 0xff) 
		{
			if (status & (LCx9H_SPIS_STA_RD_ERR_MASK|LCx9H_SPIS_STA_WR_ERR_MASK)) 
			{
				clear_cmd_buf[0]= LCx9H_SPIS_WS_CMD;
				clear_cmd_buf[1] = status;
				Ql_SPI_ReadWrite(clear_cmd_buf,status_receive,2,10);
				return HOST_MUX_STATUS_ERROR;
			}
			else if ((bit_mask & status)== bit_value) 
			{
				return HOST_MUX_STATUS_OK;
			}
		}
	}
	return HOST_MUX_STATUS_ERROR;
}

/**
 * @brief Query the status of the SPI slave with retry mechanism .
 * for other command
 * This function queries the status of the SPI slave using the SPI interface, 
 * retrying until success or until the maximum number of retries is reached.
 * It is primarily used in SPI communication with LCx9H series chips to ensure 
 * that the slave status meets expectations.
 * 
 * @param bit_mask Bit mask for specifying which status bits to check
 * @param bit_value Expected value of the status bits
 * @param retry_counter Maximum number of retry attempts
 * @return LCx9H_SPIS_STATUS Returns the communication status, either success or error
 */
static LCx9H_SPIS_STATUS SPI_query_slave_status(uint32_t bit_mask,uint32_t bit_value,uint32_t retry_counter)
{
    uint8_t status_cmd[2] = {LCx9H_SPIS_RS_CMD,0x00};
    uint8_t status;
    uint8_t status_receive[2]={0};
    uint8_t clear_cmd_buf[2];
    LCx9H_SPI_Receive_Send spi_send_and_receive_config;

    /* Note:
     * The value of receive_length is the valid number of bytes received plus the number of bytes to send.
     * For example, here the valid number of bytes received is 1 byte,
     * and the number of bytes to send also is 1 byte, so the receive_length is 2.
     */
		for(int i = 0;i<retry_counter;i++)
		{
			status_receive[1] = 0;
			spi_send_and_receive_config.receive_length = 2;
			spi_send_and_receive_config.send_length = 1;
			spi_send_and_receive_config.send_data = status_cmd;
			spi_send_and_receive_config.receive_buffer = status_receive;
			
			Ql_SPI_ReadWrite(spi_send_and_receive_config.send_data,\
			spi_send_and_receive_config.receive_buffer,spi_send_and_receive_config.receive_length,10);
			status = status_receive[1];
			if (status != 0xff) //0x2d 00101101
			{
				
				if (status & (LCx9H_SPIS_STA_RD_ERR_MASK|LCx9H_SPIS_STA_WR_ERR_MASK)) //0x5c 01011100
				{
					clear_cmd_buf[0]= LCx9H_SPIS_WS_CMD;
					clear_cmd_buf[1] = status;
					Ql_SPI_ReadWrite(clear_cmd_buf,status_receive,2,10);
					if(bit_mask!=LCx9H_SPIS_STA_FIFO_RDY_MASK)
					{
						Power_off();
						Power_on();
					}
					return HOST_MUX_STATUS_ERROR;
				}
				else if((bit_mask & status)== bit_value) 
				{
					return HOST_MUX_STATUS_OK;
				}
				else if (((bit_mask == LCx9H_SPIS_STA_FIFO_RDY_MASK) || (bit_mask == LCx9H_SPIS_STA_RDWR_FINISH_MASK)) && (status & LCx9H_SPIS_STA_ON_MASK) == 0) 
				{
					Power_off();
					Power_on();
					return HOST_MUX_STATUS_ERROR;
				}
			}
			else
			{
				return HOST_MUX_STATUS_ERROR;
			}
		}
		return HOST_MUX_STATUS_ERROR;
}
/**
 * @brief Writes a command to the SPI device.
 * for PWON and PWOFF command
 * This function sends a specified command to the SPI device and waits for the device to respond with the expected status.
 * It mainly handles two types of commands: power on and power off. For each type of command, it sends the command via SPI
 * and waits for the device to return the corresponding status. If the expected status is not received within the specified
 * number of retries, it logs an error message.
 * 
 * @param cmd The command to be written to the SPI device, which determines the operation to be performed.
 */
void write_cmd(uint8_t cmd)
{
	uint8_t temp_cmd = cmd;
	uint8_t recv_buf[2] = {0};
	switch(temp_cmd)
	{
		case LCx9H_SPIS_PWON_CMD:
			while(1)
			{
				Ql_SPI_ReadWrite(&temp_cmd,recv_buf,1,10);
                //QL_LOG_D("recv_buf = %02x\r\n",recv_buf[0]);
				if (HOST_MUX_STATUS_OK == HOST_SPI_MASTER_QUERY_SLAVE_STATUS_WITH_RETRY( LCx9H_SPIS_STA_ON_MASK, LCx9H_SPIS_STA_ON_MASK , 10))
                {
                    break;
                }
                else
                {
                    QL_LOG_D("**** %s LCx9H_SPIS_PWON_CMD ERROR ****\r\n",__func__);
                }
			}
			break;
		case LCx9H_SPIS_PWOFF_CMD:
			while(1)
			{
				Ql_SPI_ReadWrite(&temp_cmd,recv_buf,1,10);
                
				if (HOST_MUX_STATUS_OK == HOST_SPI_MASTER_QUERY_SLAVE_STATUS_WITH_RETRY( LCx9H_SPIS_STA_ON_MASK, 0 , 100))
                {
                    break;
                }
                else
                {
                    QL_LOG_D("**** %s LCx9H_SPIS_PWOFF_CMD ERROR ****\r\n",__func__);
                }
			}
			break;
	}
}
/**
 * @brief Send a configuration command to the LCx9H device
 *
 * This function prepares and sends a configuration command to the LCx9H device. It packs the command, reg, and data length into a buffer,
 * then communicates via the SPI interface.
 *
 * @param cmd      The command byte to send.
 * @param reg      The reg value to be sent with the command.
 * @param buf      Pointer to the buffer where the data is stored.
 * @param length   Length of the data to be sent.
 *
 * @return         0 on success, -1 if the buffer is NULL or length is 0.
 */
static int8_t LCx9HSPI_Send_Cfg_Cmd(uint8_t cmd,uint32_t reg,uint8_t *buf,uint32_t length)
{
	if(buf == NULL || length == 0)
	{
		return -1;
	}
	uint8_t recv_cmd[9];
	buf[0] = cmd;
	buf[1] = reg & 0xff;
	buf[2] = (reg >> 8) & 0xff;
	buf[3] = (reg >> 16) & 0xff;
	buf[4] = (reg >> 24) & 0xff;
	buf[5] = (length - 1) & 0xff;
	buf[6] = ((length - 1) >> 8) & 0xff;
	buf[7] = ((length - 1) >> 16) & 0xff;
	buf[8] = ((length - 1) >> 24) & 0xff;
	Ql_SPI_ReadWrite(buf,recv_cmd,9,10);
	return 0;

}
uint8_t g_recvbuf[LCx9H_SPIS_RX_SIZE] = {0};
uint8_t g_sendbuf[LCx9H_SPIS_TX_SIZE] = {0};
/**
 * @brief Send data or command via SPI interface.
 *
 * This function sends data or commands to a device via the SPI interface. 
 * It supports two operation modes: read and write.
 *
 * @param Cmd       Command byte to be sent (LCx9H_SPIS_RD_CMD for read, LCx9H_SPIS_WR_CMD for write)
 * @param SendBuf   Pointer to the buffer containing data to send (ignored if performing a read operation)
 * @param RecvBuf   Pointer to the buffer where received data will be stored
 * @param Length    Number of bytes to transfer (excluding the command byte)
 *
 * @return int8_t   0 on success, -1 on failure (invalid parameters or unsupported command)
 */
static int8_t LCx9HSPI_Send_Data_Cmd(uint8_t Cmd,uint8_t *SendBuf,uint8_t* RecvBuf,uint32_t Length)
{
	if(SendBuf == NULL || Length == 0 || RecvBuf == NULL)
	{
		return -1;
	}
	Length += 1;
	if(Cmd == LCx9H_SPIS_RD_CMD)
	{
		SendBuf[0] = Cmd;
		Ql_SPI_ReadWrite(SendBuf, RecvBuf, Length, 10);
	}
	else if (Cmd == LCx9H_SPIS_WR_CMD)
	{
		g_sendbuf[0] = Cmd;
		memcpy(&g_sendbuf[1],SendBuf,Length);
		Ql_SPI_ReadWrite(g_sendbuf, RecvBuf, Length, 10);
	}
	else
	{
		return -1;
	}
	return 0;
}
/**
 * @brief Write command and address with length
 * 
 * This function is used to communicate with the SPI slave device by writing commands and addresses along with length information. It handles the sending of commands, checking of status, and data transmission. In case of communication failure, it retries the operation.
 * 
 * @param offset Register offset address
 * @param buf Data buffer
 * @param length Pointer to data length
 * @return uint8_t Returns the status of the operation
 */
uint8_t write_cmd_addr_length(uint32_t offset, uint8_t *buf, uint32_t *length)
{
	uint8_t cfg_cmd[9];
	uint8_t *temp_buf = buf;
	uint32_t receive_reg_value;
	uint32_t temp_offset = offset;
	uint32_t temp_length = 0;
	uint32_t total_length;
    uint8_t fail_counter = 0;
	static  uint8_t temp_host_mux_send_buf[4+1] = {0};//Airoha chip SPI master need the buffer 4B align
	static  uint8_t temp_host_mux_receive_buf[4+1];//Airoha chip SPI master need the buffer 4B align
	
_restart:	
	temp_length = 4;
	if(LCx9HSPI_Send_Cfg_Cmd(LCx9H_SPIS_CFG_RD_CMD,offset,cfg_cmd,temp_length) != 0)
	{
		return HOST_MUX_STATUS_ERROR;
	}
	QL_LOG_D("mux_spi_master_demo_receive-Step1_a: Send VG_SPI_SLAVE_CRD_CMD.\r\n");
	if(SPI_query_slave_status(LCx9H_SPIS_STA_FIFO_RDY_MASK,
				LCx9H_SPIS_STA_FIFO_RDY_MASK,MAX_ERROR_NUMBER)==HOST_MUX_STATUS_ERROR)
	{
        if(fail_counter >= FAIL_MAX_TIME)
        {
            QL_LOG_D("read data step 1-b fail over 5 times\r\n");
            fail_counter = 0;
            return HOST_MUX_STATUS_ERROR;
        }
        fail_counter++;
        QL_LOG_D("mux_spi_master_demo_receive-Step1_a: #### too many ERROR, now go to restart!!!!!\r\n");    
		goto _restart;
	}
    else
    {
        fail_counter = 0;
    }
	switch(temp_offset)
	{
		case LCx9H_SPIS_TX_LEN_REG://读取NMEA
		{	
			QL_LOG_D("mux_spi_master_demo_receive-Step1_c: Receive SPI slave Rx_len Reg value. \r\n");
			QL_LOG_D("mux_spi_master_demo_receive-Step1_c: Master want to receive 4B.\r\n");
			if(0 != LCx9HSPI_Send_Data_Cmd(LCx9H_SPIS_RD_CMD,temp_host_mux_send_buf,temp_host_mux_receive_buf,4))
			{
				return HOST_MUX_STATUS_ERROR;
			}
			if(SPI_query_slave_status(LCx9H_SPIS_STA_RDWR_FINISH_MASK, \
                        LCx9H_SPIS_STA_RDWR_FINISH_MASK,MAX_ERROR_NUMBER)==HOST_MUX_STATUS_ERROR)
			{
                if(fail_counter>=FAIL_MAX_TIME)
                {
                    QL_LOG_D("read data step 1-d fail over 5 times\r\n");
                    fail_counter = 0;
                    return HOST_MUX_STATUS_ERROR;
                }
                fail_counter++;
				QL_LOG_D(" read data mux_spi_master_demo_receive-Step1_d: #### too many ERROR, now go to restart!!!!!\r\n");
				goto _restart;
			}
            else
            {
                fail_counter = 0;
            }
			receive_reg_value = temp_host_mux_receive_buf[1] | (temp_host_mux_receive_buf[2]<<8) | (temp_host_mux_receive_buf[3]<<16)|(temp_host_mux_receive_buf[4]<<24);
			QL_LOG_D("mux_spi_master_demo_receive-Step1_c: Receive SPI slave Rx_len Reg value:0x%x. success!!!\r\n", (unsigned int)receive_reg_value);
			if(receive_reg_value > LCx9H_SPIS_RX_SIZE)
			{
				QL_LOG_D("mux_spi_master_demo_receive-Step1_c: slave data len is %d too big, request %d B firstly;\r\n", receive_reg_value, (int)*length);
				receive_reg_value = *length;
			}
			if(receive_reg_value==0)
			{
				*length = receive_reg_value;
				return HOST_MUX_STATUS_OK;
			}
			temp_length = receive_reg_value;
			
			if(0 != LCx9HSPI_Send_Cfg_Cmd(LCx9H_SPIS_CFG_RD_CMD,LCx9H_SPIS_TX_BUF_REG,cfg_cmd,temp_length))
			{
				return HOST_MUX_STATUS_ERROR;
			}
			QL_LOG_D("mux_spi_master_demo_receive-Step2_a: send VG_SPI_SLAVE_RD_CMD.\r\n");
			QL_LOG_D("mux_spi_master_demo_receive-Step2_a: Master want to send 0x0a cmd.\r\n");
			if(SPI_query_slave_status(LCx9H_SPIS_STA_FIFO_RDY_MASK, \
						LCx9H_SPIS_STA_FIFO_RDY_MASK,MAX_ERROR_NUMBER)==HOST_MUX_STATUS_ERROR)
			{
				if(fail_counter>=FAIL_MAX_TIME)
				{
					QL_LOG_D("read data step 2-b fail over 5 times\r\n");
					fail_counter = 0;
					return HOST_MUX_STATUS_ERROR;
				}
				fail_counter++;
				QL_LOG_D("mux_spi_master_demo_receive-Step2_b: #### too many ERROR, now go to restart!!!!!\r\n");
				goto _restart;
			}
			else
			{
				fail_counter = 0;
			}
			if(0 != LCx9HSPI_Send_Data_Cmd(LCx9H_SPIS_RD_CMD,g_sendbuf,g_recvbuf,temp_length))
			{
				return HOST_MUX_STATUS_ERROR;
			}
			QL_LOG_D("mux_spi_master_demo_receive-Step2_c: Master want to receive:%d\r\n", (int)temp_length);
			if(SPI_query_slave_status(LCx9H_SPIS_STA_RDWR_FINISH_MASK,
						LCx9H_SPIS_STA_RDWR_FINISH_MASK,MAX_ERROR_NUMBER)==HOST_MUX_STATUS_ERROR)
			{
				if(fail_counter>=FAIL_MAX_TIME)
				{
					QL_LOG_D("read data step 2-d fail over 5 times\r\n");
					fail_counter = 0;
					return HOST_MUX_STATUS_ERROR;
				}
				fail_counter++;
				QL_LOG_D("mux_spi_master_demo_receive-Step2_d: #### too many ERROR, now go to restart!!!!!\r\n");
				goto _restart;
			}
			else
			{
				fail_counter = 0;
			}
			*length = receive_reg_value;
			return HOST_MUX_STATUS_OK;
		}
		case LCx9H_SPIS_RX_LEN_REG://发送
		{
			if(0 != LCx9HSPI_Send_Data_Cmd(LCx9H_SPIS_RD_CMD,temp_host_mux_send_buf,temp_host_mux_receive_buf,4))
			{
				return HOST_MUX_STATUS_ERROR;
			}
            QL_LOG_D("mux_spi_master_demo_send-Step1_c: receive SPI slave Tx_len Reg value.\r\n", 0);
            QL_LOG_D("mux_spi_master_demo_send-Step1_c: Master want to receive 4B.\r\n", 0);
			if(SPI_query_slave_status(LCx9H_SPIS_STA_RDWR_FINISH_MASK, \
                        LCx9H_SPIS_STA_RDWR_FINISH_MASK,MAX_ERROR_NUMBER) == HOST_MUX_STATUS_ERROR)
			{
                if(fail_counter>=FAIL_MAX_TIME)
                {
                    QL_LOG_D("write data step 1-d fail over 5 times\r\n");
                    fail_counter = 0;
                    return HOST_MUX_STATUS_ERROR;
                }
                fail_counter++;
				QL_LOG_D("write data mux_spi_master_demo_send-Step1_d: #### too many ERROR, now go to restart!!!!!\r\n");
				goto _restart;
			}
            else
            {
                fail_counter = 0;
            }
			receive_reg_value = temp_host_mux_receive_buf[1] | (temp_host_mux_receive_buf[2]<<8) | (temp_host_mux_receive_buf[3]<<16)|(temp_host_mux_receive_buf[4]<<24);
            QL_LOG_D("read rx buf size: %d\r\n",receive_reg_value);
			QL_LOG_D("mux_spi_master_demo_send-Step1_c: Receive SPI slave Rx_len Reg value:0x%x. success!!!\r\n", (unsigned int)receive_reg_value);
			if(receive_reg_value >= *length)
			{
                QL_LOG_D("mux_spi_master_demo_send-Step1_c: Receive SPI slave Rx_len Reg value:%d, but master just want to send:%d\r\n", (int)receive_reg_value, (int)*length);
                temp_length = *length;
			}
			else
			{
				QL_LOG_D("module rx buffer is too small, total %d bytes ---- send %d B firstly;\r\n",*length, receive_reg_value);
				temp_length = receive_reg_value;
			}
			total_length = *length;		
			do
			{
				//QL_LOG_I("%d bytes send\r\n", temp_length);
				LCx9HSPI_Send_Cfg_Cmd(LCx9H_SPIS_CFG_WR_CMD, LCx9H_SPIS_RX_BUF_REG, cfg_cmd, temp_length);
				QL_LOG_D("mux_spi_master_demo_send-Step2_a: send VG_SPI_SLAVE_WD_CMD.\r\n");
				QL_LOG_D("mux_spi_master_demo_send-Step2_a: Master want to send 8B \r\n");
				if(SPI_query_slave_status(LCx9H_SPIS_STA_FIFO_RDY_MASK,
				LCx9H_SPIS_STA_FIFO_RDY_MASK,MAX_ERROR_NUMBER)==HOST_MUX_STATUS_ERROR)
				{
					if(fail_counter>=FAIL_MAX_TIME)
					{
						QL_LOG_I("write data step 1-d fail over 5 times\r\n");
						fail_counter = 0;
						return HOST_MUX_STATUS_ERROR;
					}
					fail_counter++;
					QL_LOG_I("mux_spi_master_demo_send-Step2_b: #### too many ERROR, now go to restart!!!!!\r\n"); 
					goto _restart;
				}
				else
				{
					fail_counter = 0;
				}
			
				if(LCx9HSPI_Send_Data_Cmd(LCx9H_SPIS_WR_CMD,temp_buf,g_recvbuf,temp_length) != 0)
				{
					return HOST_MUX_STATUS_ERROR;
				}
				QL_LOG_D("mux_spi_master_demo_send-Step2_c:Master want to send:%d\r\n", (int)receive_reg_value);
				QL_LOG_D("mux_spi_master_demo_send-Step2_c: master send data. buf address:0x%x\r\n", (unsigned int)buf);
				total_length -= temp_length;
				if(total_length != 0)
				{
					//QL_LOG_I("Remain %d bytes send\r\n", total_length);
					temp_buf = &buf[temp_length];
					temp_length = total_length;
				}
			} while (total_length > 0);
			if(receive_reg_value > *length)
				//QL_LOG_I("after send rx buf size: %d\r\n",receive_reg_value-*length);
			return HOST_MUX_STATUS_OK;
		}
			
		default:QL_LOG_D("Error Ofset = %08x\r\n",offset); break;
	}
	return HOST_MUX_STATUS_ERROR;
}

/**
 * @brief Perform data operation based on the provided command.
 *
 * This function executes data operations such as writing or reading from a buffer based on the specified command.
 *
 * @param cmd        Command to execute (e.g., LCx9H_SPIS_WR_CMD, LCx9H_SPIS_RD_CMD).
 * @param data_buf   Pointer to the data buffer for read/write operations.
 * @param length     Pointer to a variable that holds the length of the data.
 *
 * @return uint8_t   Return code indicating the result of the operation:
 *                   - 0: Operation successful.
 *                   - 1: Write command error.
 *                   - 2: Read command error.
 *                   - 0xFF: Unsupported command.
 */
uint8_t Data_operation(uint8_t cmd,uint8_t* data_buf,uint32_t* length)
{
	switch(cmd)
	{
		case LCx9H_SPIS_WR_CMD:
			if(write_cmd_addr_length(LCx9H_SPIS_RX_LEN_REG/*0x04*/,data_buf,length)==HOST_MUX_STATUS_OK)	
			{
				QL_LOG_D("send ok\r\n");
				return 0;
			}
			else              
			{
                QL_LOG_D("**** %s SPIS_WR_CMD ERROR ****\r\n",__func__);
				return 1;
			}
			
		case LCx9H_SPIS_RD_CMD:	
			if(write_cmd_addr_length(LCx9H_SPIS_TX_LEN_REG/*0x08*/,data_buf,length)==HOST_MUX_STATUS_OK)
			{
				QL_LOG_D("recv ok\r\n");
				return 0;	
			}
			else
			{
                QL_LOG_D("**** %s SPIS_RD_CMD ERROR ****\r\n",__func__);
				return 2;
			}

		default: ;
	}
	return 0xFF;
}

/**
 * @brief Write data to the LCx9H device via SPI interface
 *
 * This function is responsible for writing data to the LCx9H device through the SPI interface.
 * 
 * @param buffer Pointer to the data buffer that needs to be written.
 * @param size   The size of the data buffer in bytes.
 * @param timeout Timeout value for the operation in milliseconds.
 * 
 * @return Returns 0 on success, non-zero on failure.
 */
int32_t Ql_LCx9H_PortSPIWrite(uint8_t* buffer,uint32_t size,int32_t timeout)
{
    uint8_t result = 0;
    uint32_t temp_length = size;
    result = Data_operation(LCx9H_SPIS_WR_CMD,buffer,&temp_length);
	if(result == 0)
	{
		QL_LOG_D("write %s",buffer);
	}
    return result;
}
/**
 * @brief Reads data from the SPI port.
 *
 * This function reads data from the SPI interface and stores it into the provided buffer.
 *
 * @param buffer Pointer to the buffer where the read data will be stored.
 * @param size   Pointer to a uint32_t variable that specifies the size of the data to read. 
 *               On return, it contains the actual number of bytes read.
 * @param timeout Timeout value for the operation in milliseconds. 
 *                If the operation does not complete within this time, it may fail.
 *
 * @return Returns 0 if the operation is successful, non-zero otherwise.
 */
int32_t Ql_LCx9H_PortSPIRead(uint8_t *buffer, uint32_t* size,int32_t timeout)
{
    uint8_t result = 0;
    result = Data_operation(LCx9H_SPIS_RD_CMD,g_recvbuf,size);
	memcpy(buffer,&g_recvbuf[1],*size);
    return result;
}
#endif // DEBUG
