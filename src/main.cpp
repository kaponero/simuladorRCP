
#include <Arduino.h>
#include <random>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "GFButton.h"

//Definicion de instancias
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic_Frec = NULL;
//parametros y variables
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned int frecuencia = 100;
int compresion = 0;
int posicion = 0;
bool comprimiendo = true;
long time_notify = 0;
long time_act = 0;
long time_compresion = 0;
int maxima_compresion = 50;
int maximo = 0;
long time_posicion;
float tiempo = 0;
double segundos = 0;
double minutos = 0;
double latidos = 0;
// Usamos el siguiente generador UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID      "0000aa20-0000-1000-8000-00805f9b34fb"
#define FRECUENCIA_UUID   "0000aa21-0000-1000-8000-00805f9b34fb"

//salidas y entradas
#define LED_BLUETOOTH 19
#define LED_ENCENDIDO 18

#define LED_NORMAL 02
#define LED_LENTO 00
#define LED_RAPIDO 15

#define BOTON_MANO 22
#define BOTON_CT 23

GFButton buttonMano(BOTON_MANO,E_GFBUTTON_PULLDOWN);
GFButton buttonCT(BOTON_CT,E_GFBUTTON_PULLDOWN);

//funciones y clases
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(LED_BLUETOOTH, HIGH);
      //encender un led cuando conecta
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(LED_BLUETOOTH, LOW);
      //apagar el led
    }
};

class MycharacteristicCallbacks: public BLECharacteristicCallbacks{
    void onNotify(BLECharacteristic* pCharacteristic){
      //metodo para prender un led por ejemplo
      if ((int)frecuencia < 100){
        digitalWrite(LED_LENTO, HIGH);
        digitalWrite(LED_RAPIDO, LOW);
        digitalWrite(LED_NORMAL, LOW);
      }
        else if ((int)frecuencia < 120){
          digitalWrite(LED_LENTO, LOW);
          digitalWrite(LED_RAPIDO, LOW);
          digitalWrite(LED_NORMAL, HIGH);
        }
        else{
          digitalWrite(LED_LENTO, LOW);
          digitalWrite(LED_RAPIDO, HIGH);
          digitalWrite(LED_NORMAL, LOW);
        }
    };
};


void compresiones_por_minuto(){
  if (buttonCT.wasPressed()){
    latidos++;
    frecuencia = (unsigned int) ((latidos)/(minutos + segundos/60));
    if (frecuencia>160){
      frecuencia = 160;
    }
    //comprimiendo = true;
  }
}

void posicion_de_la_mano (){
  if (buttonMano.isPressed()){
    posicion=1;
  }else
    posicion=0;
}


void profundidad(){
  if (buttonCT.isPressed()){
    if (millis() > time_compresion + 15){
      time_compresion = millis();
      if(compresion < maxima_compresion){
        compresion+=3;
        if (compresion>50){
          compresion=50;
        }
      }
    }
  }else
    maximo = compresion;
    comprimiendo = false;
  if (!comprimiendo){
    if (millis() > time_compresion + 15){
      time_compresion = millis();
      if (compresion > 0){
      compresion-=3;
      }
      else{
      comprimiendo = true;
      }
    }
  }
}

void time(){
  if (millis()>time_posicion + 1000){
    time_posicion = millis();
    segundos++;
    if (segundos == 60){
      segundos = 0;
      minutos++;
    }
  }  
}

void reset_parametros(){
  frecuencia = 0;
  latidos = 0;
  minutos = 0;
  segundos = 0;
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_ENCENDIDO, OUTPUT);
  pinMode(LED_BLUETOOTH, OUTPUT);
  pinMode(LED_NORMAL, OUTPUT);
  pinMode(LED_RAPIDO, OUTPUT);
  pinMode(LED_LENTO, OUTPUT);

  //buttonMano.setDebounceTime(80);

  digitalWrite(LED_ENCENDIDO, HIGH);

  // Se crea el dispositivo BLE
  BLEDevice::init("SimuladorRCP");

  // Se crea el servidor BLE
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Se crea un servicio BLE
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Se crean las caracteristicas, NOTIFY: el cliente sera notificado cuando el valor de la carcteristica cambie
  pCharacteristic_Frec = pService->createCharacteristic(
                      FRECUENCIA_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic_Frec->setCallbacks(new MycharacteristicCallbacks());

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Crear un Descriptor
  pCharacteristic_Frec->addDescriptor(new BLE2902());

  // Iniciamos el servicio
  pService->start();

  // Se crea el advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Esperando conexion con un cliente para notificar...");

  time_act = millis();
  time_compresion = millis();
}

void loop() {

  // notifica cuando el valor cambia
  if (deviceConnected) {
    if(millis()>time_notify + 3){
      
        time();
        compresiones_por_minuto();
        profundidad();
        posicion_de_la_mano();

      time_notify =millis(); 
      std::string valor;
      valor.push_back((char)frecuencia);
      valor.push_back((char)compresion);
      valor.push_back((char)posicion);

      pCharacteristic_Frec->setValue(valor);
      pCharacteristic_Frec->notify();       
      // bluetooth se congestionará, si se envían demasiados paquetes, en la prueba de 6 horas pude llegar a 3 ms
    }
  }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // darle al bluetooth la oportunidad de preparar las cosas
        pServer->startAdvertising(); // reiniciar la publicidad
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;

        digitalWrite(LED_LENTO, LOW);
        digitalWrite(LED_RAPIDO, LOW);
        digitalWrite(LED_NORMAL, LOW);
        reset_parametros();        
    }
    // conectando
    if (deviceConnected && !oldDeviceConnected) {
        //hacer cosas aquí en la conexión
        oldDeviceConnected = deviceConnected;
        time_notify = millis(); 
    }
}
