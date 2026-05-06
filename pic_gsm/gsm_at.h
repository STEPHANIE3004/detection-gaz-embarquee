/*
 * gsm_at.h — Commandes AT SIM800L / SIM900
 * ==========================================
 * Référence des commandes AT utilisées pour l'envoi de SMS.
 * Compatible avec les modules GSM SIM800L, SIM800C, SIM900.
 */
#ifndef GSM_AT_H
#define GSM_AT_H

/* ── Commandes AT de base ────────────────────────────────────────────────── */
#define AT_TEST         "AT"              // Test communication → réponse "OK"
#define AT_RESET        "ATZ"             // Reset configuration module
#define AT_ECHO_OFF     "ATE0"            // Désactiver l'écho des commandes
#define AT_SIGNAL       "AT+CSQ"          // Niveau signal (0-31, 99=inconnu)
#define AT_SIM_STATUS   "AT+CPIN?"        // État SIM (READY / PIN requis)
#define AT_NETWORK      "AT+CREG?"        // Enregistrement réseau (0,1=OK)
#define AT_OPERATOR     "AT+COPS?"        // Opérateur réseau actuel

/* ── SMS ─────────────────────────────────────────────────────────────────── */
#define AT_SMS_MODE_TXT "AT+CMGF=1"       // Mode texte (vs mode PDU)
#define AT_SMS_MODE_PDU "AT+CMGF=0"       // Mode PDU (encodage binaire)
#define AT_SMS_CHARSET  "AT+CSCS=\"GSM\"" // Jeu de caractères GSM 7-bit
#define AT_SMS_SEND     "AT+CMGS=\""      // Envoi SMS : AT+CMGS="<numero>"
                                          // Suivi du message + Ctrl+Z (0x1A)
#define AT_SMS_LIST     "AT+CMGL=\"ALL\"" // Lister tous les SMS reçus
#define AT_SMS_READ     "AT+CMGR="        // Lire SMS par index
#define AT_SMS_DELETE   "AT+CMGD="        // Supprimer SMS par index

/* ── Gestion énergie ─────────────────────────────────────────────────────── */
#define AT_POWERDOWN    "AT+CPOWD=1"      // Arrêt propre du module

/* ── Séquence d'initialisation recommandée ───────────────────────────────── */
/*
 * 1. Attendre 3s après mise sous tension
 * 2. AT          → vérifier "OK"
 * 3. ATE0        → désactiver écho
 * 4. AT+CPIN?    → vérifier SIM présente et déverrouillée
 * 5. AT+CREG?    → attendre enregistrement réseau (réponse "+CREG: 0,1")
 * 6. AT+CMGF=1   → mode texte
 * 7. AT+CSCS="GSM" → encodage
 * → Prêt à envoyer des SMS
 */

/* ── Codes de retour typiques ─────────────────────────────────────────────── */
/*
 * "OK"           : commande acceptée
 * "ERROR"        : commande invalide ou non supportée
 * "+CMS ERROR:"  : erreur SMS (ex: 302 = opération non autorisée)
 * "+CME ERROR:"  : erreur module (ex: 10 = SIM non insérée)
 * "> "           : module prêt à recevoir le corps du SMS
 */

#endif /* GSM_AT_H */
