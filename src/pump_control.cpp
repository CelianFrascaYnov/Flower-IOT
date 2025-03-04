#include <Arduino.h>
#include <WiFi.h>
#include <mqtt_client.h>

// Définition des broches
#define PIN_RELAY D5   // GPIO pour le relais (connecté à la pompe)
#define LED_PIN D9     // LED d’indication (optionnel)
#define WATER_SENSOR_PIN A2  // Capteur de niveau d'eau

// Étalonnage du capteur de niveau d'eau
#define DRY_VALUE  2000  // Valeur quand le capteur est sec
#define WET_VALUE  3000   // Valeur quand le capteur est totalement immergé

// WiFi
#define WIFI_SSID "CMF"
#define WIFI_PASSWORD "gggggggg"

// MQTT Configuration
#define MQTT_BROKER "mqtt://192.168.143.123:1883"
#define MQTT_TOPIC_POMPE "pompe/commande"
#define MQTT_TOPIC_WATER_LEVEL "capteur/eau"
#define MQTT_TOPIC_POMPE_ETAT "pompe/etat"

bool pompeActive = false;
bool blocagePompe = false;  // Bloque l'activation de la pompe si le niveau d'eau est trop bas

esp_mqtt_client_handle_t client;

// Déclarations des fonctions
void connectToWiFi();
void connectToMQTT();
bool isWiFiConnected();
bool isMQTTConnected();
void reconnectWiFi();
void reconnectMQTT();
void messageReceived(esp_mqtt_event_handle_t event);
void checkWaterLevel(); // Vérifie le niveau d'eau et met à jour le statut

// Connexion au WiFi
void connectToWiFi() {
    Serial.print("Connexion au WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        Serial.print(".");
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connecté !");
        Serial.print("📡 Adresse IP : ");
        Serial.println(WiFi.localIP());
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

// Reconnexion au WiFi
void reconnectWiFi() {
    if (!isWiFiConnected()) {
        Serial.println("⚠️ WiFi déconnecté, tentative de reconnexion...");
        connectToWiFi();
    }
}

// Reconnexion au MQTT
void reconnectMQTT() {
    if (!isMQTTConnected()) {
        Serial.println("⚠️ Client MQTT non connecté, tentative de reconnexion...");
        connectToMQTT();
    }
}

// Envoi des données MQTT
void sendMQTTData(const char* topic, float value) {
    if (!isMQTTConnected()) {
        Serial.println("🚫 MQTT indisponible, données non envoyées.");
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

// Envoi d'un message MQTT ("ON" ou "OFF")
void sendMQTTMessage(const char* topic, const char* message) {
    if (!isMQTTConnected()) {
        Serial.println("🚫 MQTT indisponible, message non envoyé.");
        reconnectMQTT();
        return;
    }

    esp_mqtt_client_publish(client, topic, message, 0, 0, 0);
}

// Vérification et gestion du niveau d'eau
void checkWaterLevel() {
    int rawValue = analogRead(WATER_SENSOR_PIN);
    Serial.print("Valeur du capteur : ");
    Serial.println(rawValue);

    if (rawValue < DRY_VALUE) {
        sendMQTTData(MQTT_TOPIC_WATER_LEVEL, 0.0);

        // Si la pompe est allumée et l'eau est trop basse, éteindre immédiatement
        if (pompeActive) {
            Serial.println("⚠️ Niveau d'eau trop bas ! Pompe arrêtée.");
            digitalWrite(PIN_RELAY, LOW);
            pompeActive = false;
            sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "ALERTE: Niveau d'eau bas !");
        }

        // Bloquer toute future activation
        blocagePompe = true;
    }
    else if (rawValue > WET_VALUE) {
        sendMQTTData(MQTT_TOPIC_WATER_LEVEL, 100.0);

        // Si le niveau d'eau est suffisant, lever le blocage
        if (blocagePompe) {
            Serial.println("✅ Niveau d'eau suffisant, pompe réactivable.");
            blocagePompe = false;
        }
    }
    else {
        sendMQTTData(MQTT_TOPIC_WATER_LEVEL, 50.0);
    }
}

// Callback de réception des messages MQTT
void messageReceived(esp_mqtt_event_handle_t event) {
    String topic = String(event->topic, event->topic_len);
    String payload = String(event->data, event->data_len);

    Serial.printf("📥 Message reçu sur %s : %s\n", topic.c_str(), payload.c_str());

    // Vérification du niveau d'eau avant d'activer la pompe
    checkWaterLevel();

    if (topic == MQTT_TOPIC_POMPE) {
        if (payload == "ON") {
            if (!blocagePompe) {
                digitalWrite(PIN_RELAY, HIGH);
                pompeActive = true;
                sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "ON");
                Serial.println("💧 Pompe activée !");
            } else {
                Serial.println("⚠️ Activation refusée : Niveau d'eau trop bas !");
                sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "ALERTE: Niveau d'eau bas !");
            }
        }
        else if (payload == "OFF") {
            digitalWrite(PIN_RELAY, LOW);
            pompeActive = false;
            sendMQTTMessage(MQTT_TOPIC_POMPE_ETAT, "OFF");
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

// Connexion MQTT
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
    digitalWrite(PIN_RELAY, LOW);
    pinMode(WATER_SENSOR_PIN, ANALOG);
    analogReadResolution(12);
    analogSetAttenuation(ADC_0db);

    connectToWiFi();
    connectToMQTT();
}

void loop() {
    reconnectWiFi();
    reconnectMQTT();
    checkWaterLevel();
    delay(1000);
}
