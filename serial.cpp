/*
 * serial.cpp
 *
 *  Created on: 9 dic. 2022
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "tty.h"
#include "gets.h"
#include "string.h"
#include <stdlib.h>
#include "chprintf.h"
#include "calendarUTC.h"
#include <w25q16/w25q16.h>
#include "variables.h"
#include "version.h"
#include "flashVars.h"

extern "C" {
    void initSerial(void);
    void closeSerial(void);
    void opciones(void);
    void printSerial(const char *msg);
}

void ajustaP(uint16_t porcP);
uint8_t ajustaHoraDetallada(uint16_t ano, uint8_t mes, uint8_t dia, uint8_t hora, uint8_t min, uint8_t sec);
void diSecAmanecerAnochecer(uint16_t *secActual, uint16_t *secAmanecer, uint16_t *secAnochecer);
void estadoDeseadoPuertaC(uint8_t *estDes, uint16_t *sec2change);
void mueveServoPos(uint16_t porcPosicion);
void leeTension(float *vBat);
bool tensionCritica(void);
float hallaCapBat(float *vBat);
void ajustaCALMP(int16_t dsDia);
uint8_t startGPS(void);
void calibraConGPS(uint8_t soloCheck);
uint8_t stopGPS(void);

extern "C" {
    void cierraPuertaC(void);
    void abrePuertaC(void);
}

extern uint8_t hayGps;

extern int16_t addAmanecer;
extern int16_t addAtardecer;
extern uint16_t autoPuerta;  // 0:cerrada, 1:abierta, 2: automatico, 3: autoConMargen
extern uint32_t secAdaptacion;
extern uint16_t diaAdaptado;
extern int16_t dsAddPordia;
extern uint16_t posAbierto;
extern uint16_t posCerrado;
extern uint8_t hayW25q16;

extern uint16_t pObjetivo;
extern uint16_t hayTension;
extern int16_t incAdPormil;
uint8_t puertoAbierto = 0, puertoAbiertoHM10 = 0;

const char *estPuertaAutoStr[] = {"cerrada", "abierta","abierta de noche","abierta adaptando gatos"};
const char *estPuertaStr[] = {"cerrada", "abierta"};
static const SerialConfig ser_cfg = {9600, 0, 0, 0, };//{115200, 0, 0, 0, };


// Los mensajes cortos se sacan siempre en ttyPin (&SD2)
// Para las opciones se activa HM10 y se usa &SD2 (en EE2) o &SD1 (en EE3)
// luego se desactiva HM10 al salir

void initSerial(void) {
    if (puertoAbierto==1)
        return;
    palClearLine(LINE_TXSERIAL);
    palSetLine(LINE_RXSERIAL);
    palSetLineMode(LINE_RXSERIAL, PAL_MODE_ALTERNATE(7));
    palSetLineMode(LINE_TXSERIAL, PAL_MODE_ALTERNATE(7));
    sdStart(ttyPin, &ser_cfg);
    puertoAbierto = 1;
}

void closeSerial(void) {
    palSetLineMode(LINE_RXSERIAL, PAL_MODE_INPUT_ANALOG);
    palSetLineMode(LINE_TXSERIAL, PAL_MODE_INPUT_ANALOG);
    if (puertoAbierto==0)
        return;
    sdStop(ttyPin);
    puertoAbierto = 0;
}

void printSerial(const char *msg)
{
//    palSetLineMode(LINEONSERIAL, PAL_MODE_OUTPUT_PUSHPULL);
//    ACTIVAPAD(LINEONSERIAL);
    initSerial();
    chThdSleepMilliseconds(10);
    chprintf((BaseSequentialStream *) ttyPin,msg);
    chThdSleepMilliseconds(20+strlen(msg));
    closeSerial();
//    QUITAPAD(LINEONSERIAL);
}

void initSerialHM10(void) {
    if (puertoAbiertoHM10==1)
        return;
    palSetLineMode(LINEONSERIALHM10, PAL_MODE_OUTPUT_PUSHPULL);
    ACTIVAPAD(LINEONSERIALHM10);
    chThdSleepMilliseconds(10); // para que el HM-10 se ponga en marcha
    palClearLine(LINE_TXHM10SERIAL);
    palSetLine(LINE_RXHM10SERIAL);
    palSetLineMode(LINE_RXHM10SERIAL, PAL_MODE_ALTERNATE(7));
    palSetLineMode(LINE_TXHM10SERIAL, PAL_MODE_ALTERNATE(7));
    sdStart(ttyHM10, &ser_cfg);
    puertoAbiertoHM10 = 1;
}

void closeSerialHM10(void) {
    palSetLineMode(LINE_RXHM10SERIAL, PAL_MODE_INPUT_ANALOG);
    palSetLineMode(LINE_TXHM10SERIAL, PAL_MODE_INPUT_ANALOG);
    QUITAPAD(LINEONSERIALHM10);
    if (puertoAbiertoHM10==0)
        return;
    sdStop(ttyHM10);
    puertoAbiertoHM10 = 0;
}



void printSerialCPP(const char *msg)
{
  printSerial(msg);
}

void ajustaPuerta(void)
{
    int16_t result;
    int16_t opcion;
    uint8_t estDes;
    uint32_t sec2change;
    chprintf((BaseSequentialStream *)ttyHM10,"Estado de la puerta: %s\n",estPuertaAutoStr[autoPuerta]);
    uint16_t autoPuertaOld = autoPuerta;
    while (1==1)
    {
        opcion = autoPuerta;
        chprintf((BaseSequentialStream *)ttyHM10,"0 Cerrada siempre (OJO)\n");
        chprintf((BaseSequentialStream *)ttyHM10,"1 Abierta siempre\n");
        chprintf((BaseSequentialStream *)ttyHM10,"2 Abrir y cerrar con el sol\n");
        chprintf((BaseSequentialStream *)ttyHM10,"3 Idem con adaptacion\n");
        chprintf((BaseSequentialStream *)ttyHM10,"4 Salir\n");
        result = preguntaNumero((BaseChannel *) ttyHM10, "Dime opcion", &opcion, 0, 4);
        if (result==2 || (result==0 && opcion==4))
            return;
        if (result==0 && opcion!=autoPuertaOld && opcion>=0 && opcion<=3)
        {
          if (autoPuerta!=3 && opcion==3) // si cambio a adaptacion, reinicializa
              calendar::iniciaSecAdaptacion();
          autoPuerta = opcion;
          if (autoPuerta!=3)
            secAdaptacion = 0;
          escribeVariables();
          calendar::estadoDeseadoPuerta(&estDes, &sec2change);
          if (estDes == 1)
              abrePuertaC();//mueveServoPos(posAbierto);
          else
              cierraPuertaC();//mueveServoPos(posCerrado);
          break;
        }
    }
}


void ajustaPosiciones(void)
{
    int16_t result;
    int16_t opcion;
    int16_t posicion;
    uint8_t estDes;
    uint32_t sec2change;
    while (1==1)
    {
        chprintf((BaseSequentialStream *)ttyHM10,"Posicion servo abierto: %d, cerrado: %d\n",posAbierto, posCerrado);
        chprintf((BaseSequentialStream *)ttyHM10,"1 Posicion abierto\n");
        chprintf((BaseSequentialStream *)ttyHM10,"2 Posicion cerrado\n");
        chprintf((BaseSequentialStream *)ttyHM10,"3 Salir\n");
        result = preguntaNumero((BaseChannel *) ttyHM10, "Dime opcion", &opcion, 1, 3);
        chprintf((BaseSequentialStream *)ttyHM10,"\n");
        if (result==2 || (result==0 && opcion==3))
            return;
        if (result==0 && opcion==1)
        {
            posicion = posAbierto;
            result = preguntaNumero((BaseChannel *) ttyHM10, "Posicion abierto", &posicion, 0, 100);
            if (result == 0)
            {
                posAbierto = posicion;
                escribeVariables();
            }
        }
        if (result==0 && opcion==2)
        {
            posicion = posCerrado;
            result = preguntaNumero((BaseChannel *) ttyHM10, "Posicion cerrado", &posicion, 0, 100);
            if (result == 0)
            {
                posCerrado = posicion;
                escribeVariables();
            }
        }
        calendar::estadoDeseadoPuerta(&estDes, &sec2change);
        if (estDes == 1)
            mueveServoPos(posAbierto);
        else
            mueveServoPos(posCerrado);
    }
}

void ajustaAddMinutos(void)
{
    int16_t result;
    int16_t opcion;
    int16_t addMin;
    while (1==1)
    {
        chprintf((BaseSequentialStream *)ttyHM10,"Minutos adcionales amanecer: %d atardecer: %d\n",addAmanecer, addAtardecer);
        chprintf((BaseSequentialStream *)ttyHM10,"1 Adicionales al amanecer\n");
        chprintf((BaseSequentialStream *)ttyHM10,"2 Adicionales al atardecer\n");
        chprintf((BaseSequentialStream *)ttyHM10,"3 Salir\n");
        result = preguntaNumero((BaseChannel *) ttyHM10, "Dime opcion", &opcion, 1, 3);
        chprintf((BaseSequentialStream *)ttyHM10,"\n");
        if (result==2 || (result==0 && opcion==3))
            return;
        if (result==0 && opcion==1)
        {
            addMin = addAmanecer;
            result = preguntaNumero((BaseChannel *) ttyHM10, "Minutos adicionales al amanecer", &addMin, -300, 300);
            if (result == 0)
            {
                addAmanecer = addMin;
                escribeVariables();
            }
        }
        if (result==0 && opcion==2)
        {
            addMin = addAtardecer;
            result = preguntaNumero((BaseChannel *) ttyHM10, "Minutos adicionales al atardecer", &addMin, -300, 300);
            if (result == 0)
            {
                addAtardecer = addMin;
                escribeVariables();
            }
        }
    }
}

void ajustaHora(void)
{
    int16_t ano;
    int16_t mes, dia, hora, min, sec;
    char buff[50];
    struct tm tim;
    int16_t result;
    calendar::rtcGetFecha();
    calendar::gettm(&tim);
    ano = tim.tm_year+100;
    mes = tim.tm_mon+1;
    dia = tim.tm_mday;
    hora = tim.tm_hour;
    min = tim.tm_min;
    sec = tim.tm_sec;
    if (preguntaNumero((BaseChannel *) ttyHM10, "Anyo", &ano, 2023, 2060) == 2)
        return;
    preguntaNumero((BaseChannel *) ttyHM10, "Mes", &mes, 1, 12);
    preguntaNumero((BaseChannel *) ttyHM10, "Dia", &dia, 1, 31);
    preguntaNumero((BaseChannel *) ttyHM10, "Hora", &hora, 0, 23);
    preguntaNumero((BaseChannel *) ttyHM10, "Minutos", &min, 0, 59);
    result = preguntaNumero((BaseChannel *) ttyHM10, "Segundos", &sec, 0, 59);
    if (result==2)
        return;
    ajustaHoraDetallada(ano, mes, dia, hora, min, sec);
    calendar::printFecha(buff,sizeof(buff));
    chprintf((BaseSequentialStream *)ttyHM10,"Fecha actual UTC: %s\n",buff);
}

void ajustaDsAdd(void)
{
    int16_t result;
    int16_t opcion;
    int16_t dsAdd;
    while (1==1)
    {
        chprintf((BaseSequentialStream *)ttyHM10,"Decimas de segundo dia adicionales: %d\n",dsAddPordia);
        chprintf((BaseSequentialStream *)ttyHM10,"1 Nuevo valor a mano\n");
        chprintf((BaseSequentialStream *)ttyHM10,"2 Calibrar RTC con pulsos GPS\n");
        chprintf((BaseSequentialStream *)ttyHM10,"3 Comprobar calibracion RTC con pulsos GPS\n");
        chprintf((BaseSequentialStream *)ttyHM10,"4 Salir\n");
        result = preguntaNumero((BaseChannel *) ttyHM10, "Dime opcion", &opcion, 1, 4);
        chprintf((BaseSequentialStream *)ttyHM10,"\n");
        if (result==2 || (result==0 && opcion==4))
            return;
        if (result==0 && opcion==1)
        {
            dsAdd = dsAddPordia;
            result = preguntaNumero((BaseChannel *) ttyHM10, "Decimas de segundo adicionales/dia", &dsAdd, -400, 400);
            if (result == 0)
            {
                dsAddPordia = dsAdd;
                escribeVariables();
                ajustaCALMP(dsAddPordia);
            }
        }
        if (result==0 && opcion==2)
        {
            startGPS();
            calibraConGPS(0);
            stopGPS();
            if (SD2.state==SD_STOP) // se ha usado printSerialCPP(), que cierra SD2
                initSerial();
        }
        if (result==0 && opcion==3)
        {
            startGPS();
            calibraConGPS(1);
            stopGPS();
            if (SD2.state==SD_STOP) // se ha usado printSerialCPP(), que cierra SD2
                initSerial();
        }
    }
}


void ajustaIncAD(void)
{
    int16_t result;
    int16_t opcion;
    int16_t incAD;
    float vBat;
    while (1==1)
    {
        leeTension(&vBat);
        chprintf((BaseSequentialStream *)ttyHM10,"Vbat:%.2f Add a A/D %d o/oo\n",vBat, incAdPormil);
        chprintf((BaseSequentialStream *)ttyHM10,"1 Nuevo valor\n");
        chprintf((BaseSequentialStream *)ttyHM10,"2 Salir\n");
        result = preguntaNumero((BaseChannel *) ttyHM10, "Dime opcion", &opcion, 1, 2);
        chprintf((BaseSequentialStream *)ttyHM10,"\n");
        if (result==2 || (result==0 && opcion==2))
            return;
        if (result==0 && opcion==1)
        {
            incAD = incAdPormil;
            result = preguntaNumero((BaseChannel *) ttyHM10, "Nuevo valor o/oo", &incAD, -600, 600);
            if (result == 0)
            {
                incAdPormil = incAD;
                escribeVariables();
            }
        }
    }
}


//BaseChannel *ttyHM10BC = (BaseChannel *) &SD1;
// si no esta conectado HM10, vuelve
// si hay un HM-10, le da 10 segundos para que se conecte alguien
// devuelve 0: hay un HM10, pero nadie se conecta 1: hay HM10 conectado o alguien
uint8_t esperaHM10(void)
{
    uint8_t huboTimeout;
    uint16_t dsTotal = 0;
    char buffer[10];
    do
    {
        chThdSleepMilliseconds(100);
        limpiaBuffer((BaseChannel *) ttyHM10);
        chprintf((BaseSequentialStream *)ttyHM10,"AT+BAUD1\r\n"); //19200 baud
        chprintf((BaseSequentialStream *)ttyHM10,"AT\r\n");
        // si hay HM-10, respondera con "OK" o si no con "ERROR"
        chgetsNoEchoTimeOut((BaseChannel *) ttyHM10, (uint8_t *) buffer,sizeof(buffer), TIME_MS2I(100), &huboTimeout);
        if (strstr(buffer,"OK") || strstr(buffer,"ERROR"))
        {
            dsTotal++;
            if (dsTotal>100)
                return 0;  // HM-10 sin conectar
        }
        else
        {
            if (dsTotal>0)
                return 1;  // HM-10 conectado
            else
                return 2;  // Puerta serie normal
        }
    } while (true);
}


uint8_t cambiaNombreModulo(void)
{
    uint8_t huboTimeout;
    char buffer[15];
    chprintf((BaseSequentialStream *)ttyHM10,"Nuevo nombre modulo (3-10 caracteres):");
    chgetsNoEchoTimeOut((BaseChannel *) ttyHM10, (uint8_t *) buffer,sizeof(buffer), TIME_MS2I(20000), &huboTimeout);
    chprintf((BaseSequentialStream *)ttyHM10,"\n");
    if (strlen(buffer)<3 || strlen(buffer)>10)
    {
        chprintf((BaseSequentialStream *)ttyHM10,"Longitud erronea\n");
        return 0;
    }
    chprintf((BaseSequentialStream *)ttyHM10,"Pongo nombre %s\n",buffer);
    // reseteamos el modulo
    QUITAPAD(LINEONSERIALHM10);
    chThdSleepMilliseconds(250);
    ACTIVAPAD(LINEONSERIALHM10);
    chThdSleepMilliseconds(250);
    chprintf((BaseSequentialStream *)ttyHM10,"AT+NAME%s\r\n",buffer);
    chThdSleepMilliseconds(200);
    closeSerial();
    QUITAPAD(LINEONSERIALHM10);
    return 1;
}


void opciones(void)
{
    int16_t result, numErrores;
    int16_t opcion;
    uint8_t estDes, tipoSerie;
    uint32_t sec2change;
    uint32_t secActual, secAmanecer, secAnochecer;
    float vBat;
    char buff[50];

#ifdef  BOARD_VERSION_EE3
    initSerialHM10();
#else
    initSerial();
#endif
    tipoSerie = esperaHM10();
    if (tipoSerie == 0)
    {
        closeSerial();
        closeSerialHM10();
        return; // nadie se ha conectado al HM-10
    }
    numErrores = 0;
    while (1==1)
    {
        leeVariables();
        calendar::diSecAmanecerAnochecer(&secActual, &secAmanecer, &secAnochecer);
        calendar::estadoDeseadoPuerta(&estDes, &sec2change);
        leeTension(&vBat);
        float porcBat = hallaCapBat(&vBat);
        calendar::rtcGetFecha();
        chprintf((BaseSequentialStream *)ttyHM10,"\n");
        if (hayW25q16)
            chprintf((BaseSequentialStream *)ttyHM10,"GIT Tag:%s Commit:%s, usando W25q16\n",GIT_TAG,GIT_COMMIT);
        else
            chprintf((BaseSequentialStream *)ttyHM10,"GIT Tag:%s Commit:%s, NO hay W25q16, uso flash\n",GIT_TAG,GIT_COMMIT);
        calendar::printFecha(buff,sizeof(buff));
        chprintf((BaseSequentialStream *)ttyHM10,"Fecha actual UTC: %s\n",buff);
        calendar::printHoras(buff,sizeof(buff));
        chprintf((BaseSequentialStream *)ttyHM10,"Tension bateria:%.2f (%d o/o) compAD:%d o/oo\n",vBat,(uint16_t) porcBat,incAdPormil);
        if (tensionCritica())
            chprintf((BaseSequentialStream *)ttyHM10,"OJO: Tension bateria criticamente baja\n");
        chprintf((BaseSequentialStream *)ttyHM10,"Hora amanecer y atardecer UTC: %s\n",buff);
        chprintf((BaseSequentialStream *)ttyHM10,"sec. amanecer:%d. anochecer:%d\n",secAmanecer, secAnochecer);
        chprintf((BaseSequentialStream *)ttyHM10,"sec. actual:%d, prox. cambio:%d, estado puerta:%d:%s\n",secActual, sec2change, estDes,estPuertaStr[estDes]);
        chprintf((BaseSequentialStream *)ttyHM10,"Minutos adicionales amanecer: %d atardecer: %d  secAdaptacion:%d (diaAdapt:%d)\n",addAmanecer, addAtardecer, secAdaptacion, diaAdaptado);
        chprintf((BaseSequentialStream *)ttyHM10,"Automatizacion de puerta: %d:%s\n",autoPuerta, estPuertaAutoStr[autoPuerta]);
        chprintf((BaseSequentialStream *)ttyHM10,"Correccion diaria de hora: %d ds/dia\n",dsAddPordia);
        chprintf((BaseSequentialStream *)ttyHM10,"Posicion servo abierto: %d, cerrado: %d\n\n",posAbierto, posCerrado);

        chprintf((BaseSequentialStream *)ttyHM10,"1 Ajusta fecha y hora\n");
        chprintf((BaseSequentialStream *)ttyHM10,"2 Automatizacion puerta\n");
        chprintf((BaseSequentialStream *)ttyHM10,"3 Posiciones de servos\n");
        chprintf((BaseSequentialStream *)ttyHM10,"4 Minutos adicionales\n");
        chprintf((BaseSequentialStream *)ttyHM10,"5 Correccion de hora\n");
        chprintf((BaseSequentialStream *)ttyHM10,"6 Correccion A/D\n");
        chprintf((BaseSequentialStream *)ttyHM10,"7 Examinar variables\n");
        if (tipoSerie == 1)
            chprintf((BaseSequentialStream *)ttyHM10,"8 Cambiar nombre modulo y salir\n");
        chprintf((BaseSequentialStream *)ttyHM10,"9 Salir\n");

        limpiaBuffer((BaseChannel *) ttyHM10); // por si esta conectado HM-10 y da mensajes de error
        result = preguntaNumero((BaseChannel *) ttyHM10, "Dime opcion", &opcion, 1, 9);
        chprintf((BaseSequentialStream *)ttyHM10,"\n");
        if (result==2 || (result==0 && opcion==9))
        {
            closeSerial();
            closeSerialHM10();
            return;
        }
        if (result!=0)
        {
            numErrores++;
            if (numErrores>5)
            {
                closeSerial();
                closeSerialHM10();
                return;
            }
            continue;
        }
        numErrores = 0;
        if (opcion==1)
            ajustaHora();
        if (opcion==2)
            ajustaPuerta();
        if (opcion==3)
            ajustaPosiciones();
        if (opcion==4)
            ajustaAddMinutos();
        if (opcion==5)
            ajustaDsAdd();
        if (opcion==6)
            ajustaIncAD();
        if (opcion==7)
            menuVars();
        if (tipoSerie==1 && opcion==8)
            if (cambiaNombreModulo())
                return;
    }
}
