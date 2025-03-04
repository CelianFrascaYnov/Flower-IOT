# 🌱 Flower IOT - Système de Surveillance et d’Arrosage Automatisé

## 📘 Introduction
**Flower IOT** est un projet de surveillance et de gestion intelligente de l'environnement des plantes basé sur l’**ESP32-S3** et utilisant **Node-RED** pour une interface de visualisation en temps réel.

Ce système permet de **monitorer** et **automatiser** l’arrosage des plantes en fonction des données environnementales captées (humidité du sol, luminosité, température et humidité de l’air). Toutes les informations sont transmises via **MQTT** vers **Node-RED**, où un tableau de bord permet de suivre l’état des plantes et de déclencher la pompe d’arrosage en fonction des besoins.

---

## 🔧 Matériel Utilisé

### 🟢 ESP32-S3 (x2)
1. **ESP32-S3 #1** : Gestion de l’arrosage et du niveau d’eau  
   - Pompe à eau  
   - Capteur de pluie *(vérifie si le réservoir d’eau est vide ou non)*  

2. **ESP32-S3 #2** : Surveillance des conditions environnementales  
   - AHT10 *(Capteur de température et d’humidité de l’air)*  
   - Capteur d’humidité du sol  
   - Photorésistance *(Mesure de la luminosité)*  

### 🟡 Capteurs et Actionneurs

| Capteur/Actionneur | Rôle |
|--------------------|------|
| **Pompe à eau** | Arrosage automatique |
| **Capteur de pluie** | Vérifie le niveau d'eau du réservoir |
| **AHT10** | Mesure température et humidité de l'air |
| **Capteur d'humidité du sol** | Vérifie si le sol est sec ou humide |
| **Photorésistance** | Mesure l'intensité lumineuse |

### 🔵 Communication
- **Protocole MQTT** : Transmission des données entre les ESP32-S3 et **Node-RED**  
- **Node-RED** : Interface pour la gestion des capteurs et automatisation des actions  

---

## ⚙️ Architecture du Système

1. **ESP32-S3 #1** : Envoie les valeurs du **capteur de pluie** et contrôle la **pompe à eau** en fonction des seuils définis.
2. **ESP32-S3 #2** : Mesure et transmet périodiquement les valeurs des **capteurs environnementaux** (température, humidité de l’air et du sol, luminosité).
3. **Broker MQTT** (ex: **Mosquitto**) : Centralise la communication entre les ESP32-S3 et Node-RED.
4. **Node-RED** :  
   - Affiche les valeurs en **temps réel** via des jauges et des graphiques.  
   - Déclenche l’arrosage automatiquement si l’humidité du sol est trop basse et si le réservoir d’eau est rempli.  
   - Permet une **activation manuelle** de la pompe via une interface.  

---

## 🎛 Dashboard Node-RED

### 📌 Fonctionnalités
- **Visualisation des capteurs** :  
  - Jauges pour l'humidité du sol et de l'air, la température et la luminosité.  
  - Graphiques pour l’évolution des données.  
- **Contrôle de la pompe** :  
  - Activation automatique selon les seuils définis.  
  - Mode manuel pour forcer l’arrosage.  
- **Affichage du niveau d’eau** dans le réservoir grâce au capteur de pluie.  

### 📌 Automatisations
- La pompe est déclenchée **uniquement si l'humidité du sol est trop basse et si le réservoir d’eau contient encore de l’eau**.
- L’interface permet de modifier les **seuils de déclenchement** selon le type de plante.

  
