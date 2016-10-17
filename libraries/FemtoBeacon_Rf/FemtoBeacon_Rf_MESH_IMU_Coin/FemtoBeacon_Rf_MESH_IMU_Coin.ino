/**
 * FemtoBeacon wirless IMU and LPS platform.
 * Mesh networked IMU demo.
 *
 * @author A. Alibno <aalbino@femtoduino.com>
 * @version 1.0.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>


#define Serial SERIAL_PORT_USBVIRTUAL

/** BEGIN Atmel's LightWeight Mesh stack. **/
    #include "lwm.h"
    #include "lwm/sys/sys.h"
    #include "lwm/nwk/nwk.h"
/** END Atmel's LightWeight Mesh stack. **/

/** BEGIN mjs513/FreeIMU-Updates library. **/
    //These are optional depending on your IMU configuration

    //#include <ADXL345.h>
    //#include <HMC58X3.h>
    //#include <LSM303.h>
    //#include <LPS.h> 
    //#include <ITG3200.h> //note LPS library must come before ITG lib
    //#include <bma180.h>
    //#include <MS561101BA.h> //Comment out for APM 2.5
    //#include <BMP085.h>
    #include <I2Cdev.h>
    #include <MPU60X0.h>
    //#include <AK8975.h>
    #include <AK8963.h>
    //#include <L3G.h>
    //#include <SFE_LSM9DS0.h>
    //#include <BaroSensor.h>
    #include <AP_Baro_MS5611.h>  //Uncomment for APM2.5


    //These are mandatory
    #include <AP_Math_freeimu.h>
    #include <Butter.h>    // Butterworth filter
    #include <iCompass.h>
    #include <MovingAvarageFilter.h>

    //#define DEBUG
    #include "DebugUtils.h"
    #include "CommunicationUtils.h"
    //#include "DCM.h"
    #include "FilteringScheme.h"
    #include "RunningAverage.h"
    #include "FreeIMU.h"

    // Arduino Zero: no eeprom 
    #define HAS_EEPPROM 0
/** END mjs513/FreeIMU-Updates library. **/

/** BEGIN Networking vars **/
    extern "C" {
      void                      println(char *x) { Serial.println(x); Serial.flush(); }
    }

    #ifdef NWK_ENABLE_SECURITY
    #define APP_BUFFER_SIZE     (NWK_MAX_PAYLOAD_SIZE - NWK_SECURITY_MIC_SIZE)
    #else
    #define APP_BUFFER_SIZE     NWK_MAX_PAYLOAD_SIZE
    #endif

    // Address must be set to 1 for the first device, and to 2 for the second one.
    #define APP_ADDRESS         1
    #define APP_ENDPOINT        1
    #define APP_PANID                 0x4567
    #define APP_SECURITY_KEY          "TestSecurityKey0"
    
    char                        bufferData[APP_BUFFER_SIZE];
    static NWK_DataReq_t        sendRequest;
    static void                 sendMessage(void);
    static void                 sendMessageConfirm(NWK_DataReq_t *req);
    static bool                 receiveMessage(NWK_DataInd_t *ind);

    static bool                 send_message_busy = false;

    byte pingCounter            = 0;
/** END Networking vars **/

/** BEGIN Sensor vars **/
    //int raw_values[11];
    //char str[512];
    float ypr[3]; // yaw pitch roll
    //float val[9];
    
    

    // Set the FreeIMU object
    FreeIMU sensors = FreeIMU();
/** END Sensor vars **/

byte delimeter = (byte) '|';
byte filler = (byte) ' ';

void setup() {
  // put your setup code here, to run once:
  setupSerialComms();
  Serial.print("Starting LwMesh...");
  setupMeshNetworking();
  Serial.println("OK.");
  delay(500);
  Serial.print("Starting Sensors...");
  setupSensors();
  Serial.println("OK.");

  // REQUIRED! calls to dtostrf will otherwise fail (optimized out?)
  char cbuff[7];
  dtostrf(123.4567, 6, 2, cbuff);
  Serial.print("cbuff test is ");
  Serial.println(cbuff);

  Serial.println("OK, ready!");
}

void setupSerialComms() {
    while(!Serial);
    
    Serial.begin(115200);
    Serial.print("LWP Ping Demo. Serial comms started. ADDRESS is ");
    Serial.println(APP_ADDRESS);
}

void setupMeshNetworking() {
    SPI.usingInterrupt(digitalPinToInterrupt(PIN_SPI_IRQ));
    
    SPI.beginTransaction(
      SPISettings(
          MODULE_AT86RF233_CLOCK, 
          MSBFIRST, 
          SPI_MODE0
      )
    );

    attachInterrupt(digitalPinToInterrupt(PIN_SPI_IRQ), HAL_IrqHandlerSPI, RISING);
    /*  wait for SPI to be ready  */
    delay(10);

    SYS_Init();
    NWK_SetAddr(APP_ADDRESS);
    NWK_SetPanId(0x01);
    PHY_SetChannel(0x1a);
    PHY_SetRxState(true);
    NWK_OpenEndpoint(1, receiveMessage);
}

void setupSensors() {
    Wire.begin();
  
    delay(10);
    sensors.init(); // the parameter enable or disable fast mode
    delay(10);
}

void loop() {
  //float fNumber = 123.01;
  //char chrNumber[5];
  
  //dtostrf(fNumber, 1, 1, chrNumber);
  // put your main code here, to run repeatedly:
  
  handleSensors();
  delay(20);
  handleNetworking();
  //String str(chrNumber);
  //Serial.print(str);
  Serial.println("----");
  
  delay(1000);
}

void handleNetworking()
{
    SYS_TaskHandler();
    
    if(APP_ADDRESS == 1) {
      Serial.println("handleNetworking() ->sendMessage()");
        sendMessage();
    }

    /*if(pingCounter % 2) {
        //analogWrite(LED_GREEN, 128);
        Serial.println("-");
    } else {
        //analogWrite(LED_GREEN, 255);
        Serial.println("*");
    }*/
    Serial.println("handleNetworking()");
}

void handleSensors()
{
    //sprintf (bufferData, "\r\nLIGHT LAMP AT ADDRESS %d!\r\n", APP_ADDRESS);
    Serial.println("handleSensors()");
    sensors.getYawPitchRoll(ypr);

    Serial.println("...getting YPR string");
    Serial.print("Buffer size is ");
    Serial.println(APP_BUFFER_SIZE);
    
    Serial.print("sensor data is ");
    Serial.print(ypr[0]);
    Serial.print(',');
    Serial.print(ypr[1]);
    Serial.print(',');
    Serial.println(ypr[2]);


    //// Use dtostrf?
    // Copy ypr to buffer.
    memset(bufferData, ' ', APP_BUFFER_SIZE);
    bufferData[APP_BUFFER_SIZE] = '\0';

    // ...We need a wide enough size (8 should be enough to cover negative symbol and decimal). 
    // ...Precision is 2 decimal places.
    dtostrf(ypr[0], 8, 2, &bufferData[0]);
    dtostrf(ypr[1], 8, 2, &bufferData[9]);
    dtostrf(ypr[2], 8, 2, &bufferData[18]);
    // ...Delimeters
    bufferData[8] = ',';
    bufferData[17] = ',';
    

    Serial.print("Got ");
    Serial.println(bufferData);
}


static void sendMessage(void) {

  if (send_message_busy) {
    return;
  }
  //pingCounter++;
  //char sensorData[5] = "    ";
  //byte i = 0;

  //if (ypr[0] != NULL)
  //{
  //  dtostrf(ypr[0], 1, 1, sensorData);
  //}

  // we just leak for now
  //NWK_DataReq_t *message = (NWK_DataReq_t*)malloc(sizeof(NWK_DataReq_t));

  Serial.println("sendMessage()");
  sendRequest.dstAddr       = 2;//1 - APP_ADDRESS;
  sendRequest.dstEndpoint   = 1;//APP_ENDPOINT;
  sendRequest.srcEndpoint   = 1;//APP_ENDPOINT;
  //sendRequest.options       = NWK_OPT_ACK_REQUEST;
  sendRequest.data          = (uint8_t*)&bufferData;
  sendRequest.size          = min(APP_BUFFER_SIZE, sizeof(bufferData));
  sendRequest.confirm       = sendMessageConfirm;
  
  NWK_DataReq(&sendRequest);

  send_message_busy = true;
}

static void sendMessageConfirm(NWK_DataReq_t *req)
{
  Serial.print("sendMessageConfirm() req->status is ");
  if (NWK_NO_ACK_STATUS == req->status)
  {
    Serial.println("NWK_NO_ACK_STATUS");
  } else if (NWK_NO_ROUTE_STATUS == req->status) {
    Serial.println("NWK_NO_ROUTE_STATUS");
  } else if (NWK_ERROR_STATUS) {
    Serial.println("NWK_ERROR_STATUS");
  }
  
  if (NWK_SUCCESS_STATUS == req->status)
  {
    send_message_busy = false;
    Serial.println("NWK_SUCCESS_STATUS");
  }
  (void) req;
}

static bool receiveMessage(NWK_DataInd_t *ind) {
    //char sensorData[5];

    Serial.print("receiveMessage() ");
    Serial.print("lqi: ");
    Serial.print(ind->lqi, DEC);

    Serial.print("  ");

    Serial.print("rssi: ");
    Serial.print(ind->rssi, DEC);
    Serial.print("  ");

    Serial.print("ping: ");

    //sensorData = (char) &ind->data;
    //memcpy();
    
    //String str((char*)ind->data);

    //Serial.println(str);

    //pingCounter = (byte)*(ind->data);
    //Serial.println(pingCounter);

    //Serial.print("my Yaw:");
    //Serial.print(ypr[0]);
    //Serial.print(", Pitch:");
    //Serial.print(ypr[1]);
    //Serial.print(", Roll:");
    //Serial.println(ypr[2]);
    
    return true;
}

//static void setYPRToBytes(float* arrYPR, uint8_t* bytes) {

    //dtostrf(arrYPR[0], 1, 1, &bufferData[0]);
    //dtostrf(arrYPR[1], 1, 1, &bufferData[6]);
    //dtostrf(arrYPR[2], 1, 1, &bufferData[11]);


    //bufferData[5]   = '|';
    //bufferData[10]  = '|';

    //bufferSize  = min(20, sizeof(buffer));

    //BTLEserial.write((byte*)chrData, sendbuffersize);
    //bytes = (byte*)bufferData;
//}


/*
 * SEE http://forum.arduino.cc/index.php?topic=368720.0
 * 
  dtostrf - Emulation for dtostrf function from avr-libc
  Copyright (c) 2015 Arduino LLC.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>

#if 0
char *dtostrf (double val, signed char width, unsigned char prec, char *sout) {
  char fmt[20];
  sprintf(fmt, "%%%d.%df", width, prec);
  sprintf(sout, fmt, val);
  return sout;
}
#else

char *dtostrf(double val, int width, unsigned int prec, char *sout)
{
  int decpt, sign, reqd, pad;
  const char *s, *e;
  char *p;
  s = fcvtf(val, prec, &decpt, &sign);
  if (prec == 0 && decpt == 0) {
  s = (*s < '5') ? "0" : "1";
    reqd = 1;
  } else {
    reqd = strlen(s);
    if (reqd > decpt) reqd++;
    if (decpt == 0) reqd++;
  }
  if (sign) reqd++;
  p = sout;
  e = p + reqd;
  pad = width - reqd;
  if (pad > 0) {
    e += pad;
    while (pad-- > 0) *p++ = ' ';
  }
  if (sign) *p++ = '-';
  if (decpt <= 0 && prec > 0) {
    *p++ = '0';
    *p++ = '.';
    e++;
    while ( decpt < 0 ) {
      decpt++;
      *p++ = '0';
    }
  }    
  while (p < e) {
    *p++ = *s++;
    if (p == e) break;
    if (--decpt == 0) *p++ = '.';
  }
  if (width < 0) {
    pad = (reqd + width) * -1;
    while (pad-- > 0) *p++ = ' ';
  }
  *p = 0;
  return sout;
}
#endif
