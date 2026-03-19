/*
 * esp8266.h
 *
 *  Created on: Mar 7, 2026
 *      Author: Rubin Khadka
 */

#ifndef INC_ESP8266_H_
#define INC_ESP8266_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
  ESP8266_OK = 0,
  ESP8266_ERROR,
  ESP8266_TIMEOUT,
  ESP8266_NO_RESPONSE,
} ESP8266_Status;

typedef enum
{
  ESP8266_DISCONNECTED = 0,
  ESP8266_CONNECTED_NO_IP,
  ESP8266_CONNECTED_IP
} ESP8266_ConnectionState;

extern ESP8266_ConnectionState ESP_ConnState;

// Public Functions
ESP8266_Status ESP_Init(void);
ESP8266_Status ESP_ConnectWiFi(const char *ssid, const char *password, char *ip_buffer, uint16_t buffer_len);
ESP8266_ConnectionState ESP_GetConnectionState(void);
ESP8266_Status ESP_SendToThingSpeak(const char *apiKey, float val1, float val2);

#endif /* INC_ESP8266_H_ */
