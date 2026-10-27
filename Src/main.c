/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);


/* Space for your global variables. */

#define RX_RING_SIZE 256

static volatile uint8_t  rx_ring[RX_RING_SIZE];
static volatile uint16_t ring_head = 0;
static volatile uint16_t ring_tail = 0;

static void rxRingPush(uint8_t byte)
{
	uint16_t next = (ring_head + 1) % RX_RING_SIZE;

	if(next != ring_tail)
	{
		rx_ring[ring_head] = byte;
		ring_head = next;
	}
}

static uint8_t rxRingPop(uint8_t *byte)
{
	if(ring_tail == ring_head)
	{
		return 0;
	}

	*byte = rx_ring[ring_tail];
	ring_tail = (ring_tail + 1) % RX_RING_SIZE;
	return 1;
}

#define MAX_STRING_LEN 35
#define STATUS_PERIOD_MS 2000

typedef enum
{
	WAIT_FOR_START,
	READING_STRING
} parser_state_t;

letter_count_ letters = {0};

static parser_state_t parser_state = WAIT_FOR_START;
static char    rx_string[MAX_STRING_LEN + 1];
static uint8_t rx_string_len = 0;
static uint8_t string_ready = 0;

static void startNewString(void)
{
	rx_string_len = 0;
	letters.small_letter = 0;
	letters.capital_letter = 0;
	parser_state = READING_STRING;
}

static char tx_buffer[128];

static void usartSendText(const char *text)
{
	while(LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7));

	uint8_t length = (uint8_t)strlen(text);
	memcpy(tx_buffer, text, length);
	USART2_PutBuffer((uint8_t *)tx_buffer, length);
}

int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();

  /* Space for your local variables, callback registration ...*/

  USART2_RegisterCallback(rxRingPush);
  uint8_t rx_byte;
  char msg[128];
  uint16_t ms_elapsed = 0;

  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */

	  while(rxRingPop(&rx_byte))
	  {
		  proccesDmaData(rx_byte);

		  if(string_ready)
		  {
			  string_ready = 0;
			  snprintf(msg, sizeof(msg), "Valid string: %s, lower-case: %d, upper-case: %d\r\n",
					   rx_string, letters.small_letter, letters.capital_letter);
			  usartSendText(msg);
		  }
	  }
	  if(LL_SYSTICK_IsActiveCounterFlag())
	  {
		  ms_elapsed++;

		  if(ms_elapsed >= STATUS_PERIOD_MS)
		  {
			  ms_elapsed = 0;

			  uint16_t occupied = USART2_GetRxOccupied();
			  float load = 100.0f * occupied / DMA_USART2_BUFFER_SIZE;

			  snprintf(msg, sizeof(msg),
					   "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %%]: %.1f%%\r\n",
					   DMA_USART2_BUFFER_SIZE, occupied, load);
			  usartSendText(msg);
		  }
	  }
  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
	/* Process received data */

	switch(parser_state)
	{
		case WAIT_FOR_START:
			if(sign == '#')
			{
				startNewString();
			}
			break;

		case READING_STRING:
			if(sign == '#')
			{
				startNewString();
			}
			else if(sign == '$')
			{
				rx_string[rx_string_len] = '\0';
				string_ready = 1;
				parser_state = WAIT_FOR_START;
			}
			else
			{
				rx_string[rx_string_len++] = sign;

				if(sign >= 'a' && sign <= 'z')
				{
					letters.small_letter++;
				}
				else if(sign >= 'A' && sign <= 'Z')
				{
					letters.capital_letter++;
				}

				if(rx_string_len >= MAX_STRING_LEN)
				{
					parser_state = WAIT_FOR_START;
				}
			}
			break;
	}
}


void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
