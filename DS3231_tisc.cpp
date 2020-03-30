/*****************************************************************************
***  TechIsSoCool.com DS3231 Real-Time Clock Module Interface for Arduino  ***
***  DS3231_tisc.cpp                                                       ***
***  Visit: https://TechIsSoCool.com for details						   ***
******************************************************************************/
#include <stdint.h>   //include standard typdef definitions
#include <Wire.h>     //include Arduino serial library for I2C
#include "DS3231_tisc.h"  //include header for this file

/****************************************************************
* See the header file "DS3231_tisc.h" for class definitions of
* 'Time', 'Date' and 'AlarmSetting' objects used in conjunction
* with many of these functions. 
****************************************************************/


/*****************************************************************
* setTime(Time t) 
* @t - Time object with time settings to make
*      handles decimal to BCD conversion where needed
******************************************************************/
void setTime(Time t) {
	if(twelveHourMode) {
		t.hour12 = _toBcd(t.hour12 & 0x1f);
		t.hour12 |= 0x40;	//sets 12 hour bit high	
		if(t.pm) {
			t.hour12 |= 0x20;	//sets PM high
		}
		writeRegister(DS3231_HOURS,t.hour12);
	}
	else {	//24-hour mode
		t.hour24 = _toBcd(t.hour24);
		t.hour24 &= 0x3f;	//first two bits low, all else pass through
		writeRegister(DS3231_HOURS,t.hour24);
	}
	writeRegister(DS3231_MINUTES,_toBcd(t.minute));
	writeRegister(DS3231_SECONDS,_toBcd(t.second));
}
/************************************************
* Time readTime(void) 
* @return - time read from DS3231, converted from BCD to decimal
*************************************************/
Time readTime(void){
	uint8_t h, m, s;
	Time timeRet;
	//Read Hours
	Wire.beginTransmission(DS3231_ADDR);
	//Wire.write();
	Wire.write(DS3231_HOURS);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDR,1);
    h = Wire.read();   
	//Serial.print("Hours decoded from bcd: "); 
    //Serial.print(_fromBcd(h));      
 
  //Read Minutes
	Wire.beginTransmission(DS3231_ADDR);
	//Wire.write(DS3231_WRITE);
	Wire.write(DS3231_MINUTES);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDR,1,false);
	//Serial.println("Requested minute");
    m = Wire.read();   
	//Serial.print("\tMinutes: "); 
    //Serial.print(_fromBcd(m));      
 
	//Read Seconds
	Wire.beginTransmission(DS3231_ADDR);
	//Wire.write(DS3231_WRITE);
	Wire.write(DS3231_SECONDS);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDR,1,true);
	//Serial.println("Requested second");
	
    s = Wire.read();   
	//Serial.print("\tSeconds: "); 
    //Serial.println(_fromBcd(s));    
	//Hour 0	12/!24	!am/pm/20hour	10hour	hour	hour	hour 	hour
	if(h & 0x40) {
		//12 hour mode
		timeRet.pm = ((h & 0x20)>>5); //mask !am/pm flag and shift to LSB
		timeRet.hour12 = _fromBcd(h & 0x1f);
		timeRet.hour24 = timeRet.hour12;
		if(timeRet.pm) {
			timeRet.hour24 += 12;
		} 
	}
	else {	
		//24 hour mode
		timeRet.hour24 = _fromBcd(h);
		timeRet.pm = 0;
		timeRet.hour12 = timeRet.hour24;
		if(timeRet.hour24 > 12) {
			timeRet.hour12 -= 12;
			timeRet.pm = 1;
		}
	}
	timeRet.minute = _fromBcd(m);
	timeRet.second = _fromBcd(s);
	return timeRet;
}

/************************************************
* setDate(Date d) 
* sets date registers in DS3231
* @d - date object with decimal values for year
*      and date. 1-7 for weekday, and 1-12 for month
*************************************************/
void setDate(Date d){
	uint8_t regData = 0;
	//Write tens and ones of year into register 0x06
	writeRegister(DS3231_DEC_YEAR,_toBcd(d.year-2000));
	//start building century + 00 + 10month + month
	regData = _toBcd(d.month);	//convert to bcd 
	if(d.year >= 2100)
		regData |= 0x80;	//set the century bit if needed
	
	writeRegister(DS3231_CEN_MONTH,regData);	//write century flag + 00 + 10month + 1month
	writeRegister(DS3231_DATE,_toBcd(d.date));	//write date
	writeRegister(DS3231_DAY,_toBcd(d.weekday));	//write weekday
}
/*****************************************************************
* Date readDate(void) 
* @return - date read from DS3231, converted from BCD to decimal
******************************************************************/
Date readDate(void){
	
	Date d;
	
	//Read Year
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(DS3231_DEC_YEAR);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDR,1);
    d.year = _fromBcd(Wire.read());   
	//Read Month & Century
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(DS3231_CEN_MONTH);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDR,1);
    d.month = Wire.read();   
	if (d.month & 0x80) 	//Century flag is set, it's 2100!
		d.year += 2100;
	else	 //Century flag is clear, it's still the 2000's
		d.year += 2000;
	d.month = _fromBcd(d.month & 0x1f);	//mask off century bit, two fixed 0's, convert from bcd
	//Read Date
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(DS3231_DATE);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDR,1);
    d.date = _fromBcd(Wire.read());   
	//Read Weekday
	Wire.beginTransmission(DS3231_ADDR);
	Wire.write(DS3231_DAY);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_ADDR,1);
    d.weekday = _fromBcd(Wire.read());   
	
	return d;
}
/************************************************
* setAlarm(AlarmSetting a) 
* sets Alarm registers and mask bits in DS3231
* @a - AlarmSetting object with necessary time/day/date
*      info, flag to indicate which alarm it is for, 
*      and alarm register mask bits
	//                        ____________________________________________________________
	//alarm_mask for alarm 1: |  0   |  0  |  0  | DY/!DT | A1M4  | A1M3 | A1M2 | A1M1   |
    // bit position:          |b7/MSB|  b6 | b5  |  b4    |  b3   |  b2  |  b1  | b0/LSB |
    //alarm_mask for alarm 2: |  1   |  0  |  0  |   0    |DY/!DT | A2M4 | A2M3 | A2M2   |
    //                        ------------------------------------------------------------
*************************************************/
void setAlarm(AlarmSetting a){
	uint8_t data;
	if(!(a.alarm_mask & 0x80)) { //true is MSB = 0, therefore alarm 1
		//Start building register 0x07, A1M1 + seconds
		data = _toBcd(a.t.second);
		//Set bit A1M1 in data to match A1M1 in alarm_mask
		if(a.alarm_mask & 0x01)
			data |= 0x80;
		writeRegister(DS3231_ALARM1_SECONDS,data);
		//same for minutes and A1M2 for register 0x08
		data = _toBcd(a.t.minute);
		if(a.alarm_mask & 0x02)
			data |= 0x80;
		writeRegister(DS3231_ALARM1_MINUTES,data);
		//For the hours register, we need to know if time is stored in 12 or 24 hour mode.
		//The alarms sounds when the registers match, so the alarms need to be set
		//in the same mode as the time. The clock may be displaying either, regardless
		//of which is stored, so passing that setting here would not have helped
		data = readRegister(DS3231_HOURS);
		if(data & 0x40) {  //12 hour mode
			data = _toBcd(a.t.hour12);
			data |= 0x40;	//set 12/!24 bit
			if(a.t.pm)
				data |= 0x20;	//if pm = 1, set pm flag = 1
		}
		else { //24 hour mode
			data = _toBcd(a.t.hour24);
		}
		if(a.alarm_mask & 0x04)
				data |= 0x80;	//if A1M3 is set in incoming data, set A1M3 in outgoing data
		writeRegister(DS3231_ALARM1_HOURS,data);
		//If A1M4 == 1, then it's an everyday alarm and none of the rest of register 0x0A matters
		if(a.alarm_mask & 0x08) {
			writeRegister(DS3231_ALARM1_DAY_DATE, 0x80); //Setting A1M4, clearing the rest, they will be ignored
		}
		else { 	//Either the day or the date will be set in incoming data, DY/!DT determines how it is interpreted
			data = 0x00;
			//leave MSB A1M4 = 0 since it's 0 in alarm_mask
			//next get the BCD number loaded, then OR in the remaining flag
			if(a.alarm_mask & 0x10) { //DY/!DT == 1, so it's day
				data = _toBcd(a.weekday);
				data |= 0x40;	
			}
			else { //date
				data = _toBcd(a.date);
				data &= 0xbf;	//sets DY/!DT to 0
			}
			writeRegister(DS3231_ALARM1_DAY_DATE, data);
		}
		//All Alarm 1 registers are set
	} //end if (alarm 1)
	else { //MSB = 1, so alarm 2
		//Minutes register
		data = _toBcd(a.t.minute);
		//Need to OR in A2M2
		if(a.alarm_mask & 0x01)
			data |= 0x80;
		writeRegister(DS3231_ALARM2_MINUTES, data);
		//Hour register - read 12/24 mode
		data = readRegister(DS3231_HOURS);
		if(data & 0x40) {  //12 hour mode
			data = _toBcd(a.t.hour12);
			data |= 0x40;	//set 12/!24 bit
			if(a.t.pm)
				data |= 0x20;	//if pm = 1, set pm flag = 1
		}
		else { //24 hour mode
			data = _toBcd(a.t.hour24);
		}
		if(a.alarm_mask & 0x02)
				data |= 0x80;	//if A2M3 is set in incoming data, set A2M3 in outgoing data
		writeRegister(DS3231_ALARM2_HOURS,data);
		// Day/Date Register: if A2M4 == 1, then everyday & rest of register is 'don't care'.
		if(a.alarm_mask & 0x04) {
			writeRegister(DS3231_ALARM2_DAY_DATE, 0x80);
		}
		else { //day or date, DY/!DT tells which
			data = 0x00;
			if(a.alarm_mask & 0x08) { //day
				data = _toBcd(a.weekday);
				data |= 0x40;	//set day flag
			}
			else { //date
				data = _toBcd(a.date);
				data &= 0xbf;	//sets DY/!DT to 0
			}
			writeRegister(DS3231_ALARM2_DAY_DATE, data);
		}
		//All Alarm 2 registers are set
	} //end else alarm 2
}
/****************************************************************************
* turnAlarmOn(alarms)
* @alarms - alarms to turn on. 1=Alarm 1, 2=Alarm 2, 3=Both Alarms
* Sets the interrupt enable flag for the requested alarms (A1IE, A2IE)
* Clears the interrupt flag for requested alarms to avoid immediate interrupt
*****************************************************************************/
void turnAlarmOn(uint8_t alarms){
    uint8_t data;
    alarms &= 0x03;
    //Clear interrupt flag for any alarms we are turning on
    data = readRegister(DS3231_STATUS);
    //if alarms 1 is set and the DS3231 interrupt flag for alarm 1 is already set, clear it
    if(alarms & 0x01 && data & 0x01)                                  
      data &= 0xfe;
    //same for alarm 2
    if(alarms & 0x02 && data & 0x02)
      data &= 0xfd;
    writeRegister(DS3231_STATUS,data);
    //Get the alarm interrupt register
    data = readRegister(DS3231_CONTROL);
    //Set alarm 1 and/or 2 interrupt enable flags by ORing them in
    data |= alarms;
    writeRegister(DS3231_CONTROL,data);
}
/*********************************************************
* getAlarmStatus() - returns the state of A1IE and A2IE
* bits in the DS3231 Control register
* 0 = both disabled, 1= Alarm 1 interrupts enabled
* 2= Alarm 2 interrupts enabled, 3= Both alarms enabled
*********************************************************/
uint8_t getAlarmStatus(){
	return readRegister(DS3231_CONTROL) & 0x03;
}
/*****************************************************************
* uint8_t serviceAlarms() 
* Determine which alarm(s) are tripped (flag set in Status register)
* Clear the alarm flags
* returns which alarms were tripped (0=none,1,2,3=both)
*******************************************************************/
uint8_t serviceAlarms() {
  uint8_t data;
  //Read the flags
  data = readRegister(DS3231_STATUS);
  //Clear both alarm flags to clear interrupt
  writeRegister(DS3231_STATUS,data & 0xfc);
  return data & 0x03;
  
}
/*******************************************************************
* toggleAlarms()
* Using the A1IE and A2IE (interrupt enable) flags, this routine
* rotates between these states each time it is called:
* >both off
* >Alarm 1 on, Alarm 2 off
* >Alarm 1 off, Alarm 2 on
* >both on
* Returns the resulting state. 
* Related: You can check the state without changing it by calling getAlarmStatus(). 
* Related: You can set/clear a specific alarm with turnAlarmOn()
*********************************************************************/
uint8_t toggleAlarms() {
  uint8_t data;
  uint8_t current;
  data = readRegister(DS3231_CONTROL);
  current = data & 0x03;
  if(++current > 3)
    current = 0;
  data &= 0xfc;
  data |= current;
  writeRegister(DS3231_CONTROL,data);
  return current;
}
/*****************************************************************************************
* initializeDS3231() 
* Configures some initial settings in the DS3231. 
* - Sets the clock to 12 hour mode
* - Sets the time to 12:00:00 PM
* - Clears both Alarm Interrupt flags, so there are no pending alarms
* - Sets the SQW/!INT pin to be an interrupt pin 
*	>>YOU MUST ADD A PULLUP RESISTOR OR CONFIGURE YOUR ARDUINO PIN AS INPUT_PULLUP
* - Sets the INTCN, A1IE, and A2IE bits, allowing alarms to generate interrupts
*   >IN YOUR CODE ADD: "#define ALARM-INTERRUPT_PIN N" WHERE N IS THE PIN NUMBER 
*   >THE SQW/!INT PIN IS CONNECTED TO. THEN ADD THIS TO YOUR setup() FUNCTION:
*	>"attachInterrupt(digitalPinToInterrupt(ALARM_INTERRUPT_PIN), alarmHandler, FALLING);"
*	>ALL ABOVE WITHOUT QUOTES OF COURSE. 'alarmHandler' IS THE NAME OF YOUR ISR 
*******************************************************************************************/
void initializeDS3231(void) {
	uint8_t data;	
	//Set the time
	writeBcdRegister(DS3231_HOURS,12);
	writeBcdRegister(DS3231_MINUTES,0);
	writeBcdRegister(DS3231_SECONDS,0);
	
	// Configure the DS3231 to use interrupts instead of square wave out, and disable 
    // the Alarm 1 and Alarm 2 interrupts until the alarms are turned on
    // These are the default DS3231 settings, but it could easily be in an unknown state
    // since it has battery backup
    //read existing contents of control register
  
    data = readRegister(DS3231_CONTROL);
  
    //set the INTCN, A2IE, and A1IE bits, leave all else alone
    data &= 0xfc; //clear A1IE and A2IE
    data |= 0x04; //Set INTCN, use SQW/!INT pin as !INT
    //write the data back to the register
    writeRegister(DS3231_CONTROL, data);
}

/************************************************************
* readBcdRegister(reg) 
* @reg - number from 0x00 to 0x12 indicating 
*     which register to read
* @return - data value from requested register, 
*     converts from register BCD to decimal before returning
*************************************************************/
uint8_t readBcdRegister(uint8_t reg){
  Wire.beginTransmission(DS3231_ADDR);  //Sends start bit, slave address, and write bit, waits for ack from device
  Wire.write(reg);                  //Sends 8 bits the function was passed, a register address, waits for ack
  Wire.endTransmission();           //Sends the stop bit to indicate end of write
  Wire.requestFrom(DS3231_ADDR,1);    //Sends start, slave address, read bit, waits for ack and one byte, then sends stop
  return _fromBcd(Wire.read());       //reads the received byte from the buffer and returns it to whoever called this function
}

/************************************************
* writeBcdRegister(reg, data) 
* @reg - number from 0x00 to 0x12 indicating 
*    which register to write data to
* @data - data value in decimal to write to that register, 
*         will be converted to BCD before writing
* @return - void
*************************************************/
void writeBcdRegister(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(DS3231_ADDR);  //Sends start bit, slave address, and write bit, waits for ack from device
  Wire.write(reg);                  //Writes the first passed parameter value to the device, hopefully a register address
  Wire.write(_toBcd(data));                 //Writes the second passed parameter, the data for that register
  Wire.endTransmission();             //Completes the transaction by sending stop bit
}
/************************************************
* readRegister(reg) 
* @reg - number from 0x00 to 0x12 indicating 
*		 which register to read
* @return - data value from requested register
*************************************************/
uint8_t readRegister(uint8_t reg){
  Wire.beginTransmission(DS3231_ADDR);  //Sends start bit, slave address, and write bit, waits for ack from device
  Wire.write(reg);			            //Sends 8 bits the function was passed, a register address, waits for ack
  Wire.endTransmission();   		    //Sends the stop bit to indicate end of write
  Wire.requestFrom(DS3231_ADDR,1);  	//Sends start, slave address, read bit, waits for ack and one byte, then sends stop
  return Wire.read();      	//reads the received byte from the buffer and returns it to whoever called this function
}

/************************************************
* writeRegister(reg, data) 
* @reg - number from 0x00 to 0x12 indicating 
*		 which register to write data to
* @data - data value to write to that register
* @return - void
*************************************************/
void writeRegister(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(DS3231_ADDR);  //Sends start bit, slave address, and write bit, waits for ack from device
  Wire.write(reg);            			//Writes the first passed parameter value to the device, hopefully a register address
  Wire.write(data);           			//Writes the second passed parameter, the data for that register
  Wire.endTransmission();         		//Completes the transaction by sending stop bit
}

/* _toBcd() */
uint8_t _toBcd(uint8_t num)
{
  uint8_t bcd = ((num / 10) << 4) + (num % 10);
  return bcd;
}
/* _fromBcd() */
uint8_t _fromBcd(uint8_t bcd) {
  uint8_t num = (10*((bcd&0xf0) >>4)) + (bcd & 0x0f);
  return num;
}
