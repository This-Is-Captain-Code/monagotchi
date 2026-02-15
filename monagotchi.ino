#include <FS.h>
using namespace fs;

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// WiFi credentials - CHANGE THESE TO YOUR WIFI!
const char* ssid = "";
const char* password = "";

// ========== OPENCLAW CONFIG ==========
// Replace 192.168.1.XXX with YOUR PC's local IP (run ipconfig on your PC)
const char* openclawURL = "http://192.168.1.18:18789/hooks/agent";
const char* hookToken = "";

unsigned long lastOpenClawPush = 0;
const unsigned long openclawPushInterval = 60000;  // Push status every 60 seconds
String lastAlertedState = "normal";
// =====================================

// Web server on port 80
WebServer server(80);

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Display dimensions for Waveshare 1.69" (240x280)
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 280

// Button pin definitions (connected to GND when pressed)
#define BTN_FEED 32
#define BTN_PLAY 33
#define BTN_CLEAN 25
#define BTN_SLEEP 26
#define BTN_HEAL 27

// Pet variables
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 60000;  // Update pet stats every minute
unsigned long lastDrawTime = 0;
const unsigned long drawInterval = 1000;  // Redraw screen every 1000ms (1 second)

// Animation variables
int animFrame = 0;
unsigned long lastAnimTime = 0;
const unsigned long animInterval = 500;  // Animation frame change interval

// Pet stats (0-100)
struct {
  int hunger = 50;
  int happiness = 50;
  int health = 80;
  int energy = 100;
  int cleanliness = 80;
  int age = 0;
  bool isAlive = true;
  String state = "normal";
  unsigned long stateChangeTime = 0;
  unsigned long lastInteractionTime = 0;
} pet;

// Previous state for detecting changes
String previousState = "";

// Track previous stats to avoid unnecessary redraws
int prevHunger = -1;
int prevHappiness = -1;
int prevHealth = -1;
int prevEnergy = -1;
int prevCleanliness = -1;
int prevAge = -1;
bool prevIsAlive = true;
int prevAnimFrame = -1;

// Message display
String messageText = "";
unsigned long messageTime = 0;
const unsigned long messageDuration = 3000;

// Color definitions (RGB565 format)
#define COLOR_BG 0x0000        // Black
#define COLOR_PET 0xFEC0       // Orange/Yellow
#define COLOR_HUNGER 0xFB20    // Orange
#define COLOR_HAPPY 0xFFE0     // Yellow
#define COLOR_HEALTH 0x07E0    // Green
#define COLOR_ENERGY 0x001F    // Blue
#define COLOR_CLEAN 0x07FF     // Cyan
#define COLOR_TEXT 0xFFFF      // White
#define COLOR_BAR_BG 0x4208    // Dark gray
#define COLOR_SICK 0xA7E0      // Sickly green
#define COLOR_DEAD 0x7BEF      // Gray

// ========== OPENCLAW FUNCTIONS ==========

void sendToOpenClaw(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(openclawURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(hookToken));
  http.setTimeout(5000);

  // Escape any quotes in the message
  message.replace("\"", "'");

  String payload = "{\"message\":\"" + message + "\",\"name\":\"TamaPetchi\",\"deliver\":true,\"channel\":\"telegram\"}";

  int httpCode = http.POST(payload);
  if (httpCode == 202) {
    Serial.println("OpenClaw: agent run started");
  } else {
    Serial.printf("OpenClaw error: %d\n", httpCode);
  }
  http.end();
}

void pushPetStatus() {
  String msg = "TamaPetchi status update - ";
  msg += "Hunger:" + String(pet.hunger) + " ";
  msg += "Happy:" + String(pet.happiness) + " ";
  msg += "Health:" + String(pet.health) + " ";
  msg += "Energy:" + String(pet.energy) + " ";
  msg += "Clean:" + String(pet.cleanliness) + " ";
  msg += "Age:" + String(pet.age) + "min ";
  msg += "State:" + pet.state + " ";
  msg += "Alive:" + String(pet.isAlive ? "yes" : "no");
  sendToOpenClaw(msg);
}

// =========================================

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 TamaPetchi LCD + Web + OpenClaw Starting...");

  // Initialize TFT display FIRST
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);

  // Show connecting message
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.print("Connecting");
  tft.setCursor(30, 120);
  tft.print("to WiFi...");

  Serial.println("Display initialized");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());

    // Show IP on display
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.print("WiFi Connected!");
    tft.setCursor(10, 120);
    tft.print("IP: ");
    tft.print(WiFi.localIP());
    tft.setCursor(10, 140);
    tft.print("Open in browser");
    tft.setCursor(10, 160);
    tft.print("OpenClaw: ENABLED");
    delay(3000);
  } else {
    Serial.println("");
    Serial.println("WiFi failed - buttons only");
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 120);
    tft.print("WiFi Failed");
    tft.setCursor(10, 140);
    tft.print("Buttons only mode");
    delay(2000);
  }

  // Initialize buttons with internal pull-up resistors
  pinMode(BTN_FEED, INPUT_PULLUP);
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_CLEAN, INPUT_PULLUP);
  pinMode(BTN_SLEEP, INPUT_PULLUP);
  pinMode(BTN_HEAL, INPUT_PULLUP);

  Serial.println("Buttons initialized");

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/pet", HTTP_GET, handleGetPet);
  server.on("/feed", HTTP_POST, handleFeed);
  server.on("/play", HTTP_POST, handlePlay);
  server.on("/clean", HTTP_POST, handleClean);
  server.on("/sleep", HTTP_POST, handleSleep);
  server.on("/heal", HTTP_POST, handleHeal);
  server.on("/reset", HTTP_POST, handleResetWeb);

  // Start web server
  if (WiFi.status() == WL_CONNECTED) {
    server.begin();
    Serial.println("Web server started!");
  }

  Serial.println("Starting with fresh pet data");

  // Show welcome screen
  tft.fillScreen(COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(30, 120);
  tft.print("TamaPetchi");
  tft.setTextSize(1);
  tft.setCursor(60, 150);
  tft.print("Loading...");

  delay(2000);

  previousState = pet.state;

  // Clear screen for game
  tft.fillScreen(COLOR_BG);

  Serial.println("ESP32 TamaPetchi Ready!");
  Serial.println("LCD + Buttons + Web + OpenClaw");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Open browser to: http://");
    Serial.println(WiFi.localIP());
    Serial.print("OpenClaw webhook: ");
    Serial.println(openclawURL);
  }
  Serial.println("- Button 1 (GPIO32): Feed");
  Serial.println("- Button 2 (GPIO33): Play");
  Serial.println("- Button 3 (GPIO25): Clean");
  Serial.println("- Button 4 (GPIO26): Sleep/Wake");
  Serial.println("- Button 5 (GPIO27): Heal");

  showMessage("Hello!");

  // Send boot notification to OpenClaw
  sendToOpenClaw("TamaPetchi has booted up and is ready! Initial stats - Hunger:50 Happy:50 Health:80 Energy:100 Clean:80");
}

void loop() {
  unsigned long currentMillis = millis();

  // Handle web server requests
  server.handleClient();

  // Update pet stats every minute
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;
    updatePet();
    Serial.println("Pet stats updated");
  }

  // Update animation frame
  if (currentMillis - lastAnimTime >= animInterval) {
    lastAnimTime = currentMillis;
    animFrame = (animFrame + 1) % 4;
  }

  // Redraw screen
  if (currentMillis - lastDrawTime >= drawInterval) {
    lastDrawTime = currentMillis;
    drawScreen();
  }

  // Check button presses
  handleButtons();

  // Periodic status push to OpenClaw
  if (currentMillis - lastOpenClawPush >= openclawPushInterval) {
    lastOpenClawPush = currentMillis;
    pushPetStatus();
  }

  delay(10);
}

void updatePet() {
  previousState = pet.state;

  if (!pet.isAlive) return;

  // Handle different stat decreases based on sleep state
  if (pet.state == "sleeping") {
    pet.hunger = max(0, pet.hunger - 2);
    pet.happiness = max(0, pet.happiness - 1);
    pet.energy = min(100, pet.energy + 10);
    pet.cleanliness = max(0, pet.cleanliness - 2);

    // Auto wake-up when energy is full
    if (pet.energy >= 100) {
      pet.state = "normal";
      pet.stateChangeTime = millis();
      showMessage("Pet woke up!");
      Serial.println("Pet woke up refreshed!");
    }
  } else {
    pet.hunger = max(0, pet.hunger - 5);
    pet.happiness = max(0, pet.happiness - 3);
    pet.energy = max(0, pet.energy - 2);
    pet.cleanliness = max(0, pet.cleanliness - 4);
  }

  // Health decreases if other stats are too low
  if (pet.hunger < 20 || pet.cleanliness < 20) {
    pet.health = max(0, pet.health - 5);
  }

  // Auto sleep when energy is very low
  if (pet.energy < 20 && pet.state != "sleeping" && pet.state != "dead") {
    pet.state = "sleeping";
    pet.stateChangeTime = millis();
    showMessage("Pet is sleepy!");
    Serial.println("Pet went to sleep (low energy)");
  }

  // Check if pet is sick
  if (pet.health < 30 && pet.state != "sick" && pet.state != "dead" && pet.state != "sleeping") {
    pet.state = "sick";
    pet.stateChangeTime = millis();
    showMessage("Pet is sick!");
    Serial.println("Pet became sick!");
  }
  // Check if pet is hungry
  else if (pet.hunger < 20 && pet.state != "sick" && pet.state != "dead" && pet.state != "sleeping") {
    pet.state = "hungry";
    pet.stateChangeTime = millis();
    showMessage("Pet is hungry!");
    Serial.println("Pet is hungry!");
  }
  // Return to normal if stats improve
  else if (pet.state == "hungry" && pet.hunger > 40) {
    pet.state = "normal";
    showMessage("Feeling better!");
  }
  else if (pet.state == "sick" && pet.health > 50) {
    pet.state = "normal";
    showMessage("Recovered!");
  }

  // Check if pet died
  if (pet.health <= 0) {
    pet.isAlive = false;
    pet.state = "dead";
    showMessage("Pet died :(");
    Serial.println("Pet has died!");
  }

  pet.age++;

  // Print stats to serial
  Serial.printf("Age: %d | Hunger: %d | Happy: %d | Health: %d | Energy: %d | Clean: %d | State: %s\n",
                pet.age, pet.hunger, pet.happiness, pet.health, pet.energy, pet.cleanliness, pet.state.c_str());

  // Send immediate alerts to OpenClaw on critical state changes
  if (pet.state != lastAlertedState) {
    lastAlertedState = pet.state;
    if (pet.state == "sick")
      sendToOpenClaw("ALERT: TamaPetchi is sick! Health:" + String(pet.health) + " Hunger:" + String(pet.hunger) + " Clean:" + String(pet.cleanliness) + ". Needs healing!");
    else if (pet.state == "hungry")
      sendToOpenClaw("ALERT: TamaPetchi is hungry! Hunger:" + String(pet.hunger) + ". Please feed!");
    else if (pet.state == "dead")
      sendToOpenClaw("ALERT: TamaPetchi has died! Age was " + String(pet.age) + " minutes. Press reset to start over.");
    else if (pet.state == "sleeping")
      sendToOpenClaw("TamaPetchi fell asleep. Energy was low at " + String(pet.energy) + ".");
  }
}

void drawScreen() {
  // Only redraw title bar if age changed
  if (pet.age != prevAge) {
    tft.fillRect(0, 0, SCREEN_WIDTH, 20, TFT_NAVY);
    tft.setTextColor(COLOR_TEXT, TFT_NAVY);
    tft.setTextSize(2);
    tft.setCursor(10, 3);
    tft.print("TamaPetchi");

    // Draw age
    tft.setTextSize(1);
    tft.setCursor(180, 7);
    tft.printf("Age:%d", pet.age);
    prevAge = pet.age;
  }

  // Only redraw pet if animation frame changed or state changed
  if (animFrame != prevAnimFrame || pet.state != previousState || pet.happiness != prevHappiness) {
    drawPet(120, 90);
    prevAnimFrame = animFrame;
  }

  // Only redraw stats if they changed
  if (pet.hunger != prevHunger || pet.happiness != prevHappiness ||
      pet.health != prevHealth || pet.energy != prevEnergy ||
      pet.cleanliness != prevCleanliness) {
    drawStats();
    prevHunger = pet.hunger;
    prevHappiness = pet.happiness;
    prevHealth = pet.health;
    prevEnergy = pet.energy;
    prevCleanliness = pet.cleanliness;
  }

  // Draw message if active
  if (millis() - messageTime < messageDuration) {
    drawMessage();
  }

  // Only redraw state indicator if changed
  if (pet.state != previousState || pet.isAlive != prevIsAlive) {
    tft.fillRect(0, 265, SCREEN_WIDTH, 15, COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 268);
    tft.print("State: ");
    tft.print(pet.state);

    // Draw button labels
    drawButtonLabels();

    if (!pet.isAlive) {
      tft.fillRect(130, 265, 110, 15, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.setCursor(135, 268);
      tft.print("BTN5=RESET");
    }

    previousState = pet.state;
    prevIsAlive = pet.isAlive;
  }
}

void drawButtonLabels() {
  int y = 275;
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, COLOR_BG);

  int spacing = 48;

  tft.setCursor(2, y);
  tft.print("Feed");

  tft.setCursor(2 + spacing, y);
  tft.print("Play");

  tft.setCursor(2 + spacing*2, y);
  tft.print("Clen");

  tft.setCursor(2 + spacing*3, y);
  if (pet.state == "sleeping") {
    tft.print("Wake");
  } else {
    tft.print("Slep");
  }

  tft.setCursor(2 + spacing*4, y);
  tft.print("Heal");
}

void drawPet(int x, int y) {
  // Clear pet area
  tft.fillRect(x - 55, y - 55, 110, 110, COLOR_BG);

  uint16_t petColor = COLOR_PET;
  if (pet.state == "sick") petColor = COLOR_SICK;
  if (pet.state == "dead") petColor = COLOR_DEAD;

  int bodyY = y;
  if (pet.state == "normal" && pet.happiness > 50 && (animFrame % 2 == 0)) {
    bodyY -= 3;
  }
  if (pet.state == "playing" && (animFrame % 2 == 0)) {
    bodyY -= 8;
  }
  if (pet.state == "hungry") {
    bodyY += (animFrame % 2) * 2;
  }

  // Body
  tft.fillCircle(x, bodyY, 35, petColor);

  // Belly
  tft.fillCircle(x, bodyY + 12, 18, TFT_WHITE);

  // Eyes
  if (pet.state == "sleeping") {
    tft.drawLine(x - 14, bodyY - 10, x - 8, bodyY - 12, TFT_BLACK);
    tft.drawLine(x - 8, bodyY - 12, x - 6, bodyY - 10, TFT_BLACK);
    tft.drawLine(x + 6, bodyY - 10, x + 8, bodyY - 12, TFT_BLACK);
    tft.drawLine(x + 8, bodyY - 12, x + 14, bodyY - 10, TFT_BLACK);
  } else if (pet.state == "dead") {
    tft.drawLine(x - 16, bodyY - 14, x - 8, bodyY - 6, TFT_BLACK);
    tft.drawLine(x - 8, bodyY - 14, x - 16, bodyY - 6, TFT_BLACK);
    tft.drawLine(x + 8, bodyY - 14, x + 16, bodyY - 6, TFT_BLACK);
    tft.drawLine(x + 16, bodyY - 14, x + 8, bodyY - 6, TFT_BLACK);
  } else {
    int eyeSize = 5;
    if (pet.happiness > 70) eyeSize = 6;
    if (pet.happiness < 30) eyeSize = 4;

    tft.fillCircle(x - 12, bodyY - 10, eyeSize, TFT_BLACK);
    tft.fillCircle(x + 12, bodyY - 10, eyeSize, TFT_BLACK);
    tft.fillCircle(x - 11, bodyY - 11, 2, TFT_WHITE);
    tft.fillCircle(x + 13, bodyY - 11, 2, TFT_WHITE);
  }

  // Mouth
  if (pet.state == "eating") {
    tft.fillCircle(x, bodyY + 5, 5, TFT_BLACK);
    tft.fillCircle(x, bodyY + 4, 3, TFT_RED);
  } else if (pet.happiness > 70 && pet.isAlive && pet.state != "sleeping") {
    for (int i = 0; i < 3; i++) {
      tft.drawArc(x, bodyY, 10 + i, 8 + i, 200, 340, TFT_BLACK, COLOR_BG);
    }
  } else if (pet.happiness > 30 && pet.state != "dead") {
    tft.fillRect(x - 8, bodyY + 5, 16, 2, TFT_BLACK);
  } else if (pet.state != "sleeping") {
    for (int i = 0; i < 3; i++) {
      tft.drawArc(x, bodyY + 12, 10 + i, 8 + i, 20, 160, TFT_BLACK, COLOR_BG);
    }
  }

  // Cheeks
  if (pet.happiness > 60 && pet.isAlive && pet.state != "sleeping") {
    tft.fillCircle(x - 25, bodyY + 2, 4, TFT_PINK);
    tft.fillCircle(x + 25, bodyY + 2, 4, TFT_PINK);
  }

  // Feet
  tft.fillCircle(x - 22, bodyY + 32, 6, petColor);
  tft.fillCircle(x + 22, bodyY + 32, 6, petColor);

  // Ears
  tft.fillEllipse(x - 28, bodyY - 25, 6, 10, petColor);
  tft.fillEllipse(x + 28, bodyY - 25, 6, 10, petColor);

  // State indicators
  if (pet.state == "sleeping") {
    int zOffset = (animFrame % 3) * 8;
    tft.setTextColor(TFT_DARKGREY, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(x + 30, bodyY - 25 + zOffset);
    tft.print("z");
    if (animFrame > 1) {
      tft.setTextSize(3);
      tft.setCursor(x + 40, bodyY - 35);
      tft.print("Z");
    }
  } else if (pet.state == "sick") {
    tft.drawLine(x + 35, bodyY - 20, x + 35, bodyY - 8, TFT_SILVER);
    tft.fillCircle(x + 35, bodyY - 6, 4, TFT_RED);
    if (animFrame % 2 == 0) {
      tft.fillCircle(x - 30, bodyY - 30, 2, COLOR_SICK);
      tft.fillCircle(x + 32, bodyY - 28, 2, COLOR_SICK);
    }
  } else if (pet.state == "hungry") {
    tft.drawCircle(x - 40, bodyY + 20, 6, TFT_BROWN);
    tft.drawCircle(x - 40, bodyY + 20, 7, TFT_BROWN);
    tft.setTextColor(TFT_RED, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(x + 25, bodyY - 30);
    tft.print("HUNGRY");
  } else if (pet.state == "playing" && pet.isAlive) {
    if (animFrame % 2 == 0) {
      tft.fillCircle(x - 35, bodyY - 20, 2, TFT_YELLOW);
      tft.fillCircle(x + 38, bodyY - 15, 2, TFT_YELLOW);
    } else {
      tft.fillCircle(x - 30, bodyY + 25, 2, TFT_PINK);
      tft.fillCircle(x + 35, bodyY + 20, 2, TFT_CYAN);
    }
  }

  // Dead state
  if (pet.state == "dead") {
    tft.drawCircle(x, bodyY - 45, 12, TFT_YELLOW);
    tft.drawCircle(x, bodyY - 45, 13, TFT_YELLOW);
    tft.fillRect(x + 35, bodyY + 15, 15, 20, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(x + 38, bodyY + 23);
    tft.print("RIP");
  }
}

void drawStats() {
  int y = 170;
  int barWidth = 180;
  int barHeight = 12;
  int x = 50;
  int spacing = 18;

  drawStatBar(x, y, barWidth, barHeight, pet.hunger, COLOR_HUNGER, "Hun");
  y += spacing;
  drawStatBar(x, y, barWidth, barHeight, pet.happiness, COLOR_HAPPY, "Hap");
  y += spacing;
  drawStatBar(x, y, barWidth, barHeight, pet.health, COLOR_HEALTH, "HP");
  y += spacing;
  drawStatBar(x, y, barWidth, barHeight, pet.energy, COLOR_ENERGY, "Eng");
  y += spacing;
  drawStatBar(x, y, barWidth, barHeight, pet.cleanliness, COLOR_CLEAN, "Cln");
}

void drawStatBar(int x, int y, int width, int height, int value, uint16_t color, const char* label) {
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(x - 30, y + 2);
  tft.print(label);

  tft.fillRect(x, y, width, height, COLOR_BAR_BG);

  int fillWidth = (width * value) / 100;
  tft.fillRect(x, y, fillWidth, height, color);

  tft.drawRect(x, y, width, height, COLOR_TEXT);

  tft.setCursor(x + width + 5, y + 2);
  tft.printf("%d", value);
}

void drawMessage() {
  int msgWidth = 200;
  int msgHeight = 30;
  int x = (SCREEN_WIDTH - msgWidth) / 2;
  int y = 35;

  tft.fillRoundRect(x, y, msgWidth, msgHeight, 5, TFT_YELLOW);
  tft.drawRoundRect(x, y, msgWidth, msgHeight, 5, TFT_BLACK);
  tft.drawRoundRect(x+1, y+1, msgWidth-2, msgHeight-2, 5, TFT_BLACK);

  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  tft.setTextSize(2);

  int textWidth = messageText.length() * 12;
  int textX = x + (msgWidth - textWidth) / 2;
  tft.setCursor(textX, y + 10);
  tft.print(messageText);
}

void showMessage(String msg) {
  messageText = msg;
  messageTime = millis();
  Serial.print("Message: ");
  Serial.println(msg);
}

// ========== WEB SERVER HANDLERS ==========

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 TamaPetchi</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 400px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            padding: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        }
        h1 {
            text-align: center;
            color: #667eea;
            margin-bottom: 20px;
        }
        .pet-display {
            background: linear-gradient(145deg, #e0f0ff, #b0d7ff);
            border-radius: 15px;
            padding: 30px;
            text-align: center;
            margin-bottom: 20px;
        }
        .pet {
            font-size: 80px;
            margin: 20px 0;
        }
        .stat {
            margin: 10px 0;
        }
        .stat-label {
            display: inline-block;
            width: 100px;
            font-weight: bold;
        }
        .stat-bar {
            display: inline-block;
            width: 200px;
            height: 20px;
            background: #ddd;
            border-radius: 10px;
            overflow: hidden;
            vertical-align: middle;
        }
        .stat-fill {
            height: 100%;
            transition: width 0.3s;
        }
        .hunger { background: linear-gradient(90deg, #ff9800, #ff5722); }
        .happiness { background: linear-gradient(90deg, #ffeb3b, #ffc107); }
        .health { background: linear-gradient(90deg, #4caf50, #2e7d32); }
        .energy { background: linear-gradient(90deg, #2196f3, #1565c0); }
        .cleanliness { background: linear-gradient(90deg, #00bcd4, #0097a7); }
        .buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-top: 20px;
        }
        button {
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.1s;
        }
        button:hover {
            transform: scale(1.05);
        }
        button:active {
            transform: scale(0.95);
        }
        .feed-btn { background: #ff9800; color: white; }
        .play-btn { background: #4caf50; color: white; }
        .clean-btn { background: #00bcd4; color: white; }
        .sleep-btn { background: #9c27b0; color: white; }
        .heal-btn { background: #f44336; color: white; }
        .reset-btn { background: #607d8b; color: white; grid-column: 1 / -1; }
        .state {
            text-align: center;
            font-size: 18px;
            font-weight: bold;
            color: #667eea;
            margin-top: 10px;
        }
        .message {
            text-align: center;
            padding: 10px;
            margin: 10px 0;
            border-radius: 10px;
            font-weight: bold;
            display: none;
        }
        .message.show {
            display: block;
        }
        .message.success {
            background: #4caf50;
            color: white;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎮 TamaPetchi 🐾</h1>
        
        <div class="pet-display">
            <div class="pet" id="pet-emoji">🐱</div>
            <div class="state" id="state">State: normal</div>
            <div style="margin-top: 10px; color: #666;">Age: <span id="age">0</span> min</div>
        </div>
        
        <div class="message" id="message"></div>
        
        <div class="stat">
            <span class="stat-label">Hunger:</span>
            <div class="stat-bar">
                <div class="stat-fill hunger" id="hunger-bar" style="width: 50%"></div>
            </div>
            <span id="hunger-val">50%</span>
        </div>
        
        <div class="stat">
            <span class="stat-label">Happiness:</span>
            <div class="stat-bar">
                <div class="stat-fill happiness" id="happiness-bar" style="width: 50%"></div>
            </div>
            <span id="happiness-val">50%</span>
        </div>
        
        <div class="stat">
            <span class="stat-label">Health:</span>
            <div class="stat-bar">
                <div class="stat-fill health" id="health-bar" style="width: 80%"></div>
            </div>
            <span id="health-val">80%</span>
        </div>
        
        <div class="stat">
            <span class="stat-label">Energy:</span>
            <div class="stat-bar">
                <div class="stat-fill energy" id="energy-bar" style="width: 100%"></div>
            </div>
            <span id="energy-val">100%</span>
        </div>
        
        <div class="stat">
            <span class="stat-label">Cleanliness:</span>
            <div class="stat-bar">
                <div class="stat-fill cleanliness" id="cleanliness-bar" style="width: 80%"></div>
            </div>
            <span id="cleanliness-val">80%</span>
        </div>
        
        <div class="buttons">
            <button class="feed-btn" onclick="action('feed')">🍖 Feed</button>
            <button class="play-btn" onclick="action('play')">🎮 Play</button>
            <button class="clean-btn" onclick="action('clean')">🧼 Clean</button>
            <button class="sleep-btn" onclick="action('sleep')">😴 Sleep</button>
            <button class="heal-btn" onclick="action('heal')">💚 Heal</button>
            <button class="reset-btn" onclick="action('reset')">🔄 Reset</button>
        </div>
    </div>

    <script>
        function updateDisplay() {
            fetch('/pet')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('hunger-bar').style.width = data.hunger + '%';
                    document.getElementById('hunger-val').textContent = data.hunger + '%';
                    
                    document.getElementById('happiness-bar').style.width = data.happiness + '%';
                    document.getElementById('happiness-val').textContent = data.happiness + '%';
                    
                    document.getElementById('health-bar').style.width = data.health + '%';
                    document.getElementById('health-val').textContent = data.health + '%';
                    
                    document.getElementById('energy-bar').style.width = data.energy + '%';
                    document.getElementById('energy-val').textContent = data.energy + '%';
                    
                    document.getElementById('cleanliness-bar').style.width = data.cleanliness + '%';
                    document.getElementById('cleanliness-val').textContent = data.cleanliness + '%';
                    
                    document.getElementById('age').textContent = data.age;
                    document.getElementById('state').textContent = 'State: ' + data.state;
                    
                    let emoji = '🐱';
                    if (!data.isAlive) emoji = '💀';
                    else if (data.state === 'sleeping') emoji = '😴';
                    else if (data.state === 'eating') emoji = '😋';
                    else if (data.state === 'playing') emoji = '😸';
                    else if (data.state === 'sick') emoji = '🤢';
                    else if (data.state === 'hungry') emoji = '😿';
                    else if (data.happiness > 70) emoji = '😺';
                    document.getElementById('pet-emoji').textContent = emoji;
                });
        }
        
        function action(act) {
            fetch('/' + act, { method: 'POST' })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showMessage(data.message || 'Success!');
                    }
                    updateDisplay();
                });
        }
        
        function showMessage(msg) {
            const msgEl = document.getElementById('message');
            msgEl.textContent = msg;
            msgEl.className = 'message success show';
            setTimeout(() => {
                msgEl.className = 'message';
            }, 3000);
        }
        
        setInterval(updateDisplay, 2000);
        updateDisplay();
    </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleGetPet() {
  String json = "{";
  json += "\"hunger\":" + String(pet.hunger) + ",";
  json += "\"happiness\":" + String(pet.happiness) + ",";
  json += "\"health\":" + String(pet.health) + ",";
  json += "\"energy\":" + String(pet.energy) + ",";
  json += "\"cleanliness\":" + String(pet.cleanliness) + ",";
  json += "\"age\":" + String(pet.age) + ",";
  json += "\"isAlive\":" + String(pet.isAlive ? "true" : "false") + ",";
  json += "\"state\":\"" + pet.state + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleFeed() {
  if (pet.isAlive) {
    pet.hunger = min(100, pet.hunger + 20);
    pet.energy = max(0, pet.energy - 5);
    pet.state = "eating";
    pet.stateChangeTime = millis();
    showMessage("Yummy!");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Fed!\"}");
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Pet is dead\"}");
  }
}

void handlePlay() {
  if (pet.isAlive && pet.energy >= 10) {
    pet.happiness = min(100, pet.happiness + 15);
    pet.energy = max(0, pet.energy - 15);
    pet.hunger = max(0, pet.hunger - 10);
    pet.state = "playing";
    pet.stateChangeTime = millis();
    showMessage("Wheee!");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Playing!\"}");
  } else if (pet.isAlive) {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Too tired!\"}");
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Pet is dead\"}");
  }
}

void handleClean() {
  if (pet.isAlive) {
    pet.cleanliness = 100;
    pet.health = min(100, pet.health + 5);
    showMessage("Sparkly!");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Clean!\"}");
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Pet is dead\"}");
  }
}

void handleSleep() {
  if (pet.isAlive) {
    if (pet.state == "sleeping") {
      pet.state = "normal";
      showMessage("Good morning!");
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Awake!\"}");
    } else {
      pet.state = "sleeping";
      pet.stateChangeTime = millis();
      showMessage("Zzz...");
      server.send(200, "application/json", "{\"success\":true,\"message\":\"Sleeping!\"}");
    }
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Pet is dead\"}");
  }
}

void handleHeal() {
  if (pet.isAlive) {
    pet.health = 100;
    pet.state = "normal";
    showMessage("Healthy!");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Healed!\"}");
  } else {
    server.send(200, "application/json", "{\"success\":false,\"message\":\"Pet is dead\"}");
  }
}

void handleResetWeb() {
  pet.hunger = 50;
  pet.happiness = 50;
  pet.health = 80;
  pet.energy = 100;
  pet.cleanliness = 80;
  pet.age = 0;
  pet.isAlive = true;
  pet.state = "normal";
  pet.stateChangeTime = millis();
  lastAlertedState = "normal";
  showMessage("New Pet!");
  sendToOpenClaw("TamaPetchi has been reset! New pet starting fresh.");
  server.send(200, "application/json", "{\"success\":true,\"message\":\"Reset!\"}");
}

void handleButtons() {
  // Special case: If pet is dead, Button 5 becomes RESET button
  if (!pet.isAlive && digitalRead(BTN_HEAL) == LOW) {
    delay(200);
    Serial.println("RESETTING PET - Starting fresh!");

    pet.hunger = 50;
    pet.happiness = 50;
    pet.health = 80;
    pet.energy = 100;
    pet.cleanliness = 80;
    pet.age = 0;
    pet.isAlive = true;
    pet.state = "normal";
    pet.stateChangeTime = millis();
    lastAlertedState = "normal";

    showMessage("New Pet!");
    Serial.println("Pet reset complete! Starting fresh.");
    sendToOpenClaw("TamaPetchi has been reset via button! New pet starting fresh.");

    tft.fillScreen(COLOR_BG);

    while(digitalRead(BTN_HEAL) == LOW);
    return;
  }

  // Feed Button
  if (digitalRead(BTN_FEED) == LOW) {
    delay(200);
    if (pet.isAlive) {
      pet.hunger = min(100, pet.hunger + 20);
      pet.energy = max(0, pet.energy - 5);
      pet.state = "eating";
      pet.stateChangeTime = millis();
      showMessage("Yummy!");
      Serial.println("Fed the pet!");

      for(int i = 0; i < 3; i++) {
        delay(100);
        drawStatBar(50, 170, 180, 12, pet.hunger, TFT_WHITE, "Hun");
        delay(100);
        drawStatBar(50, 170, 180, 12, pet.hunger, COLOR_HUNGER, "Hun");
      }
    }
    while(digitalRead(BTN_FEED) == LOW);
  }

  // Play Button
  if (digitalRead(BTN_PLAY) == LOW) {
    delay(200);
    if (pet.isAlive && pet.energy >= 10) {
      pet.happiness = min(100, pet.happiness + 15);
      pet.energy = max(0, pet.energy - 15);
      pet.hunger = max(0, pet.hunger - 10);
      pet.state = "playing";
      pet.stateChangeTime = millis();
      showMessage("Wheee!");
      Serial.println("Playing with pet!");

      for(int i = 0; i < 3; i++) {
        delay(100);
        drawStatBar(50, 188, 180, 12, pet.happiness, TFT_WHITE, "Hap");
        delay(100);
        drawStatBar(50, 188, 180, 12, pet.happiness, COLOR_HAPPY, "Hap");
      }
    } else if (pet.isAlive) {
      showMessage("Too tired!");
    }
    while(digitalRead(BTN_PLAY) == LOW);
  }

  // Clean Button
  if (digitalRead(BTN_CLEAN) == LOW) {
    delay(200);
    if (pet.isAlive) {
      pet.cleanliness = 100;
      pet.health = min(100, pet.health + 5);
      showMessage("Sparkly!");
      Serial.println("Cleaned the pet!");

      for(int i = 0; i < 3; i++) {
        delay(100);
        drawStatBar(50, 242, 180, 12, pet.cleanliness, TFT_WHITE, "Cln");
        delay(100);
        drawStatBar(50, 242, 180, 12, pet.cleanliness, COLOR_CLEAN, "Cln");
      }
    }
    while(digitalRead(BTN_CLEAN) == LOW);
  }

  // Sleep Button
  if (digitalRead(BTN_SLEEP) == LOW) {
    delay(200);
    if (pet.isAlive) {
      if (pet.state == "sleeping") {
        pet.state = "normal";
        showMessage("Good morning!");
        Serial.println("Pet woke up!");
      } else {
        pet.state = "sleeping";
        pet.stateChangeTime = millis();
        showMessage("Zzz...");
        Serial.println("Pet went to sleep!");
      }
    }
    while(digitalRead(BTN_SLEEP) == LOW);
  }

  // Heal Button
  if (digitalRead(BTN_HEAL) == LOW) {
    delay(200);
    if (pet.isAlive) {
      pet.health = 100;
      pet.state = "normal";
      showMessage("Healthy!");
      Serial.println("Healed the pet!");

      for(int i = 0; i < 3; i++) {
        delay(100);
        drawStatBar(50, 206, 180, 12, pet.health, TFT_WHITE, "HP");
        delay(100);
        drawStatBar(50, 206, 180, 12, pet.health, COLOR_HEALTH, "HP");
      }
    }
    while(digitalRead(BTN_HEAL) == LOW);
  }
}
