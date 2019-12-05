// ProgrammingMode.h

#ifndef _PROGRAMMINGMODE_h
#define _PROGRAMMINGMODE_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif


#endif

#define EEPROM_PROG_KEY 0x45

void enter_programming_mode();
void InitESPNow();
void configDeviceAP();
void OnDataRecv(const uint8_t*, const uint8_t*, int);
void esp_loop();

