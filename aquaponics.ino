#include <EEPROM.h>
#include <Wire.h>
#include <Time.h>
#include "RTClib.h"
#include <NewPing.h>
#include <Servo.h>
#include <OneWire.h> 
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#define DEBUG 1

#define UP 7
#define DOWN 9
#define OK 6
#define BACK 5

#define PIN_LIGHT_FISH 3
#define PIN_LIGHT_PLANTS 4
#define PIN_WATER_PUMP 5
#define PIN_NIGHT_LIGHT 6
#define PIN_ECHO 7
#define PIN_TRIG 8
#define PIN_FOOD 9
#define PIN_TEMP 10
#define PIN_BL 13

#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

Adafruit_PCD8544 display = Adafruit_PCD8544(A3, A0, 11, A1, 12);
NewPing sonar(PIN_TRIG, PIN_ECHO, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
Servo food_servo;
OneWire ds(PIN_TEMP);


/*
10 hours fish light
12 to 14 hours plant light
*/

const char *m_menu[] = {
  "Clock",
  "Lighting",
  "Feeding freq",
  "Level",
  "Pump interv.",
  "Def. values" 
};

int items_m_menu = 6;

const char *light_menu[] = {
  "Plant start",
  "Plant dur.",
  "Fish start",
  "Fish dur."  
};

int items_light_menu = 4;


const char *level_menu[] = {
  "Top",
  "Bottom"
};

int items_level_menu = 2;

unsigned long last_water = 0;
unsigned long last_time_check = 0;
unsigned long water_interval = 1200000; //Every 20 minutes
unsigned long time_check_interval = 60000; //Every minute
unsigned long last_int = 0;
unsigned long alarm_light_int = 500;
unsigned long last_alarm_light = 0;
unsigned long pause_alarm_light = 5000;

uint8_t offset_sel = 0;
int selection = 0;
int pressed_button = 0;
int state = 0;

boolean time_ok = true;
boolean refresh_screen = true;
boolean int_flag = false;
boolean pumping_water = false;
boolean full_water = false;
boolean night_mode = false;
boolean light_plants = false;
unsigned int min_level = 0;
unsigned int max_level = 0;
uint8_t cont_pump = 0;

uint8_t food_pharse = 0;
const uint8_t food_num_pharses = 1;
uint8_t food_hours[1] = {9};
const uint8_t servo_up = 180;
const uint8_t servo_down = 0;
uint8_t food_moves_servo = 1;

uint8_t light_fish_hours[2] = {9, 19};
uint8_t light_plant_hours[2] = {9, 21};
uint8_t level_dist[2] = {50, 110};

//food moves, fish start, fish duration, plant start, plant duration, level top, level bottom, pump interval
uint8_t def_vals[] = {1, 9, 19, 9, 21, 50, 110, 20};

// hours - min- sec
uint8_t clock_d[3] = {0, 0, 0};

RTC_DS1307 rtc;
DateTime now;

void setup() 
{
  pinMode(PIN_WATER_PUMP, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_NIGHT_LIGHT, OUTPUT);
  pinMode(PIN_LIGHT_FISH, OUTPUT);
  pinMode(PIN_LIGHT_PLANTS, OUTPUT);
  pinMode(PIN_BL, OUTPUT);    
  pinMode(PIN_ECHO, INPUT);
  
  
  digitalWrite(PIN_WATER_PUMP, LOW);
  digitalWrite(PIN_NIGHT_LIGHT, LOW);
  digitalWrite(PIN_BL, HIGH);
  Serial.begin(9600);
  
  display.begin();
  display.setContrast(50); 
  display.clearDisplay();  
  display.display();

  readEeprom();  

  time_ok = readTime();
  
  if (time_ok) {
    checkFood();
    checkLight(true);
    setFoodStage();
  }
  
  attachInterrupt(digitalPinToInterrupt(2), buttonInt, RISING);
}


void loop() 
{
  if (int_flag) {
    int_flag = false;  
    pressed_button = analogRead(A2) / 100;

    if (pressed_button > 4) {
      if (digitalRead(PIN_BL) == LOW) {
        digitalWrite(PIN_BL, HIGH);
      }
      
      actionButton();
      refresh_screen = true;
    }
  
    pressed_button = 0;
  }

  if (state == 0 && (millis() % 1000 == 0)) {
    readTime();
    refresh_screen = true;
  }
  
  if (refresh_screen) {
    printScreen(); 
    refresh_screen = false;
  }
  
  checkWaterPump();
  
  if (compareTime(last_time_check, time_check_interval)) {
    time_ok = readTime();
    if (time_ok) {
      checkFood();
      checkLight();
    }
  }

  if (!time_ok && compareTime(last_alarm_light, alarm_light_int) && ((millis() - last_int) > pause_alarm_light)) {
      time_ok = readTime();
      if (time_ok) {
        digitalWrite(PIN_BL, HIGH);
      } else {
        digitalWrite(PIN_BL, !digitalRead(PIN_BL));
      }
  }
}


boolean readTime()
{
  if (!rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    return false;
  }
  
  if (!rtc.isrunning()) {
    Serial.println(F("MUST ADJUST"));
    return false;
  }

  now = rtc.now();

  return true;
}


void checkFood()
{
  if ((now.hour() == food_hours[food_pharse]) && (now.minute() == 0)) {
    food_servo.attach(PIN_FOOD);
    food_servo.write(servo_up);
    delay(800);
    
    for (uint8_t cont = 0; cont < food_moves_servo; cont++) {
      food_servo.write(servo_down);
      if (DEBUG) {
        Serial.print(F("Servo down")); 
      }
     
      delay(800);
      
      food_servo.write(servo_up);
      
      if (DEBUG) {
        Serial.print(F("Servo up")); 
      }
      
      delay(800);
    }
     food_pharse++;
     
     if (food_pharse >= food_num_pharses) {
      food_pharse = 0; 
     }
     
    food_servo.detach();
 }
  /*
  if (food_pharse > 0) {
    if (now.hour() >= food_hours[food_pharse]) {
      food_pharse++;
      if (food_pharse == food_num_pharses) {
        food_pharse = 0;
      }
    }
  } else if((now.hour() < food_hours[2]) && (now.hour() < food_hours[0])) {
    food_pharse++;      
  }*/
}


void checkLight()
{
  checkLight(false);
}

void checkLight(boolean initial)
{
  if (now.hour() >= light_fish_hours[0] && now.hour() < light_fish_hours[1]) {
    if (initial || night_mode) {
      night_mode = false;
      digitalWrite(PIN_LIGHT_FISH, HIGH); 
      digitalWrite(PIN_NIGHT_LIGHT, LOW); 
      if (DEBUG) {
        Serial.println(F("Day light mode."));
      }
    }
  } else if (initial || !night_mode) {
    night_mode = true;
    digitalWrite(PIN_LIGHT_FISH, LOW); 
    digitalWrite(PIN_NIGHT_LIGHT, HIGH); 
    if (DEBUG) {
      Serial.println(F("Night light mode."));
    }
  }
  
  
  if (now.hour() >= light_plant_hours[0] && now.hour() < light_plant_hours[1]) {
    if (initial || light_plants) {
      light_plants = false;
      digitalWrite(PIN_LIGHT_PLANTS, HIGH); 
      if (DEBUG) {
        Serial.println(F("Plant light ON."));
      }
    }
  } else if (initial || !light_plants) {
    light_plants = true;
    digitalWrite(PIN_LIGHT_PLANTS, LOW); 
    if (DEBUG) {
      Serial.println(F("Plant light OFF."));
    }
  }
}


void checkWaterPump()
{  
  if (pumping_water) {
    
    unsigned long median = 0;
    for (uint8_t cont = 0; cont < 5; cont++) {
     median += sonar.ping_median();
     delay(10);
    }
    
    median /= 5;
    
    if (!full_water) {
      if (median >= min_level) {
        cont_pump++;
        
        if (cont_pump >= 3) {
          cont_pump = 0;
          full_water = true; 
          
          if (DEBUG) {
            Serial.println(F("Grow bed full."));
          }
        }
      } else {
        cont_pump = 0;
      }
    } else {
      Serial.println(median);
      if (median <= max_level) {
        cont_pump++;
        
        if (cont_pump >= 3) {
          
          if (DEBUG) {
            Serial.println(F("Grow bed flushed."));
          }
          
          digitalWrite(PIN_WATER_PUMP, LOW);
          pumping_water = false; 
          full_water = false;
          cont_pump = 0;
        }
      } else {
        cont_pump = 0;
      }
    }
    
  } else if (compareTime(last_water, water_interval)) {
    unsigned long cont_level = 0;
    
    for (uint8_t cont = 0; cont < 5; cont++) {
      cont_level += sonar.ping_median();
      delay(50);
    }
    cont_level /= 5;
    
    max_level = cont_level + level_dist[0];
    min_level = max_level + level_dist[1];
    
    
    if (DEBUG) {
      Serial.println("Starting flood cycle... \nCurrent level: " + String(cont_level) + "\nMinimum water level: " + String(min_level) + "\nMaximum water level: " + String(max_level));
    }
    
    digitalWrite(PIN_WATER_PUMP, HIGH);
    last_water = millis();
    pumping_water = true; 
  }
}



void setFoodStage()
{
  uint8_t cont = 0;
  
  for (cont = 0; cont < food_num_pharses; cont++) {
    if (now.hour() <= food_hours[cont]) {
      food_pharse = cont;
      break;
    }
  }
  
  if (DEBUG) {
   Serial.println("Food stage: " + String(food_pharse)); 
  }
}


boolean compareTime(unsigned long &last, unsigned long interval)
{
  if ((millis() - last) >= interval) {
    last = millis();
    return true;
  }
  
  return false;
}


void checkWaterLevel()
{
  long duration = 0;
  
  for (uint8_t i = 0; i < 5; i++) {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(5);
    
    digitalWrite(PIN_TRIG, LOW);
    
    duration += pulseIn(PIN_ECHO, HIGH);
  }
  
  duration /= 5;
  
  
  Serial.println(String(duration/29/2));
}


float getTemp()
{
 //returns the temperature from one DS18S20 in DEG Celsius

  byte data[12];
  byte addr[8];
  
  if ( !ds.search(addr)) {
   //no more sensors on chain, reset search
   ds.reset_search();
   return -1000;
  }
  
  if ( OneWire::crc8( addr, 7) != addr[7]) {
   Serial.println(F("CRC is not valid!"));
   return -1000;
  }
  
  if ( addr[0] != 0x10 && addr[0] != 0x28) {
   Serial.print(F("Device is not recognized"));
   return -1000;
  }
  
  ds.reset();
  ds.select(addr);
  ds.write(0x44,1); // start conversion, with parasite power on at the end
  
  byte present = ds.reset();
  ds.select(addr);  
  ds.write(0xBE); // Read Scratchpad
  
  
  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }
  
  ds.reset_search();
  
  byte MSB = data[1];
  byte LSB = data[0];
  
  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  float TemperatureSum = tempRead / 16;
  return TemperatureSum;
}



// ############################################## User Interface ##############################################

void printScreen()
{  
  switch (state) {
    // Init screen
    case 0:
      initScreen();
      break;
    
    // Set menu
    case 1:
      printMenu(m_menu, items_m_menu, "SETUP");
      break; 
      
    // Set Frequency
    case 2:
      setItem(food_moves_servo, "", "FEEDING");
      break;
      
    // Set Clock
    case 3:
      setClock();
      break;
      
    // Set pum interval
    case 4:
      setItem(water_interval / 60000, " min", "PUMP INTERVAL");
      break;
      
    // Light menu
    case 5:
      printMenu(light_menu, items_light_menu, "LIGHTING");
      break;
      
    case 6:
      setItem(light_plant_hours[0], " h", "PLANT START");
      break;
      
    case 8:
      setItem(light_fish_hours[0], " h", "FISH START");
      break;
       
    case 7:
      setItem(computeDuration(light_plant_hours[0], light_plant_hours[1]), " h", "PLANT DUR.");
      break;
      
    case 9:
      setItem(computeDuration(light_fish_hours[0], light_fish_hours[1]), " h", "FISH DURATION");
      break;
      
    // Level menu
    case 10:
      printMenu(level_menu, items_level_menu, "LEVEL");
      break;
      
    case 11:
      setItem(level_dist[0], "", "TOP");
      break;
      
    case 12:
      setItem(level_dist[1], "", "BOTTOM");
      break;
      
    case 13:
      display.clearDisplay();
      display.setTextColor(BLACK);
      display.setCursor(4, 20);
      display.println("Are you sure?");
      display.display();

      break;
  }
}


void initScreen() 
{
  display.clearDisplay();
  
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  
  printClock();
  display.println("");
  
  display.print("Freq: ");
  display.println(food_moves_servo);


  display.print("Plant: ");
  display.print(light_plant_hours[0]);
  display.print(" - ");
  display.println(light_plant_hours[1]);
  
  display.print("Fish: ");
  display.print(light_fish_hours[0]);
  display.print(" - ");
  display.println(light_fish_hours[1]);
  
  display.print("Temp: ");
  display.println(getTemp());

  display.print("Level: ");
  display.println(sonar.ping_median());
  display.display();
}


void printClock()
{
  printZero(now.hour());
  display.print(now.hour()); 
  display.print(":");
  printZero(now.minute());
  display.print(now.minute()); 
  display.print(":");
  printZero(now.second());
  display.print(now.second());   
}


void printZero(int num) 
{
  if (num < 10) {
    display.print(0);  
  }
}


void printMenu(const char **items, int num_items, String tittle)
{      
  display.clearDisplay();
  
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  
  display.setTextSize(1);
  
  display.setCursor((display.width() / 2) - (((tittle.length() / 2) * 6)),0);
  
  display.println(tittle);

  int y = 11;// (num_items > 4) ? 5 : 11;
  display.setTextSize(1);
  
  for (int i = 0; i < (num_items < 4 ? num_items : 4); i++) {
    if (selection == i) {
      display.drawLine(0, y + 3, 4, y + 3, BLACK);  
    }
    
    display.setCursor(7, y);
    display.println(items[i + offset_sel]);
    y += 9;
  }
    

  display.display();
}


void setItem(int value, String post, String tittle)
{
  display.clearDisplay();
  
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  
  display.setTextSize(1);
  
  display.setCursor((display.width() / 2) - (((tittle.length() / 2) * 7)),0);
  display.println(tittle);
  
  display.setTextSize(2);
  display.setCursor((display.width() / 2) - (4 + ((post.length() / 2) * 15)), (display.height() / 2) - 8);
  display.print(value);
  display.println(post);
  
  display.display();
}


void setClock() 
{
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(24,0);
  display.println("CLOCK");
  display.setTextSize(1);
  display.setCursor(15, (display.height() / 2) - 4);
  
  printZero(clock_d[0]);
  display.print(clock_d[0]); 
  display.print(":");
  printZero(clock_d[1]);
  display.print(clock_d[1]); 
  display.print(":");
  printZero(clock_d[2]);
  display.print(clock_d[2]);   
  
  display.drawLine((15 + (selection * 18)), (display.height() / 2) + 4, 15 + ((selection * 18) + 10), (display.height() / 2) + 4, BLACK); 
  
  display.display();
  
}

/*
-Set clock
-Feed times
-Difference
-Light time

*/


void actionButton()
{
  int tmp_interval;
  int states[10];
  
  switch (state) {
    case 0:
      if (pressed_button == 6) {
        state = 1;
        selection = 0;
      }
      break;
    
    case 1:
    
        states[0] = 3;
        states[1] = 5;
        states[2] = 2;
        states[3] = 10;
        states[4] = 4;
        states[5] = 13;
        processMenu(states, 0, items_m_menu);
        
        if (state == 3) {
          clock_d[0] = now.hour();
          clock_d[1] = now.minute();
          clock_d[2] = now.second();
        }
      
      break;
      
      case 2:  
          processItem(food_moves_servo, 1, 0, 5);
          break;
        
        case 3:
          switch (pressed_button) {
            case BACK:
              if (selection == 0) {
                state = 1;  
                selection = 0;
                storeClock();
              } else {
                selection--; 
              }
              break;
              
            case OK:
              if (selection == 2) {
                state = 1;  
                selection = 0;
                storeClock();
              } else {
                selection++; 
              }
              break;
              
            case UP:
              if (selection == 0) {
                clock_d[selection] = (clock_d[selection] < 24) ? clock_d[selection] + 1 : 0 ;
              } else {
                clock_d[selection] = (clock_d[selection] < 60) ? clock_d[selection] + 1 : 0 ;  
              } 
              break;
              
            case DOWN:
              if (selection == 0) {
                clock_d[selection] = (clock_d[selection] > 0) ? clock_d[selection] - 1 : 23 ;
              } else {
                clock_d[selection] = (clock_d[selection] > 0) ? clock_d[selection] - 1 : 59 ;  
              } 
              break;
          }            
          break;
          
        case 4:
              
          tmp_interval = water_interval / 60000;
          switch (pressed_button) {
            case BACK:
            case OK:
              storeEeprom();
              selection = 0;
              state = 1;
              break;
              
            case UP:
              tmp_interval = (tmp_interval < 60) ? tmp_interval + 1 : 5;
              water_interval = tmp_interval * 60000;
              break;
              
            case DOWN:
              tmp_interval = (tmp_interval > 5) ? tmp_interval - 1 : 60;
              water_interval =  tmp_interval * 60000;
              break;
          }  
          break;
          
          
        case 5:
          states[0] = 6;
          states[1] = 7;
          states[2] = 8;
          states[3] = 9;
          processMenu(states, 1, items_light_menu);
          break;
        
        case 6:
          processStart(light_plant_hours);
          break;  
          
        case 7:
          processDuration(light_plant_hours);      
          break;
           
        case 8:
          processStart(light_fish_hours);
          break;  
          
        case 9:
          processDuration(light_fish_hours);
          break;
          
          
        case 10:
          states[0] = 11;
          states[1] = 12;
          processMenu(states, 1, items_level_menu);
          break;
          
        case 11:
            processItem(level_dist[0], 10, 10, 255);
            break;
         
        case 12:
          processItem(level_dist[1], 10, 10, 255);
          break;
          
        case 13:
          switch (pressed_button) {
            case BACK:
              selection = 0;
              state = 1;
              break;
              
            case OK:
              setDefaults();
              storeEeprom();
              selection = 0;
              state = 0;
              break;
          }  
          break;
  } 
}

void storeClock()
{
  rtc.adjust(DateTime(2014, 1, 21, clock_d[0], clock_d[1], clock_d[2]));
}

void processItem(uint8_t &value, int ret_state, int minim, int maxim) 
{
  switch (pressed_button) {
    case BACK:
    case OK:
      storeEeprom();
      selection = 0;
      state = ret_state;
      break;
      
    case UP:
      value = (value < maxim) ? value + 1 : minim;
      break;
      
    case DOWN:
      value = (value > minim) ? value - 1 : maxim;
      break;
  }  
}

void processMenu(int *ok_states, int ret_state, int items) 
{
  switch (pressed_button) {
    case BACK:
      state = ret_state;
      selection = 0;
      offset_sel = 0;
      break;
      
    case OK:
      state = ok_states[selection + offset_sel];
      selection = 0;
      offset_sel = 0;
      break;
      
    case UP:
      if (offset_sel == 0 && selection > 0) {
        selection -= 1;
      }
      //selection = (selection > 0) ? selection - 1 : items - 1;
      
      if (offset_sel > 0) {
        offset_sel -= 1;
      }
      
      break;
      
    case DOWN:
      if (selection == 3 && ((selection + offset_sel) < (items - 1))) {
        offset_sel += 1;
      }
      
      if ((selection < (items - 1)) && selection < 3) {
        selection += 1;
      }
      //selection = (selection < items - 1) ? selection + 1 : 0;
      
      break;
  }  
}

void processStart(uint8_t *hours) 
{
  switch (pressed_button) {
    case BACK:
    case OK:
        storeEeprom();
        state = 5;  
        selection = 0;
      break;
      
    case UP:
        hours[0] = (hours[0] < 23) ? hours[0] + 1 : 0 ;
        hours[1] = (hours[1] < 23) ? hours[1] + 1 : 0 ;
      break;
      
    case DOWN:
        hours[0] = (hours[0] > 0) ? hours[0] - 1 : 23 ;
        hours[1] = (hours[1] > 0) ? hours[1] - 1 : 23 ;
      break;
  }
}

void processDuration(uint8_t *hours) 
{
  switch (pressed_button) {
    case BACK:
    case OK:
      storeEeprom();
      state = 5;  
      selection = 0;
    break;
    
    case UP:                
      if (computeDuration(hours[0], hours[1])  < 23) {
        hours[1] = (hours[1] < 23) ? hours[1] + 1 : 0 ;
      }
    break;
    
    case DOWN:
      if (computeDuration(hours[0], hours[1])  > 1) {
        hours[1] = (hours[1] > 0) ? hours[1] - 1 : 23 ;
      }
    break;
  }  
}


int computeDuration(int h_start, int h_end) 
{
  if (h_start < light_plant_hours[1]) {
     return h_end - h_start;
  } else {
    return (24 - h_start) + h_end;
  } 
}


void setDefaults()
{
  food_moves_servo = def_vals[0];
  light_fish_hours[0] = def_vals[1];
  light_fish_hours[1] = def_vals[2];
  light_plant_hours[0] = def_vals[3];
  light_plant_hours[1] = def_vals[4];
  level_dist[0] = def_vals[5];
  level_dist[1] = def_vals[6];
  water_interval = def_vals[7] * 60000;
}


void storeEeprom() 
{
  switch (state)
  {
    case 2:
      EEPROM.write(0, food_moves_servo);
      break;
      
    case 4:
      EEPROM.write(7, water_interval / 60000);
      break;
      
    case 6:
      EEPROM.write(3, light_plant_hours[0]);
      break;
      
    case 7:
      EEPROM.write(4, light_plant_hours[1]);
      break;
      
    case 8:
      EEPROM.write(1, light_fish_hours[0]);
      break;
      
    case 9:
      EEPROM.write(2, light_fish_hours[1]);
      break;
      
    case 11:
      EEPROM.write(5, level_dist[0]);
      break;
      
    case 12:
      EEPROM.write(6, level_dist[1]);
      break;      
      
    case 13:
      EEPROM.write(0, food_moves_servo);
      EEPROM.write(1, light_fish_hours[0]);
      EEPROM.write(2, light_fish_hours[1]);
      EEPROM.write(3, light_plant_hours[0]);
      EEPROM.write(4, light_plant_hours[1]);
      EEPROM.write(5, level_dist[0]);
      EEPROM.write(6, level_dist[1]);
      EEPROM.write(7, water_interval / 60000);
      break;
  }
}

void readEeprom()
{
  food_moves_servo = EEPROM.read(0);
  light_fish_hours[0] = EEPROM.read(1);
  light_fish_hours[1] = EEPROM.read(2);
  light_plant_hours[0] = EEPROM.read(3);
  light_plant_hours[1] = EEPROM.read(4);
  level_dist[0] = EEPROM.read(5);
  level_dist[1] = EEPROM.read(6);
   water_interval = EEPROM.read(7) * 60000;
}

void buttonInt()
{
  if ((millis() - last_int) > 200) {
    int_flag = true;
    last_int = millis();
  }
}
