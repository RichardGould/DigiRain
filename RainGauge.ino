//
//    MiS Rain Gauge

#include <HX711.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>

#define MY_PIN D4
#define MY_MIN 500
#define MY_MAX 2500
#define MY_MID 1500
#define MY_DEL 15
#define MY_HOME 130
#define MY_MOUT 27
#define MY_SDA D2
#define MY_SCL D1
#define MY_HXD D5
#define MY_HXC D6
#define MSG_BUFFER_SIZE 50
#define MQTT_SIZE 20

#define SizePass 20

Servo myservo;  // create servo object to control a servo

/*  variable instantiation  */

/*  Network Credentials */
const char* Home_WiFi     = "GOULD_TP";
char Home_Pass[SizePass]  = "pr`rxgvx";
const char* Home_MQTT     = "192.168.1.178";

const char* MiS_WiFi      = "BTB-NTCHT6";
char MiS_Pass[SizePass]   = "RHCagINtyI}`k>";
const char* MiS_MQTT      = "192.168.1.249";

const char* Jim_WiFi      = "BT-S7AT5Q";
char Jim_Pass[SizePass]   = "vHqbS8{:YqKX@q";
const char* Jim_MQTT      = "192.168.1.165";

const char* Rich_WiFi     = "GOULDWAN24";
char Rich_Pass[SizePass]  = "Tgkf2;47cd";
const char* Rich_MQTT     = "192.168.1.178";

const char* MiS_BASE      = "MiS";
const char* MiS_DEVICE    = "RAIN";
const char* IN_TOPIC      = "FromMaster";
const char* OUT_TOPIC     = "Data";
const char* RST_TOPIC     = "RST";
const char* STAT_IN_TOPIC = "STAT/IN";
const char* STAT_OUT_TOPIC = "STAT/OUT";
const char* Version       = "0.1";

char MQTT_server[20];
char MQTT_PUB[200], MQTT_IN[MQTT_SIZE], MQTT_OUT[MQTT_SIZE], MQTT_RST[MQTT_SIZE], MQTT_STATIN[MQTT_SIZE], MQTT_STATOUT[ MQTT_SIZE], MiS_HEAD[10];

int MQTT_PORT = 1883;
char my_dir[ 10 ];
char WiFi_Pass[20];
char PUB_message[MSG_BUFFER_SIZE], SUB_message[MSG_BUFFER_SIZE];
char MQTT_in_buffer[MSG_BUFFER_SIZE], MQTT_in_topic[20];
char my_IP_Address[20];  //  xxx:qsort.xxx.xxx.xxx
int MQTT_in_flag, MQTT_in_length;
unsigned long epoch = 0, pulse, payload_time;
uint8_t i = 0, rtn = 0, count = 0;
int net, network, networks;
int in, now, then, pa, period;
char * q;

int ON  = 1;
int OFF = 0;

int sequence = 0;
uint8_t dataPin  = MY_HXD;
uint8_t clockPin = MY_HXC;

/*  Instance creation */

WiFiClient espClient;              // WiFi instance

PubSubClient client(espClient);    // MQTT instance

HX711 scale;                        // HX711 instance
/*
 *	Setup
 */
void setup()
{
  Serial.begin(115200);
  while (!Serial) delay(1000);

/*
 *    Construct MQTT tokens
 */ 
  sprintf( MiS_HEAD,      "%s/%s", MiS_BASE, MiS_DEVICE );
  sprintf( MQTT_IN,       "%s/%s", MiS_HEAD, IN_TOPIC );
  sprintf( MQTT_OUT,      "%s/%s", MiS_HEAD, OUT_TOPIC );
  sprintf( MQTT_RST,      "%s/%s", MiS_HEAD, RST_TOPIC );
  sprintf( MQTT_STATIN,   "%s/%s", MiS_HEAD, STAT_IN_TOPIC );
  sprintf( MQTT_STATOUT,  "%s/%s", MiS_HEAD, STAT_OUT_TOPIC );
  
/*
 *  decrypt passwords
 */
   int i;
  for (i = 0; i < (int)strlen(Home_Pass); i++) Home_Pass[i] = Home_Pass[i] + 3 - i;
  for (i = 0; i < (int)strlen(MiS_Pass);  i++) MiS_Pass[i]  = MiS_Pass[i]  + 3 - i;
  for (i = 0; i < (int)strlen(Jim_Pass);  i++) Jim_Pass[i]  = Jim_Pass[i]  + 3 - i;
  for (i = 0; i < (int)strlen(Rich_Pass); i++) Rich_Pass[i] = Rich_Pass[i] + 3 - i;
/*
 *  determine where the WiFi is
 */
  int net = fn_WiFiScan();

  if (net == -2) fn_ReStart();  // restart wemo
  
  Serial.print( "Network : " );
  Serial.println( net );

/*
 *  Connect to recognised WiFi
 */
  int count = 0;
  Serial.print(" WiFi Connecting ");
  fn_WiFi_Connect( net );
  Serial.print( "IP Address : " );
  Serial.println( WiFi.localIP().toString().c_str() );

/*
 *  MQTT setup
 */
  client.setServer(MQTT_server, MQTT_PORT);
  client.setCallback(MQTT_CB);
  delay(1000);
  client.loop();
  fn_MQTT_Connect();
/*
 *	Arduino OTA Setup
 */ 
  ArduinoOTA.setHostname("newvane");
  ArduinoOTA.setPassword("starwest");
/*
 *  Arduino OTA Callbacks
 */
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
	});

  ArduinoOTA.begin();

  epoch = millis();      //  start the clock

  Serial.println();
  Serial.println(__FILE__);
  Serial.print("HX711_LIB_VERSION: ");
  Serial.println(HX711_LIB_VERSION);
  Serial.println();

  myservo.attach( MY_PIN, MY_MIN, MY_MAX, MY_MID );

  scale.begin(MY_HXD, MY_HXC);

  if ( !scale.is_ready() ) delay(1000);

  Serial.print("UNITS: ");
  Serial.println(scale.get_units(10));
  Serial.print("Read:");
  Serial.println( scale.read() );

//  scale.set_scale( -8829.607143 );       //  TODO you need to calibrate this yourself.
//  scale.set_offset(-1125202);
  scale.set_scale( -9000 );       //  TODO you need to calibrate this yourself.
  scale.set_offset(-110000);

  //  reset the scale to zero = 0
  scale.tare(10);
  Serial.print("UNITS: ");
  Serial.println(scale.get_units(10));
  myservo.write( MY_HOME );
}

void fn_tip( void )
{
  int pos = 0;
  for ( pos = MY_HOME; pos >= MY_MOUT; pos-- )
  {
    myservo.write( pos );
    delay( 10 );
  }
  delay( 2000 );
  myservo.write( MY_HOME );
//  fn_calibrate();
  epoch = 0;
}

/*
 *  Scan WiFi for networks
 */
int fn_WiFiScan() {
  int network;
  int networks = WiFi.scanNetworks();
  Serial.print("Networks found : ");
  Serial.println( networks );

  if (networks == 0) return (-2);               // No Networks found

  for (network = 0; network <= networks; network++) {
    Serial.print( "Network : " );
    Serial.print( network );
    Serial.print( " : " );
    Serial.print( WiFi.SSID(network).c_str());
    Serial.print( " : " );
  	Serial.println( WiFi.RSSI() ); // Get the RSSI value
  }
  for (network = 0; network <= networks; network++) {
    if (strcmp(WiFi.SSID(network).c_str(), Rich_WiFi) == 0) {
      strcpy(WiFi_Pass, Rich_Pass);
      strcpy(MQTT_server, Rich_MQTT);
      return (network);
    }
    if (strcmp(WiFi.SSID(network).c_str(), Home_WiFi) == 0) {
      strcpy(WiFi_Pass, Home_Pass);
      strcpy(MQTT_server, Home_MQTT);
      return (network);
    }
    if (strcmp(WiFi.SSID(network).c_str(), MiS_WiFi) == 0) {
      strcpy(WiFi_Pass, MiS_Pass);
      strcpy(MQTT_server, MiS_MQTT);
      return (network);
    }
    if (strcmp(WiFi.SSID(network).c_str(), Jim_WiFi) == 0) {
      strcpy(WiFi_Pass, Jim_Pass);
      strcpy(MQTT_server, Jim_MQTT);
      return (network);
    }
  }
/* if we get here the network found is not recognised */
  fn_ReStart();
  return(0);
}

/*	Connect to Wifi */
int fn_WiFi_Connect(int network) {
  int j = 0;
  delay(2000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFi.SSID(network), WiFi_Pass);

  while (j++ < 20) {
    if (WiFi.status() == WL_CONNECTED) return(0);
    Serial.print(".");
	delay(1000);
  }

  fn_ReStart();
  return(0);
}

/*
 *  MQTT callback
 */
int MQTT_CB(char* topic, byte* payload, uint8_t length) {
  Serial.println( "Message Arrived" );
  int k;
  memset(MQTT_in_topic, 0, sizeof(MQTT_in_topic));
  memset(MQTT_in_buffer, 0, sizeof(MQTT_in_buffer));

  strcpy(MQTT_in_topic, topic);
  MQTT_in_length = length;
  for (k = 0; k < length; k++) MQTT_in_buffer[k] = (char)payload[k];
  MQTT_in_buffer[k] = 0;
  Serial.print( "MQTT : " );
  Serial.print( MQTT_in_topic );
  Serial.print( " : " );
  Serial.println( MQTT_in_buffer );
  
  if (strcmp(MQTT_in_topic, MQTT_STATIN) == 0)  // Status Enquiry
  {
    if (strcmp(MQTT_in_buffer, "ASK") == 0)
    {
      strcpy( MQTT_PUB, "OK - ");
      strcat( MQTT_PUB, Version );
/*  publish status to MQTT  */
      client.publish(MQTT_STATOUT, (const uint8_t*)MQTT_PUB, strlen(MQTT_PUB), false);
    }
  }
  if (strcmp(MQTT_in_topic, MQTT_RST) == 0)
  {
    if (strcmp(MQTT_in_buffer, "Yes") == 0) fn_ReStart();
  }
  return (1);
}

/*	connect to MQTT broker */
int fn_MQTT_Connect() {
  int j = 0;
  while (j < 10) {
    if (client.connect(MiS_DEVICE) == 1) {
      delay(2000);
      client.subscribe(MQTT_RST,  1);
      client.subscribe(MQTT_STATIN, 1);
      return (0);
    }
    Serial.print(F("."));
    delay( 3000 );
    j++;
  }
  fn_ReStart();
  return( 1 );
}

void fn_ReStart(void) {
  ESP.restart();
}

void fn_calibrate()
{
  Serial.println("\n\nCALIBRATION\n===========");
  Serial.println("remove all weight from the loadcell");
  Serial.println("Determine zero weight offset");
  //  average 20 measurements.
  scale.tare(20);
  int32_t offset = scale.get_offset();

  Serial.print("OFFSET: ");
  Serial.println(offset);
  Serial.println();
  delay(5000);
  uint32_t weight = 7;
  
  Serial.print("WEIGHT: ");
  Serial.println(weight);
  scale.calibrate_scale(weight, 20);
  float my_scale = scale.get_scale();

  Serial.print("SCALE:  ");
  Serial.println(my_scale, 6);

  Serial.print("\nuse scale.set_offset(");
  Serial.print(offset);
  Serial.print("); and scale.set_scale(");
  Serial.print(my_scale, 6);
  Serial.print(");\n");
  Serial.println("in the setup of your project");

  Serial.println("\n\n");
}

void loop()
{
    ArduinoOTA.handle();
  epoch = millis();
  sequence++;
  float weight = 0;
  if (scale.is_ready())
  {
    weight = scale.get_units(10);
    float readw =   scale.read_average( 10 );

    //Serial.print( epoch );
    //Serial.print( " : " );
    //Serial.print(weight);
    //Serial.print( " Read :" );
    //Serial.println( readw);

    sprintf( MQTT_PUB, "$%05i,%05i,%03.2f", sequence, epoch - now, weight);
    strcat( MQTT_PUB, "#" );
    if (!client.connected()) fn_MQTT_Connect();
    client.publish(MQTT_OUT, (const uint8_t*)MQTT_PUB, strlen(MQTT_PUB), false);
    Serial.println( MQTT_PUB);
    now = epoch;
    if ( weight < -25 ) fn_tip();
    if ( weight > 40 ) fn_tip();
  }
  
  //fn_tip();
  delay(1000);
}
//  -- END OF FILE --

