# Journal des modifications — Artifact Remover VST3

**Comparaison :** état actuel vs. dernier commit GitHub (`0ba655b — Some cleanup`)  
**Date :** 10 juin 2026

---

## Vue d'ensemble

La version GitHub contenait un **défaut d'architecture critique** : la SVD (Singular Value Decomposition) — une opération qui prend 10 à 50 ms — s'exécutait directement dans le thread audio temps-réel (`processBlock`). Cela provoquait des coupures audio systématiques, des xruns, et des plantages potentiels.

La version actuelle corrige ce défaut en déplaçant la SVD dans un thread background, et ajoute deux corrections supplémentaires à l'étage d'upsampling.

---

## 1. SVD dans le thread audio (bug critique)

### Ce qui ne fonctionnait pas

Dans la version GitHub, `processBlock` appelait directement `remover.remove_artifact()` :

```cpp
// Version GitHub — processBlock (RT thread)
if (static_cast<int>(downsampledBuffer.size()) >= windowSize)
{
    // ...construit le vecteur signal...

    RemovalResult result = remover.remove_artifact(   // ← 10 à 50 ms ICI
        signal, hankelSize, 1, PROCESSING_SAMPLE_RATE,
        lowerFreq, upperFreq, factor, svdThreshold
    );

    // ...upsample et écrit en sortie...
}
```

`processBlock` est appelé par le DAW environ toutes les **5 ms** (256 samples à 48 kHz). Placer une opération de 10–50 ms dans cette fonction, c'est dépasser systématiquement le budget temporel alloué au thread RT, ce qui provoque :

- **Xruns** (coupures audio) signalés par le DAW à chaque fenêtre traitée
- **Priority inversion** : le scheduler OS peut préempter le thread RT en attente de ressources, rendant le comportement imprévisible
- **Plantages potentiels** : certains DAWs tuent le thread RT s'il dépasse son délai alloué

Le plugin "démarrait sans planter" uniquement parce que le DAW tolérait les dépassements sans fermer la session immédiatement — mais le signal de sortie était inexploitable en pratique.

### Ce qui a changé

La SVD est maintenant déportée dans un **thread background dédié**. `processBlock` ne fait que gérer les ring buffers et la communication inter-threads (opérations O(1), sans allocation mémoire).

**Architecture nouvelle :**

```
Thread RT (processBlock, ~5 ms)          Thread Background (SVD, ~10-50 ms)
─────────────────────────────────        ──────────────────────────────────
① Downsample input → downsampledRing     ④ Attend sur condition_variable
② Si buffer prêt → copie snapshot
   dans pendingInput (try_lock)          ⑤ Reçoit signal → copie pendingInput
   → signal bgCv                            → exécute remove_artifact()
③ Si résultat prêt → copie pendingOutput ⑥ Écrit résultat dans pendingOutput
   dans outputRing (try_lock)               → newOutputReady = true
   → upsample → écriture sortie
```

Les accès partagés (`pendingInput`, `pendingOutput`) sont protégés par `std::mutex bgMutex`. Les flags de statut (`newInputReady`, `newOutputReady`, `shouldStop`) sont `std::atomic<bool>` pour permettre une lecture sans lock depuis le thread RT.

---

## 2. Allocations mémoire dans le thread RT (bug de latence)

### Ce qui ne fonctionnait pas

La version GitHub utilisait `std::deque<double>` pour les buffers d'entrée et de sortie :

```cpp
// Version GitHub — header
std::deque<double> downsampledBuffer;
std::deque<float>  outputQueue;        // déclaré mais jamais utilisé

// Version GitHub — processBlock
downsampledBuffer.push_back(...);   // ← allocation heap possible
while (downsampledBuffer.size() > maxBufferSize)
    downsampledBuffer.pop_front();  // ← désallocation heap possible
```

`std::deque::push_back` et `pop_front` peuvent appeler `malloc`/`free` pour gérer leur stockage interne par blocs. Les allocations heap dans un thread RT sont **interdites** dans les applications audio professionnelles pour deux raisons :

1. **Latence imprévisible** : `malloc` peut bloquer si l'allocateur OS doit demander de nouvelles pages mémoire
2. **Priority inversion** : si l'allocateur utilise un mutex interne tenu par un thread de priorité basse, le thread RT se retrouve bloqué

En pratique, sur un système chargé, cela produisait des glitches audio aléatoires même si la SVD avait été déplacée.

### Ce qui a changé

Les `std::deque` sont remplacés par des **ring buffers à taille fixe** (`std::vector<double>` pré-alloués dans `prepareToPlay`). `processBlock` n'effectue que des lectures/écritures à des positions indexées — zéro allocation.

```cpp
// Version actuelle — header
std::vector<double> downsampledRing;   // alloué une fois dans prepareToPlay
std::vector<double> outputRing;        // alloué une fois dans prepareToPlay

// Version actuelle — prepareToPlay (hors RT)
downsampledRing.assign(MAX_DOWNSAMPLED_BUFFER, 0.0);
outputRing.assign(MAX_DOWNSAMPLED_BUFFER, 0.0);

// Version actuelle — processBlock (RT)
downsampledRing[ringWritePos] = sample;            // écriture indexée, sans allocation
ringWritePos = (ringWritePos + 1) % MAX_DOWNSAMPLED_BUFFER;
```

---

## 3. Latence non déclarée au DAW (bug d'alignement)

### Ce qui ne fonctionnait pas

La version GitHub ne déclarait jamais la latence de traitement au DAW :

```cpp
// Version GitHub — prepareToPlay
// setLatencySamples() absent
```

Le plugin introduit une latence d'environ **75 ms** (fenêtre de 500 samples @ 6 kHz × facteur 8 = 4 000 samples @ 48 kHz). Sans déclaration, Ardour ne compensait pas ce délai. Conséquences :

- Lors d'une comparaison A/B (piste brute vs. piste traitée), les deux signaux étaient **décalés de 75 ms l'un par rapport à l'autre**
- Toute analyse spectrale comparative donnait des résultats incorrects (interférence de phase entre les deux pistes)
- En contexte de production, le signal traité était en avance sur le reste du mix

### Ce qui a changé

```cpp
// Version actuelle — prepareToPlay
latencySamples = currentWindowSize * DOWNSAMPLE_FACTOR;
setLatencySamples(latencySamples);
```

Ardour (et tout DAW VST3 conforme) reçoit maintenant la latence réelle et décale automatiquement la sortie traitée pour la réaligner avec les autres pistes.

---

## 4. Absence de nettoyage du thread background dans le destructeur (crash potentiel)

### Ce qui ne fonctionnait pas

La version GitHub avait un destructeur vide :

```cpp
// Version GitHub — destructeur
ArtifactRemoverAudioProcessor::~ArtifactRemoverAudioProcessor()
{
    DebugLogger::getInstance().log("ArtifactRemoverAudioProcessor destructor - cleaning up");
    // ← rien d'autre
}
```

Comme la version GitHub n'avait pas de thread background, ce n'était pas un problème en soi. Mais dans la version actuelle (avec thread background), un destructeur sans `stopBackgroundThread()` laisserait le thread BG en vie après la destruction de l'objet. Le thread accèderait alors à de la mémoire libérée → **undefined behavior**, crash quasi-garanti au déchargement du plugin.

### Ce qui a changé

```cpp
// Version actuelle — destructeur
ArtifactRemoverAudioProcessor::~ArtifactRemoverAudioProcessor()
{
    stopBackgroundThread();  // ← shouldStop = true, notify, join
}

void ArtifactRemoverAudioProcessor::stopBackgroundThread()
{
    shouldStop = true;
    bgCv.notify_all();       // réveille le thread s'il attend sur condition_variable
    if (bgThread.joinable())
        bgThread.join();     // attend la fin propre du thread avant de continuer
}
```

L'ordre est important : `notify_all` avant `join`, sinon le thread peut rester bloqué sur `bgCv.wait()` indéfiniment.

---

## 5. Lecture atomique incorrecte des paramètres

### Ce qui ne fonctionnait pas

La version GitHub lisait la valeur d'un paramètre avec `*param` (opérateur de déréférencement) :

```cpp
// Version GitHub
auto* param = apvts.getRawParameterValue(name);
return *param;   // ← déréférencement d'un std::atomic<float>
```

`getRawParameterValue` retourne un pointeur vers un `std::atomic<float>`. L'opérateur `*` sur un atomique est valide mais utilise `memory_order_seq_cst` par défaut, ce qui est plus coûteux que nécessaire. Plus important : le code était entouré d'un `try/catch` pour une opération qui ne peut pas lever d'exception, ce qui masquait des problèmes réels potentiels.

Dans la version GitHub, les paramètres étaient aussi lus **directement dans `processBlock`** à chaque bloc — une lecture atomique depuis le thread RT à chaque appel.

### Ce qui a changé

Deux améliorations :

**a) Lecture atomique explicite :**
```cpp
// Version actuelle
return param->load();   // load() explicite, plus lisible et idiomatique
```

**b) Paramètres miroirs atomiques pour le thread BG :**

Les paramètres sont copiés dans des atomiques dédiés dans `processBlock`, puis lus par le thread background depuis ces miroirs — évitant toute contention sur `apvts` :

```cpp
// Écriture (thread RT, processBlock)
bgWindowSize  .store(getParameterIntSafe("window_size", 500));
bgLowerFreq   .store(getParameterSafe("lower_freq",    10.0f));
// ...

// Lecture (thread BG, backgroundThreadFunc)
const int   windowSize = bgWindowSize.load();
const float lowerFreq  = bgLowerFreq.load();
// ...
```

---

## 6. Images spectrales d'upsampling (artefact HF)

### Ce qui ne fonctionnait pas

L'upsampling par interpolation linéaire (×8, de 6 kHz à 48 kHz) ne disposait pas de filtre de reconstruction. L'interpolation linéaire est mathématiquement équivalente à :
1. Insertion de zéros entre chaque échantillon (zero-stuffing)
2. Convolution avec une fenêtre triangulaire

Dans le domaine fréquentiel, une fenêtre triangulaire a une réponse en `sinc²`. Sa décroissance est insuffisante pour supprimer les **images spectrales** — copies du spectre utile apparaissant à des fréquences multiples de 6 kHz (6 kHz ± f, 12 kHz ± f, 18 kHz ± f...).

**Observation dans Ardour :** la courbe spectrale de la piste traitée dépassait l'originale au-dessus de 1 kHz, avec une forte énergie parasite entre 1,5–4 kHz et 8–16 kHz.

### Ce qui a changé

Un **filtre Butterworth passe-bas du 2ème ordre** à 3 kHz (Nyquist du signal traité à 6 kHz) est appliqué après chaque upsampling :

```cpp
// PluginProcessor.h — structure Biquad ajoutée (directement dans le header)
struct Biquad {
    double b0, b1, b2, a1, a2;
    double z1 = 0, z2 = 0;   // état interne : persiste entre les blocs

    double processSample(double x) {
        double y = b0*x + z1;
        z1 = b1*x - a1*y + z2;
        z2 = b2*x - a2*y;
        return y;
    }
    static Biquad makeLowPass(double sampleRate, double frequency);
};

// PluginProcessor.h — membre ajouté
Biquad reconstructionFilter;

// prepareToPlay — initialisation
reconstructionFilter = Biquad::makeLowPass(inSampleRate, PROCESSING_SAMPLE_RATE / 2.0);
reconstructionFilter.reset();

// processBlock — application après upsample
std::vector<double> upsampled = linearUpsample(toUpsample, DOWNSAMPLE_FACTOR);
for (auto& s : upsampled)
    s = reconstructionFilter.processSample(s);
```

Le filtre est un **membre persistant** : ses variables d'état `z1` et `z2` sont conservées entre les blocs. Si elles étaient remises à zéro à chaque bloc, chaque bloc commencerait par un transitoire de filtre qui se manifesterait comme un clic à chaque buffer.

Le Q = 1/√2 (≈ 0,707) correspond au profil Butterworth : réponse maximalement plate dans la bande passante, sans oscillation.

---

## 7. Zero-order hold dans le ring buffer de sortie (artefact comb)

### Ce qui ne fonctionnait pas

Ce bug est apparu dans la **réécriture de l'architecture** (version post-GitHub, avant notre correction). Il n'existait pas dans la version GitHub originale car celle-ci n'avait pas de ring buffer de sortie.

Dans la version avec thread background, la lecture du ring buffer utilisait systématiquement la position relative `writePos - N` :

```cpp
// Bug introduit lors de la réécriture
const int readStart = (outputRingWritePos - samplesNeeded
                       + MAX_DOWNSAMPLED_BUFFER) % MAX_DOWNSAMPLED_BUFFER;
for (int i = 0; i < samplesNeeded; ++i)
    toUpsample[i] = outputRing[(readStart + i) % MAX_DOWNSAMPLED_BUFFER];
```

`outputRingWritePos` n'avance que lorsque le thread background dépose un résultat — toutes les ~83 ms. Entre deux dépôts, `processBlock` s'exécute ~16 fois avec `writePos` inchangé, lisant donc **les mêmes 32 échantillons** à chaque bloc.

Répéter périodiquement une fenêtre de 32 samples @ 6 kHz crée un signal périodique de fréquence :

```
f₀ = 6 000 Hz / 32 = 187,5 Hz
```

Les harmoniques s'échelonnaient à 187, 375, 562, 750 Hz... — un peigne spectral visible dans Ardour dès 600 Hz, s'étendant jusqu'à la fréquence de Nyquist.

### Ce qui a changé

Ajout d'un curseur de lecture **indépendant** `outputRingReadPos` qui avance de façon continue à chaque bloc :

```cpp
// PluginProcessor.h
int outputRingWritePos = 0;
int outputRingReadPos  = 0;   // ← ajouté

// prepareToPlay
outputRingWritePos = 0;
outputRingReadPos  = 0;       // ← reset à chaque session

// processBlock — lecture séquentielle
std::vector<double> toUpsample(samplesNeeded, 0.0);
int avail = (outputRingWritePos - outputRingReadPos
             + MAX_DOWNSAMPLED_BUFFER) % MAX_DOWNSAMPLED_BUFFER;
for (int i = 0; i < samplesNeeded; ++i)
{
    if (avail > 0) {
        toUpsample[i]     = outputRing[outputRingReadPos];
        outputRingReadPos = (outputRingReadPos + 1) % MAX_DOWNSAMPLED_BUFFER;
        --avail;
    }
    // sinon : 0.0 (underrun — silence pendant la latence initiale)
}
```

Le silence en cas d'underrun est correct : Ardour compense la latence déclarée et aligne automatiquement le signal traité.

---

## 8. Chemin codé en dur dans le test CLI

### Ce qui ne fonctionnait pas

`demo/JuceAudioProcessingTest.cpp` avait les chemins d'entrée et de sortie codés en dur pointant vers la machine de développement d'origine :

```cpp
// Version GitHub
std::ifstream inFile("C:\\Users\\neuromobility_lab\\Documents\\amedeo\\dev\\...");
std::ofstream outFile("C:\\Users\\neuromobility_lab\\Documents\\amedeo\\dev\\...");
```

L'exécutable échouait immédiatement sur toute autre machine.

### Ce qui a changé

```cpp
// Version actuelle
std::string inputPath = (argc > 1) ? argv[1] : "test_48khz.txt";
std::ifstream inFile(inputPath);
// sortie dérivée automatiquement : test_48khz.txt → test_48khz_processed.txt
```

Usage : `.\build\Release\audio_test.exe [chemin_optionnel.txt]`

---

## 9. Scripts Python de test et validation (nouveaux fichiers)

### Problème découvert

Le premier test utilisait deux sinusoïdes pures (50 Hz + 400 Hz) comme signal de validation. L'algorithme rejetait **les deux**, y compris le signal utile à 50 Hz.

Raison : l'algorithme combine deux critères de rejet en **OU logique** :
1. Fréquence dominante hors bande → rejeté
2. Ratio énergie pic / énergie locale > seuil → rejeté

Une sinusoïde pure a un spectre maximalement concentré — son ratio énergie est toujours maximal. Le critère (2) la rejette même si sa fréquence est dans la bande [10–300 Hz]. Le plugin est conçu pour un signal large bande (EMG) dont les composantes SVD ont une énergie distribuée, pas pour des sinusoïdes pures.

### `generate_test_signal.py` (nouveau fichier)

Génère un signal adapté :
- **Signal** : bruit filtré 10–300 Hz (simule EMG) → composantes SVD à énergie distribuée → conservées
- **Artefact** : sinusoïde pure à 400 Hz → hors bande et énergie concentrée → rejetée

```
signal mixte = bruit_filtré(10–300 Hz, amp≈0.5) + sin(400 Hz, amp=0.3)
```

### `verify_test.py` (nouveau fichier)

Vérifie le résultat par **puissance de bande** (et non par pic de fréquence, inadapté aux signaux larges bandes) :

| Bande | Critère |
|-------|---------|
| 10–300 Hz | Puissance conservée > 50 % |
| 380–420 Hz | Puissance résiduelle < 20 % |

Les 3 600 premiers échantillons (latence) sont ignorés avant la mesure.

---

## Récapitulatif — Comparaison avant/après

| Aspect | Version GitHub (`0ba655b`) | Version actuelle |
|--------|---------------------------|-----------------|
| SVD dans `processBlock` | **Oui — sur le thread RT** | Non — thread background |
| Allocations heap dans RT | **Oui — `std::deque`** | Non — ring buffers pré-alloués |
| Latence déclarée au DAW | **Non** | Oui — `setLatencySamples()` |
| Nettoyage à la destruction | **Non (pas de thread BG)** | Oui — `stopBackgroundThread()` |
| Lecture atomique paramètres | `*param` (déréférencement) | `param->load()` + miroirs atomiques |
| Filtre de reconstruction | **Absent** | Biquad Butterworth LP @ 3 kHz |
| Ring buffer sortie (curseur) | N/A (pas de ring buffer) | `outputRingReadPos` indépendant |
| Chemin test CLI | **Codé en dur** | Argument CLI |
| Signal de test | Sinusoïdes pures (inadapté) | Bruit large bande + ton pur |

### Ordre de sévérité des bugs

| Priorité | Bug | Impact |
|----------|-----|--------|
| 🔴 Critique | SVD dans le thread RT | Coupures audio constantes, xruns, crash possible |
| 🔴 Critique | Allocations heap dans RT | Glitches aléatoires, instabilité |
| 🟠 Majeur | Latence non déclarée | Décalage temporel de ~75 ms, analyses incorrectes |
| 🟠 Majeur | Pas de nettoyage du destructeur | Crash au déchargement du plugin |
| 🟡 Modéré | Zero-order hold ring buffer | Peigne harmonique dans tout le spectre traité |
| 🟡 Modéré | Absence de filtre de reconstruction | Images spectrales > 1 kHz dans la sortie |
| 🟢 Mineur | Chemin codé en dur | Test CLI inutilisable hors machine d'origine |
