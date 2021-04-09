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
 * Helper Functions
 ******************************************************************************/
void linktest_sanitize_string(char *payload, uint8_t size) {
  uint32_t i;
  for (i = 0; i < size; i++) {
    if (payload[i] < 33 || payload[i] > 126 || payload[i] == '\\' || payload[i] == '"') {
      payload[i] = '?';
    }
  }
}

/******************************************************************************
 * Linktest with point-to-point (P2P) transmissions
 ******************************************************************************/
#if TESTCONFIG_P2P_MODE

void linktest_init(uint32_t *slotTime) {
  linktest_radio_init();

  // /* Calculate SlotTime */
  linktest_message_t msg = {
    .counter=0,
    .key=TESTCONFIG_KEY,
  };
  uint16_t key_length = 0;
  key_length = strlen(msg.key);
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

#endif /* TESTCONFIG_P2P_MODE */

/******************************************************************************
 * Linktest with flood transmissions
 ******************************************************************************/
#if TESTCONFIG_FLOOD_MODE

static uint32_t flood_time = 0;
static linktest_message_t msg_tx = {
  .counter=0,
  .key=TESTCONFIG_KEY,
};
static uint16_t payload_len_tx;

void linktest_init(uint32_t *slotTime) {
  gloria_set_band(FLOODCONFIG_RF_BAND);
  gloria_set_tx_power(FLOODCONFIG_TX_POWER);
  gloria_set_modulation(FLOODCONFIG_MODULATION);

  // calc flood and slot time
  uint16_t key_length = strlen(msg_tx.key);
  key_length = (key_length > 254) ? 254 : key_length;
  payload_len_tx = sizeof(msg_tx.counter) + key_length;
  flood_time = gloria_get_flood_time(payload_len_tx, FLOODCONFIG_N_TX + FLOODCONFIG_NUM_HOPS - 1) / 1000; // gloria_get_flood_time returns us, we need ms
  *slotTime = flood_time + 2*FLOODCONFIG_FLOOD_GAP;
}

void linktest_round_pre(uint8_t roundIdx) {
  // nothing to do here
}

void linktest_round_post(uint8_t roundIdx) {
  // nothing to do here
}

void linktest_slot(uint8_t roundIdx, uint16_t slotIdx, uint32_t slotStartTs) {
  bool is_initiator = false;
#if FLOODCONFIG_DELAY_TX==0
  // no delayed retransmissions (every node is initiator in the corresponding round)
  is_initiator = (TESTCONFIG_NODE_LIST[roundIdx] == NODE_ID);
#else
  // delay retransmissions on a single node (the node which corresponds to the current round, except initiator)
  if (TESTCONFIG_NODE_LIST[roundIdx] == NODE_ID && NODE_ID!=FLOODCONFIG_INITIATOR) {
    gloria_set_tx_delay(FLOODCONFIG_DELAY_TX);
  }
  // fixed initiator
  is_initiator = (FLOODCONFIG_INITIATOR == NODE_ID);
#endif /* FLOODCONFIG_DELAY_TX==0 */

  if (is_initiator) {
    /* Node is sending in this round */

    // wait FLOODCONFIG_FLOOD_GAP
    vTaskDelayUntil(&slotStartTs, pdMS_TO_TICKS(FLOODCONFIG_FLOOD_GAP));

    msg_tx.counter = slotIdx;
    gloria_start(
      is_initiator,                      // is_initiator
      (uint8_t*) &msg_tx,                // *payload
      payload_len_tx,                    // payload_len
      FLOODCONFIG_N_TX,                  // n_tx_max
      true                               // sync_slot
    );

    // wait flood_time
    vTaskDelayUntil(&slotStartTs, pdMS_TO_TICKS(flood_time));

    gloria_stop();
    linktest_print_flood_stats(true, &msg_tx);
  }
  else {
    /* Node is receiving in this round */
    uint8_t msg_rx[GLORIA_INTERFACE_MAX_PAYLOAD_LEN];
    memset(msg_rx, 0, GLORIA_INTERFACE_MAX_PAYLOAD_LEN);
    gloria_start(
      is_initiator,                      // is_initiator
      msg_rx,                            // *payload
      GLORIA_INTERFACE_MAX_PAYLOAD_LEN,  // payload_len
      FLOODCONFIG_N_TX,                  // n_tx_max
      true                               // sync_slot
    );

    vTaskDelayUntil(&slotStartTs, pdMS_TO_TICKS(2*FLOODCONFIG_FLOOD_GAP + flood_time));

    gloria_stop();
    linktest_print_flood_stats(false, (linktest_message_t*) msg_rx);
  }
}

void linktest_print_flood_stats(bool is_initiator, linktest_message_t* msg)
{
    uint8_t  rx_cnt        = gloria_get_rx_cnt();
    uint8_t  rx_started    = gloria_get_rx_started_cnt();
    uint8_t  rx_idx        = 0;
    int8_t   snr           = -99;
    int16_t  rssi          = -99;
    uint8_t  payload_len   = 0;
    uint8_t  t_ref_updated = 0;
    // uint64_t t_ref         = 0;

    if (rx_cnt > 0) {
      rx_idx         = gloria_get_rx_index();
      snr            = gloria_get_snr();
      rssi           = gloria_get_rssi();
      payload_len    = gloria_get_payload_len();
      t_ref_updated  = gloria_is_t_ref_updated();
      // t_ref          = gloria_get_t_ref();
    }

    if (payload_len > sizeof(msg->counter)) {
      linktest_sanitize_string(msg->key, payload_len - sizeof(msg->counter));
    }

    /* print in json format */
    LOG_INFO("{"
             "\"type\":\"FloodDone\","
             "\"is_initiator\":%d,"
             "\"rx_cnt\":%d,"
             "\"rx_idx\":%d,"
             "\"rx_started\":%d,"
             "\"rssi\":%d,"
             "\"snr\":%d,"
             "\"payload_len\":%d,"
             "\"t_ref_updated\":%d,"
             // "\"t_ref\":%llu," // requires 64bit printf support (needs to be enabled in IDE Project Properties -> C/C++ Build -> Settings)
             "\"msg_counter\":%u,"
             "\"msg_key\":\"%s\""
             "}",
      is_initiator,
      rx_cnt,
      rx_idx,
      rx_started,
      rssi,
      snr,
      payload_len,
      t_ref_updated,
      // t_ref,
      msg->counter,
      payload_len > sizeof(msg->counter) ? msg->key : ""
    );
}

#endif /* TESTCONFIG_FLOOD_MODE */

/******************************************************************************
 * Radio Callbacks
 ******************************************************************************/
#if TESTCONFIG_P2P_MODE

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
  linktest_sanitize_string(msg->key, size - sizeof(msg->counter));
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

#endif /* TESTCONFIG_P2P_MODE */

/******************************************************************************
 * Radio Config Functions
 ******************************************************************************/
#if TESTCONFIG_P2P_MODE

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

#endif /* TESTCONFIG_P2P_MODE */
