#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

LiquidCrystal_I2C lcd(0x27,16,2);

//Pinos Reset e SS módulo MFRC522
#define SS_PIN D4
#define RST_PIN D3
MFRC522 mfrc522(SS_PIN, RST_PIN);

//definição da senha do Wi-Fi + IP do Servidor IFMG
const char *ssid = "";
const char *password = "";

//broker IFMG
const char* mqtt_server = "";

//broker online
//const char* mqtt_server = "broker.hivemq.com";

//definição de variáveis
String Link = "http://server_adress:port/validarUser";
String LinkStatus = "http://server_adress:port/contagem/";
String body = "";
int timeout,counter = 0;
bool pcd = false;

//Criando objetos
MFRC522::MIFARE_Key key;
WiFiClient wifiClient;
HTTPClient http;
PubSubClient  client(wifiClient);


void setup()
{
  pinMode(D0,INPUT);  //Pino 0 entrada digital para o botão
  startLCD();
  startRFID();
  startWIFI();
  startMQTT();
  //Tela Inicial Estacioanmento
  lcd.setCursor(0, 0);
  lcd.print("Estacionamento");
  lcd.setCursor(6, 1);
  lcd.print("IFMG");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aprox. o card");
}
 
void loop()
{
  client.loop();
  String conteudo = "";

  //Aguarda cartao
  while(!mfrc522.PICC_IsNewCardPresent())
  {
    ////MQTT
    if(digitalRead(D0)==1)
    {
      atualizarVagasEspeciais();
      pcd = true;
      delay(5000);
    }
    else
    {
      if(pcd)
      {
        atualizarVagasEspeciais_default(); 
        pcd = false;      
      }
    }

    delay(300);
  }

  if(!mfrc522.PICC_ReadCardSerial())
  {
    return;
  }

  //ler UID do cartão
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    conteudo.concat(String(mfrc522.uid.uidByte[i]<0x10 ? " 0" : " "));
    conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  //se um cartão for reconhecido
  if(conteudo.substring(1) != 0)
  {
    conteudo.toUpperCase();
    String tag = conteudo.substring(1);

    //Chamada POST passando o ID da TAG RFID
    PostRequest(tag);

    //atualiza Status das vagas
    atualizarVagas();
  }
  delay(2000);
}

void startLCD()
{
  Serial.begin(9600);   //Inicia a serial
  //Inicialização do LCD com o conversor I2C
  lcd.init();
  lcd.setBacklight(HIGH);
  lcd.clear();
  //Tela Inicial Estacioanmento
  lcd.setCursor(0, 0);
  lcd.print("Ligando...");
  delay (3000);
}

void startRFID()
{
  //leitor RFID
  SPI.begin();      //Inicia  SPI bus
  mfrc522.PCD_Init();   //Inicia MFRC522
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
}

void startWIFI()
{
  //conexão com o módulo WIFI
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  Serial.println(WiFi.localIP());
}

void startMQTT()
{
  client.setServer(mqtt_server, 1883);
  
  while (!client.connected())
  {
    Serial.print("Conectando no broker MQTT...");
    // Cria identificação randômica do cliente
    String clientId = "ESP8266Client-999";
    if(client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      //client.publish("Vaga01", "hello world");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void atualizarVagasEspeciais()
{
  Serial.println("Publicando no topico esp/vagas");
  Serial.println(client.connected());

  if(!client.connected())
  {
    startMQTT();
  }

  const char *message = "{'Vaga01':'Ocupado','Vaga02':'Livre','Vaga03':'Livre','Vaga04':'Livre'}";
  int length = strlen(message);
  boolean retained = true; 
  client.publish("esp/vagas", (byte*)message, length, retained);
}

void atualizarVagasEspeciais_default()
{
  Serial.println("Publicando no topico esp/vagas");
  Serial.println(client.connected());

  if(!client.connected())
  {
    startMQTT();
  }

  const char *message = "{'Vaga01':'Livre','Vaga02':'Livre','Vaga03':'Livre','Vaga04':'Livre'}";
  int length = strlen(message);
  boolean retained = true; 
  client.publish("esp/vagas", (byte*)message, length, retained);
}

void PostRequest(String tag)
{
    tag.trim();
    tag.replace(" ", "");
    String body = "{\"tag_id\":\"";
    body = body + tag + "\"}";
    Serial.println(body);
    Serial.println(Link);
    http.setTimeout(20000);
    http.begin(wifiClient,Link);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(body);
    delay(250);
    String payload = http.getString();
    Serial.println(payload);
    http.end();
    
    //JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    String status = doc["Status"];
    Serial.println(status);
    status.trim();
    status.replace(" ", "");

     if(status == "Invalido")
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card Invalido");
      delay(5000);
      lcd.clear();
      atualizarVagas();
      return;
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(status);
      delay(2000);
      lcd.clear();
    }
}

void atualizarVagas()
{
  //obter quantidade atual
  http.setTimeout(20000);
  http.begin(wifiClient,LinkStatus);
  int httpCode = http.GET();
  delay(250);
  String payload = http.getString();
  http.end();
  //Fim do request HTTP
  
  //Obtendo local selecionado a partir do JSON entregue pela API
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);
  int  EmUso = doc["EmUso"];
  int Livres = 100 - EmUso;

  String msg2 = "Livre:"+ String(Livres) + " Uso:" + String(EmUso);
  lcd.setCursor(0, 0);
  lcd.print("Aprox. o card");
  lcd.setCursor(0, 1);
  lcd.print(msg2);
}
