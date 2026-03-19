/*
 * esp8266.c
 *
 *  Created on: Mar 7, 2026
 *      Author: Rubin Khadka
 */

#include "esp8266.h"
#include "usart1.h"
#include "usart2.h"
#include "timer2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

ESP8266_ConnectionState ESP_ConnState = ESP8266_DISCONNECTED;
static char esp_rx_buffer[2048];
static ESP8266_Status ESP_GetIP(char *ip_buffer, uint16_t buffer_len);
static ESP8266_Status ESP_SendCommand(const char *cmd, const char *ack, uint32_t timeout);

ESP8266_Status ESP_Init(void)
{
  ESP8266_Status res;
  TIMER2_Delay_ms(1000);

  res = ESP_SendCommand("AT\r\n", "OK", 2000);
  if(res != ESP8266_OK)
  {
    USART2_SendString("ESP_Init: AT command failed\r\n");
    return res;
  }

  res = ESP_SendCommand("ATE0\r\n", "OK", 2000); // Disable echo
  if(res != ESP8266_OK)
  {
    USART2_SendString("ESP_Init: ATE0 failed\r\n");
    return res;
  }

  return ESP8266_OK;
}

ESP8266_Status ESP_ConnectWiFi(const char *ssid, const char *password, char *ip_buffer, uint16_t buffer_len)
{
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "AT+CWMODE=1\r\n");

  ESP8266_Status result = ESP_SendCommand(cmd, "OK", 2000); // wait up to 2s
  if(result != ESP8266_OK)
  {
    USART2_SendString("ESP_ConnectWiFi: Failed to set mode\r\n");
    return result;
  }

  USART2_SendString("Connecting to WIFI: ");
  USART2_SendString(ssid);
  USART2_SendString("...\n");

  // Send join command
  snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);

  result = ESP_SendCommand(cmd, "WIFI CONNECTED", 10000); // wait up to 10s
  if(result != ESP8266_OK)
  {
    USART2_SendString("WiFi connection failed\r\n");
    ESP_ConnState = ESP8266_CONNECTED_NO_IP;
    return result;
  }

  USART2_SendString("Connected to WiFi !!!\n");
  ESP_ConnState = ESP8266_CONNECTED_NO_IP;

  // Fetch IP with retries inside ESP_GetIP
  result = ESP_GetIP(ip_buffer, buffer_len);
  if(result != ESP8266_OK)
  {
    USART2_SendString("Failed to get IP address\r\n");
    return result;
  }

  USART2_SendString("WiFi + IP ready: ");
  USART2_SendString(ip_buffer);
  USART2_SendString("\r\n");

  return ESP8266_OK;
}

ESP8266_ConnectionState ESP_GetConnectionState(void)
{
  return ESP_ConnState;
}

ESP8266_Status ESP_SendToThingSpeak(const char *apiKey, float val1, float val2)
{
  char cmd[256];
  ESP8266_Status result;

  USART2_SendString("Connecting to ThingSpeak...\r\n");

  // Start TCP connection (HTTP port 80)
  snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n");
  result = ESP_SendCommand(cmd, "CONNECT", 5000);
  if(result != ESP8266_OK)
  {
    USART2_SendString("Failed to connect to ThingSpeak\r\n");
    return result;
  }

  // Build HTTP GET request
  char httpReq[256];
  snprintf(httpReq, sizeof(httpReq), "GET /update?api_key=%s&field1=%.2f&field2=%.2f\r\n", apiKey, val1, val2);

  // Tell ESP how many bytes we will send
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", (int) strlen(httpReq));
  result = ESP_SendCommand(cmd, ">", 2000);
  if(result != ESP8266_OK)
  {
    USART2_SendString("CIPSEND failed\r\n");
    return result;
  }

  // Send actual request and wait for ThingSpeak response
  result = ESP_SendCommand(httpReq, "SEND OK", 5000);
  if(result != ESP8266_OK)
  {
    USART2_SendString("Failed to send data\r\n");
    return result;
  }

  // Parse ThingSpeak reply
  char *ipd = strstr(esp_rx_buffer, "+IPD,");
  if(ipd)
  {
    char *colon = strchr(ipd, ':');
    if(colon)
    {
      int entryId = atoi(colon + 1);  // convert server reply to int

      if(entryId > 0)
      {
        USART2_SendString("Update successful! Entry ID: ");
        USART2_SendNumber(entryId);
        USART2_SendString("\r\n");
        return ESP8266_OK;
      }
      else
      {
        USART2_SendString("Invalid entry ID received\r\n");
        return ESP8266_ERROR;
      }
    }
  }

  USART2_SendString("No valid ThingSpeak response found\r\n");
  USART2_SendString("Raw response: ");
  USART2_SendString(esp_rx_buffer);
  USART2_SendString("\r\n");

  return ESP8266_ERROR;
}

static ESP8266_Status ESP_GetIP(char *ip_buffer, uint16_t buffer_len)
{
  for(int attempt = 1; attempt <= 3; attempt++)
  {
    USART2_SendString("ESP_GetIP: Attempt ");
    USART2_SendNumber(attempt);
    USART2_SendString(" of 3\r\n");

    ESP8266_Status result = ESP_SendCommand("AT+CIFSR\r\n", "OK", 5000);
    if(result != ESP8266_OK)
    {
      USART2_SendString("ESP_GetIP: CIFSR command failed\r\n");
      continue;
    }

    char *search = esp_rx_buffer;
    char *last_ip = NULL;

    while((search = strstr(search, "STAIP,")) != NULL)
    {
      char *ip_start = strstr(search, "STAIP,\"");
      if(ip_start)
      {
        ip_start += 7;
        char *end = strchr(ip_start, '"');
        if(end && ((end - ip_start) < buffer_len))
        {
          last_ip = ip_start;
        }
      }
      search += 6;
    }

    if(last_ip)
    {
      char *end = strchr(last_ip, '"');
      strncpy(ip_buffer, last_ip, end - last_ip);
      ip_buffer[end - last_ip] = '\0';

      USART2_SendString("ESP_GetIP: Found IP: ");
      USART2_SendString(ip_buffer);
      USART2_SendString("\r\n");

      if(strcmp(ip_buffer, "0.0.0.0") == 0)
      {
        USART2_SendString("ESP_GetIP: IP is 0.0.0.0, retrying...\r\n");
        ESP_ConnState = ESP8266_CONNECTED_NO_IP;
        TIMER2_Delay_ms(1000);
        continue;
      }

      ESP_ConnState = ESP8266_CONNECTED_IP;
      return ESP8266_OK;
    }

    USART2_SendString("ESP_GetIP: No IP found in response\r\n");
    TIMER2_Delay_ms(500);
  }

  USART2_SendString("ESP_GetIP: Failed after 3 attempts\r\n");
  ESP_ConnState = ESP8266_CONNECTED_NO_IP;  // still connected, but no IP
  return ESP8266_ERROR;
}

static ESP8266_Status ESP_SendCommand(const char *cmd, const char *ack, uint32_t timeout)
{
  uint8_t ch;
  uint16_t idx = 0;
  uint32_t tickstart;
  int found = 0;

  // Debug: Show command being sent
  USART2_SendString(">> Sending: ");
  USART2_SendString(cmd);
  if(cmd[strlen(cmd) - 1] != '\n')
    USART2_SendString("\r\n");

  memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
  tickstart = TIMER2_GetMillis();

  if(strlen(cmd) > 0)
  {
    USART1_SendString(cmd);
  }

  while((TIMER2_GetMillis() - tickstart) < timeout && idx < sizeof(esp_rx_buffer) - 1)
  {
    if(USART1_DataAvailable())
    {
      ch = USART1_GetChar();
      esp_rx_buffer[idx++] = ch;
      esp_rx_buffer[idx] = '\0';

      // check for ACK
      if(!found && strstr(esp_rx_buffer, ack))
      {
        found = 1;
        USART2_SendString("<< Found ACK: ");
        USART2_SendString(ack);
        USART2_SendString("\r\n");
      }

      // handle busy response
      if(strstr(esp_rx_buffer, "busy"))
      {
        USART2_SendString("<< Device busy, retrying...\r\n");
        TIMER2_Delay_ms(1500);
        idx = 0;
        memset(esp_rx_buffer, 0, sizeof(esp_rx_buffer));
        continue;
      }
    }
  }

  if(found)
  {
    return ESP8266_OK;
  }

  if(idx == 0)
  {
    USART2_SendString("<< No response received\r\n");
    return ESP8266_NO_RESPONSE;
  }

  return ESP8266_TIMEOUT;
}
