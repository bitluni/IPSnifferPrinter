//bitluni was here
//Prerequisites: Larry Banks Thermal Printer Library
//Working with Peripage printer and esp32
//Setting to Minimal SPIFFS 1.9MB App
//fill in ssid and password

#include <EEPROM.h>
#include <Thermal_Printer.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <AsyncUDP.h>


const char* ssid = "";
const char* password = "";
AsyncUDP udp;

void printHex(char *data, int length)
{
	int p = 0;
	while(p < length)
	{
		char ascii[17];
		int i = 0;
		for(; i < 16; i++)
		{
			Serial.printf("%02X ", data[p]);
			if(data[p] >= 32)// || data[p] < 128)
				ascii[i] = data[p];
			else
				ascii[i] = '.';
			p++;
			if(p == length)
			{ i++; break;}
		}
		ascii[i] = 0;
		Serial.println(ascii);
	}
}

void printIP(char *data)
{
	for(int i = 0; i < 4; i++)
	{
		Serial.print((int)data[i]);
		if(i < 3)
			Serial.print('.');
	}
}

const int DHCP_PACKET_CLIENT_ADDR_LEN_OFFSET = 2;
const int DHCP_PACKET_CLIENT_ADDR_OFFSET = 28;

enum State
{
	READY,
	RECEIVING,
	RECIEVED
};

volatile State state = READY;
String newMAC;
String newIP;
String newName;

const char *hexDigits = "0123456789ABCDEF";

void printData()
{
	if(tpScan())
	{
		Serial.println((char *)"Found a printer!, connecting...");
		if(tpConnect())
		{
			tpSetFont(1, 0, 1, 1, 1);
			tpPrint((char*)"New Device\n");
			tpPrint((char*)newName.c_str()); tpPrint((char*)"\n");
			tpPrint((char*)newIP.c_str()); tpPrint((char*)"\n");
			tpSetFont(1, 0, 0, 1, 1);
			tpPrint((char*)"MAC: ");
			tpPrint((char*)newMAC.c_str()); tpPrint((char*)"\n");
			tpPrint((char*)"\n\n\n\n");
			tpDisconnect();
		}
	}
}

void parsePacket(char* data, int length)
{
	if(state == RECIEVED) return;
	String tempName;
	String tempIP;
	String tempMAC;

	Serial.println("DHCP Packet");
	//printHex(data, length);
	Serial.print("MAC address: ");
	for(int i = 0; i < data[DHCP_PACKET_CLIENT_ADDR_LEN_OFFSET]; i++)
		if(i < data[DHCP_PACKET_CLIENT_ADDR_LEN_OFFSET] - 1)
			Serial.printf("%02X:", (int)data[DHCP_PACKET_CLIENT_ADDR_OFFSET + i]);
		else
			Serial.printf("%02X", (int)data[DHCP_PACKET_CLIENT_ADDR_OFFSET + i]);
	for(int i = 0; i < data[DHCP_PACKET_CLIENT_ADDR_LEN_OFFSET]; i++)
	{
		tempMAC += hexDigits[(int)data[DHCP_PACKET_CLIENT_ADDR_OFFSET + i] >> 4];
		tempMAC += hexDigits[(int)data[DHCP_PACKET_CLIENT_ADDR_OFFSET + i] & 15];
		if(i < data[DHCP_PACKET_CLIENT_ADDR_LEN_OFFSET] - 1)
			tempMAC += ":";
	}

	Serial.println();
	//parse options
	int opp = 240;
	while(opp < length)
	{
		switch(data[opp])
		{
			case 0x0C:
			{
				Serial.print("Device name: ");
				for(int i = 0; i < data[opp + 1]; i++)
				{
					Serial.print(data[opp + 2 + i]);
					tempName += data[opp + 2 + i];
				}
				Serial.println();
				break;
			}
			case 0x35:
			{
				Serial.print("Packet Type: ");
				switch(data[opp + 2])
				{
					case 0x01:
						Serial.println("Discover");
					break;
					case 0x02:
						Serial.println("Offer");
					break;
					case 0x03:
						Serial.println("Request");
						if(state == READY)
							state = RECEIVING;
					break;
					case 0x05:
						Serial.println("ACK");
					break;
					default:
						Serial.println("Unknown");
				}
				break;
			}
			case 0x32:
			{
				Serial.print("Device IP: ");
				printIP(&data[opp + 2]);
				Serial.println();
				for(int i = 0; i < 4; i++)
				{
					tempIP += (int)data[opp + 2 + i];
					if(i < 3) tempIP += '.';
				}
				break;
			}
			case 0x36:
			{
				Serial.print("Server IP: ");
				printIP(&data[opp + 2]);
				Serial.println();
				break;
			}
			case 0x37:
			{
				Serial.println("Request list: ");
				printHex(&data[opp + 2], data[opp + 1]);
				break;
			}
			case 0x39:
			{
				Serial.print("Max DHCP message size: ");
				Serial.println(((unsigned int)data[opp + 2] << 8) | (unsigned int)data[opp + 3]);
				break;
			}
			case 0xff:
			{
				Serial.println("End of options.");
				opp = length; 
				continue;
			}
			default:
			{
				Serial.print("Unknown option: ");
				Serial.print((int)data[opp]);
				Serial.print(" (length ");
				Serial.print((int)data[opp + 1]);
				Serial.println(")");
				printHex(&data[opp + 2], data[opp + 1]);
			}
		}

		opp += data[opp + 1] + 2;
	}
	if(state == RECEIVING)
	{
		newName = tempName;
		newIP = tempIP;
		newMAC = tempMAC;
		Serial.println("Stored data.");
		state = RECIEVED;
	}
	Serial.println();
}
void setupUDP()
{
	if(udp.listen(67)) 
	{
		Serial.print("UDP Listening on IP: ");
		Serial.println(WiFi.localIP());
		udp.onPacket([](AsyncUDPPacket packet) 
		{
			char *data = (char *)packet.data();
			int length = packet.length();
			parsePacket(data, length);
		});
	};
}

bool isDataStored()
{
	return EEPROM.read(96) != 0;
}

String readString(int offset, int len = 32)
{
	String s;
	int p = offset;
	for(int i = 0; i < len; i++)
	{
		char c = EEPROM.read(p++);
		if(c)
			s+=c;
	}
	return s;
}

void readData()
{
	newMAC = readString(0, 32);
	newIP = readString(32, 32);
	newName = readString(64, 32);
}

void clearData()
{
	EEPROM.write(96, 0);
	EEPROM.commit();
}

void writeString(String s, int offset, int len = 32)
{
	int p = offset;
	for(int i = 0; i < len; i++)
		if(i < s.length())
			EEPROM.write(p++, s.charAt(i));
		else
			EEPROM.write(p++, 0);
}

void writeData()
{
	writeString(newMAC, 0, 32);
	writeString(newIP, 32, 32);
	writeString(newName, 64, 32);
	EEPROM.write(96, 1);
	EEPROM.commit();
}

void setup() 
{
	Serial.begin(115200);
	/*EEPROM.begin(128);
	if(isDataStored())
	{
		readData();
		printData();
		clearData();
		ESP.restart();
	}*/

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.waitForConnectResult() != WL_CONNECTED) 
	{
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}
	Serial.println("Ready");
  	Serial.print("IP address: ");
  	Serial.println(WiFi.localIP());
	setupUDP();
	Serial.println();
}

void loop() 
{
	delay(20);
	if(state == RECIEVED)
	{
		//udp.close();
		//WiFi.disconnect();
		//WiFi.mode(WIFI_MODE_NULL);
		//writeData();
		//delay(1000);
		//ESP.restart();
		printData();
		state = READY;
	}
}
