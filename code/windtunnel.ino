// Develope by: Óscar Marbán Lucas
// Libreria para el manejo del sensor de temperatura y humedad
#include <DHT.h>
// Libreria para el manejo de la pantalla LCD
#include <LiquidCrystal.h>

// Tipo de sensor de temperatura y humedad
#define DHT_TYPE DHT11

// Uso de pines digitales - 2 3 soportan interrupciones
#define DHT_PIN 2     // pin para lectura del sensor de temperatura y humedad
#define BUTTON_PIN 3  // pin para lectura del push button
#define D7_PIN 4      // pin LCD para datos D7
#define D6_PIN 5      // pin LCD para datos D6
#define MOSFET_PIN 6  // pin para activacion de la señal del MOSFET para activar la fuente 12V
#define D5_PIN 7      // pin LCD para datos D5
#define D4_PIN 8      // pin LCD para datos D4
#define RS_PIN 12     // pin LCD para RS - Registry select
#define EN_PIN 11     // pin LCD para EN - enable

// Uso de pines analogicos
#define ANE_PIN A0

// declaracion de constantes
#define resistencia100 100000.0  //Resistencia de 100K
#define resistencia10 10000.0    //Resistencia de 10k
#define COLS 16                  // Filas y columnas de la pantalla LCD
#define ROWS 2
#define NUM_MUESTRAS 50  // numero de muestras por lectura
#define TAM 10           // capacidad del vector de voltajes

// declaracion de variables
// variables para la gestion de la señal del MOSFET, activacion de la fuente de 12 V
byte motorState = LOW;  // estado del motor, incialmente apagado

// Variables para el calculo de la velocidad del anemometro
int sum = 0;                       // sum of samples taken para medir voltaje en el disor de voltaje
unsigned char numeroMuestras = 0;  // current sample number
float voltaje = 0.0;               // Voltaje calculado en la entrada del dividor de voltaje, procedente del motor del anemometro
float voltaje_final;
float voltajes[TAM];  // coleccion de valores de voltajes para calculo de medias

boolean inicio = false;  // control de arranque para inicio del vector de voltajes. Primera vez, se rellena con el mismo valor

// declaracion de la variable para el manejo de la pantalla LCD
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);

// declaracion de la variable para el manejo del sensor de temperatua y humedad
DHT dht(DHT_PIN, DHT_TYPE);

// dibujo de cara sonriente
byte smiley[8] = {
  0b00000,
  0b00000,
  0b01010,
  0b00000,
  0b00000,
  0b10001,
  0b01110,
  0b00000
};

// las variables utilizadas en las interrupciones deben ser volalite
volatile byte buttonReleased = false;

// Funcion para tratamiento de la interrupcion al pulsar el push button
void buttonReleasedInterrupt() {
  buttonReleased = true;
}

// funcion que calcula la media de los valores de un array v de tamaño capacidad
float calcularMedia(float v[], int capacidad) {
  float suma = 0;
  for (int i = 0; i < capacidad; i++)
    suma += v[i];

  return suma / (float)capacidad;
}

// funcion que desplaza a la izquierda todos los elementos del vector v de tamaño capacidad, saca el primero e inserta elemento en la ultima posicion del vector c
void insertarFinal(float v[], int capacidad, int valor) {
  for (int i = 0; i < capacidad - 1; i++)
    v[i] = v[i + 1];
  v[capacidad - 1] = valor;
}

void setup() {
  // inicializacion de pin para control fuente 12V, inicialmente apagado
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, motorState);

  // inicializacion de pin para control del push button
  pinMode(BUTTON_PIN, INPUT);
  // interrupcion para el manejo del push button
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN),
                  buttonReleasedInterrupt,
                  FALLING);

  // inicializacion de pin para lectura de datos del anemometro
  pinMode(ANE_PIN, INPUT);

  // inicializacion pantalla LCD
  lcd.begin(COLS, ROWS);

  // inicializacion del sensor de temperatura y humedad
  dht.begin();

  // inicializacion puerto serie para depuracion
  Serial.begin(9600);

  // inicializacion pines digitales a lavlor INTERNAL para referencia de lectura
  analogReference(INTERNAL);

  // incializa en el lcd el caracter carita sonriente
  lcd.createChar(0, smiley);
}

void loop() {

  // si el tunel esta encendido
  if (motorState == HIGH) {
    // lectura de datos del sensor de temperatura y humedad
    float humidity = dht.readHumidity();
    float temp = dht.readTemperature();

    // escritura de los datos de temperatura y humedad en la pantalla LCD
    lcd.setCursor(0, 0);
    lcd.print("T: ");
    lcd.setCursor(2, 0);
    if (isnan(temp))  // si no se puede leer temperatura muestra -
      lcd.print("-");
    else {
      lcd.print(temp);
      lcd.setCursor(6, 0);
      lcd.print((char)223);  // simbolo de grado centigrado
      lcd.setCursor(7, 0);
      lcd.print("C");
    }

    lcd.setCursor(9, 0);
    lcd.print("H:");
    lcd.setCursor(11, 0);
    if (isnan(humidity))
      lcd.print("-");
    else {
      lcd.print(humidity);  // si no se puede leer humedad muestra -
      lcd.setCursor(15, 0);
      lcd.print("%");
    }

    // lectura de muestras analogicas de datos del anemometro
    sum = 0;
    numeroMuestras = 0;
    while (numeroMuestras < NUM_MUESTRAS) {
      sum += analogRead(ANE_PIN);
      numeroMuestras++;
      delay(10);
    }

    voltaje = ((((float)sum / (float)NUM_MUESTRAS)) * INTERNAL) / 1024.0;  // /1024 de 2^10 que son los valores del conversor analogico digital
    voltaje_final = voltaje * ((resistencia100 + resistencia10) / resistencia10);  // divisor de voltaje
    voltaje_final = voltaje_final * 1000;                                          // se multiplica por 1000 porque el voltaje es bajo, se muestra en milivoltios

    if (!inicio) {  // si el vector de voltajes no tiene valores
      // inicializar el vector de voltajes con el primer valor leido
      for (int i = 0; i < TAM; i++)
        voltajes[i] = voltaje_final;
      inicio = true;  // vector ya con valores
    } else {
      if (voltaje_final != 0)  // si el valor leido es 0 no se inserta para no distorsionar la muestra
        voltaje_final = constrain(voltaje_final, 10.0, 20.0);
        insertarFinal(voltajes, TAM, voltaje_final);
    }

    voltaje_final = calcularMedia(voltajes, TAM) * 0.1533;  // calcula la media de los valores del vector de voltajes 0.1533 es el valor de la recta de la regresion lineal  de calibracion

    
    // imprime el voltaje final o la velocidad del viento en la pantalla
    lcd.setCursor(0, 1);
    lcd.print("V: ");
    lcd.setCursor(2, 1);
    lcd.print(voltaje_final);  // multiplicamos por el factor de calibracion del anemometro con el anemometro real
    lcd.setCursor(7, 1);
    lcd.print("m/s");
  } else {  // si el tunel esta en espera, se muestra un mensaje de espera
    lcd.setCursor(0, 0);
    lcd.print("Waiting ...");
    lcd.setCursor(15, 0);
    lcd.write(byte(0));
    lcd.setCursor(0, 1);
    lcd.print("Wind tunnel  OML");
  }
  if (buttonReleased) {  // al presioanr el boton, se cambia el estado, de activo a inactivo y viceversa
    buttonReleased = false;
    inicio = false;
    motorState = (motorState == HIGH) ? LOW : HIGH;
    lcd.clear();
    digitalWrite(MOSFET_PIN, motorState);
  }
}
