#!/usr/bin/env node
// Valide les données de calibration (gares + tronçons) avant de les coller
// dans src/stations.cpp.
//
// Usage :
//   node tools/validate_calib_csv.js [dossier_CSV]          (défaut: CSV/)
//   node tools/validate_calib_csv.js un_export_unique.txt    (sortie "EXPORT CSV" du calib_tool)
//
// Accepte deux mises en page, mélangeables dans un même dossier :
//  - un fichier "Gares.txt" (header bus,index,type,code,api_name,pivot) +
//    un fichier par ligne "<CODE>.txt" (header bus,index,type,ligne,gare_amont,gare_aval
//    au format courant, une ligne de ligne par (bus,index) ; l'ancien header
//    pluriel bus,index,type,lignes,gare_amont,gare_aval — une ligne par LED,
//    codes de ligne joints par ";" — reste accepté en lecture pour les
//    exports pré-existants) ;
//  - une sortie "EXPORT CSV" unique du calib_tool (les deux sections à la
//    suite dans le même fichier, un seul instantané cohérent du firmware).
//
// Modèle de données : amont/aval sont stockés PAR LIGNE sur chaque LED
// (CalibTronconEntry::gareAmontCodes[]/gareAvalCodes[], un slot par ligne
// taguée). Une même LED physique peut donc légitimement être traversée par
// plusieurs lignes qui divergent juste après (embranchement/jonction) : ce
// n'est PAS une erreur. Ce qui reste une erreur, c'est quand la MÊME ligne,
// sur la MÊME LED, se voit attribuer deux paires amont/aval incompatibles
// selon la source (fichiers désynchronisés, ou saisie corrigée entre deux
// exports qu'on a mélangés par erreur).

const fs = require('fs');
const path = require('path');

const target = process.argv[2] || path.join(__dirname, '..', 'CSV');
const NUM_BUSES = 4;
const MAX_LEDS_PER_BUS = 250;

// Chaque élément : { bus, index, ligne, amont, aval, source }
// (une ligne taguée sur une LED donnée = une entrée, quelle que soit la
// mise en page du fichier source).
function parseFile(filePath) {
  const base = path.basename(filePath, path.extname(filePath));
  const lines = fs.readFileSync(filePath, 'utf8').split(/\r?\n/);
  const gareRows = [];
  const ligneRows = [];
  let section = null; // 'GARE' | 'TRONCON_SINGULIER' | 'TRONCON_PLURIEL'
  const warnings = [];
  for (const raw of lines) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith('bus,index,type,code,api_name,pivot')) { section = 'GARE'; continue; }
    if (line.startsWith('bus,index,type,ligne,gare_amont,gare_aval')) { section = 'TRONCON_SINGULIER'; continue; }
    if (line.startsWith('bus,index,type,lignes,gare_amont,gare_aval')) { section = 'TRONCON_PLURIEL'; continue; }
    if (!section) continue; // titre ("R1", "Gares", ...)
    const cols = line.split(',');
    if (section === 'GARE') {
      if (cols[2] !== 'GARE') { warnings.push(`${base}: ligne attendue GARE, trouvé "${cols[2]}": ${line}`); continue; }
      gareRows.push({ bus: Number(cols[0]), index: Number(cols[1]), code: cols[3], apiName: cols[4], pivot: cols[5] === '1', source: base });
    } else if (section === 'TRONCON_SINGULIER') {
      if (cols[2] !== 'TRONCON') { warnings.push(`${base}: ligne attendue TRONCON, trouvé "${cols[2]}": ${line}`); continue; }
      const ligne = cols[3];
      if (base !== 'Gares' && base.toUpperCase() !== 'EXPORT' && ligne !== base) {
        warnings.push(`${base}.txt: ligne "${ligne}" ne correspond pas au nom du fichier`);
      }
      ligneRows.push({ bus: Number(cols[0]), index: Number(cols[1]), ligne, amont: cols[4], aval: cols[5], source: base });
    } else { // TRONCON_PLURIEL — ancien format, une ligne par LED, codes joints par ";"
      if (cols[2] !== 'TRONCON') { warnings.push(`${base}: ligne attendue TRONCON, trouvé "${cols[2]}": ${line}`); continue; }
      const codes = cols[3].split(';').filter(Boolean);
      if (base !== 'Gares' && base.toUpperCase() !== 'EXPORT' && !codes.includes(base)) {
        warnings.push(`${base}.txt: code(s) de ligne "${cols[3]}" ne contient(nent) pas le nom du fichier`);
      }
      // L'ancien format ne distinguait pas l'amont/aval par ligne : la même
      // paire s'applique à tous les codes listés pour cette LED. On le
      // signale si plusieurs lignes sont présentes, car un éventuel
      // embranchement n'aurait pas pu être capturé correctement par ce
      // format — c'est justement pour ça qu'il a été remplacé.
      if (codes.length > 1) {
        warnings.push(`${base}.txt: ancien format (bus${cols[0]} idx${cols[1]}) — ${codes.length} lignes (${codes.join('+')}) partagent une seule paire amont/aval dans ce fichier ; si c'est un embranchement, re-exporter au nouveau format pour distinguer les bornes par ligne.`);
      }
      for (const ligne of codes) {
        ligneRows.push({ bus: Number(cols[0]), index: Number(cols[1]), ligne, amont: cols[4], aval: cols[5], source: base });
      }
    }
  }
  return { gareRows, ligneRows, warnings };
}

function collectFiles(target) {
  const stat = fs.statSync(target);
  if (stat.isFile()) return [target];
  return fs.readdirSync(target)
    .filter(f => f.toLowerCase().endsWith('.txt'))
    .map(f => path.join(target, f));
}

function pairKey(a, b) {
  return [a, b].sort().join('|');
}

function main() {
  const files = collectFiles(target);
  if (!files.length) { console.error('Aucun fichier .txt trouvé dans', target); process.exit(1); }

  const errors = [];
  const warnings = [];
  const info = [];

  const gares = new Map(); // code -> {bus,index,apiName,pivot,source}
  const allLigneRows = [];

  for (const f of files) {
    const { gareRows, ligneRows, warnings: w } = parseFile(f);
    warnings.push(...w);
    allLigneRows.push(...ligneRows);
    for (const g of gareRows) {
      if (gares.has(g.code)) {
        const prev = gares.get(g.code);
        if (prev.bus !== g.bus || prev.index !== g.index) {
          errors.push(`Gare "${g.code}" dupliquée avec des positions différentes : bus${prev.bus} idx${prev.index} (${prev.source}) vs bus${g.bus} idx${g.index} (${g.source})`);
        }
        continue;
      }
      gares.set(g.code, g);
    }
  }

  // Validation plage bus/index des gares
  for (const [code, g] of gares) {
    if (g.bus < 0 || g.bus >= NUM_BUSES) errors.push(`Gare ${code}: bus ${g.bus} hors plage (0-${NUM_BUSES - 1})`);
    if (g.index < 0 || g.index >= MAX_LEDS_PER_BUS) errors.push(`Gare ${code}: index ${g.index} hors plage (0-${MAX_LEDS_PER_BUS - 1})`);
  }

  // Pivots : décision verrouillée par l'utilisateur, toutes les gares sont
  // pivot=1 pour l'instant (pas de sous-ensemble à vérifier). On signale
  // seulement les gares qui ne le seraient PAS, puisque ce serait l'écart
  // par rapport à la décision prise.
  const nonPivots = [...gares.entries()].filter(([, g]) => !g.pivot).map(([c]) => c);
  if (nonPivots.length) {
    warnings.push(`Gares non marquées PIVOT=1 (décision actuelle : toutes les gares sont pivots) : ${nonPivots.join(', ')}`);
  } else if (gares.size > 0) {
    info.push(`Les ${gares.size} gares sont toutes marquées PIVOT=1, conformément à la décision actuelle (round-robin API sur l'ensemble des gares).`);
  }

  // Regroupement par (bus,index,ligne) -> un slot (gareAmontCodes[i]/gareAvalCodes[i])
  // dans le firmware. Plusieurs lignes différentes sur la même (bus,index)
  // sont normales (embranchement) ; ce qu'on recoupe ici, c'est la cohérence
  // ENTRE SOURCES pour une même (bus,index,ligne).
  const byPosLigne = new Map(); // "bus,index,ligne" -> { bus, index, ligne, observations:[] }
  for (const e of allLigneRows) {
    if (e.bus < 0 || e.bus >= NUM_BUSES) { errors.push(`${e.source}: bus ${e.bus} hors plage (idx ${e.index})`); continue; }
    if (e.index < 0 || e.index >= MAX_LEDS_PER_BUS) { errors.push(`${e.source}: index ${e.index} hors plage (bus ${e.bus})`); continue; }
    const key = `${e.bus},${e.index},${e.ligne}`;
    if (!byPosLigne.has(key)) byPosLigne.set(key, { bus: e.bus, index: e.index, ligne: e.ligne, observations: [] });
    byPosLigne.get(key).observations.push(e);
  }

  // Résolution amont/aval par (bus,index,ligne) : les "?" sont ignorés s'il
  // existe une valeur connue ailleurs pour le même (bus,index,ligne) ; les
  // paires non-"?" doivent toutes désigner les deux mêmes gares (ordre
  // indifférent : amont/aval peut être inversé d'une source à l'autre selon
  // le sens de parcours lors de la saisie).
  const resolved = new Map(); // key -> {bus,index,ligne,amont,aval,conflict?}
  for (const [key, slot] of byPosLigne) {
    const concrete = slot.observations.filter(o => o.amont !== '?' && o.aval !== '?');
    const pairs = new Map(); // pairKey -> {amont,aval,sources:[]}
    for (const o of concrete) {
      const pk = pairKey(o.amont, o.aval);
      if (!pairs.has(pk)) pairs.set(pk, { amont: o.amont, aval: o.aval, sources: [] });
      pairs.get(pk).sources.push(o.source);
    }
    if (pairs.size > 1) {
      const detail = [...pairs.values()].map(p => `${p.amont}->${p.aval} [${p.sources.join(', ')}]`).join('  vs  ');
      errors.push(`CONFLIT bus${slot.bus} idx${slot.index} ligne ${slot.ligne} : plusieurs paires amont/aval incompatibles selon la source : ${detail}`);
      resolved.set(key, { bus: slot.bus, index: slot.index, ligne: slot.ligne, amont: null, aval: null, conflict: true });
      continue;
    }
    if (pairs.size === 0) {
      resolved.set(key, { bus: slot.bus, index: slot.index, ligne: slot.ligne, amont: '?', aval: '?' });
      continue;
    }
    const [p] = pairs.values();
    if (p.amont === p.aval) {
      errors.push(`AUTO-BOUCLE bus${slot.bus} idx${slot.index} ligne ${slot.ligne} : amont == aval == "${p.amont}" (sources: ${p.sources.join(', ')}) — valeur suspecte, vérifier sur le hardware (GOTO ${slot.bus} ${slot.index} puis STATUS) et corriger avec SETBORDERS ${slot.ligne} <amont> <aval>.`);
    }
    resolved.set(key, { bus: slot.bus, index: slot.index, ligne: slot.ligne, amont: p.amont, aval: p.aval });
  }

  // "?" encore présents après fusion (aucune source n'a jamais résolu ce côté)
  for (const r of resolved.values()) {
    if (r.conflict) continue;
    if (r.amont === '?' || r.aval === '?') {
      warnings.push(`NON RESOLU bus${r.bus} idx${r.index} ligne ${r.ligne} : amont=${r.amont} aval=${r.aval} — à corriger via SETBORDERS ${r.ligne} avant export.`);
    }
  }

  // Codes de gare référencés mais absents de la table des gares
  const unknownCodes = new Set();
  for (const r of resolved.values()) {
    for (const code of [r.amont, r.aval]) {
      if (code && code !== '?' && !gares.has(code)) unknownCodes.add(code);
    }
  }
  for (const code of unknownCodes) errors.push(`Code de gare "${code}" référencé par des tronçons mais absent de la table des gares`);

  // Vue par position : LEDs portant plusieurs lignes (embranchements) — info,
  // pas une erreur, juste un résumé pour vérification visuelle rapide.
  const byPos = new Map(); // "bus,index" -> Set(ligne)
  for (const r of resolved.values()) {
    const key = `${r.bus},${r.index}`;
    if (!byPos.has(key)) byPos.set(key, new Set());
    byPos.get(key).add(r.ligne);
  }
  // (Détection simple des embranchements : positions où au moins deux lignes
  // taguées sur la même LED ont des paires amont/aval différentes — c'est
  // attendu et volontairement non bloquant, juste listé en info.)
  const jonctionsInfo = [];
  for (const [key, lignesSet] of byPos) {
    if (lignesSet.size < 2) continue;
    const pairsSeen = new Set();
    for (const ligne of lignesSet) {
      const r = resolved.get(`${key},${ligne}`);
      if (!r || r.conflict || r.amont === '?' || r.aval === '?') continue;
      pairsSeen.add(pairKey(r.amont, r.aval));
    }
    if (pairsSeen.size > 1) {
      const [bus, index] = key.split(',');
      const detail = [...lignesSet].map(l => {
        const r = resolved.get(`${key},${l}`);
        return `${l}(${r ? r.amont + '->' + r.aval : '?'})`;
      }).join(', ');
      jonctionsInfo.push(`bus${bus} idx${index} : ${detail}`);
    }
  }
  if (jonctionsInfo.length) {
    info.push(`Embranchements/jonctions détectés (plusieurs lignes sur la même LED avec des bornes différentes — normal sur un croisement physique, à vérifier visuellement) :\n      ${jonctionsInfo.join('\n      ')}`);
  }

  // Vérification topologique par ligne : degré <= 2 par gare, un seul chemin connexe
  const byLigne = new Map();
  for (const r of resolved.values()) {
    if (r.conflict || r.amont === '?' || r.aval === '?') continue;
    if (!byLigne.has(r.ligne)) byLigne.set(r.ligne, new Map());
    const g = byLigne.get(r.ligne);
    if (!g.has(r.amont)) g.set(r.amont, new Set());
    if (!g.has(r.aval)) g.set(r.aval, new Set());
    g.get(r.amont).add(r.aval);
    g.get(r.aval).add(r.amont);
  }
  for (const [ligne, graph] of byLigne) {
    for (const [gare, neighbours] of graph) {
      if (neighbours.size > 2) {
        warnings.push(`Ligne ${ligne} : la gare ${gare} a ${neighbours.size} voisins directs (${[...neighbours].join(', ')}) — un tracé de ligne ne devrait pas se ramifier, vérifier un index mal tagué.`);
      }
    }
    const seen = new Set();
    const components = [];
    for (const start of graph.keys()) {
      if (seen.has(start)) continue;
      const stack = [start], comp = [];
      while (stack.length) {
        const g = stack.pop();
        if (seen.has(g)) continue;
        seen.add(g); comp.push(g);
        for (const n of graph.get(g)) if (!seen.has(n)) stack.push(n);
      }
      components.push(comp);
    }
    if (components.length > 1) {
      warnings.push(`Ligne ${ligne} : tracé non connexe, ${components.length} segments séparés : ${components.map(c => `[${c.join(' - ')}]`).join('  //  ')}`);
    }
  }

  // Rapport
  const sep = '─'.repeat(70);
  console.log(sep);
  console.log(`Validation calibration — ${gares.size} gares, ${byLigne.size} lignes, ${byPos.size} LEDs de tronçon référencées`);
  console.log(sep);

  if (errors.length) {
    console.log(`\n❌ ERREURS BLOQUANTES (${errors.length}) — à corriger avant tout export :\n`);
    errors.forEach(e => console.log('  - ' + e));
  } else {
    console.log('\n✅ Aucune erreur bloquante.');
  }

  if (warnings.length) {
    console.log(`\n⚠️  AVERTISSEMENTS (${warnings.length}) — à vérifier, non bloquant en soi :\n`);
    warnings.forEach(w => console.log('  - ' + w));
  }

  if (info.length) {
    console.log(`\nℹ️  INFO :\n`);
    info.forEach(i => console.log('  - ' + i));
  }

  console.log('\n' + sep);
  console.log(errors.length === 0
    ? 'Prêt : connecte-toi au calib_tool et lance EXPORT CPP pour générer stations.cpp.'
    : `${errors.length} erreur(s) à corriger avant de lancer EXPORT CPP.`);

  process.exitCode = errors.length ? 1 : 0;
}

main();
