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

--------------------------------------------------------------------------------------------------------
========================================================================================================
*** Conexiones de hardware / Uso de pines ***

========================================================================================================
*/

//Librerías utilizadas


//Declaraciones de etiquetas para los pines utilizados
//Entradas
const int pinCOR = 3;        //Señal de recepción activa (COR).
const int pinDTMF_IRQ = 2;   //Señal que indica que hay datos DTMF para leer. //para placas basadas en atmega328
const int pinDTMFData0 = 8;  // |
const int pinDTMFData1 = 9;  // |Datos DTMF, corresponden al los 4 bits bajos de puerto B del Atmega.
const int pinDTMFData2 = 10; // |
const int pinDTMFData3 = 11; // |
const int pinVBat = A0;      //Señal para sensar voltaje de batería.
const int pinSensTemp = A1;  //Señal para sensar temperatura.
const int pinFinID = A2;     //señal que indica el fin del ID
const int pinAuxIn1 = A3;    //Reservado para futuro
const int pinAuxIn2 = A4;    //Reservado para futuro
// Salidas
const int pinPTT = 4;       //Salida para accionar PTT del TX.
const int pinBip = 5;       //Salida para emitir en roger bip y otros sonidos de retorno (audio).
const int pinID = 6;        //Salida para disparar la reproducción del ID.
const int pinTxPwr = 7;     //Salida para apagar y encender el transmisor.
const int pinFanTx = 12;    //Salida para activar ventilador en el tx.
const int pinFanRack = 13;  //Salida para activar ventilador del gabinete
const int pinAuxOut1 = A5;  //Reservado para futuro
const int pinAuxOut2 = A6;  //Reservado para futuro//No disponible en UNO, si en nano o pro mini

//Declaraciones de etiquetas para las constantes

const int TOT = 60;   //duración del "Time Out Tx" (anti-poncho) en segundos.
const int intervaloID = 15; //duración del intervalo entre reproduccióndes del ID en minutos.
const int colaTx = 1000;     //duración de la cola del transmisor en milisegundos.

//Declaraciones de variables globales
volatile byte comandoDTMF[5] = {0}; //vector para recibir el comando (clave + comando) de control DTMF.
volatile byte digitoDTMF = 0;       //variable para definir el indice del vector anterior.
volatile bool hayDTMF = false;      //flag para saber si hay DTMF.
byte claveDTMF[4] = {1, 2, 3, 4}; //vector con clave de comandos de control DTMF
bool txPwr = true;   // indica si el transmisor esta alimentado - pasarlo a eeprom
bool batBaja = false; //valor verdadero indica que la batería esta pelgrosamente ba

void setup() {

  Serial.begin(9600);

  //Entradas
  pinMode(pinCOR, INPUT_PULLUP);
  pinMode(pinDTMF_IRQ, INPUT_PULLUP);
  pinMode(pinDTMFData0, INPUT_PULLUP);
  pinMode(pinDTMFData1, INPUT_PULLUP);
  pinMode(pinDTMFData2, INPUT_PULLUP);
  pinMode(pinDTMFData3, INPUT_PULLUP);
  pinMode(pinFinID, INPUT_PULLUP);
  // pinMode(pinSensTemp, INPUT);     //Activar solo si se usa sensor de temp digital.
  pinMode(pinAuxIn1, INPUT_PULLUP);   //Desactivar si se necesita usar analógico.
  pinMode(pinAuxIn2, INPUT_PULLUP);  //Desactivar si se necesita usar analógico.

  //Salidas
  pinMode(pinPTT, OUTPUT);
  pinMode(pinBip, OUTPUT);
  pinMode(pinID, OUTPUT);
  pinMode(pinTxPwr, OUTPUT);
  pinMode(pinFanTx, OUTPUT);
  pinMode(pinFanRack, OUTPUT);
  pinMode(pinAuxOut1, OUTPUT);
  pinMode(pinAuxOut2, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(pinDTMF_IRQ), isrLeerDTMF, FALLING); //activamos interrupcion para leer DTMF - usar activo bajo
  
  if (txPwr){  //colocamos el transmisor como se encontraba anteriormente
    digitalWrite(pinTxPwr, HIGH);
  }
  else{
    digitalWrite(pinTxPwr, LOW);
  }
  
}

void loop() { 
  bool COR = false;             //indica que hay recepción presente.
  static bool tx = false;       //indica si la transmisión está activa.
  static bool timeOutTx = false;  //Indica si se alcanzó el timeout de transmisión.
  static bool txID = false;      //indica que se está transmitiendo el ID.

  COR = !digitalRead(pinCOR);   //Leemos el COR(si hay rx presente) (lo invertimos porque por hardware es activo bajo)

  if (COR && !timeOutTx) {        //Ejecuta si el COR está activo y no se cumpió el timeout
    if (!tx) {                    //solo si no esta activa la transmisión la acticva
      PTTon();
      tx = true;
    }
  }
  else if (timeOutTx) {           //Ejecuta si se ha cumplido el timeout
    timeOut();
    tx = false;    
  }
  else if (txID){               //está en proceso la transmisión del ID ****REVISAR ESTO**** NO SE CUMPLIRIA ESTA CONDICION SI HAY COR ****
    tx = false;                 //ponemos en falso el flag TX en caso que hubiera deteccion de COR durante el ID
  }
  else {                        //Ejecuta si no hay COR ni se cumplió el timeout
    if (tx) {
      cola();
      tx = false;      
    }
  }
  
  timeOutTx = checkTOT(tx);   //chequea (en la funcion) si se cumplió el TOT y lo pone en la variable.
  txID = transmitirID(tx);    //si se cumpló el tiempo (chequea en la función) transmite el ID.
  checkDTMF();                //Chequeamos si ha llegado un comando DTMF válido.
  checkBat();                 //Chequeamos el nivel de carga de batería.
  checkTemp();                //Chequeamos temperatura.
}
