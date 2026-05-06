# 🚨 Détection de Gaz Embarquée — Arduino Uno + PIC16F877A/GSM

Système d'alarme embarqué en deux variantes complémentaires :  
une version **Arduino** (alerte locale, LCD) et une version **PIC16F877A + SIM800L** (alerte SMS autonome, sans WiFi ni infrastructure réseau).

![Arduino](https://img.shields.io/badge/Arduino-Uno%20ATmega328P-teal?logo=arduino)
![PIC](https://img.shields.io/badge/PIC-16F877A%20%40%204MHz-red)
![GSM](https://img.shields.io/badge/GSM-SIM800L%20%2F%20AT%20commands-orange)
![C](https://img.shields.io/badge/C-XC8%20%7C%20gcc-blue)

---

## 🎯 Deux variantes, un seul domaine

| | Version Arduino | Version PIC + GSM |
|--|-----------------|-------------------|
| **Microcontrôleur** | ATmega328P (Arduino Uno) | PIC16F877A @ 4 MHz |
| **Alerte** | LCD 16×2 + buzzer + LEDs | SMS via SIM800L (commandes AT) |
| **Infrastructure** | Locale (câblé) | Autonome — aucun WiFi requis |
| **Compilateur** | Arduino IDE / avr-gcc | MPLAB X / XC8 |
| **Fichier** | `detection_gaz/detection_gaz.ino` | `pic_gsm/detection_gaz_pic.c` |

La version PIC+GSM répond à un cas réel : déployer une alarme dans un site industriel ou rural **sans réseau WiFi**, en envoyant un SMS dès que le gaz dépasse le seuil.

---

## 🏗️ Structure du projet

```
detection-gaz-embarquee/
├── detection_gaz/
│   └── detection_gaz.ino           ← Arduino Uno : MQ-2/7/135 + LCD + buzzer
│
├── pic_gsm/
│   ├── detection_gaz_pic.c         ← PIC16F877A : ADC + UART → SIM800L → SMS
│   └── gsm_at.h                    ← Référence commandes AT (SIM800L/SIM900)
│
├── src/
│   └── detection_gaz.c             ← Simulation C portable (calibration ppm)
│
├── include/
└── docs/
    └── screenshot_gaz.png
```

---

## 🔧 Module 1 — Arduino Uno (alerte locale)

**Fichier :** `detection_gaz/detection_gaz.ino`  
**Cible :** Arduino Uno (ATmega328P)

Trois capteurs MQ lus en continu avec moyenne sur 5 échantillons (anti-bruit). Déclenche un affichage LCD, une LED et un buzzer selon le niveau de danger.

### Capteurs et câblage

| Capteur | Gaz détecté | Broche |
|---------|------------|--------|
| MQ-2 | GPL, propane, fumée | A0 |
| MQ-7 | Monoxyde de carbone (CO) | A1 |
| MQ-135 | CO2, NH3, alcool, benzène | A2 |
| LCD 16×2 I2C | Affichage — SDA/SCL | A4, A5 |
| Buzzer 5V | Alarme sonore | D8 |
| LED Rouge | Niveau DANGER | D9 |
| LED Jaune | Niveau WARNING | D10 |
| LED Verte | Niveau OK | D11 |

### Seuils d'alerte (ADC 0–1023)

| Capteur | WARNING | DANGER |
|---------|---------|--------|
| MQ-2 (GPL) | 300 | 600 |
| MQ-7 (CO) | 200 | 500 |
| MQ-135 (Air) | 300 | 650 |

> ⚠️ Les capteurs MQ nécessitent **24h de chauffe** avant calibration précise.

### 3 niveaux d'alerte

```
Niveau OK      → LED verte allumée, LCD "Système OK"
Niveau WARNING → LED jaune + bip court + LCD "ATTENTION: [gaz]"
Niveau DANGER  → LED rouge + buzzer urgent + LCD "DANGER: EVACUEZ"
```

---

## 📱 Module 2 — PIC16F877A + SIM800L (alerte SMS autonome)

**Fichiers :** `pic_gsm/detection_gaz_pic.c` + `pic_gsm/gsm_at.h`  
**Compilateur :** MPLAB X + XC8

Système **100% autonome** : lit le capteur via ADC, envoie un SMS via commandes AT dès dépassement de seuil — **sans WiFi, sans Ethernet, sans infrastructure**.

### Brochage PIC16F877A

```
PIC16F877A @ 4 MHz
├── RA0/AN0 → Capteur gaz (MQ-2 ou MQ-7) — ADC 10 bits
├── RC6/TX  → SIM800L RX  (UART @ 9600 bauds)
├── RC7/RX  → SIM800L TX
├── RB0     → LED verte (état normal)
├── RB1     → LED rouge (alerte)
└── RB2     → Buzzer
```

### UART implémenté from scratch (XC8)

```c
// PIC16F877A — USART @ 9600 bauds, 4 MHz
// BRGH=1 : SPBRG = (4MHz / (16 × 9600)) - 1 = 25
void uart_init(void) {
    SPBRG  = 25;
    TXSTAbits.BRGH = 1;
    TXSTAbits.TXEN = 1;    // Activer émetteur
    RCSTAbits.SPEN = 1;    // Activer port série
    RCSTAbits.CREN = 1;    // Récepteur continu
    TRISCbits.TRISC6 = 0;  // TX en sortie
}
```

### Envoi SMS par commandes AT

Le fichier `gsm_at.h` documente toutes les commandes AT utilisées pour piloter le SIM800L :

```
Séquence d'initialisation :
  AT           → vérifier "OK"
  ATE0         → désactiver l'écho
  AT+CPIN?     → vérifier SIM déverrouillée
  AT+CREG?     → attendre enregistrement réseau (+CREG: 0,1)
  AT+CMGF=1    → mode texte SMS
  AT+CSCS="GSM"→ encodage GSM 7-bit

Envoi SMS :
  AT+CMGS="+33600000000"
  > Message d'alerte : GAZ DETECTE - Niveau DANGER  [Ctrl+Z]
```

**Ce que ça prouve :** maîtrise d'une famille de microcontrôleurs PIC (différente d'Arduino), implémentation UART from scratch sous XC8, et intégration d'un module cellulaire par protocole AT — compétences directement applicables en industrie (IoT distant, M2M).

---

## 📐 Module 3 — Calibration ADC → ppm (C portable)

**Fichier :** `src/detection_gaz.c`

Implémente la conversion des valeurs ADC brutes en concentration ppm selon les courbes log-log des datasheets MQ, utilisable en simulation PC ou comme base pour les deux variantes embarquées.

```c
// Conversion ADC → Rs (résistance capteur)
static float adc_vers_rs(uint16_t adc_val) {
    float v_out = adc_val * (VCC / 1023.0f);
    return RL * (VCC - v_out) / v_out;   // RL = résistance charge = 10kΩ
}

// Rs → ppm GPL (MQ-2) via courbe puissance de la datasheet
static float rs_vers_ppm_mq2(float rs) {
    return 613.9f * powf(rs / MQ2_R0, -2.074f);  // MQ2_R0 = 9.8 (air pur)
}
```

Seuils exprimés en ppm (pas en ADC brut) : GPL > 1000 ppm = DANGER, CO > 200 ppm = DANGER, CO2 > 2000 ppm = DANGER.

---

## ⚠️ Limites connues

**R0 non calibrée sur site.** Les constantes `MQ2_R0`, `MQ7_R0`, `MQ135_R0` sont des valeurs typiques de datasheet. Une calibration réelle (mesure en air pur après 24h de chauffe) peut faire varier ces valeurs de ±30 %.

**PIC non testé sur silicium.** Le code PIC16F877A a été écrit et compilé sous XC8 mais non testé sur carte réelle faute de matériel disponible. La logique UART et la séquence AT sont validées par simulation MPLAB.

**SIM800L en 2G.** Le module SIM800L est 2G uniquement. Dans les zones où le réseau 2G est décommissionné (certaines régions), un module 4G LTE-M (SIM7070G) serait nécessaire.

**Pas de confirmation de livraison SMS.** Le code actuel envoie le SMS et passe à la suite sans vérifier la réponse `+CMGS:` du module — en production, une confirmation et un mécanisme de retry seraient nécessaires.

---

## ⚙️ Installation & Lancement

### Version Arduino
```
Arduino IDE → Arduino Uno
Bibliothèque : LiquidCrystal_I2C (Frank de Brabander)
Téléverser → Serial Monitor 9600 bauds
```

### Version PIC + GSM
```
MPLAB X IDE + compilateur XC8
Cible : PIC16F877A
Modifier NUM_TEL dans detection_gaz_pic.c
Programmer via PICkit 3/4
```

### Simulation C (PC)
```bash
gcc -Wall -lm -o gaz_sim src/detection_gaz.c
./gaz_sim
```

---

## 🛠️ Technologies

**ATmega328P (Arduino C++)** · **PIC16F877A (XC8 / MPLAB X)** · **SIM800L (commandes AT)** · **ADC 10 bits** · **UART from scratch** · **I2C (LCD)** · **C11 / gcc**

---

## 👩‍💻 Auteure

**Vanelle Stéphanie MANGOUA** — Recherche d'alternance en IA & Systèmes Embarqués
