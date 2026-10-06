#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "BluetoothSerial.h"

// --- Config Pins ---
#define DHTPIN 4
#define DHTTYPE DHT22
     

// --- RGB LED PINS ---
#define RED_PIN 25
#define GREEN_PIN 26
#define BLUE_PIN 27
  // to contron plentiar module 
#define relayPin1 17
#define relayPin2 19
#define relayPin3 18  // NEW: Peltier Relay 3 (polarity flip ke liye)
#define baudRate 115200
#define wait 5000
 //  to controll humidity 
#define humiRelaypin 5

// --- PWM Properties ---
// नोट: ESP32 Core 3.0+ के लिए freq और resolution सही हैं
const int freq = 5000;
const int resolution = 8; 

// --- Objects ---
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // अगर स्क्रीन नहीं चले तो 0x27 try करें
BluetoothSerial SerialBT;

// --- Shared Data ---
float temperature = 0.0;
float humidity = 0.0;

// --- Mutex Handles ---
SemaphoreHandle_t xDataMutex = NULL; 
SemaphoreHandle_t xI2CMutex = NULL; 

// --- Task Handles ---
TaskHandle_t dhtTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t btTaskHandle = NULL;

// --- Function Prototypes ---
void dhtTask(void *parameter);
void displayTask(void *parameter);
void btTask(void *parameter);

// =====================================================
// PELTIER HELPER FUNCTIONS
// Relay2 aur Relay3 milke Peltier polarity control karte hain
// NC = Normally Closed = GND (relay deactivated)
// NO = Normally Open   = VCC (relay activated)
//
// IDLE:    Relay2=LOW(NC=GND),  Relay3=LOW(NC=GND)  → Peltier off (both GND)
// HEATING: Relay2=HIGH(NO=VCC), Relay3=LOW(NC=GND)  → current ek direction
// COOLING: Relay2=LOW(NC=GND),  Relay3=HIGH(NO=VCC) → current ulti direction
// =====================================================

void peltierIdle() {
  digitalWrite(relayPin2, HIGH);
  digitalWrite(relayPin3, HIGH);
}

void peltierHeating() {
  // Safety: pehle dono band karo
  peltierIdle();
  delay(50);
  // Relay2 NO = VCC, Relay3 NC = GND → forward current
  digitalWrite(relayPin2, LOW);
  digitalWrite(relayPin3, HIGH);
}

void peltierCooling() {
  // Safety: pehle dono band karo
  peltierIdle();
  delay(50);
  // Relay2 NC = GND, Relay3 NO = VCC → reverse current (polarity flip)
  digitalWrite(relayPin2, HIGH);
  digitalWrite(relayPin3, LOW
  
  
  
  
  );
}

void setup() {
  Serial.begin(115200);
  
  // Power stabilization delay
  delay(1000); 
  Serial.println("System Booting...");

  // 1. Hardware Init
  Wire.begin();
  dht.begin();
  
  // LCD Safety Check
  // I2C बस को स्कैन करने से कभी-कभी हैंग होने से बचा जा सकता है
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() == 0) {
    lcd.init();
    lcd.backlight();
    lcd.print("Booting...");
    delay(200);
    lcd.clear();
  
    Serial.print(
      "i am  satating"
    );
  } else {
    Serial.println("LCD Not Found at 0x3F");
  }

  // Peltier relays setup - default IDLE (dono GND)
  digitalWrite(relayPin2, HIGH); 
  digitalWrite(relayPin3, HIGH);
 
  pinMode(relayPin2, OUTPUT_OPEN_DRAIN);
  pinMode(relayPin3, OUTPUT_OPEN_DRAIN);

  peltierIdle();

  // Humidity relay setup
   digitalWrite(humiRelaypin, HIGH);
  pinMode(humiRelaypin, OUTPUT_OPEN_DRAIN);
  lcd.print("sytem is runing");


  // 3. RGB LED Setup (Compatible with ESP32 Core 3.0.x)
  // अगर आपके पास Core 2.x है, तो आपको ledcSetup और ledcAttachPin यूज़ करना होगा
  if (!ledcAttach(RED_PIN, freq, resolution)) Serial.println("Red PWM Fail");
  if (!ledcAttach(GREEN_PIN, freq, resolution)) Serial.println("Green PWM Fail");
  if (!ledcAttach(BLUE_PIN, freq, resolution)) Serial.println("Blue PWM Fail");

  ledcWrite(RED_PIN, 255);   // OFF (Common Anode logic assumed)
  ledcWrite(GREEN_PIN, 255);
  ledcWrite(BLUE_PIN, 255);

  // 4. Mutex Create
  xDataMutex = xSemaphoreCreateMutex();
  xI2CMutex  = xSemaphoreCreateMutex();

  if (xDataMutex == NULL || xI2CMutex == NULL) {
    Serial.println("Mutex Failed!");
    while(1);
  }

  // 5. Bluetooth Init
  SerialBT.begin("ESP32_Pro_Controller"); 
  Serial.println("Bluetooth Started!");

  // --- Tasks Created on CORE 1 (App Core) ---
  // Core 0 ब्लूटूथ के लिए छोड़ दिया है
  
  xTaskCreatePinnedToCore(
    dhtTask, "DHT Task", 4096, NULL, 1, &dhtTaskHandle, 1
  );

  xTaskCreatePinnedToCore(
    displayTask, "LCD Task", 6144, NULL, 1, &displayTaskHandle, 1
  );

  xTaskCreatePinnedToCore(
    btTask, "BT Task", 10240, NULL, 2, &btTaskHandle, 1
  );
  Serial.println("    fsldfjaso");
}

void loop() {
  // FreeRTOS में loop खाली छोड़ना सबसे अच्छा है
  vTaskDelete(NULL);
  Serial.println("hhh");
}

// --- Task 1: Sensor Reading --- (UNCHANGED)
void dhtTask(void *parameter) {
  for (;;) {
    // DHT पढ़ना धीमा होता है, इसे कम बार करें
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        temperature = t;
        humidity = h;
        xSemaphoreGive(xDataMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// --- Task 2: LCD Display --- (UPDATED)
void displayTask(void *parameter) {
  Serial.println("LCD Task Started");
  for (;;) {
    float t = NAN, h = NAN;

    // Data Copy
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      t = temperature;
      h = humidity;
      xSemaphoreGive(xDataMutex);
    }
 
    // I2C Mutex protection
    if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      // Print Temperature on Row 0
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      if (!isnan(t)) lcd.print(t, 1);
      else lcd.print("--.-");
      lcd.print(" C    "); // Extra spaces overwrite any old lingering characters

      // Print Humidity on Row 1
      lcd.setCursor(0, 1);
      lcd.print("Hum : ");
      if (!isnan(h)) lcd.print(h, 1);
      else lcd.print("--.-");
      lcd.print(" %    "); // Extra spaces overwrite any old lingering characters
      
      xSemaphoreGive(xI2CMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// --- Task 3: Bluetooth --- (ONLY RELAY LOGIC UPDATED)
void btTask(void *parameter) {
  String inputString = "";
  inputString.reserve(50); // मेमोरी बार-बार एलोकेट होने से बचाएं

  for (;;) {
    // 1. READ Incoming Data
    while (SerialBT.available()) {
      char incomingChar = SerialBT.read();
      if (incomingChar != '\n') {
        if (inputString.length() < 40) { // बफर ओवरफ्लो से सुरक्षा
             inputString += incomingChar;
        }
      } else {
        inputString.trim();
        
        // Commands
        if (inputString.length() > 0) {
     
            if (inputString.startsWith("R")) {
              int val = inputString.substring(1).toInt();
              ledcWrite(RED_PIN, 255 - val);
           }
           else if (inputString.startsWith("G")) {
              int val = inputString.substring(1).toInt();
              ledcWrite(GREEN_PIN, 255 - val);
           }
           else if (inputString.startsWith("B")) {
              int val = inputString.substring(1).toInt();
              ledcWrite(BLUE_PIN, 255 - val);
           }

          // =====================================================
          // TEMPERATURE / PELTIER CONTROL - UPDATED
          // Commands: "THEAT" = heating on
          //           "TCOOL" = cooling on
          //           "TOFF"  = peltier off (both GND/idle)
          // Safety: Heating aur Cooling kabhi ek saath nahi chalenge
          // =====================================================
          else if (inputString == "THEAT") {
            peltierHeating();
            Serial.println("Peltier: HEATING ON (Relay2=VCC, Relay3=GND)");
          }
          else if (inputString == "TCOOL") {
            peltierCooling();
            Serial.println("Peltier: COOLING ON (Relay2=GND, Relay3=VCC)");
          }
          else if (inputString == "TOFF") {
            peltierIdle();
            Serial.println("Peltier: OFF (both GND)");
          }

          // =====================================================
          // HUMIDITY CONTROL - UPDATED
          // Commands: "HON" = humidifier ON (manual switch)
          //           "HOFF" = humidifier OFF (manual switch)
          // =====================================================
          else if (inputString == "HON") {
            digitalWrite(humiRelaypin, HIGH);
            Serial.println("Humidifier: ON");
          }
          else if (inputString == "HOFF") {
            digitalWrite(humiRelaypin, LOW );
            Serial.println("Humidifier: OFF");
          }
        }
        inputString = "";
      }
    }

    // 2. SEND Data periodically (हर 1 सेकंड)
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 1000) {
      if (SerialBT.hasClient()) {
        float t = NAN, h = NAN;
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          t = temperature;
          h = humidity;
          xSemaphoreGive(xDataMutex);
        }
        // String creation को कम से कम रखें
        if(!isnan(t)) {
            SerialBT.print(t, 1);
            SerialBT.print(",");
            SerialBT.println(h, 1);
        }
      }
      lastSendTime = millis();
    }
    
    vTaskDelay(pdMS_TO_TICKS(20)); // Bluetooth स्टैक को सांस लेने दें
  }
}