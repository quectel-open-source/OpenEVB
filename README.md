# OPENEVB

[![License](https://img.shields.io/badge/License-Apache2-blue.svg)](LICENSE)


OPENEVB is a GD32F4-based evaluation platform designed to accelerate development with Quectel GNSS modules(e.g., LCx9H, LCx6G). It provides:

- Complete NMEA protocol parsing
- Firmware upgrade workflows
- Hardware abstraction layer for Quectel EVB boards
- Verification of RTK functionality via cellular module
- Save NMEA logs


Target Hardware:

- MCU: GD32F470ZI (Cortex-M4 @ 240MHz)
- GNSS Module: Quectel LCx9H (e.g.)
- EVB Board: Quectel EVB



# Table of Contents  
- [OPENEVB](#openevb)
- [Table of Contents](#table-of-contents)
  - [Supported Features](#supported-features)
  - [Quectel EVB](#quectel-evb)
  - [Quick Start](#quick-start)
    - [Code Structure](#code-structure)
    - [Development Environment Setup](#development-environment-setup)
    - [Feature Selection](#feature-selection)
    - [Compilation](#compilation)
    - [Download](#download)
  - [License](#license)

---

## Supported Features
This open-source project provides the following example functionalities:  
- &zwnj;**LCx6G I2C Firmware Upgrade**&zwnj;  
- &zwnj;**LCx9H I2C Firmware Upgrade & Download**&zwnj;  
- &zwnj;**NMEA0183 Data Reading via LCx9H I2C/SPI Interfaces**&zwnj;  
- &zwnj;**LG69TAP I2C NMEA0183 Data Acquisition**&zwnj;  
- &zwnj;**NMEA0183 Data Parsing & Storage**&zwnj;  
- &zwnj;**QuecRTK Differential Injection**&zwnj;  
- &zwnj;**NTRIP Client-Based Differential Injection**&zwnj;  

---

## Quectel EVB

To better validate Quectel GNSS modules, we have developed the Quectel EVB. Below is the framework of our EVB:
The EVB is powered via a Type-C USB cable, with power routed to the GNSS module through a Low-Dropout Regulator (LDO). The GNSS module outputs signals through its communication interface on the EVB via a USB-to-UART bridge chip.
 
![alt text](.\doc\images\EVB-Framework.png)


## Quick Start

### Code Structure

Introduction to directory structure:
```bash
.
├── doc                  -- Store documents that explain the function or structure of the project
│   └── dir_describe.md
├── os                   -- Operating system
│   └── FreeRTOS
├── plat                 -- Chip platform
│   └── gd32f4xx
├── quectel              -- Code ported or developed by quectel
│   ├── app              -- Configuration files and public files
│   ├── bootloader       -- Binary file of the bootloader
│   ├── bsp              -- Board support package
│   ├── component        -- Various components
│   ├── example          -- Sample code for specific business implementation
│   ├── hal              -- Hardware abstraction layer
│   ├── project          -- Project compilation mode
│   └── tools            -- Tools for packaging
└── third_party          -- Third party
    ├── cJSON            -- JSON library
    ├── ff15             -- FAT file system
    └── mbedtls          -- C library that implements cryptographic primitives.
```

### Development Environment Setup  
1. &zwnj;**Install Keil MDK Software**&zwnj;  
   - Download and install the Keil MDK development environment.  
2. &zwnj;**Install Software Package**&zwnj;  
   - Add the &zwnj;**GigaDevice.GD32F4xx_DFP**&zwnj; package via Keil's Pack Installer.  

---

### Feature Selection  
The example feature code resides in the `Quectel/example` directory. Use macro definitions in `inc/example.h` to enable specific functionalities. Ensure &zwnj;**only one macro is active**&zwnj; multiple enabled macros will cause compilation errors. For example:

```c
#define __EXAMPLE_NMEA_PARSE__      // Enable NMEA data parsing
// #define __EXAMPLE_NMEA_SAVE__    // Disable other macros
```

The sample code is located in the `src/` directory. The entry function &zwnj;**Ql_Example_Task**&zwnj; is called in `quectel/app/ql_application.c`. This function is registered as a FreeRTOS task when the system is initialized.

```c
void Ql_Example_Task(void *Param)
{
    static uint8_t rx_buf[NMEA_BUF_SIZE] = {0};
    int32_t Length = 0;

    (void)Param;
    
    QL_LOG_I("--->NMEA Sentence Parse<---");

    Ql_Uart_Init("GNSS COM1", NMEA_PORT, 115200, NMEA_BUF_SIZE, NMEA_BUF_SIZE);

    // NMEA Handle
    extern const Ql_NMEA_Table_TypeDef NMEA_Table[];
    Ql_NMEA_Init(&NMEA_Handle,
                 (Ql_NMEA_Table_TypeDef *)NMEA_Table,
                 NULL,
                 NMEA_BUF_SIZE);

    while (1)
    {
        Length = Ql_Uart_Read(NMEA_PORT, rx_buf, NMEA_BUF_SIZE, 100);
        QL_LOG_D("read data from uart, len: %d", Length);

        if (Length <= 0)
        {
            continue;
        }
        rx_buf[Length] = '\0';

        Ql_NMEA_Parse(&NMEA_Handle, (const char *)rx_buf, Length);
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

...
```

### Compilation
After selecting the desired example features, proceed with the following steps to compile and deploy the firmware.

### Download
1. &zwnj;**ST-Link Download**&zwnj;
   - EVB leads to 4 pins: 3.3V, MCU_RESET, MCU_SWCLK, MCU_SWDIO. Use ST-Link to connect and download to the EVB board.
2. &zwnj;**QGNSS Download**&zwnj;
   - Download QGNSS tool from Quectel official website: [www.quectel.com.cn/download-zone](https://www.quectel.com.cn/download-zone).
   - EVB Connects to QGNSS and Click “Firmware Download”
     - Connect the serial port of MCU to QGNSS tool, then click the “Tools” and select the “Firmware Download” after the EVB connects to the QGNSS tool successfully.
  ![alt text](.\doc\images\EVB-Connects.png)
   - Download Firmware
     - Step 1: Select the firmware you want to download.
     - Step 2: Click the start button.
     - Step 3: Restart the MCU when the screen displays &zwnj;**“Synchronize…”.**&zwnj; The firmware update can be started after the synchronization is finished.
  ![alt text](.\doc\images\Update-MCU-APP-Firmware.png)

## License
Apache 2.0 License: Permits commercial closed-source derivatives.
Requirements: Copyright statement is retained, and there is no mandatory open source requirement.