#include <Arduino.h>
#include <KeyboardDevice.h>
#include <BleCompositeHID.h>

// LED de estado del Bluetooth
const int btLedPin = 3; // Pin 3 para el LED del Bluetooth

// Número de botones y LEDs
const int buttons = 6;

// Pines de botones y LEDs
int but[] = {4, 7, 17, 2, 40, 37};
int led[] = {5, 15, 18, 1, 41, 38};

// Intervalos de parpadeo (en ms)
const int FAST_BLINK_INTERVAL = 200;  // Parpadeo rápido
const int SLOW_BLINK_INTERVAL = 1000; // Parpadeo lento

// Dispositivo BLE tipo teclado
BleCompositeHID dispositivoBLE("Footswitch", "Blackbox", 100);
KeyboardDevice* teclado;

// Variable para indicar el estado de conexión
bool isConnected = false;

// Tarea para el parpadeo del LED Bluetooth
void btLedBlinkTask(void* parameter) {
  int blinkInterval = FAST_BLINK_INTERVAL;

  while (true) {
    blinkInterval = isConnected ? SLOW_BLINK_INTERVAL : FAST_BLINK_INTERVAL;

    // Controla el LED del Bluetooth
    digitalWrite(btLedPin, HIGH); // Encendido
    vTaskDelay(blinkInterval / portTICK_PERIOD_MS);
    digitalWrite(btLedPin, LOW);  // Apagado
    vTaskDelay(blinkInterval / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  // Configuración del LED Bluetooth
  pinMode(btLedPin, OUTPUT);
  digitalWrite(btLedPin, LOW); // Asegúrate de que el LED esté apagado al inicio

  // Configuración de botones y LEDs
  for (int i = 0; i < buttons; i++) {
    pinMode(but[i], INPUT);
    pinMode(led[i], OUTPUT);
    digitalWrite(led[i], LOW); // Asegúrate de que los LEDs estén apagados al inicio
  }

  // Inicialización del dispositivo BLE
  teclado = new KeyboardDevice();
  dispositivoBLE.addDevice(teclado);
  dispositivoBLE.begin();

  // Mensaje de inicio
  Serial.println("Conéctate con el dispositivo");
  
  // Crea la tarea para el parpadeo del LED Bluetooth
  xTaskCreate(
    btLedBlinkTask,     // Función de la tarea
    "BT LED Blink Task", // Nombre de la tarea
    1024,               // Tamaño de la pila
    NULL,               // Parámetros de la tarea
    1,                  // Prioridad de la tarea
    NULL                // Identificador de la tarea (opcional)
  );
}

void loop() {
  // Actualiza el estado de conexión
  isConnected = dispositivoBLE.isConnected();

  // Si está conectado, procesa los botones
  if (isConnected) {
    for (int i = 0; i < buttons; i++) {
      bool estado = digitalRead(but[i]);
      if (estado) {
        for (size_t j = 0; j < buttons; i++){// Apaga los leds
          digitalWrite(led[j], LOW);
        }
        digitalWrite(led[i], HIGH);
        
        teclado->keyPress(KEY_A + i); // Envía una tecla distinta por botón
        delay(10); // Pequeña pausa
        teclado->keyRelease(KEY_A + i);
        delay(100); // Ajusta el retraso según sea necesario
      }
    }
  }
}



/*
#include <Arduino.h>
#include <KeyboardDevice.h>
#include <BleCompositeHID.h>

int buttons = 6;

//Se definen los pines para los leds y los botones como vectores
int but[] = {39, 32, 26, 2, 16, 22};
int led[] = {34, 33, 27, 0, 17, 23};

//Se define y se crea el dispositivo tipo teclado
BleCompositeHID dispositivoBLE("Footswitch", "Blackbox",100);
KeyboardDevice* teclado;



void setup() {
  Serial.begin(115200);
  
  for (size_t i = 0; i < buttons; i++){
    pinMode(but[i], INPUT);
    pinMode(led[i], OUTPUT);
  }
  

  teclado = new KeyboardDevice();
  dispositivoBLE.addDevice(teclado);
  dispositivoBLE.begin();

  Serial.println("Conectate con el dispositivo");
  delay(3000);
}

void loop() {
  if (dispositivoBLE.isConnected()){
    bool estado = digitalRead(boton);
    if (estado){
      teclado -> keyPress(KEY_A);
      delay(10);
      teclado -> keyRelease(KEY_A);
      delay(500);
    }    
  }  
}
*/