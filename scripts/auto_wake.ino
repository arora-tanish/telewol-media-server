#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// --- CONFIGURATION ---
const char* ssid     = "wifi_ssid";
const char* password = "wifi_pass";

#define BOTtoken "bot_token"

// --- STATIC IP CONFIGURATION ---
IPAddress local_IP(192, 168, 1, 102);  //do this config if you want to assign static ip
IPAddress gateway(192, 168, 1, 1);     
IPAddress subnet(255, 255, 255, 0);    
IPAddress primaryDNS(1, 1, 1, 1);      
IPAddress secondaryDNS(8, 8, 8, 8);    

// --- AUTHORIZED USERS ---
const String AUTHORIZED_CHATS[] = {"123456789", "987654321"}; // should be Updated with your actual Chat IDs
const int AUTHORIZED_COUNT = sizeof(AUTHORIZED_CHATS) / sizeof(AUTHORIZED_CHATS[0]);

// Server's target physical Ethernet MAC address
byte targetMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // should be updated with the mac address of the system

// --- Core Initializations ---
WiFiClientSecure client;
WiFiUDP udp;

unsigned long lastTimeBotRan;
const unsigned long botRequestDelay = 2000; 
long last_update_id = 0; // Tracks messages cleanly without library bugs

bool isAuthorized(String chat_id) {
    for (int i = 0; i < AUTHORIZED_COUNT; i++) {
        if (chat_id == AUTHORIZED_CHATS[i]) return true;
    }
    return false;
}

void sendWakeOnLan() {
    byte magicPacket[102];
    for (int i = 0; i < 6; i++) magicPacket[i] = 0xFF;
    for (int i = 1; i <= 16; i++) {
        for (int j = 0; j < 6; j++) {
            magicPacket[i * 6 + j] = targetMac[j];
        }
    }
    udp.beginPacket(IPAddress(192, 168, 1, 255), 9); 
    udp.write(magicPacket, sizeof(magicPacket));
    udp.endPacket();
    // Serial.println("🚀 Magic Packet Broadcasted!");
}
// Custom function to send Telegram messages back securely with connection safety
void sendTelegramMessage(String chat_id, String text) {
    client.setTimeout(5000); // 5-second max wait time
    if (!client.connect("api.telegram.org", 443)) {
        // Serial.println("❌ Failed to connect to Telegram to send response.");
        return;
    }
    String url = "/bot" + String(BOTtoken) + "/sendMessage?chat_id=" + chat_id + "&text=" + text;
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Connection: close\r\n\r\n");
    
    // Give it a brief moment to transmit before cutting the wire
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 3000) {
            break;
        }
    }
    client.stop();
}

void checkTelegram() {
    client.setTimeout(5000); 
    
    if (!client.connect("api.telegram.org", 443)) {
        // Serial.println("❌ SSL Connection to Telegram Failed or Timed Out!");
        client.stop();
        return;
    }

    String url = "/bot" + String(BOTtoken) + "/getUpdates?offset=" + String(last_update_id + 1) + "&limit=1";
    
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            // Serial.println("❌ Server took too long to send data response.");
            client.stop();
            return;
        }
    }

    String rawResponse = "";
    while (client.available()) {
        char c = client.read();
        rawResponse += c;
    }
    client.stop();

    int jsonStart = rawResponse.indexOf("{");
    int jsonEnd = rawResponse.lastIndexOf("}");
    
    if (jsonStart == -1 || jsonEnd == -1 || jsonEnd < jsonStart) {
        return;
    }
    
    String cleanJson = rawResponse.substring(jsonStart, jsonEnd + 1);

    if (cleanJson.indexOf("\"ok\":true") == -1) {
        return;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, cleanJson);
    if (error) {
        // Serial.print("❌ JSON Parsing Error Details: ");
        // Serial.println(error.c_str());
        return;
    }

    JsonArray result = doc["result"];
    if (result.size() == 0) {
        // Serial.println("Done. Result code: 0 (No new messages)");
        return; 
    }

    JsonObject updateObj = result[0];
    last_update_id = updateObj["update_id"].as<long>();

    if (!updateObj.containsKey("message")) {
        return;
    }

    String chat_id = updateObj["message"]["chat"]["id"].as<String>();
    String text = updateObj["message"]["text"].as<String>();

    // Serial.println("\n✅ Done. Result code: " + String(result.size()) + " (Message Found!)");
    // Serial.println("📨 Message content: [" + text + "] from Chat ID: " + chat_id);

    // 🔒 Security Verification
    if (!isAuthorized(chat_id)) {
        sendTelegramMessage(chat_id, "Unauthorized user.");
        return;
    }

    // 💬 FIXED RESPONSE ARRAY LOGIC
    if (text == "/wake") {
        sendWakeOnLan();
        // Url-encode the space characters (%20) so the raw HTTP GET request doesn't break
        sendTelegramMessage(chat_id, "🚀%20Magic%20packet%20broadcasted%20to%20network%20arrays!");
    } 
    else if (text == "/start") {
        sendTelegramMessage(chat_id, "🤖%20Wake%20Gateway%20Online.%20Use%20/wake%20to%20power%20up.");
    }
}

void setup() {
    // Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        // Serial.println("❌ STA Configuration Failed.");
    }
    
    WiFi.begin(ssid, password);
    
    // Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        // Serial.print(".");
    }
    // Serial.println("\n✅ Wi-Fi Connected!");
    // Serial.print("📍 Fixed IP Address: ");
    // Serial.println(WiFi.localIP());
    
    // Set up secure SSL fingerprints bypass rule globally
    client.setInsecure(); 
    udp.begin(9);
    
    // Serial.println("🤖 Custom Wake Gateway Booted successfully. Listening for commands...");
}

void loop() {
    if (millis() > lastTimeBotRan + botRequestDelay) {
        // Serial.print("🌐 Querying api.telegram.org endpoints... ");
        checkTelegram();
        lastTimeBotRan = millis();
    }
}