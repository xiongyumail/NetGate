/*
    This sketch shows the Ethernet event usage

*/

#include <ETH.h>

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

static bool eth_connected = false;
WiFiClient client;

#define packTimeout 5 // ms (if nothing more on UART, then send packet)
#define BUFFERSIZE 1024
uint8_t *buf;
uint16_t bufCount=0;

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void Client_Bridge(const char * host, uint16_t port)
{
  while (!client.connected())
  {
        if (!client.connect(host, port))
        {
            Serial.println("connection....");
            delay(200);
        }
  }
  if(client.available()) {
    while(client.available()) {
      buf[bufCount] = (uint8_t)client.read(); // read char from client (RoboRemo app)
      if(bufCount < BUFFERSIZE - 1) bufCount++;
    }
    // now send to UART:
    Serial.write((const uint8_t*)buf, bufCount);
    SerialBT.write((const uint8_t*)buf, bufCount);
    bufCount = 0;
  }

  if(Serial.available()) {
    // read the data until pause:
    while(1) {
      if(Serial.available()) {
        buf[bufCount] = (char)Serial.read(); // read char from UART
        if(bufCount < BUFFERSIZE - 1) bufCount++;
      } else {
        delay(packTimeout);
        if(!Serial.available()) {
          break;
        }
      }
    }
    // now send to WiFi:
    client.write((const uint8_t*)buf, bufCount);
    SerialBT.write((const uint8_t*)buf, bufCount);
    bufCount = 0;
  }
  if(SerialBT.available()) {
    // read the data until pause:
    while(1) {
      if(SerialBT.available()) {
        buf[bufCount] = (char)SerialBT.read(); // read char from BT
        if(bufCount < BUFFERSIZE - 1) bufCount++;
      } else {
        delay(packTimeout);
        if(!SerialBT.available()) {
          break;
        }
      }
    }
    // now send to WiFi:
    client.write((const uint8_t*)buf, bufCount);
    Serial.write((const uint8_t*)buf, bufCount);
    bufCount = 0;
  }
}

void setup()
{
  Serial.begin(115200);
  SerialBT.begin("NetGate"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!\r\n\r\n");
  WiFi.onEvent(WiFiEvent);
  ETH.begin();
  buf = (uint8_t *)malloc(BUFFERSIZE);
}


void loop()
{
  if (eth_connected) {
    Client_Bridge("emake.space", 1878);
  }
  delay(20);
}
