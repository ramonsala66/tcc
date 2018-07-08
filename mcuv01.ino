#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include "DHT.h"


// Update these with values suitable for your network.
const char* ssid = "Temer";
const char* password = "2444666668888888";
const char* mqtt_server = "m23.cloudmqtt.com";
#define mqtt_port 15474
#define MQTT_USER "node"
#define MQTT_PASSWORD "node"
#define MQTT_TOPIC_TEMP "publico/temp"
#define MQTT_TOPIC_HUMID "publico/umid"
#define MQTT_TOPIC_HUMIDAIR "publico/umidair"
#define MQTT_TOPIC_LUX "publico/lux"
#define DHTPIN 6     // em qual porta o DHT vai funcionar
#define DHTTYPE DHT22   // especificando o modelo DHT utilizado

Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345); //inicializando o TSL no endereço padrão (0x39)
DHT dht(DHTPIN, DHTTYPE); //criamos objeto DHT
int relay = 4; //definido o pino 4 para o acionamento do relé da bomba d'agua

String temp_str;
String lum_str;
String umis_str;
String umia_str;    //inicializando variáveis String e char para manipulação dos dados e envio ao servidor
char temperatura[50];
char luminosidade[50];
char umidadeAr[50];
char umidadeSolo[50];

WiFiClient wifiClient;

PubSubClient client(wifiClient);

//Função para leitura de umidade do solo
//Parâmetros: nenhum
//Retorno: umidade de 0 a 100
//Observação: o A0 do NodeMCU permite até, no máximo, 3.3V. Dessa forma,
//            para 3.3V, temos 978 como leitura de A0
float FazLeituraUmidadeSolo(void)
{
    int ValorA0;
    float UmidadePercent;
 
     ValorA0 = analogRead(0);   //978 -> 3,3V (usando porta A0)
     Serial.print("[Leitura A0] ");
     Serial.println(ValorA0);

     UmidadePercent = 100 * ((978-(float)ValorA0) / 978);
     Serial.print("[Umidade Percentual] ");
     Serial.print(UmidadePercent);
     Serial.println("%");
 
     return UmidadePercent;
}

//Função para leitura de temperatura ambiente
//Parâmetros: nenhum
//Retorno: temperatura em Celsius (float)
//Observação: nenhuma
float FazLeituraDHTemp(void)
{
    // Faz a leitura em Celsius
    float t = dht.readTemperature();

    return t;    
}

//Função para leitura de umidade do ar
//Parâmetros: nenhum
//Retorno: umidade em percentual
//Observação: nenhuma
float FazLeituraDHTUmid(void)
{
    float h = dht.readHumidity();

    return h;    
}

//Função para realizar a configuração e conexão ao WiFi
//Parâmetros: nenhum
//Retorno: nenhum
//Observação: O nome da rede e senha foram inicializados no começo do código.
void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    client.publish("publico/in", "hello world");
    // Create a random client ID
    String clientId = "NodeMCUClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD)) {
      Serial.println("connected.");
      //Once connected, publish an announcement...
      client.publish("publico/in", "connected to broker");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);// Set time out for setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  reconnect();
  if(!tsl.begin())
  {
    Serial.print("No TSL2561 detected");
    while(1);
  }
  tsl.enableAutoRange(true);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
  pinMode(relay, OUTPUT);

}

void loop() {
   client.loop();

// faremos a leitura do evento TSL2561 agora
   /* Get a new sensor event */ 
   sensors_event_t event;
   tsl.getEvent(&event);
   /* REALIZAÇÃO DAS LEITURAS COM OS SENSORES*/
   float fluminosidade = event.light;
   float ftemperatura = FazLeituraDHTemp();
   float fumidadeSolo = FazLeituraUmidadeSolo();
   float fumidadeAr = FazLeituraDHTUmid();
   /*ACIONAMENTO DA BOMBA*/
   digitalWrite(relay, LOW);  //Liga relé e bomba por alguns segundos
   delay(4000); //tempo de rega
   digitalWrite(relay, HIGH); //Desliga relé e bomba

   /*TROCANDO O TIPO DE DADO PARA ENVIO AO MQTT*/
   temp_str = String(ftemperatura); //converting ftemp (the float variable above) to a string 
   temp_str.toCharArray(temperatura, temp_str.length() + 1); //packaging up the data to publish to mqtt whoa...

   lum_str = String(fluminosidade); 
   lum_str.toCharArray(luminosidade, lum_str.length() + 1);

   umis_str = String(fumidadeSolo); 
   umis_str.toCharArray(umidadeSolo, umis_str.length() + 1);

   umia_str = String(fumidadeAr); 
   umia_str.toCharArray(umidadeAr, lum_str.length() + 1);
   
   /*ENVIO DE INFORMAÇÕES AO SERVIDOR*/
   client.publish(MQTT_TOPIC_TEMP, temperatura);
   client.publish(MQTT_TOPIC_HUMID, umidadeSolo);
   client.publish(MQTT_TOPIC_HUMIDAIR, umidadeAr);
   client.publish(MQTT_TOPIC_LUX, luminosidade); 
   
   ESP.deepSleep(29 * 60000000);//Dorme por 29 minutos (Deep-Sleep em Micro segundos).

 }
