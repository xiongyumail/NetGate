/*
    This sketch shows the Ethernet event usage

*/

#include <Arduino.h>
#include <ETH.h>
#include <SPI.h>

static const int spiClk = 10000000; // 10 MHz

//uninitalised pointers to SPI objects
SPIClass * hspi = NULL;


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
      ETH.setHostname("ULT");
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

void hspi_write(const uint8_t *data,uint16_t len) {
  hspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  for (uint16_t i = 0; i < len; i++) {
    digitalWrite(15, LOW);
    hspi->transfer(data[i]);
    digitalWrite(15, HIGH);
  }
  hspi->endTransaction();
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
    hspi_write((const uint8_t*)buf, bufCount);
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
    hspi_write((const uint8_t*)buf, bufCount);
    bufCount = 0;
  }
}


void setup()
{
  Serial.begin(115200);
  Serial.println("The device started, now you can pair it with bluetooth!\r\n\r\n");
  WiFi.onEvent(WiFiEvent);
  delay(100); // delay for ethernet start up.
  ETH.begin();
  buf = (uint8_t *)malloc(BUFFERSIZE);

  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  hspi = new SPIClass(HSPI);
  
  //clock miso mosi ss
  //initialise hspi with default pins
  //SCLK = 14, MISO = 12, MOSI = 13, SS = 15
  hspi->begin(); 
  //alternatively route through GPIO pins
  //hspi->begin(25, 26, 27, 32); //SCLK, MISO, MOSI, SS

  //set up slave select pins as outputs as the Arduino API
  //doesn't handle automatically pulling SS low
  pinMode(15, OUTPUT); //HSPI SS
}


void loop()
{
  if (eth_connected) {
    Client_Bridge("192.168.0.100", 1878);
  }
  delay(20);
}

