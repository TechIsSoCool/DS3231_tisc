//Include the Wire library and the DS3231_tisc code
#include <Wire.h>
#include "DS3231_tisc.h"
#include "colors.h"   //Simply a list of #define color names to hex color codes, 
                      //used for display, like '#define WHITE 0xffff', can omit
                      //and #define what you use in this file instead

//For 3.5" TFT LCD Touchscreen display - may not apply to your hardware
#include <TouchScreen.h>
#include <LCDWIKI_GUI.h>
#include <LCDWIKI_KBV.h>
#define TS_MINX 906
#define TS_MAXX 116
#define TS_MINY 92
#define TS_MAXY 952
#define MINPRESSURE 10
#define MAXPRESSURE 1000
#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin


//Constants to refer to home screen user buttons
//You may also use these to refer to hardware buttons
//used in loop() and check_button_press()
#define BTN_NO_BUTTON 0
#define BTN_SET_TIME 1
#define BTN_SET_ALARM_1 2
#define BTN_SET_ALARM_2 3
#define BTN_ALARM_TOGGLE 4

//References to Arrays for user settings below
//Used when calling getUserArrayChoice() and
//in that function
#define MONTH_L 0
#define MONTH_S 1
#define WEEKDAY_L 2
#define WEEKDAY_S 3
#define ALARM_FREQUENCY 4

//The pin number that DS3231 SQW/!INT is connected to
//TODO: Change this based on your hardware
//See this page if you have questions about which pins you can use: 
//https://www.arduino.cc/reference/tr/language/functions/external-interrupts/attachinterrupt/
#define ALARM_INTERRUPT_PIN 18

//Uncomment this next line to see feedback on Serial port, adds 2300 bytes to code size
//#define DEBUG

//Global Variables
boolean twelveHourMode = true; // 12/!24 mode, mirrors DS3231 flag value in time register
volatile boolean alarmTripped = false; //Set when alarm interrupt happens, cleared in loop when alarm handled

//--next 4 uncommented lines for LCD & touchscreen
//TODO: Update for your hardware - instantiate any global objects needed and set any global display vars
LCDWIKI_KBV lcd(ILI9486,A3,A2,A1,A0,A4); //Init LCD, declare 'lcd' var (model,cs,cd,wr,rd,reset)
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
int w = 0; //display width
int h = 0; //display height

//Arrays used in setting time, date, and alarms
String weekdays_l[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String month_l[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
uint8_t days_per_month[12] = {31,29,31,30,31,30,31,31,30,31,30,31};
char ampm[2][3] = {"AM","PM"};
String alarmFreqOptions[3] = {"Every Day", "On Date", "On Weekday"};

//If you want to use short strings for your display, uncomment this next array and swap weekdays_l 
//for weekdays_s - find & replace throughout this file
//String weekdays_s[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
//If you want to use short strings for your display, uncomment this next array and swap month_l 
//for month_s - find & replace throughout this file
//String month_s[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

////////////////////////////////////
// setup()
////////////////////////////////////
void setup() {
  //DS3231 Interrupt pin is open-drain so needs a pullup
  pinMode(ALARM_INTERRUPT_PIN, INPUT_PULLUP);
  #ifdef DEBUG
  Serial.begin(9600);       //Init serial port
  while(!Serial){;}         //Wait for serial port connection
  #endif
  //Initialize the I2C bus using Arduino Wire library
  Wire.begin();
  //Initialize LCD, draw buttons
  initializeDisplay();    

  //Initialize the DS3231 
  initializeDS3231();
  //'alarmHandler' is the routine called when an interrupt happens (interrupt service routine / ISR)
  //Trigger interrupt on falling edge, otherwise the Arduino gets stuck when the pin stays low. 
  attachInterrupt(digitalPinToInterrupt(ALARM_INTERRUPT_PIN), alarmHandler, FALLING);
}
////////////////////////////////////
// loop()
////////////////////////////////////
void loop() {
  uint8_t button;
  uint8_t whichAlarmTripped;
  //Get the time & date - see DS3231_TISC.h for definitions of Date and Time classes
  Date d = readDate();
  Time t = readTime();
  //Update the display of date & time
  displayTimeDate(t, d);
  delay(100);
  //Check for user input - returns 0 if no button
  button = check_button_press();
  if(button) { //if non-zero, a button was pressed
    switch(button) {
      case BTN_SET_TIME:      //Calls enterNewTime which gets settings from user and returns Time object, 
                              //that object gets passed to setTime() which writes the time values to the DS3231
                              setTime(enterNewTime()); 
                              //Calls enterNewDate which gets settings from user and returns Date object, 
                              //that object gets passed to setDate() which writes the date values to the DS3231
                              setDate(enterNewDate());
                              //leave the switch statement
                              break;
                              
      case BTN_SET_ALARM_1:   //Calls enterNewAlarm with parameter of alarm number,gets settings from user and returns 
                              //an AlarmSetting object, that object gets passed to setAlarm() which writes the alarm
                              //values to the DS3231
                              setAlarm(enterNewAlarm(1));
                              //User has configured alarm settings, so I'll turn on that alarm
                              turnAlarmOn(1);
                              //Alarm on/off status has changed, so update the display of alarm status
                              showAlarmStatus(getAlarmStatus());
                              break;
                              
      case BTN_SET_ALARM_2:   setAlarm(enterNewAlarm(2));
                              turnAlarmOn(2);
                              showAlarmStatus(getAlarmStatus());
                              break;
                              
      case BTN_ALARM_TOGGLE:  //toggleAlarms() turns interrupt enable flags for Alarm 1 & 2 on and off
                              //in a cycle (Both off, 1 only, 2 only, both on) and returns the state
                              //of the flags. showAlarmStatus upates the UI to match the new settings
                              showAlarmStatus(toggleAlarms());
                              break;
                              
      default:                //button was non-zero but not a predefined value - ?!
                              #ifdef DEBUG
                              Serial.print("loop: ERROR: Button value returned but not defined - value was: "); Serial.println(button);
                              #endif
                              break;
    }//end switch
  } //end if(button)
  
  //Check to see the alarm interrupt was triggered
  if(alarmTripped) {
    //Figure out which alarm was tripped, and clear it's flag in the DS3231
    //serviceAlarms() returns 0 for none, 1 for alarm 1, 2 for alarm 2, 3 for both
    whichAlarmTripped = serviceAlarms();
    if(whichAlarmTripped & 1) {
    #ifdef DEBUG
    Serial.println("Alarm 1 Tripped");
    #endif
    displayAlarm(1);
    }
    if(whichAlarmTripped & 2) {
      #ifdef DEBUG
      Serial.println("Alarm 2 Tripped");
      #endif
      displayAlarm(2);
    }
    //clear the global flag set by the interrupt service routine
    alarmTripped = false;
  }//end if(alarmTripped) 
}//end loop()

////////////////////////////////////////////////////////////////////
// displayTimeDate() - called from setup()
// Rewrite this routine to display the time and date on your display
////////////////////////////////////////////////////////////////////
void displayTimeDate(Time t, Date d){
  //show will hold weekday, then date string
  String show;
  //disp will hold time string - longest => '12:00:00 PM'
  char disp[12];  //Time needs 12 chars (including terminating null)
 
  lcd.Set_Text_colour(GREEN);
  lcd.Set_Text_Back_colour(BLACK);
  
  //Weekday - select the [weekday-1]th string from the array
  //ex: d.weekday = 1, get string weekdays_l[0]
  //weekday is 1-based, weekdays_l is 0-based
  show = String(weekdays_l[d.weekday-1]);
  lcd.Set_Text_Size(4);
  lcd.Print_String(show,20,50);
  //Display Time - using sprintf to create a formatted string, 
  //allows adding colons, forcing one digit entries to have a leading zero
  if(twelveHourMode) {
    sprintf(disp,"%2d:%02d:%02d %s",t.hour12,t.minute,t.second,ampm[t.pm]);
  }
  else {
    sprintf(disp,"%2d:%02d:%02d",t.hour24,t.minute,t.second);
  }
  lcd.Set_Text_Size(5); 
  lcd.Print_String(disp,10,130);
  //Show Date, passing strings and numbers to String constructor, very handy!
  show = String(month_l[d.month-1] + " " + d.date + ", " + d.year);
  lcd.Set_Text_Size(3); 
  lcd.Print_String(show,20,225);
}
///////////////////////////////////////////
// Time enterNewTime() - called from loop()
// Rewrite this routine and those it calls 
// to match your input/output devices
// TODO:
// 1. Create a Time object
// 2. Get data for each object member
//  2a) .hour24 (uint8_t)
//  2b) .hour12 (uint8_t)
//  2c) .minute (uint8_t)
//  2d) .second (uint8_t)
//  2e) .pm (bool)
// 3. Return the Time object
///////////////////////////////////////////
Time enterNewTime(void){
  // 1. Create a Time object
  Time newTime;
  //Initializing values
  newTime.hour12 = newTime.hour24 = 12;
  newTime.minute = newTime.second = 0;
  newTime.pm = false;
  //Everything's set, could return newTime here. I'll get user input instead.

  //Ask user to set 12/24 hour mode, sets global flag 'twelveHourMode' so no need to pass values
  set12_24(); 
  //Set Hours (TODO: 2a and 2b)
  if(twelveHourMode){
      //Prompt user for an Hour, number between 1 and 12, starting with 12
      //Uses helper function => getUserNumber(title, min, max, default)
      newTime.hour12 = getUserNumber("Set Hour", 1, 12, 12);
      //Set the 24 hour time member to be the same as the 12 hour value just set by user
      newTime.hour24 = newTime.hour12; //Add 12 hours later if user picks PM
  }
  else { 
      //We're in 24 hour mode. Get an hour between 0 and 23, set both hour values
      newTime.hour24 = getUserNumber("Set Hour", 0, 23, 12);
      newTime.hour12 = newTime.hour24;
      if(newTime.hour24 > 12){    //i.e. if hour24=15, then set hour12=3 and pm = true;
        newTime.hour12 -= 12;
        newTime.pm = true;
      }
  }
  // Set minutes - get number from 0-59, starting with 30 (TODO: 2c)
  newTime.minute = getUserNumber("Set Minutes",0,59,30);
  
  // Seconds (TODO: 2d) - I'm forcing to 0, initialied above, no user setting. Done.
  
  //Set AM/PM (TODO: 2e)
  if(twelveHourMode) { //only need to ask in 12 hour mode
      //Prompt user to choose am (pm=false) or pm (pm=true)
      newTime.pm = getUserAmPm();
      //Correct .hour24 if they selected pm=true
      //i.e.: If they set hour12=4 earlier, then just chose pm=true, hour24 needs to change from 4 to 16.
      if(newTime.pm)
        newTime.hour24 += 12;
  }
  //Prep the screen for setting the date - next display func to be called
  lcd.Fill_Screen(WHITE);
  
  // 3. Return the Time object
  return newTime;
}
 ///////////////////////////////////////////
// Date enterNewDate() - called from loop()
// Rewrite this routine and those it calls 
// to match your input/output devices
// 1. Create a Date object
// 2. Get data for each object member
//  2a) .year (uint16_t)
//  2b) .month (uint8_t) 1=Jan...12=Dec
//  2c) .date (uint8_t) 1..31
//  2d) .weekday (uint8_t) 1=Sun...7=Sat
// 3. Return the Date object
///////////////////////////////////////////
Date enterNewDate(void){
  // 1. Create a Date object
  Date newDate;
  newDate.year = 2020; newDate.month = 4;
  newDate.date = 1; newDate.weekday = 4;
  uint8_t tens_ones = 0;
  
  //TODO: 2a) Get year - I'm limiting the user input for year to 2000-2099. The DS3231 could go to 2199.
  tens_ones = getUserNumber("Set Year: 20XX",0,99,20);
  newDate.year = 2000 + tens_ones;
  //TODO: 2b) Get month
  //Prompts user to choose a value from month_l array values (month long names)
  newDate.month = getUserArrayChoice(MONTH_L) + 1;    //add 1 because of 0 offset of index in array

  //TODO: 2c) Get date
  //Prompt user for date number. Max number from days_per_month array based on month they just chose
  newDate.date = getUserNumber("Set Date:",1,days_per_month[newDate.month-1],15);
  
  //TODO: 2d) Weekday
  //We could calculate the weekday - see: https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
  //It's not as fun as it looks, I'll just ask the user for it until I'm feeling industrious enough to 
  //calculate it myself
  newDate.weekday = getUserArrayChoice(WEEKDAY_L) + 1;
  
  //Prep the screen for going back to home
  lcd.Fill_Screen(BLACK);
  drawButtons();
  
  //TODO: 3) Return Date object
  return newDate;
}  

///////////////////////////////////////////
// set12_24() - called from enterNewTime(), 
// sets global flag 'twelveHourMode' which
// is used to determine how time is displayed
// and how alarm settings are entered
// Rewrite this routine to set twelveHourMode
// based on your hardware
// false = 24 hour mode
// true = 12 hour mode
///////////////////////////////////////////
void set12_24(void){
  //Clear screen
  lcd.Fill_Screen(WHITE);
  lcd.Set_Text_colour(RED);
  lcd.Set_Text_Back_colour(WHITE);
  lcd.Set_Text_Size(4);
  lcd.Print_String("Set Time Mode",CENTER,0);
  lcd.Set_Text_colour(WHITE);
  lcd.Set_Text_Back_colour(RED);
  lcd.Set_Text_Size(3);
  lcd.Print_String("12 Hour Mode - ex:  4:00 PM",CENTER,100);
  lcd.Print_String("24 Hour Mode - ex: 16:00",CENTER,210);
  //Await press - largely LCD library code
  do {
      digitalWrite(13, HIGH);
      TSPoint p = ts.getPoint();
      digitalWrite(13, LOW);
      pinMode(XM, OUTPUT);
      pinMode(YP, OUTPUT);
      if (p.z > MINPRESSURE && p.z < MAXPRESSURE)
      {
        p.x = map(p.x, TS_MINX, TS_MAXX, w,0);
        p.y = map(p.y, TS_MINY, TS_MAXY, h,0);
        if(is_pressed(160,13,190,300,p.x,p.y)) { //12 Hour  
          #ifdef DEBUG
          Serial.println("Pressed 12 Hour Mode");
          #endif
          twelveHourMode = true; 
          lcd.Fill_Screen(BLACK);
          return;
        }
        else if (is_pressed(325,25,360,290,p.x,p.y)) { //24 Hour  
          #ifdef DEBUG
          Serial.println("Pressed 24 Hour Mode");
          #endif
          twelveHourMode = false; 
          lcd.Fill_Screen(BLACK);
          return;
        }
        delay(200); //Debounce delay
    }
  }while(1); //loops forever until one of the above is pressed 
             //and a return statement is encountered. Better
             //practice would be to timeout at some point
}
/////////////////////////////////////////////////////////
// uint8_t getUserNumber(title, minNum, maxNum, startNum) 
// Helper called from various places to get an hour, minute, date, etc
// @title is title to display, 
// @minNum is lowest number in the allowed range,
// @maxNum is highest number in the allowed range, 
// returns the number selected
// TODO: Rewrite this match your hardware.
////////////////////////////////////////////////////////
uint8_t getUserNumber(String title, uint8_t minNum, uint8_t maxNum, uint8_t startNum) {
  uint16_t pixelColor = 0;
  uint16_t upColor = 0;
  uint16_t downColor = 0;
  uint16_t nextColor = 0;
  uint16_t dispX, dispY;
  float tYfactor = 0.56471;
  float tXfactor = 0.43836;
  uint8_t userNum;
  bool notDone = true;
  
  lcd.Fill_Screen(WHITE);
  lcd.Set_Text_colour(RED);
  lcd.Set_Text_Back_colour(WHITE);
  lcd.Set_Text_Size(4);
  lcd.Print_String(title,CENTER,0);
  lcd.Set_Text_colour(WHITE);
  lcd.Set_Text_Back_colour(RED);
  lcd.Set_Text_Size(5);

  upColor = lcd.Color_To_565(0,255,0);
  downColor = lcd.Color_To_565(250,0,0);
  nextColor = lcd.Color_To_565(0,0,255);
  //Display the number to change, starting with startNum
  lcd.Print_Number_Int((long)startNum,220,125,2,' ',10);
  userNum = startNum;
  //I'm drawing three triangles, up, down, and next
  //Each will be a different color. When the user presses
  //I will check the color of the pixel they pressed
  //As long as these colors are unique on the screen, I 
  //will know which button they pressed
  //Tried this method since the other touch logic uses
  //rectangular areas only - it works fine
  lcd.Set_Draw_color(upColor);
  lcd.Fill_Triangle(210,105,250,55,290,105);
  lcd.Set_Draw_color(downColor);
  lcd.Fill_Triangle(210,185,250,235,290,185);
  lcd.Set_Draw_color(nextColor);
  lcd.Fill_Triangle(375,120,435,150,375,180);

  do {
      digitalWrite(13, HIGH);
      TSPoint p = ts.getPoint();
      digitalWrite(13, LOW);
      pinMode(XM, OUTPUT);
      pinMode(YP, OUTPUT);
      if (p.z > MINPRESSURE && p.z < MAXPRESSURE){
        /*****************************************************
         * Because my display screen is rotated 270 degrees 
         * and the touchscreen doesn't rotate, I have to translate
         * touch coordinates to display coordinates. 
         * Touchscreen X axis is Display Y axis and vice versa
         * (touch X - lowest touchpoint number) * factor = display Y
         * (touch Y - lowest touchpoint number) * otherfactor = display X
         * factor = 1 / (touchpoint axis range / display axis range)
         ******************************************************/
        dispX = round((p.y-100)* tYfactor);
        dispY = round((p.x-180) * tXfactor);
        pixelColor = lcd.Read_Pixel(dispX,dispY);
        if(pixelColor == upColor){
            userNum++;
            if(userNum > maxNum)
              userNum = minNum;
        }
        else if(pixelColor == downColor){
            userNum--;
            if(userNum < minNum || userNum > maxNum)
              userNum = maxNum;
        }
        else if (pixelColor == nextColor) {
            notDone = false;
        }
        lcd.Print_String("  ",220,125);
        lcd.Print_Number_Int((long)userNum,220,125,2,' ',10);
      }
     delay(200);
  }while(notDone);
  #ifdef DEBUG
  Serial.print("getUserNumber: userNum is: "); Serial.println(userNum);
  #endif
  return userNum;
}
/////////////////////////////////////////////
// bool getUserAmPm()- Helper gets am/pm user
// choice. Returns false for am, true for pm,
// to match the Time.pm class member.
// Called when setting time, alarm
// TODO: Rewrite this to match your hardware
/////////////////////////////////////////////
bool getUserAmPm(void) {
      bool ampmDone = false; 
      bool ampm = false;
      lcd.Fill_Screen(WHITE);
      lcd.Set_Text_colour(RED);
      lcd.Set_Text_Back_colour(WHITE);
      lcd.Set_Text_Size(5);
      lcd.Print_String("Set AM / PM",CENTER,0);
      lcd.Set_Text_Size(5);
      lcd.Set_Text_colour(WHITE);
      lcd.Set_Text_Back_colour(RED);
      lcd.Print_String("AM",100,120);
      lcd.Print_String("PM",325,120);
      
      do{  
          digitalWrite(13, HIGH);
          TSPoint p = ts.getPoint();
          digitalWrite(13, LOW);
          pinMode(XM, OUTPUT);
          pinMode(YP, OUTPUT);
          if (p.z > MINPRESSURE && p.z < MAXPRESSURE)
          {
            p.x = map(p.x, TS_MINX, TS_MAXX, w,0);
            p.y = map(p.y, TS_MINY, TS_MAXY, h,0);
            if(is_pressed(190,215,245,255,p.x,p.y)) { //AM  
              #ifdef DEBUG
              Serial.println("getUserAmPm: Chose AM");
              #endif
              ampm = false;
              ampmDone = true;
            }
            else if (is_pressed(190,60,245,90,p.x,p.y)) { //PM  
              #ifdef DEBUG
              Serial.println("getUserAmPm: Chose PM");
              #endif
              ampm = true;
              ampmDone = true;
            }
        }  
        delay(200);
      }while(!ampmDone);
      return ampm;
}
////////////////////////////////////////////////////////////////////////
// uint8_t getUserArrayChoice(which)
// called to set Month, Weekday, Alarm Type by names from global arrays
// @which is from the #defines near the array definitions
// returns index in that array of user choice
// TODO: Rewrite this to match your hardware
// Note - it's very similar to getUserNumber()
// so write that, then copy & modify that to get  this
///////////////////////////////////////////////////////////////////////
uint8_t getUserArrayChoice(uint8_t which) {
  uint16_t pixelColor = 0;
  uint16_t upColor = 0;
  uint16_t downColor = 0;
  uint16_t nextColor = 0;
  uint16_t dispX, dispY;
  float tYfactor = 0.56471;
  float tXfactor = 0.43836;
  uint8_t userNum = 0;
  bool notDone = true;
  
  lcd.Fill_Screen(WHITE);
  lcd.Set_Text_colour(RED);
  lcd.Set_Text_Back_colour(WHITE);
  lcd.Set_Text_Size(5);
  //Lazily using a generic heading instead of looking up what we're setting
  lcd.Print_String("Select:",CENTER,0);
  lcd.Set_Text_colour(WHITE);
  lcd.Set_Text_Back_colour(RED);
  lcd.Set_Text_Size(5);

  upColor = lcd.Color_To_565(0,255,0);
  downColor = lcd.Color_To_565(250,0,0);
  nextColor = lcd.Color_To_565(0,0,255);
  if(which == MONTH_L) {
    lcd.Print_String(month_l[userNum],80,125);
  }
  else if(which == WEEKDAY_L) {
    lcd.Print_String(weekdays_l[userNum],80,125);
  }
  else if(which == ALARM_FREQUENCY) {
    lcd.Print_String(alarmFreqOptions[userNum],80,125);
  }
  lcd.Set_Draw_color(upColor);
  lcd.Fill_Triangle(210,105,250,55,290,105);
  lcd.Set_Draw_color(downColor);
  lcd.Fill_Triangle(210,185,250,235,290,185);
  lcd.Set_Draw_color(nextColor);
  lcd.Fill_Triangle(375,120,435,150,375,180);

  do {
      digitalWrite(13, HIGH);
      TSPoint p = ts.getPoint();
      digitalWrite(13, LOW);
      pinMode(XM, OUTPUT);
      pinMode(YP, OUTPUT);
      if (p.z > MINPRESSURE && p.z < MAXPRESSURE){
        dispX = round((p.y-100)* tYfactor);
        dispY = round((p.x-180) * tXfactor);
        pixelColor = lcd.Read_Pixel(dispX,dispY);
        if(pixelColor == upColor){
            userNum++;
            if( (which == MONTH_L && userNum > 11) ||
                (which == WEEKDAY_L && userNum > 6) ||
                (which == ALARM_FREQUENCY && userNum > 2))
              userNum = 0;      
        }
        else if(pixelColor == downColor){
            userNum--;
            if(which == MONTH_L && userNum > 11)
              userNum = 11;
            else if(which == WEEKDAY_L && userNum > 6)
              userNum = 6;      
            else if(which == ALARM_FREQUENCY   && userNum > 2)
              userNum = 2;
        }
        else if (pixelColor == nextColor) {
            notDone = false;
        }
        //Overwrite previous value in case new value is shorter or artifacts will remain
        lcd.Print_String("          ",70,125); //longest value displayed is 10 chars, so 10 spaces
        if(which == MONTH_L) {
          lcd.Print_String(month_l[userNum],70,125);
        }
        else if(which == WEEKDAY_L) {
          lcd.Print_String(weekdays_l[userNum],70,125);
        }
        else if(which == ALARM_FREQUENCY) {
          lcd.Print_String(alarmFreqOptions[userNum],70,125);
        }
     }//end if pressure  
     delay(200);
  }while(notDone);
  
  return userNum;
}
/////////////////////////////////////////////////////////
// enterNewAlarm(alarm_number) - called from loop()
// Gets alarm settings from user for alarm_number (1 or 2)
// TODO: Rewrite to match your hardware
// 1) Create an AlarmSetting object
// 2) Set all the values of the object
// 2a).t Time object settings (.hour12, .hour24, .minute, .second, .pm)
// 2b) .date if alarm type is date, otherwise ignore
// 2c) .day if alarm type is weekday, otherwise ignore
// 2d) .alarm_mask to specify alarm number and alarm type (details in comment below)
// 3) return the AlarmSetting object
//    My logic flow:
//    For alarm 1, set seconds to 01 to avoid collisions
//    Get hour
//    Get minute    
//    Choose Everyday / Date / Day
//    Set which alarm and alarm type masks
//    >if Everyday - we're done
//    >if Date - enter date
//    >if Day - enter day
//    Return
////////////////////////////////////////////////////
AlarmSetting enterNewAlarm(uint8_t alarm_number) {
  //Class AlarmString has a Time member t, 8-bit vars for numeric date, weekday index, and flags alarm_mask
  //See DS3231_tisc.h for definition
  AlarmSetting newAlarm;    
  uint8_t frequency;

  //I'm forcing alarm 1 seconds to = 01, not allowing user to set.
  //This will avoid possibility of simultaneous alarms since alarm 2 is always at 00 seconds
  //and in an alarm clock application, the user won't need to set seconds anyway.
  if(alarm_number == 1)
    newAlarm.t.second = 1;
  //get hours - same logic as getting hours in getNewTime()
  if(twelveHourMode) {
    newAlarm.t.hour12 = getUserNumber("Set Alarm Hour:",1,12,6);
    newAlarm.t.hour24 = newAlarm.t.hour12;
  } else { //24 hour mode
    newAlarm.t.hour24 = getUserNumber("Set Alarm Hour:",0,23,6);
    newAlarm.t.hour12 = newAlarm.t.hour24;
    if(newAlarm.t.hour12 > 12)
      newAlarm.t.hour12 -= 12;
  }
  //get minutes
  newAlarm.t.minute = getUserNumber("Set Alarm Minutes:",0,59,30);

  //get AM/PM - if displating in 12 hour mode, set alarm in 12 hour mode
  if(twelveHourMode){
    newAlarm.t.pm = getUserAmPm();
    if(newAlarm.t.pm)
      newAlarm.t.hour24 += 12;
  }
  //get frequency
  frequency = getUserArrayChoice(ALARM_FREQUENCY);    //0=Everyday,1=Date,2=Weekday
  if(frequency == 0){
    /************************************************************************************
    //The DS3231 has several flags for each alarm, and I need to specify which alarm the 
    //AlarmSetting object is holding data for, so alarm_mask is a collection of flags
    //allowing all this data to be communicated.
    *************************************************************************************/
    //Set register flags for every day - using table format from DS3231 spec, page 12, Table 2
    //Datasheet: https://datasheets.maximintegrated.com/en/ds/DS3231.pdf
    //setAlarm function in DS3231_tisc will move these bits around to the proper registers before writing
    //to the DS3231. The MSB of alarm_mask will be 0=Alarm 1, 1=Alarm 2
    //                        ____________________________________________________________
    //alarm_mask for alarm 1: |  0   |  0  |  0  | DY/!DT | A1M4  | A1M3 | A1M2 | A1M1   |
    // bit position:          |b7/MSB|  b6 | b5  |  b4    |  b3   |  b2  |  b1  | b0/LSB |
    //alarm_mask for alarm 2: |  1   |  0  |  0  |   0    |DY/!DT | A2M4 | A2M3 | A2M2   |
    //                        ------------------------------------------------------------
    if(alarm_number == 1)
      newAlarm.alarm_mask = 0x08;   //MSB = 0 => Alarm 1, A1M4-A1M1 = b1000 => hours/minutes/seconds
    else if (alarm_number == 2)
      newAlarm.alarm_mask = 0x84;   //MSB = 1 => Alarm 2, A2M4-A2M2 = b100 => hours/minutes/seconds
  } 
  else if(frequency == 1) { //by date
    if(alarm_number == 1)
      newAlarm.alarm_mask = 0x00; //MSB = 0 => Alarm 1, DY/!DT = 0, A1M4-A1M1 = b0000 => by date
    else if (alarm_number == 2)
      newAlarm.alarm_mask = 0x80; //MSB = 1 => Alarm 2, DY/!DT = 0, A2M4-A2M2 = b000 => by date
    
    //get alarm date, allowing 1-31 because month is unknown, 
    //starting with tomorrow as it's the most likely answer
    //NOTE: This is the ONLY direct DS3231 call I'm making from a 
    //      user I/O routine. All other usages are in loop()
    uint8_t tomorrow = readBcdRegister(DS3231_DATE) + 1;
    if(tomorrow > 31) //I don't know what month it is, but 32 is no good for all months
      tomorrow = 1;
    newAlarm.date = getUserNumber("Set Alarm Date:",0,31,tomorrow);

    //TODO: Optional: Enhance by allowing user to select date range, or 'all dates until selected end date'
    //      DS3231 can only hold 1 date, so this program would have to retain user selection 
    //      and reprogram DS3231 every day to match.
  }
  else if(frequency == 2) { //by weekday
    if(alarm_number == 1)
      newAlarm.alarm_mask = 0x10; //MSB = 0 => Alarm 1, DY/!DT = 1, A1M4-A1M1 = b0000 => By weekday
    else if (alarm_number == 2)
      newAlarm.alarm_mask = 0x88; //MSB = 1 => Alarm 2, DY/!DT = 1, A2M4-A2M2 = b000 => By weekday
    newAlarm.weekday = getUserArrayChoice(WEEKDAY_L) + 1;

    //TODO: Optional: Enhance by allowing user to select multiple weekdays like Mon-Fri. DS3231 can only
    //      hold one at a time, so this program would have to keep the user's selections and 
    //      reprogram the DS3231 every day to match.
  }
  //Prep the screen for going back to home
  lcd.Fill_Screen(BLACK);
  drawButtons();
  #ifdef DEBUG
  Serial.print("enterNewAlarm: New settings for alarm "); Serial.print(alarm_number);Serial.println(":");
  Serial.print("     alarm_mask: "); Serial.println(newAlarm.alarm_mask,BIN);
  Serial.print("     weekday: "); Serial.println(newAlarm.weekday);
  Serial.print("     date: "); Serial.println(newAlarm.date);
  Serial.print("     t.hour24: "); Serial.println(newAlarm.t.hour24);
  Serial.print("     t.hour12: "); Serial.println(newAlarm.t.hour12);
  Serial.print("     t.minute: "); Serial.println(newAlarm.t.minute);
  Serial.print("     t.second: "); Serial.println(newAlarm.t.second);
  Serial.print("     t.pm: "); Serial.println(newAlarm.t.pm);
  #endif
  return newAlarm;
}

/////////////////////////////////////////////////////////////////////////////////////
// alarmHandler() - Interrupt Service Routine to handle alarm interrupts from DS3231
//  registered in setup(), called on interrupt
//  Sets global bool flag 'alarmTripped' which must be monitored and reset from 
//  the non-interrupt space - i.e. in loop()
////////////////////////////////////////////////////////////////////////////////////
void alarmHandler(){
  alarmTripped = true;
}

/////////////////////////////////////////////////////////////////////////
// displayAlarm(which) called from loop()
// Implements action taken when an alarm is reached
// @which - which alarm was reached
// TODO: Rewrite for your hardware - buzzer, lights, etc
// Note: My program flow is to stay in this routine until
//       the alarm is cancelled by user. If you want to return to 
//       loop() instead, set your alarm indicators here and return, 
//       then create a different function to cancel the alarm 
//       based on whatever user input you're using (i.e. button press)
/////////////////////////////////////////////////////////////////////////
void displayAlarm(uint8_t which){
  bool cancelled = false;
  bool inverted = false;
  lcd.Fill_Screen(ORANGE);
  lcd.Set_Text_colour(BLUE);
  lcd.Set_Text_Size(5);
  if(which == 1)
    lcd.Print_String("ALARM 1",CENTER,1);
  else if(which == 2)
    lcd.Print_String("ALARM 2",CENTER,1);
  lcd.Set_Draw_color(WHITE);
  lcd.Fill_Round_Rectangle(100,100,380,200,10);
  lcd.Set_Text_Back_colour(WHITE);
  lcd.Set_Text_Size(4);
  lcd.Print_String("Cancel",170,110);
  lcd.Print_String("Alarm",180,150);

  do {
    lcd.Invert_Display(inverted);
    inverted = !inverted;
    digitalWrite(13, HIGH);
    TSPoint p = ts.getPoint();
    digitalWrite(13, LOW);
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
    if (p.z > MINPRESSURE && p.z < MAXPRESSURE)
    {
      p.x = map(p.x, TS_MINX, TS_MAXX, w,0);
      p.y = map(p.y, TS_MINY, TS_MAXY, h,0);
      if(is_pressed(100,100,380,200,p.x,p.y)) { //Cancel Alarm button
        cancelled = true;
      }
    }//end if(pressure)
    delay(200);
  }while(!cancelled);
  lcd.Invert_Display(0);
  lcd.Fill_Screen(BLACK);
  drawButtons();
}

/////////////////////////////////////////////////////////////////////////////
// showAlarmStatus(data) - called from loop() and drawButtons()
// TODO: Rewrite this for your hardware - indicate which alarm(s) are enabled
// @data = 0= Both alarms are disabled; 1=Alarm 1 enabled, Alarm 2 disabled;
//         2=Alarm 1 disbaled, Alarm 2 enabled;  3= Both alarms enabled
/////////////////////////////////////////////////////////////////////////////
void showAlarmStatus(uint8_t data){
  #ifdef DEBUG
  Serial.print("showAlarmStatus: called. Was passed: "); Serial.println(data,BIN);
  #endif
  if(data & 0x01) { //alarm 1 is on
      lcd.Set_Text_Back_colour(RED);
      lcd.Set_Text_colour(WHITE);
      lcd.Set_Text_Size(2);
      lcd.Print_String(" Alarm 1 On ",20,300);
  }
  else{ //Alarm 1 is off
      lcd.Set_Text_Back_colour(BLACK);
      lcd.Set_Text_colour(DARKGRAY);
      lcd.Set_Text_Size(2);
      lcd.Print_String(" Alarm 1 Off",20,300);
  }
  if(data & 0x02) { //alarm 2 is on
      lcd.Set_Text_Back_colour(BLUE);
      lcd.Set_Text_colour(WHITE);
      lcd.Set_Text_Size(2);
      lcd.Print_String(" Alarm 2 On ",200,300);
  }
  else{ //Alarm 2 is off
    lcd.Set_Text_Back_colour(BLACK);
      lcd.Set_Text_colour(DARKGRAY);
      lcd.Set_Text_Size(2);
      lcd.Print_String(" Alarm 2 Off",200,300);
  }
}
////////////////////////////////////////////////////////////
// initializeDisplay() - called from setup()
// Specific to 9486 Controller based LCD TFT 
// TODO: Rewrite for your display hardware - any initial
//       power-on display housekeeping goes here
////////////////////////////////////////////////////////////
void initializeDisplay() {
  lcd.Init_LCD();
  lcd.Fill_Screen(BLACK);
  lcd.Set_Rotation(3); //0=0deg, 1=90deg, 2=180deg, 3=270deg
  w = lcd.Get_Display_Width();    //setting global h & w variables so they're accesible elsewhere
  h = lcd.Get_Display_Height();
  #ifdef DEBUG
  Serial.print("initializeDisplay: LCD Model: "); Serial.println(lcd.Read_ID(),HEX);
  Serial.print("initializeDisplay: Display width: "); Serial.print(w); Serial.print("\tHeight: "); Serial.println(h);
  #endif
  drawButtons();
}
///////////////////////////////////////////////////////////////////////////////
// drawButtons() - draws menu buttons on LCD - called from initializeDisplay
//                 and when returning to home display. Also updates alarm
//                 on/off indicators which are on home screen.
// TODO: Rewrite for your hardware - this makes sure the user can access menu
//       functions.
///////////////////////////////////////////////////////////////////////////////
void drawButtons(){

  lcd.Set_Text_colour(BLACK);
  lcd.Set_Text_Size(2); //1=smallest, 5=largest
  lcd.Set_Draw_color(GREEN);
  lcd.Fill_Round_Rectangle(400,0,480,80,10);
  lcd.Set_Text_Back_colour(GREEN);
  lcd.Print_String("Set",422,15);
  lcd.Print_String("Time",415,45);
  
  lcd.Set_Text_colour(WHITE);
  lcd.Set_Draw_color(RED);
  lcd.Fill_Round_Rectangle(400,80,480,160,10);
  lcd.Set_Text_Back_colour(RED);
  lcd.Print_String("Set",422,85);
  lcd.Print_String("Alarm",410,110);
  lcd.Print_String("1",435,135);

  lcd.Set_Draw_color(BLUE);
  lcd.Fill_Round_Rectangle(400,160,480,240,10);
  lcd.Set_Text_colour(WHITE);
  lcd.Set_Text_Back_colour(BLUE);
  lcd.Print_String("Set",422,165);
  lcd.Print_String("Alarm",410,190);
  lcd.Print_String("2",435,215);
  
  lcd.Set_Draw_color(YELLOW);
  lcd.Fill_Round_Rectangle(400,240,480,320,10);
  lcd.Set_Text_Back_colour(YELLOW);
  lcd.Set_Text_colour(BLACK);
  lcd.Print_String("Alarm",413,255);
  lcd.Print_String("On/Off",407,285);

  showAlarmStatus(getAlarmStatus());
}
///////////////////////////////////////////////////////////////////////////////////////
// check_button_press() - checks for pressure on button areas, returns button
//                        pressed or 0 (BTN_NO_BUTTON) if no button pressed - 
//                        called in main loop
// TODO: Rewrite for your hardware - look for user input, return 0 for none or
//       a value representing the specific input        
///////////////////////////////////////////////////////////////////////////////////////
uint8_t check_button_press(){
  uint8_t button_pressed = BTN_NO_BUTTON;
  //Adapted "Touchscreen" library code to check for presses on buttons drawn in drawButtons()
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE)
  {
    p.x = map(p.x, TS_MINX, TS_MAXX, w,0);
    p.y = map(p.y, TS_MINY, TS_MAXY, h,0);
    if(is_pressed(0,0,120,50,p.x,p.y)) { //Set Time button
      #ifdef DEBUG
      Serial.println("check_button_press: Pressed Set Time");
      #endif
      button_pressed = BTN_SET_TIME;
    }
    else if(is_pressed(121,00,240,50,p.x,p.y)) { //Set Alarm 1 button
      #ifdef DEBUG
      Serial.println("check_button_press: Pressed Set Alarm 1");
      #endif
      button_pressed = BTN_SET_ALARM_1;
    }
    else if(is_pressed(241,0,360,50,p.x,p.y)){
      #ifdef DEBUG
      Serial.println("check_button_press: Pressed Set Alarm 2");
      #endif
      button_pressed = BTN_SET_ALARM_2;
    }
    else if(is_pressed(361,0,480,50,p.x,p.y)){
      #ifdef DEBUG
      Serial.println("check_button_press: Pressed Alarm On/Off");
      #endif
      button_pressed = BTN_ALARM_TOGGLE;
    }
    #ifdef DEBUG
    else {  //Pressed outside a button
      Serial.print("check_button_press: Pressed at X: "); Serial.print(p.x); Serial.print("\tY: "); Serial.print(p.y); Serial.print("\tZ: "); Serial.println(p.z);
    }
    #endif
    delay(200); //Debounce delay
  }
  return button_pressed;
}
////////////////////////////////////////////////////////////////////////////////////
// bool is_pressed(x1, y1, x2, y2, pressed_x, pressed_y) 
// Helper function to determine if a given touch point is within an area bounded 
// by a rectangle withcorners at points x1,y1 and x2,y2.  This is example code from 
// the 'Touchscreen' library. It's a bit of a pain in that x1 must be less than x2,
// and y1 must be less than y2, or it won't return true. You can't pass arbitray
// corners. Could be fixed, but not worth the time...just be careful when calling it
////////////////////////////////////////////////////////////////////////////////////
boolean is_pressed(int16_t x1,int16_t y1,int16_t x2,int16_t y2,int16_t px,int16_t py)
{
    if((px > x1 && px < x2) && (py > y1 && py < y2))
    {
        return true;  
    } 
    else
    {
        return false;  
    }
 }
