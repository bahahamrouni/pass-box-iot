/************************************************************
 *  PASS-BOX SUPERVISION SYSTEM – FULLY COMMENTED VERSION
 *  -------------------------------------------------------
 *  Features:
 *   - Dual-door control with interlocking
 *   - Decontamination cycle (8 steps)
 *   - LCD supervision
 *   - MQTT communication (HiveMQ)
 *   - Emergency stop system
 *   - Error handling with temporary LCD blocking
 *
 *  Hardware: ESP32 + LCD 16x2 I2C + Pushbuttons
 ************************************************************/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>

/*********************** LCD CONFIGURATION ********************/
#define LCD_ADDR 0x27          // I2C LCD address
#define LCD_COLS 16            // 16 columns
#define LCD_ROWS 2             // 2 rows
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

/*********************** WIFI & MQTT CONFIG *********************/
const char* ssid = "Wokwi-GUEST";       
const char* password = "";              
const char* mqtt_server = "broker.hivemq.com"; 

WiFiClient espClient;
PubSubClient client(espClient);

/*********************** BUTTON DEFINITIONS **********************/
#define BTN_DEPART 27               // Start cycle button
#define BTN_ARRET 14                // Emergency stop button
#define BTN_STERILE_OUVERT 26       // Sterile door open
#define BTN_STERILE_FERME 25        // Sterile door close
#define BTN_CONTAMINEE_OUVERT 13    // Contaminated door open
#define BTN_CONTAMINEE_FERME 12     // Contaminated door close

/*********************** STATE VARIABLES ************************/
// Door states
bool porteSterileOuverte = false;
bool porteContamineeOuverte = false;

// Cycle control
bool cycleDemarre = false;
bool cycleTermine = false;

// Cycle timing system
int etapeCycle = 0;
unsigned long chrono = 0;
unsigned long dureeEtape = 0;

// Used to avoid sending duplicate MQTT messages
String lastUrgenceMsg = "";
String lastContamineeMsg = "";
String lastSterileMsg = "";

// LCD error message lock
unsigned long tempsErreur = 0;
bool afficheMessageErreur = false;

/*********************** ERROR LCD MESSAGE FUNCTION *************/
void afficherErreur(const char* ligne1, const char* ligne2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(ligne1);
  lcd.setCursor(0, 1); lcd.print(ligne2);

  // Lock LCD for 1.5 seconds to prevent overwriting
  afficheMessageErreur = true;
  tempsErreur = millis();
}

/*********************** MQTT CALLBACK ***************************
 * This function is called every time the ESP32 receives
 * a message from any subscribed MQTT topic.
 ***************************************************************/
void callback(char* topic, byte* payload, unsigned int length) {

  // If an error is being displayed, ignore MQTT updates temporarily
  if (afficheMessageErreur && millis() - tempsErreur < 1500) {
    return;
  }

  // Convert raw MQTT payload into a readable string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String topicStr = String(topic);

  /************** DOOR: CONTAMINATED SIDE *****************/
  if (topicStr == "porte/contaminee") {
    if (message != lastContamineeMsg) {

      lastContamineeMsg = message;
      Serial.print("MQTT ["); Serial.print(topic); Serial.print("]: "); Serial.println(message);

      lcd.clear();
      if (message == "ON") {
        porteContamineeOuverte = true;
        lcd.setCursor(0, 0); lcd.print("Cote Contamine");
        lcd.setCursor(0, 1); lcd.print("ouverte");
        client.publish("porte/contaminee", "ON");
      } else {
        porteContamineeOuverte = false;
        lcd.setCursor(0, 0); lcd.print("Cote Contamine");
        lcd.setCursor(0, 1); lcd.print("fermee");
        client.publish("porte/contaminee", "OFF");
      }
    }
  }

  /************** DOOR: STERILE SIDE *****************/
  else if (topicStr == "porte/sterile") {
    if (message != lastSterileMsg) {

      lastSterileMsg = message;
      Serial.print("MQTT ["); Serial.print(topic); Serial.print("]: "); Serial.println(message);

      lcd.clear();
      if (message == "ON") {
        porteSterileOuverte = true;
        lcd.setCursor(0, 0); lcd.print("Cote Sterile");
        lcd.setCursor(0, 1); lcd.print("ouverte");
        
      } else {
        porteSterileOuverte = false;
        lcd.setCursor(0, 0); lcd.print("Cote Sterile");
        lcd.setCursor(0, 1); lcd.print("fermee");

        cycleTermine = false; 
      }
    }
  }

  /************** URGENCY / EMERGENCY *****************/
  else if (topicStr == "urgence") {
    if (message != lastUrgenceMsg) {

      lastUrgenceMsg = message;
      Serial.print("MQTT ["); Serial.print(topic); Serial.print("]: "); Serial.println(message);

      lcd.clear();
      if (message == "ON") {
        lcd.setCursor(0, 0); lcd.print("ALERTE URGENCE!");
        lcd.setCursor(0, 1); lcd.print("LED ALLUMEE");
      } else {
        lcd.setCursor(0, 0); lcd.print("Urgence OFF");
        lcd.setCursor(0, 1); lcd.print("LED ETEINTE");
      }
    }
  }

  /************** CYCLE START/STOP *****************/
  else if (topicStr == "cycle/depart") {

    Serial.print("MQTT ["); Serial.print(topic); Serial.print("]: "); Serial.println(message);
    lcd.clear();

    if (message == "ON") {
      lcd.setCursor(0, 0); lcd.print("Depart cycle");
      lcd.setCursor(0, 1); lcd.print("Verification OK");
    } else {
      lcd.setCursor(0, 0); lcd.print("Cycle termine !");
    }
  }
}

/*********************** WIFI SETUP *****************************/
void setup_wifi() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("\nConnexion WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connecté");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

/*********************** MQTT RECONNECT *************************/
void reconnect() {

  while (!client.connected()) {
    Serial.print("Connexion MQTT... ");

    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    // Attempt connection
    if (client.connect(clientId.c_str())) {
      Serial.println("OK");

      // Subscribe to all topics needed
      client.subscribe("porte/contaminee");
      client.subscribe("porte/sterile");
      client.subscribe("urgence");
      client.subscribe("cycle/depart");

    } else {
      Serial.print("Échec, code=");
      Serial.print(client.state());
      Serial.println(" -> retry in 5s");
      delay(5000);
    }
  }
}

/*********************** SETUP **********************************/
void setup() {
  Wire.begin(21, 22);   // I2C pins for ESP32
  lcd.init();
  lcd.backlight();
  Serial.begin(115200);

  // Configure all buttons as digital inputs
  pinMode(BTN_DEPART, INPUT);
  pinMode(BTN_ARRET, INPUT);
  pinMode(BTN_STERILE_OUVERT, INPUT);
  pinMode(BTN_STERILE_FERME, INPUT);
  pinMode(BTN_CONTAMINEE_OUVERT, INPUT);
  pinMode(BTN_CONTAMINEE_FERME, INPUT);

  lcd.setCursor(0, 0);
  lcd.print("Systeme Pret");

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

/*********************** MAIN LOOP ******************************/
void loop() {

  /***** Keep MQTT connection alive *****/
  if (!client.connected()) reconnect();
  client.loop();

  /***** Protect LCD when displaying error messages *****/
  if (afficheMessageErreur && millis() - tempsErreur < 1500) {
    return;
  } else {
    afficheMessageErreur = false;
  }

  /*************************************************************
   * EMERGENCY STOP HANDLING
   *************************************************************/
  bool arret = digitalRead(BTN_ARRET);

  if (arret == HIGH && cycleDemarre) {
    lcd.clear();
    lcd.print("ARRET URGENCE !");
    cycleDemarre = false;
    etapeCycle = 0;

    client.publish("cycle/depart", "OFF");
    client.publish("urgence", "ON");
    return; 
  }

  /*************************************************************
   * DOOR CONTROL LOGIC (Only allowed when cycle is NOT running)
   *************************************************************/
  if (!cycleDemarre) {

    /********** CONTAMINATED DOOR – OPEN **********/
    if (digitalRead(BTN_CONTAMINEE_OUVERT) == HIGH && !porteContamineeOuverte) {

      if (cycleTermine) {  
        afficherErreur("Acces interdit!", "Cycle termine");
      }
      else if (porteSterileOuverte) {  
        afficherErreur("Fermer cote", "Sterile!");
      }
      else {
        // Valid open
        porteContamineeOuverte = true;
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Cote Contamine");
        lcd.setCursor(0, 1); lcd.print("ouverte");

        if (lastContamineeMsg != "ON") {
          client.publish("porte/contaminee", "ON");
          lastContamineeMsg = "ON";
        }
      }
    }

    /********** CONTAMINATED DOOR – CLOSE **********/
    if (digitalRead(BTN_CONTAMINEE_FERME) == HIGH && porteContamineeOuverte) {

      porteContamineeOuverte = false;
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Cote Contamine");
      lcd.setCursor(0, 1); lcd.print("fermee");

      if (lastContamineeMsg != "OFF") {
        client.publish("porte/contaminee", "OFF");
        lastContamineeMsg = "OFF";
      }
    }

    /********** STERILE DOOR – OPEN **********/
    if (digitalRead(BTN_STERILE_OUVERT) == HIGH && !porteSterileOuverte) {

      if (porteContamineeOuverte) {
        afficherErreur("Fermer cote", "Contamine!");
      }
      else {
        porteSterileOuverte = true;
        lcd.clear();
        lcd.setCursor(0, 0); lcd.print("Cote Sterile");
        lcd.setCursor(0, 1); lcd.print("ouverte");

        if (lastSterileMsg != "ON") {
          client.publish("porte/sterile", "ON");
          lastSterileMsg = "ON";
        }
      }
    }

    /********** STERILE DOOR – CLOSE **********/
    if (digitalRead(BTN_STERILE_FERME) == HIGH && porteSterileOuverte) {

      porteSterileOuverte = false;
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Cote Sterile");
      lcd.setCursor(0, 1); lcd.print("fermee");

      if (lastSterileMsg != "OFF") {
        client.publish("porte/sterile", "OFF");
        lastSterileMsg = "OFF";
      }

      cycleTermine = false; // Reset after retrieval
    }

    /********** START DECONTAMINATION CYCLE **********/
    if (digitalRead(BTN_DEPART) == HIGH) {

      if (porteContamineeOuverte || porteSterileOuverte) {
        afficherErreur("Verifier la", "fermeture portes");
      }
      else {
        cycleDemarre = true;
        etapeCycle = 1;

        lcd.clear();
        lcd.print("Depart Cycle");

        chrono = millis();  
        dureeEtape = 3000;

        client.publish("cycle/depart", "ON");
        client.publish("urgence", "OFF");
      }
    }
  }

  /*************************************************************
   * AUTOMATED DECONTAMINATION CYCLE STATE MACHINE
   *************************************************************/
  if (cycleDemarre) {

    unsigned long maintenant = millis();

    if (maintenant - chrono >= dureeEtape) {
      chrono = maintenant;

      switch (etapeCycle) {

        case 1:
          lcd.clear(); lcd.print("1: Extraction air");
          client.publish("cycle/etape", "1: Extraction air");
          dureeEtape = 1000;
          etapeCycle++;
          break;

        case 2:
          lcd.clear(); lcd.print("2: Arret air");
          client.publish("cycle/etape", "2: Arret air");
          dureeEtape = 1000;
          etapeCycle++;
          break;

        case 3:
          lcd.clear();
          lcd.setCursor(0,0); lcd.print("3: Injection");
          lcd.setCursor(0,1); lcd.print("produit");
          client.publish("cycle/etape", "3: Injection produit");
          dureeEtape = 1000;
          etapeCycle++;
          break;

        case 4:
          lcd.clear();
          lcd.setCursor(0,0); lcd.print("4: Pause");
          lcd.setCursor(0,1); lcd.print("20 min");
          client.publish("cycle/etape", "4: Pause 20 min");
          dureeEtape = 1000;
          etapeCycle++;
          break;

        case 5:
          lcd.clear();
          lcd.setCursor(0,0); lcd.print("5: Extrait");
          lcd.setCursor(0,1); lcd.print("produit");
          client.publish("cycle/etape", "5: Extrait produit");
          dureeEtape = 1000;
          etapeCycle++;
          break;

        case 6:
          lcd.clear(); lcd.print("6: Air propre");
          client.publish("cycle/etape", "6: Air propre");
          dureeEtape = 1000;
          etapeCycle++;
          break;

        case 7:
          lcd.clear();
          lcd.setCursor(0,0); lcd.print("7: Ouvrir");
          lcd.setCursor(0,1); lcd.print("cote sterile");
          client.publish("cycle/etape", "7: Ouvrir cote sterile");
          dureeEtape = 0;
          etapeCycle++;
          break;

        case 8:
          lcd.clear(); lcd.print("Cycle termine");
          client.publish("cycle/etape", "Termine");

          client.publish("cycle/depart", "OFF");

          cycleDemarre = false;
          cycleTermine = true;
          etapeCycle = 0;
          break;
      }
    }
  }
}
