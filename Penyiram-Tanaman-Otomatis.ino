#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID "TMPL6yya_5tAz"
#define BLYNK_TEMPLATE_NAME "Tugas TMA Sistem Penyiram Tanaman Otomatis"
#define BLYNK_AUTH_TOKEN "6SngNVgQdK2R2LEWDZPZz8arXjd1LoEP"

#include <LiquidCrystal.h>
#include <DHT_U.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#define LEDred 23
#define LEDgreen 22
#define DHTPIN 12
#define SERVOPIN 14
#define TRIG_PIN 26 
#define ECHO_PIN 27 

#define DHTTYPE DHT22 
DHT_Unified dht(DHTPIN, DHTTYPE);

Servo servo;
LiquidCrystal lcd(2, 0, 4, 16, 17, 5);
BlynkTimer timer;

float temp, hum;
long duration;
int distance;
bool switchState = false;
bool isWaterEmpty = false;
bool blinkState = false;

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

void updateBlynkLED(bool isWatering) {
  if (isWatering) {
    Blynk.virtualWrite(V3, 255); 
    Blynk.virtualWrite(V5, 0);   
  } else {
    Blynk.virtualWrite(V3, 0);   
    Blynk.virtualWrite(V5, 255); 
  }
}

void updateLCD() {
  lcd.clear();
  if (switchState) {
    if (isWaterEmpty) {
      lcd.setCursor(0, 0);
      lcd.print("! AIR HABIS !");
      lcd.setCursor(0, 1);
      lcd.print("Isi Tangki...");
    } else {
      lcd.setCursor(0, 0);
      lcd.print("S:"); lcd.print(temp, 1); lcd.print("C");
      lcd.setCursor(9, 0);
      lcd.print("A:"); 
      lcd.print(distance);
      
      lcd.setCursor(0, 1);
      lcd.print("K:"); lcd.print(hum, 1); lcd.print("%");
    }
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Sistem Mati");
    lcd.setCursor(0, 1);
    lcd.print("OFF");
  }
}

void readSensors() {
  if (!switchState) return;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;
  
  isWaterEmpty = (distance > 180);
  Blynk.virtualWrite(V6, distance);

  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    temp = event.temperature;
    Blynk.virtualWrite(V0, temp);
  }

  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) {
    hum = event.relative_humidity;
    Blynk.virtualWrite(V1, hum);
  }
}

void runControlLogic() {
  updateLCD();

  if (switchState) {
    if (isWaterEmpty) {
      servo.write(0);
      Blynk.virtualWrite(V2, 0);
      digitalWrite(LEDgreen, LOW);
      updateBlynkLED(false);
    } else {
      if (hum < 30.0) {
        digitalWrite(LEDgreen, HIGH);
        digitalWrite(LEDred, LOW);
        updateBlynkLED(true);

        if (temp > 30.0) {
          servo.write(90);
          Blynk.virtualWrite(V2, 100);
        } else {
          servo.write(60);
          Blynk.virtualWrite(V2, 50);
        }
      } else {
        servo.write(0);
        Blynk.virtualWrite(V2, 0);
        digitalWrite(LEDgreen, LOW);
        digitalWrite(LEDred, HIGH);
        updateBlynkLED(false);
      }
    }
  } else {
    // SISTEM MATI
    servo.write(0);
    Blynk.virtualWrite(V2, 0);
    digitalWrite(LEDgreen, LOW);
    digitalWrite(LEDred, HIGH);
    updateBlynkLED(false);
  }
}

void blinkWarning() {
  if (switchState && isWaterEmpty) {
    blinkState = !blinkState;
    digitalWrite(LEDred, blinkState);
  }
}

void setup() {  
  pinMode(LEDred, OUTPUT);
  pinMode(LEDgreen, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.begin(9600);    
  
  lcd.begin(16, 2);                  
  lcd.clear();
  lcd.print("Connecting...");
  
  Blynk.begin(auth, ssid, pass);

  dht.begin();
  servo.attach(SERVOPIN, 500, 2400);
  servo.write(0);

  lcd.clear();                   
  lcd.setCursor(0, 0);
  lcd.print("Sistem Penyiram");
  lcd.setCursor(0, 1);
  lcd.print("Otomatis Ready");
  
  timer.setInterval(1000L, []() {
    readSensors();
    runControlLogic();
  });

  timer.setInterval(300L, blinkWarning);

  Blynk.syncVirtual(V4); 
}

void loop() {
  Blynk.run();
  timer.run();
}

BLYNK_WRITE(V4) {
  switchState = param.asInt();
  runControlLogic(); 
}