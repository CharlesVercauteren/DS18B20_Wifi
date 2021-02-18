// DS18B20_Wifi
//
// ©2021 by Charles Vercauteren 
// 18 febrary 2021
//
// Tested on ARduino Uno Wifi.
//
// Nederlands:
//
// Applicatie:
// -----------
// Dit is een applicatie voor wifi configuratie software die ik geschreven heb. Er is ook een 
// bijpassende app voor macOS geschreven. Het is mogelijk om de tijd, het log interval en de naam
// in te stellen. We gebruiken hiertoe 1-byte commando's die we mbv. UDP versturen/ontvangen.
// Verbinden via terminal op Mac, gebruik "sudo cu -s 9600 -l /dev/tty.usbmodem14102". Gebruik "~." 
// om cu te verlaten (~ is Option-n).
// Aanpassen van dit gedeelte van de sketch laat ook toe andere sensoren te gebruiken.
//
// Wifi configuratie:
// ------------------
// Het SSID en wachtwoord voor het wifi-netwerk wordt gelezen uit EEPROM. Om deze waarden
// te configureren zal een drukknop, aangesloten ts D2 (zie #define PIN_CONFIG) en GND, 
// ons in het configuratiemenu brengen indien we deze ingedrukt houden bij een reset of 
// inschakelen van de voeding.
// We vragen dan een SSID, wachtwoord, DHCP/Fixed en eventueel het vaste IP-adres
// voor het draadloze netwerk en bewaren deze in de EEPROM van de processor. 
// Bij een volgende herstart of reset zullen dan deze waarden gebruikt worden.
//
// English:
//
// Application
// -----------
// This is an application for the wifi configuration software I wrote earlier. I have also
// written an accompayning app voor macOS. Time, log interval and name can be configured. We
// use 1-byte commands that we send/receive using UDP.
// Connection via terminal on Mac, use "sudo cu -s 9600 -l /dev/tty.usbmodem14102". Ise "~." to 
// leave cu (~ is Option-n).
// By adapting this part of the software other sensor can be used.
//
// Wifi configuration:
// -------------------
// The SSID and password for the wifi-network will be read out of EEPROM. To configure these
// values a pushbutton, connected between D2 (see #define PIN_CONFIG) and GND, will
// show the configurationmenu when activated during powerup or reset.
// You will be prompted for an SSID, password, name, DHCP/Fixed and if necessary the IP-address
// for the wifi-network. Values will be saved in EEPROM and used on next powerup or reset.
//
// Credits:
// Created 13 July 2010 by dlf (Metodo2 srl)
// Modified 31 May 2012 by Tom Igoe


// --- Wifi/EEPROM includes begin ---
#include <WiFiNINA.h>
#include <EEPROM.h>
#include <IPAddress.h>

// Seriële
#define MAX_CHAR 32               // Max. number of chars for ssid, password and name
#define CR '\r'                   // You can use CR or LF to end the input on the serial
#define LF '\n'                   // line, but nog both !!! (See also include thermometer)
bool dataAvailable = false;       // Data is available on serial.
bool ssidOK = false;              // SSID is read.
bool passwordOK = false;          // Password is read.
bool hostnameOK = false;          // Name is read/
bool dhcpOK = false;              // Choice DHCP/Fixed is made.
bool fixedIpOK = false;           // Fixed IP-address is read.
char receivedChars[MAX_CHAR];     // Number of chars received on serial.

// EEPROM
int ssidAddress = 0 ;             // EEPROM address ssid
int passwordAddress = MAX_CHAR;   // EEPROM address password
int hostnameAddress = 2*MAX_CHAR; // EEPROM address name
int dhcpModeAddress = 3*MAX_CHAR; // EEPROM address dhcp(1)/fixed(0) choice
int fixedIpAddress = 3*MAX_CHAR+1;// EEPROM address for fixed IP-address

String ssid = "ssid";             // Wifi SSID
String password = "wachtwoord";   // Wifi password (WPA or WEP key)
String hostname = "hostname";     // Network/bord/... name
byte dhcp = 0;                    // Use DHCP by default
String fixedIp = "127.0.0.1";     // Default fixed IP

// IP
bool dhcpMode = false;
IPAddress ip;                     
int status = WL_IDLE_STATUS;      // Wifi radio status

// Diverse
#define PIN_CONFIG   2
bool configMode = false;          // Confifuration mode active.
bool runOnce = true;              // Run code in loop only once.
// --- Wifi/EEPROM includes end ---


// --- Thermometre includes begin ---
#include <OneWire.h>
OneWire ds(6);

#include <WiFiUdp.h>
WiFiUDP Udp;
unsigned int localPort = 2000;
char packetBuffer[256];              // Buffer to hold incoming packet
char replyBuffer[2048];

// Commando's
#define GET_TEMPERATURE         10
#define SET_TIME                20
#define GET_TIME                21
#define SET_LOG_INTERVAL        22
#define GET_LOG_INTERVAL        23
#define GET_LOG                 30
#define GET_HOSTNAME            40
#define SET_HOSTNAME            41
#define UNKNOWN_COMMAND         99

#define END_MARKER  LF        // End marker for data on serial line (see also above)

// Diverse
bool ledOnOff = false;    

//Tijd
int command = 0;
short hours = 0;
short minutes = 0;
short seconds = 0;
short milliseconds = 0;
unsigned long prevMillis = 0;
unsigned long currentMillis = 0;

// Temperature log
#define MAX_LOG 48              // Number of log entries (adapt replyBuffer also !!!)
#define MAX_LOGSECONDS  3600    // Number of seconds between logs
short logSeconds = 0;
short maxLogSeconds = MAX_LOGSECONDS;
float currentTemperature = 0;
String currentTemperatureStr = "0.00";
float temperatureLog[MAX_LOG];
String timeLog[MAX_LOG];
int indexLog = 0;
// --- Thermometer includes einde ---


void setup() {
  
// --> Wifi/EEPROM setup begin ---
  // Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("No wifi module found, stopping !");
    // Don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // Pushbutton configuration active ?
  pinMode(PIN_CONFIG,INPUT_PULLUP);
  if (digitalRead(PIN_CONFIG)==LOW) { 
    configMode = true; 
    Serial.println("Config button pressed.");
  }
// <-- Wifi/EEPROM setup end ---

// --> Thermometer setup begin ---
  // Start UDP server
  if (Udp.begin(localPort) != 1) {  
    Serial.println("UDP can't be started, stopping !");
    while(true); 
  } 
  
  // Init logs
  for (int i=0; i<MAX_LOG; i++) { 
    temperatureLog[i] = 0;
    timeLog[i] = "99:99:99";
  }

  // Init temperatuur sensor
  byte data[8];
  ds.reset();
// <-- Thermometer setup einde ---
  
}

void loop() {
  
// --> Wifi/EEPROM loop begin ---
  if (configMode) {
    // Configmode
    // ----------
    // Opvragen SSID/wachtwoord/...
    static bool ssidRunOnce = false;      // Ask for SSID, show only once
    static bool passwordRunOnce = true;   // Ask for password, show only once
    static bool hostnameRunOnce = true;   // Ask for name, show only once
    static bool dhcpRunOnce = true;       // Ask for dhcp/fixed keuze, show only once
    static bool fixedIpRunOnce = true;    // Ask for IP-address, show only once
      
    if (!ssidRunOnce)     { Serial.print("\n  SSID: "); ssidRunOnce = true;  }
    if (!passwordRunOnce) { Serial.print("\n  Password: "); passwordRunOnce = true; }
    if (!hostnameRunOnce) { Serial.print("\n  Name: "); hostnameRunOnce = true; }
    if (!dhcpRunOnce)     { Serial.print("\n  DHCP/Fixed IP-address (D/F): "); dhcpRunOnce = true; }
    if (!fixedIpRunOnce)  { Serial.print("\n  IP-address (e.g. 192.168.0.1): "); fixedIpRunOnce = true; }

    recvString();
    if (dataAvailable) {
      if (!ssidOK) {
        // read ssid 
        ssid = String(receivedChars);
        ssidOK = true;
        dataAvailable = false;
        passwordRunOnce = false;
      }
      else if (!passwordOK) {
        // Read password
        password = String(receivedChars);
        passwordOK = true;
        dataAvailable = false;

        hostnameRunOnce = false;
      }
      else if (!hostnameOK) {
        // Read name
        hostname = String(receivedChars);
        hostnameOK = true;
        dataAvailable = false;

        dhcpRunOnce = false;
      }
      else if (!dhcpOK) {
        // DHCP mode ?
        if (String(receivedChars)=="D" || String(receivedChars)=="d") { 
          Serial.println("DHCP mode");
          dhcpMode = true;
          fixedIpOK = true;         // No need to ask for IP-address
          configMode = false;
          }
        else { 
          dhcpMode = false; 
          fixedIpRunOnce = false; 
          }
          
        dataAvailable = false;
        dhcpOK = true;
      }
      else if (!dhcpMode) {
        fixedIp = String(receivedChars);
        fixedIpOK = true;
        dataAvailable = false;

        configMode = false;
      }
    }

    
    if (ssidOK && passwordOK && hostnameOK && dhcpOK && fixedIpOK) {
      Serial.println("Saving SSID/password in EEPROM.");
      Serial.print("  SSID = "); Serial.println(ssid);
      Serial.print("  Password = "); Serial.println(password);
      Serial.print("  Name = "); Serial.println(hostname);
      if (dhcpMode) { 
        Serial.println("  DHCP mode"); 
        }
      else { 
        Serial.println("  Fixed IP-address"); 
        }
      Serial.println(); Serial.println("Leaving config mode.");

      saveSsidToEeprom(ssid);
      savePasswordToEeprom(password);
      saveHostnameToEeprom(hostname);
      saveDhcpModeToEeprom(dhcpMode);
      if (!dhcpMode) { 
        saveIpAddressToEeprom(fixedIp);
      }

      Serial.println();
    }
  }
  else {
    if (runOnce) {
      Serial.println();
      Serial.println("RunOnce");
      // Connect to wifi
      // ---------------
      // Read ssid/password/name
      ssid="";
      for (int i=0; i<MAX_CHAR; i++) { ssid+=(char)EEPROM.read(ssidAddress+i); }
      password="";
      for (int i=0; i<MAX_CHAR; i++) { password+=(char)EEPROM.read(passwordAddress+i); }
      hostname="";
      for (int i=0; i<MAX_CHAR; i++) { hostname+=(char)EEPROM.read(hostnameAddress+i); }
      if (EEPROM.read(dhcpModeAddress)==0) { 
        dhcpMode = false; 
        fixedIp = "";
        for (int i=0; i<MAX_CHAR; i++) { fixedIp+=(char)EEPROM.read(fixedIpAddress+i); }
        Serial.println("Fixed mode.");
        Serial.println(fixedIp);
        }
      else { 
        dhcpMode = true; 
        Serial.println("Dhcp mode.");
      }

      Serial.println("In EEPROM:");
      Serial.print("  SSID = "); Serial.println(ssid);
      Serial.print("  Password = "); Serial.println("********");
      Serial.print("  Name = "); Serial.println(hostname);
      Serial.print("  DHCP = ");
      if (dhcpMode) { Serial.println("Yes"); }
      else { 
        Serial.println("No");  
        Serial.print("  IP: "); Serial.println(fixedIp);
        }
      
      if (!dhcpMode) { 
        ip.fromString(fixedIp.c_str());
        WiFi.config(ip);
      }
      while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        // Connect to WPA/WPA2 network:
        status = WiFi.begin(ssid.c_str(), password.c_str());
        // Wacht 10 seconden en probeer opnieuw
        delay(10000);
      }

      // We'r" connected, print info.
      Serial.println("You're connected to the network");
      printCurrentNet();
      printWifiData();
      
      runOnce = false;
      Serial.println("Starting loop.");
    }
// <-- Wifi/EEPROM loop end ---

    else {
      
// --> Thermometer loop begin ---

      // Measure and log temperature
      currentTemperature = measureTemperature();
      logTemperature();
      
      // Replystring for client
      String replyString = "";

      // Read data from client
      int packetSize = Udp.parsePacket();
      if (packetSize) {
        // Data to packetBuffer
        int len = Udp.read(packetBuffer, 255);
        if (len > 0) {
          packetBuffer[len] = 0;    // Make c-string from buffer data
        }

        // Convert question from client to integer, commandos see higher.
        command = String(packetBuffer).toInt();
        // Create reply for client = command + " " + result
        // so client knows for which question the answer applies
        replyString = String(command) + " ";
        switch (command) {
          case GET_TEMPERATURE:
            replyString += String(currentTemperature); 
            break;
          case SET_TIME:
            hours = String(packetBuffer).substring(3,5).toInt();
            minutes = String(packetBuffer).substring(6,8).toInt();
            replyString += buildTimeString();
            break;
          case GET_TIME:
            replyString += buildTimeString();
            break;
          case SET_LOG_INTERVAL:
            maxLogSeconds = String(packetBuffer).substring(3).toInt();
            break;
          case GET_LOG_INTERVAL:
            replyString += String(maxLogSeconds);
            break;
          case GET_LOG:
            for (int i=indexLog; i<MAX_LOG; i++) {
              replyString += timeLog[i];
              replyString += "\t";
              replyString += String(temperatureLog[i]);
              replyString += "\n";
            }
            for (int i=0; i<indexLog; i++) {
              replyString += timeLog[i];
              replyString += "\t";
              replyString += String(temperatureLog[i]);
              replyString += "\n";
            }
            break;
          case GET_HOSTNAME:
            replyString += hostname; 
            break; 
          case SET_HOSTNAME:
            hostname = String(packetBuffer).substring(3);
            replyString += hostname;
            saveHostnameToEeprom(hostname);
            break;  
          default:
            replyString = String(UNKNOWN_COMMAND) + " "; 
            break;
        }

        // send a reply, to the IP address and port that sent us the packet we received
       Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
       replyString.toCharArray(replyBuffer,replyString.length()+1);
       Udp.write(replyBuffer);
       Udp.endPacket();
      }
  
      // updateClock creates 1s delay independant of run time loop
      // !!! This is not accurate enough !!!
      updateClock();
      
// <-- Thermometer loop end ---  
    }
  }
}

// --> EEPROM functions begin ---

void saveSsidToEeprom(String name) {
  for (int i=0; i<MAX_CHAR; i++) {
    EEPROM.write(ssidAddress+i,name.charAt(i));
  }
}

void savePasswordToEeprom(String name) {
  for (int i=0; i<MAX_CHAR; i++) {
    EEPROM.write(passwordAddress+i,name.charAt(i));
  }
}

void saveHostnameToEeprom(String name) {
  for (int i=0; i<MAX_CHAR; i++) {
    EEPROM.write(hostnameAddress+i,name.charAt(i));
  }
}

void saveDhcpModeToEeprom(bool mode) {
  if (mode) { 
    EEPROM.write(dhcpModeAddress, 1); 
    }
  else { 
    EEPROM.write(dhcpModeAddress, 0); 
  }
}

void saveIpAddressToEeprom(String name) {
  for (int i=0; i<MAX_CHAR; i++) {
    EEPROM.write(fixedIpAddress+i,name.charAt(i));
  }
}

// <-- EEPROM functions end ---


// --> Wifi functions begin ---

void printWifiData() {
  // Print IP-adres:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  // Print MAC-adres
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void recvString() {
  // Check for new data on serial.
  // If so, read it. If END_MARKER, close string with '\0'
  // to create a valid C-string. Make "dataAvailable" true.
  // Check the number of characters is smaller then  MAX_CHAR
  // and limit if necessary.
  
  static byte ndx = 0;
  char rc;

  while (Serial.available() > 0 && dataAvailable == false) {
    rc = Serial.read();
    Serial.print(rc);       //echo character

    if (rc != CR && rc != LF) {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= MAX_CHAR) {
        ndx = MAX_CHAR - 1;
      }
    }
    else {
     receivedChars[ndx] = '\0'; // terminate the string
     ndx = 0;
     dataAvailable = true;
    }
  }
}
// <-- Wifi functions end ---

// --> Thermometer functions begin ---

float measureTemperature() {
  // returns the temperatue in °C
  
  byte data[2];
  ds.reset(); 
  //Select all devices on bus and
  //start conversion
  ds.write(0xCC);
  ds.write(0x44);
  //Measure ones per second
  delay(1000);
  ds.reset();
  ds.write(0xCC);
  ds.write(0xBE);
  data[0] = ds.read(); 
  data[1] = ds.read();
  float temperature = 256*data[1] + data[0];
  if (temperature >= 32768) {
    // Positive temp
    // !!! Must be tested for negative temperatures !!!
    temperature = 65536 - temperature;
  }
  temperature = temperature/16;

  Serial.println(temperature);
  return(temperature);
}

void logTemperature() {
  // Logs the temperature every MAX_LOGSECONDS s

  if( logSeconds >= maxLogSeconds) {
    logSeconds = 0;
    temperatureLog[indexLog] = currentTemperature;
    timeLog[indexLog] = buildTimeString();
    #ifdef DEBUG
      Serial.println("Updating log");
    #endif
    indexLog += 1;
    
    // Rotate log if full
    if (indexLog>=MAX_LOG) { indexLog = 0; }
  }
}

String buildTimeString() {
  // Builds and returns a string format HH:MM:SS
  
  String timeString = "";
  
  if (hours<10) { timeString = "0" + String(hours); }
        else { timeString = String(hours) ;}
        timeString += ":";
        if (minutes <10) { timeString += "0" + String(minutes); }
        else { timeString += String(minutes) ; }
        timeString+= ":";
        if (seconds<10) { timeString += "0" + String(seconds); }
        else { timeString += String(seconds);
        }
  return timeString;
}

void updateClock() {
  // Update clock every second

  #ifdef DEBUG
  //Serial.print("Free memory: "); Serial.println(freeMemory());
  #endif
  
  // Create 1s delay independant of run time loop
  // Serial.println("Updating clock");
  short prevSeconds = 0;
  String str = "";

  //Wait 1s
  while ((currentMillis - prevMillis) < 1000) {
    currentMillis = millis();
  }
  prevMillis = currentMillis;

  //Update clock
  seconds += 1; 
  if (seconds >= 60) { minutes += 1; seconds=seconds - 60; }
  if (minutes >= 60) { hours += 1; minutes=minutes - 60; }
  if (hours >= 24) { hours = 0; }  
  // Update logging timer
  logSeconds += 1;
}
// <-- Thermometer functions end ---
