#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <WebSocketsServer.h>

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
//const char* PARAM_INPUT_3 = "ip";
//const char* PARAM_INPUT_4 = "gateway";

//Variables to save values from HTML form
String ssid;
String pass;
// String ip;
// String gateway;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
// const char* ipPath = "/ip.txt";
// const char* gatewayPath = "/gateway.txt";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledPin = 2;

// Variables to save signal timestamps
unsigned long lastSignalTime = 0;
unsigned long signalTimestamps[100];  // Array to store timestamps of pulses
int signalIndex = 0;  // Index to keep track of number of signals

// File path to save signal timestamps permanently
const char* signalsPath = "/signals.txt";

// Stores LED state
String ledState;

// Button
struct Button {
	const uint8_t PIN;
	uint32_t numberKeyPresses;
	bool pressed;
};

volatile bool newSignal = false;  // global flag

Button button1 = {18, 0, false};

// Function to get the current timestamp in real-time
String getCurrentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time. Are you connected to the internet?");
    return "N/A";  // In case time cannot be obtained
  }
  
  // Format timestamp as "YYYY-MM-DD HH:MM:SS"
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}

void IRAM_ATTR isr() {
  button1.numberKeyPresses++;
  button1.pressed = true;

  // Save timestamp of pulse
  lastSignalTime = millis();
  if (signalIndex < 100) {  // Avoid array overflow
    signalTimestamps[signalIndex] = lastSignalTime;
    signalIndex++;
  }
}

// Function to write the current timestamp to the file
void writeSignalToFile() {
  String timestamp = getCurrentTimestamp();  // Get the current timestamp
  
  File file = LittleFS.open(signalsPath, FILE_APPEND);  // Open the file in append mode
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  
  file.println(timestamp);  // Write the timestamp as a new line
  file.close();
  
  Serial.printf("Timestamp %s written to file.\n", timestamp.c_str());
}

// Function to read all saved timestamps from LittleFS
void readSignalsFromFile() {
  File file = LittleFS.open(signalsPath);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  
  Serial.println("Reading saved signal timestamps:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line);
  }
  file.close();
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {  // 'true' will format the filesystem if it fails to mount
    Serial.println("Failed to mount or format LittleFS. Please check filesystem.");
  } else {
    Serial.println("LittleFS mounted successfully");
  }
}

// Read File from LittleFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }
  
  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  return fileContent;
}

// Write file to LittleFS
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written successfully");
  } else {
    Serial.println("- write failed");
  }

  Serial.println("Reading file after writing...");
  Serial.println(readFile(LittleFS, ssidPath));
  Serial.println(readFile(LittleFS, passPath));
  // Serial.println(readFile(LittleFS, ipPath));
  // Serial.println(readFile(LittleFS, gatewayPath));

  file.close();
}

// Initialize WiFi
bool initWiFi() {
  if(ssid==""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  // localIP.fromString(ip.c_str());
  // localGateway.fromString(gateway.c_str());


  // if (!WiFi.config(localIP, localGateway, subnet)){
  //   Serial.println("STA Failed to configure");
  //   return false;
  // }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  if(var == "STATE") {
    if(digitalRead(ledPin)) {
      ledState = "ON";
    }
    else {
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}

// Setup button and ISR
void setupButtonPress() {
  pinMode(button1.PIN, INPUT_PULLUP);  // Enable internal pull-up resistor
  attachInterrupt(button1.PIN, isr, FALLING);  // Trigger on button press (FALLING edge)
}

void setupWiFiManager() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  initLittleFS();

  // Set GPIO 2 as an OUTPUT
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  // Load values saved in LittleFS
  ssid = readFile(LittleFS, ssidPath);
  pass = readFile(LittleFS, passPath);
  // ip = readFile(LittleFS, ipPath);
  // gateway = readFile (LittleFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  // Serial.println(ip);
  // Serial.println(gateway);

  if (initWiFi()) {
    Serial.println("WiFi connected. Waiting for NTP time sync...");
    delay(2000);  // Wait for NTP sync
  } else {
    Serial.println("Failed to connect to WiFi.");
  }

  if(initWiFi()) {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", LittleFS, "/");
    
    // Route to set GPIO state to HIGH
    server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, HIGH);
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });

    // Route to set GPIO state to LOW
    server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, LOW);
      request->send(LittleFS, "/index.html", "text/html", false, processor);
    });
    server.begin();
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting up Access Point");
    // NULL sets an open Access Point
    WiFi.softAP("JJDED-ESP-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("Access point IP: ");
    Serial.println(IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });
    
    server.serveStatic("/", LittleFS, "/");
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
          // // HTTP POST ip value
          // if (p->name() == PARAM_INPUT_3) {
          //   ip = p->value().c_str();
          //   Serial.print("IP Address set to: ");
          //   Serial.println(ip);
          //   // Write file to save value
          //   writeFile(LittleFS, ipPath, ip.c_str());
          // }
          // // HTTP POST gateway value
          // if (p->name() == PARAM_INPUT_4) {
          //   gateway = p->value().c_str();
          //   Serial.print("Gateway set to: ");
          //   Serial.println(gateway);
          //   // Write file to save value
          //   writeFile(LittleFS, gatewayPath, gateway.c_str());
          // }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: ");
      delay(3000);
      ESP.restart();
    });
    server.begin();
  }
  if (WiFi.status() != WL_CONNECTED) {
  Serial.println("WiFi not connected!");
  }
  else {
    Serial.println("WiFi connected!");
  }
}

void setup() {
  Serial.begin(115200);
  initLittleFS();  // Initialize LittleFS
  setupWiFiManager();
  setupButtonPress();

  // Read previously saved signals from file at startup
  readSignalsFromFile();

  // Open file for reading
  File file = LittleFS.open("/ssid.txt", "/pw.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Reading saved signal timestamps:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line);
  }

  file.close();
}

void loop() {  
  if (button1.pressed) {
    Serial.printf("Button has been pressed %u times\n", button1.numberKeyPresses);
    Serial.printf("Last signal timestamp: %lu ms\n", lastSignalTime);

    // Print all stored timestamps
    Serial.println("All signal timestamps:");
    for (int i = 0; i < signalIndex; i++) {
       Serial.printf("%lu ms\n", signalTimestamps[i]);
     }

    button1.pressed = false;
  }
}