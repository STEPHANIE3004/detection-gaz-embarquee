/*
 * ============================================================
 *  DETECTION DE GAZ EMBARQUEE — Arduino Uno
 *  Systeme d'alarme multi-capteurs MQ
 * ============================================================
 *  Auteure  : Vanelle Stephanie MANGOUA DJOUSSEU
 *  Materiel : Arduino Uno + MQ-2 + MQ-7 + MQ-135
 *             + LCD 16x2 I2C + Buzzer + LEDs
 *
 *  Librairies requises :
 *    - LiquidCrystal_I2C (Frank de Brabander)
 *
 *  SCHEMA DE CABLAGE :
 *  -------------------
 *  MQ-2   (GPL/Fumee) : AOUT -> A0  | VCC -> 5V | GND -> GND
 *  MQ-7   (CO)        : AOUT -> A1  | VCC -> 5V | GND -> GND
 *  MQ-135 (CO2/Air)   : AOUT -> A2  | VCC -> 5V | GND -> GND
 *  LCD I2C  : SDA -> A4 | SCL -> A5 | VCC -> 5V
 *  Buzzer   : + -> D8  | - -> GND
 *  LED Rouge: + -> D9  (via 220 ohm) | - -> GND
 *  LED Jaune: + -> D10 (via 220 ohm) | - -> GND
 *  LED Verte: + -> D11 (via 220 ohm) | - -> GND
 *
 *  NOTE : Les capteurs MQ necessitent 24h de chauffe initiale
 *         pour une calibration optimale.
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ─── Broches capteurs ─────────────────────────────────────
#define PIN_MQ2    A0    // GPL, fumee, propane, butan
#define PIN_MQ7    A1    // Monoxyde de carbone (CO)
#define PIN_MQ135  A2    // CO2, NH3, alcool, benzene

// ─── Broches sorties ──────────────────────────────────────
#define PIN_BUZZER    8
#define PIN_LED_ROUGE 9
#define PIN_LED_JAUNE 10
#define PIN_LED_VERTE 11

// ─── Configuration LCD I2C ────────────────────────────────
// Adresse I2C du module LCD : 0x27 ou 0x3F selon le modele
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─── Seuils ADC (0-1023) ──────────────────────────────────
// Calibrer selon votre environnement apres 24h de chauffe
#define MQ2_SEUIL_WARNING    300
#define MQ2_SEUIL_DANGER     600
#define MQ7_SEUIL_WARNING    200
#define MQ7_SEUIL_DANGER     500
#define MQ135_SEUIL_WARNING  300
#define MQ135_SEUIL_DANGER   650

// ─── Niveaux d'alerte ─────────────────────────────────────
#define NIVEAU_OK       0
#define NIVEAU_WARNING  1
#define NIVEAU_DANGER   2

// ─── Timing ───────────────────────────────────────────────
#define INTERVALLE_MESURE   2000   // ms
#define DUREE_BUZZER_COURT   100   // ms (bip warning)
#define DUREE_BUZZER_LONG    500   // ms (bip danger)
#define FREQ_BUZZER_WARNING  1000  // Hz
#define FREQ_BUZZER_DANGER   2500  // Hz

// ─── Variables globales ───────────────────────────────────
int      valMQ2   = 0;
int      valMQ7   = 0;
int      valMQ135 = 0;
int      niveauAlerte = NIVEAU_OK;
unsigned long derniereMesure  = 0;
unsigned long nbAlertes       = 0;

// ════════════════════════════════════════════════════════════
//  FONCTIONS UTILITAIRES
// ════════════════════════════════════════════════════════════

// Lecture avec moyenne sur 5 echantillons (anti-bruit)
int lireCapteurMoyen(uint8_t pin) {
  long somme = 0;
  for (int i = 0; i < 5; i++) {
    somme += analogRead(pin);
    delay(10);
  }
  return (int)(somme / 5);
}

// Conversion ADC brut -> ppm (approximation lineaire simplifiee)
// Pour un calcul precis, utiliser la courbe log-log de la datasheet
float adcVersPPM_MQ2(int adc) {
  // MQ-2 : 200-10000 ppm GPL
  return map(adc, 0, 1023, 0, 10000);
}

float adcVersPPM_MQ7(int adc) {
  // MQ-7 : 20-2000 ppm CO
  return map(adc, 0, 1023, 0, 2000);
}

float adcVersPPM_MQ135(int adc) {
  // MQ-135 : 10-1000 ppm CO2/air
  return map(adc, 0, 1023, 0, 1000);
}

// ════════════════════════════════════════════════════════════
//  LECTURE CAPTEURS
// ════════════════════════════════════════════════════════════
void lireTousCapteurs() {
  valMQ2   = lireCapteurMoyen(PIN_MQ2);
  valMQ7   = lireCapteurMoyen(PIN_MQ7);
  valMQ135 = lireCapteurMoyen(PIN_MQ135);
}

// ════════════════════════════════════════════════════════════
//  CLASSIFICATION NIVEAU D'ALERTE
// ════════════════════════════════════════════════════════════
int calculerNiveauAlerte() {
  // Le niveau le plus eleve parmi les 3 capteurs
  int niveau = NIVEAU_OK;

  if (valMQ2 >= MQ2_SEUIL_DANGER ||
      valMQ7 >= MQ7_SEUIL_DANGER ||
      valMQ135 >= MQ135_SEUIL_DANGER) {
    niveau = NIVEAU_DANGER;
  }
  else if (valMQ2 >= MQ2_SEUIL_WARNING ||
           valMQ7 >= MQ7_SEUIL_WARNING ||
           valMQ135 >= MQ135_SEUIL_WARNING) {
    niveau = NIVEAU_WARNING;
  }

  return niveau;
}

// ════════════════════════════════════════════════════════════
//  COMMANDE SORTIES (LEDs + Buzzer)
// ════════════════════════════════════════════════════════════
void commanderSorties(int niveau) {
  // Eteindre tout d'abord
  digitalWrite(PIN_LED_ROUGE, LOW);
  digitalWrite(PIN_LED_JAUNE, LOW);
  digitalWrite(PIN_LED_VERTE, LOW);
  noTone(PIN_BUZZER);

  switch (niveau) {
    case NIVEAU_OK:
      digitalWrite(PIN_LED_VERTE, HIGH);
      break;

    case NIVEAU_WARNING:
      digitalWrite(PIN_LED_JAUNE, HIGH);
      tone(PIN_BUZZER, FREQ_BUZZER_WARNING, DUREE_BUZZER_COURT);
      nbAlertes++;
      break;

    case NIVEAU_DANGER:
      digitalWrite(PIN_LED_ROUGE, HIGH);
      // Buzzer intermittent urgent
      for (int i = 0; i < 3; i++) {
        tone(PIN_BUZZER, FREQ_BUZZER_DANGER, DUREE_BUZZER_LONG);
        delay(DUREE_BUZZER_LONG + 100);
        noTone(PIN_BUZZER);
        delay(100);
      }
      nbAlertes++;
      break;
  }
}

// ════════════════════════════════════════════════════════════
//  AFFICHAGE LCD
// ════════════════════════════════════════════════════════════
void afficherLCD(int niveau) {
  lcd.clear();

  // Ligne 1 : valeurs capteurs
  lcd.setCursor(0, 0);
  lcd.print(F("G:"));  lcd.print(valMQ2);
  lcd.print(F(" C:"));  lcd.print(valMQ7);
  lcd.print(F(" A:"));  lcd.print(valMQ135);

  // Ligne 2 : etat
  lcd.setCursor(0, 1);
  switch (niveau) {
    case NIVEAU_OK:
      lcd.print(F("Etat : OK       "));
      break;
    case NIVEAU_WARNING:
      lcd.print(F("!! ATTENTION !! "));
      break;
    case NIVEAU_DANGER:
      lcd.print(F("!!! DANGER !!!  "));
      break;
  }
}

// ════════════════════════════════════════════════════════════
//  AFFICHAGE SERIAL MONITOR (debug)
// ════════════════════════════════════════════════════════════
void afficherSerial(int niveau) {
  Serial.print(F("[MQ2="));   Serial.print(valMQ2);
  Serial.print(F(" | MQ7=")); Serial.print(valMQ7);
  Serial.print(F(" | MQ135="));Serial.print(valMQ135);
  Serial.print(F("] Etat: "));

  switch (niveau) {
    case NIVEAU_OK:      Serial.print(F("OK"));       break;
    case NIVEAU_WARNING: Serial.print(F("WARNING"));  break;
    case NIVEAU_DANGER:  Serial.print(F("DANGER!!")); break;
  }

  Serial.print(F(" | Alertes: "));
  Serial.println(nbAlertes);

  // Valeurs en ppm approximatives
  Serial.print(F("  GPL~"));   Serial.print(adcVersPPM_MQ2(valMQ2));   Serial.print(F("ppm"));
  Serial.print(F("  CO~"));    Serial.print(adcVersPPM_MQ7(valMQ7));   Serial.print(F("ppm"));
  Serial.print(F("  Air~"));   Serial.print(adcVersPPM_MQ135(valMQ135)); Serial.println(F("ppm"));
}

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);

  // Sorties
  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_LED_ROUGE, OUTPUT);
  pinMode(PIN_LED_JAUNE, OUTPUT);
  pinMode(PIN_LED_VERTE, OUTPUT);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(2, 0);
  lcd.print(F("DETECTION GAZ"));
  lcd.setCursor(1, 1);
  lcd.print(F("Initialisation.."));

  Serial.println(F("=== SYSTEME DETECTION GAZ ==="));
  Serial.println(F("Auteure : Vanelle Stephanie MANGOUA DJOUSSEU"));
  Serial.println(F("Capteurs : MQ-2 (GPL) | MQ-7 (CO) | MQ-135 (Air)"));
  Serial.println(F("Prechauffage en cours..."));

  // Test sorties au demarrage
  digitalWrite(PIN_LED_ROUGE, HIGH);
  tone(PIN_BUZZER, 1000, 200);
  delay(300);
  digitalWrite(PIN_LED_JAUNE, HIGH);
  delay(300);
  digitalWrite(PIN_LED_VERTE, HIGH);
  delay(300);
  digitalWrite(PIN_LED_ROUGE, LOW);
  digitalWrite(PIN_LED_JAUNE, LOW);
  digitalWrite(PIN_LED_VERTE, LOW);
  noTone(PIN_BUZZER);

  // Prechauffage minimal (en production : attendre 24h)
  for (int i = 10; i > 0; i--) {
    lcd.setCursor(0, 1);
    lcd.print(F("Attente ")); lcd.print(i); lcd.print(F("s...    "));
    delay(1000);
  }

  lcd.clear();
  Serial.println(F("Systeme pret - surveillance active\n"));
}

// ════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  unsigned long maintenant = millis();

  if (maintenant - derniereMesure >= INTERVALLE_MESURE) {
    derniereMesure = maintenant;

    lireTousCapteurs();
    niveauAlerte = calculerNiveauAlerte();
    commanderSorties(niveauAlerte);
    afficherLCD(niveauAlerte);
    afficherSerial(niveauAlerte);
  }
}
