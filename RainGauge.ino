//
//    MiS Rain Gauge

#include <HX711.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
//#include <ESP8266mDNS.h>
//#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <PTSolns_AHTx.h>
#include <BMx280.h>

#define MY_PIN  D5
#define MY_MIN 540
#define MY_MAX 2450
#define MY_MID 1500
#define MY_DEL 15
#define MY_HOME 150
#define MY_MOUT 40
#define MY_SDA D2
#define MY_SCL D1
#define MY_HXD D7
#define MY_HXC D6
#define MSG_BUFFER_SIZE 50
#define MQTT_SIZE 20
#define TWEIGHT 5
#define PERIOD 30

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
char MQTT_TIME[20], MQTT_TIP[20];

int MQTT_PORT = 1883;
char my_dir[ 10 ];
char WiFi_Pass[20];
char PUB_message[MSG_BUFFER_SIZE], SUB_message[MSG_BUFFER_SIZE];
char MQTT_in_buffer[MSG_BUFFER_SIZE], MQTT_in_topic[20];
char my_IP_Address[20];  //  xxx:qsort.xxx.xxx.xxx
int MQTT_in_flag, MQTT_in_length;
unsigned long epoch = 0, now = 0, interval = 0, pulse, payload_time, period;
uint8_t i = 0, rtn = 0, count = 0;
int net, network, networks;
int in, then, pa;
char * q;
/*  time variables */
char a_timestamp[ 13 ], a_year[ 5 ], a_month[ 4 ], a_day[ 4 ], a_hour[ 4 ], a_minute[ 4 ], a_second[ 4 ];
char a_date[ 20 ];
long int timestamp;
int year, month, day, hour, minute, second;
int servo = 0;
int time_flag = 0;

int ON  = 1;
int OFF = 0;

int sequence = 0, tips = 0, time_seq = 0;
uint8_t dataPin  = MY_HXD;
uint8_t clockPin = MY_HXC;
float weight = 0, oweight = 0, start_weight, temp;

// Setting PWM properties
const int APWM = MY_PIN;
const int freq = 100000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 200;

const uint8_t  I2C_ADDR      = 0x76;     // Default address
const unsigned POLL_MS       = 2000;     // [mS] How often to read sensor
const float    SEA_LEVEL_HPA = 1013.25f;

/*  Instance creation */

WiFiClient espClient;              // WiFi instance

PubSubClient client(espClient);    // MQTT instance

HX711 scale;                        // HX711 instance

PTSolns_AHTx aht;

BMx280 bmx;

/*
 *	Setup
 */
void setup()
{
  Serial.begin(115200);
  while (!Serial) delay(1000);
  Wire.begin( D2, D1 );
  if (!bmx.beginI2C(I2C_ADDR)) { 
    Serial.println("BMx280 not found"); 
    while(1){
      Serial.println("BMx280 not found"); 
    } 
  }
  bmx.setSampling(1,1,1, 0,0, BMx280::MODE_NORMAL); 

  Serial.print("chipID=0x"); Serial.println(bmx.chipID(), HEX);
  Serial.print("hasHumidity="); Serial.println(bmx.hasHumidity());
  Serial.println("I2C Continuous");
/*
 *    Construct MQTT tokens
 */ 
  sprintf( MiS_HEAD,      "%s/%s", MiS_BASE, MiS_DEVICE );
  sprintf( MQTT_IN,       "%s/%s", MiS_HEAD, IN_TOPIC );
  sprintf( MQTT_OUT,      "%s/%s", MiS_HEAD, OUT_TOPIC );
  sprintf( MQTT_RST,      "%s/%s", MiS_HEAD, RST_TOPIC );
  sprintf( MQTT_STATIN,   "%s/%s", MiS_HEAD, STAT_IN_TOPIC );
  sprintf( MQTT_STATOUT,  "%s/%s", MiS_HEAD, STAT_OUT_TOPIC );
  sprintf( MQTT_TIME,     "%s/%s", MiS_BASE, "TIME");
  sprintf( MQTT_TIP,      "%s/%s", MiS_HEAD, "TIP");
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
  Serial.println(" WiFi Connecting ");
  fn_WiFi_Connect( net );
  Serial.print( "IP Address : " );
  Serial.println( WiFi.localIP().toString().c_str() );
  Serial.println(" WiFi Connected");

/*
 *  MQTT setup
 */
   Serial.println (" MQTT Connecting ");
  client.setServer(MQTT_server, MQTT_PORT);
  client.setCallback(MQTT_CB);
  delay(1000);
  client.loop();
  fn_MQTT_Connect();
  Serial.println(" MQTT Connected ");
 
  epoch = millis();      //  start the clock

  Serial.println("1");
  scale.begin(MY_HXD, MY_HXC);
  Serial.println("2");
  //scale.set_gain( 128 );

  if ( !scale.is_ready() ) delay(1000);
  Serial.println("3");
  scale.tare(20);
  Serial.println("4");
  int32_t offset = scale.get_offset();

  Serial.print("OFFSET: ");
  Serial.println(offset);
  Serial.println();

  scale.set_scale( 10600 );
  scale.set_offset(870000);

  //  reset the scale to zero = 0
  scale.tare(0);

	myservo.attach(MY_PIN, 700, 2300);

  Wire.begin( D2, D1 );

  aht.begin();

  myservo.write(MY_HOME);

  time_seq = PERIOD * 1000;

  Serial.println(" Setup ends ");
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
  for ( pos = MY_MOUT; pos <= MY_HOME; pos++ )
  {
    myservo.write( pos );
	  delay( 20 );
  }
  delay( 2000 );

//  myservo.write( MY_HOME);
  scale.tare(0);
  start_weight = 0;
  tips++;
}

/*
 *  Scan WiFi for networks
 */
int fn_WiFiScan() {
  int network;
  int networks = WiFi.scanNetworks();

  if (networks == 0) return (-2);               // No Networks found

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

  if ( strcmp( MQTT_in_topic, MQTT_TIME) == 0)  // Time Pulse  
  {
    Serial.print( "MQTT Time : " );
    Serial.println( MQTT_in_buffer );
//  1296651311^2025/09/27^14:45:01
//  012345678901234567890123456789
    for ( i=0; i<=9; i++ )
    {
      a_timestamp[ i ] = MQTT_in_buffer[ i ];
    }
    a_timestamp[ 10 ] = 0;
    timestamp = atof( a_timestamp );
    for ( i=11; i<=14; i++ )
    {
      a_year[ i - 11 ] = MQTT_in_buffer[ i ];
    }
    a_year[ 4 ] = 0;
    year = atoi( a_year );
    a_month[ 0 ] = MQTT_in_buffer[ 16 ];
    a_month[ 1 ] = MQTT_in_buffer[ 17 ];
    a_month[ 2 ] = 0;
    month = atoi( a_month );
    a_day[ 0 ] = MQTT_in_buffer[ 19 ];
    a_day[ 1 ] = MQTT_in_buffer[ 20 ];
    a_day[ 2 ] = 0;
    sprintf( a_date, "%s/%s/%s", a_day, a_month, a_year );
    day = atoi( a_day );
    a_hour[ 0 ] = MQTT_in_buffer[ 22 ];
    a_hour[ 1 ] = MQTT_in_buffer[ 23 ];
    a_hour[ 2 ] = 0;
    hour = atoi( a_hour );
    a_minute[ 0 ] = MQTT_in_buffer[ 25 ];
    a_minute[ 1 ] = MQTT_in_buffer[ 26 ];
    a_minute[ 2 ] = 0;
    minute = atoi( a_minute );
    a_second[ 0 ] = MQTT_in_buffer[ 28 ];
    a_second[ 1 ] = MQTT_in_buffer[ 29 ];
    a_second[ 2 ] = 0;
    second = atoi( a_second );
    client.unsubscribe( MQTT_TIME );
  }
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

  if (strcmp(MQTT_in_topic, MQTT_TIP) == 0)
  {
    if (strcmp(MQTT_in_buffer, "Yes") == 0) fn_tip();
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
      client.subscribe(MQTT_TIME, 1);
      client.subscribe(MQTT_TIP, 1);
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

void fn_delay( int interval )
{
  now = millis();
  while ( ( millis() - now ) < interval )
  {
    delay( 1000 );
      client.loop();
  }
}

void loop()
{
  epoch = millis();
  client.loop();
  float temp, humid;
  float T, P_hPa, H;
  AHTxStatus st = aht.readTemperatureHumidity(temp, humid, 120);
  bmx.read280(T, P_hPa, H);
 
  sequence++;
  weight = 0;
  if (scale.is_ready())
  {
    weight = scale.get_units(10);
    if ( start_weight == 0 ) start_weight = weight;
   // float readw =   scale.read_average( 10 );
    if ( weight < 0 ) weight = 0 - weight;
    if ( weight > ( TWEIGHT + TWEIGHT ) ) weight = oweight;

    sprintf( MQTT_PUB, "$%05i,%s,%02i:%02i:%02i,%05i,%05i,%07.2f,%07.2f,%07.2f,%07.2f,%0.2f#",
           sequence, a_date, hour, minute, second, epoch - now, tips, temp, humid, T, P_hPa,  weight );
    Serial.println( MQTT_PUB );
    if (!client.connected()) fn_MQTT_Connect();
    client.publish(MQTT_OUT, (const uint8_t*)MQTT_PUB, strlen(MQTT_PUB), false);

   // fn_tip();

    if ( weight >= ( start_weight + TWEIGHT ) ) fn_tip();
    oweight = weight;
  }
  interval = millis() - epoch;

  second = second + PERIOD;
  if ( second >= 60 ) {
    minute = minute + 1;
    second = second - 60;
    if ( minute >= 60 ) {
      hour = hour + 1;
      minute = minute - 60;
      if ( hour >= 24 ) hour = hour - 24; 
    }
  }
  if ( hour == 1 ) {
    if ( minute == 5 ) {
        client.subscribe( MQTT_TIME );
    }
  }
  now = epoch;
  if ( interval > ( time_seq ) ) interval = ( time_seq ) -100;
  fn_delay( ( time_seq ) - interval );
}
//  -- END OF FILE --

