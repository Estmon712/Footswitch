#include <Arduino.h>
#include <BLEMidi.h>

// ─────────────────────────────────────────────
//  Configuración de hardware
// ─────────────────────────────────────────────
const int BT_LED_PIN = 3;   // LED de estado Bluetooth
const int NUM_BUTTONS = 6;  // Número de botones/LEDs

// Pines de botones y LEDs (deben coincidir con el PCB)
const int BUT_PINS[NUM_BUTTONS] = {4,  7,  17, 2,  40, 37};
const int LED_PINS[NUM_BUTTONS] = {5, 15,  18, 1,  41, 38};

// ─────────────────────────────────────────────
//  Configuración MIDI
// ─────────────────────────────────────────────
// Canal MIDI (1–16). El mVAVe debe estar configurado en el mismo canal.
const uint8_t MIDI_CHANNEL = 1;

// Número de CC asignado a cada botón (0–127).
// Configura estos mismos valores en el mVAVe con la app CubeSuite.
const uint8_t CC_NUMBERS[NUM_BUTTONS] = {20, 21, 22, 23, 24, 25};

// Valor MIDI enviado cuando el botón está ON y OFF
const uint8_t CC_VAL_ON  = 127;
const uint8_t CC_VAL_OFF = 0;

// ─────────────────────────────────────────────
//  Temporización
// ─────────────────────────────────────────────
const unsigned long DEBOUNCE_MS       = 50;   // Anti-rebote
const unsigned long FAST_BLINK_MS     = 200;  // LED BT sin conexión
const unsigned long SLOW_BLINK_MS     = 1000; // LED BT con conexión

// ─────────────────────────────────────────────
//  Estado interno
// ─────────────────────────────────────────────
bool buttonToggle[NUM_BUTTONS]      = {false}; // Estado ON/OFF de cada botón
bool lastButtonRaw[NUM_BUTTONS]     = {false}; // Último estado físico leído
unsigned long lastDebounceTime[NUM_BUTTONS] = {0};

bool isConnected = false;

// ─────────────────────────────────────────────
//  Tarea FreeRTOS: parpadeo del LED Bluetooth
// ─────────────────────────────────────────────
void btLedBlinkTask(void* parameter) {
  while (true) {
    unsigned long interval = isConnected ? SLOW_BLINK_MS : FAST_BLINK_MS;
    digitalWrite(BT_LED_PIN, HIGH);
    vTaskDelay(interval / portTICK_PERIOD_MS);
    digitalWrite(BT_LED_PIN, LOW);
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}

// ─────────────────────────────────────────────
//  Callback de conexión/desconexión
// ─────────────────────────────────────────────
void onConnect() {
  isConnected = true;
  Serial.println("[BLE] Dispositivo conectado");
}

void onDisconnect() {
  isConnected = false;
  Serial.println("[BLE] Dispositivo desconectado");

  // Apagar todos los LEDs de botones al desconectar
  for (int i = 0; i < NUM_BUTTONS; i++) {
    digitalWrite(LED_PINS[i], LOW);
    buttonToggle[i] = false;
  }
}

// ─────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // LED Bluetooth
  pinMode(BT_LED_PIN, OUTPUT);
  digitalWrite(BT_LED_PIN, LOW);

  // Botones y LEDs de botones
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUT_PINS[i], INPUT);
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  // Inicia el servidor BLE-MIDI con callbacks
  BLEMidiServer.begin("Footswitch");
  BLEMidiServer.setOnConnectCallback(onConnect);
  BLEMidiServer.setOnDisconnectCallback(onDisconnect);

  Serial.println("[BLE] Servidor MIDI BLE iniciado. Esperando conexión...");

  // Tarea FreeRTOS para el parpadeo del LED BT
  xTaskCreate(
    btLedBlinkTask,      // Función
    "BT LED Blink Task", // Nombre
    1024,                // Stack size
    NULL,                // Parámetros
    1,                   // Prioridad
    NULL                 // Handle (opcional)
  );
}

// ─────────────────────────────────────────────
//  Loop - lectura de botones con debounce
// ─────────────────────────────────────────────
void loop() {
  if (!isConnected) return;

  unsigned long now = millis();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool reading = digitalRead(BUT_PINS[i]);

    // Si el estado físico cambió, reinicia el temporizador de debounce
    if (reading != lastButtonRaw[i]) {
      lastDebounceTime[i] = now;
    }

    // Solo procesa si el estado es estable por más de DEBOUNCE_MS
    if ((now - lastDebounceTime[i]) > DEBOUNCE_MS) {
      // Detecta flanco de subida (botón presionado)
      if (reading == HIGH && lastButtonRaw[i] == LOW) {
        // Toggle: alterna entre ON y OFF
        buttonToggle[i] = !buttonToggle[i];

        uint8_t ccValue = buttonToggle[i] ? CC_VAL_ON : CC_VAL_OFF;

        // Enviar mensaje MIDI Control Change
        BLEMidiServer.controlChange(
          MIDI_CHANNEL - 1,  // La librería usa 0-indexed (0 = canal 1)
          CC_NUMBERS[i],
          ccValue
        );

        // Actualizar LED del botón
        digitalWrite(LED_PINS[i], buttonToggle[i] ? HIGH : LOW);

        Serial.printf("[MIDI] Botón %d → CC#%d = %d\n",
                      i + 1, CC_NUMBERS[i], ccValue);
      }
    }

    lastButtonRaw[i] = reading;
  }
}