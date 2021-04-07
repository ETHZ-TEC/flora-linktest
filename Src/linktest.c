/**
  ******************************************************************************
  * Flora
  ******************************************************************************
  * @author Roman Trub
  * @brief  Linktest helper functions
  *
  *
  ******************************************************************************
  */



#include "main.h"


/* Global variables */
RadioEvents_t radioEvents;

const uint16_t TESTCONFIG_NODE_LIST[TESTCONFIG_NUM_NODES] = {
  TESTCONFIG_NODE_IDS
};


/******************************************************************************
 * Linktest Functions
 ******************************************************************************/

void linktest_init(uint32_t *slotTime) {
  // /* Calculate SlotTime */
  linktest_message_t msg = {
    .counter=0,
    .key=TESTCONFIG_KEY,
  };
  uint16_t key_length = 0;
  key_length = strlen(msg.key)+1;
  key_length = (key_length > 254) ? 254 : key_length;
  *slotTime = Radio.TimeOnAir(
    RADIOCONFIG_MODULATION,
    RADIOCONFIG_BANDWIDTH,
    RADIOCONFIG_DATARATE,
    RADIOCONFIG_CODERATE,
    RADIOCONFIG_PREAMBLE_LEN,
    0,  // fixLen
    sizeof(msg.counter) + key_length,
    RADIOCONFIG_CRC_ON
  )/1000; // TimeOnAir returns us, we need ms

  // workaround for no successful rx in FSK mode if no Tx happened before
  // simple Radio.Send alone with 0 size payload does not work
  if (RADIOCONFIG_MODULATION == MODEM_FSK) {
    linktest_set_tx_config_fsk();
    Radio.Standby();
    Radio.SendPayload((uint8_t*) &msg, sizeof(msg.counter) + key_length);
    // Radio.Send((uint8_t*) &msg, 0);
  }
}

void linktest_round_pre(uint8_t roundIdx) {
  // set radio config (fixed for entire round)
  if (RADIOCONFIG_MODULATION == MODEM_LORA) {
   linktest_set_tx_config_lora();
   linktest_set_rx_config_lora();
  }
  else {
   linktest_set_tx_config_fsk();
   linktest_set_rx_config_fsk();
  }
  Radio.Standby();

  if (TESTCONFIG_NODE_LIST[roundIdx] != NODE_ID) {
    /* Node is receiving in this round */

    // start rx mode (with deactivated preamble IRQs)
    Radio.RxBoostedMask(LINKTEST_IRQ_MASK, 0, true, false);
  }
}

void linktest_round_post(uint8_t roundIdx) {
  Radio.Standby(); // required for Rx, no harm for Tx
}

void linktest_slot(uint8_t roundIdx, uint16_t slotIdx, uint32_t slotStartTs) {
  if (TESTCONFIG_NODE_LIST[roundIdx] == NODE_ID) {
    /* Node is transmitting in this round */

    // send
    static linktest_message_t msg = {
      .counter=0,
      .key=TESTCONFIG_KEY,
    };
    msg.counter = slotIdx;
    uint16_t key_length = strlen(msg.key);
    key_length  = (key_length > 254) ? 254 : key_length;
    Radio.SendPayload((uint8_t*) &msg, sizeof(msg.counter) + key_length);
  } else {
    /* Node is receiving in this round */
    // nothing to do
  }
}

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
    false                        // iqInverted
  );
}

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
  int32_t bandwidth_rx = radio_get_rx_bandwidth(RADIOCONFIG_FREQUENCY, RADIOCONFIG_BANDWIDTH);

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
    false                     // iqInverted
  );
}
