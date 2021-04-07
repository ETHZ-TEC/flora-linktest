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


#ifndef LINKTEST_H_
#define LINKTEST_H_

void linktest_init(uint32_t *slotTime);
void linktest_round_pre(uint8_t roundIdx);
void linktest_round_post(uint8_t roundIdx);
void linktest_slot(uint8_t roundIdx, uint16_t slotIdx, TickType_t slotStartTs);


void linktest_set_tx_config_lora(void);
void linktest_set_tx_config_fsk(void);
void linktest_set_rx_config_lora(void);
void linktest_set_rx_config_fsk(void);
void linktest_radio_init(void);
void linktest_radio_irq_capture_callback(void);
void linktest_check_radio_status(bool restart_rx);
extern void (*RadioOnDioIrqCallback)(void);
extern const struct Radio_s Radio;

void linktest_OnRadioCadDone(_Bool detected);
void linktest_OnRadioRxDone(uint8_t* payload, uint16_t size, int16_t rssi, int8_t snr, bool crc_error);
void linktest_OnRadioTxDone(void);
void linktest_OnRxSync(void);
void linktest_Dummy(void);

typedef struct {
  uint16_t counter;
  char key[254];
} linktest_message_t;

#define LINKTEST_IRQ_MASK   (IRQ_HEADER_VALID | IRQ_SYNCWORD_VALID | IRQ_RX_DONE | IRQ_TX_DONE | IRQ_HEADER_ERROR | IRQ_CRC_ERROR)

#endif /* LINKTEST_H_ */
