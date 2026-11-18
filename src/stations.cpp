// stations.cpp — Généré automatiquement depuis les CSV (Gares.txt + Rx/Sx.txt)
// Ne pas modifier manuellement : régénérer depuis les CSV si le câblage change.

#include "stations.h"
#include <string.h>
#include <strings.h>

// =============================================================================
// GARES — table de correspondance gare → LED (bus, index) + nom API
// Source : CSV/Gares.txt
// niveauPMR = 0 (inconnu) pour toutes, à remplir en Phase 3
// Index dans ce tableau (pour TronconLigne.idxGareAmont/Aval) :
//   0=BRAS  1=VALL  2=PUID  3=PALE  4=ROMO  5=FRIB
//   6=NYON  7=GLAN  8=ROLL  9=ALLA  10=MORG 11=RENE 12=LAUS
//   13=VEVE 14=MONT 15=VILL 16=AIGL 17=BEX  18=STMA
//   19=MORA 20=AVEN 21=PAYE 22=ESTA
//   23=KERZ 24=INS  25=NEUC 26=GRAN 27=YVER
//   28=HORS (gare virtuelle hors-PCB, bout de chaîne LED Kerzers)
// =============================================================================
const Gare GARES[] = {
    // bus 0
    { "BRAS", "Le Brassus",         {0,  0}, true,  0,    900, LHandStatus::NON_CONFORME },  // 0
    { "VALL", "Vallorbe",            {0, 11}, true,  0,   2600, LHandStatus::NON_CONFORME },  // 1
    { "PUID", "Puidoux",             {0, 42}, true,  0,   4200, LHandStatus::NON_CONFORME },  // 2
    { "PALE", "Palezieux",           {0, 45}, true,  0,  10800, LHandStatus::PARTIEL      },  // 3
    { "ROMO", "Romont",              {0, 56}, true,  0,   5400, LHandStatus::NON_CONFORME },  // 4
    { "FRIB", "Fribourg",            {0, 64}, true,  0,  32500, LHandStatus::PARTIEL      },  // 5
    // bus 1
    { "NYON", "Nyon",                {1,  0}, true,  0,  23400, LHandStatus::PARTIEL      },  // 6
    { "GLAN", "Gland",               {1,  3}, true,  0,  11200, LHandStatus::CONFORME     },  // 7
    { "ROLL", "Rolle",               {1,  6}, true,  0,   7100, LHandStatus::CONFORME     },  // 8
    { "ALLA", "Allaman",             {1,  8}, true,  0,   6300, LHandStatus::CONFORME     },  // 9
    { "MORG", "Morges",              {1, 13}, true,  0,  26200, LHandStatus::PARTIEL      },  // 10
    { "RENE", "Renens",              {1, 19}, true,  0,  35100, LHandStatus::CONFORME     },  // 11
    { "LAUS", "Lausanne",            {1, 23}, true,  0, 134000, LHandStatus::PARTIEL      },  // 12
    { "VEVE", "Vevey",               {1, 38}, true,  0,  21500, LHandStatus::PARTIEL      },  // 13
    { "MONT", "Montreux",            {1, 44}, true,  0,  15300, LHandStatus::PARTIEL      },  // 14
    { "VILL", "Villeneuve",          {1, 47}, true,  0,   4800, LHandStatus::INCONNU      },  // 15
    { "AIGL", "Aigle",               {1, 51}, true,  0,   9600, LHandStatus::PARTIEL      },  // 16
    { "BEX",  "Bex",                 {1, 53}, true,  0,   3100, LHandStatus::NON_CONFORME },  // 17
    { "STMA", "St-Maurice",          {1, 55}, true,  0,   3900, LHandStatus::PARTIEL      },  // 18
    // bus 2
    { "MORA", "Morat",               {2,  3}, true,  0,   3600, LHandStatus::INCONNU      },  // 19
    { "AVEN", "Avenches",            {2,  7}, true,  0,   1900, LHandStatus::NON_CONFORME },  // 20
    { "PAYE", "Payerne",             {2,  9}, true,  0,   5800, LHandStatus::INCONNU      },  // 21
    { "ESTA", "Estavayer-Le-Lac",   {2, 18}, true,  0,   2700, LHandStatus::NON_CONFORME },  // 22
    // bus 3
    { "KERZ", "Kerzers",             {3, 10}, true,  0,   3200, LHandStatus::INCONNU      },  // 23
    { "INS",  "INS",                 {3, 17}, true,  0,   2100, LHandStatus::INCONNU      },  // 24
    { "NEUC", "Neuchatel",           {3, 26}, true,  0,  27800, LHandStatus::PARTIEL      },  // 25
    { "GRAN", "Grandson",            {3, 37}, true,  0,   2200, LHandStatus::CONFORME     },  // 26
    { "YVER", "Yverdon-les-Bains",  {3, 39}, true,  0,  18500, LHandStatus::PARTIEL      },  // 27
    // gare virtuelle : bout de chaîne LED hors PCB (S5/S52 côté Berne)
    { "HORS", "",                    {3,  9}, false, 0,      0, LHandStatus::INCONNU      },  // 28
};
const uint8_t NB_GARES = sizeof(GARES) / sizeof(GARES[0]);

// =============================================================================
// TRONCONS — table unifiée LED x lignes
// Chaque entrée = une LED physique + la liste des lignes qui la traversent
// (jusqu'a 4 lignes par LED, cf. struct Troncon dans stations.h).
// idxGareAmont/idxGareAval indiquent entre quelles gares se situe la LED
// (sans sens imposé — le sens n'est pas utilisé dans les modes actuels).
// =============================================================================

#define TL(L,A,B) { LigneId::L, A, B }

const Troncon TRONCONS[] = {

    // --- BUS 0 ---

    // R4 : BRAS(0) -- VALL(1)
    { {0,  1}, { TL(R4, 0, 1) }, 1 },
    { {0,  2}, { TL(R4, 0, 1) }, 1 },
    { {0,  3}, { TL(R4, 0, 1) }, 1 },
    { {0,  4}, { TL(R4, 0, 1) }, 1 },
    { {0,  5}, { TL(R4, 0, 1) }, 1 },
    { {0,  6}, { TL(R4, 0, 1) }, 1 },
    { {0,  7}, { TL(R4, 0, 1) }, 1 },
    { {0,  8}, { TL(R4, 0, 1) }, 1 },
    { {0,  9}, { TL(R4, 0, 1) }, 1 },
    { {0, 10}, { TL(R4, 0, 1) }, 1 },

    // R3+R4 : VALL(1) -- RENE(11)  (index 12-21)
    { {0, 12}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 13}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 14}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 15}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 16}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 17}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 18}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 19}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 20}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },
    { {0, 21}, { TL(R3, 1,11), TL(R4, 1,11) }, 2 },

    // R1+R2+R3+R4 : YVER(27)/VALL(1) -- RENE(11)  (index 22-30, jonction des 4 lignes)
    { {0, 22}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 23}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 24}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 25}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 26}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 27}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 28}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 29}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },
    { {0, 30}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },

    // R8+R9 : LAUS(12) -- PUID(2)  (index 31-41)
    { {0, 31}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 32}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 33}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 34}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 35}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 36}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 37}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 38}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 39}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 40}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },
    { {0, 41}, { TL(R8,12, 2), TL(R9,12, 2) }, 2 },

    // R8+R9+S40 : PUID(2) -- PALE(3)  (index 43-44)
    { {0, 43}, { TL(R8, 2, 3), TL(R9, 2, 3), TL(S40, 2, 3) }, 3 },
    { {0, 44}, { TL(R8, 2, 3), TL(R9, 2, 3), TL(S40, 2, 3) }, 3 },

    // R8+R9 : PALE(3) -- PAYE(21)  (index 46, 78-90)
    { {0, 46}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 78}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 79}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 80}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 81}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 82}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 83}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 84}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 85}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 86}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 87}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 88}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 89}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },
    { {0, 90}, { TL(R8, 3,21), TL(R9, 3,21) }, 2 },

    // S40 : ROMO(4) -- PALE(3)  (index 47-55)
    { {0, 47}, { TL(S40, 4, 3) }, 1 },
    { {0, 48}, { TL(S40, 4, 3) }, 1 },
    { {0, 49}, { TL(S40, 4, 3) }, 1 },
    { {0, 50}, { TL(S40, 4, 3) }, 1 },
    { {0, 51}, { TL(S40, 4, 3) }, 1 },
    { {0, 52}, { TL(S40, 4, 3) }, 1 },
    { {0, 53}, { TL(S40, 4, 3) }, 1 },
    { {0, 54}, { TL(S40, 4, 3) }, 1 },
    { {0, 55}, { TL(S40, 4, 3) }, 1 },

    // S40 : FRIB(5) -- ROMO(4)  (index 57-63)
    { {0, 57}, { TL(S40, 5, 4) }, 1 },
    { {0, 58}, { TL(S40, 5, 4) }, 1 },
    { {0, 59}, { TL(S40, 5, 4) }, 1 },
    { {0, 60}, { TL(S40, 5, 4) }, 1 },
    { {0, 61}, { TL(S40, 5, 4) }, 1 },
    { {0, 62}, { TL(S40, 5, 4) }, 1 },
    { {0, 63}, { TL(S40, 5, 4) }, 1 },

    // S30 : YVER(27) -- FRIB(5)  (index 65-77)
    { {0, 65}, { TL(S30,27, 5) }, 1 },
    { {0, 66}, { TL(S30,27, 5) }, 1 },
    { {0, 67}, { TL(S30,27, 5) }, 1 },
    { {0, 68}, { TL(S30,27, 5) }, 1 },
    { {0, 69}, { TL(S30,27, 5) }, 1 },
    { {0, 70}, { TL(S30,27, 5) }, 1 },
    { {0, 71}, { TL(S30,27, 5) }, 1 },
    { {0, 72}, { TL(S30,27, 5) }, 1 },
    { {0, 73}, { TL(S30,27, 5) }, 1 },
    { {0, 74}, { TL(S30,27, 5) }, 1 },
    { {0, 75}, { TL(S30,27, 5) }, 1 },
    { {0, 76}, { TL(S30,27, 5) }, 1 },
    { {0, 77}, { TL(S30,27, 5) }, 1 },

    // --- BUS 1 ---

    // RE33 : NYON(6) -- GLAN(7)  (index 1-2)
    { {1,  1}, { TL(RE33, 6, 7) }, 1 },
    { {1,  2}, { TL(RE33, 6, 7) }, 1 },

    // RE33 : GLAN(7) -- ROLL(8)  (index 4-5)
    { {1,  4}, { TL(RE33, 7, 8) }, 1 },
    { {1,  5}, { TL(RE33, 7, 8) }, 1 },

    // RE33 : ROLL(8) -- ALLA(9)  (index 7)
    { {1,  7}, { TL(RE33, 8, 9) }, 1 },

    // R8+R9+RE33 : ALLA(9) -- MORG(10)  (index 9-12)
    { {1,  9}, { TL(R8, 9,10), TL(R9, 9,10), TL(RE33, 9,10) }, 3 },
    { {1, 10}, { TL(R8, 9,10), TL(R9, 9,10), TL(RE33, 9,10) }, 3 },
    { {1, 11}, { TL(R8, 9,10), TL(R9, 9,10), TL(RE33, 9,10) }, 3 },
    { {1, 12}, { TL(R8, 9,10), TL(R9, 9,10), TL(RE33, 9,10) }, 3 },

    // R8+R9+RE33 : MORG(10) -- RENE(11)  (index 14-17)
    { {1, 14}, { TL(R8,10,11), TL(R9,10,11), TL(RE33,10,11) }, 3 },
    { {1, 15}, { TL(R8,10,11), TL(R9,10,11), TL(RE33,10,11) }, 3 },
    { {1, 16}, { TL(R8,10,11), TL(R9,10,11), TL(RE33,10,11) }, 3 },
    { {1, 17}, { TL(R8,10,11), TL(R9,10,11), TL(RE33,10,11) }, 3 },

    // R1+R2+R3+R4 : YVER(27)/VALL(1) -- RENE(11)  (index 18 bus1 — jonction)
    { {1, 18}, { TL(R1,27,11), TL(R2,27,11), TL(R3, 1,11), TL(R4, 1,11) }, 4 },

    // R1+R2+RE33 : RENE(11) -- LAUS(12)  (index 20-22)
    { {1, 20}, { TL(R1,11,12), TL(R2,11,12), TL(RE33,11,12) }, 3 },
    { {1, 21}, { TL(R1,11,12), TL(R2,11,12), TL(RE33,11,12) }, 3 },
    { {1, 22}, { TL(R1,11,12), TL(R2,11,12), TL(RE33,11,12) }, 3 },

    // R1+R2+R3+RE33 : LAUS(12) -- VEVE(13)  (index 24-37)
    { {1, 24}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 25}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 26}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 27}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 28}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 29}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 30}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 31}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 32}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 33}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 34}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 35}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 36}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },
    { {1, 37}, { TL(R1,12,13), TL(R2,12,13), TL(R3,12,13), TL(RE33,12,13) }, 4 },

    // R1+R2+RE33 : VEVE(13) -- MONT(14)  (index 39-43)
    { {1, 39}, { TL(R1,13,14), TL(R2,13,14), TL(RE33,13,14) }, 3 },
    { {1, 40}, { TL(R1,13,14), TL(R2,13,14), TL(RE33,13,14) }, 3 },
    { {1, 41}, { TL(R1,13,14), TL(R2,13,14), TL(RE33,13,14) }, 3 },
    { {1, 42}, { TL(R1,13,14), TL(R2,13,14), TL(RE33,13,14) }, 3 },
    { {1, 43}, { TL(R1,13,14), TL(R2,13,14), TL(RE33,13,14) }, 3 },

    // R1+R2+RE33 : MONT(14) -- VILL(15)  (index 45-46)
    { {1, 45}, { TL(R1,14,15), TL(R2,14,15), TL(RE33,14,15) }, 3 },
    { {1, 46}, { TL(R1,14,15), TL(R2,14,15), TL(RE33,14,15) }, 3 },

    // R1+R2+RE33 : VILL(15) -- AIGL(16)  (index 48-50)
    { {1, 48}, { TL(R1,15,16), TL(R2,15,16), TL(RE33,15,16) }, 3 },
    { {1, 49}, { TL(R1,15,16), TL(R2,15,16), TL(RE33,15,16) }, 3 },
    { {1, 50}, { TL(R1,15,16), TL(R2,15,16), TL(RE33,15,16) }, 3 },

    // R1+R2+RE33 : AIGL(16) -- BEX(17)  (index 52)
    { {1, 52}, { TL(R1,16,17), TL(R2,16,17), TL(RE33,16,17) }, 3 },

    // R1+RE33 : BEX(17) -- STMA(18)  (index 54)
    { {1, 54}, { TL(R1,17,18), TL(RE33,17,18) }, 2 },

    // --- BUS 2 ---

    // S5+S52 : MORA(19) -- KERZ(23)  (index 0-2)
    { {2,  0}, { TL(S5,19,23), TL(S52,19,23) }, 2 },
    { {2,  1}, { TL(S5,19,23), TL(S52,19,23) }, 2 },
    { {2,  2}, { TL(S5,19,23), TL(S52,19,23) }, 2 },

    // R9+S5+S52 : AVEN(20) -- MORA(19)  (index 4-6)
    { {2,  4}, { TL(R9,20,19), TL(S5,20,19), TL(S52,20,19) }, 3 },
    { {2,  5}, { TL(R9,20,19), TL(S5,20,19), TL(S52,20,19) }, 3 },
    { {2,  6}, { TL(R9,20,19), TL(S5,20,19), TL(S52,20,19) }, 3 },

    // R8+R9+S52 : PAYE(21) -- AVEN(20)  (index 8)
    { {2,  8}, { TL(R8,21,20), TL(R9,21,20), TL(S52,21,20) }, 3 },

    // R8+S30 : PALE(3)/ESTA(22) -- PAYE(21)  (index 10)
    { {2, 10}, { TL(R8, 3,21), TL(S30,22,21) }, 2 },

    // S30 : ESTA(22) -- PAYE(21)  (index 11-17)
    { {2, 11}, { TL(S30,22,21) }, 1 },
    { {2, 12}, { TL(S30,22,21) }, 1 },
    { {2, 13}, { TL(S30,22,21) }, 1 },
    { {2, 14}, { TL(S30,22,21) }, 1 },
    { {2, 15}, { TL(S30,22,21) }, 1 },
    { {2, 16}, { TL(S30,22,21) }, 1 },
    { {2, 17}, { TL(S30,22,21) }, 1 },

    // S30 : YVER(27) -- ESTA(22)  (index 19-24)
    { {2, 19}, { TL(S30,27,22) }, 1 },
    { {2, 20}, { TL(S30,27,22) }, 1 },
    { {2, 21}, { TL(S30,27,22) }, 1 },
    { {2, 22}, { TL(S30,27,22) }, 1 },
    { {2, 23}, { TL(S30,27,22) }, 1 },
    { {2, 24}, { TL(S30,27,22) }, 1 },

    // R7 : PUID(2) -- VEVE(13)  (index 25-28)
    { {2, 25}, { TL(R7, 2,13) }, 1 },
    { {2, 26}, { TL(R7, 2,13) }, 1 },
    { {2, 27}, { TL(R7, 2,13) }, 1 },
    { {2, 28}, { TL(R7, 2,13) }, 1 },

    // --- BUS 3 ---

    // S5+S52 : KERZ(23) -- HORS(28) (bout de chaîne hors PCB, index 0-9)
    { {3,  0}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  1}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  2}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  3}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  4}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  5}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  6}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  7}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  8}, { TL(S5,23,28), TL(S52,23,28) }, 2 },
    { {3,  9}, { TL(S5,23,28), TL(S52,23,28) }, 2 },

    // S5 : KERZ(23) -- INS(24)  (index 11-16)
    { {3, 11}, { TL(S5,23,24) }, 1 },
    { {3, 12}, { TL(S5,23,24) }, 1 },
    { {3, 13}, { TL(S5,23,24) }, 1 },
    { {3, 14}, { TL(S5,23,24) }, 1 },
    { {3, 15}, { TL(S5,23,24) }, 1 },
    { {3, 16}, { TL(S5,23,24) }, 1 },

    // S52 : INS(24) -- KERZ(23)  (index 11-16, mêmes LEDs que S5 sens opposé)
    // Représentées implicitement par les entrées S5 ci-dessus.

    // S5 : INS(24) -- NEUC(25)  (index 18-25)
    { {3, 18}, { TL(S5,24,25) }, 1 },
    { {3, 19}, { TL(S5,24,25) }, 1 },
    { {3, 20}, { TL(S5,24,25) }, 1 },
    { {3, 21}, { TL(S5,24,25) }, 1 },
    { {3, 22}, { TL(S5,24,25) }, 1 },
    { {3, 23}, { TL(S5,24,25) }, 1 },
    { {3, 24}, { TL(S5,24,25) }, 1 },
    { {3, 25}, { TL(S5,24,25) }, 1 },

    // R13 : GRAN(26) -- NEUC(25)  (index 27-36)
    { {3, 27}, { TL(R13,26,25) }, 1 },
    { {3, 28}, { TL(R13,26,25) }, 1 },
    { {3, 29}, { TL(R13,26,25) }, 1 },
    { {3, 30}, { TL(R13,26,25) }, 1 },
    { {3, 31}, { TL(R13,26,25) }, 1 },
    { {3, 32}, { TL(R13,26,25) }, 1 },
    { {3, 33}, { TL(R13,26,25) }, 1 },
    { {3, 34}, { TL(R13,26,25) }, 1 },
    { {3, 35}, { TL(R13,26,25) }, 1 },
    { {3, 36}, { TL(R13,26,25) }, 1 },

    // R1+R13 : GRAN(26) -- YVER(27)  (index 38)
    { {3, 38}, { TL(R1,26,27), TL(R13,27,26) }, 2 },

    // R1+R2 : YVER(27) -- RENE(11)  (index 40-52)
    { {3, 40}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 41}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 42}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 43}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 44}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 45}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 46}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 47}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 48}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 49}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 50}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 51}, { TL(R1,27,11), TL(R2,27,11) }, 2 },
    { {3, 52}, { TL(R1,27,11), TL(R2,27,11) }, 2 },

};
const uint8_t NB_TRONCONS = sizeof(TRONCONS) / sizeof(TRONCONS[0]);

// =============================================================================

int findGareIndexByCode(const char* nomCourt) {
    for (uint8_t i = 0; i < NB_GARES; i++) {
        if (strcasecmp(GARES[i].nomCourt, nomCourt) == 0) return i;
    }
    return -1;
}

uint8_t affluencePercentForGare(const Gare& g) {
    if (g.voyageurs > 100000) return 100;
    if (g.voyageurs > 30000)  return 80;
    if (g.voyageurs > 25000)  return 60;
    if (g.voyageurs > 20000)  return 40;
    if (g.voyageurs > 10000)  return 20;
    return 10;
}

LineColor colorForLHand(LHandStatus status) {
    switch (status) {
        case LHandStatus::CONFORME:     return LineColor{0x00, 0xE6, 0x00}; // Vert : conforme entièrement
        case LHandStatus::PARTIEL:      return LineColor{0xFF, 0x66, 0x00}; // Orange : partiellement conforme
        case LHandStatus::NON_CONFORME: return LineColor{0xE6, 0x00, 0x00}; // Rouge : non conforme
        case LHandStatus::INCONNU:      return LineColor{0x00, 0x66, 0xFF}; // Bleu : statut inconnu
        default:                        return LineColor{0x00, 0x66, 0xFF};
    }
}
