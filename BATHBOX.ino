#include <SoftwareSerial.h>
#include <Servo.h>
#define SW1 2
#define SW2 3
#define echo_pin 4
#define trigger_pin 5
#define LED 7
#define BZ 6
#define SV 8
#define AutoLED 9
#define Rain A0
#define LDR A1
Servo myservo;

SoftwareSerial se_read(12, 13); // write only
SoftwareSerial se_write(10, 11); // read only

long duration, cm, floor_length;
long microsecondsToCentimeters(long microseconds)
{
  return microseconds / 29 / 2;
}

struct ProjectData {
  /*your data*/
  int32_t IsBoxEmpty;
} project_data = {1}; //your value

struct ServerData {
  /*your data*/
  int32_t IsBoxEmpty;
} server_data = {1};// your value

const char GET_SERVER_DATA = 1;
const char GET_SERVER_DATA_RESULT = 2;
const char UPDATE_PROJECT_DATA = 3;

void send_to_nodemcu(char code, void *data, char data_size) {
  char *b = (char*)data; 
  char sent_size = 0;
  while (se_write.write(code) == 0) {
    delay(1);
  }
  while (sent_size < data_size) {
    sent_size += se_write.write(b, data_size);
    delay(1);
  }
}

void AutoClose() {
  myservo.write(250);
  delay(1000);
  myservo.write(130);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  se_read.begin(38400);
  se_write.begin(38400);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(BZ, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(Rain, INPUT);
  myservo.attach(SV);
  myservo.write(130);
  pinMode(LDR, INPUT);
  pinMode(AutoLED, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(trigger_pin, OUTPUT);
  digitalWrite(trigger_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger_pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(trigger_pin, LOW);
  pinMode(echo_pin, INPUT);
  duration = pulseIn(echo_pin, HIGH);
  floor_length = microsecondsToCentimeters(duration);
  while (!se_read.isListening()) {
    se_read.listen();
  }

  Serial.println((int)sizeof(ServerData));
  Serial.println("ARDUINO READY!");
}

int32_t UserDetected,StillDetected,Alert,IsFloorWet = 0;
uint32_t last_sent_time = 0;
boolean is_data_header = false;
char expected_data_size = 0;
char cur_data_header = 0;
char buffer[256];
int8_t cur_buffer_length = -1;

void loop() {
  uint32_t cur_time = millis();
  //send to nodemcu
  if (cur_time - last_sent_time > 2000) {//always update
    send_to_nodemcu(UPDATE_PROJECT_DATA, &project_data, sizeof(ProjectData));
    //send_to_nodemcu(GET_SERVER_DATA, &server_data, sizeof(ServerData));
    last_sent_time = cur_time;
  }

  //read from sensor....
  pinMode(trigger_pin, OUTPUT);
  digitalWrite(trigger_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger_pin, LOW);
  duration = pulseIn(echo_pin, HIGH);
  //send to nodemcu
  
  
  //read data from server pass by nodemcu
  while (se_read.available()) {
    char ch = se_read.read();
    //Serial.print("RECV: ");
    //Serial.println((byte)ch);
    if (cur_buffer_length == -1) {
      cur_data_header = ch;
      switch (cur_data_header) {
        case GET_SERVER_DATA_RESULT:
        //unknown header
          expected_data_size = sizeof(ServerData);
          cur_buffer_length = 0;
          break;
      }
    } else if (cur_buffer_length < expected_data_size) {
      buffer[cur_buffer_length++] = ch;
      if (cur_buffer_length == expected_data_size) {
        switch (cur_data_header) {
          case GET_SERVER_DATA_RESULT: {
            ServerData *data = (ServerData*)buffer;
            //use data to control sensor
            server_data.IsBoxEmpty = data->IsBoxEmpty;
          } break;
        }
        cur_buffer_length = -1;
      }
    }
  }
  Serial.println("******************************************************************************************");
  ///////////////Check item in the box & Total usage///////////////////////////////////////////////////////////////////////////////
  //Serial.print("Button1 = ");
  //Serial.println(digitalRead(SW1));
  //Serial.print("Button2 = ");
  //Serial.println(digitalRead(SW2));
  if(digitalRead(SW1) == 0 || digitalRead(SW2) == 0){
    project_data.IsBoxEmpty = 0;
    //Serial.println("Not Empty");
  }
  else{
    if(Alert == 1){
      Alert = 0;
    }
    project_data.IsBoxEmpty = 1;
    //Serial.println("Empty");
  }
  Serial.print("IsBoxEmpty = ");
  Serial.println(project_data.IsBoxEmpty);
  ////////////////Check user walk pass the door/////////////////////////////////////////////////////////////////////////
  if(microsecondsToCentimeters(duration) < 10){
    if(StillDetected == 0){
      if(Alert == 0){
        UserDetected++;
      }
      StillDetected = 1;
    }
  }
  else{
    StillDetected = 0;
  }
  Serial.print("Range = ");
  Serial.println(microsecondsToCentimeters(duration));
  Serial.print("User still stand infront of sensor? = ");
  Serial.println(StillDetected);
  ////////////////Alert when box isn't empty and user left the bathroom///////////////////////////////////////////////////////
  if(project_data.IsBoxEmpty == 0 && UserDetected == 2 && Alert == 0){
    UserDetected--;
    Alert = 1;
  }
  if(UserDetected == 2){
    UserDetected = 0;
  }
  if(Alert == 1){
    analogWrite(BZ, HIGH);
    digitalWrite(LED, HIGH);
  }
  else{
    analogWrite(BZ, LOW);
    digitalWrite(LED, LOW);
  }
  Serial.print("User detected = ");
  Serial.println(UserDetected);
  Serial.print("Alert = ");
  Serial.println(Alert);
  //////////////////////Auto-Close when shower//////////////////////////////////////////////////////////
  Serial.print("Rain Voltage = ");
  Serial.println(analogRead(Rain));
  if(analogRead(Rain) < 350 && IsFloorWet == 0)
  {
    IsFloorWet = 1;
    AutoClose();
  }
  else if(analogRead(Rain) < 700 && IsFloorWet == 1)
  {
    IsFloorWet = 0;
  } 
  //////////////////////LED auto-on when open the box////////////////////////////////////////////////////////////////////////
  Serial.print("LDR = ");
  Serial.println(analogRead(LDR));
  if(analogRead(LDR) > 100){
    digitalWrite(AutoLED, HIGH);
    Serial.println("Auto LED = ON");
  }
  else{
    digitalWrite(AutoLED, LOW);
    Serial.println("Auto LED = OFF");
  }
  delay(1000);
}

