/* --- LIBRERIAS ---*/
#include <ContinuousStepper.h> // Biblioteca para el control del motor de pasos
#include <LiquidCrystal_I2C.h> // Biblioteca para la pantalla LCD
#include <Wire.h> // Biblioteca para la comunicación I2C
#include<SoftwareSerial.h> // Biblioteca para la comunicación serial por software

/* --- INSTANCIAS ---*/
ContinuousStepper stepper; // Instancia del motor de pasos
LiquidCrystal_I2C lcd(0x27, 16, 2); // Instancia de la pantalla LCD
SoftwareSerial bt(10,11);  // Instancia de la comunicación serial por software

/* --- CONEXIONES --- */
// Define los pines de conexión para los diferentes componentes
// Conexiones Driver
const int stepPin = 9; // Pin de paso del driver del motor
const int dirPin = 8; // Pin de dirección del driver del motor

// Conexiones Encoder
const int encoderPin = 2; // Pin del encoder

/* --- CONFIGURACIÓN --- */
// Define las variables de configuración
int pulsesPerRevolution = 25; // Número de pulsos por revolución del encoder
long interval = 2500; // Intervalo para la actualización de la pantalla LCD

/* --- VARIABLES GLOBALES --- */
// Define las variables globales
// Variables Encoder
volatile unsigned long pulseCount = 0; // Contador de pulsos del encoder

// Variables RPM
int d = 0; // Variable para almacenar los datos recibidos por la comunicación serial
float steps_per_second = 0; // Velocidad del motor en pasos por segundo
float rpm = 0; // Velocidad del motor en revoluciones por minuto

// Variables Tiempo
unsigned long previousDisplayMillis = 0; // Tiempo anterior para la actualización de la pantalla LCD
unsigned long currentDisplayMillis = 0; // Tiempo actual para la actualización de la pantalla LCD
static volatile unsigned long debounceTime = 0; // Tiempo para el debounce del encoder

// Declaración Variables Display
char buffer[10]; // Buffer para formatear la velocidad en RPM
String formatted_rpm; // Velocidad en RPM formateada para la pantalla LCD

void setup() {
  // Configura el encoder
  pinMode(encoderPin, INPUT);
  attachInterrupt(0, pulseEvent, RISING); // Asocia una interrupción al pin del encoder

  // Configura el motor de pasos
  stepper.begin(stepPin, dirPin);
  stepper.spin(steps_per_second);

  // Configura la pantalla LCD
  lcd.init();
  lcd.backlight();

  // Configura la comunicación serial por software
  bt.begin(9600);  /* Define baud rate for software serial communication */

  // Inicializa el tiempo para la actualización de la pantalla LCD
  previousDisplayMillis = millis();
}

void loop() {
  // Actualiza el estado del motor de pasos
  stepper.loop();

  // Actualiza el tiempo actual para la actualización de la pantalla LCD
  currentDisplayMillis = millis();

  // Si ha pasado el intervalo definido, calcula la velocidad en RPM, la muestra en la pantalla LCD y la envía por la comunicación serial
  if (currentDisplayMillis - previousDisplayMillis >= interval) {
    float elapsedTime = (float)(currentDisplayMillis - previousDisplayMillis) / 1000;
    float revolutions = (float)pulseCount / pulsesPerRevolution;
    rpm = (revolutions / elapsedTime) * 60;
    dtostrf(rpm, 5, 1, buffer);
    formatted_rpm = "Vel Ang: " + String(buffer) + " RPM";
    displayMessage(0, 0, formatted_rpm);
    bt.print(String(buffer));
    pulseCount = 0;
    previousDisplayMillis = currentDisplayMillis;
  }

  // Si hay datos disponibles en la comunicación serial, lee los datos y cambia la velocidad del motor de pasos
  if (bt.available() > 0) {
    d = bt.read();
    stepper.spin(d * (800 / 60));
  }
}

// Función para mostrar un mensaje en la pantalla LCD
void displayMessage(int row, int col, String message) {
  lcd.setCursor(col, row);
  lcd.print(message);
}

// Función de interrupción para el encoder
void pulseEvent() {
  // Si el pin del encoder está en alto y ha pasado suficiente tiempo desde el último pulso (debounce), incrementa el contador de pulsos
  if (digitalRead(encoderPin) && (micros() - debounceTime > 1000) && digitalRead(encoderPin)) {
    pulseCount++;
    debounceTime = micros();
  }
}
