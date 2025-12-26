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
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LEDred 23
#define LEDgreen 22
#define DHTPIN 12
#define SERVOPIN 14
#define TRIG_PIN 26 
#define ECHO_PIN 27 
#define POT_PIN 34

#define DHTTYPE DHT22 
DHT_Unified dht(DHTPIN, DHTTYPE);

Servo servo;
LiquidCrystal lcd(2, 0, 4, 16, 17, 5);

RTC_DS1307 rtc;
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

BlynkTimer timer;

float temp = 0.0, hum = 0.0;
int distance = 0;
int soilMoisture = 0;

bool switchState = false;
bool isWaterEmpty = false;
bool blinkState = false;

volatile unsigned long echoStartTime = 0;
volatile unsigned long echoDuration = 0;
volatile bool newDistanceAvailable = false;

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

void IRAM_ATTR echoISR() {
  if (digitalRead(ECHO_PIN) == HIGH) {
    echoStartTime = micros();
  } else {
    echoDuration = micros() - echoStartTime;
    newDistanceAvailable = true;
  }
}

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
      lcd.print("T:"); lcd.print((int)temp); lcd.print("C ");
      lcd.print("H:"); lcd.print((int)hum); lcd.print("%");
      
      lcd.setCursor(0, 1);
      lcd.print("Soil:"); lcd.print(soilMoisture); lcd.print("% ");
      lcd.print("W:"); lcd.print(distance); lcd.print("cm");
    }
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Sistem Mati");
    lcd.setCursor(0, 1);
    lcd.print("OFF");
  }
}

void updateOLED() {
  DateTime now = rtc.now();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  display.setCursor(0, 0);
  display.print("Time: ");
  display.print(now.hour()); display.print(':');
  if(now.minute() < 10) display.print('0');
  display.println(now.minute());

  display.setCursor(0, 15);
  if (switchState) {
    if (isWaterEmpty) {
      display.setTextSize(2);
      display.println("NO WATER!");
    } else {
      display.print("Soil: "); 
      display.print(soilMoisture); 
      display.println(" %");
      
      display.setCursor(0, 35);
      if (soilMoisture < 30) {
        display.println("STATUS: [ON]");
        display.println("WATERING...");
      } else {
        display.println("STATUS: [OFF]");
        display.println("MOIST OK");
      }
    }
  } else {
    display.setTextSize(2);
    display.println("SYS OFF");
  }
  display.display();
}

void triggerUltrasonic() {  
  digitalWrite(TRIG_PIN, LOW);
  long startWait = micros();
  while (micros() - startWait < 2);
  
  digitalWrite(TRIG_PIN, HIGH);
  startWait = micros();
  while (micros() - startWait < 10);
  
  digitalWrite(TRIG_PIN, LOW);
}

void readSensors() {
  int potValue = analogRead(POT_PIN);
  soilMoisture = map(potValue, 0, 4095, 0, 100);
  Blynk.virtualWrite(V7, soilMoisture); 

  if (!switchState) return;

  if (newDistanceAvailable) {
    distance = echoDuration * 0.034 / 2;
    newDistanceAvailable = false;
    
    if (distance > 200 || distance < 0) distance = 200;

    isWaterEmpty = (distance > 180); 
    Blynk.virtualWrite(V6, distance);
  }

  triggerUltrasonic();

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
  updateOLED();

  if (switchState) {
    if (isWaterEmpty) {
      servo.write(0);
      Blynk.virtualWrite(V2, 0);
      digitalWrite(LEDgreen, LOW);
      updateBlynkLED(false);
    } else {
      if (soilMoisture < 30) { 
        digitalWrite(LEDgreen, HIGH); 
        digitalWrite(LEDred, LOW);
        updateBlynkLED(true);

        if (temp > 30.0) {
          servo.write(180); 
          Blynk.virtualWrite(V2, 100);
        } else {
          servo.write(90);  
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
  
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);
  
  Serial.begin(9600);     

  lcd.begin(16, 2);                  
  lcd.clear();
  lcd.print("Connecting...");
  
  Wire.begin(); 
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.display();
  display.clearDisplay();

  Blynk.begin(auth, ssid, pass);

  dht.begin();
  servo.attach(SERVOPIN, 500, 2400);
  servo.write(0);

  lcd.clear();                    
  lcd.setCursor(0, 0);
  lcd.print("Smart Farming");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  
  timer.setInterval(1000L, []() {
    readSensors();
    runControlLogic();
  });

  timer.setInterval(500L, blinkWarning);

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