
#include <ArduinoJSON.h>
#include <EEPROM.h>
#include <M5StickC.h>
#include <esp_now.h>
#include <WiFi.h>
#include "ProgrammingMode.h"

#define CHANNEL 1

bool done = false;

void enter_programming_mode()
{
	M5.Lcd.setCursor(0, 0);
	
	InitESPNow();
}



void InitESPNow()
{
	WiFi.mode(WIFI_AP);
	// configure device AP mode
	configDeviceAP();
	// This is the mac address of the Slave in AP Mode
	M5.Lcd.print("AP MAC: "); M5.Lcd.println(WiFi.softAPmacAddress());
	// Init ESPNow with a fallback logic
	
	
	WiFi.disconnect();
	if (esp_now_init() == ESP_OK) {
		M5.Lcd.println("ESPNow Init Success");
	}
	else {
		M5.Lcd.println("ESPNow Init Failed");
		// Retry InitESPNow, add a counte and then restart?
		// InitESPNow();
		// or Simply Restart
		ESP.restart();
	}





	


	// Once ESPNow is successfully Init, we will register for recv CB to
	// get recv packer info.
	esp_now_register_recv_cb(OnDataRecv);
}

void esp_loop()
{
	if (done)
	{
		delay(4000);
		ESP.restart();
	}
}

void sendReply(const uint8_t* macAddr) {
	// create a peer with the received mac address
	esp_now_peer_info_t info;
	memset(&info, 0, sizeof(info));
	info.channel = CHANNEL;
	info.encrypt = 0;
	memcpy((uint8_t*)&((info.peer_addr)[0]), macAddr, 6);
	info.ifidx = ESP_IF_WIFI_AP;

	esp_now_add_peer(&info);
	

	//replyData.time = millis();
	uint8_t to_send[6];
	WiFi.macAddress(to_send);


	int val = esp_now_send(info.peer_addr, to_send, 6); // NULL means send to all peers

	//M5.Lcd.println(ESP_ERR_WIFI_BASE);
	//M5.Lcd.println(val);
	//Serial.println("sendReply sent data");

	// data sent so delete the peer
	esp_now_del_peer(macAddr);
}

// config AP SSID
void configDeviceAP() {
	const char* SSID = "Slave_1";
	bool result = WiFi.softAP(SSID, "Slave_1_Password", CHANNEL, 0);
	if (!result) {
		M5.Lcd.println("AP Config failed.");
	}
	else {
		M5.Lcd.println("AP Config Success. Broadcasting with AP: " + String(SSID));
	}
}

// callback when data is recv from Master
void OnDataRecv(const uint8_t* mac_addr, const uint8_t* data, int data_len)
{
	M5.Lcd.fillScreen(BLACK);
	M5.Lcd.setCursor(0,0);
	
	char macStr[18];
	snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	M5.Lcd.println("Last Packet Recv from: "); M5.Lcd.println(macStr);
	M5.Lcd.println("Last Packet Recv Data: "); M5.Lcd.println(*data);
	M5.Lcd.println("");

	uint8_t rx_data[200];

	for (int i = 0; i < 200; i++)
		rx_data[i] = data[i];

	EEPROM.put(1, rx_data);

	EEPROM.write(0, EEPROM_PROG_KEY);

	EEPROM.commit();

	M5.Lcd.println(data_len);

	sendReply(mac_addr);

	//vTaskDelay(10000);

	//ESP.restart();

	done = true;
}
