#include <EEPROM.h>
typedef struct
{
	int test;
}Rtc;

RTC_DATA_ATTR Rtc rtc;

void setup() 
{
	Serial.begin(115200);
	Serial.println(rtc.test++);
	ESP.restart();
}

void loop() 
{
}
