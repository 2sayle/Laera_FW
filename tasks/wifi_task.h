/**
  ******************************************************************************
  * @file    wifi_task.h
  * @brief   Public interface for Wi-Fi station task.
  * @author  Elyass Jaoudat (ejaoudat@outlook.fr)
  ******************************************************************************
  * @attention
  *
  * This header exposes Wi-Fi connection status bits and the task entry point
  * used by the scheduler.
  *
  ******************************************************************************
  */

#ifndef LAERA_FW_WIFI_HANDLER_H
#define LAERA_FW_WIFI_HANDLER_H

/* Exported constants --------------------------------------------------------*/
/**
  * @brief Event bit set when station is connected and has a valid IP.
  */
#define WIFI_CONNECTED_BIT  BIT0
/**
  * @brief Event bit set when station connection fails after retries.
  */
#define WIFI_FAIL_BIT       BIT1

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Wi-Fi task function.
  * @param  arg Task argument pointer.
  * @retval None
  */
void wifi_task(void *arg);


#endif //LAERA_FW_WIFI_HANDLER_H
