// +=====================================================================+
// |  Biomedionics -- Isometri Muscle Meter  |  Firmware v4.0            |
// |                                                                      |
// |  NEW: WiFiManager -- No buttons needed for WiFi setup!              |
// |                                                                      |
// |  How it works:                                                       |
// |  1. Device ON karo                                                   |
// |  2. Phone mein WiFi settings -> "Biomedionics_Setup" connect karo   |
// |  3. Web page khulega -> apna WiFi SSID + Password dalo              |
// |  4. Save karo -> device restart -> Firebase connect!                |
// |                                                                      |
// |  Reset WiFi: GPIO32 button 5 sec hold -> settings erase             |
// |                                                                      |
// |  Libraries (Sketch -> Manage Libraries):                            |
// |  1. WiFiManager          by tzapu                                   |
// |  2. HX711                by bogde                                   |
// |  3. Adafruit SSD1306     by Adafruit                                |
// |  4. Adafruit GFX         by Adafruit                                |
// |  5. Firebase ESP32       by Mobizt                                  |
// +=====================================================================+

#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <HX711.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FirebaseESP32.h>

// ======================================================================
//  FIREBASE CONFIG
// ======================================================================
#define FIREBASE_HOST  "biomedionics-5d664-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH  "Pjfop6VbVGIWrCMYLrooGC7gTC9UlceDS3j3a0Il"

// ======================================================================
//  HARDWARE PINS
// ======================================================================
#define PIN_HX711_DOUT    16
#define PIN_HX711_SCK     17
#define OLED_SDA          21
#define OLED_SCL          22
#define OLED_WIDTH       128
#define OLED_HEIGHT       64
#define OLED_ADDR        0x3C
#define PIN_RESET_BTN     32   // Hold 5s to reset WiFi settings

// ======================================================================
//  CALIBRATION
// ======================================================================
#define CALIBRATION_FACTOR   2280.0f
#define CALIBRATION_MODE     false

// ======================================================================
//  TIMING
// ======================================================================
#define READING_INTERVAL_MS   800
#define CONTRACTION_THR_N     5.0f
#define RESET_HOLD_MS         5000   // 5 seconds hold to reset WiFi

// ======================================================================
//  OBJECTS
// ======================================================================
Adafruit_SSD1306  oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
HX711             scale;
FirebaseData      fbdo;
FirebaseAuth      fbAuth;
FirebaseConfig    fbConfig;
WiFiManager       wifiManager;

// ======================================================================
//  GLOBAL STATE
// ======================================================================
String  deviceID   = "";
String  fbPath     = "";
String  localIP    = "";
bool    fbReady    = false;

// 7 session parameters
float   mvc_N      = 0, mvc_Kg    = 0, peakN       = 0;
float   timeToPeakS= 0, rfd       = 0, rfdMax      = 0;
float   prevForceN = 0, initForcN = 0, finalForceN = 0;
float   fatigueIdx = 0, impulse   = 0, forceSumN   = 0;
bool    initCaptur = false;
unsigned long sessionStartMs = 0, peakTimeMs = 0;
unsigned long lastReadTime   = 0;
int     readingCnt = 0;
float   dt_s       = READING_INTERVAL_MS / 1000.0f;
bool    g_sessionActive = false;
String  lastSerialKey = "";

// ======================================================================
//  DISPLAY HELPERS
// ======================================================================
void oledClear()  { oled.clearDisplay(); }
void oledShow()   { oled.display(); }

void show(String h, String l1, String l2, String l3, String foot,
          bool force = false) {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0); oled.print(h);
  oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  if (l1.length()) { oled.setCursor(0, 13); oled.print(l1); }
  if (l2.length()) { oled.setCursor(0, 24); oled.print(l2); }
  if (l3.length()) { oled.setCursor(0, 35); oled.print(l3); }
  if (foot.length()) {
    oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1); oled.setCursor(2, 54);
    oled.print(foot);
    oled.setTextColor(SSD1306_WHITE);
  }
  oled.display();

  String key = h+l1+l2+l3+foot;
  if (force || key != lastSerialKey) {
    lastSerialKey = key;
    Serial.println(F(""));
    Serial.println(F("+-------------------------------------+"));
    if (h.length())    Serial.println("|  " + h);
    if (l1.length())   Serial.println("|  " + l1);
    if (l2.length())   Serial.println("|  " + l2);
    if (l3.length())   Serial.println("|  " + l3);
    if (foot.length()) Serial.println("|  [" + foot + "]");
    Serial.println(F("+-------------------------------------+"));
  }
}

void showLive(float fN, float fKg, float pctMVC, float mvc,
              float rfdVal, float ttpVal, float fi, float imp,
              float avgF, int rdg, float elS, bool screenB) {
  String p1 = deviceID.substring(0,6);
  String p2 = deviceID.substring(6,12);

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print(" LIVE " + p1 + " " + p2);
  oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  if (!screenB) {
    oled.setTextSize(2); oled.setCursor(0, 13);
    oled.print(String(fN, 1) + "N");
    oled.setTextSize(1); oled.setCursor(72, 13);
    oled.print(String(pctMVC, 0) + "%MVC");
    oled.setCursor(0, 34);
    oled.print("MVC:" + String(mvc, 1) + "N");
    oled.setCursor(68, 34);
    oled.print("RFD:" + String(rfdVal, 0));
    oled.setCursor(0, 44);
    oled.print(String(fKg, 2) + "kg");
    oled.setCursor(50, 44);
    oled.print("t:" + String(elS, 1) + "s");
  } else {
    oled.setTextSize(1);
    oled.setCursor(0, 13); oled.print("TTP  : " + String(ttpVal, 2) + " s");
    oled.setCursor(0, 24); oled.print("FI   : " + String(fi, 1) + " %");
    oled.setCursor(0, 35); oled.print("Imp  : " + String(imp, 1) + " Ns");
    oled.setCursor(0, 45); oled.print("Avg  : " + String(avgF, 1) + " N");
  }

  oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(1); oled.setCursor(2, 54);
  oled.print(" CONNECTED | Sending");
  oled.setTextColor(SSD1306_WHITE);
  oled.display();

  Serial.println(
    "[DATA] #" + String(rdg) +
    " | F:" + String(fN,2) + "N" +
    " | MVC:" + String(mvc,2) + "N" +
    " | %MVC:" + String(pctMVC,1) +
    " | RFD:" + String(rfdVal,1) + "N/s" +
    " | TTP:" + String(ttpVal,2) + "s" +
    " | FI:" + String(fi,1) + "%" +
    " | Imp:" + String(imp,2) + "Ns" +
    " | t:" + String(elS,1) + "s"
  );
}

// ======================================================================
//  WIFIMANAGER CALLBACKS
// ======================================================================

// Called when AP mode starts (no saved WiFi found)
void onAPStarted(WiFiManager* wm) {
  Serial.println(F("[WiFiManager] AP mode started!"));
  Serial.println(F("[WiFiManager] Connect to: Biomedionics_Setup"));
  Serial.println(F("[WiFiManager] Then open: 192.168.4.1"));

  // Show on OLED
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);  oled.print(" WiFi Setup Needed");
  oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  oled.setCursor(0, 13); oled.print(" Connect to:");
  oled.setTextSize(1);
  oled.setCursor(0, 23); oled.print(" Biomedionics_Setup");
  oled.setCursor(0, 33); oled.print(" Then open browser");
  oled.setCursor(0, 43); oled.print(" 192.168.4.1");
  oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(2, 54); oled.print(" ID: " + deviceID);
  oled.setTextColor(SSD1306_WHITE);
  oled.display();
}

// ======================================================================
//  RESET BUTTON CHECK (call in loop)
// ======================================================================
void checkResetButton() {
  if (digitalRead(PIN_RESET_BTN) == LOW) {
    unsigned long pressStart = millis();

    // Show countdown on OLED
    while (digitalRead(PIN_RESET_BTN) == LOW) {
      int held = (millis() - pressStart) / 1000;
      int remaining = 5 - held;

      oled.clearDisplay();
      oled.setTextColor(SSD1306_WHITE);
      oled.setTextSize(1);
      oled.setCursor(0, 0);  oled.print(" RESET WIFI?");
      oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
      oled.setCursor(0, 18); oled.print(" Hold " + String(remaining) + " more sec...");
      oled.setCursor(0, 30); oled.print(" Release = cancel");
      oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      oled.setCursor(2, 54); oled.print(" Keep holding...");
      oled.setTextColor(SSD1306_WHITE);
      oled.display();

      if (millis() - pressStart >= RESET_HOLD_MS) {
        // 5 seconds held -- erase WiFi and restart
        Serial.println(F("[RESET] 5s held -- Erasing WiFi credentials!"));

        oled.clearDisplay();
        oled.setTextColor(SSD1306_WHITE);
        oled.setTextSize(1);
        oled.setCursor(0, 0);  oled.print(" WiFi Reset!");
        oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
        oled.setCursor(0, 20); oled.print(" Credentials erased");
        oled.setCursor(0, 32); oled.print(" Restarting...");
        oled.display();
        delay(2000);

        wifiManager.resetSettings();  // Erase saved WiFi
        ESP.restart();                // Restart device
      }
      delay(200);
    }

    // Button released before 5s -- cancel
    Serial.println(F("[RESET] Released early -- cancelled"));
  }
}

// ======================================================================
//  CALIBRATION
// ======================================================================
void runCalibration() {
  show(" CALIBRATION", " Remove all weight", " from sensor now!", " Waiting 5 sec...", " Do not touch");
  delay(5000);
  scale.tare();
  long zeroVal = scale.read_average(15);

  show(" CALIBRATION", " Zero done!", " Place known weight", " now...", " Waiting 5 sec...");
  Serial.println("[CALIB] Zero: " + String(zeroVal));
  delay(5000);

  long withVal = scale.read_average(15);
  show(" CALIB DONE!", " Zero:" + String(zeroVal), " With:" + String(withVal), " See Serial!", " Set false+upload");
  Serial.println(F(""));
  Serial.println(F("+==================================+"));
  Serial.println(F("|  CALIBRATION RESULT               |"));
  Serial.println("|  Zero   : " + String(zeroVal));
  Serial.println("|  With wt: " + String(withVal));
  Serial.println("|  Factor : " + String(withVal - zeroVal));
  Serial.println(F("|  Put factor in CALIBRATION_FACTOR |"));
  Serial.println(F("+==================================+"));
}

// ======================================================================
//  SETUP
// ======================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Reset button pin
  pinMode(PIN_RESET_BTN, INPUT_PULLUP);

  // OLED init
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[OLED] ERROR -- check wiring"));
  }
  oled.clearDisplay(); oled.display();

  // Boot screen
  show(" Biomedionics", " Isometri Muscle", " Meter  v4.0", "", " Starting up...");
  delay(1200);

  // Get MAC address -> Device ID
  WiFi.mode(WIFI_STA);
  delay(100);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char buf[13];
  snprintf(buf, 13, "%02X%02X%02X%02X%02X%02X",
           mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  deviceID = String(buf);
  fbPath   = "/dynometer/" + deviceID;

  Serial.println(F(""));
  Serial.println(F("+==================================+"));
  Serial.println(F("|  YOUR DEVICE ID                   |"));
  Serial.println("|  ID: " + deviceID);
  Serial.println("|  Path: " + fbPath);
  Serial.println(F("+==================================+"));

  // Show Device ID on OLED (blink 3 times)
  String p1 = deviceID.substring(0,6);
  String p2 = deviceID.substring(6,12);
  for (int i = 0; i < 3; i++) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0); oled.print(" Your Device ID:");
    oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(4, 14); oled.print(p1);
    oled.setCursor(4, 35); oled.print(p2);
    oled.fillRect(0, 53, 128, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    oled.setCursor(2, 54); oled.print(" Enter on website!");
    oled.display();
    delay(1600);
    oled.clearDisplay(); oled.display();
    delay(150);
  }

  // HX711 init
  show(" Load Cell", " HX711 Init...", "", "", " Checking sensor");
  scale.begin(PIN_HX711_DOUT, PIN_HX711_SCK);
  delay(800);

  if (CALIBRATION_MODE) {
    runCalibration();
    show(" Calib Done!", " Check Serial", " Set false+upload", "", " Halted");
    while (true);
  }

  if (!scale.is_ready()) {
    show(" HX711 Warning!", " Sensor not found", " Connect when ready", " Continuing...", " WiFi setup next");
    Serial.println(F("[HX711] Not ready -- continuing without sensor"));
    delay(2500);
  } else {
    scale.set_scale(CALIBRATION_FACTOR);
    scale.tare();
    show(" Load Cell OK!", " HX711 Ready", " Tared to zero", "", " Sensor active");
    Serial.println(F("[HX711] Ready -- tared to zero"));
    delay(1000);
  }

  // ======================================================
  //  WIFIMANAGER SETUP
  // ======================================================

  // Custom HTML for setup portal -- show Device ID
  String customHTML =
    "<br/><div style='background:#0a1628;color:white;padding:15px;"
    "border-radius:8px;font-family:sans-serif;text-align:center;'>"
    "<h2 style='color:#38bdf8;margin:0 0 8px'>Biomedionics</h2>"
    "<p style='margin:0 0 4px;font-size:13px;color:#94a3b8'>Isometri Muscle Meter</p>"
    "<hr style='border-color:#1e3a5f;margin:12px 0'/>"
    "<p style='font-size:12px;color:#94a3b8;margin:0 0 4px'>Your Device ID:</p>"
    "<p style='font-size:22px;font-weight:bold;letter-spacing:2px;color:#38bdf8;margin:0'>"
    + deviceID +
    "</p>"
    "<p style='font-size:11px;color:#64748b;margin:8px 0 0'>"
    "Enter this ID on biomedionics.vercel.app</p>"
    "</div><br/>";

  WiFiManagerParameter customParam("", "", "", 0, customHTML.c_str(),
                                   WFM_LABEL_BEFORE);
  wifiManager.addParameter(&customParam);

  // Configure WiFiManager
  wifiManager.setAPCallback(onAPStarted);
  wifiManager.setConfigPortalTimeout(180);  // 3 min portal timeout
  wifiManager.setConnectTimeout(20);        // 20s connect attempt
  wifiManager.setTitle("Biomedionics Setup");
  wifiManager.setClass("invert");           // Dark theme

  // Show connecting screen
  show(" WiFi Connecting", " Checking saved", " credentials...", "", " Please wait...");
  Serial.println(F("[WiFi] Starting WiFiManager..."));

  // autoConnect: if saved creds exist -> connect directly
  //              if not -> start AP "Biomedionics_Setup"
  bool wifiConnected = wifiManager.autoConnect("Biomedionics_Setup", "");
  // Empty password = open AP (anyone can connect for setup)

  if (!wifiConnected) {
    show(" WiFi FAILED!", " Portal timeout", " Restarting...", "", " Please wait");
    Serial.println(F("[WiFi] Connection failed -- restarting"));
    delay(3000);
    ESP.restart();
  }

  // WiFi connected!
  localIP = WiFi.localIP().toString();
  show(" WiFi Connected!", " SSID: " + WiFi.SSID(), " IP: " + localIP, "", " Firebase next...");
  Serial.println(F(""));
  Serial.println(F("+==================================+"));
  Serial.println(F("|  WiFi CONNECTED!                  |"));
  Serial.println("|  SSID: " + WiFi.SSID());
  Serial.println("|  IP  : " + localIP);
  Serial.println(F("+==================================+"));
  delay(1500);

  // ======================================================
  //  FIREBASE INIT
  // ======================================================
  show(" Firebase", " Connecting...", "", "", " Please wait...");
  Serial.println(F("[Firebase] Connecting..."));

  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  bool ok =
    Firebase.setString(fbdo, fbPath + "/status",   "waiting") &&
    Firebase.setString(fbdo, fbPath + "/deviceId", deviceID)  &&
    Firebase.setString(fbdo, fbPath + "/ip",       localIP)   &&
    Firebase.setString(fbdo, fbPath + "/ssid",     WiFi.SSID()) &&
    Firebase.setString(fbdo, fbPath + "/firmware", "v4.0");
  fbReady = ok;

  String p12 = deviceID.substring(0,6);
  String p22 = deviceID.substring(6,12);

  if (fbReady) {
    show(" Firebase OK!", " Status: waiting", " ID: "+p12+" "+p22, "", " Enter ID on web");
    Serial.println(F("[Firebase] Connected -- status: waiting"));
  } else {
    show(" Firebase ERR!", " Check Host/Auth", "", "", " Retrying...");
    Serial.println(F("[Firebase] ERROR!"));
    delay(3000);
  }
  delay(2000);
}

// ======================================================================
//  LOOP
// ======================================================================
bool g_loopSessionActive = false;

void loop() {
  // Check reset button every loop iteration
  checkResetButton();

  if (!fbReady) { delay(2000); fbReady = Firebase.ready(); return; }

  String p1 = deviceID.substring(0,6);
  String p2 = deviceID.substring(6,12);

  if (!Firebase.getString(fbdo, fbPath + "/status")) {
    Serial.println("[FB ERR] " + fbdo.errorReason());
    delay(2000); return;
  }
  String status = fbdo.stringData();

  // -- WAITING --------------------------------------------------------
  if (status == "waiting") {
    show(" Biomedionics", " ID: " + p1, "     " + p2,
         " WiFi: " + WiFi.SSID().substring(0,14),
         " Waiting for web...");

    static String lp = "";
    if (lp != "waiting") {
      lp = "waiting";
      Serial.println(F(""));
      Serial.println(F("[STATUS] waiting -- Open website and connect device"));
      Serial.println("[STATUS] Device ID: " + deviceID);
    }
    delay(1000);
  }

  // -- CONNECTING -----------------------------------------------------
  else if (status == "connecting") {
    static String lp = "";
    if (lp != "connecting") {
      lp = "connecting";
      Serial.println(F("[STATUS] connecting -- Website signal received!"));
    }
    // Blink animation
    show(" Biomedionics", " ID: " + p1, "     " + p2,
         " >> CONNECTING <<", " Website signal...");
    delay(400);
    oled.clearDisplay();
    oled.fillRect(0, 0, 128, 64, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    oled.setCursor(12, 20); oled.print(">> CONNECTING <<");
    oled.setCursor(22, 34); oled.print("Please wait...");
    oled.display();
    delay(300);
  }

  // -- CONNECTED -- live data streaming -------------------------------
  else if (status == "connected") {
    if (!g_loopSessionActive) {
      g_loopSessionActive = true;
      g_sessionActive     = true;
      mvc_N=0; mvc_Kg=0; peakN=0; timeToPeakS=0;
      rfd=0; rfdMax=0; prevForceN=0;
      initForcN=0; finalForceN=0; fatigueIdx=0;
      impulse=0; forceSumN=0; readingCnt=0;
      sessionStartMs=millis(); peakTimeMs=0;
      initCaptur=false;
      dt_s = READING_INTERVAL_MS / 1000.0f;

      Serial.println(F(""));
      Serial.println(F("[SESSION] Started -- 7 parameters active"));
      Serial.println(F("[SESSION] MVC | Peak | TTP | RFD | FI | Impulse | F-T Graph"));
    }

    unsigned long now = millis();
    if (now - lastReadTime < READING_INTERVAL_MS) { delay(30); return; }
    lastReadTime = now;

    float fKg = 0, fN = 0;
    if (scale.is_ready()) {
      fKg = scale.get_units(5);
      if (fKg < 0) fKg = 0;
      fN = fKg * 9.80665f;
    } else {
      Serial.println(F("[HX711] Not ready -- sending 0.0 N"));
    }

    unsigned long elMs = now - sessionStartMs;
    float         elS  = elMs / 1000.0f;
    readingCnt++; forceSumN += fN;

    // 1+2. MVC / Peak
    if (fN > mvc_N) {
      mvc_N=fN; mvc_Kg=fKg; peakN=fN;
      peakTimeMs=elMs; timeToPeakS=elS;
    }
    // 4. RFD
    if (readingCnt > 1) {
      rfd = (fN - prevForceN) / dt_s;
      if (rfd > rfdMax) rfdMax = rfd;
    }
    // 5. Fatigue Index
    if (!initCaptur && fN >= CONTRACTION_THR_N) {
      initForcN = fN; initCaptur = true;
    }
    if (initCaptur) {
      finalForceN = fN;
      fatigueIdx  = (initForcN > 0) ? ((initForcN-finalForceN)/initForcN*100.0f) : 0;
      if (fatigueIdx < 0) fatigueIdx = 0;
    }
    // 6. Impulse
    impulse += fN * dt_s;

    float avgF   = (readingCnt > 0) ? forceSumN/readingCnt : 0;
    float pctMVC = (mvc_N > 0) ? (fN/mvc_N*100.0f) : 0;
    bool  scrB   = ((readingCnt/3) % 2 == 1);

    showLive(fN, fKg, pctMVC, mvc_N, rfd, timeToPeakS,
             fatigueIdx, impulse, avgF, readingCnt, elS, scrB);

    // Firebase push
    FirebaseJson json;
    json.set("force",         fN);
    json.set("forceKg",       fKg);
    json.set("elapsedS",      elS);
    json.set("timestamp",     (long)millis());
    json.set("readingNum",    readingCnt);
    json.set("unit",          "Newton");
    json.set("mvc_N",         mvc_N);
    json.set("peakN",         peakN);
    json.set("pctMVC",        pctMVC);
    json.set("timeToPeakS",   timeToPeakS);
    json.set("timeToPeakMs",  (long)peakTimeMs);
    json.set("rfd",           rfd);
    json.set("rfdMax",        rfdMax);
    json.set("fatigueIndex",  fatigueIdx);
    json.set("forceImpulse",  impulse);
    json.set("avgForce",      avgF);
    bool sent = Firebase.pushJSON(fbdo, fbPath+"/readings", json);
    if (!sent) Serial.println("[FB] Push failed: " + fbdo.errorReason());

    prevForceN = fN;
  }

  // -- DISCONNECTED ---------------------------------------------------
  else if (status == "disconnected") {
    float avgFinal = (readingCnt > 0) ? forceSumN/readingCnt : 0;
    float dur      = (millis()-sessionStartMs)/1000.0f;
    if (initCaptur && initForcN > 0)
      fatigueIdx = ((initForcN-finalForceN)/initForcN*100.0f);
    if (fatigueIdx < 0) fatigueIdx = 0;

    // Push lastSession
    FirebaseJson summ;
    summ.set("mvc_N",           mvc_N);
    summ.set("peakN",           peakN);
    summ.set("timeToPeakS",     timeToPeakS);
    summ.set("rfdMax",          rfdMax);
    summ.set("fatigueIndex",    fatigueIdx);
    summ.set("forceImpulse",    impulse);
    summ.set("avgForce",        avgFinal);
    summ.set("totalReadings",   readingCnt);
    summ.set("sessionDuration", dur);
    summ.set("initialForce",    initForcN);
    summ.set("finalForce",      finalForceN);
    Firebase.setJSON(fbdo, fbPath+"/lastSession", summ);

    Serial.println(F(""));
    Serial.println(F("+================================================+"));
    Serial.println(F("|  SESSION COMPLETE -- FULL REPORT                |"));
    Serial.println("|  1. MVC          : " + String(mvc_N,2) + " N");
    Serial.println("|  2. Peak Force   : " + String(peakN,2) + " N");
    Serial.println("|  3. TTP          : " + String(timeToPeakS,3) + " s");
    Serial.println("|  4. RFD Max      : " + String(rfdMax,2) + " N/s");
    Serial.println("|  5. Fatigue Index: " + String(fatigueIdx,2) + " %");
    Serial.println("|  6. Impulse      : " + String(impulse,3) + " Ns");
    Serial.println("|  7. Avg Force    : " + String(avgFinal,2) + " N");
    Serial.println("|  Readings        : " + String(readingCnt));
    Serial.println("|  Duration        : " + String(dur,1) + " s");
    Serial.println(F("+================================================+"));

    // OLED summary 3 screens
    show(" Session Done! 1/3",
         " MVC : " + String(mvc_N,1) + " N",
         " Peak: " + String(peakN,1) + " N",
         " TTP : " + String(timeToPeakS,2) + " s",
         " Summary 1/3");
    delay(2500);
    show(" Session Done! 2/3",
         " RFD : " + String(rfdMax,1) + " N/s",
         " FI  : " + String(fatigueIdx,1) + " %",
         " Avg : " + String(avgFinal,1) + " N",
         " Summary 2/3");
    delay(2500);
    show(" Session Done! 3/3",
         " Imp : " + String(impulse,1) + " Ns",
         " Rdgs: " + String(readingCnt),
         " Dur : " + String(dur,1) + " s",
         " Reconnect on web");
    delay(2500);

    // Reset all
    g_loopSessionActive = false;
    g_sessionActive     = false;
    mvc_N=0; mvc_Kg=0; peakN=0; timeToPeakS=0;
    rfd=0; rfdMax=0; prevForceN=0;
    initForcN=0; finalForceN=0; fatigueIdx=0;
    impulse=0; forceSumN=0; readingCnt=0; peakTimeMs=0;
    initCaptur=false;

    Firebase.setString(fbdo, fbPath+"/status", "waiting");
    Serial.println(F("[STATUS] -> waiting -- Ready for next session"));
  }

  else {
    Serial.println("[WARNING] Unknown status: '" + status + "'");
    Firebase.setString(fbdo, fbPath+"/status", "waiting");
    delay(1000);
  }
}
