# dfPlayer application for ESP8266

See schematic for hook up.
Instructable at https://www.instructables.com/id/ESP8266-DfPlayer-Audio-Player/

## Features
- 4 local buttons for standalone use
- mobile browser interface
- Volume
- Play controls
- Folder selection
- Original folder names
- Mute control
- WifiManager for initial set up
- OTA for updates
- File browser for maintenance
- USB support into dfPlayer
- battery monitoring

## Basic Web interface (basic.htm)
### Commands are sent to ip/dfPlayer/cmd with arguments p1,p2,p3
- ?cmd=play&p1=folder&p2=track
- ?cmd=playmp3&p1=track
- ?cmd=volume&p1=level (0-30)
- ?cmd=stop
- ?cmd=volumeup
- ?cmd=volumedown
- ?cmd=speaker&p1=offon (0/1)
- ?cmd=pause
- ?cmd=start
- ?cmd=next
- ?cmd=previous
- ?cmd=mode&p1=type
- ?cmd=loopFolder&p1=folder
- ?cmd=random
- ?cmd=eq&p1=type
- ?cmd=device&p1=type
- ?cmd=setting&p1=folder&p2=track
- ?cmd=sleep
- ?cmd=reset
- ?cmd=raw&p1=cmdcode&p2=par1&p3=par2
- ?cmd=init

## Mobile browser interface (/)
- Works using folderMap (see tools)
- Volume slider
- Play controls
- Named folder list

## Other web support
- /edit for file browsing and upload
- /upload for simple file upload to bootstrap
- /dfPlayerStatus for basic status info
- /firmware for OTA

### Config
- Edit dfPlayer.ino
	- Manual Wifi set up (Comment out WM_NAME)
		- AP_SSID Local network ssid
		- AP_PASSWORD 
		- AP_IP If static IP to be used
	- Wifi Manager set up
		- WM_NAME (ssid of wifi manager network)
		- WM_PASSWORD (password to connect)
		- On start up connect to WM_NAME network and browse to 192.168.4.1 to set up wifi
	- AP_PORT to access web services
	- update_username user for updating firmware
	- update_password
	
### Libraries
- BitMessages Routines to look up and create pulse sequences for a commands
- BitTx Bit bang routines to execute a pulse sequence
	- Interrupt driven and supports accurate modulation
- WifiManager
- FS
- DNSServer
- ArduinoJson
- ESP8266mDNS
- ESP8266HTTPUpdateServer
- SoftwareSerial.h
- DFRobotDFPlayerMini.h
- ESP8266WiFi.h

### Install procedure
- Normal arduino esp8266 compile and upload
- A simple built in file uploader (/upload) should then be used to upload the base files to SPIFF
  edit.htm.gz
  index.html (can be changed to refer to different favicon)
  favicon*.png
  graphs.js.gz
- The /edit operation is then supported to upload basic.htm and SD naming files
	
### Tool for folder / track naming
The dfPlayer uses a very simple folder and track naming scheme
folders are 01 to 99
tracks are 001,002 

To avoid doing tedious renaming and also to support the mobile web interface a vbscript is used

- Copy normal folders and tracks mp3 to SD card (e.g. album names for folders)
- Run the vbscript and point it at the SD card volume&p1
- Tool will run and in a few seconds will rename all the files
- It will create a file called Folders.txt mapping the renamed folders to their original names
- It will also create a set of Tracks.txt file mapping the tracks to their original names (currently unused)
- The Folders.txt should be uploaded to the ESP8266 via /edit
- Tracks files may also be uploaded. Software may be enhanced to use these at some stage
- SD card can be maintained by deleting folders, adding new named ones and re-running the tool. Renaming will be done on new folders and new map files produced.

### Mute
The dfPlayer does not naturally support mute. Volume can be set to 0 but this also mutes the headphone output.

To get separate speaker mute control a small hardware mod is needed. Replace the 0 Ohm resistor with 10K and solder a mute control onto the pad as shown in the picture in docs. This mute control goes to GPIO16
