#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

IPAddress broker(192, 168, 2, 101); // Casa

const int ledPin = D4;
const int buttonPin = D3;

const char* ssid     = "Papillon"; //Naviter"; //
const char* password = "70445312"; //N4v1t3rWIFI2015"; //

const char* devID = "Lamp2";
// const char* devUS = "dev02";
// const char* devPW = "dev02";

const char* devTopic = "PapillonIoT/Lamp2/+";

const unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

int status = WL_IDLE_STATUS;   // the Wifi radio's status

// Initialize the Ethernet client object
WiFiClient WIFIclient;

PubSubClient client(WIFIclient);

int brilloLampara = 0;
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled

/////////////////////////////////////
//
// CUSTOM HW FUNCITONS
//
/////////////////////////////////////

void controlLampara (int brillo)
{
  Serial.print("controlLampara(");
  Serial.print(brillo);
  Serial.println(")");
  brilloLampara = brillo;
  if (brillo > 0) {
    if (brillo == 100) {
      Serial.println("Encender Lampara");
      digitalWrite(ledPin, HIGH);
    } 
    else {
      Serial.print("Ajustar brillo a ");
      Serial.println(brillo);
      int brillo1024 = map(brillo, 0, 100, 0, 1023);
      analogWrite(ledPin, brillo1024);
    }
  } 
  else {
    Serial.println("Apagar Lampara");
    digitalWrite(ledPin, LOW);
  }
}

bool controlButton() 
{
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      // return pressed
      if (buttonState == LOW)
        return true;
    }
  }

  lastButtonState = reading;
  return false;
}

/////////////////////////////////////
//
// MQTT FUNCTIONS
//
/////////////////////////////////////

//print any message received for subscribed topic
void callback(char* topic, byte* payload, unsigned int length) {
  String sTopic = topic;
  String sCommand = sTopic.substring(sTopic.lastIndexOf("/") + 1);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String sPayload = "";
  for (unsigned int i=0; i < length; i++) {
    sPayload += (char)payload[i];
  }
  Serial.println(sPayload);

  Serial.println("Command: " + sCommand);

  if (sCommand == "set") {
    Serial.println("ENCENDIDO/APAGADO");
    if (sPayload == "ON") {
      if (brilloLampara == 0)
        controlLampara(100);
      else
        controlLampara(brilloLampara);
    } else {
      controlLampara(0);
    }
  }

  if (sCommand == "setBrightness") {
    Serial.println("AJUSTE BRILLO");
    int valorBrillo = sPayload.toInt();
    controlLampara(valorBrillo);
  }
}

bool mqttReconnect() {
  int tries = 0;
  // Loop until we're reconnected
  while (!client.connected() && (tries < 10)) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect, just a name to identify the client
    if (client.connect(devID)) { //}, devUS, devPW)) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(devTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
      tries++;
    }
  }
  if (tries == 10) {
    Serial.println("Too many trials, no MQTT connection was possible");
    return false;
  } else {
    return true;
  }
}

bool connectNetwork(bool outDebug = true) {
  int tries = 0;

  if (outDebug) Serial.println("Trying Main WiFi");
  while ((WiFi.status() != WL_CONNECTED) && (tries < 10)) {
    delay(500);
    if (outDebug) Serial.print(".");
    tries++;
  }
  if (outDebug) Serial.println();
    
  if (tries == 10) {
    Serial.println("Too many trials, no WiFi connection was possible");
    return false;
  } else {
    if (outDebug) Serial.println("WiFi connected");  
    if (outDebug) Serial.println("IP address: ");
    if (outDebug) Serial.println(WiFi.localIP());
  
    return mqttReconnect();
  }
}

void notificarBroker() {
  String payload = "{\"state\":";
  if (brilloLampara > 0)
    payload += "\"ON\",";
  else
    payload += "\"OFF\",";
  payload += "\"brightness\":\"";
  payload += brilloLampara;
  payload += "\"}";

  Serial.println(payload);
 
  client.publish("PapillonIoT/Lamp2/status", (char*) payload.c_str());

}

/////////////////////////////////////
//
// Setup board
//
/////////////////////////////////////

void setup() {
  // initialize serial for debugging
  Serial.begin(115200);
  Serial.println("<<<<< SETUP START >>>>>");
  // initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // configure MQTT server
  client.setServer(broker, 1883);
  client.setCallback(callback);

  if (connectNetwork()) {
    Serial.println("Network OK");
  } else {
    Serial.println("Network Problem. We will try again in Loop");
  }

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(ledPin, OUTPUT);

  // Set Button
  pinMode(buttonPin, INPUT_PULLUP);

  // Estado inicial
  controlLampara(brilloLampara);
  notificarBroker();
  Serial.println("<<<<< SETUP END >>>>>");
}

/////////////////////////////////////
//
// Process loop
//
/////////////////////////////////////

void loop() {

  int brilloInicial = brilloLampara;

  if (connectNetwork(false)) {
    // Serial.println("Loop with Network OK");
    client.loop();
  } else {
    Serial.println("Loop with Network Problem. We will try again in next Loop");
  }

  // Check button
  if (controlButton()) {
    Serial.println("Button!");
     // Change the Lamp status
    if (brilloLampara > 0) 
      brilloLampara = 0;
    else
      brilloLampara = 100;
    controlLampara(brilloLampara);
  }
  
  if (brilloInicial != brilloLampara)
    notificarBroker();

}