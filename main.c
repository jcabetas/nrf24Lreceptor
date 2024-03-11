/*
 ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */
/*
    PLAY Embedded demos - Copyright (C) 2014...2019 Rocco Marco Guglielmi

    This file is part of PLAY Embedded demos.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/*
 * Funcionalidad:
 * - El receptor se queda esperando a ordenes del emisor
 * - El emisor periodicamente pide estado al receptor
 *   * el receptor envía estado del servo, bateria y nivel de señal
 *   * el emisor enseña datos propios y del receptor en display
 * - Cuando se cambia palanca, se envia orden de cierre
 *   * el receptor activa el servo y envia nuevo estado al emisor
 *
 * Necesario:
 * - Salida I2C display
 * - Entrada palanca
 * - Puerto serie, para programar (emisor/receptor, clave, posAbierto, posCerrado)
 * - Salida PWM
 */

#include "ch.h"
#include "hal.h"

#include "rf.h"
#include "string.h"
#include <stdio.h>


void cierraPuertaC(void);
void abrePuertaC(void);
uint8_t abrePuertaHastaA1C(void);
void initServo(void);
void leeVariablesC(void);
void leeTension(float *vBat);

uint8_t estadoPuerta;
float vBatJaula;

#undef  TRANSMITTER
#define  FRAME_LEN                            5

static const SPIConfig spicfg = {
  false,
  false,
  NULL,
  NULL,
  GPIOA,
  GPIOA_CS,
  SPI_CR1_BR_1 | SPI_CR1_BR_0,
  0
};


static RFConfig nrf24l01_cfg = {
  .line_ce          = LINE_CE,
  .line_irq         = LINE_IRQ,
  .spip             = &SPID1,
  .spicfg           = &spicfg,
  .auto_retr_count  = NRF24L01_ARC_15_times,
  .auto_retr_delay  = NRF24L01_ARD_4000us,
  .address_width    = NRF24L01_AW_5_bytes,
  .channel_freq     = 120,
  .data_rate        = NRF24L01_ADR_2Mbps,
  .out_pwr          = NRF24L01_PWR_0dBm,
  .lna              = NRF24L01_LNA_disabled,
  .en_dpl           = NRF24L01_DPL_enabled,
  .en_ack_pay       = NRF24L01_ACK_PAY_disabled,
  .en_dyn_ack       = NRF24L01_DYN_ACK_disabled
};

/*===========================================================================*/
/* Generic code.                                                             */
/*===========================================================================*/


void parpadear(uint8_t numVeces, uint16_t ms) {
    palSetLineMode(LINE_LED, PAL_MODE_OUTPUT_PUSHPULL);
    for (uint8_t i = 0; i < numVeces; i++) {
        palClearLine(LINE_LED); // enciende led placa
        chThdSleepMilliseconds(30);
        palSetLine(LINE_LED); // apaga led placa
        chThdSleepMilliseconds(ms);
    }
    palSetLineMode(LINE_LED, PAL_MODE_INPUT_ANALOG);
}


//            01234567890
// respuesta "1 12.34 100"
//            P   V   V%
/*


uint8_t estadoPuerta;  // abierto:0 cerrado:1
float vBatJaula;
uint8_t porcBatJaula;
 */
void encodeStatus(char *buffer, uint8_t sizeofBuffer)
{
    if (sizeofBuffer<15)
        return;
    snprintf(buffer,sizeofBuffer, "%1d%5d%4d",estadoPuerta,(uint16_t)(100*vBatJaula),80);
}



/*
 * Application entry point.
 */
int main(void) {
    char string[15];
    char buffer[15];
    uint8_t estadoDeseado;
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  parpadear(3,200);
  leeVariablesC();

  leeTension(&vBatJaula);

  // test servo
  initServo();
  cierraPuertaC();
  estadoPuerta = 1;

  /*
   * SPID1 I/O pins setup.(It bypasses board.h configurations)
   */
  palSetLineMode(LINE_SPI1SCK, PAL_MODE_ALTERNATE(0) | PAL_STM32_OSPEED_HIGHEST);
  palSetLineMode(LINE_SPI1MISO, PAL_MODE_ALTERNATE(0) | PAL_STM32_OSPEED_HIGHEST);
  palSetLineMode(LINE_SPI1MOSI, PAL_MODE_ALTERNATE(0) | PAL_STM32_OSPEED_HIGHEST);
  palSetLineMode(LINE_CS, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
  /*
   * CE and IRQ pins setup.
   */
  palSetLineMode(LINE_CE, PAL_MODE_OUTPUT_PUSHPULL | PAL_STM32_OSPEED_HIGHEST);
  palSetLineMode(LINE_IRQ, PAL_MODE_INPUT | PAL_STM32_OSPEED_HIGHEST);

  /* Starting Serial Driver 2 with default configurations. */
  sdStart(&SD2, NULL);

  /* RF Driver Object constructor. */
  rfInit();

  /* Starting RF driver. */
  rfStart(&RFD1, &nrf24l01_cfg);

  while (TRUE) {
    string[0] = '\0';
    estadoDeseado = 3;
    rf_msg_t msg = rfReceiveString(&RFD1, string, "RXadd", TIME_MS2I(20000));
    if (msg == RF_OK)
    {
        //      "PUERTA:%d"
        if (strstr(string,"PUERTA:"))
        {
            char estadoPuertaC = string[7];
            if (estadoPuertaC == '0')
                estadoDeseado = 0;
            if (estadoPuertaC == '1')
                estadoDeseado = 1;
            if (estadoDeseado != 3)
                estadoPuerta = estadoDeseado;
            encodeStatus(buffer,sizeof(buffer));
            msg = rfTransmitString(&RFD1, buffer, "RXadd", TIME_MS2I(75));
        }
        else if (strstr(string,"ESTADO"))
        {
            encodeStatus(buffer,sizeof(buffer));
            msg = rfTransmitString(&RFD1, buffer, "RXadd", TIME_MS2I(75));
        }
        if (estadoDeseado == 0)
            abrePuertaC();
        else if (estadoDeseado == 1)
            cierraPuertaC();
        parpadear(1,10);
    }
    else
        parpadear(2,50);
  }
  rfStop(&RFD1);
}
