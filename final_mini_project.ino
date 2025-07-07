// Blynk Configuration
#define BLYNK_TEMPLATE_ID "TMPL6RmiQN86M"
#define BLYNK_TEMPLATE_NAME "PORTABLE SPECTROMETER"
#define BLYNK_AUTH_TOKEN "x4jfbwdTMBVz_06rIsok42Th90JkGXyL"
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

// Hardware Configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define LDR_PIN 36
#define SERVO_PIN 13

// WiFi Configuration
const char* ssid = "Maisarah";
const char* password = "stevecomel";

// Google Sheets Configuration
const String GSCRIPT_URL = "https://script.google.com/macros/s/AKfycbzIo9brZMZOlZ0aUo6kWx0Lz1Xz5zT6UBo05nu0jXv2ifENbBTC1H2n8uQc5K8nMglQ/exec";

// Blynk Virtual Pins (Only V0-V3 as requested)
#define BLYNK_ANGLE_PIN V0
#define BLYNK_WAVELENGTH_PIN V1
#define BLYNK_LDR_PIN V2
#define BLYNK_CONTAMINATION_PIN V3

Servo diffractionServo;
int angles[] = {10, 15, 20, 25, 30};

// Calibration constants
const float a = 0.15;  // slope (nm per LDR unit)
const float b = 250.0; // intercept

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);

  // Initialize LCD
  Wire.begin(21, 22);
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("PORTABLE");
  lcd.setCursor(0, 1);
  lcd.print("SPECTROMETER");
  delay(2000);

  // Initialize Servo
  diffractionServo.setPeriodHertz(50);
  diffractionServo.attach(SERVO_PIN, 500, 2400);

  // Connect to WiFi and Blynk
  connectToWiFi();
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();
}

void connectToWiFi() {
  lcd.clear();
  lcd.print("Connecting WiFi");
  
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    lcd.clear();
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
  } else {
    Serial.println("\nConnection Failed");
    lcd.clear();
    lcd.print("WiFi Failed");
    lcd.setCursor(0, 1);
    lcd.print("Offline Mode");
    delay(2000);
  }
}

void sendToGoogleSheets(int angle, float wavelength, int ldrValue, String contamination) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = GSCRIPT_URL + "?angle=" + String(angle) +
               "&wavelength=" + String(wavelength, 2) +
               "&ldr=" + String(ldrValue) +
               "&contamination=" + urlEncode(contamination);
  
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.GET();
  http.end();
}

String urlEncode(String str) {
  String encoded;
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else if (c == ' ') {
      encoded += '+';
    } else {
      encoded += '%';
      encoded += String(c, HEX);
    }
  }
  return encoded;
}

String determineContamination(float wavelength) {
  if (wavelength < 200 || wavelength > 800) return "Invalid Range";
  if (abs(wavelength - 498) <= 5) return "Copper Ions";
  if (abs(wavelength - 405) <= 5) return "Zinc Oxide";
  if (abs(wavelength - 363) <= 5) return "Ammonium Nitrate";
  if (abs(wavelength - 361) <= 5) return "Organic Pesticide";
  if (wavelength >= 250 && wavelength <= 380) return "Organic Matter";
  if (wavelength >= 400 && wavelength <= 500) return "Heavy Metal";
  return "No contamination";
}

void loop() {
  Blynk.run();
  
  for (int i = 0; i < 5; i++) {
    int angle = angles[i];
    diffractionServo.write(angle);
    delay(1000);

    // Read sensor with averaging
    int ldrValue = 0;
    for (int j = 0; j < 5; j++) {
      ldrValue += analogRead(LDR_PIN);
      delay(200);
    }
    ldrValue /= 5;

    // Calculate wavelength
    float wavelength = a * ldrValue + b;
    String contamination = determineContamination(wavelength);

    // Update displays
    updateLCD(angle, wavelength, contamination);
    updateBlynk(angle, wavelength, ldrValue, contamination);
    sendToGoogleSheets(angle, wavelength, ldrValue, contamination);

    delay(3000);
  }

  // After scan completion
  lcd.clear();
  lcd.print("Scan Complete");
  lcd.setCursor(0, 1);
  lcd.print("Data Uploaded");
  
  while (true) {
    Blynk.run();
    delay(1000);
  }
}

void updateLCD(int angle, float wavelength, String contamination) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("A:");
  lcd.print(angle);
  lcd.print((char)223); // Degree symbol
  lcd.print(" WL:");
  lcd.print((int)wavelength);
  lcd.print("nm");
  
  lcd.setCursor(0, 1);
  lcd.print("C:");
  lcd.print(contamination.substring(0, 13));
}

void updateBlynk(int angle, float wavelength, int ldrValue, String contamination) {
  Blynk.virtualWrite(BLYNK_ANGLE_PIN, angle);
  Blynk.virtualWrite(BLYNK_WAVELENGTH_PIN, wavelength);
  Blynk.virtualWrite(BLYNK_LDR_PIN, ldrValue);
  Blynk.virtualWrite(BLYNK_CONTAMINATION_PIN, contamination);
}