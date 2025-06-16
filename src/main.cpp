#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// ตัวแปรเก็บค่า WiFi credentials
String wifi_ssid = "";
String wifi_password = "";

// กำหนด pin สำหรับ LED
const int LED_PIN = 2; // Built-in LED ของ ESP32

// สร้าง Web Server object
AsyncWebServer server(80);

// ตัวแปรสำหรับ blink LED
unsigned long previousMillis = 0;
const long interval = 1000; // blink ทุก 1 วินาที
bool ledState = false;


// ฟังก์ชันเริ่มต้น LittleFS
bool initLittleFS() {
  Serial.println("Initializing LittleFS...");
  
  if (!LittleFS.begin(true)) {
    Serial.println("An error occurred while mounting LittleFS");
    return false;
  }
  
  Serial.println("LittleFS initialized successfully");
  return true;
}

// ฟังก์ชันอ่านไฟล์ config.json
bool readConfig() {
  Serial.println("Reading config.json...");
  
  // ตรวจสอบว่าไฟล์มีอยู่หรือไม่
  if (!LittleFS.exists("/config.json")) {
    Serial.println("config.json file not found");
    return false;
  }
  
  // เปิดไฟล์อ่าน
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config.json for reading");
    return false;
  }
  
  // อ่านเนื้อหาไฟล์
  String jsonString = configFile.readString();
  configFile.close();
  
  Serial.println("Config file content:");
  Serial.println(jsonString);
  
  // แปลง JSON
  StaticJsonDocument<512> doc ;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // ดึงค่าจาก JSON
  if (doc.containsKey("wifi")) {
    JsonObject wifiObj = doc["wifi"].as<JsonObject>();
    if (wifiObj.containsKey("ssid") && wifiObj.containsKey("password")) {
      wifi_ssid = wifiObj["ssid"].as<String>();
      wifi_password = wifiObj["password"].as<String>();
      Serial.println("Config loaded successfully:");
      Serial.println("SSID: " + wifi_ssid);
      Serial.println("Password: " + String("*").substring(0, wifi_password.length())); // ซ่อน password
      return true;
    } else {
      Serial.println("Required keys not found in wifi object");
      return false;
    }
  } else {
    Serial.println("Required 'wifi' object not found in config.json");
    return false;
  }
}

// ฟังก์ชันแสดงข้อมูล WiFi
void printWiFiInfo() {
  Serial.println("=== WiFi Connection Info ===");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP Address: " + WiFi.localIP().toString());
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("Subnet Mask: " + WiFi.subnetMask().toString());
  Serial.println("DNS Server: " + WiFi.dnsIP().toString());
  Serial.println("Signal Strength (RSSI): " + String(WiFi.RSSI()) + " dBm");
  Serial.println("MAC Address: " + WiFi.macAddress());
  Serial.println("==========================");
}
// ฟังก์ชันเชื่อมต่อ WiFi
void connectWiFi() {
  if (wifi_ssid == "" || wifi_password == "") {
    Serial.println("WiFi credentials not loaded");
    return;
  }
  
  Serial.println("Connecting to WiFi...");
  Serial.println("SSID: " + wifi_ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  int attempts = 0;
  const int maxAttempts = 20; // ลองต่อสูงสุด 20 ครั้ง (40 วินาที)
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(2000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected successfully!");
    printWiFiInfo();
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
    Serial.println("Please check your credentials and network availability");
  }
}


// ฟังก์ชัน blink LED
void blinkLED() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
}
// ฟังก์ชันบันทึกค่า config
bool saveConfig(String ssid, String password) {
  Serial.println("Saving new config...");
  
  DynamicJsonDocument doc(512);
  doc["wifi_ssid"] = ssid;
  doc["wifi_password"] = password;
  
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config.json for writing");
    return false;
  }
  
  if (serializeJson(doc, configFile) == 0) {
    Serial.println("Failed to write to config.json");
    configFile.close();
    return false;
  }
  
  configFile.close();
  Serial.println("Config saved successfully");
  return true;
}
// ฟังก์ชันตั้งค่า Web Server
void setupWebServer() {
  Serial.println("Setting up Web Server...");
  
  // Route สำหรับหน้าแรก
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "index.html not found");
    }
  });
  
  // Route สำหรับอ่านค่า config
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(512);
    doc["wifi_ssid"] = wifi_ssid;
    doc["wifi_password"] = wifi_password;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // Route สำหรับบันทึกค่า config
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    String newSSID = "";
    String newPassword = "";
    
    if (request->hasParam("wifi_ssid", true)) {
      newSSID = request->getParam("wifi_ssid", true)->value();
    }
    if (request->hasParam("wifi_password", true)) {
      newPassword = request->getParam("wifi_password", true)->value();
    }
    
    if (newSSID != "" && newPassword != "") {
      if (saveConfig(newSSID, newPassword)) {
        wifi_ssid = newSSID;
        wifi_password = newPassword;
        request->send(200, "text/plain", "Config saved successfully! Restarting...");
        delay(2000);
        ESP.restart();
      } else {
        request->send(500, "text/plain", "Failed to save config");
      }
    } else {
      request->send(400, "text/plain", "Missing SSID or Password");
    }
  });
  
  // เริ่มต้น server
  server.begin();
  Serial.println("Web Server started!");
  Serial.println("Access web interface at: http://" + WiFi.localIP().toString());
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Starting ESP32 WiFi Connection...");
  
  // ตั้งค่า LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // เริ่มต้น LittleFS
  if (!initLittleFS()) {
    Serial.println("Failed to initialize LittleFS");
    return;
  }
  
  // อ่านค่า config จากไฟล์ JSON
  if (!readConfig()) {
    Serial.println("Failed to read config file");
    return;
  }
  
  // เชื่อมต่อ WiFi
  connectWiFi();
  
  // เริ่มต้น Web Server
  setupWebServer();
}

void loop() {
  // ตรวจสอบสถานะการเชื่อมต่อ WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    connectWiFi();
  }
  
  // เรียกใช้ function blink LED
  blinkLED();
  
  delay(100); // หน่วงเวลาเล็กน้อย
}
