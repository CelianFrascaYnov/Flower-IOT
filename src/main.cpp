#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <mqtt_client.h>
#include <ArduinoJson.h>
#include <Adafruit_AHTX0.h>

#define SOIL_MOISTURE_PIN A0  // Capteur d'humidité du sol
#define LIGHT_SENSOR_PIN A1   // Photoresistor
#define BLUE_LED_PIN D9       // LED d'indication

// Calibration du capteur d'humidité du sol
#define DRY_VALUE 3150
#define WET_VALUE 1450

// Calibration du capteur de lumière
#define DARK_VALUE 4095
#define BRIGHT_VALUE 40

// WiFi
#define WIFI_SSID "iPhone de Quentin"
#define WIFI_PASSWORD "gggggggg"

// MQTT Configuration
#define MQTT_BROKER "mqtt://192.0.0.3:1883"
#define MQTT_TOPIC_SOL "capteur/sol"
#define MQTT_TOPIC_LUMIERE "capteur/lumiere"
#define MQTT_TOPIC_TEMPERATURE "capteur/temperature"
#define MQTT_TOPIC_HUMIDITE "capteur/humidite"

esp_mqtt_client_handle_t client;
Adafruit_AHTX0 aht;

void connectToWiFi();
void connectToMQTT();
bool isWiFiConnected();
bool isMQTTConnected();
void reconnectWiFi();
void reconnectMQTT();

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
    Serial.println("📥 Message reçu:");
    Serial.printf("📌 Topic: %.*s\n", event->topic_len, event->topic);
    Serial.printf("📨 Payload: %.*s\n", event->data_len, event->data);
}

// Gestion des événements MQTT
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            Serial.println("✅ Connecté au broker MQTT.");
            esp_mqtt_client_subscribe(client, "capteur/#", 0);
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

void setup() {
    Serial.begin(115200);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);

    digitalWrite(BLUE_LED_PIN, HIGH);  // Allumer la LED en continu

    connectToWiFi();  // Connexion au WiFi
    connectToMQTT();  // Connexion au broker MQTT (si WiFi dispo)

    // Initialisation du capteur AHT10
    if (!aht.begin()) {
        Serial.println("❌ Impossible de détecter le AHT10");
        while (1);
    }
    Serial.println("✅ AHT10 détecté avec succès !");
}

void loop() {
    // Vérifier connexion WiFi et MQTT
    reconnectWiFi();
    reconnectMQTT();

    // **Ne rien faire si le WiFi ou MQTT est déconnecté**
    if (!isWiFiConnected() || !isMQTTConnected()) {
        Serial.println("🚫 Pas de connexion stable, aucune donnée relevée ni envoyée.");
        delay(5000);
        return;
    }

    digitalWrite(BLUE_LED_PIN, LOW);  // Éteindre brièvement la LED pour indiquer la lecture
    delay(100);

    // Lecture de l'humidité du sol
    int sensorValue = analogRead(SOIL_MOISTURE_PIN);
    int moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
    moisturePercentage = constrain(moisturePercentage, 0, 100);

    // Lecture du capteur de lumière
    int lightValue = analogRead(LIGHT_SENSOR_PIN);
    int lightPercentage = map(lightValue, DARK_VALUE, BRIGHT_VALUE, 0, 100);
    lightPercentage = constrain(lightPercentage, 0, 100);

    // Lecture du AHT10 (température et humidité)
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);

    // Envoi des données MQTT
    sendMQTTData(MQTT_TOPIC_SOL, moisturePercentage);
    sendMQTTData(MQTT_TOPIC_LUMIERE, lightPercentage);
    sendMQTTData(MQTT_TOPIC_TEMPERATURE, temp.temperature);
    sendMQTTData(MQTT_TOPIC_HUMIDITE, humidity.relative_humidity);

    // Affichage des résultats
    Serial.println("📡 [NOUVELLE LECTURE]");
    Serial.printf("🌱 Sol -> Humidité : %d%%\n", moisturePercentage);
    Serial.printf("💡 Lumière -> Luminosité : %d%%\n", lightPercentage);
    Serial.printf("🌡️ Air -> Température : %.2f°C | Humidité : %.2f%%\n", temp.temperature, humidity.relative_humidity);
    Serial.println("----------------------------------");

    digitalWrite(BLUE_LED_PIN, HIGH); // Rallumer la LED après lecture

    delay(5000);  // Rafraîchissement toutes les 5 secondes
}
