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

#ifndef LINKTEST_H_
#define LINKTEST_H_

#ifndef FLOODCONFIG_DELAY_TX
#define FLOODCONFIG_DELAY_TX          0
#endif /* FLOODCONFIG_DELAY_TX */

typedef struct {
  uint16_t counter;
  char key[254];
} linktest_message_t;

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

void linktest_sanitize_string(char *payload, uint8_t size);
void linktest_print_flood_stats(bool is_initiator, linktest_message_t* msg);

#define LINKTEST_IRQ_MASK   (IRQ_HEADER_VALID | IRQ_SYNCWORD_VALID | IRQ_RX_DONE | IRQ_TX_DONE | IRQ_HEADER_ERROR | IRQ_CRC_ERROR)

#endif /* LINKTEST_H_ */
