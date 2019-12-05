
#include <ArduinoJSON.h>
#include <EEPROM.h>
#include <M5StickC.h>
#include <WiFi.h>
#include "ProgrammingMode.h"



bool init_test = true;
String ssid = "your_ssid_here";;
String password = "your_password_here";

WiFiServer tcp_server(4001);
WiFiUDP broadcast;

byte on_cmd[] = { 0xAA, 0x05, 0x00, 0x00, 0xFF, 0x00, 0x95, 0xE1 };
byte off_cmd[] = { 0xAA, 0x05, 0x00, 0x00, 0x00, 0x00, 0xd4, 0x11 };

ulong broadcast_timer;

ulong last_checkin;
bool output_value = false;

ulong off_at;

//_lock_t wifi_lock;

void m5_mgmt(void* pvParameters)
{
	ulong tmr = 0;
	
	while (true)
	{
		M5.update();

		vTaskDelay(10);
	}
}

void setup()
{
	M5.begin();
	EEPROM.begin(1024);
	
	if (EEPROM.read(1000) == 0xba)
	{
		EEPROM.write(1000, 0x00);
		EEPROM.commit();

		enter_programming_mode();

		while (true)
		{
			esp_loop();
			delay(1000);
		}

	}

	//check to see if eeprom contains key
	if (EEPROM.read(0) == EEPROM_PROG_KEY)
		read_cfg();
	
	
	Serial2.begin(9600, SERIAL_8N1, 33, 32);
	pinMode(M5_LED, OUTPUT);

	xTaskCreate(m5_mgmt, "m5_mgmt", 8192, NULL, 1, NULL);
	xTaskCreate(programming_mode_task, "prog_mode", 8192, NULL, 1, NULL);
	
	

	//scan for wireless networks and connect
	if (init_test)
	{
		init_wifi();

		IPAddress _ip = WiFi.localIP();
		M5.Lcd.println(_ip.toString());

		tcp_server.begin(4001);


		//IPAddress broadcast_ip = WiFi.localIP();

		//broadcast_ip[4] = 255;

		broadcast.begin(4000);

	}
		
	
	xTaskCreate(udp_broadcast_task, "udp_broadcast", 8192, NULL, 2, NULL);

	xTaskCreate(switch_control_task, "switch_control", 8192, NULL, 1, NULL);
	
}

void switch_control_task(void* pvParameters)
{
	while (true)
	{
		//if ((ulong)((long)millis() - last_checkin) >= 10000)
		//	output_value = false;
		if (off_at >= millis())
			output_value = true;
		else
			output_value = false;



		rs485_write(output_value);
		digitalWrite(M5_LED, output_value ? HIGH : LOW);

		vTaskDelay(100);
	}
	
}

void programming_mode_task(void* pvParameters)
{
	while (true)
	{
		if (M5.BtnA.pressedFor(2000))
		{
			//enter esp now programming mode
			//M5.Lcd.println("Enter Programming Mode");

			EEPROM.write(1000, 0xba);
			EEPROM.commit();

			delay(100);

			ESP.restart();
		}
		else
			vTaskDelay(10);
	}
}

void udp_broadcast_task(void* pvParameters)
{
	while (true)
	{
		if ((ulong)((long)millis() - broadcast_timer) >= 2000)
		{
			//_lock_acquire(&wifi_lock);
			broadcast_timer = millis();
			send_broadcast_packet();
			

			//_lock_release(&wifi_lock);
		}
		else
			vTaskDelay(2);
	}
	
}

int client_num = 0;
void client_mgmt_task(void* pvParameters)
{
	while (true)
	{
		WiFiClient * client = (WiFiClient*)pvParameters;

		while(client->connected())
		{
			//_lock_acquire(&wifi_lock);

			if (client->available() >= 8)
			{
				
				ulong buffer = 0;
				uint8_t value = client->readBytes((uint8_t*)&buffer, 8); //receive duration

				//M5.Lcd.println(buffer);

				client->write(0x1);

				//_lock_release(&wifi_lock);

				if (off_at < millis())
					off_at = millis() + buffer;
				else
					off_at += buffer;

				output_value = true;
				//last_checkin = millis();

				//M5.Lcd.println(value);

				client->stop();

				break;
			}
			else
				vTaskDelay(10);

			
		}



		client->stop();

		vTaskDelete(NULL);
	}
}

// Add the main program code into the continuous loop() function
void loop()
{
	//_lock_acquire(&wifi_lock);

	WiFiClient client = tcp_server.available();

	if (client) {

		String client_name = "client_mgmt_task_";
		client_name += client_num;
		const char * cst = (client_name.c_str());

		xTaskCreate(client_mgmt_task, cst, 8192, (void*)&client, 1, NULL);
		client_num++;

		
		//client.stop();
		//Serial.println("Client disconnected");
	}

	//_lock_release(&wifi_lock);

	vTaskDelay(100);
}

void rs485_write(bool val)
{
	for (int i = 0; i < 8; i++)
	{
		if(val)
			Serial2.write(on_cmd[i]);
		else
			Serial2.write(off_cmd[i]);
	}
}





void send_broadcast_packet()
{
	char buffer[128];
	StaticJsonDocument<128> broadcast_message;

	memset(buffer, 0, 128);

	IPAddress local_ip = WiFi.localIP();

	uint8_t mac[6];
	WiFi.macAddress(mac);

	broadcast_message["mac"] = mac;

	JsonArray ip_Array = broadcast_message.createNestedArray("ip");
	for (int i = 0; i < 4; i++)
		ip_Array.add(local_ip[i]);
	
	serializeJson(broadcast_message, buffer, 128);

	IPAddress address(255, 255, 255, 255);

	broadcast.beginPacket(address, 4000); // subnet Broadcast IP and port
	broadcast.write((uint8_t*)buffer, 128);
	broadcast.endPacket();
}

void init_wifi()
{
	bool connected = false;
	
	while (!connected)
	{
		int num_ssid = 0;
		while ((num_ssid = WiFi.scanNetworks()) < 1)
			delay(1000);

		M5.Lcd.fillScreen(BLACK);
		M5.Lcd.setCursor(0,0);
		M5.Lcd.println(WiFi.macAddress());

		M5.Lcd.print("detected ");
		M5.Lcd.print(num_ssid);
		M5.Lcd.println(" networks");

		for (uint8_t i = 0; i < num_ssid; i++)
		{
			String found_ssid = WiFi.SSID(i);
			int rssi = WiFi.RSSI(i);

			M5.Lcd.print("ssid ");
			M5.Lcd.println(found_ssid);
			M5.Lcd.print("rssi ");
			M5.Lcd.println(rssi);

			while (found_ssid == ssid)
			{
				M5.Lcd.println("match found");
				
				//found the saved network. attempt connection.

				WiFi.begin(ssid.c_str(), password.c_str());

				//poll connection status until retry

				for (int k = 0; k < 30; k++)
				{
					if (WiFi.status() != WL_CONNECTED)
						delay(500);
					else
						if (WiFi.status() == WL_CONNECTED)
							break;

					//background();
				}

				if (WiFi.status() == WL_CONNECTED)
					return;
				//else
				//{
				//	M5.Lcd.fillScreen(BLACK);
				//	M5.Lcd.setCursor(0, 0);
				//}
			}

			//background();
		}
	}
}

void read_cfg()
{
	StaticJsonDocument<200> wifi_credentials;
	char buffer[200];

	EEPROM.get(1, buffer);
	
	deserializeJson(wifi_credentials, buffer);

	const char * p1 = wifi_credentials["ssid"];
	const char * p2 = wifi_credentials["password"];



	ssid = p1;
	password = p2;

	M5.Lcd.println(ssid);
	M5.Lcd.println(password);

	//delay(8000);
}

void write_cfg()
{
	StaticJsonDocument<200> wifi_credentials;
	wifi_credentials["ssid"] = ssid;
	wifi_credentials["password"] = password;

	char buffer[200];

	serializeJson(wifi_credentials, buffer, 200);

	EEPROM.put(1, buffer);

	EEPROM.write(0, EEPROM_PROG_KEY);

	EEPROM.commit();
}
