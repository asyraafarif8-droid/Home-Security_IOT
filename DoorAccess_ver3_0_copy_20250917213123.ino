#define BLYNK_TEMPLATE_ID "TMPL6dpKE-jMm"
#define BLYNK_TEMPLATE_NAME "DOOR ACCESS IoT"
#define BLYNK_AUTH_TOKEN "Sq6DkMo-gXlb-OknpvfOGN5m393X4Aod"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ===== LCD setup =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== RFID setup (MFRC522v2) =====
#define SS_PIN 5
#define RST_PIN 0
MFRC522DriverPinSimple ss_pin(SS_PIN);
MFRC522DriverSPI driver(ss_pin, SPI);
MFRC522 mfrc522(driver);

// ===== Blynk setup =====
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Project CSC";
char pass[] = "abc123456";

// ===== Servo setup =====
Servo myServo;
int servoPin = 13;
bool isLocked = true;

#define LOCK_POS 90
#define UNLOCK_POS 0

// ===== Buzzer setup =====
#define BUZZER_PIN 15

// ===== Authorized RFID UIDs ===== TO BE EDITED
byte authorizedUIDs[][4] = {
  {0xFB, 0x9C, 0xBF, 0x01},
  {0xAC, 0xB6, 0x3E, 0x02},
  {0xAA, 0xBB, 0xCC, 0xDD}
};
int totalAuthorized = sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]);

// ===== Keypad setup =====
const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {12, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String password = "1234";
String inputPassword = "";

// ===== Timing control =====
unsigned long lastLCDUpdate = 0;
const unsigned long lcdInterval = 2000;

unsigned long messageUntil = 0;
bool showMessage = false;

// ===== Alarm control =====
bool alarmActive = false;
unsigned long unlockTime = 0;
const unsigned long alarmDelay = 10000; // 10 seconds

// ===== Function Prototypes =====
bool isAuthorized(byte *uid, byte uidSize);
void unlockDoor();
void lockDoor();
void playUnlockTone();
void playLockTone();
void playErrorTone();
void updateIdleScreen();
void checkWiFi();
void showMessageOnLCD(String msg, unsigned long duration = 2000);
void handleAlarm();

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door Access IoT");

  // RFID init
  mfrc522.PCD_Init();
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);

  // Servo init
  myServo.attach(servoPin);
  myServo.write(LOCK_POS);

  // Buzzer init
  pinMode(BUZZER_PIN, OUTPUT);

  // Blynk init
  Blynk.begin(auth, ssid, pass);

  delay(1000);
  updateIdleScreen();
}


void loop() {
  Blynk.run();
  checkWiFi();

  // ===== RFID Check =====
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    if (isAuthorized(mfrc522.uid.uidByte, mfrc522.uid.size)) {
      if (isLocked) unlockDoor();
      else lockDoor();
    } else {
      Serial.println("❌ Unauthorized card");
      showMessageOnLCD("Access Denied", 2000);
      playErrorTone();
    }
    mfrc522.PICC_HaltA();
  }

  // ===== Keypad Check =====
  char key = keypad.getKey();
  if (key) {
    if (key == '#') {
      if (inputPassword == password) {
        if (isLocked) unlockDoor();
        else lockDoor();
      } else {
        Serial.println("❌ Wrong Password");
        showMessageOnLCD("Wrong Password", 2000);
        playErrorTone();
      }
      inputPassword = "";
    }
    else if (key == '*') {
      inputPassword = "";
      showMessageOnLCD("Cleared", 1000);
    }
    else {
      inputPassword += key;
      lcd.setCursor(0, 0);
      lcd.print("Enter Password: ");
      lcd.setCursor(0, 1);
      for (int i = 0; i < inputPassword.length(); i++) lcd.print('*');
      lcd.print("                ");
    }
  }

  // ===== Alarm handling =====
  handleAlarm();

  updateIdleScreen();
}

// ===== UID Checker =====
bool isAuthorized(byte *uid, byte uidSize) {
  if (uidSize != 4) return false;
  for (int i = 0; i < totalAuthorized; i++) {
    if (memcmp(uid, authorizedUIDs[i], 4) == 0) return true;
  }
  return false;
}

// ===== Unlock Function =====
void unlockDoor() {
  if (isLocked) {
    myServo.write(UNLOCK_POS);
    showMessageOnLCD("Door Unlocked", 2000);
    playUnlockTone();
    Blynk.logEvent("door_unlocked", "Door opened");
    isLocked = false;
    unlockTime = millis();
    alarmActive = false;
  }
}

// ===== Lock Function =====
void lockDoor() {
  if (!isLocked) {
    myServo.write(LOCK_POS);
    showMessageOnLCD("Door Locked", 2000);
    playLockTone();
    Blynk.logEvent("door_locked", "Door closed");
    isLocked = true;
    alarmActive = false;
    noTone(BUZZER_PIN);
  }
}

// ===== Unlock Tone =====
void playUnlockTone() {
  tone(BUZZER_PIN, 1000, 150);
  delay(200);
  tone(BUZZER_PIN, 1500, 150);
  delay(200);
  noTone(BUZZER_PIN);
}

// ===== Lock Tone =====
void playLockTone() {
  tone(BUZZER_PIN, 800, 200);
  delay(250);
  tone(BUZZER_PIN, 600, 200);
  delay(250);
  noTone(BUZZER_PIN);
}

// ===== Error Tone =====
void playErrorTone() {
  tone(BUZZER_PIN, 400, 300);
  delay(350);
  tone(BUZZER_PIN, 300, 300);
  delay(350);
  noTone(BUZZER_PIN);
}

// ===== Show Temporary Message =====
void showMessageOnLCD(String msg, unsigned long duration) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg);
  messageUntil = millis() + duration;
  showMessage = true;
}

// ===== Update LCD Idle Screen =====
void updateIdleScreen() {
  static unsigned long lastBlink = 0;
  static bool showAlarmText = true;

  if (alarmActive && !isLocked) {
    if (millis() - lastBlink > 500) {
      lcd.clear();
      if (showAlarmText) {
        lcd.setCursor(0, 0);
        lcd.print("ALARM! DOOR OPEN");
        lcd.setCursor(0, 1);
        lcd.print("LOCK IMMEDIATELY");
      } else {
        lcd.clear();
      }
      showAlarmText = !showAlarmText;
      lastBlink = millis();
    }
    return;
  }

  if (showMessage && millis() < messageUntil) return;
  showMessage = false;

  if (millis() - lastLCDUpdate > lcdInterval) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(isLocked ? "Door Locked    " : "Door Unlocked  ");
    lcd.setCursor(0, 1);
    lcd.print("Scan card / Key ");
    lastLCDUpdate = millis();
  }
}

// ===== Alarm Handler =====
void handleAlarm() {
  static unsigned long lastBeep = 0;

  if (!isLocked && !alarmActive && millis() - unlockTime > alarmDelay) {
    alarmActive = true;
  }

  if (alarmActive && !isLocked) {
    if (millis() - lastBeep > 500) {
      static bool buzzerOn = false;
      if (buzzerOn) noTone(BUZZER_PIN);
      else tone(BUZZER_PIN, 1000);
      buzzerOn = !buzzerOn;
      lastBeep = millis();
    }
  }
}

// ===== Blynk Button (V1) =====
BLYNK_WRITE(V1) {
  int pinValue = param.asInt();
  if (pinValue == 1) {
    if (isLocked) unlockDoor();
    else lockDoor();
  }
}

// ===== Blynk Sync =====
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V1);
}

// ===== WiFi Reconnect =====
void checkWiFi() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);
    }
    lastCheck = millis();
  }
}
