#include "network.h"
#include "serial.h"
#include "config.h"
#include "ESP8266WiFi.h"
#include "ESPWebDAV.h"

volatile long Network::_spiBlockoutTime = 0;
bool Network::_weHaveBus = false;

void Network::setup() {
  // ----- GPIO -------
	// Detect when other master uses SPI bus
	pinMode(CS_SENSE, INPUT);
	attachInterrupt(CS_SENSE, []() {
		if(!_weHaveBus)
	    _spiBlockoutTime = millis() + SPI_BLOCKOUT_PERIOD;
	}, FALLING);

	// wait for other master to assert SPI bus first
	delay(SPI_BLOCKOUT_PERIOD);
}

bool Network::start() {
  wifiConnected = false;
  
  // Set hostname first
  WiFi.hostname(HOSTNAME);
  // Reduce startup surge current
  WiFi.setAutoConnect(false);
  WiFi.mode(WIFI_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.begin(config.ssid(), config.password());

  // Wait for connection
  unsigned int timeout = 0;
  while(WiFi.status() != WL_CONNECTED) {
    //blink();
    SERIAL_ECHO(".");
    timeout++;
    if(timeout++ > WIFI_CONNECT_TIMEOUT/100) {
      SERIAL_ECHOLN("");
      return false;
    }
    else
      delay(100);
  }

  SERIAL_ECHOLN("");
  SERIAL_ECHO("Connected to "); SERIAL_ECHOLN(config.ssid());
  SERIAL_ECHO("IP address: "); SERIAL_ECHOLN(WiFi.localIP());
  SERIAL_ECHO("RSSI: "); SERIAL_ECHOLN(WiFi.RSSI());
  SERIAL_ECHO("Mode: "); SERIAL_ECHOLN(WiFi.getPhyMode());
  SERIAL_ECHO("Asscess to SD at the Run prompt : \\\\"); SERIAL_ECHO(WiFi.localIP());SERIAL_ECHOLN("\\DavWWWRoot");

  wifiConnected = true;

  config.save();

  startDAVServer();

  return true;
}

void Network::startDAVServer() {
  // Check to see if other master is using the SPI bus
  while(millis() < _spiBlockoutTime) {
    //blink();
  }
  
  takeBusControl();
  
  // start the SD DAV server
  if(!dav.init(SD_CS, SPI_FULL_SPEED, SERVER_PORT))   {
    DBG_PRINT("ERROR: "); DBG_PRINTLN("Failed to initialize SD Card");
    // indicate error on LED
    //errorBlink();
    initFailed = true;
  }
  else {
    //blink();
  }
  
  relenquishBusControl();
  DBG_PRINTLN("FYSETC WebDAV server started");
}

bool Network::isConnected() {
  return wifiConnected;
}

// a client is waiting and FS is ready and other SPI master is not using the bus
bool Network::ready() {
  if(!isConnected()) return false;
  
  // do it only if there is a need to read FS
	if(!dav.isClientWaiting())	return false;
	
	if(initFailed) {
	  dav.rejectClient("Failed to initialize SD Card");
	  return false;
	}
	
	// has other master been using the bus in last few seconds
	if(millis() < _spiBlockoutTime) {
		dav.rejectClient("Marlin is reading from SD card");
		return false;
	}

	return true;
}

void Network::handle() {
  if(network.ready()) {
	  takeBusControl();
	  dav.handleClient();
	  relenquishBusControl();
	}
}

// ------------------------
void Network::takeBusControl()	{
// ------------------------
	_weHaveBus = true;
	//LED_ON;
	pinMode(MISO_PIN, SPECIAL);	
	pinMode(MOSI_PIN, SPECIAL);	
	pinMode(SCLK_PIN, SPECIAL);	
	pinMode(SD_CS, OUTPUT);
}

// ------------------------
void Network::relenquishBusControl()	{
// ------------------------
	pinMode(MISO_PIN, INPUT);	
	pinMode(MOSI_PIN, INPUT);	
	pinMode(SCLK_PIN, INPUT);	
	pinMode(SD_CS, INPUT);
	//LED_OFF;
	_weHaveBus = false;
}


Network network;
