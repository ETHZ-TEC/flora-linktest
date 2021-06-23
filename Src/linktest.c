/*
 * Copyright (c) 2019 - 2021, ETH Zurich, Computer Engineering Group (TEC)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * @author Roman Trub
 * @brief  Linktest helper functions
 */

#include "main.h"


/* Global variables */
RadioEvents_t radioEvents;


/******************************************************************************
 * Functions
 ******************************************************************************/



/******************************************************************************
 * Radio Callbacks
 ******************************************************************************/

void linktest_radio_irq_capture_callback(void) {
  // Execute Radio driver callback
  (*RadioOnDioIrqCallback)();
  Radio.IrqProcess();
}

void linktest_OnRadioCadDone(_Bool detected) {
  /* nothing to do */
}

void linktest_Dummy(void) {
  /* nothing to do */
}

void linktest_OnRxSync(void) {
  // LOG_INFO("RxSync");
}

void linktest_OnRadioTxDone(void) {
  /* TxDone callback from the radio */
  LOG_INFO("{\"type\": \"TxDone\"}");
}

void linktest_OnRadioRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr, bool crc_error) {
  /* RxDone callback from the radio */
  linktest_message_t *msg = (linktest_message_t*) payload;
  /* replace all invalid characters in the payload */
  uint32_t i;
  for (i = sizeof(msg->counter); i < size; i++) {
    if (payload[i] < 33 || payload[i] > 126 || payload[i] == '\\' || payload[i] == '"') {
      payload[i] = '?';
    }
  }
  /* make sure the string is terminated by a zero at the end */
  payload[size] = 0;
  LOG_INFO( "{\"type\":\"RxDone\","
            "\"key\":\"%s\","
            "\"size\":%d,"
            "\"counter\":%d,"
            "\"rssi\":%d,"
            "\"snr\":%d,"
            "\"crc_error\":%d}",
    msg->key,
    size,
    msg->counter,
    rssi,
    snr,
    crc_error
  );
}

/******************************************************************************
 * Radio Config Functions
 ******************************************************************************/

  void linktest_radio_init(void) {
    if (RADIO_READ_DIO1_PIN()) {
      LOG_WARNING("SX1262 DIO1 pin is high");
    }

    radioEvents.CadDone   = &linktest_OnRadioCadDone;
    radioEvents.RxDone    = &linktest_OnRadioRxDone;
    radioEvents.RxError   = &linktest_Dummy;
    radioEvents.RxTimeout = &linktest_Dummy;
    radioEvents.TxDone    = &linktest_OnRadioTxDone;
    radioEvents.TxTimeout = &linktest_Dummy;
    radioEvents.RxSync    = &linktest_OnRxSync;
    Radio.Init(&radioEvents);

    /* Setup callback for radio interrupts */
    hs_timer_capture(&linktest_radio_irq_capture_callback);

    // sets the center frequency and performs image calibration if necessary -> can take up to 2ms (see datasheet p.56)
    // NOTE: Image calibration is valid for the whole frequency band (863 - 870MHz), no recalibration necessary if another frequency within this band is selected later on
    Radio.SetChannel(radio_bands[RADIO_DEFAULT_BAND].centerFrequency);
  }

void linktest_set_tx_config_lora(void) {
  Radio.Standby();
  Radio.SetChannel(RADIOCONFIG_FREQUENCY); // center frequency [Hz]

  Radio.SetTxConfig(
    MODEM_LORA,                  // modem (options: MODEM_LORA, MODEM_FSK)
    RADIOCONFIG_TX_POWER,        // power [dBm]
    0,                           // frequency deviation (FSK only)
    RADIOCONFIG_BANDWIDTH,       // bandwidth (0=125KHz) (LoRa only)
    RADIOCONFIG_DATARATE,        // datarate (FSK: bits/s, LoRa: spreading-factor)
    RADIOCONFIG_CODERATE,        // coderate (LoRa only)
    RADIOCONFIG_PREAMBLE_LEN,    // preamble length (FSK: num bytes, LoRa: symbols (HW adds 4 symbols))
    RADIOCONFIG_IMPLICIT_HEADER, // implicit, i.e. fixed length packets [0: variable, 1: fixed]
    RADIOCONFIG_CRC_ON,          // CRC on
    false,                       // FreqHopOn (FHSS)
    0,                           // hop period (for frequency hopping (FHSS) only
    false,                       // iqInverted
    false                        // no timeout
  );
}

void linktest_set_rx_config_lora(void) {
  Radio.Standby();
  Radio.SetChannel(RADIOCONFIG_FREQUENCY); // center frequency [Hz]

  Radio.SetRxConfig(
    MODEM_LORA,                  // modem
    RADIOCONFIG_BANDWIDTH,       // bandwidth
    RADIOCONFIG_DATARATE,        // datarate
    RADIOCONFIG_CODERATE,        // coderate
    0,                           // bandwidthAfc (not used with SX126x!)
    RADIOCONFIG_PREAMBLE_LEN,    // preambleLen
    0,                           // symbTimeout
    RADIOCONFIG_IMPLICIT_HEADER, // fixLen
    0,                           // payloadLen (only if implicit header is used)
    RADIOCONFIG_CRC_ON,          // crcOn
    false,                       // FreqHopOn
    0,                           // HopPeriod
    false,                       // iqInverted
    true                         // rxContinuous
  );
}

/*$$$*/
void linktest_set_tx_config_fsk(void) {
  // determine fdev from bandwidth and datarate
  // NOTE: according to the datasheet afc_bandwidth (automated frequency control bandwidth) variable represents the frequency error (2x crystal frequency error)
  uint32_t fdev = (RADIOCONFIG_BANDWIDTH - RADIOCONFIG_DATARATE) / 2;

  Radio.Standby();
  Radio.SetChannel(RADIOCONFIG_FREQUENCY);  // center frequency [Hz]

  Radio.SetTxConfig(
    MODEM_FSK,                 // modem (options: MODEM_LORA, MODEM_FSK)
    RADIOCONFIG_TX_POWER,      // power [dBm]
    fdev,                      // frequency deviation (FSK only)
    RADIOCONFIG_BANDWIDTH,     // bandwidth (0=125KHz) (LoRa only)
    RADIOCONFIG_DATARATE,      // datarate (FSK: bits/s, LoRa: spreading-factor)
    0,                         // coderate (LoRa only)
    RADIOCONFIG_PREAMBLE_LEN,  // preamble length (FSK: num bytes, LoRa: symbols (HW adds 4 symbols))
    false,                     // implicit, i.e. fixed length packets [0: variable, 1: fixed] (LoRa only)
    RADIOCONFIG_CRC_ON,        // CRC on
    false,                     // FreqHopOn (FHSS)
    0,                         // hop period (for frequency hopping (FHSS) only
    false,                     // iqInverted
    false                      // timeout
  );
}

void linktest_set_rx_config_fsk(void) {
  // determine fdev from bandwidth and datarate
  // NOTE: according to the datasheet afc_bandwidth (automated frequency control bandwidth) variable represents the frequency error (2x crystal frequency error)
  uint32_t afc_bandwidth = 2 * (RADIOCONFIG_BANDWIDTH / 2 + RADIOCONFIG_FREQUENCY) / RADIO_CLOCK_DRIFT;
  int32_t  bandwidth_rx  = RADIOCONFIG_BANDWIDTH + afc_bandwidth; // for rx only

  Radio.Standby();
  Radio.SetChannel(RADIOCONFIG_FREQUENCY);  // center frequency [Hz]

  Radio.SetRxConfig(
    MODEM_FSK,                // modem
    bandwidth_rx,             // bandwidth
    RADIOCONFIG_DATARATE,     // datarate
    0,                        // coderate
    0,                        // bandwidthAfc (not used with SX126x!)
    RADIOCONFIG_PREAMBLE_LEN, // preambleLen
    0,                        // symbTimeout
    false,                    // fixLen
    0,                        // payloadLen (only if implicit header is used)
    RADIOCONFIG_CRC_ON,       // crcOn
    false,                    // FreqHopOn
    0,                        // HopPeriod
    false,                    // iqInverted
    true                      // rxContinuous
  );
}
