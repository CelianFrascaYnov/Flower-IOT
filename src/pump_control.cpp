#include <Arduino.h>
#include <WiFi.h>
#include <mqtt_client.h>

// Définition des broches
#define PIN_RELAY D5  // GPIO pour le relais (connecté à la pompe)
#define LED_PIN D9     // LED d’indication (optionnel)
#define WATER_SENSOR_PIN A0   // Capteur de niveau d'eau

// Étalonnage du capteur de niveau d'eau
#define DRY_VALUE  100  // Valeur quand le capteur est sec
#define WET_VALUE 80  // Valeur quand le capteur est totalement immergé

// WiFi
#define WIFI_SSID "CMF"
#define WIFI_PASSWORD "gggggggg"

// MQTT Configuration
#define MQTT_BROKER "mqtt://192.168.143.123:1883"
#define MQTT_TOPIC_POMPE "pompe/commande"
#define MQTT_TOPIC_WATER_LEVEL "capteur/eau"
#define MQTT_TOPIC_POMPE_ETAT "pompe/etat"

bool pompeActive = false;
bool blocagePompe = false; // Bloque l'activation de la pompe si le niveau d'eau est trop bas

esp_mqtt_client_handle_t client;

// Déclarations des fonctions
void connectToWiFi();
void connectToMQTT();
bool isWiFiConnected();
bool isMQTTConnected();
void reconnectWiFi();
void reconnectMQTT();
void messageReceived(esp_mqtt_event_handle_t event);

// Connexion au WiFi avec affichage de l'IP
void connectToWiFi() {
    Serial.print("Connexion au WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // 10 secondes max (20 x 500ms)
        Serial.print(".");
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connecté !");
        Serial.print("📡 Adresse IP de l'ESP32 : ");
        Serial.println(WiFi.localIP());  // Affichage de l'adresse IP
    } else {
        Serial.println("\n❌ Échec de la connexion WiFi !");
    }
}

// Vérifier si le WiFi est connecté
bool isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Vérifier si MQTT est connecté
bool isMQTTConnected() {
    return client != nullptr;
}

// Reconnexion au WiFi si nécessaire
void reconnectWiFi() {
    if (!isWiFiConnected()) {
        Serial.println("⚠️ WiFi déconnecté, tentative de reconnexion...");
        connectToWiFi();
    }
}

// Reconnexion au MQTT si nécessaire
void reconnectMQTT() {
    if (!isMQTTConnected()) {
        Serial.println("⚠️ Client MQTT non connecté, tentative de reconnexion...");
        connectToMQTT();
    }
}

// Envoi des données aux topics MQTT avec logs
void sendMQTTData(const char* topic, float value) {
    if (!isMQTTConnected()) {
        Serial.println("🚫 Données non envoyées : MQTT indisponible.");
        reconnectMQTT();
        return;
    }

    char payload[20];
    snprintf(payload, sizeof(payload), "%.2f", value);
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 0, 0);

    if (msg_id != -1) {
        Serial.printf("📤 Données envoyées : %s -> %s\n", topic, payload);
    } else {
        Serial.printf("⚠️ Échec de l'envoi MQTT pour %s\n", topic);
    }
}

// Fonction pour envoyer un message MQTT en texte ("ON" ou "OFF")
void sendMQTTMessage(const char* topic, const char* message) {
    if (!isMQTTConnected()) {
        Serial.println("🚫 Message non envoyé : MQTT indisponible.");
        reconnectMQTT();
        return;
    }

    int msg_id = esp_mqtt_client_publish(client, topic, message, 0, 0, 0);

    if (msg_id != -1) {
        Serial.printf("📤 Message envoyé : %s -> %s\n", topic, message);
    } else {
        Serial.printf("⚠️ Échec de l'envoi MQTT pour %s\n", topic);
    }
}

// Callback de réception des messages MQTT
void messageReceived(esp_mqtt_event_handle_t event) {
    String topic = String(event->topic, event->topic_len);
    String payload = String(event->data, event->data_len);

    Serial.printf("📥 Message reçu sur %s : %s\n", topic.c_str(), payload.c_str());

    if (topic == MQTT_TOPIC_POMPE) {
        if (payload == "ON") {
            if (!blocagePompe) {  // Vérifie si l'eau est suffisante avant d'activer la pompe
                digitalWrite(PIN_RELAY, HIGH);
                pompeActive = true;
                sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "ON");  // Publier l'état de la pompe
                Serial.println("💧 Pompe activée !");
            } else {
                Serial.println("⚠️ Activation refusée : Niveau d'eau trop bas !");
                sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "ALERTE: Niveau d'eau bas !");
            }
        } else if (payload == "OFF") {
            digitalWrite(PIN_RELAY, LOW);
            pompeActive = false;
            sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "OFF");  // Publier l'état de la pompe
            Serial.println("🚫 Pompe désactivée !");
        }
    }
}

// Gestion des événements MQTT
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            Serial.println("✅ Connecté au broker MQTT.");
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_POMPE, 0);
            break;

        case MQTT_EVENT_DATA:
            messageReceived(event);
            break;

        case MQTT_EVENT_DISCONNECTED:
            Serial.println("⚠️ Déconnecté du MQTT, tentative de reconnexion...");
            reconnectMQTT();
            break;

        default:
            break;
    }
}

// Connexion MQTT avec logs (uniquement si le WiFi est actif)
void connectToMQTT() {
    if (!isWiFiConnected()) {
        Serial.println("🚫 Connexion MQTT annulée : WiFi non disponible.");
        return;
    }

    Serial.println("🔄 Connexion au broker MQTT...");
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.uri = MQTT_BROKER;

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);  // Pompe éteinte au démarrage
    pinMode(WATER_SENSOR_PIN, INPUT);

    connectToWiFi();
    connectToMQTT();
}

void loop() {
    reconnectWiFi();
    reconnectMQTT();

    int rawValue = analogRead(WATER_SENSOR_PIN); // Lecture brute du capteur

    Serial.print("Valeur brute du capteur : ");
    Serial.println(rawValue);

    // Vérification du niveau d'eau et envoi de l'état
    if (rawValue < DRY_VALUE) {
        sendMQTTData(MQTT_TOPIC_WATER_LEVEL, 0.0);

        // Si l'eau est trop basse, désactiver la pompe et bloquer son activation
        if (pompeActive) {
            Serial.println("⚠️ Niveau d'eau trop bas ! Pompe arrêtée.");
            digitalWrite(PIN_RELAY, LOW);
            pompeActive = false;
            blocagePompe = true;  // Bloquer l'activation de la pompe tant que l'eau est insuffisante
            sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "ALERTE: Niveau d'eau bas !");
        }
    }
    else if (rawValue > WET_VALUE) {
        sendMQTTData(MQTT_TOPIC_WATER_LEVEL, 100.0);

        // Si l'eau est revenue à un niveau normal, autoriser l'activation de la pompe
        if (blocagePompe) {
            Serial.println("✅ Niveau d'eau suffisant, pompe réactivable.");
            blocagePompe = false;
        }
    }
    else {
        sendMQTTData(MQTT_TOPIC_WATER_LEVEL, 50.0);
    }

    delay(1000);
}
