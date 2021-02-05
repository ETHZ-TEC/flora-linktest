/**
  ******************************************************************************
  * Flora
  ******************************************************************************
  * @file   app_config.h
  * @brief  application config file
  *
  *
  ******************************************************************************
  */

#ifndef CONFIG_H_
#define CONFIG_H_

/* General config *************************************************************/
#define FLOCKLAB                        1
#define FLOCKLAB_SWD                    0
#define SWO_ENABLE                      0
#define CLI_ENABLE                      0
#define LOG_USE_COLORS                  0
#define LOG_PRINT_IMMEDIATELY           1
#define LOG_LEVEL                       LOG_LEVEL_VERBOSE
#define LOG_USE_DMA                     1      // use DMA (FIFO) for serial output

#define HOST_ID                         2
#define NODE_ID                         FLOCKLAB_NODE_ID


/* Link test configuration ****************************************************/

#define TESTCONFIG_NUM_NODES            4
#define TESTCONFIG_NODE_IDS             1, 2, 3, 4  //, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
#define TESTCONFIG_NUM_TX               100
#define TESTCONFIG_SETUP_TIME           500          // SetupTime [ms]
#define TESTCONFIG_START_DELAY          1000         // StartDelay [ms]
#define TESTCONFIG_STOP_DELAY           1000         // StopDelay [ms]
#define TESTCONFIG_TX_SLACK             100          // TxSlack [ms]
#define TESTCONFIG_KEY                  "deadbeef"   // payload of RF packets (must not contain escaped chars!)
// TESTCONFIG_NODE_LIST is defined in task_linktest.c for practical reasons

// /* mod5 (SF7) */
// #define RADIOCONFIG_TX_POWER            0            // transmit power [dBm] (valid range: -9...+22)
// #define RADIOCONFIG_FREQUENCY           865440000    // center frequency [Hz]
// #define RADIOCONFIG_MODULATION          MODEM_LORA   // modem (options: MODEM_LORA, MODEM_FSK)
// #define RADIOCONFIG_DATARATE            7            // datarate (FSK: bits/s, LoRa: spreading-factor)
// #define RADIOCONFIG_BANDWIDTH           0            // bandwidth (LoRa: 0 = 125 kHz, FSK: bandwidth in Hz)
// #define RADIOCONFIG_CODERATE            1
// #define RADIOCONFIG_PREAMBLE_LEN        10           // preamble length (LoRa: num symbols, FSK: num bytes)
// #define RADIOCONFIG_IMPLICIT_HEADER     0            // implicit header (LoRa only)
// #define RADIOCONFIG_CRC_ON              1            // CRC enabled

/* mod7 (SF5) */
#define RADIOCONFIG_TX_POWER            -9           // transmit power [dBm] (valid range: -9...+22)
#define RADIOCONFIG_FREQUENCY           865440000    // center frequency [Hz]
#define RADIOCONFIG_MODULATION          MODEM_LORA   // modem (options: MODEM_LORA, MODEM_FSK)
#define RADIOCONFIG_DATARATE            5            // datarate (FSK: bits/s, LoRa: spreading-factor)
#define RADIOCONFIG_BANDWIDTH           0            // bandwidth (LoRa: 0 = 125 kHz, FSK: bandwidth in Hz)
#define RADIOCONFIG_CODERATE            1
#define RADIOCONFIG_PREAMBLE_LEN        12           // preamble length (LoRa: num symbols, FSK: num bytes)
#define RADIOCONFIG_IMPLICIT_HEADER     0            // implicit header (LoRa only)
#define RADIOCONFIG_CRC_ON              1            // CRC enabled

/* mod0 (SF12) */
// #define RADIOCONFIG_TX_POWER            14           // transmit power [dBm] (valid range: -9...+22)
// #define RADIOCONFIG_FREQUENCY           865440000    // center frequency [Hz]
// #define RADIOCONFIG_MODULATION          MODEM_LORA   // modem (options: MODEM_LORA, MODEM_FSK)
// #define RADIOCONFIG_DATARATE            12           // datarate (FSK: bits/s, LoRa: spreading-factor)
// #define RADIOCONFIG_BANDWIDTH           0            // bandwidth (LoRa: 0 = 125 kHz, FSK: bandwidth in Hz)
// #define RADIOCONFIG_CODERATE            1
// #define RADIOCONFIG_PREAMBLE_LEN        10           // preamble length (LoRa: num symbols, FSK: num bytes)
// #define RADIOCONFIG_IMPLICIT_HEADER     0            // implicit header (LoRa only)
// #define RADIOCONFIG_CRC_ON              1            // CRC enabled

// /* mod8 (FSK 125kHz) */
// #define RADIOCONFIG_TX_POWER            14           // transmit power [dBm] (valid range: -9...+22)
// #define RADIOCONFIG_FREQUENCY           869012500    // center frequency [Hz]
// #define RADIOCONFIG_MODULATION          MODEM_FSK    // modem (options: MODEM_LORA, MODEM_FSK)
// #define RADIOCONFIG_DATARATE            125000       // datarate (FSK: bits/s, LoRa: spreading-factor)
// #define RADIOCONFIG_BANDWIDTH           234300       // bandwidth (LoRa: 0 = 125 kHz, FSK: bandwidth in Hz)
// #define RADIOCONFIG_CODERATE            0
// #define RADIOCONFIG_PREAMBLE_LEN        2            // preamble length (LoRa: num symbols, FSK: num bytes)
// #define RADIOCONFIG_IMPLICIT_HEADER     0            // implicit header (LoRa only)
// #define RADIOCONFIG_CRC_ON              1            // CRC enabled

/* mod10 (FSK 250kHz) */
// #define RADIOCONFIG_TX_POWER            14           // transmit power [dBm] (valid range: -9...+22)
// #define RADIOCONFIG_FREQUENCY           869012500    // center frequency [Hz]
// #define RADIOCONFIG_MODULATION          MODEM_FSK    // modem (options: MODEM_LORA, MODEM_FSK)
// #define RADIOCONFIG_DATARATE            125000       // datarate (FSK: bits/s, LoRa: spreading-factor)
// #define RADIOCONFIG_BANDWIDTH           234300       // bandwidth (LoRa: 0 = 125 kHz, FSK: bandwidth in Hz)
// #define RADIOCONFIG_CODERATE            0
// #define RADIOCONFIG_PREAMBLE_LEN        2            // preamble length (LoRa: num symbols, FSK: num bytes)
// #define RADIOCONFIG_IMPLICIT_HEADER     0            // implicit header (LoRa only)
// #define RADIOCONFIG_CRC_ON              1            // CRC enabled

// /* mod10 (FSK 250kHz) */
// #define RADIOCONFIG_TX_POWER            0            // transmit power [dBm] (valid range: -9...+22)
// #define RADIOCONFIG_FREQUENCY           869012500    // center frequency [Hz]
// #define RADIOCONFIG_MODULATION          MODEM_FSK    // modem (options: MODEM_LORA, MODEM_FSK)
// #define RADIOCONFIG_DATARATE            250000       // datarate (FSK: bits/s, LoRa: spreading-factor)
// #define RADIOCONFIG_BANDWIDTH           312000       // bandwidth (LoRa: 0 = 125 kHz, FSK: bandwidth in Hz)
// #define RADIOCONFIG_CODERATE            0
// #define RADIOCONFIG_PREAMBLE_LEN        4            // preamble length (LoRa: num symbols, FSK: num bytes)
// #define RADIOCONFIG_IMPLICIT_HEADER     0            // implicit header (LoRa only)
// #define RADIOCONFIG_CRC_ON              1            // CRC enabled


/* debugging ******************************************************************/
#define CPU_ON_IND()                    //PIN_SET(COM_GPIO2)  /* pin to indicate activity (e.g. to calculate the duty cycle) */
#define CPU_OFF_IND()                   //PIN_CLR(COM_GPIO2)
#define GLORIA_START_IND()              led_on(LED_SYSTEM); \
                                        FLOCKLAB_PIN_SET(FLOCKLAB_INT2);
#define GLORIA_STOP_IND()               led_off(LED_SYSTEM); \
                                        FLOCKLAB_PIN_CLR(FLOCKLAB_INT2);
#define RADIO_TX_START_IND()            FLOCKLAB_PIN_SET(FLOCKLAB_LED2);
#define RADIO_TX_STOP_IND()             FLOCKLAB_PIN_CLR(FLOCKLAB_LED2);
#define RADIO_RX_START_IND()            FLOCKLAB_PIN_SET(FLOCKLAB_LED3);
#define RADIO_RX_STOP_IND()             FLOCKLAB_PIN_CLR(FLOCKLAB_LED3);


#endif /* CONFIG_H_ */
