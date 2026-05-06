/*
 * detection_gaz.c - Systeme de detection de gaz dangereux embarque
 * Cible : PIC18F4550 / STM32 (simulation sur PC)
 * Capteurs : MQ-2 (GPL/fumee), MQ-7 (CO), MQ-135 (NH3/NOx)
 * Auteure : Vanelle Stephanie MANGOUA DJOUSSEU
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

/* ─── Calibration capteurs ─────────────────────────────────────────── */
#define MQ2_R0       9.8f    /* resistance propre en air pur */
#define MQ7_R0       10.0f
#define MQ135_R0     76.63f
#define RL           10.0f   /* resistance charge (kohm) */
#define VCC          5.0f    /* tension alimentation */

/* ─── Seuils d'alerte (ppm) ──────────────────────────────────────────── */
#define SEUIL_GPL_WARN   300.0f
#define SEUIL_GPL_DANGER 1000.0f
#define SEUIL_CO_WARN    50.0f
#define SEUIL_CO_DANGER  200.0f
#define SEUIL_CO2_WARN   800.0f
#define SEUIL_CO2_DANGER 2000.0f

/* ─── Niveaux d'alerte ───────────────────────────────────────────────── */
typedef enum {
    NIVEAU_OK      = 0,
    NIVEAU_WARNING = 1,
    NIVEAU_DANGER  = 2,
    NIVEAU_ALARME  = 3
} NiveauAlerte;

typedef struct {
    float         gpl_ppm;
    float         co_ppm;
    float         co2_ppm;
    NiveauAlerte  niveau;
    bool          buzzer_on;
    bool          led_rouge;
    bool          led_jaune;
    uint32_t      timestamp;
} MesureGaz;

/* ─── Conversion ADC -> ppm (courbe log-log de la datasheet) ─────────── */
static float adc_vers_rs(uint16_t adc_val) {
    float v_out = adc_val * (VCC / 1023.0f);
    return RL * (VCC - v_out) / v_out;
}

static float rs_vers_ppm_mq2(float rs) {
    /* y = 10^( (log(x) - log(Ro)) / slope ) avec slope=-0.47 */
    return 613.9f * powf(rs / MQ2_R0, -2.074f);
}

static float rs_vers_ppm_mq7(float rs) {
    return 99.042f * powf(rs / MQ7_R0, -1.518f);
}

static float rs_vers_ppm_mq135(float rs) {
    return 116.60f * powf(rs / MQ135_R0, -2.769f);
}

/* ─── Simulation ADC ─────────────────────────────────────────────────── */
static uint16_t lire_adc_mq2(void) {
    return (uint16_t)(300 + rand() % 400);
}
static uint16_t lire_adc_mq7(void) {
    return (uint16_t)(100 + rand() % 300);
}
static uint16_t lire_adc_mq135(void) {
    return (uint16_t)(200 + rand() % 350);
}

/* ─── Acquisition complete ───────────────────────────────────────────── */
static MesureGaz acquerir(void) {
    MesureGaz m;
    m.timestamp = (uint32_t)time(NULL);

    float rs2   = adc_vers_rs(lire_adc_mq2());
    float rs7   = adc_vers_rs(lire_adc_mq7());
    float rs135 = adc_vers_rs(lire_adc_mq135());

    m.gpl_ppm = rs_vers_ppm_mq2(rs2);
    m.co_ppm  = rs_vers_ppm_mq7(rs7);
    m.co2_ppm = rs_vers_ppm_mq135(rs135);

    /* Classification niveau d'alerte */
    m.niveau    = NIVEAU_OK;
    m.buzzer_on = false;
    m.led_rouge = false;
    m.led_jaune = false;

    if (m.gpl_ppm > SEUIL_GPL_DANGER || m.co_ppm > SEUIL_CO_DANGER
        || m.co2_ppm > SEUIL_CO2_DANGER) {
        m.niveau    = NIVEAU_ALARME;
        m.buzzer_on = true;
        m.led_rouge = true;
    } else if (m.gpl_ppm > SEUIL_GPL_WARN || m.co_ppm > SEUIL_CO_WARN
               || m.co2_ppm > SEUIL_CO2_WARN) {
        m.niveau    = NIVEAU_DANGER;
        m.buzzer_on = true;
        m.led_jaune = true;
    } else {
        m.niveau = NIVEAU_OK;
    }
    return m;
}

static const char *niveau_str(NiveauAlerte n) {
    switch (n) {
        case NIVEAU_OK:      return "OK      ";
        case NIVEAU_WARNING: return "WARNING ";
        case NIVEAU_DANGER:  return "DANGER  ";
        case NIVEAU_ALARME:  return "ALARME!!";
        default:             return "INCONNU ";
    }
}

/* ─── Affichage mesure ───────────────────────────────────────────────── */
static void afficher_mesure(const MesureGaz *m, int idx) {
    printf("[%02d] GPL=%6.1f ppm | CO=%5.1f ppm | CO2=%6.1f ppm | %s | BUZ=%s LED_R=%s\n",
           idx,
           m->gpl_ppm, m->co_ppm, m->co2_ppm,
           niveau_str(m->niveau),
           m->buzzer_on ? "ON " : "OFF",
           m->led_rouge ? "ON" : "OFF");
}

/* ─── Main ──────────────────────────────────────────────────────────── */
int main(void) {
    srand((unsigned)time(NULL));
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  SYSTEME DETECTION GAZ EMBARQUE - MQ2/MQ7/MQ135     ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("Seuils : GPL>%.0fppm DANGER, CO>%.0fppm DANGER\n\n",
           SEUIL_GPL_DANGER, SEUIL_CO_DANGER);

    int nb_alarmes = 0;
    for (int i = 1; i <= 20; i++) {
        MesureGaz m = acquerir();
        afficher_mesure(&m, i);
        if (m.niveau >= NIVEAU_DANGER) nb_alarmes++;
    }

    printf("\nTotal alertes (DANGER ou ALARME) : %d / 20\n", nb_alarmes);
    return 0;
}
