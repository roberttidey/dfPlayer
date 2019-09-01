/***************************************************
dfPlayer - A Mini MP3 Player for ESP8266
R.J.Tidey March 2018
****************************************************/
#define ESP8266

#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include "FS.h"
#include <DNSServer.h>
#include <WiFiManager.h>

//put -1 s at end
int unusedPins[11] = {0,2,4,5,12,14,16,-1,-1,-1,-1};

/*
Wifi Manager Web set up
If WM_NAME defined then use WebManager
*/
#define WM_NAME "dfPlayerSetup"
#define WM_PASSWORD "password"
#ifdef WM_NAME
	WiFiManager wifiManager;
#endif
//uncomment to use a static IP
//#define WM_STATIC_IP 192,168,0,100
//#define WM_STATIC_GATEWAY 192,168,0,1

int timeInterval = 25;
#define WIFI_CHECK_TIMEOUT 30000
unsigned long elapsedTime;
unsigned long wifiCheckTime;

//For update service
String host = "esp8266-hall";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "password";

//AP definitions
#define AP_SSID "ssid"
#define AP_PASSWORD "password"
#define AP_MAX_WAIT 10
String macAddr;

#define AP_AUTHID "14153143"
#define AP_PORT 80

ESP8266WebServer server(AP_PORT);
ESP8266HTTPUpdateServer httpUpdater;
//holds the current upload
File fsUploadFile;

SoftwareSerial mySoftwareSerial(5, 15); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
#define INIT_SKIP -1
#define INIT_START 0
#define INIT_DELAY 1
#define INIT_OK1 2
#define INIT_OK2 3
#define INIT_RUN 4

int dfPlayerInit = INIT_START;
int dfPlayerType;
int dfPlayerParameter;

int pinInputs[4] = {4,12,13,14};
int pinStates[4];
unsigned long pinTimes[4];
int pinChanges[4];
int muteState;
unsigned long pinTimers[4];
//delay initalising dfPlayer module after power on (mSec)
#define DFPLAYER_STARTUP 4000
#define VOLUP 0
#define VOLDOWN 1
#define SELECT1 2
#define SELECT2 3
#define MUTE 16
#define LONG_PRESS 1000

int folderSelect = 1;
int fileSelect = 1;
int volume = 15;
String folderMap[100];

//only check battery every 10th loop
#define BATTERY_DIVIDER 10
float ADC_CAL = 0.976;
float battery_mult = 57.0/10.0/1024;//resistor divider, vref, max count
float battery_volts = 4.2;
int batteryCounter = 0;

void ICACHE_RAM_ATTR  delaymSec(unsigned long mSec) {
	unsigned long ms = mSec;
	while(ms > 100) {
		delay(100);
		ms -= 100;
		ESP.wdtFeed();
	}
	delay(ms);
	ESP.wdtFeed();
	yield();
}

void ICACHE_RAM_ATTR  delayuSec(unsigned long uSec) {
	unsigned long us = uSec;
	while(us > 100000) {
		delay(100);
		us -= 100000;
		ESP.wdtFeed();
	}
	delayMicroseconds(us);
	ESP.wdtFeed();
	yield();
}

void unusedIO() {
	int i;
	
	for(i=0;i<11;i++) {
		if(unusedPins[i] < 0) {
			break;
		} else if(unusedPins[i] != 16) {
			pinMode(unusedPins[i],INPUT_PULLUP);
		} else {
			pinMode(16,INPUT_PULLDOWN_16);
		}
	}
}

/*
  Connect to local wifi with retries
  If check is set then test the connection and re-establish if timed out
*/
int wifiConnect(int check) {
	if(check) {
		if((elapsedTime - wifiCheckTime) * timeInterval > WIFI_CHECK_TIMEOUT) {
			if(WiFi.status() != WL_CONNECTED) {
				Serial.println(F("Wifi connection timed out. Try to relink"));
			} else {
				return 1;
			}
		} else {
			wifiCheckTime = elapsedTime;
			return 0;
		}
	}
	wifiCheckTime = elapsedTime;
#ifdef WM_NAME
	Serial.println(F("Set up managed Web"));
#ifdef WM_STATIC_IP
	wifiManager.setSTAStaticIPConfig(IPAddress(WM_STATIC_IP), IPAddress(WM_STATIC_GATEWAY), IPAddress(255,255,255,0));
#endif
	if(check == 0) {
		wifiManager.setConfigPortalTimeout(180);
		//Revert to STA if wifimanager times out as otherwise APA is left on.
		if(!wifiManager.autoConnect(WM_NAME, WM_PASSWORD)) WiFi.mode(WIFI_STA);
	} else {
		WiFi.begin();
	}
#else
	Serial.println(F("Set up manual Web"));
	int retries = 0;
	Serial.print(F("Connecting to AP"));
	#ifdef AP_IP
		IPAddress addr1(AP_IP);
		IPAddress addr2(AP_DNS);
		IPAddress addr3(AP_GATEWAY);
		IPAddress addr4(AP_SUBNET);
		WiFi.config(addr1, addr2, addr3, addr4);
	#endif
	WiFi.begin(AP_SSID, AP_PASSWORD);
	while (WiFi.status() != WL_CONNECTED && retries < AP_MAX_WAIT) {
		delaymSec(1000);
		Serial.print(".");
		retries++;
	}
	Serial.println("");
	if(retries < AP_MAX_WAIT) {
		Serial.print(F("WiFi connected ip "));
		Serial.print(WiFi.localIP());
		Serial.printf(":%d mac %s\r\n", AP_PORT, WiFi.macAddress().c_str());
		return 1;
	} else {
		Serial.println(F("WiFi connection attempt failed")); 
		return 0;
	} 
#endif
}

void initFS() {
	if(dfPlayerInit == INIT_SKIP) {
		//force a format
		SPIFFS.format();
	}
	if(!SPIFFS.begin()) {
		Serial.println(F("No SIFFS found. Format it"));
		if(SPIFFS.format()) {
			SPIFFS.begin();
		} else {
			Serial.println(F("No SIFFS found. Format it"));
		}
	} else {
		Serial.println(F("SPIFFS file list"));
		Dir dir = SPIFFS.openDir("/");
		while (dir.next()) {
			Serial.print(dir.fileName());
			Serial.print(F(" - "));
			Serial.println(dir.fileSize());
		}
	}
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  Serial.printf_P(PSTR("handleFileRead: %s\r\n"), path.c_str());
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.printf_P(PSTR("handleFileUpload Name: %s\r\n"), filename.c_str());
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    Serial.printf_P(PSTR("handleFileUpload Data: %d\r\n"), upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    Serial.printf_P(PSTR("handleFileUpload Size: %d\r\n"), upload.totalSize);
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.printf_P(PSTR("handleFileDelete: %s\r\n"),path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.printf_P(PSTR("handleFileCreate: %s\r\n"),path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  Serial.printf_P(PSTR("handleFileList: %s\r\n"),path.c_str());
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  output += "]";
  server.send(200, "text/json", output);
}

void handleMinimalUpload() {
  char temp[700];

  snprintf ( temp, 700,
    "<!DOCTYPE html>\
    <html>\
      <head>\
        <title>ESP8266 Upload</title>\
        <meta charset=\"utf-8\">\
        <meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
      </head>\
      <body>\
        <form action=\"/edit\" method=\"post\" enctype=\"multipart/form-data\">\
          <input type=\"file\" name=\"data\">\
          <input type=\"text\" name=\"path\" value=\"/\">\
          <button>Upload</button>\
         </form>\
      </body>\
    </html>"
  );
  server.send ( 200, "text/html", temp );
}

void handleSpiffsFormat() {
	SPIFFS.format();
	server.send(200, "text/json", "format complete");
}

void init_dfPlayer() {
	Serial.println();
	Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

	//Use softwareSerial to communicate with mp3.	
	if (myDFPlayer.begin(mySoftwareSerial)) {
		dfPlayerInit = INIT_OK1;
		Serial.println(F("DFPlayer Mini online."));
	} else {
		dfPlayerInit = INIT_OK2;
		Serial.println(F("Failed to init player"));
	}
}

void readFolders() {
	if(SPIFFS.exists("/Folders.txt")) {
		File f = SPIFFS.open("/Folders.txt", "r");
		int Index = 0;
		String temp; 
		while( f.available()) {
			temp =f.readStringUntil(10);
			temp.replace("\r","");
			temp.toLowerCase();
			Index = temp.substring(0,2).toInt();
			folderMap[Index] = temp.substring(3);
		}
		f.close();
	}
}

void getFolder(String folderSearch) {
	if(folderSearch.length() < 3) {
		folderSelect = folderSearch.toInt();
	} else {
		int Index;
		String temp;
		folderSearch.toLowerCase();
		for(Index = 99; Index > 0;Index--) {
			if(folderMap[Index].indexOf(folderSearch)>=0) {
				folderSelect = Index;
				break;
			}
		}
	}
	if(folderSelect < 1 || folderSelect > 99) folderSelect = 0;
}

void dfPlayerSetVolume(int vol) {
	if(vol > 30) vol = 30;
	if(vol < 0) vol = 0;
	if(vol != volume) {
		volume = vol;
		myDFPlayer.volume(volume);
		saveConfig();
	}
}

void dfPlayerCmd() {
	String cmd = server.arg("cmd");
	int p1 = server.arg("p1").toInt();
	int p2 = server.arg("p2").toInt();
	int p3 = server.arg("p3").toInt();
	server.send(200, "text/html", F("dfPlayer cmd being processed"));
	Serial.print(cmd);Serial.printf(" p1=%d p2=%d\r\n",p1,p2);
	
	if(dfPlayerInit != INIT_RUN  && dfPlayerInit >= INIT_OK1) {
		myDFPlayer.volume(volume);
		dfPlayerInit = INIT_RUN;
		delaymSec(500);
	}

	if(cmd.equalsIgnoreCase("play")) {
		getFolder(server.arg("p1"));
		if(folderSelect > 0) {
			myDFPlayer.playFolder(folderSelect, p2);
		}
	} else if(cmd.equalsIgnoreCase("playmp3")) {
		myDFPlayer.playMp3Folder(p1);
	} else if(cmd.equalsIgnoreCase("volume")) {
		dfPlayerSetVolume(p1);
	} else if(cmd.equalsIgnoreCase("stop")) {
		myDFPlayer.stop();
	} else if(cmd.equalsIgnoreCase("volumeup")) {
		dfPlayerSetVolume(volume+1);
	} else if(cmd.equalsIgnoreCase("volumedown")) {
		dfPlayerSetVolume(volume-1);
	} else if(cmd.equalsIgnoreCase("speaker")) {
		p1 = p1 ? 0 : 1;
		digitalWrite(MUTE, p1);
		muteState = p1;
		saveConfig();
	} else if(cmd.equalsIgnoreCase("pause")) {
		myDFPlayer.pause();
	} else if(cmd.equalsIgnoreCase("start")) {
		myDFPlayer.start();
	} else if(cmd.equalsIgnoreCase("next")) {
		myDFPlayer.next();
	} else if(cmd.equalsIgnoreCase("previous")) {
		myDFPlayer.previous();
	} else if(cmd.equalsIgnoreCase("mode")) {
		myDFPlayer.loop(p1);
	} else if(cmd.equalsIgnoreCase("loopFolder")) {
		getFolder(server.arg("p1"));
		if(folderSelect > 0) {
			myDFPlayer.loopFolder(folderSelect);
		}
	} else if(cmd.equalsIgnoreCase("random")) {
		myDFPlayer.randomAll();
	} else if(cmd.equalsIgnoreCase("eq")) {
		myDFPlayer.EQ(p1);
	} else if(cmd.equalsIgnoreCase("device")) {
		myDFPlayer.outputDevice(p1);
	} else if(cmd.equalsIgnoreCase("setting")) {
		myDFPlayer.outputSetting(p1,p2);
	} else if(cmd.equalsIgnoreCase("sleep")) {
		myDFPlayer.sleep();
	} else if(cmd.equalsIgnoreCase("reset")) {
		myDFPlayer.reset();
	// needs sendStack public in library
	//} else if(cmd.equalsIgnoreCase("raw")) {
	//	myDFPlayer.sendStack(p1,p2,p3);
	} else if(cmd.equalsIgnoreCase("init")) {
		init_dfPlayer();
	}
}

void dfPlayerStatus() {
	String response;
	int i;
	
	response  = "Init:" + String(dfPlayerInit);
	for(i=0;i<4;i++) {
		response += "<BR>Button" + String(i) + ":" + String(pinStates[i]);
	}
	response += "<BR>Battery:" + String(battery_volts);
	response += "<BR>Volume:" + String(volume);
	response += "<BR>Mute:" + String(muteState);
	response += "<BR>Folder:" + String(folderSelect) + "," + folderMap[folderSelect] + "<BR>";
	server.send(200, "text/html", response);
}

void dfPlayerTest() {
	String response = "Test";
	
	response += "<BR>Available:" + String(myDFPlayer.available());
	response += "<BR>_handleType:" + String(myDFPlayer._handleType);
	response += "<BR>_handleCommand:" + String(myDFPlayer._handleCommand);
	response += "<BR>_handleParameter:" + String(myDFPlayer._handleParameter);
	response += "<BR>_isAvailable:" + String(myDFPlayer._isAvailable);
	response += "<BR>_isSending:" + String(myDFPlayer._isSending);
	//response += "<BR>State:" + String(myDFPlayer.readState()) + "<BR>";
	server.send(200, "text/html", response);
}

bool dfPlayerFinished() {
	myDFPlayer.available();
	return (myDFPlayer._handleCommand == 61);
}

int checkButtons() {
	int i;
	int pin;
	unsigned long period;
	int changed = 0;
	
	for(i=0; i<4;i++) {
		pin = digitalRead(pinInputs[i]);
		if(pin == 0 && pinStates[i] == 1){
			pinTimes[i] = elapsedTime;
		} else if(pin == 1 && (pinStates[i] == 0)) {
			changed = 1;
			period = (elapsedTime - pinTimes[i]) * timeInterval;
			pinChanges[i] = (period>LONG_PRESS) ? 2:1;
		}
		pinStates[i] = pin;
	}
	return changed;
}

void processButtons() {
	if(pinChanges[VOLUP] == 2) {
		digitalWrite(MUTE, 0);
		muteState = 0;
		saveConfig();
		pinChanges[VOLUP] = 0;
	} else if (pinChanges[VOLUP] == 1) {
		dfPlayerSetVolume(volume+1);
		pinChanges[VOLUP] = 0;
	} else if (pinChanges[VOLDOWN] == 2) {
		digitalWrite(MUTE, 1);
		muteState = 1;
		saveConfig();
		pinChanges[VOLDOWN] = 0;
	} else if (pinChanges[VOLDOWN] == 1) {
		dfPlayerSetVolume(volume-1);
		pinChanges[VOLDOWN] = 0;
	} else if (pinChanges[SELECT1] == 2) {
		myDFPlayer.loopFolder(folderSelect);
		pinChanges[SELECT1] = 0;
	} else if (pinChanges[SELECT1] == 1) {
		if(folderSelect < 99) folderSelect++;
		pinChanges[SELECT1] = 0;
	} else if (pinChanges[SELECT2] == 2) {
		myDFPlayer.randomAll();
		pinChanges[SELECT2] = 0;
	} else if (pinChanges[SELECT2] == 1) {
		if(folderSelect >1) folderSelect--;
		pinChanges[SELECT2] = 0;
	}
}

void checkBattery() {
	int adc;
	batteryCounter++;
	if(batteryCounter >= BATTERY_DIVIDER) {
		adc = analogRead(A0);
		battery_volts = (battery_volts * 15 + battery_mult * ADC_CAL * analogRead(A0)) / 16;
		batteryCounter = 0;
	}
}

void readConfig() {
	if(SPIFFS.exists("/config.txt")) {
		File f = SPIFFS.open("/config.txt", "r");;
		int Index = 0;
		String temp;
		
		while(f.available()) {
			temp = f.readStringUntil(10);
			temp.replace("\r","");
			if(temp.length() && temp.charAt(0) != '#') {
				switch(Index) {
					case 0: ADC_CAL = temp.toFloat();break;
					case 1: volume = temp.toInt();break;
					case 2: muteState = temp.toInt();break;
				}
				Index++;
			}
		}
		f.close();
	}
	Serial.print(F("ADC Calibration "));Serial.println(ADC_CAL);
}

void saveConfig() {
	File f = SPIFFS.open("/config.txt", "w");
	f.println(F("#ADC_CAL,volume,mute"));
	f.println(ADC_CAL);
	f.println(volume);
	f.println(muteState);
	f.close();
}

void setup()
{
	unusedIO();
	int i;
	for(i=0; i<4;i++) {
		pinMode(pinInputs[i], INPUT_PULLUP);
		pinStates[i] = 1;
		pinTimers[i] = elapsedTime;
	}
	Serial.begin(115200);
	Serial.println(F("Set up Web update service"));
	digitalWrite(MUTE, 1);
	pinMode(MUTE, OUTPUT);
	if(digitalRead(pinInputs[0]) == 0 && digitalRead(pinInputs[1]) == 0) {
		dfPlayerInit = INIT_SKIP;
		Serial.println(F("Skip init dfPlayer and format SPIFFS"));
	}
	mySoftwareSerial.begin(9600);
	wifiConnect(0);
	macAddr = WiFi.macAddress();
	macAddr.replace(":","");
	Serial.println(macAddr);

	//Update service
	MDNS.begin(host.c_str());
	httpUpdater.setup(&server, update_path, update_username, update_password);
	Serial.println(F("Set up Web command handlers"));
	server.on("/dfPlayer", dfPlayerCmd);
	server.on("/dfPlayerStatus", dfPlayerStatus);
	server.on("/test", dfPlayerTest);
	//Simple upload
	server.on("/upload", handleMinimalUpload);
	server.on("/format", handleSpiffsFormat);
	server.on("/list", HTTP_GET, handleFileList);
	//load editor
	server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");});
	//create file
	server.on("/edit", HTTP_PUT, handleFileCreate);
	//delete file
	server.on("/edit", HTTP_DELETE, handleFileDelete);
	//first callback is called after the request has ended with all parsed arguments
	//second callback handles file uploads at that location
	server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

	//called when the url is not defined here
	//use it to load content from SPIFFS
	server.onNotFound([](){if(!handleFileRead(server.uri())) server.send(404, "text/plain", "FileNotFound");});
	server.begin();

	MDNS.addService("http", "tcp", 80);
	Serial.println(F("Set up filing system"));
	initFS();
	delaymSec(500);
	Serial.println(F("Read Config"));
	readConfig();
	readFolders();
	Serial.println(F("Set up complete"));
}

void loop() {
	server.handleClient();
	wifiConnect(1);
	delaymSec(timeInterval);
	elapsedTime++;
	if(dfPlayerInit == INIT_START && elapsedTime * timeInterval > DFPLAYER_STARTUP) {
		Serial.println(F("Delayed init of dfPlayer"));
		dfPlayerInit = INIT_DELAY;
		init_dfPlayer();
		digitalWrite(MUTE, 0);
	}
	if(dfPlayerInit >= INIT_OK1 && checkButtons()) {
		processButtons();
	}
	checkBattery();
}

