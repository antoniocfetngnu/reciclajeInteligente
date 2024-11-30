#include <Servo.h>

Servo miServo;
int posicionCentro = 50;    // Posición neutra o central
int posicionIzquierda = 0;  // Posición de 0 grados
int posicionDerecha = 120;

int servoPin = 3;
// Pines primer sensor
int TRIG = 8;
int ECHO = 9;
// Pines del segundo sensor ultrasónico
int segundoTRIG = 10;
int segundoECHO = 11;
// Tercer Sensor
int tercerTRIG = 12;
int tercerECHO = 13;

int distanciaPrimerSensor;
int distanciaSegundoSensor;
int distanciaTercerSensor;

bool flack1 = false;
bool flack2 = false;
bool flack3 = false;
bool flackAlto = false;
bool flackBajo = false;

bool LLenado1 = false;
bool Llenado2 = false;


int contador = 0;
int contador3 = 0;
int contadorAlto = 0;
int contadorBajo = 0;
// Contadores para validación
int contadorPrimerSensor = 0;
int contadorSegundoSensor = 0;

int intervalo = 1000;  // Tiempo en milisegundos para regresar al centro

void setup() {
  miServo.attach(servoPin);       // Conectar el servo al pin definido
  miServo.write(posicionCentro);  // Posicionar el servo en el centro
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(segundoTRIG, OUTPUT);  // Configuración del segundo TRIG
  pinMode(segundoECHO, INPUT);   // Configuración del segundo ECHO
  pinMode(tercerTRIG, OUTPUT);   // Configuración del TRIG del tercer sensor
  pinMode(tercerECHO, INPUT);
  Llenado2 = false;

  Serial.begin(115200);  // Inicia la comunicación serial con el ESP32
  Serial.println("Arduino listo para comunicarse.");
}

void loop() {


  // Leer el segundo sensor de distancia

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duracionPrimerSensor = pulseIn(ECHO, HIGH);
  distanciaPrimerSensor = duracionPrimerSensor / 58.2;
  Serial.print("PrimerSensor:");
  Serial.println(distanciaPrimerSensor);
  delay(100);


  digitalWrite(segundoTRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(segundoTRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(segundoTRIG, LOW);
  long duracionSegundoSensor = pulseIn(segundoECHO, HIGH);
  distanciaSegundoSensor = duracionSegundoSensor / 58.2;
  Serial.println(distanciaSegundoSensor);


  // Validar el rango del segundo sensor
  if (distanciaSegundoSensor > 19 && distanciaSegundoSensor < 24) {



    digitalWrite(tercerTRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(tercerTRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(tercerTRIG, LOW);
    long duracionTercerSensor = pulseIn(tercerECHO, HIGH);
    distanciaTercerSensor = duracionTercerSensor / 58.2;
    Serial.print("Distancia tercer sensor: ");
    Serial.println(distanciaTercerSensor);

    if (LLenado1) {
      if (distanciaPrimerSensor > 18 && distanciaPrimerSensor < 24) {
        flack2 = true;
        flack3 = false;
      }
    }
    if (flack2) {
      contador++;
      if (contador > 6) {
        if (distanciaPrimerSensor > 18 && distanciaPrimerSensor < 24) {
          LLenado1 = false;
          contador = 0;
          Serial.println("mitadfalse");
          flack2 = false;
        }
      }
    }

    if (!LLenado1) {
      if (distanciaPrimerSensor < 18 || distanciaPrimerSensor > 24) {
        flack3 = true;
        flack2 = false;
      }
    }
    if (flack3) {
      contador3++;
      if (contador3 > 6) {
        if (distanciaPrimerSensor < 18 || distanciaPrimerSensor > 24) {
          Serial.println("mitadtrue");
          contador3 = 0;
          LLenado1 = true;
          flack3 = false;
        }
      }
    }



    // Evaluar condiciones para enviar el mensaje al ESP32
    if (distanciaTercerSensor > 6 || distanciaTercerSensor < 0) {
      flack1 = true;
      delay(300);
    }
    if (flack1) {
      if (distanciaTercerSensor <= 5) {
        delay(300);
        Serial.println("true");  // Enviar el mensaje al ESP32
        flack1 = false;
      }
    }

    // Verificar si hay datos del ESP32 en el puerto serie
    if (Serial.available()) {
      String mensajeESP32 = Serial.readStringUntil('\n');  // Leer el mensaje del ESP32
      mensajeESP32.trim();                                 // Limpiar caracteres adicionales
      if (mensajeESP32.length() > 0) {                     // Validar que el mensaje no esté vacío
        Serial.print("Mensaje del ESP32: ");
        Serial.println(mensajeESP32);

        // Control del servo según el mensaje recibido
        if (mensajeESP32 == "botella") {
          miServo.write(posicionDerecha);
        } else if (mensajeESP32 == "papel") {
          miServo.write(posicionIzquierda);
        } else if (mensajeESP32 == "sin accion") {
          miServo.write(posicionCentro);
        }
        delay(intervalo);               // Tiempo para regresar al centro
        miServo.write(posicionCentro);  // Volver el servo a la posición central
      }
    }
  }
  
  if (Llenado2) {
    if (distanciaSegundoSensor > 18 && distanciaSegundoSensor < 24) {
      flackAlto = true;
      flackBajo = false;
    }
  }
  if (flackAlto) {
    contadorAlto++;
    if (contadorAlto > 6) {
      if (distanciaSegundoSensor > 18 && distanciaSegundoSensor < 24) {
        Llenado2 = false;
        contadorAlto = 0;
        Serial.println("llenofalse");
        flackAlto = false;
      }
    }
  }

  if (!Llenado2) {
    if (distanciaSegundoSensor < 18 || distanciaSegundoSensor > 24) {
      flackBajo = true;
      flackAlto = false;
    }
  }
  if (flackBajo) {
    contadorBajo++;
    if (contadorBajo > 6) {
      if (distanciaSegundoSensor < 18 || distanciaSegundoSensor > 24) {
        Serial.println("llenotrue");
        contadorBajo = 0;
        Llenado2 = true;
        flackBajo = false;
      }
    }
  }


  delay(300);  // Esperar antes de la siguiente iteración del loop
}