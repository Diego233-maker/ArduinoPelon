/*
  Control de temperatura de lazo cerrado - TINKERCAD - version POR SOFTWARE (para pruebas)

  Aqui el Arduino SI decide y activa las salidas (no hay comparadores):
    D8  -> 1k -> base del 2N2222 -> bobina del rele K1 -> foco     (T < 18 C)
    D11 -> 1k -> base del TIP120 -> motor del ventilador            (T > 22 C)
    Entre 18 y 22 C ambos quedan apagados.

  Conexiones:
    A0  <- Vsig (salida del op-amp acondicionador, o directo del TMP36 si USAR_ACONDICIONADOR = false)
    LCD 16 x 2 (I2C):  SDA -> A4,  SCL -> A5,  VCC -> 5V,  GND -> GND
    GND comun con todo el circuito

  Esta version es solo para probar la parte de potencia. El diseno final usa
  comparadores analogicos y el Arduino solo lee y muestra.
*/

#include <Adafruit_LiquidCrystal.h>

// ---------- Pines ----------
const uint8_t PIN_TEMP    = A0;
const uint8_t PIN_FOCO    = 8;     // activa el rele del foco
const uint8_t PIN_VENT    = 11;    // activa el TIP120 del ventilador

// ---------- Sensor y acondicionador ----------
const bool  USAR_TMP36           = true;    // true = TMP36 (Tinkercad), false = LM35
const bool  USAR_ACONDICIONADOR  = true;    // false = TMP36 conectado directo a A0 (ganancia 1)
const float RF_OHMS              = 10000.0;
const float RG_OHMS              = 10000.0;
const float VREF                 = 5.0;

// ---------- Umbrales ----------
const float T_ALTA_C   = 22.0;    // arriba de esto: ventilador
const float T_BAJA_C   = 18.0;    // abajo de esto: foco
const float HISTERESIS = 0.5;     // evita que las salidas oscilen en el umbral

// ---------- Tiempos ----------
const unsigned long PERIODO_MS = 500;
const uint8_t MUESTRAS_ADC     = 16;

// ---------- Estados ----------
const uint8_t ESTADO_NORMAL = 0;
const uint8_t ESTADO_ALTA   = 1;   // ventilador encendido
const uint8_t ESTADO_BAJA   = 2;   // foco encendido

// ---------- LCD I2C de Tinkercad ----------
Adafruit_LiquidCrystal lcd(0);

unsigned long ultimaActualizacion = 0;
uint8_t estadoActual = ESTADO_NORMAL;

float leerTemperaturaC() {
  long suma = 0;
  for (uint8_t i = 0; i < MUESTRAS_ADC; i++) {
    suma += analogRead(PIN_TEMP);
    delayMicroseconds(200);
  }
  float adc      = suma / (float)MUESTRAS_ADC;
  float vsig     = adc * VREF / 1023.0;
  float ganancia = USAR_ACONDICIONADOR ? (1.0 + RF_OHMS / RG_OHMS) : 1.0;
  float vsensor  = vsig / ganancia;

  if (USAR_TMP36) {
    return (vsensor - 0.5) * 100.0;      // TMP36: 500 mV a 0 C, 10 mV/C
  }
  return vsensor * 100.0;                // LM35: 10 mV/C
}

// Maquina de estados con histeresis
uint8_t actualizarEstado(float tempC, uint8_t estado) {
  if (estado == ESTADO_ALTA) {
    if (tempC < T_ALTA_C - HISTERESIS) return ESTADO_NORMAL;
    return ESTADO_ALTA;
  }
  if (estado == ESTADO_BAJA) {
    if (tempC > T_BAJA_C + HISTERESIS) return ESTADO_NORMAL;
    return ESTADO_BAJA;
  }
  if (tempC > T_ALTA_C) return ESTADO_ALTA;
  if (tempC < T_BAJA_C) return ESTADO_BAJA;
  return ESTADO_NORMAL;
}

void aplicarSalidas(uint8_t estado) {
  digitalWrite(PIN_VENT, estado == ESTADO_ALTA ? HIGH : LOW);
  digitalWrite(PIN_FOCO, estado == ESTADO_BAJA ? HIGH : LOW);
}

const char* textoEstado(uint8_t estado) {
  switch (estado) {
    case ESTADO_ALTA: return "ALTA  ";
    case ESTADO_BAJA: return "BAJA  ";
    default:          return "NORMAL";
  }
}

void mostrar(float tempC, uint8_t estado) {
  char linea[17];
  char numero[8];

  // Linea 1: "Alta:22  Baja:18"
  snprintf(linea, sizeof(linea), "Alta:%-2d  Baja:%-2d", (int)T_ALTA_C, (int)T_BAJA_C);
  lcd.setCursor(0, 0);
  lcd.print(linea);

  // Linea 2: "Amb:21.5C NORMAL"
  dtostrf(tempC, 4, 1, numero);
  snprintf(linea, sizeof(linea), "Amb:%sC %s", numero, textoEstado(estado));
  lcd.setCursor(0, 1);
  lcd.print(linea);
}

void setup() {
  pinMode(PIN_FOCO, OUTPUT);
  pinMode(PIN_VENT, OUTPUT);
  digitalWrite(PIN_FOCO, LOW);
  digitalWrite(PIN_VENT, LOW);

  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Control de temp.");
  lcd.setCursor(0, 1);
  lcd.print("Modo software");
  delay(1500);
  lcd.clear();
}

void loop() {
  if (millis() - ultimaActualizacion >= PERIODO_MS) {
    ultimaActualizacion = millis();

    float tempC = leerTemperaturaC();
    estadoActual = actualizarEstado(tempC, estadoActual);
    aplicarSalidas(estadoActual);
    mostrar(tempC, estadoActual);

    Serial.print("T=");
    Serial.print(tempC, 1);
    Serial.print(" C  D8(foco)=");
    Serial.print(digitalRead(PIN_FOCO));
    Serial.print("  D11(vent)=");
    Serial.print(digitalRead(PIN_VENT));
    Serial.print("  estado=");
    Serial.println(textoEstado(estadoActual));
  }
}
