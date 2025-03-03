#include <Arduino.h>
#include <WiFi.h>
#include <mqtt_client.h>

// Définition des broches
#define RELAY_PIN D5  // GPIO pour le relais (connecté à la pompe)
#define LED_PIN D9     // LED d’indication (optionnel)

// WiFi
#define WIFI_SSID "WifiCadeau"
#define WIFI_PASSWORD "CadeauWifi"

// MQTT Configuration
#define MQTT_BROKER "mqtt://192.168.121.123:1883"
#define MQTT_TOPIC_POMPE "pompe/commande"

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

// Callback quand un message est reçu
void messageReceived(esp_mqtt_event_handle_t event) {
    String topic = String(event->topic, event->topic_len);
    String payload = String(event->data, event->data_len);

    Serial.printf("📥 Message reçu sur %s : %s\n", topic.c_str(), payload.c_str());

    if (topic == MQTT_TOPIC_POMPE) {
        if (payload == "ON") {
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("💧 Pompe activée !");
        } else if (payload == "OFF") {
            digitalWrite(RELAY_PIN, LOW);
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
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // Pompe éteinte au démarrage

    connectToWiFi();
    connectToMQTT();
}

void loop() {
    reconnectWiFi();
    reconnectMQTT();
    delay(1000);
}
