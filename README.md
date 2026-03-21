# STM32 Bare-Metal IoT: DHT11 Sensor to MQTT via ESP8266

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
[![STM32](https://img.shields.io/badge/STM32-F103C8T6-blue)](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html)
[![CubeIDE](https://img.shields.io/badge/IDE-STM32CubeIDE-darkblue)](http://st.com/en/development-tools/stm32cubeide.html)
[![ESP8266](https://img.shields.io/badge/NodeMCU-ESP8266-orange)](https://www.espressif.com/en/products/socs/esp8266)
[![MQTT](https://img.shields.io/badge/MQTT-3.1.1-green)](https://mqtt.org/)

## Project Overview

A complete bare-metal IoT project that reads temperature and humidity from a DHT11 sensor using an STM32F103C8T6 microcontroller, displays the data on a 16x2 LCD, and publishes it to a HiveMQ MQTT broker via an ESP8266 WiFi module. Built entirely from scratch without libraries or RTOS.

While NodeMCU is powerful enough to build complete IoT projects on its own, the objective here is different: **use NodeMCU strictly as a WiFi modem** controlled entirely by an STM32. The NodeMCU runs AT firmware and acts as a slave device, the STM32 sends commands, and the ESP8266 executes them.

I had this module available, so I used it for demonstration but the code works with any ESP8266 module running AT firmware.

## Video Demonstrations

### Hardware Setup

https://github.com/user-attachments/assets/a0a5db33-6efd-470d-9a2d-b6af465087df

### Hterm and MQTT Explorer Output

https://github.com/user-attachments/assets/941c0d22-def7-4e97-9dfe-1fc78a94a81a

## Project Schematic

<img width="1457" height="602" alt="schematic_diagram" src="https://github.com/user-attachments/assets/11b0fce3-c832-4395-ac85-7d13268fc907" />

## Hardware Requirements

| Component | Quantity | Description |
|-----------|----------|-------------|
| **STM32F103C8T6 (Blue Pill)** | 1 | Main microcontroller |
| **NodeMCU ESP8266** | 1 | WiFi module running AT firmware (acts as modem) |
| **DHT11** | 1 | Temperature and humidity sensor |
| **LCD 16x2 with I2C** | 1 | Character display module with I2C backpack (PCF8574) |
| **ST-Link V2 Programmer** | 1 | For flashing/debugging STM32 |
| **USB to UART Converter** | 1 | For debug output (USART2) |

## Pin Configuration

### STM32 to NodeMCU (USART1)

| STM32 Pin | Function | NodeMCU Pin | Notes |
|-----------|----------|-------------|-------|
| **PA9** | USART1 TX | RX | STM32 transmits, NodeMCU receives |
| **PA10** | USART1 RX | TX | STM32 receives, NodeMCU transmits |
| **GND** | Ground | GND | Must be connected! |
| **3.3V** | Power | VCC | Optional if NodeMCU is USB-powered |

### DHT11 Sensor

| STM32 Pin | Function | DHT11 Pin | Notes |
|-----------|----------|-----------|-------|
| **PB13** | GPIO | DATA | Data pin (module already contains pull-up) |
| **3.3V** | Power | VCC | Power supply |
| **GND** | Ground | GND | Common ground |

### Debug Output (USART2)

| STM32 Pin | Function | USB-UART Converter | Purpose |
|-----------|----------|--------------------|---------|
| **PA2** | USART2 TX | RX | Send debug messages to PC |
| **PA3** | USART2 RX | TX | (Optional) Receive from PC |
| **GND** | Ground | GND | Common ground |

### LCD 16x2 I2C

| Peripheral | Pin | Connection | Notes |
|------------|-----|------------|-------|
| **LCD 16x2 I2C** | PB10 | SCL | I2C2 clock |
| | PB11 | SDA | I2C2 data |
| | 5V | VCC | Power |
| | GND | GND | Common ground |

| Device | I2C Address (7-bit) | 8-bit Write | 8-bit Read |
|--------|---------------------|-------------|------------|
| **LCD (PCF8574)** | 0x27 (7-bit) | 0x4E (write) | 0x4F (read) |

🔗 [View Custom Written I2C Driver Source Code](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/i2c1.c) <br>
🔗 [View LCD Driver Source Code](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/lcd.c) 

## Task Scheduling

The system uses a **10ms timer-based control loop** with independent counters for each task. TIMER3 is configured to drive the main control loop.

### Task Frequencies

| Task | Frequency | Period | Execution |
|------|-----------|--------|-----------|
| **DHT11 Read** | 1 Hz | 1 second | Every 100 loops |
| **LCD Update** | 10 Hz | 100 ms | Every 10 loops |
| **MQTT Publish** | 0.1 | 10 seconds | every 1000 loops |

### Timer Configuration

| Timer | Resolution | Purpose |
|-------|------------|---------|
| **DWT** | 1µs | DS18B20 1-Wire protocol precise timing |
| **TIMER2** | 1ms | System millisecond counter and delays |
| **TIMER3** | 0.1ms | **10ms control loop scheduler** (heartbeat) |

🔗 [View DWT Driver (Microsecond Delay)](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/dwt.c)  
🔗 [View TIMER2 Driver (Millisecond Counter)](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/timer2.c)  
🔗 [View TIMER3 Driver (10ms Heartbeat)](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/timer3.c)  

> **Note:** DWT (Data Watchpoint and Trace) is a built-in peripheral in ARM Cortex-M3 cores that provides a cycle counter running at CPU frequency (72MHz). This gives ~13.9ns resolution, making it ideal for generating precise microsecond delays required by the DS18B20 and DHT11 1-Wire protocol. Unlike traditional timer-based delays, DWT does not occupy a dedicated timer peripheral and continues running in the background.

## MQTT Driver

The STM32 implements the **MQTT 3.1.1 protocol** from scratch, handling packet construction and parsing without external libraries. All packets are built manually using custom buffer manipulation functions.

### How MQTT Works

MQTT is a **binary protocol** that uses lightweight fixed and variable headers. Each packet starts with a control byte followed by remaining length:
- Byte 0: Packet Type + Flags (1 byte) 
- Byte 1+: Remaining Length (1-4 bytes)
- Variable Header + Payload (optional)

| Packet | Binary | Purpose |
|--------|--------|---------|
| **CONNECT** | `0x10` | Establishes connection with broker |
| **CONNACK** | `0x20` | Broker acknowledges connection |
| **PUBLISH** | `0x30` | Sends data to a topic |
| **SUBSCRIBE** | `0x82` | Requests messages from a topic |
| **PINGREQ** | `0xC0` | Keeps connection alive |

### HiveMQ Public Broker

HiveMQ provides a **free public MQTT broker** at `broker.hivemq.com` with:
- No registration required
- No authentication needed
- Ideal for testing and prototyping
- Supports unlimited devices (with unique client IDs)

### Complete Stack
```
Application Layer (tasks.c) - Read sensor, publish every 10 seconds
↓
MQTT Protocol (esp8266.c) - CONNECT, PUBLISH, SUBSCRIBE packet builder
↓
ESP8266 AT Commands (esp8266.c) - TCP connection, data transmission
↓
Hardware (STM32 UART1 + ESP8266)
```

### HiveMQ configuration
```
#define MQTT_BROKER     "broker.hivemq.com"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "STM32_DHT11_001"
#define MQTT_TOPIC      "stm32/dht11/sensor"
#define MQTT_KEEPALIVE  60
```
> **Note:** Publishing every 10 seconds automatically keeps the connection alive, eliminating the need for explicit PINGREQ packets. 

🔗 [View ESP8266 Driver (AT Commands)](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/esp8266.c)  
🔗 [View MQTT Implementation](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/tasks.c)  

> Test: Subscribe with MQTT Explorer at `broker.hivemq.com:1883` to topic `stm32/dht11/sensor`.

## DHT11 Sensor Driver

The DHT11 uses a **single-wire protocol** with precise timing:

| Phase | Duration | Description |
|-------|----------|-------------|
| **Start Signal** | 18ms LOW + 20µs HIGH | MCU wakes sensor |
| **Sensor Response** | 80µs LOW + 80µs HIGH | Sensor acknowledges |
| **Bit "0"** | 50µs LOW + 26-28µs HIGH | Logic 0 |
| **Bit "1"** | 50µs LOW + 70µs HIGH | Logic 1 |
| **Data Frame** | 40 bits | 5 bytes (humidity ×2 + temp ×2 + checksum) |

Instead of measuring pulse width, I used a **simpler approach** looking at datasheet:

For each bit:
1. Wait for line to go HIGH
2. Delay exactly 40µs
3. If line still HIGH → logic 1 <br>
   If line is LOW → logic 0

To ensure the timing is not interrupted, **interrupts are disabled** while communicating with the sensor. The checksum provided by the sensor is used to verify data integrity.

🔗 [View DHT11 Driver Source Code](https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11/blob/main/Src/dht11.c)

## Getting Started

### Software Prerequisites

| Software | Version | Purpose |
|----------|---------|---------|
| STM32CubeIDE | v1.13.0+ | IDE for development and flashing |
| Serial Terminal | Any (PuTTY, Arduino IDE, Hterm) | For debugging via USART2 |
| ESP8266 AT Firmware | Official or AI-Thinker | Must be flashed to NodeMCU first |

### Setting Up the ESP8266

Before connecting to STM32, ensure the NodeMCU has AT firmware:

1. **Test with PC** using USB and serial terminal:
    ```
    AT 
    OK

    AT+CWMODE=1
    OK

    AT+GMR
    AT version:...
    ```

2. If the NodeMCU has Lua firmware, you'll need to **flash AT firmware first**. The AI-Thinker firmware works well with NodeMCU boards.

### Installation

1. Clone the repository
```bash
git clone https://github.com/rubin-khadka/STM32_BareMetal_ESP8266_MQTT_DHT11.git
```
2. Open in STM32CubeIDE
    - `File` → `Import...`
    - `General` → `Existing Projects into Workspace` → `Next`
    - Select the project directory
    - Click `Finish`    

3. Verify Project Settings
    - `Project` → `Properties` → `C/C++ Build` → `Settings`
    - `MCU GCC Compiler` → `Preprocessor`
    - Ensure `STM32F103xB` is defined
    - `MCU GCC Compiler` → `Include paths` Add the following paths
        - "~\STM32Cube_FW_F1_V1.8.0\Drivers\CMSIS\Core\Include"
        - "~\STM32Cube_FW_F1_V1.8.0\Drivers\CMSIS\Device\ST\STM32F1xx\Include"

4. Update Configuration
    - Open `main.c` and update your WiFi credentials:
    ```c
    ESP_ConnectWiFi("your_ssid", "your_password", ip_buf, sizeof(ip_buf))
    ```

5. Build & Flash
    - Build: `Ctrl+B`
    - Debug: `F11`
    - Run: `F8` (Resume)

## Related Projects 
- [STM32_ESP8266_DHT11_Thingspeak](https://github.com/rubin-khadka/STM32_ESP8266_DHT11_Thingspeak)
- [STM32_ESP8266_IP_ATCommand](https://github.com/rubin-khadka/STM32_ESP8266_IP_ATCommand)
- [STM32_DHT11_MPU6050_LCD](https://github.com/rubin-khadka/STM32_DHT11_MPU6050_LCD) 

## Resources
- [STM32F103 Datasheet](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [DHT11 Sensor Datasheet](https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf)
- [PCF8574 I2C Backpack Datasheet](https://www.ti.com/lit/ds/symlink/pcf8574.pdf)
- [ESP8266 AT Command Set](https://docs.espressif.com/projects/esp-at/en/latest/esp32/AT_Command_Set/index.html)
- [NodeMCU Documentation](https://nodemcu.readthedocs.io/en/release/)
- [Flashing AT Firmware to NodeMCU](https://docs.ai-thinker.com/en/esp8266/)

## Project Status
- **Status**: Complete
- **Version**: v1.0
- **Last Updated**: March 2026

## Contact
**Rubin Khadka Chhetri**  
📧 rubinkhadka84@gmail.com <br>
🐙 GitHub: https://github.com/rubin-khadka
