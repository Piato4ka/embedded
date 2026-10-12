#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// Налаштування Wi-Fi (змініть на свої дані)
const char* ssid = "Nokstella";
const char* password = "FourWordsAllCaps&24";

WebServer server(80);

const int SENSOR_PIN = 6; 
const int RELAY_PIN = 4;  

const int THRESHOLD_DARK = 2200;  
const int THRESHOLD_LIGHT = 2800; 

const float EMA_ALPHA = 0.10;
float emaValue = 0;

unsigned long lastPrintTime = 0;        

// Змінні для ручного/веб-керування
bool webControlEnabled = false; // false = AUTO, true = MANUAL
bool relayState = false;        // Поточний стан реле

// HTML-сторінка для керування
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<meta http-equiv=\"refresh\" content=\"1\">";
  html += "<style>body{font-family:Arial; text-align:center; margin-top:50px;} ";
  html += ".btn{padding:15px 30px; font-size:20px; cursor:pointer; background-color:#4CAF50; color:white; border:none; border-radius:5px; margin: 5px; width: 200px;}";
  html += ".btn-off{background-color:#f44336;}";
  html += ".btn-mode{background-color:#2196F3;}</style></head><body>"; // Синя кнопка для режимів
  
  html += "<h2>ESP32-S3 LDR & Relay</h2>";
  html += "<p>Raw Value: <b>" + String((int)emaValue) + "</b></p>";
  
  // ---------------- БЛОК РЕЖИМІВ ----------------
  html += "<p>Current Mode: <span style=\"font-size:20px; font-weight:bold; color:" + String(webControlEnabled ? "#f44336" : "#4CAF50") + ";\">";
  html += String(webControlEnabled ? "MANUAL" : "AUTO") + "</span></p>";
  
  if (webControlEnabled) {
    // Якщо ручний режим, показуємо кнопку переходу в АВТО
    html += "<a href=\"/auto\"><button class=\"btn btn-mode\">Switch to AUTO</button></a><br><br>";
  } else {
    // Якщо авто режим, показуємо кнопку переходу в РУЧНИЙ
    html += "<a href=\"/manual\"><button class=\"btn btn-mode\">Switch to MANUAL</button></a><br><br>";
  }
  // ----------------------------------------------

  // Блок керування реле
  html += "<hr style=\"width:80%; margin-top:20px; margin-bottom:20px;\">";
  html += "<p>Relay Status: <b>" + String(relayState ? "ON" : "OFF") + "</b></p>";
  
  if (relayState) {
    html += "<a href=\"/toggle\"><button class=\"btn btn-off\">Turn OFF</button></a>";
  } else {
    html += "<a href=\"/toggle\"><button class=\"btn\">Turn ON</button></a>";
  }
  
  html += "<br><br><p><a href=\"/\"🔄 Refresh Page</a></p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Функція перемикання реле
void handleToggle() {
  webControlEnabled = true; // Примусово переходимо в ручний режим при натисканні
  relayState = !relayState;
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
  Serial.printf("Relay: %s (Manual control)\n", relayState ? "ON" : "OFF");
  server.sendHeader("Location", "/");
  server.send(303);
}

// Функція ввімкнення АВТО режиму
void handleAuto() {
  webControlEnabled = false;
  Serial.println("Mode changed to: AUTO");
  server.sendHeader("Location", "/");
  server.send(303);
}

// Функція ввімкнення РУЧНОГО режиму
void handleManual() {
  webControlEnabled = true;
  Serial.println("Mode changed to: MANUAL");
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 
  analogReadResolution(12);

  emaValue = analogRead(SENSOR_PIN);

  // Підключення до Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connected!");
  Serial.print(">>> IP ADDRESS: http://");
  Serial.println(WiFi.localIP());

  // Налаштування маршрутів вебсервера
  server.on("/", handleRoot); //  маршрути
  server.on("/toggle", handleToggle);   
  server.on("/auto", handleAuto);       
  server.on("/manual", handleManual);   
  server.begin();
}

void loop() {
  server.handleClient(); // Обробка запитів від клієнтів Wi-Fi

  int rawValue = analogRead(SENSOR_PIN);
  emaValue = (EMA_ALPHA * rawValue) + ((1.0 - EMA_ALPHA) * emaValue);
  int smoothedValue = (int)emaValue; 
  
  if (millis() - lastPrintTime >= 1000) { // Виводимо раз на 1 секунду
    lastPrintTime = millis();
    Serial.printf("Mode: %s | Raw: %d | EMA: %d | Relay: %s\n", 
                  webControlEnabled ? "MANUAL" : "AUTO", 
                  rawValue, 
                  smoothedValue, 
                  relayState ? "ON" : "OFF");
  Serial.print(">>> IP ADDRESS: http://");
  Serial.println(WiFi.localIP());
  }

  // Логіка керування реле (ТІЛЬКИ якщо включено АВТО режим)
  if (!webControlEnabled) {
    if (smoothedValue < THRESHOLD_DARK && !relayState) {
      relayState = true;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Auto trigger: Relay ON");
    } 
    else if (smoothedValue > THRESHOLD_LIGHT && relayState) {
      relayState = false;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Auto trigger: Relay OFF");
    }
  }

  delay(100); 
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               