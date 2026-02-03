# 📊 Guide de conversion Fiche Produit → PowerPoint

## 🎯 Objectif
Ce guide explique comment transformer le fichier `FICHE_PRODUIT_APRU40.md` en présentation PowerPoint professionnelle.

---

## 📋 Structure recommandée de la présentation

### Slides suggérées (20-25 diapositives)

1. **Page de titre** (1 slide)
   - Titre : "APRU40 - Réseau de capteurs ESP32"
   - Sous-titre : "Solution IoT industrielle sécurisée"
   - Logo/Image

2. **Vue d'ensemble** (1 slide)
   - 6 points clés avec icônes
   - Chiffres clés en gros

3. **Architecture système** (1 slide)
   - Schéma topologie réseau
   - Flux de données visuels

4. **Composants matériels** (1-2 slides)
   - Tableau composants avec images
   - Photos ESP32-POE-ISO

5. **Technologie ESP-NOW** (2 slides)
   - Slide 1 : Tableau comparatif (ESP-NOW vs LoRa vs BLE)
   - Slide 2 : Avantages ESP-NOW (6 points)

6. **Sécurité** (2 slides)
   - Slide 1 : Architecture sécurité (schéma paquet)
   - Slide 2 : Mécanismes de protection (tableau)

7. **Acquisition données** (1 slide)
   - Tableau ADC
   - Format JSON exemple

8. **Scanner Bluetooth** (1 slide)
   - Caractéristiques + image scanner
   - Types de codes supportés

9. **Performances** (2 slides)
   - Slide 1 : Occupation canal, latence, portée
   - Slide 2 : Graphiques de performance

10. **Architecture logicielle** (1 slide)
    - Arborescence composants
    - Tableau tasks FreeRTOS

11. **Déploiement** (1 slide)
    - 6 étapes en timeline
    - Outils nécessaires

12. **Monitoring** (1 slide)
    - Statistiques sécurité
    - Indicateurs santé

13. **Maintenance** (1 slide)
    - Tableau dépannage (symptôme → solution)

14. **Spécifications techniques** (1 slide)
    - Environnement, alimentation, interfaces
    - Certifications

15. **Coûts** (1 slide)
    - Graphique coût par nœud
    - Total réseau 30 nœuds

16. **Cas d'usage** (1 slide)
    - Applications industrielles (icônes)
    - Secteurs cibles

17. **Avantages clés** (1 slide)
    - 3 colonnes : Technique / Opérationnel / Économique
    - Checkmarks pour chaque avantage

18. **Conclusion** (1 slide)
    - 5 raisons de choisir APRU40
    - Call to action

19. **Contact** (1 slide)
    - Informations projet
    - Support et documentation

20. **Questions** (1 slide optionnelle)
    - Slide Q&A avec contact

---

## 🛠️ Méthodes de conversion

### Méthode 1 : Conversion automatique (Rapide)

#### Outils recommandés :
1. **Marp** (Markdown to PowerPoint)
   ```bash
   npm install -g @marp-team/marp-cli
   marp FICHE_PRODUIT_APRU40.md -o APRU40_Presentation.pptx
   ```

2. **Pandoc** (Conversion universelle)
   ```bash
   pandoc FICHE_PRODUIT_APRU40.md -o APRU40_Presentation.pptx
   ```

3. **VSCode + Marp extension**
   - Installer extension "Marp for VS Code"
   - Ouvrir fichier .md
   - Exporter vers PPTX

#### Avantages :
- ✅ Très rapide (2-5 minutes)
- ✅ Structure préservée
- ✅ Automatisable

#### Inconvénients :
- ⚠️ Mise en page basique
- ⚠️ Personnalisation limitée
- ⚠️ Design générique

---

### Méthode 2 : Conversion manuelle (Qualité professionnelle)

#### Étape 1 : Préparation du template PowerPoint
1. Ouvrir PowerPoint
2. Créer/choisir un template professionnel
3. Définir les couleurs de marque :
   - Primaire : Bleu (#2196F3) ou couleur entreprise
   - Secondaire : Vert (#4CAF50) pour succès
   - Accent : Orange (#FF9800) pour alertes
4. Définir polices :
   - Titres : Arial Bold ou Segoe UI Bold
   - Corps : Arial ou Segoe UI (11-14pt)

#### Étape 2 : Création des slides (section par section)

**Slide 1 : Page de titre**
```
Titre : APRU40
Sous-titre : Réseau de capteurs ESP32 avec communication sécurisée ESP-NOW
Background : Image industrielle/IoT
Footer : 2026 | Version 1.0
```

**Slide 2 : Vue d'ensemble**
```
Titre : Points clés
Layout : 2 colonnes, 3 lignes avec icônes
Contenu :
  ✓ 30 nœuds capteurs
  ✓ <10ms latence
  ✓ AES-256 sécurisé
  ✓ 16 canaux ADC
  ✓ Scanner Bluetooth
  ✓ 200m portée
```

**Slide 3 : Architecture**
```
Titre : Architecture système
Layout : Diagramme centré
Contenu : Copier schéma topologie
Utiliser SmartArt → Hiérarchie ou Process
```

**Slide 4 : Composants matériels**
```
Titre : Composants (par nœud)
Layout : Tableau 6 lignes × 4 colonnes
Copier tableau de la section "Composants matériels"
Ajouter images si disponibles
```

**Slide 5 : Comparaison technologies**
```
Titre : Pourquoi ESP-NOW ?
Layout : Tableau comparatif
Copier tableau ESP-NOW vs LoRa vs BLE
Mettre en évidence colonne ESP-NOW (fond vert clair)
Utiliser ✅ ⚠️ ❌ pour lisibilité
```

**Slide 6 : Avantages ESP-NOW**
```
Titre : Avantages ESP-NOW
Layout : Liste à puces (6 points)
Contenu : Copier section "Avantages ESP-NOW pour ce projet"
Utiliser animations : Apparition par point (optionnel)
```

**Slide 7 : Sécurité - Architecture**
```
Titre : Architecture de sécurité
Layout : Schéma centré + texte
Contenu : Reproduire schéma paquet ESP-NOW
Utiliser formes PowerPoint ou copier bloc code
Ajouter : "Niveau 7/10 - Industriel"
```

**Slide 8 : Sécurité - Mécanismes**
```
Titre : Mécanismes de protection
Layout : Tableau 4 lignes × 3 colonnes
Copier tableau mécanismes (Chiffrement, Auth, Anti-replay, Whitelist)
Couleur fond : Bleu clair pour header
```

**Slide 9 : Acquisition données**
```
Titre : Acquisition de données
Layout : Tableau + bloc code
Haut : Tableau ADC (4 lignes × 5 colonnes)
Bas : Format JSON (petite police, monospace)
```

**Slide 10 : Scanner Bluetooth**
```
Titre : Scanner Zebra DS2278
Layout : Image + liste
Gauche : Image scanner (si disponible)
Droite : 
  - Bluetooth Classic SPP
  - Portée 10-30m
  - 1D/2D codes
  - Medium Security
```

**Slide 11-12 : Performances**
```
Slide 11 - Titre : Performances réseau
  - Occupation : 0.3%
  - Latence : <20ms
  - Débit : 1 Mbps

Slide 12 - Titre : Portée et capacité
  - Graphique barres (portée par environnement)
  - 30 nœuds validés / 100+ théorique
```

**Slide 13 : Architecture logicielle**
```
Titre : Composants logiciels
Layout : 2 colonnes
Gauche : Arborescence composants
Droite : Tableau tasks FreeRTOS
```

**Slide 14 : Déploiement**
```
Titre : Déploiement en 6 étapes
Layout : SmartArt → Process horizontal
1. Générer clés
2. Configurer
3. Compiler
4. Pairing
5. Vérifier
6. Installer
```

**Slide 15 : Monitoring**
```
Titre : Monitoring et diagnostics
Layout : 2 tableaux
Haut : Statistiques sécurité (4 lignes)
Bas : Indicateurs santé (4 lignes)
```

**Slide 16 : Maintenance**
```
Titre : Dépannage rapide
Layout : Tableau 5 lignes × 3 colonnes
Colonnes : Symptôme | Cause | Solution
Design : Alternance lignes grises/blanches
```

**Slide 17 : Spécifications**
```
Titre : Spécifications techniques
Layout : 4 blocs (2×2)
Bloc 1 : Environnement (-40°C à +85°C, etc.)
Bloc 2 : Alimentation (POE, 0.45W)
Bloc 3 : Interfaces (Ethernet, I2C, USB)
Bloc 4 : Conformité (CE, FCC, WiFi, BT)
```

**Slide 18 : Coûts**
```
Titre : Coûts estimatifs
Layout : 2 graphiques
Gauche : Pie chart "Coût par nœud" (composants)
Droite : Bar chart "Total réseau 30 nœuds" (1845€)
Gros chiffre : 61.50€/nœud
```

**Slide 19 : Cas d'usage**
```
Titre : Applications et secteurs
Layout : Icônes + texte (2 sections)
Haut : Applications (5 icônes avec texte)
Bas : Secteurs (5 icônes avec texte)
```

**Slide 20 : Avantages clés**
```
Titre : Pourquoi choisir APRU40 ?
Layout : 3 colonnes égales
Colonne 1 : Technique (5 points ✅)
Colonne 2 : Opérationnel (5 points ✅)
Colonne 3 : Économique (5 points ✅)
```

**Slide 21 : Conclusion**
```
Titre : APRU40 - Solution IoT professionnelle
Layout : Grand texte centré + liste
5 raisons numérotées (grandes puces)
Call to action : "Déployez votre réseau dès aujourd'hui"
```

**Slide 22 : Contact**
```
Titre : Contact et support
Layout : 2 colonnes
Gauche : Infos projet (année, licence, version)
Droite : Support (GitHub, email, site web)
Footer : www.apru40.example.com
```

#### Étape 3 : Finalisation
1. **Vérifier cohérence** :
   - Polices identiques
   - Couleurs cohérentes
   - Alignements propres
   
2. **Ajouter transitions** (optionnel) :
   - Transition "Fondu" entre slides (subtil)
   - Éviter animations excessives
   
3. **Numéroter les slides** :
   - Format : "3/22" en bas à droite
   
4. **Ajouter notes du présentateur** :
   - Utiliser section "Notes" pour détails additionnels
   - Copier paragraphes pertinents de la fiche produit

#### Étape 4 : Export
1. Enregistrer en .pptx (PowerPoint)
2. Exporter en PDF (pour partage)
3. Créer version 16:9 et 4:3 si besoin

---

## 🎨 Conseils de design

### Palette de couleurs recommandée
```
Primaire   : #2196F3 (Bleu) - Titres, accents
Secondaire : #4CAF50 (Vert) - Succès, validations
Accent     : #FF9800 (Orange) - Alertes, warnings
Neutre     : #607D8B (Gris bleu) - Texte secondaire
Fond       : #FFFFFF (Blanc) ou #F5F5F5 (Gris clair)
Texte      : #212121 (Noir) ou #424242 (Gris foncé)
```

### Typographie
```
Titres      : Arial Bold ou Segoe UI Bold, 32-40pt
Sous-titres : Arial Bold ou Segoe UI Bold, 24-28pt
Corps       : Arial ou Segoe UI, 14-18pt
Code/Données: Consolas ou Courier New, 11-14pt
```

### Iconographie
- Utiliser icônes flaticon/fontawesome
- Style cohérent (flat design, une couleur)
- Taille uniforme (64×64 ou 128×128 px)

### Graphiques
- Utiliser graphiques PowerPoint natifs
- Couleurs de la palette
- Légendes claires
- Pas de 3D (préférer flat)

### Images
- Haute résolution (min 1920×1080)
- Cohérence style (photos ou illustrations)
- Compression optimisée (< 500 KB/image)

---

## ⏱️ Temps estimé

| Méthode | Temps | Qualité | Personnalisation |
|---------|-------|---------|------------------|
| **Automatique (Marp/Pandoc)** | 5 min | ⭐⭐⭐ | Faible |
| **Semi-automatique + retouches** | 30 min | ⭐⭐⭐⭐ | Moyenne |
| **Manuelle complète** | 2-4h | ⭐⭐⭐⭐⭐ | Totale |

---

## ✅ Checklist de validation

Avant de finaliser la présentation :

### Contenu
- [ ] Toutes les sections importantes sont présentes
- [ ] Aucune faute d'orthographe
- [ ] Chiffres et statistiques corrects
- [ ] Tableaux lisibles (pas trop denses)
- [ ] Code/JSON bien formatés

### Design
- [ ] Palette de couleurs cohérente
- [ ] Polices uniformes
- [ ] Alignements propres
- [ ] Espacement régulier
- [ ] Images haute qualité

### Navigation
- [ ] Numérotation des slides
- [ ] Sommaire en slide 2 (optionnel)
- [ ] Transitions subtiles
- [ ] Pas d'animations distrayantes

### Export
- [ ] Version PPTX enregistrée
- [ ] Version PDF exportée
- [ ] Taille fichier raisonnable (< 10 MB)
- [ ] Test d'ouverture sur autre PC

---

## 📦 Fichiers livrables

Après conversion, vous devriez avoir :
```
APRU40/
├── FICHE_PRODUIT_APRU40.md          ← Source Markdown
├── GUIDE_CONVERSION_PPT.md          ← Ce guide
├── APRU40_Presentation.pptx         ← PowerPoint (méthode manuelle)
├── APRU40_Presentation_AUTO.pptx    ← PowerPoint (automatique)
└── APRU40_Presentation.pdf          ← PDF pour partage
```

---

## 🚀 Prochaines étapes

1. **Choisir méthode** : Automatique (rapide) ou Manuelle (qualité)
2. **Créer présentation** : Suivre guide ci-dessus
3. **Valider contenu** : Checklist validation
4. **Tester présentation** : Projection sur grand écran
5. **Partager** : Distribuer PDF ou PPTX

---

## 💡 Astuces supplémentaires

### Pour présentation orale
- Limiter texte par slide (règle 6×6 : 6 lignes, 6 mots/ligne)
- Utiliser images/graphiques plutôt que texte
- Préparer notes du présentateur
- Chronométrer (~2 min/slide = 40 min total)

### Pour document de référence
- Plus de texte acceptable
- Tableaux détaillés OK
- Notes de bas de page
- Index/sommaire cliquable

### Pour pitch commercial
- Focus sur avantages (slides 17, 20, 21)
- Graphiques impactants (slide 18 coûts)
- Démonstration vidéo si possible
- Call to action clair (slide 21)

---

## 📞 Support

Pour questions sur la conversion :
- Consulter documentation Marp : https://marp.app/
- Consulter documentation Pandoc : https://pandoc.org/
- Tutoriels PowerPoint : YouTube "PowerPoint tutorial"

---

*Guide créé le 2026-02-03*  
*Compatible avec Microsoft PowerPoint 2016+, LibreOffice Impress, Google Slides*
