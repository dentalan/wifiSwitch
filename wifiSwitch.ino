/*
Reference to the board I use to upload this file is this:
http://www.instructables.com/id/ESP8266-MPSM-v2-DevBoard-MAINS-WIFI-Web-Power-Swit/
The projects acts both lie a web server and an MQTT Client
 It connects to an MQTT server then:
  - on 0 switches off relay
  - on 1 switches on relay
  - on 2 switches the state of the relay
  - on 3 sends the relay status
  - sends 0 on off relay
  - sends 1 on on relay

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 The current state is stored in EEPROM and restored on bootup

*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <EEPROM.h>

MDNSResponder mdns;
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "----ENTER Wifi SSID ---- ";
const char* password = "----ENTER wifi key----";
const char* mqtt_server = "----Enter mqtt server IP---";
ESP8266WebServer server(80);

String ccwebPage = "";
int val=0;
long lastMsg = 0;
char msg[50];
int value = 0;
String rl_State="OFF";
const char* outTopic = "Sonoff1out"; //the MQTT topic that we write messages to inform users
const char* inTopic = "Sonoff1in"; //The MQTT Topic where users send commands (0,1,2,3)
int relay_pin = 5;
int button_pin = 0;
bool relayState = LOW;

// Instantiate a Bounce object :
Bounce debouncer = Bounce(); 


void setup_wifi() {
 //  ccwebPage += "<h1>ESP8266 Web Server</h1><p>Socket #1 <a href=\"socket1On\"><button>ON</button></a>&nbsp;<a href=\"socket1Off\"><button>OFF</button></a></p>";
 //  ccwebPage += "<p>Socket #2 <a href=\"socket2On\"><button>ON</button></a>&nbsp;<a href=\"socket2Off\"><button>OFF</button></a></p>";
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    extButton();
    for(int i = 0; i<500; i++){
      extButton();
      delay(1);
    }
   
    Serial.print(".");
  }
  digitalWrite(13, LOW);
  delay(500);
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);
  digitalWrite(13, HIGH);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
 

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }
  server.on("/", [](){
    server.send(200, "text/html", webPage());
    
  });
  server.on("/socket1On", [](){
    Serial.println(getTime());
    digitalWrite(relay_pin, HIGH);
 //Store in EEPROM the value of led
    delay(1000);
    rl_State="1";
    relayState=HIGH;
    publishState();
    EEPROM.write(0, HIGH);    // Write state to EEPROM
    EEPROM.commit();
    server.send(200, "text/html", webPage());
  });
  server.on("/socket1Off", [](){
    Serial.println(getTime());
    digitalWrite(relay_pin, LOW);
    relayState=LOW;
    rl_State="0";
    publishState();
     EEPROM.write(0, LOW);    // Write state to EEPROM
    EEPROM.commit();
    delay(1000); 
    
    server.send(200, "text/html", webPage());
 //Store in EEPROM the value of led
    
    rl_State=getStatus ();
    relayState=LOW;
    
  });
  server.on("/socket2On", [](){
    server.send(200, "text/html", webPage());
//    digitalWrite(gpio2_pin, HIGH);
//    digitalWrite(gpio3_pin, HIGH);
    delay(1000);
    publishState();
  });
  server.on("/socket2Off", [](){
    server.send(200, "text/html", webPage ());
//    digitalWrite(gpio2_pin, LOW);
//    digitalWrite(gpio3_pin, LOW);
    delay(1000); 
    publishState();
  });
  server.begin();
  Serial.println("HTTP server started");

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '0') {
    digitalWrite(relay_pin, LOW);   // Turn the LED on (Note that LOW is the voltage level
    Serial.println("relay_pin -> LOW");
    rl_State="0";
    relayState = LOW;
    EEPROM.write(0, relayState);    // Write state to EEPROM
    EEPROM.commit();
  } else if ((char)payload[0] == '1') {
    digitalWrite(relay_pin, HIGH);  // Turn the LED off by making the voltage HIGH
    Serial.println("relay_pin -> HIGH");
    relayState = HIGH;
    rl_State="1";
    EEPROM.write(0, relayState);    // Write state to EEPROM
    EEPROM.commit();
  } else if ((char)payload[0] == '2') {
    relayState = !relayState;
    rl_State=getStatus();
    digitalWrite(relay_pin, relayState);  // Turn the LED off by making the voltage HIGH
    Serial.print("relay_pin -> switched to ");
    Serial.println(relayState);
//    client.publish(outTopic,getStatus); 
   // EEPROM.write(0, relayState);    // Write state to EEPROM
   // EEPROM.commit();
  }  else if ((char)payload[0] == '3') {
   //Just return 1 if LOW and 0 if HIGH
    Serial.print("relay_pin is switched to ");
    Serial.println(relayState);
  } 

publishState ();
}     
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("-----ENTER MQTT USER ID-------")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(outTopic, "Sonoff1 booted");
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      for(int i = 0; i<5000; i++){
        extButton();
        delay(1);
      }
    }
  }
}

void extButton() {
  debouncer.update();
   
   // Call code if Bounce fell (transition from HIGH to LOW) :
   if ( debouncer.fell() ) {
     Serial.println("Debouncer fell");
     // Toggle relay state :
     relayState = !relayState;
     digitalWrite(relay_pin,relayState);
     EEPROM.write(0, relayState);    // Write state to EEPROM
     if (relayState == 1){
      client.publish(outTopic, "1");
     }
     else if (relayState == 0){
      client.publish(outTopic, "0");
     }
   }
}

void setup() {
  EEPROM.begin(512);              // Begin eeprom to store on/off state
  pinMode(relay_pin, OUTPUT);     // Initialize the relay pin as an output
  pinMode(button_pin, INPUT);     // Initialize the relay pin as an output
  pinMode(13, OUTPUT);
  relayState = EEPROM.read(0);
  digitalWrite(relay_pin,relayState);
 
  
  debouncer.attach(button_pin);   // Use the bounce2 library to debounce the built in button
  debouncer.interval(50);         // Input must be low for 50 ms
  
  digitalWrite(13, LOW);          // Blink to indicate setup
  delay(500);
  digitalWrite(13, HIGH);
  delay(500);
  
  Serial.begin(115200);
 //Serial.print ("Relay Status Read From EEPROM: ");
 //Serial.println(rl_State);
 //Serial.println relayState;
 
  setup_wifi();                   // Connect to wifi 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  extButton();
  server.handleClient();
}


String getStatus () {
 if (relayState==0) {
   return "0";
   }

else if (relayState==1) {
  
  return "1";
}
}


String getTime() {
  WiFiClient client;
  while (!!!client.connect("google.com", 80)) {
    Serial.println("connection failed, retrying...");
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");
 
  while(!!!client.available()) {
     yield();
  }

  while(client.available()){
    if (client.read() == '\n') {    
      if (client.read() == 'D') {    
        if (client.read() == 'a') {    
          if (client.read() == 't') {    
            if (client.read() == 'e') {    
              if (client.read() == ':') {    
                client.read();
                String theDate = client.readStringUntil('\r');
                client.stop();
                return theDate;
              }
            }
          }
        }
      }
    }
  }
}

String webPage () {
   ccwebPage="";
   ccwebPage += "<h2>ArtInSmile WiFi Switch: ID=1</h2><p>Socket #1 <a href=\"socket1On\"><button>ON</button></a>&nbsp;<a href=\"socket1Off\"><button>OFF</button></a></p>";
   ccwebPage += "<p>Socket #2 <a href=\"socket2On\"><button>ON</button></a>&nbsp;<a href=\"socket2Off\"><button>OFF</button></a></p>";
   ccwebPage += getTime ();
   ccwebPage += "Status=";
   ccwebPage +=  rl_State;
    
      
  return ccwebPage;
    
}
void publishState() {
  //Publish to outTopic the relay State
 Serial.println (relayState);
 if (relayState == HIGH){
    Serial.println ("It is 1 and I Sent 1");
      client.publish(outTopic, "1");
     }
     else if (relayState == LOW){
      Serial.println ("It is 0 and I Sent 0");
      client.publish(outTopic, "0");
     }
}
