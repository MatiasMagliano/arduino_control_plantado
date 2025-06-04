/*
 * Control Automatizado Acuario Plantado - Versión mejorada
 * AUTOR: Matías Magliano - magliano.matias@gmail.com
 *
 * - Reloj DS3231 con ajuste desde monitor serial
 * - Control de iluminación por canales (RGB + Crecimiento + Luna)
 * - Cambio de horarios estacionales cada tres meses
 * - WatchdogTimer con WDTO_1S
 *
 * Basado en la librería DS3231 de Seeed-Studio
 * https://github.com/NorthernWidget/DS3231
 * Licencia MIT
 */

#include <Wire.h>
#include <DS3231.h>
#include <avr/wdt.h>

DS3231 rtc(SDA, SCL);

const byte rele_4 = 2;
const byte rele_3 = 3;
const byte rele_2 = 7;
const byte rele_1 = 8;
const byte canalRojo = 9;
const byte canalVerde = 10;
const byte canalAzul = 11;
const byte canalCrecimiento = 5;
const byte canalLuna = 6;

struct HorariosEstacionales {
  int comienzoRojo;
  int comienzoVerde;
  int comienzoAzul;
  int comienzoCrecimiento;
  int comienzoLuna;
  int netoRojo;
  int netoVerde;
  int netoAzul;
  int netoCrecimiento;
  int netoLuna;
};

HorariosEstacionales horarios[4] = {
  {420, 435, 405, 510, 1230, 780, 765, 750, 450, 360},
  {450, 465, 435, 540, 1230, 750, 735, 720, 420, 360},
  {480, 495, 465, 570, 1230, 720, 705, 690, 390, 360},
  {435, 450, 420, 525, 1230, 765, 750, 735, 435, 360}
};

int comienzoCanalRojo, comienzoCanalVerde, comienzoCanalAzul, comienzoCanalCrecimiento, comienzoCanalLuna;
int netoCanalRojo, netoCanalVerde, netoCanalAzul, netoCanalCrecimiento, netoCanalLuna;

const int duracionRampa = 60;
const byte maxCanalRojo = 254;
const byte maxCanalVerde = 200;
const byte maxCanalAzul = 254;
const byte maxCanalCrecimiento = 254;
const byte maxCanalLuna = 127;

int contadorMinutos = 0;
int pSegundo = 0;

void setLed(int minutos, byte pin, int comienzo, int neto, int rampa, byte maximo) {
  byte val = 0;
  if (minutos <= comienzo || minutos > comienzo + neto) val = 0;
  else if (minutos <= comienzo + rampa) val = map(minutos - comienzo, 0, rampa, 0, maximo);
  else if (minutos <= comienzo + neto - rampa) val = maximo;
  else val = map(minutos - comienzo - neto + rampa, 0, rampa, maximo, 0);
  analogWrite(pin, val);
}

void update_leds() {
  setLed(contadorMinutos, canalRojo, comienzoCanalRojo, netoCanalRojo, duracionRampa, maxCanalRojo);
  setLed(contadorMinutos, canalVerde, comienzoCanalVerde, netoCanalVerde, duracionRampa, maxCanalVerde);
  setLed(contadorMinutos, canalAzul, comienzoCanalAzul, netoCanalAzul, duracionRampa, maxCanalAzul);
  setLed(contadorMinutos, canalCrecimiento, comienzoCanalCrecimiento, netoCanalCrecimiento, duracionRampa, maxCanalCrecimiento);
  setLed(contadorMinutos, canalLuna, comienzoCanalLuna, netoCanalLuna, duracionRampa, maxCanalLuna);
}

void bombas() {
  if ((contadorMinutos > 570 && contadorMinutos < 749) || (contadorMinutos > 960 && contadorMinutos < 1079)) {
    if ((contadorMinutos % 60 - 30) < 0) {
      digitalWrite(rele_4, HIGH); delay(50);
      digitalWrite(rele_3, LOW); delay(50);
    } else {
      digitalWrite(rele_4, LOW); delay(50);
      digitalWrite(rele_3, HIGH); delay(50);
    }
  } else {
    digitalWrite(rele_4, LOW);
    digitalWrite(rele_3, LOW);
  }
}

void filtro() {
  if (contadorMinutos > 540 && contadorMinutos < 1080) {
    if ((contadorMinutos % 120 - 60) < 0) {
      digitalWrite(rele_2, HIGH); delay(50);
      digitalWrite(rele_1, HIGH); delay(50);
    } else {
      digitalWrite(rele_2, LOW); delay(50);
      digitalWrite(rele_1, LOW); delay(50);
    }
  } else {
    digitalWrite(rele_2, LOW);
    digitalWrite(rele_1, LOW);
  }
}

void ajustarHorariosPorEstacion(byte mes) {
  HorariosEstacionales h = horarios[(mes - 1) / 3];
  comienzoCanalRojo = h.comienzoRojo;
  comienzoCanalVerde = h.comienzoVerde;
  comienzoCanalAzul = h.comienzoAzul;
  comienzoCanalCrecimiento = h.comienzoCrecimiento;
  comienzoCanalLuna = h.comienzoLuna;
  netoCanalRojo = h.netoRojo;
  netoCanalVerde = h.netoVerde;
  netoCanalAzul = h.netoAzul;
  netoCanalCrecimiento = h.netoCrecimiento;
  netoCanalLuna = h.netoLuna;
}

void checkSerialForTimeUpdate() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    int y, mo, d, h, mi, s;
    if (sscanf(input.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
      rtc.setDOW(d);
      rtc.setTime((uint8_t)h, (uint8_t)mi, (uint8_t)s);
      rtc.setDate((uint8_t)d, (uint8_t)mo, (uint16_t)y);
      Serial.println("RTC actualizado correctamente.");
    } else {
      Serial.println("Formato incorrecto. Usa: YYYY-MM-DD HH:MM:SS");
    }
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();

  pinMode(canalRojo, OUTPUT);
  pinMode(canalVerde, OUTPUT);
  pinMode(canalAzul, OUTPUT);
  pinMode(canalCrecimiento, OUTPUT);
  pinMode(canalLuna, OUTPUT);
  pinMode(rele_1, OUTPUT);
  pinMode(rele_2, OUTPUT);
  pinMode(rele_3, OUTPUT);
  pinMode(rele_4, OUTPUT);

  digitalWrite(rele_1, LOW);
  digitalWrite(rele_2, LOW);
  digitalWrite(rele_3, LOW);
  digitalWrite(rele_4, LOW);

  delay(2000);
  wdt_enable(WDTO_1S);
}

void loop() {
  wdt_reset();
  checkSerialForTimeUpdate();

  int hora = rtc.getHour(true);
  int minuto = rtc.getMinute();
  int segundo = rtc.getSecond();
  int mes = rtc.getMonth();

  ajustarHorariosPorEstacion(mes);
  contadorMinutos = hora * 60 + minuto;

  if (segundo != pSegundo) {
    pSegundo = segundo;
    update_leds();
  }

  bombas();
  filtro();
  delay(500);
}
