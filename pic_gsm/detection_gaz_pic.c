/*
 * Détection de Gaz + Alerte SMS — PIC16F877A + Module GSM SIM800L
 * ================================================================
 * Échantillonne le capteur de gaz à intervalle régulier via ADC,
 * et envoie automatiquement un SMS d'alerte via commandes AT (GSM)
 * lorsque le seuil est dépassé.
 *
 * Système 100% autonome : fonctionne sans WiFi ni infrastructure réseau.
 *
 * Auteur   : Vanelle Stéphanie MANGOUA
 * Cible    : PIC16F877A @ 4 MHz (oscillateur interne)
 * Compil.  : XC8 (MPLAB X)
 *
 * Brochage :
 *   RA0/AN0 → capteur gaz (MQ-2 ou MQ-7)
 *   RC6/TX  → SIM800L RX  (UART @ 9600)
 *   RC7/RX  → SIM800L TX
 *   RB0     → LED verte (état normal)
 *   RB1     → LED rouge (alerte)
 *   RB2     → Buzzer
 */

#include <xc.h>
#include <stdint.h>
#include <string.h>

// ── Configuration bits ───────────────────────────────────────────────────────
#pragma config FOSC  = XT      // Oscillateur quartz 4 MHz
#pragma config WDTE  = OFF     // Watchdog désactivé
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP   = OFF
#pragma config CPD   = OFF
#pragma config CP    = OFF

#define _XTAL_FREQ  4000000UL

// ── Paramètres ────────────────────────────────────────────────────────────────
#define SEUIL_ALERTE   400     // Valeur ADC 10 bits (0–1023)
#define SEUIL_DANGER   700
#define INTERVALLE_MS  1000    // Lecture toutes les secondes
#define NUM_TEL        "+33600000000"   // Numéro à alerter (à personnaliser)

// ── Macros GPIO ───────────────────────────────────────────────────────────────
#define LED_VERTE   PORTBbits.RB0
#define LED_ROUGE   PORTBbits.RB1
#define BUZZER      PORTBbits.RB2

// ════════════════════════════════════════════════════════════════════════════
//  UART — PIC USART @ 9600 bauds
// ════════════════════════════════════════════════════════════════════════════

void uart_init(void) {
    // BRGH=1 (haute vitesse) : SPBRG = (4MHz / (16 × 9600)) - 1 = 25
    SPBRG  = 25;
    TXSTAbits.BRGH = 1;
    TXSTAbits.TXEN = 1;    // Activer émetteur
    RCSTAbits.SPEN = 1;    // Activer port série
    RCSTAbits.CREN = 1;    // Activer récepteur continu
    TRISCbits.TRISC6 = 0;  // RC6 = TX (sortie)
    TRISCbits.TRISC7 = 1;  // RC7 = RX (entrée)
}

void uart_send_byte(uint8_t c) {
    while (!TXSTAbits.TRMT);   // Attendre buffer vide
    TXREG = c;
}

void uart_send_str(const char* s) {
    while (*s) uart_send_byte((uint8_t)(*s++));
}

void uart_send_cmd(const char* cmd) {
    uart_send_str(cmd);
    uart_send_str("\r\n");
    __delay_ms(500);           // Délai réponse module GSM
}

// ════════════════════════════════════════════════════════════════════════════
//  ADC — lecture capteur gaz (AN0 / RA0)
// ════════════════════════════════════════════════════════════════════════════

void adc_init(void) {
    TRISAbits.TRISA0 = 1;       // RA0 en entrée analogique
    ADCON1 = 0x8E;              // AN0 analogique, ref = VDD, résultat justifié à droite
    ADCON0 = 0x41;              // Fosc/8, canal AN0, ADC activé
}

uint16_t adc_read(void) {
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);      // Attendre fin de conversion
    return ((uint16_t)ADRESH << 8) | ADRESL;
}

// ════════════════════════════════════════════════════════════════════════════
//  GSM SIM800L — Envoi SMS via commandes AT
// ════════════════════════════════════════════════════════════════════════════

void gsm_init(void) {
    __delay_ms(3000);                    // Attendre démarrage SIM800L
    uart_send_cmd("AT");                 // Test communication
    uart_send_cmd("AT+CMGF=1");          // Mode texte SMS
    uart_send_cmd("AT+CSCS=\"GSM\"");    // Encodage GSM
}

void gsm_send_sms(const char* message) {
    // 1. Commande d'envoi avec numéro destinataire
    uart_send_str("AT+CMGS=\"");
    uart_send_str(NUM_TEL);
    uart_send_str("\"\r");
    __delay_ms(500);

    // 2. Corps du message
    uart_send_str(message);

    // 3. Ctrl+Z pour valider l'envoi
    uart_send_byte(0x1A);
    __delay_ms(3000);    // Attendre confirmation envoi
}

// ════════════════════════════════════════════════════════════════════════════
//  Gestion des niveaux d'alerte
// ════════════════════════════════════════════════════════════════════════════

typedef enum { ETAT_OK, ETAT_ALERTE, ETAT_DANGER } Etat;
static Etat etat_precedent = ETAT_OK;
static uint8_t sms_envoye  = 0;

void appliquer_etat(Etat etat, uint16_t valeur) {
    LED_VERTE = 0;
    LED_ROUGE = 0;
    BUZZER    = 0;

    switch (etat) {
        case ETAT_OK:
            LED_VERTE = 1;
            sms_envoye = 0;         // Réarmer pour prochaine alerte
            break;

        case ETAT_ALERTE:
            LED_ROUGE = 1;
            BUZZER    = 1;
            __delay_ms(100);
            BUZZER    = 0;
            // Pas de SMS pour niveau alerte (seulement buzzer)
            break;

        case ETAT_DANGER:
            LED_ROUGE = 1;
            BUZZER    = 1;
            // Envoyer SMS si pas encore fait pour cette alerte
            if (!sms_envoye) {
                gsm_send_sms("ALERTE GAZ : Concentration dangereuse detectee ! Ventilation immediate necessaire.");
                sms_envoye = 1;
            }
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Boucle principale
// ════════════════════════════════════════════════════════════════════════════

void main(void) {
    // Configuration GPIO
    TRISB = 0x00;        // PORTB tout en sortie
    PORTB = 0x00;

    // Initialisation périphériques
    uart_init();
    adc_init();
    gsm_init();

    // Séquence de démarrage (clignotement LEDs)
    for (uint8_t i = 0; i < 3; i++) {
        LED_VERTE = 1; LED_ROUGE = 1;
        __delay_ms(200);
        LED_VERTE = 0; LED_ROUGE = 0;
        __delay_ms(200);
    }
    LED_VERTE = 1;    // État initial : OK

    while (1) {
        uint16_t valeur = adc_read();
        Etat etat_actuel;

        if (valeur < SEUIL_ALERTE) {
            etat_actuel = ETAT_OK;
        } else if (valeur < SEUIL_DANGER) {
            etat_actuel = ETAT_ALERTE;
        } else {
            etat_actuel = ETAT_DANGER;
        }

        appliquer_etat(etat_actuel, valeur);
        etat_precedent = etat_actuel;

        __delay_ms(INTERVALLE_MS);
    }
}
