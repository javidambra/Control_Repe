/*
  ====================================================================================================
*** Control Repe ***

  Interfaz controladora para estación repetdora de Radioaficionados
  Realiza la interfaz entre el Rx y el Tx brindando diversas funciones como:
  Cola y bip
  TOT (Time out Tx) - "Antiponcho"
  Identificacion
  Apagado remoto
  Entre otras

  Copyright (C) 2019  Javier D'Ambra

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
  ========================================================================================================
  --------------------------------------------------------------------------------------------------------
*** Archivo de funciones ***
  --------------------------------------------------------------------------------------------------------
*/

void PTTon() {  //funcion que activa el PTT del tx.
  digitalWrite(pinPTT, HIGH);
  digitalWrite(pinFanTx, HIGH);
  return;
}

void PTToff() { //funcion que apaga el PTT del tx.
  digitalWrite(pinPTT, LOW);
  digitalWrite(pinFanTx, LOW);
  return;
}

void cola() {   //función que emite el bip y le da la duración a la cola, luego libera el PTT.
  delay(150);
  tone(pinBip, 440, 200);
  delay(151);
  tone(pinBip, 1000, 300);
  delay(colaTx);
  PTToff();
  return;
}

void timeOut() {  //función que emite un sonido indicado que se alcanzó en TOT y apaga el PTT, deja un retardo.
  tone(pinBip, 1000, 250);
  delay(251);
  tone(pinBip, 440, 250);
  delay(251);
  tone(pinBip, 220, 500);
  delay(500);
  PTToff();
  delay(15000);
  return;
}

//función que chequea si se a cumplido el TOT y en ese caso devuelve verdadero.
//Recibe como parametro si está activa la transmisión.
bool checkTOT(bool tx) {
  static unsigned long millisPrevio = 0;
  unsigned long millisActual = millis();

  if (tx) { //realizamos el chequeo del TOT sólo si hay transmisión activa.
    if (millisActual - millisPrevio >= TOT * 1000UL) {
      millisPrevio = millisActual;    //actualiza el contador ya que se cumplio el TOT
      return true;
    }
    else {
      return false;
    }
  }
  millisPrevio = millisActual; //como no hay transmisión actualiza el contador
  return false;
}

//Funcion que transmite el ID si se cumplen las condiiones establecidas
bool transmitirID(bool tx) {
  static unsigned long millisPrevio = 0;
  unsigned long millisActual = millis();
  static bool txID = false; //flag para cuando se reproduce el ID

  if (tx) { //tx verdadero indica que hubo detección del COR, se retorna manteniendo el txID en el estado actual.
    return txID;
  }
  if (txID) { // chequeamos si se está reproduciendo el ID
    if (!digitalRead(pinFinID)) { //chequeamos si llegó al fin el ID --Activo por bajo--
      digitalWrite(pinID, LOW); //desactiva reprucción del mensaje
      cola(); //reproduce la cola y deja de transmitir
      txID = false;
    }
  }
  if (millisActual - millisPrevio >= intervaloID * 60 * 1000UL) { //se ejecuta si se cumplió el intervalo.
    PTTon();  //Activa el PTT
    digitalWrite(pinID, HIGH); //Activa reprucción del mensaje
    millisPrevio = millisActual; //actualiza el contador
    txID = true;
    return true;  //retorna true porque se transmite el ID
  }
  return false;   //retorna false si hacer nada
}

void isrLeerDTMF(){   //rutina de interupción al detectar señal DTMF
  hayDTMF = true;     //flah llego al menos 1 DTMF.
  if (digitoDTMF >= 5){  //se supero el tamaño del comando DTMF (5 digitos), comienza a sobreescribir
    digitoDTMF = 0;
  }
  comandoDTMF[digitoDTMF] = PINB & 0x0F;    //leemos los 4 bits bajos de puerto B - se debe tener en cuenta si se cambia de placa o asignación de pines.
  digitoDTMF++;
  
}

//Función que chequea si han llegado datos DTMF y realiza las acciones pertinentes
void checkDTMF(){
  static unsigned long millisPrevio = 0;
  unsigned long millisActual = millis();
  if (hayDTMF){
    millisPrevio = millisActual;  //actualizamos contador cuando hay dato DTMF
    hayDTMF = false;              //bajamos flag de datos DTMF
  }
  if (digitoDTMF == 5){ //El comando DTMF está completo, realizamos las tareas correpondientes
    // hacer lo que sea que hay que hacer con el comando DTMF
    // el if se cumple solo si la clave es correcta
    if (comandoDTMF[0]==claveDTMF[0]&&comandoDTMF[1]==claveDTMF[1]&&comandoDTMF[2]==claveDTMF[2]&&comandoDTMF[3]==claveDTMF[3]){
      switch(comandoDTMF[4]) {  //chequemos el ultimo dígito, correspondiente al comando, y realizamos lo que sea
        case 1:
          //comando 1 - encendido del transmisor
          if (!txPwr){  //si el transmisor está apagado, lo encendemos.
            digitalWrite(pinTxPwr, HIGH);
            txPwr = true;
          }
          break;
        case 2:
          //Comando 1 - apagado del transmisor
          if (txPwr){  //si el transmisor está encendido lo apagamos.
            digitalWrite(pinTxPwr, LOW);
            txPwr = false;
          }
          break;
        case 3: 
          //comando 3
          break;
        default:
          //comando no reconocido
          break;
      }
    }
    digitoDTMF = 0;     //reseteamos para empezar un nuevo comando
    
  }
  if (millisActual - millisPrevio >= 2000){  //si se cumplió el tiempo reseateamos contador de digitos, para que no tome secuencias DTMF muy lentas
    digitoDTMF = 0;
    
  }
}

//Función que chequea el estado de carga de la batería
void checkBat(){
  int vBat = 0;
  vBat = map(analogRead(pinVBat), 0, 1023, 0, 150); // medimos la tensión y mapeamos entre 0 (0v) y 150 (15v)
  if (vBat < 100){ //baetia < 10 volts
    //batería muy baja, apagar transmisor
    digitalWrite(pinTxPwr, LOW);
    batBaja = true;
  }
  else if(vBat >= 100 && vBat < 115){ // batería entre 10 y 11,5 volts
    //Batería baja, dar aviso
  }
  else if(vBat > 140){ // batería mas de 14 volts
    //Batería sobrecargada...
  }
  else{  //batería normal
    if(batBaja){  //si había bat. baja, ya no hay y se bebe reactivar.
      batBaja = false;
      if(txPwr){  //si el transmisor no está desactivado, lo volvemos a encender
        digitalWrite(pinTxPwr, HIGH);
      }
    }
    
  }
}

//Función que chequea la temperatura.
void checkTemp(){
  float temperatura = 0;
  static bool fanOn = false;
  // temperatura = leer temperatura;    //implementar lectura segun sensor instalado
  if (temperatura > 40 && fanOn == false && batBaja == false){
    digitalWrite(pinFanRack, HIGH);
    fanOn = true;
  }
  if (temperatura < 35 && fanOn == true){
    digitalWrite(pinFanRack, LOW);
    fanOn = false;
  }
  if (batBaja == true && fanOn == true){
    digitalWrite(pinFanRack, LOW);
    fanOn = false;
  }
}
