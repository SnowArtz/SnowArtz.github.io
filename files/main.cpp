// Incluye las bibliotecas necesarias
#include <Arduino.h>
#include "HX711.h" // Biblioteca para el módulo de balanza HX711
#include <Wire.h> // Biblioteca para la comunicación I2C
#include <LiquidCrystal_I2C.h> // Biblioteca para la pantalla LCD
#include <EEPROM.h> // Biblioteca para la memoria EEPROM
#include <freertos/FreeRTOS.h> // Biblioteca para el sistema operativo en tiempo real FreeRTOS
#include <freertos/task.h> // Biblioteca para las tareas de FreeRTOS
#include <OneWire.h> // Biblioteca para la comunicación OneWire
#include <DallasTemperature.h> // Biblioteca para el sensor de temperatura Dallas
#include "BluetoothSerial.h" // Biblioteca para la comunicación Bluetooth

/* --- CONEXIONES --- */
// Define los pines de conexión para los diferentes componentes
// Conexiones Balanza
const int DT = 13; // Pin de datos de la balanza
const int CLK = 12; // Pin de reloj de la balanza

// Conexiones Sensor de Flujo
const int flow_sensor = 35; // Pin del sensor de flujo

// Conexiones Bomba
const int enA = 33; // Pin de habilitación de la bomba
const int in1 = 32; // Primer pin de entrada de la bomba
const int in2 = 25; // Segundo pin de entrada de la bomba

// Conexiones sensor de temperatura
const int temperaturePin = 5; // Pin del sensor de temperatura

/* --- INSTANCIAS --- */
// Crea instancias de los diferentes componentes
LiquidCrystal_I2C lcd(0x27, 16, 2); // Instancia de la pantalla LCD
HX711 balanza; // Instancia de la balanza
OneWire ourWire(temperaturePin); // Instancia del bus OneWire para el sensor de temperatura
DallasTemperature sensorDS(&ourWire); // Instancia del sensor de temperatura
BluetoothSerial SerialBT; // Instancia de la comunicación Bluetooth

/* --- CONFIGURACIÓN --- */
// Define las variables de configuración
float peso_calibracion = 50.99; // Peso de calibración para la balanza
double ml_required; // Cantidad de mililitros requeridos para dispensar
double ml_per_pulse = 0.022; // Cantidad de mililitros por pulso del sensor de flujo

/* --- VARIABLES GLOBALES --- */
// Define las variables globales
// Variables sensor de flujo
volatile long pulse = 0; // Contador de pulsos del sensor de flujo
float flow = 0; // Flujo de líquido

// Estados de los botones
int tareButtonState; // Estado del botón de tarado

// Declaración variables balanza
float escala; // Escala de la balanza
float peso; // Peso medido por la balanza
float temperatura; // Temperatura medida por el sensor de temperatura

/* --- STATE MACHINE --- */
// Define los estados posibles para las máquinas de estado de pesaje y dispensación
enum StatesWeighing
{
  STATE_CALIBRATING, // Estado de calibración
  STATE_TARING, // Estado de tarado
  STATE_WEIGHING // Estado de pesaje
};

enum StatesDispensing
{
  STATE_NOTHING_D, // Estado de no dispensación
  STATE_DISPENSING, // Estado de dispensación
  STATE_STOP_DISPENSING // Estado de detención de la dispensación
};

// Define el estado inicial de las máquinas de estado
StatesWeighing weighing_CurrentState = STATE_WEIGHING;
StatesDispensing dispensing_CurrentState = STATE_NOTHING_D;

// Define las tareas de FreeRTOS
TaskHandle_t Task1, Task2;

// Función de interrupción para contar los pulsos del sensor de flujo
void count_pulse()
{
  pulse++; // Incrementa el contador de pulsos
}

// Tarea de pesaje: se encarga de la calibración, el tarado y el pesaje de la balanza, y de la lectura del sensor de temperatura
void weighingTaskFunction(void *pvParameters)
{
  for (;;)
  {
    switch (weighing_CurrentState)
    {
    case STATE_CALIBRATING:
      // En este estado, el sistema calibra la balanza. Primero muestra un mensaje en la pantalla LCD,
      // luego lee el valor de la balanza, establece la escala y realiza un tarado. Después, muestra el peso de referencia,
      // pide al usuario que coloque el peso de referencia, calcula la escala y la guarda en la memoria EEPROM.
      // Finalmente, pide al usuario que retire el peso de referencia y cambia al estado de pesaje.  
      float adc_lecture;
      lcd.setCursor(3, 0);
      lcd.print("Calibrando");
      lcd.setCursor(4, 1);
      lcd.print("Balanza");
      delay(3000);
      balanza.read();
      balanza.set_scale();
      balanza.tare(255);
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Peso referencia:");
      lcd.setCursor(1, 1);
      lcd.print(peso_calibracion);
      lcd.print(" g");
      delay(3000);
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Ponga el peso");
      lcd.setCursor(1, 1);
      lcd.print("de referencia");
      delay(3000);
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Espere");
      delay(1000);
      adc_lecture = balanza.get_value(255);
      escala = adc_lecture / peso_calibracion;
      EEPROM.put(0, escala);
      EEPROM.commit();
      delay(100);
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Retire el peso");
      lcd.setCursor(1, 1);
      lcd.print("de referencia");
      delay(3000);
      lcd.clear();
      lcd.setCursor(5, 0);
      lcd.print("Listo!");
      delay(1000);
      lcd.clear();
      balanza.set_scale(escala);
      balanza.tare(255);
      weighing_CurrentState = STATE_WEIGHING;
      break;

    case STATE_WEIGHING:
      // En este estado, el sistema lee el peso de la balanza y la temperatura del sensor de temperatura,
      // muestra estos valores en la pantalla LCD y los envía por Bluetooth. Este proceso se repite cada 200 ms.
      peso = balanza.get_units(255);
      lcd.setCursor(1, 0);
      lcd.print("Peso: ");
      lcd.print(peso);
      lcd.print(" g");
      sensorDS.requestTemperatures();
      temperatura = sensorDS.getTempCByIndex(0);
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      lcd.print(temperatura);
      lcd.print(" C");
      SerialBT.print(peso);
      SerialBT.print("|");
      SerialBT.print(temperatura);
      delay(200);
      break;

    case STATE_TARING:
      // En este estado, el sistema realiza un tarado de la balanza, muestra un mensaje en la pantalla LCD
      // y luego cambia al estado de pesaje.
      lcd.clear();
      lcd.setCursor(3, 0);
      lcd.print("Tarando...: ");
      delay(2000);
      balanza.tare(255);
      lcd.clear();
      lcd.setCursor(5, 0);
      lcd.print("Listo!");
      delay(2000);
      weighing_CurrentState = STATE_WEIGHING;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // Retraso para evitar la sobrecarga de la CPU
  }
}

// Tarea de dispensación: controla la bomba para dispensar el líquido y detener la dispensación cuando se ha alcanzado la cantidad requerida
void dispensingTaskFunction(void *pvParameters)
{
  for (;;)
  {
    switch (dispensing_CurrentState)
    {
    case STATE_NOTHING_D:
      // No hace nada en este estado
      vTaskDelay(pdMS_TO_TICKS(250)); // Retraso para evitar la sobrecarga de la CPU
      break;

    case STATE_DISPENSING:
      // En este estado, el sistema activa la bomba para dispensar el líquido. Comprueba si la cantidad de líquido
      // dispensada (calculada a partir de los pulsos del sensor de flujo) ha alcanzado la cantidad requerida.
      // Si es así, cambia al estado de detención de la dispensación.
      break;
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);

      if (pulse * ml_per_pulse >= ml_required)
      {
        dispensing_CurrentState = STATE_STOP_DISPENSING;
      }
      break;

    case STATE_STOP_DISPENSING:
      // En este estado, el sistema detiene la bomba, resetea el contador de pulsos y el flujo de líquido,
      // y cambia al estado de no dispensación.
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
      flow = 0;
      pulse = 0;
      dispensing_CurrentState = STATE_NOTHING_D;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Retraso para evitar la sobrecarga de la CPU
  }
}

// Configura los componentes en el setup
void setup()
{
  // Inicia la memoria EEPROM
  EEPROM.begin(4);

  // Inicia la balanza
  balanza.begin(DT, CLK);

  // Configura el sensor de flujo
  pinMode(flow_sensor, INPUT);
  attachInterrupt(flow_sensor, count_pulse, RISING);

  // Configura la bomba
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  analogWrite(enA, 255);

  // Configura la pantalla LCD
  lcd.init();
  lcd.backlight();

  // Inicia el sensor de temperatura
  sensorDS.begin();

  // Lee la escala de la balanza de la memoria EEPROM
  EEPROM.get(0, escala);

  // Configura la escala y el tarado de la balanza
  balanza.set_scale(escala);
  balanza.tare(255);

  // Crea las tareas de FreeRTOS
  xTaskCreatePinnedToCore(dispensingTaskFunction, "dispensingTask", 5000, NULL, 3, &Task1, 0);
  delay(500);
  xTaskCreatePinnedToCore(weighingTaskFunction, "weighingTask", 5000, NULL, 1, &Task2, 1);
  delay(500);

  // Inicia la comunicación serial y Bluetooth
  Serial.begin(115200);
  SerialBT.begin("MILLOSESP32"); // Inicia la comunicación Bluetooth con el nombre "MILLOSESP32"
}

// En el bucle principal, lee los datos enviados por Bluetooth y cambia los estados de las máquinas de estado en función de los datos recibidos
void loop()
{
  if (SerialBT.available() > 0) // Si hay datos disponibles en el Bluetooth
  {
    ml_required = SerialBT.parseInt(); // Lee el número entero enviado por Bluetooth

    // Cambia los estados de las máquinas de estado en función del número recibido
    if (ml_required == -1){
        weighing_CurrentState = STATE_TARING; // Si el número es -1, cambia a estado de tarado
    }
    else if (ml_required == -2){
        weighing_CurrentState = STATE_CALIBRATING; // Si el número es -2, cambia a estado de calibración
    }
    else {
        dispensing_CurrentState = STATE_DISPENSING; // Si el número es cualquier otro, cambia a estado de dispensación
    }
  }
}
