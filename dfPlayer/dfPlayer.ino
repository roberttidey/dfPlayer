/***************************************************
dfPlayer - A Mini MP3 Player for ESP8266
R.J.Tidey March 2018
****************************************************/
#define ESP8266
#include "BaseConfig.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

int timeInterval = 25;
unsigned long elapsedTime;

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
String cmd;
String folderMap[100];
unsigned long resetPeriod;

//only check battery every 10th loop
#define BATTERY_DIVIDER 10
float ADC_CAL = 0.976;
float battery_mult = 57.0/10.0/1024;//resistor divider, vref, max count
float battery_volts = 4.2;
int batteryCounter = 0;

void init_dfPlayer() {
	Serial.println();
	Serial.println(F("Initializing DFPlayer ... (May take 2 seconds)"));

	//Use softwareSerial to communicate with mp3.
	//Extend timeout for getting reset message back (typically takes 1 second)	
	myDFPlayer.setTimeOut(DFMSG_TIMEOUT);
	resetPeriod = millis();
	if (myDFPlayer.begin(mySoftwareSerial)) {
		dfPlayerInit = INIT_OK1;
		Serial.print(F("DFPlayer Mini online."));
	} else {
		dfPlayerInit = INIT_OK2;
		Serial.print(F("DFPlayer Mini offline."));
	}
	resetPeriod = millis() - resetPeriod;
	Serial.println(" Reset period:" + String(resetPeriod));
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
	cmd = server.arg("cmd");
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
	response += "<BR>dfPlayercmd:" + cmd;
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
	response += "<BR>resetPeriod:" + String(resetPeriod) + "<BR>";
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

void setupStart()
{
	int i;
	for(i=0; i<4;i++) {
		pinStates[i] = 1;
		pinTimers[i] = elapsedTime;
	}
	digitalWrite(MUTE, 1);
	pinMode(MUTE, OUTPUT);
	if(digitalRead(pinInputs[0]) == 0 && digitalRead(pinInputs[1]) == 0) {
		dfPlayerInit = INIT_SKIP;
		Serial.println(F("Skip init dfPlayer and format SPIFFS"));
	}
}

void extraHandlers() {
	server.on("/dfPlayer", dfPlayerCmd);
	server.on("/dfPlayerStatus", dfPlayerStatus);
	server.on("/test", dfPlayerTest);
}

void setupEnd() {
	delaymSec(500);
	Serial.println(F("Read Config"));
	readConfig();
	readFolders();
	mySoftwareSerial.begin(9600);
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

