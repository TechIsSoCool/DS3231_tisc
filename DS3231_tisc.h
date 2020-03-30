/*****************************************************************************
***  TechIsSoCool.com DS3231 Real-Time Clock Module Interface for Arduino  ***
***  DS3231_tisc.h                                                         ***
***	 Visit: https://TechIsSoCool.com for details						   ***
******************************************************************************/
#ifndef _DS3231_TISC_H
#define _DS3231_TISC_H

//DS3231 I2C Addresses - hardwired in IC
#define DS3231_READ  0xD1
#define DS3231_WRITE 0xD0
#define DS3231_ADDR  0x68

//DS3231 Registers
#define DS3231_SECONDS      	0x00
#define DS3231_MINUTES      	0x01
#define DS3231_HOURS      		0x02
#define DS3231_DAY        		0x03
#define DS3231_DATE       		0x04
#define DS3231_CEN_MONTH    	0x05
#define DS3231_DEC_YEAR     	0x06
#define DS3231_ALARM1_SECONDS 	0x07
#define DS3231_ALARM1_MINUTES 	0x08
#define DS3231_ALARM1_HOURS   	0x09
#define DS3231_ALARM1_DAY_DATE  0x0a
#define DS3231_ALARM2_MINUTES 	0x0b
#define DS3231_ALARM2_HOURS   	0x0c
#define DS3231_ALARM2_DAY_DATE  0x0d
#define DS3231_CONTROL      	0x0e
#define DS3231_STATUS   		0x0f
#define DS3231_AGING_OFFSET 	0x10
#define DS3231_TEMP_MSB     	0x11
#define DS3231_TEMP_LSB     	0x12

//This flag tells the library whether to store the time in 12-hour or 24-hour format
//It is used only in setTime(). Using 'extern' means that there MUST be a global bool in your
//code with the same name. If you do not want to use this, then allow it to be defined 
//as true or false in this file. 
//TODO: Uncomment ONE of these three, depending on how you want to maintain 12/24 hour mode
extern bool twelveHourMode;		//Main program will implement and maintain this var
//bool twelveHourMode = false;  //Time will be stored in 24-hour mode (e.g.: 16:00)
//bool twelveHourMode = true;   //Time will be stored in 12-hour mode (e.g.: 4:00 PM)

//Classes
class Time {
  public:
  uint8_t hour24;
  uint8_t hour12;
  uint8_t minute;
  uint8_t second;
  bool pm;  //0 = am, 1 = pm
};

class Date {
  public:
  uint16_t year;    //4 digit year value
  uint8_t month;  	//1= Jan...12=Dec
  uint8_t date;     //1-31
  uint8_t weekday;  //1=Sun...7=Sat
};

class AlarmSetting {
	public:
	Time t;				//Will hold hour12, hour24, minute, second, and pm for alarm setting
	uint8_t date;     	//1-31, only neeed to set if alarm type is date match
	uint8_t weekday;  	//1=Sun...7=Sat, only need to set if alarm type is weekday match
	uint8_t alarm_mask;	//Holds alarm flags and which alarm settings are for
	//                        ____________________________________________________________
    //alarm_mask for alarm 1: |  0   |  0  |  0  | DY/!DT | A1M4  | A1M3 | A1M2 | A1M1   |
    // bit position:          |b7/MSB|  b6 | b5  |  b4    |  b3   |  b2  |  b1  | b0/LSB |
    //alarm_mask for alarm 2: |  1   |  0  |  0  |   0    |DY/!DT | A2M4 | A2M3 | A2M2   |
    //                        ------------------------------------------------------------
	//See DS3231 Datasheet page 12, Table 2: Alarm Mask Bits for flag definitions [or trust me ;)]
	//Datasheet: https://datasheets.maximintegrated.com/en/ds/DS3231.pdf
};


//Function prototypes
Time readTime(void);					//reads DS3231 time registers, returns values in Time object
void setTime(Time t);					//takes values from Time object, write them to DS3231 time registers
Date readDate(void);					//reads DS3231 date registers, returns values in Date object
void setDate(Date d);					//takes values in Date object, write them to DS3231 date registers
void setAlarm(AlarmSetting a);			//takes values in AlarmSetting object and configure the cooresponding alarm in DS3231
void turnAlarmOn(uint8_t alarms);		//Enables interrupts for the passed alarms
uint8_t getAlarmStatus(void);			//returns on/off status of each alarm (A1IE and A2IE bits)
uint8_t serviceAlarms(void);			//Clears Alarm flags, returns which alarm flags were set
uint8_t toggleAlarms(void);				//Using A1IE, A2IE, cycles from both off, 1 on, 2 on, both on,...
void initializeDS3231(void);			//Sets time and configures alarm interrupts
uint8_t readRegister(uint8_t reg);		//Reads from register, returns register value
void writeRegister(uint8_t reg, uint8_t data); //Writes data to register
uint8_t readBcdRegister(uint8_t reg);    //Reads from register, returns register's BCD value converted to decimal
void writeBcdRegister(uint8_t reg, uint8_t data); //Writes data to register, pass decimal value, it converts to BCD then writes
uint8_t _toBcd(uint8_t num);    //decimal -> BCD conversion
uint8_t _fromBcd(uint8_t bcd);  //BCD -> decimal conversion
#endif