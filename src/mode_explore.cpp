// mode_explore.cpp — Navigation ligne/gare sur la carte LED
//
// Comportement :
//   Niveau 1 (sélection LIGNE) : encodeur scrolle parmi les lignes (R1..S52)
//     -> le nom de la ligne deFile sur le 14 segments
//     -> toutes les LEDs de cette ligne s'allument dans sa couleur officielle
//     -> les autres lignes sont eteintes
//   Niveau 2 (sélection GARE) : clic court depuis le niveau 1 entre en mode gare
//     -> encodeur scrolle parmi les gares appartenant a la ligne selectionnee
//     -> le nom court de la gare (4 car.) s'affiche sur le 14 segments
//     -> la LED de la gare clignote en blanc, les troncons restent allumes
//   LONG_PRESS : retour au niveau 1 depuis le niveau 2, ou sans effet au niveau 1

#include "mode_explore.h"
#include "led_driver.h"
#include "display_driver.h"
#include "stations.h"
#include "colors.h"
#include <Arduino.h>

namespace {

// ---------------------------------------------------------------------------
// Niveau de navigation
// ---------------------------------------------------------------------------
enum class NavLevel : uint8_t { LIGNE, GARE };

NavLevel  g_level        = NavLevel::LIGNE;
uint8_t   g_ligneIdx     = 0;   // index dans LigneId (0..COUNT-1)
uint8_t   g_gareListIdx  = 0;   // index dans g_garesOfLigne[]

// Suivi de la dernière sélection affichée — évite de rappeler showScrolling/showText
// à chaque frame (ce qui remettrait le scroll à 0 toutes les 20 ms).
uint8_t   g_lastDisplayedLigne  = 0xFF;
uint8_t   g_lastDisplayedGare   = 0xFF;
NavLevel  g_lastDisplayedLevel  = NavLevel::LIGNE;

// Bannière d'entrée "MODE LIGNE" pendant 5 secondes ou jusqu'à rotation d'un cran
bool      g_introBannerActive   = false;
uint32_t  g_introBannerStartMs  = 0;
constexpr uint32_t INTRO_BANNER_DURATION_MS = 5000;

// Liste des gares appartenant à la ligne sélectionnée
// (collectées dynamiquement depuis TRONCONS)
static const uint8_t MAX_GARES_PAR_LIGNE = 20;
uint8_t g_garesOfLigne[MAX_GARES_PAR_LIGNE];
uint8_t g_nbGaresOfLigne = 0;

// Clignotement gare (niveau GARE)
uint32_t g_blinkLast  = 0;
bool     g_blinkState = true;
constexpr uint32_t BLINK_PERIOD_MS = 400;

// ---------------------------------------------------------------------------
// Helpers : LED de la ligne
// ---------------------------------------------------------------------------

// Allume toutes les LEDs appartenant à la ligne g_ligneIdx.
// gareHighlight : si >= 0, fait clignoter cette gare en blanc (niveau GARE).
void paintLigne(LigneId id, int gareHighlightIdx, bool blinkOn, uint32_t nowMs) {
    LedDriver::clearAll();

    LineColor c = colorForLigne(id, nowMs);

    // Tronçons
    for (uint8_t t = 0; t < NB_TRONCONS; t++) {
        const Troncon& tr = TRONCONS[t];
        for (uint8_t l = 0; l < tr.nbLignes; l++) {
            if (tr.lignes[l].ligne == id) {
                LedDriver::setPixel(tr.led.bus, tr.led.index, c.r, c.g, c.b);
                break;
            }
        }
    }

    // Gares : allume dans la couleur de la ligne (ou blanc si blink actif)
    for (uint8_t g = 0; g < NB_GARES; g++) {
        const Gare& ga = GARES[g];
        if (!ga.pivotAPI) continue; // skip gare virtuelle HORS

        // Vérifie si cette gare est reliée à la ligne (via un tronçon adjacent)
        bool appartient = false;
        for (uint8_t t = 0; t < NB_TRONCONS && !appartient; t++) {
            const Troncon& tr = TRONCONS[t];
            for (uint8_t l = 0; l < tr.nbLignes; l++) {
                if (tr.lignes[l].ligne == id) {
                    if (tr.lignes[l].idxGareAmont == g ||
                        tr.lignes[l].idxGareAval  == g) {
                        appartient = true;
                        break;
                    }
                }
            }
        }

        if (!appartient) continue;

        if (gareHighlightIdx >= 0 && (uint8_t)gareHighlightIdx == g) {
            // Clignotement blanc pour la gare sélectionnée
            if (blinkOn) {
                LedDriver::setPixel(ga.led.bus, ga.led.index, 255, 255, 255);
            } else {
                LedDriver::setPixel(ga.led.bus, ga.led.index, 0, 0, 0);
            }
        } else {
            // Allume la gare dans la couleur de la ligne (atténuée 50 %)
            LedDriver::setPixel(ga.led.bus, ga.led.index,
                                c.r >> 1, c.g >> 1, c.b >> 1);
        }
    }

    LedDriver::showAll();
}

// Reconstruit g_garesOfLigne[] pour la ligne id.
void buildGareList(LigneId id) {
    g_nbGaresOfLigne = 0;
    // Collecte tous les indices de gares référencés dans les tronçons de la ligne.
    bool seen[64] = {};
    for (uint8_t t = 0; t < NB_TRONCONS; t++) {
        const Troncon& tr = TRONCONS[t];
        for (uint8_t l = 0; l < tr.nbLignes; l++) {
            if (tr.lignes[l].ligne == id) {
                uint8_t a = tr.lignes[l].idxGareAmont;
                uint8_t v = tr.lignes[l].idxGareAval;
                if (a < NB_GARES && !seen[a] && GARES[a].pivotAPI) {
                    seen[a] = true;
                    if (g_nbGaresOfLigne < MAX_GARES_PAR_LIGNE)
                        g_garesOfLigne[g_nbGaresOfLigne++] = a;
                }
                if (v < NB_GARES && !seen[v] && GARES[v].pivotAPI) {
                    seen[v] = true;
                    if (g_nbGaresOfLigne < MAX_GARES_PAR_LIGNE)
                        g_garesOfLigne[g_nbGaresOfLigne++] = v;
                }
                break;
            }
        }
    }
}

// Nombre total de lignes
constexpr uint8_t NB_LIGNES = (uint8_t)LigneId::COUNT;

// ---------------------------------------------------------------------------
// Rendu selon le niveau courant
// ---------------------------------------------------------------------------
void doRender() {
    uint32_t now = millis();
    LigneId id = (LigneId)g_ligneIdx;

    if (g_introBannerActive) {
        if (now - g_introBannerStartMs >= INTRO_BANNER_DURATION_MS) {
            g_introBannerActive = false;
            g_lastDisplayedLigne = 0xFF; // force l'affichage du nom de la ligne
        }
    }

    if (g_level == NavLevel::LIGNE) {
        if (!g_introBannerActive) {
            // Affichage du nom de la ligne une fois la bannière de 5s terminée ou annulée
            if (g_lastDisplayedLevel != NavLevel::LIGNE ||
                g_lastDisplayedLigne != g_ligneIdx) {
                DisplayDriver::showText(nomForLigne(id));
                g_lastDisplayedLevel = NavLevel::LIGNE;
                g_lastDisplayedLigne = g_ligneIdx;
            }
        }

        paintLigne(id, -1, true, now);

    } else { // NavLevel::GARE
        // Clignotement
        if (now - g_blinkLast >= BLINK_PERIOD_MS) {
            g_blinkLast = now;
            g_blinkState = !g_blinkState;
        }

        uint8_t gareListVal = (g_nbGaresOfLigne > 0) ? g_gareListIdx : 0xFF;
        int gareIdx = (g_nbGaresOfLigne > 0)
                      ? (int)g_garesOfLigne[g_gareListIdx]
                      : -1;

        // Met à jour l'afficheur UNIQUEMENT si la gare sélectionnée a changé
        if (g_lastDisplayedLevel != NavLevel::GARE ||
            g_lastDisplayedGare  != gareListVal) {
            if (gareIdx >= 0) {
                DisplayDriver::showText(GARES[gareIdx].nomCourt);
            }
            g_lastDisplayedLevel = NavLevel::GARE;
            g_lastDisplayedGare  = gareListVal;
        }

        paintLigne(id, gareIdx, g_blinkState, now);
    }

    DisplayDriver::update();
}

} // namespace anon

// ===========================================================================
// Interface publique
// ===========================================================================
namespace ModeExplore {

void begin() {
    g_level       = NavLevel::LIGNE;
    g_ligneIdx    = 0;
    g_gareListIdx = 0;
    g_blinkLast   = millis();
    g_blinkState  = true;
    g_introBannerActive  = true;
    g_introBannerStartMs = millis();
    g_lastDisplayedLigne = 0xFF;
    g_lastDisplayedGare  = 0xFF;
    g_lastDisplayedLevel = NavLevel::GARE;
    DisplayDriver::showScrolling("MODE LIGNE", 220);
    doRender();
}

void render() {
    doRender();
}

void handleEncoderDelta(int delta) {
    if (g_introBannerActive) {
        g_introBannerActive = false;
        g_lastDisplayedLigne = 0xFF;
    }
    if (g_level == NavLevel::LIGNE) {
        // Scroll circulaire parmi les lignes
        int next = (int)g_ligneIdx + delta;
        next = ((next % (int)NB_LIGNES) + (int)NB_LIGNES) % (int)NB_LIGNES;
        g_ligneIdx = (uint8_t)next;
    } else {
        // Scroll circulaire parmi les gares de la ligne
        if (g_nbGaresOfLigne == 0) return;
        int next = (int)g_gareListIdx + delta;
        next = ((next % (int)g_nbGaresOfLigne) + (int)g_nbGaresOfLigne) % (int)g_nbGaresOfLigne;
        g_gareListIdx = (uint8_t)next;
    }
}

void handleEncoderButton(ButtonEventType type) {
    if (g_introBannerActive) {
        g_introBannerActive = false;
        g_lastDisplayedLigne = 0xFF;
    }
    if (type == ButtonEventType::SHORT_PRESS) {
        if (g_level == NavLevel::LIGNE) {
            // Entrer dans la sélection de gare pour cette ligne
            g_level = NavLevel::GARE;
            g_gareListIdx = 0;
            buildGareList((LigneId)g_ligneIdx);
        } else {
            // Deuxième clic : retour au niveau ligne (même comportement que LONG_PRESS)
            g_level = NavLevel::LIGNE;
        }
    } else { // LONG_PRESS
        // Retour au niveau ligne depuis n'importe où
        g_level = NavLevel::LIGNE;
    }
}

} // namespace ModeExplore
