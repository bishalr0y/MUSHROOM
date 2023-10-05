#include <DFRobot_SIM7000.h>
#include <PubSubClient.h>
#include <avr/wdt.h> // Include the watchdog timer library
#include<EEPROM.h>
//Login website (http://iot.dfrobot.com.cn) to register an account, fill the following information based on your account



#define serverIP         "65.1.65.154"                                   //server ip
#define IOT_CLIENT       "dfrobot3"                                       //client name
#define IOT_USERNAME     "nav_mqtt_broker"                               //local broker name 
#define IOT_KEY          "ecV2JELeXo8x1cZH"                              //password to connect with broker
//#define IOT_TOPIC1       "COMMAND_ea3064aa-682d-4341-8f0f-86e8abe44f45"  //subcribed the TOPIC to receive and process the state(RESET/STOP/DAY/DONE)                       
#define IOT_TOPIC1        "COMMAND_c8302a8a-9883-4a40-adfb-9303ce3544d4"
#define IOT_TOPIC_ACK     "ACK"                                           //publishing the topic after receiving message from COMMAND topic 
#define IOT_TOPIC_SEND    "STATE"                                         //publishing the topic for every high-low of PINS
#define IOT_TOPIC_RECORD  "RECORD"
#define IOT_TOPIC_DONE    "DONE"                                         


//AT COMMANDS
#define AT_GPRS        "AT+CSTT=\"airtelgprs.com\",\"pavan\",\"kumar\""
#define AT_CIICR       "AT+CIICR"
#define AT_CIFSR       "AT+CIFSR"
#define AT_CGATT       "AT+CGATT=1"
#define AT_CSQ         "AT+CSQ"
#define AT_CGREG       "AT+CGREG?"
#define AT_APN         "AT+CGDCONT=1,\"IPV4V6\",\"airtelgprs.com\""
#define AT_CGACT       "AT+CGACT=1,1"
#define AT_CGPADDR     "AT+CGPADDR=1"
#define AT_CMGF        "AT+CMGF=1"
#define AT_CLTS        "AT+CLTS=1"
#define AT_W           "AT&W"

//using ARDUINO&SIM7000C pins 
#define PIN_TX          7                      //making arduino digital pin7 as TX i.e pin7 on SIM7000C is RX 
#define PIN_RX          8                      //making arduino digital pin8 as RX i.e pin8 on SIM7000C is TX

//using arduino pin to control lights
#define PIN2            2
#define PIN3            3
#define PIN4            4
#define PIN5            5
#define PIN6            6
#define PIN_RESET       10


#define MAX_BUF_SIZE 128

SoftwareSerial          mySerial(PIN_RX, PIN_TX);
DFRobot_SIM7000         sim7000(&mySerial);       //making sim7000c to communicate serially with arduino


//variables used to publish message and acknowledgement
String DEVICE_STATE = "OFF";
String COMMAND_ID;
String STATE_COMMAND_ID;
String COMMAND;
String STATE;

volatile byte CHECK_COMMAND  = 0;
volatile byte INIT_DEVICE    = 0;
volatile byte STOP_DEVICE    = 0;
volatile byte DAY_DEVICE     = 0;
volatile byte DONE_DEVICE    = 0;



volatile bool received       = false;   //stores boolean=true if mqtt message is received
volatile bool check          = true;    
unsigned long previousMillis = 0;       //stores millis() values to compare timings 

const unsigned long WAIT_1MIN  =  60000UL;
const unsigned long WAIT_20MIN =  1200000UL;
const unsigned long WAIT_2MIN  =  120000UL;
const unsigned long WAIT_5MIN  =  300000UL;

//switch case variables
byte count;
byte SWITCH_CASE =0;
int DAY_COUNT = 0;
int DAY_CYCLE = 0;
int Count;

//variables eeprom
int MEMORY = 0;
int DONE   = 0;

void setup()
{
  int i=0;
  Serial.begin(9600);
  mySerial.begin(19200);
  pinMode(PIN_RESET, OUTPUT);



  // Initially, set the SIM7000C module to normal operation
  digitalWrite(PIN_RESET, HIGH);  // Set RST pin HIGH

  //GPIOs for lights
  pinMode(PIN2, OUTPUT);
  pinMode(PIN3, OUTPUT);
  pinMode(PIN4, OUTPUT);
  pinMode(PIN5, OUTPUT);
  pinMode(PIN6, OUTPUT);

 while(i<=4)
 {
  if (!setupNetwork()) 
  { Serial.println("r...");
    resetMicrocontroller();
    i++;
  }
  else
  {break;}
 }
  setup_sim(true);
}


bool setupNetwork()
{
  int retries = 0;

  while (retries < 3) 
  {
    if (sim7000.turnON())// && sim7000.setBaudRate(19200)) 
    { 
      sendATCommand("AT+IPR=19200");
      if (sim7000.checkSIMStatus())
      {
        Serial.println("RDY");
        return true;
        //break;
      } 
      else
      {
        Serial.println("SMERR");
        delay(1000);
      }
    } 
    else 
    {
      Serial.println("0 SM7k");
      delay(3000);
    }

    retries++;

  }
  return false;
}





void resetMicrocontroller()
{
  wdt_enable(WDTO_15MS); // Enable the watchdog timer with a 15ms timeout
  while (1) {} // Wait for the watchdog timer to reset the microcontroller
}


void Check_Command()
{
  switch (CHECK_COMMAND)
  {
    case 1: INIT_DEVICE = 1; previousMillis = millis(); delay(100); break;
    case 2: STOP_DEVICE = 1; previousMillis = millis(); delay(100); break;
    case 3: DAY_DEVICE  = 1; previousMillis = millis(); delay(100); break;
    default: break; 
  }
}


void setup_sim(bool check)
{
  bool success = false;
  int i=0;
  if(check)
  {
    sendATCommand("AT+CNMP=13");
    delay(100);
    
    sendATCommand(AT_CSQ);
    delay(200);
    
    sendATCommand(AT_GPRS);
    delay(100);

    sendATCommand(AT_CIICR);
    delay(100);

    sendATCommand(AT_CIFSR);   //testing gprs
    delay(100);

    sendATCommand(AT_CGATT);
    delay(2000);

    sendATCommand(AT_CSQ);
    delay(200);

    sendATCommand(AT_CGREG);
    delay(2000);

    sendATCommand(AT_APN);    //setting apn
    delay(2000);

    sendATCommand(AT_CGACT);
    delay(2000);

    sendATCommand(AT_CGPADDR);
    delay(2000);

    sendATCommand(AT_CMGF);
    delay(2000);

    sendATCommand(AT_CLTS);
    delay(2000);

    sendATCommand(AT_W);    //AT+SMSTATE?
    delay(2000);
  }
  while ((!success)&&i<=5)
  {
    if (!connectToNetwork())
    {
      Serial.println(" 0 Nt");
      resetMicrocontroller();
    }
    success = connectMQTT()&& subscribeToTopic();
    i++;
  }


  
  STATE =EEPROM.read(MEMORY);
  bool rstart=false;
  DONE_DEVICE = 1;
  rstart=publish_ack();
  Serial.print(int(EEPROM.read(MEMORY)));
  Serial.println("{..}");
  Retrive_SwitchCaseValues_from_eeprom();
  
  previousMillis=millis();
}


bool connectToNetwork() 
{
  int retries = 0;

  while (retries < 3) 
  {
    if (sim7000.openNetwork(sim7000.eTCP, serverIP, 1883)) 
    {
      Serial.println("1nt");
      return true;
    } 
    else 
    {
      Serial.println("0nt");
      sendATCommand("AT+CIFSR");
      delay(1000);
      retries++;
    }
    sendATCommand("AT+CIFSR");
  }

  return false;
}

bool connectMQTT()
{
  int retries = 0;
  
  while (retries < 3)
  {
    if (sim7000.mqttConnect(IOT_CLIENT, IOT_USERNAME, IOT_KEY))
    {
      Serial.println("1 M");
      return true;
    } 
    else 
    {
      Serial.println("0 M");
      retries++;
    }
    sendATCommand("AT+CIFSR");
  }

  return false;
}

bool subscribeToTopic() 
{
    int retries = 0;
    while (retries < 3) 
    { 
       if (sim7000.mqttSubscribe(IOT_TOPIC1))
       {
         Serial.println("1 s");
         return true;
       } 
       else
       {
         Serial.println("0 s");
         retries++;
       }
    }
     return false;
}

void sendATCommand(String command)
{

    mySerial.println(command);
    delay(500);
    String response;
    while (mySerial.available())
    {
      response = mySerial.readStringUntil('\n');
      Serial.println(response);
    }
}


//varibles to publish alive messege for every particular interval
int j=0,SET_PIN=0;
void loop()
{
     char buffer[MAX_BUF_SIZE];
     received = sim7000.mqttRecv(IOT_TOPIC1, buffer, MAX_BUF_SIZE);
     if (received)
     {
       Serial.println("rcv");
       // Process the received data
          COMMAND_ID.remove(0);
          Serial.println(received);
          processReceivedData(buffer);
          buffer[MAX_BUF_SIZE]={0};
     }
     else {  Serial.println("nr"); }
  

     INIT_DEVICE_AGAIN:

      if (INIT_DEVICE == 1)
      {
        Serial.println("init");
        CHECK_COMMAND = 0;
        byte Pins_Control[] = {1, 20, 2, 20, 5, 20, 0, 15, 120, 127}; //0=init-cmplt,15=day_5min,120=day_20min,127=day_change
        switch (Pins_Control[SWITCH_CASE])
        {
           case 1    :  Serial.println("1");
                        SET_PIN++;
                         setAllPins(HIGH);

                        if (millis() - previousMillis >= WAIT_1MIN)
                        {
                           Serial.println("yes");
                           DEVICE_STATE = "OFF";
                           SWITCH_CASE++;
                           SET_PIN=0;
                           publish_message();
                           previousMillis = millis();
                           Serial.println(millis() / 60000UL);
                        
                        }
                        break;
           case 20   :  Serial.println("20");
                        SET_PIN++;
                        if(SET_PIN==1)  { setAllPins(LOW); }
                        else if(SET_PIN>=10){SET_PIN=0;}

                        if (millis() - previousMillis >= WAIT_20MIN)
                        {
                           Serial.println("in 20");
                           DEVICE_STATE = "ON";
                           SWITCH_CASE++;
                           SET_PIN=0;
                           publish_message();
                           previousMillis = millis();
                           Serial.println(millis() / 60000UL);
                       
                        }
                        break;

            case 2    :   Serial.println("2");
                         SET_PIN++;
                         if(SET_PIN==1) { setAllPins(HIGH); }
                         else if(SET_PIN>=10){SET_PIN=0;}

                         if (millis() - previousMillis >= WAIT_2MIN)
                         {
                           Serial.println("in2");
                           DEVICE_STATE = "OFF";
                           SWITCH_CASE++;
                           SET_PIN=0;
                           publish_message();
                      
                           previousMillis = millis();
                           Serial.println(millis() / 60000UL);
                       
                        }
 
                        break;

            case 5    :   Serial.println("5");
                         SET_PIN++;
                        if(SET_PIN==1)  { setAllPins(HIGH);  } 
                        else if(SET_PIN>=10){SET_PIN=0;}
                        
                        if (millis() - previousMillis >= WAIT_5MIN)
                        {
                         Serial.println("in5");
                         DEVICE_STATE = "OFF";
                         SWITCH_CASE++;
                         SET_PIN=0;
                         publish_message();
                         previousMillis = millis();
                         Serial.println(millis() / 60000UL);
                        }
  
                        break;
              case 0  :   
                        Serial.println("In_Ct");
                        SWITCH_CASE++;
                        if (DAY_COUNT == 0)
                        {
                          Serial.print("D:");
                          Serial.println(DAY_COUNT);
                          Count = 57; //int((60*(DAY_COUNT*24))/25);
                        }
                        previousMillis = millis();
                        Serial.println(millis() / 60000UL);
                        break;
                        
             case 15   :   Serial.println("15");
                           SET_PIN++;
                          if(SET_PIN==1)  { setAllPins(HIGH); } 
                          else if(SET_PIN>=10){SET_PIN=0;}
                          
                          if (millis() - previousMillis >= WAIT_5MIN)
                          {
                            Serial.println("in 5");
                            DEVICE_STATE = "OFF";
                            SWITCH_CASE = 8;
                            SET_PIN=0;
                            publish_message();
                        
                            previousMillis = millis();
                            Serial.println(millis() / 60000UL);
                        
                          }
                         break;
                         
            case 120   :   Serial.println("120");
                           SET_PIN++;
                           if(SET_PIN==1) {   setAllPins(LOW);  }
                           else if(SET_PIN>=10){SET_PIN=0;}
                           
                           if (millis() - previousMillis >= WAIT_20MIN)
                           {
                              Serial.println("in 20");
                              if (DAY_CYCLE == Count)
                              {
                                DEVICE_STATE = "OFF";
                              }
                              else
                              {
                                DEVICE_STATE = "ON";
                              }
                              SWITCH_CASE = 9;
                              SET_PIN=0;
                              publish_message();
                              previousMillis = millis();
                              Serial.println(millis() / 60000UL);
                         
                           }
          
                            break;

             case 127 :    SWITCH_CASE = 7;Serial.println("127");
                   
                           if (DAY_CYCLE >= Count)
                           {
 
                                if ((DAY_DEVICE == 1) && (INIT_DEVICE == 1))
                                {
                                  COMMAND = "DAY";
                                }
                                else if (INIT_DEVICE == 1)
                                {
                                  COMMAND = "RESET";
                                }
                                 STATE = "DONE";
                                 DONE = 1;
                                 EEPROM.write(MEMORY,DONE);
                    
                                 STATE_COMMAND_ID="BISHL";
                                 publish_ack();
                     
                                 INIT_DEVICE = 2;
                     



                              Serial.println("D_C");
          
                              previousMillis = millis();
                           }
                           DAY_CYCLE++;
                           Serial.print("c:");
                           Serial.println(DAY_CYCLE);
                           Serial.println("CNG");
                           break;
        
               default:      break;

    }

  }
  else if (STOP_DEVICE == 1)
  {
 
    CHECK_COMMAND = 0;
    setAllPins(LOW);
    STOP_DEVICE = 2;  //STOP_DEVICE done;
    Write_SwitchCaseValues_to_eeprom();
    previousMillis = millis();
    Serial.println(millis()/60000UL);
    Serial.println("SD");
  }
  else if (DAY_DEVICE == 1)
  {
    Serial.print("1.");
    Serial.println(INIT_DEVICE);
    CHECK_COMMAND = 0;

    if (INIT_DEVICE == 0)
    {
      INIT_DEVICE = 1;
      
    }

    if (INIT_DEVICE == 1)
    {
      Count = int((60 * (DAY_COUNT * 24)) / 25);
      Serial.println("gti");
      DEVICE_STATE = "ON";
      publish_message();
      previousMillis = millis();
      goto INIT_DEVICE_AGAIN;
    }
    else if (INIT_DEVICE == 2)
    {

      if (DAY_CYCLE >= Count)
      {
        DAY_DEVICE = 2;
        Serial.println(Count);
      }
      Serial.println("3.2");
    }
    
  }

  
 
    Serial.println("ivloop");
    
    j++;
    if(j%60==0)
    {
      int x=0;
      resend:
      if (sim7000.mqttPublish(IOT_TOPIC_RECORD,"alive-3"))             //Send data to topic
        Serial.println("Pok..");
      else
      {
        Serial.println("Pno..");
        if(x<=2)
        {
         
          x++;
          delay(300);
          goto resend;
          
        }
        if(j==300)
          j=0;
      }
    }
  
}

void setAllPins(int state) {
  Serial.println(state);
  digitalWrite(PIN2, state);
  digitalWrite(PIN3, state);
  digitalWrite(PIN4, state);
  digitalWrite(PIN5, state);
  digitalWrite(PIN6, state);
  Serial.println("ESP");

}


void publish_message()
{
  int count = 0;
  String sendData;
 // String DEVICE_ID = "ea3064aa-682d-4341-8f0f-86e8abe44f45";                   //"\"73b1cf62-803f-4015-b0b7-05d819a2391f\"";
  String DEVICE_ID = "c8302a8a-9883-4a40-adfb-9303ce3544d4";
  // String sendData1;
  sendData += DEVICE_STATE;
  sendData += ";";
  sendData += DEVICE_ID;

  Serial.println(sendData);
  RE_PUBLISH:

  if(sim7000.mqttPublish(IOT_TOPIC_SEND, sendData))  //Send data to topic
  {             
    Serial.println("P ok");
  }
  else
  {
    if (count <= 2)
    {
      Serial.println("P no");
      if(count==1)
      {

        sim7000.mqttUnsubscribe(IOT_TOPIC1);
        Write_SwitchCaseValues_to_eeprom();
        resetMicrocontroller();
      } 
      delay(500);
      count++;
      goto RE_PUBLISH;
    }
  }
  delay(100);
  sendData.remove(0);
}

bool publish_ack()
{
  int count = 0;
  //RE_PUBLISH:
  String sendData;
  int i = 0;
 // String DEVICE_ID = "ea3064aa-682d-4341-8f0f-86e8abe44f45";
  String DEVICE_ID = "c8302a8a-9883-4a40-adfb-9303ce3544d4";
  sendData += DEVICE_ID;
  sendData += ";";
  if (DONE_DEVICE == 1)
  {
    //EEPROM.write(MEMORY, DONE);
    sendData += STATE;
    DONE_DEVICE = 2;
  }
  else
  {

    sendData += STATE_COMMAND_ID;
    sendData += ";";
    sendData += COMMAND;
    sendData += ";";
    sendData += STATE;
  }
  Serial.println(sendData);
  RE_PUBLISH:

  if (sim7000.mqttPublish(IOT_TOPIC_ACK, sendData))
  { //Send data to topic
    Serial.println("P ok");
    
    delay(500);
    sendData.remove(0);
    return true;
  }
  else
  {
    if (count <= 2)
    {
      
      Serial.println("P no");
 
      delay(500);
      count++;
      goto RE_PUBLISH;
    }
     delay(500);
     sendData.remove(0);
     return false;
  }
 

}

void processReceivedData(char* data)
{
  // Do whatever you need to do with the received data
  // For example, you can print it, parse it, or take any other actions

  // Example: Printing the received data
 // char* DEVICE_ID="ea3064aa-682d-4341-8f0f-86e8abe44f45";
  char* DEVICE_ID = "c8302a8a-9883-4a40-adfb-9303ce3544d4";
  char* STATE1 = "RESET";
  char* STATE2 = "STOP";
  char* STATE4 = "DONE";
  int state1 = 0, state2 = 0, DEVICE;


  Serial.println("Rd");
  Serial.println(data);

  int i = 0, j = 0;


  while (data[i] != ';')
  {
    i++;
  }
  i++;
  //DEVICE_ID.c_str();
  if (strstr(data, DEVICE_ID))
  {
    DEVICE = 1;
    Serial.println("Dfnd");
  
  if (DEVICE)
  {

    while (data[i] != ';')
    {
      COMMAND_ID += data[i];
      i++;
    }
    i++;

    if (strstr(data, STATE4))
    {
      Serial.println("Dn-cd");
      STATE = EEPROM.read(MEMORY);
      DONE_DEVICE = 1;
      publish_ack();
      COMMAND = "DONE";
   }
    else if (strstr(data, STATE1))
    {
      Serial.println("reset");
      STATE_COMMAND_ID = "";
      DONE = 0;
      EEPROM.write(MEMORY, DONE);
      // delayMicroseconds(500);
      STATE_COMMAND_ID = COMMAND_ID;
      CHECK_COMMAND = 1;
      STOP_DEVICE = 0;
      DAY_DEVICE = 0;
      DONE_DEVICE = 0;
      SWITCH_CASE = 0;
      DAY_CYCLE = 0;
      COMMAND = "RESET";
      STATE = "OK";
      Check_Command();
      publish_ack();
    }
    else if (strstr(data, STATE2))
    {
      Serial.println("stop");
      STATE_COMMAND_ID = "";
      STATE_COMMAND_ID=COMMAND_ID;
      CHECK_COMMAND = 2;
      DAY_DEVICE = 0;
      INIT_DEVICE = 0;
      DONE_DEVICE = 0;
      COMMAND = "STOP";
      STATE = "OK";
      Check_Command();
      publish_ack();
    }
    else 
    {
       int semicolonIndex1 = String(data).indexOf(';', String(data).indexOf(';') + 1);
       int semicolonIndex = String(data).indexOf(';', semicolonIndex1 + 1);
       String integerString = String(data).substring(semicolonIndex1 + 1, semicolonIndex);

      // Convert the substring to an integer
       DAY_COUNT = integerString.toInt();

      // Print the result
      Serial.print("Ext-int: ");
      Serial.println(DAY_COUNT);
      STATE_COMMAND_ID = "";
      if((DAY_COUNT==1)||(DAY_COUNT==2)||(DAY_COUNT==3))
      {
      CHECK_COMMAND = 3;
      DONE = 0;
      EEPROM.write(MEMORY, DONE);
      STATE_COMMAND_ID = COMMAND_ID;
      INIT_DEVICE = 0;
      STOP_DEVICE = 0;
      DONE_DEVICE = 0;
      DAY_CYCLE = 0;
      SWITCH_CASE = 0;
      COMMAND = "TIME";
      STATE = "OK";
      Check_Command();
      publish_ack();
       }
      else
      {
         Serial.println("buf");
      }
     
    }
    
 
  }
  }
  else
  {
    Serial.println("NOT");
  }
  DEVICE = 0;
}

void Write_SwitchCaseValues_to_eeprom()       //(String E_INIT,String E_DAY,String E_STOP,String E_COMMAND_ID)
{
  
  //integer values  
    EEPROM.write(MEMORY+1,INIT_DEVICE);  //MEMORY=0 using for update done value  //storing INIT_DEVICE value  to mem=1
    EEPROM.write(MEMORY+2,DAY_DEVICE);   //storing DAY_DEVICE value  to mem=2
    EEPROM.write(MEMORY+3,true);  //storing eeprom_written_status value  to mem=3
  
                          //  EEPROM.write(MEMORY+(i+4), '\0'); // Null-terminate the string
                          //  EEPROM.commit(); // Commit the changes to EEPROM

   
   //integer values - DAY_CYCLE                    
   EEPROM.write(MEMORY+10,DAY_COUNT);    //mem=10 storing DAY_COUNT value
   EEPROM.write(MEMORY+11,Count);       //mem=11  storing Count value
   EEPROM.write(MEMORY+12,DAY_CYCLE);   //mem=12  storing DAY_CYCLE
   
   EEPROM.write(MEMORY+13,SWITCH_CASE);   //mem=13 storing SWITCH_CASE
}
void Retrive_SwitchCaseValues_from_eeprom()
{
  //int values
   INIT_DEVICE=byte(EEPROM.read(MEMORY+1));
   DAY_DEVICE=byte(EEPROM.read(MEMORY+2));
   DAY_COUNT=int(EEPROM.read(MEMORY+10));
   Count=int(EEPROM.read(MEMORY+11));
   DAY_CYCLE=int(EEPROM.read(MEMORY+12));
   SWITCH_CASE=byte(EEPROM.read(MEMORY+13));

   //String value
 
}