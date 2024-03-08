/*
 * pwmInts.c
 *
 *  Created on: 03/06/2012, adaptado 15/11/2015 a Flip32,
 *  adaptado y muy simplificado para BlackPill 20/9/2022
 *      Author: joaquin
 *  Salida servo 1  (PWM1, PA1, TIM2_CH2, AF02)
 */

#include "hal.h"
#include "chprintf.h"

extern "C" {
    void mueveServoPosC(uint16_t porcPosicion);
    void cierraPuertaC(void);
    void abrePuertaC(void);
    uint8_t abrePuertaHastaA1C(void);
    void initServo(void);
}

extern uint16_t posAbierto;
extern uint16_t posCerrado;
uint16_t porcPosAnterior = 50;

static PWMConfig pwmcfg = {
  3200000, /* 48 MHz PWM clock frequency antes 3125000 */
    64000,   /* PWM period 20 millisecond antes 62500 */
  NULL,  /* No callback */
  /* Channel 3 enabled */
  {
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_ACTIVE_HIGH, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
  },
  0,
  0,
  0
};

bool tensionCritica(void);


void initServo(void)
{
/*
 *   Salida servo: TIM2CH3 (PA2)
 */
    palSetLineMode(LINE_ONSERVO, PAL_MODE_OUTPUT_PUSHPULL);
    palClearLine(LINE_ONSERVO);
    chThdSleepMilliseconds(100);
    palSetLine(LINE_ONSERVO);


    palSetLineMode(LINE_PWM, PAL_MODE_OUTPUT_PUSHPULL);
    palClearLine(LINE_PWM);
    chThdSleepMilliseconds(100);
    palSetLine(LINE_PWM);


    palSetLineMode(LINE_PWM, PAL_MODE_ALTERNATE(2) | PAL_STM32_OSPEED_HIGHEST);
    palSetLine(LINE_ONSERVO);
    pwmStart(&PWMD2, &pwmcfg);
}

void closeServo(void)
{
    palSetLineMode(LINE_PWM,PAL_MODE_INPUT_ANALOG );
    palClearLine(LINE_ONSERVO);
    pwmStop(&PWMD2);
}


void mueveServoPos(uint16_t porcPosicion)
{
  uint16_t ancho;
  palSetLine(LINE_ONSERVO);
  if (PWMD2.state==PWM_STOP)
      pwmStart(&PWMD2, &pwmcfg);
  if (porcPosicion>100) porcPosicion=100;
  ancho = (uint16_t) (2700.0f+3800.0f*porcPosicion/100.0f);
  // minimo 3200, maximo 6400, medio 4800
  if (ancho<2900) ancho=2900;
  if (ancho>6700) ancho=6700;
  pwmEnableChannel(&PWMD2, 2, ancho);
}

void mueveServoPosC(uint16_t porcPosicion)
{
    mueveServoPos(porcPosicion);
}

void cierraPuertaC(void)
{
   mueveServoPos(posCerrado);
}

void abrePuertaC(void)
{
    mueveServoPos(posAbierto);
}

void cierraPuerta(void)
{
    cierraPuertaC();
}


