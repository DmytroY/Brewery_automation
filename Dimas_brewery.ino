#include <UTFT.h>                         //connect Libriary for use TFT
#include <URTouch.h>                      //connect Libriary for use Touch
#include <UTFT_Buttons.h>                 //Warning! UTFT_buttons.h string should be changed changed to #define MAX_BUTTONS 26
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS3231.h>                       //Real Time Clock
#include <EEPROM.h>
#include <RWI2C.h>                        // read/write registers of I2C device https://github.com/DmytroY/RWI2C

// main class constructors
UTFT    myGLCD(CTE32_R2, 38, 39, 40, 41);   //Type of TFT
URTouch  myTouch( 6, 5, 4, 3, 2);           //Type of Touch
UTFT_Buttons  myButtons(&myGLCD, &myTouch); //set up UTFT_Buttons
DS3231  rtc(SDA, SCL);                     // RTC module DS3231 connected to pins SDA and SCL
RWI2C rwi2c;

extern uint8_t BigFont[];          // Declare which fonts we will be using
extern uint8_t SmallFont[];        
extern uint8_t SevenSegNumFont[];  
//--------------- pin map -----------------------
#define TEMP_PIN         A1        //port for temperature sensor
#define LEVEL_TOP_PIN    A3        //port for liquid level sensor in top pot
#define HEAT_BOILER_PIN  13        //port for heater
#define PUMP_PIN         A5        //port for pump
#define BUZZER_PIN       A6        //port for buzzer
//-------------RTC REGISTERS map ------------
// 00h....06h keeps time - do not touch !!!
// 07h....oDh are Alarms - you can frelly use it
// 0Eh....12h are controll register - do not touch !!!
#define trace_adr         0x07     // 0-default( mainmenu ), 1-cooling(after desinfection), 2 - boiling, 3 - mashing
#define i_adr             0x08     // keep hop cykle counter for restore boiling process. value in range 0...5
#define pass_minutes_adr  0x09     // pass minutes for restore boiling process. value in range 0...255
#define last_hour_adr     0x0A     // last remembered hour for restore.
#define last_min_adr      0x0B     // last remembered minute for restore.
//------------ EEPROM map -------------------

#define cleanTime_adr      31      // time for cleaning
#define cleanTemp_adr      32      // temperature for cleaning
#define rNum_adr           32      // number of rests
#define hNum_adr           33      // number of hops
#define preheatDelta_adr   34      // temperature delta of preheating before adding malt (0...5)
#define coolTemp_adr       35      // target temperature after cooling
#define desinfTime_adr     36
#define twiceBoilTemp_adr  37      // temperature of boiling*2,  shoul be divided to 2 to get correct temperature of boiling
#define rTime_startAdr     40      // start address for keeping times of the rests, (6 cells of EEPROM)
#define rTemp_startAdr     50      // start address for keeping temperature of the rests, (6 cells of EEPROM)
#define hTime_startAdr     60      // start address for keeping time of adding hop(6 cells of EEPROM), last time is boiling finish time 

#define starttime_adr      70      // start address of process starttime(boiling) used for restore after power shootdown.unsigned long, 4 bytes needed (4 cells of EEPROM)

#define startLogTime_adr   90      // time. uses unsigned long type so 4 bytes needed (4 cells of EEPROM)
#define startLog_adr      100      // EEPROM temperature log start address

Time t;
String topic;                        // Name of the screen for navigation in menu
float  temp;                         // measured temp
int    pass_minutes;                 // time in minutes passed for heating or rest
int    hour, minute, sec;            // hours, minutes,secs
int    pressed_button;               // ID of pressed button
int    log_adr;                      // curent EEPROM address for logging temperature during mashing
unsigned long lastLogTime;           // time of last logging
unsigned long pumpStopTime;          //
float boilingTemp;                   // temperature of boiling
int   chDesinTime = 1;               // Time for desinfection of chiller
int   nopower_time=0;                // powed down time in minutes
byte  restore_flag=0;
byte  temp_byte;
byte   pumpStatus;                  //0=off, 1=on
byte   heaterStatus;				//0=off, 1=on

OneWire OW_boiler(TEMP_PIN);   // создаем экземпляр класса OneWire. Setup a oneWire instance to communicate with any OneWire devices on the pin
DallasTemperature t_sensor(&OW_boiler); // Pass our oneWire reference to Dallas Temperature.

//===========  user defined routines  ==============
float gettemp() {                        //getting temperature and update global variables
  temp = (t_sensor.getTempCByIndex(0));  //receive temperature to global variable temp. byIndex(0) refers to the first IC on the wire
  t_sensor.requestTemperatures();        //Send the commands to recalculate temp will be read next time
}
void printHeader() {                      //printing header string on the screen: Time, Title, Temperature
  gettemp();
  t = rtc.getTime();
  hour = t.hour;
  minute = t.min;
  rwi2c.put(last_hour_adr,hour);          //save last remembered hour for power monitor
  rwi2c.put(last_min_adr,minute);         //save last remembered minute for power monitor
  Serial.print("void printHeader(). Last remembered HH:MM = "); Serial.print(hour); Serial.print(":");Serial.println(minute);
  
  myGLCD.setFont(BigFont);                  //font big
  myGLCD.setBackColor(0, 0, 0);             //back color is black
  myGLCD.setColor(100, 255, 100);                 //color Green
  myGLCD.print(rtc.getTimeStr(FORMAT_SHORT), 0, 0); //print time HH:MM
  myGLCD.setColor(250, 100, 100);                 //color rose
  myGLCD.printNumF(temp, 1, 250, 0);              //print temperature
  myGLCD.setColor(255, 255, 255);                 //color White
  myGLCD.print(topic, CENTER, 0);                 //print topic of menu
}
void preheating(int temp_target, boolean logFlag) {
  Serial.println("preheating");
  float starttemp = temp;                                                // starting temperature. local variable
  unsigned long starttime = rtc.getUnixTime(rtc.getTime());              // starting time. local variable

  while ((temp_target - temp) > 0) {
    printHeader();
	icon();

    if ( logFlag == true and (rtc.getUnixTime(rtc.getTime()) - lastLogTime) / 60 >= 1) { //save temperature log to EEPROM every 1 minute
      lastLogTime = rtc.getUnixTime(rtc.getTime());
      EEPROM.update(log_adr, temp);
      log_adr++;
    }

	
	
	
	

      digitalWrite(HEAT_BOILER_PIN, HIGH);                     // switch-ON heaters
	  heaterStatus=1;
      if (digitalRead(LEVEL_TOP_PIN) == LOW ) {                // mash level is low. switch-ON pump (only if pump on/off delay time is passed)
        if (rtc.getUnixTime(rtc.getTime())-pumpStopTime>5){
			digitalWrite(PUMP_PIN, HIGH);
			pumpStatus=1;
			} 
      } else {
        digitalWrite(PUMP_PIN, LOW);                           // Overflow.     switch-OFF pump
		pumpStopTime=rtc.getUnixTime(rtc.getTime());
		pumpStatus=0;
      }
      myGLCD.setColor(255, 255, 255);
      myGLCD.print("   Pre-Heating      ", 0, 130);
      delay(100);
      myGLCD.print("   Pre-Heating.     ", 0, 130);
      delay(100);
      myGLCD.print("   Pre-Heating..    ", 0, 130);
      delay(100);
      myGLCD.print("   Pre-Heating...   ", 0, 130);
      delay(70);
    
	
	
	
	
	
    if ((rtc.getUnixTime(rtc.getTime()) - starttime) > 240 and (temp - starttemp) < 1) { // check if heaters work well
      digitalWrite(HEAT_BOILER_PIN, LOW);                    // switch-OFF heaters
	  heaterStatus=0;
      digitalWrite(PUMP_PIN, LOW);                           // switch-OFF pump
	  pumpStatus=0;
      myGLCD.setColor(255, 0, 0);
      myGLCD.print("    HEATER ERROR    ", 0, 130);          // Warning message: HEATER ERROR
      tone(BUZZER_PIN, 1047,400);
      delay(500);
      tone(BUZZER_PIN, 784,400);
      delay(500);
      tone(BUZZER_PIN, 1047,400);
      delay(500);
      tone(BUZZER_PIN, 784,400);
      delay(500);
	  //noTone(BUZZER_PIN);
      myGLCD.print("                    ", 0, 130);
    }
  }
  digitalWrite(HEAT_BOILER_PIN, LOW);                        // preheating finished --> switch-OFF heaters
  heaterStatus=0;
  digitalWrite(PUMP_PIN, LOW);                               // switch-OFF pump
  pumpStopTime=rtc.getUnixTime(rtc.getTime());
  pumpStatus=0;
  pass_minutes = (rtc.getUnixTime(rtc.getTime()) - starttime) / 60;             // how long was preheating. global variable
}
void termostat(int temp_target, int time_target, boolean logFlag) {      // Termostating procedure
  Serial.print("TERMOSTAT. temp:");
  Serial.print(temp_target);
  Serial.print(", time:");
  Serial.println(time_target);
  unsigned long starttime = rtc.getUnixTime(rtc.getTime());              // starting time. local variable

  pass_minutes = 0;                                                      // minutes passed since start termostating. global variable
  myGLCD.setColor(255, 255, 255);
  myGLCD.print("Termostating:  *C   ", 0, 110);
  myGLCD.printNumI(temp_target, 208, 110);
  while (time_target > pass_minutes) {                                   // check the time of termostating
    printHeader(); 
	icon();
                                                      // printing time, topic and temperature

    if ( logFlag == true and (rtc.getUnixTime(rtc.getTime()) - lastLogTime) / 60 >= 1) { //save temperature log to EEPROM every 1 minute
      lastLogTime = rtc.getUnixTime(rtc.getTime());
      EEPROM.update(log_adr, temp);
      log_adr++;
    }

      if (temp <= temp_target) {
        digitalWrite(HEAT_BOILER_PIN, HIGH);         // temperature less than target --> switch-ON the heater
		heaterStatus=1;
      } else {
        digitalWrite(HEAT_BOILER_PIN, LOW); // temperature >= target --> switch-off the heater
		heaterStatus=0;
      }
      if (digitalRead(LEVEL_TOP_PIN) == LOW) {
        if((rtc.getUnixTime(rtc.getTime())-pumpStopTime)>5){
			    digitalWrite(PUMP_PIN, HIGH); // mash level is low. switch-ON pump
			    pumpStatus=1;}
      } else {
        digitalWrite(PUMP_PIN, LOW);  // Overflow.             switch-OFF pump
		pumpStopTime=rtc.getUnixTime(rtc.getTime());
		pumpStatus=0;
      }
      myGLCD.setColor(255, 255, 255);
      myGLCD.print("Passed    of    mins", CENTER, 130);                  // print passed time
      myGLCD.printNumI(pass_minutes, 96, 130, 3);
      myGLCD.printNumI(time_target, 192, 130, 3);
   	
    pass_minutes = (rtc.getUnixTime(rtc.getTime()) - starttime) / 60;           // calculate how many minutes passed since termostating started
    rwi2c.put(pass_minutes_adr, pass_minutes);    //save pass_minutes for restore by power monitor
    //    Serial.println("save pass_minutes to RTC register");
   
  }
  myGLCD.print("                     ", 0, 110);            // termostating done
  myGLCD.print("                     ", 0, 130);
  digitalWrite(HEAT_BOILER_PIN, LOW);                       // SWITCH-OFF the heater
  heaterStatus=0;
  digitalWrite(PUMP_PIN, LOW);                              // switch-OFF pump
  pumpStopTime=rtc.getUnixTime(rtc.getTime());
  pumpStatus=0;
}
void mainmenu() {

  rwi2c.put(trace_adr, 0);
  temp_byte = rwi2c.get(trace_adr);
  Serial.print("main menu. trace="); Serial.println(temp_byte);
  
  int  b_Mashing, b_Clean, b_Boiling, b_Cool, b_Service, b_SystemS, b_seeLog; // buttons IDs
  myGLCD.clrScr();                                                      // Clear LCD
  myButtons.deleteAllButtons();                                         // Clear old buttons
  topic = "";                                                           // Topic
  myButtons.setTextFont(BigFont);                                       // Font for buttons

  b_Mashing   = myButtons.addButton(5, 30, 140, 60, "MASH");        // Initialisation of Buttons
  b_Boiling   = myButtons.addButton(5, 100, 140, 60, "BOIL");
  b_Cool      = myButtons.addButton(5, 170, 140, 60, "COOL");

  b_Clean     = myButtons.addButton(170, 30, 140, 60, "CLEAN");
  b_seeLog    = myButtons.addButton(170, 100, 140, 40, "see Log");
  b_Service   = myButtons.addButton(170, 145, 140, 40, "Service");
  b_SystemS   = myButtons.addButton(170, 190, 140, 40, "System");


  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE); // Butons color
  myButtons.drawButtons();                                                     // Draw all buttons
  Serial.println("mainMenu buttons drowing finished. Start reading of buttons");
  while (true) {                                              // read buttons cycle
    printHeader();                                            // printing time, topic and temperature
    if (myTouch.dataAvailable()) {                            // if someting touched
      pressed_button = myButtons.checkButtons();             // read buttons
      if (pressed_button == b_Clean)  {
        clean();          // CLEAN pressed
      }
      if (pressed_button == b_Mashing) {
        mashing_setup();  // MASHING pressed
      }
      if (pressed_button == b_Boiling) {
        boiling_setup();  // Boiling pressed
      }
      if (pressed_button == b_Cool)   {
        cool_setup();     // Cool pressed
      }
      if (pressed_button == b_Service) {
        service();        // Service pressed
      }
      if (pressed_button == b_SystemS) {
        systemS();        // System setup pressed
      }
      if (pressed_button == b_seeLog) {
        seeLog();         // seeLog pressed
      }
    }
  }
}
void clean() {

  Serial.println("Cleaning");
  int b_TimeMinus, b_TimePlus, b_TempMinus, b_TempPlus, b_Start, b_Menu;     //buttons identificators
  topic = "CLEANING";                                                        //topic
  myGLCD.clrScr();                                                           //clear screen
  myButtons.deleteAllButtons();                                              //clear old buttons
  myGLCD.setFont(BigFont);                                                   //font big
  myGLCD.setColor(255, 255, 255);                                            //color white
  myGLCD.setBackColor(0, 0, 0);                                              //back color black
  myGLCD.print("Time to clean", 0, 50);                                      //printing
  myGLCD.print("Temperature", 0, 80);
  myGLCD.printNumI(EEPROM.read(cleanTime_adr), 255, 50, 2);
  myGLCD.printNumI(EEPROM.read(cleanTemp_adr) * 10, 245, 80, 3);

  myButtons.setTextFont(BigFont);                                              //buttons font
  b_TimeMinus   = myButtons.addButton(220, 50, 20, 20, "-");                   //initialisation of buttons
  b_TimePlus    = myButtons.addButton(295, 50, 20, 20, "+");
  b_TempMinus   = myButtons.addButton(220, 80, 20, 20, "-");
  b_TempPlus    = myButtons.addButton(295, 80, 20, 20, "+");
  b_Start       = myButtons.addButton(140, 195, 120, 40, "Start");
  b_Menu        = myButtons.addButton(00, 195, 120, 40, "Menu");
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE); //buttons color
  myButtons.drawButtons();                                                     //draw buttons

  while (true) {                                                               //button reading cycle
    printHeader();                                                            //print time,topic and temperature
    if (myTouch.dataAvailable()) {                                            //if anything touched
      pressed_button = myButtons.checkButtons();                              //read buttons
      if (pressed_button == b_TimeMinus and EEPROM.read(cleanTime_adr) > 0 ) { // - Time pressed
        EEPROM.write(cleanTime_adr, EEPROM.read(cleanTime_adr) - 1);
        myGLCD.printNumI(EEPROM.read(cleanTime_adr), 255, 50, 2);
      }
      if (pressed_button == b_TimePlus and EEPROM.read(cleanTime_adr) < 30) { // + Time pressed
        EEPROM.write(cleanTime_adr, EEPROM.read(cleanTime_adr) + 1);
        myGLCD.printNumI(EEPROM.read(cleanTime_adr), 255, 50, 2);
      }
      if (pressed_button == b_TempMinus and EEPROM.read(cleanTemp_adr) * 10 > 10 ) {           // - temp pressed
        EEPROM.write(cleanTemp_adr, EEPROM.read(cleanTemp_adr) - 1);
        myGLCD.printNumI(EEPROM.read(cleanTemp_adr) * 10, 245, 80, 3);
      }
      if (pressed_button == b_TempPlus and EEPROM.read(cleanTemp_adr) * 10 < 100 ) {            // + temp pressed
        EEPROM.write(cleanTemp_adr, EEPROM.read(cleanTemp_adr) + 1);
        myGLCD.printNumI(EEPROM.read(cleanTemp_adr) * 10, 245, 80, 3);
      }
      if (pressed_button == b_Menu) {
        mainmenu();  // MENU pressed
      }
      if (pressed_button == b_Start) {                                    // START pressed

        myButtons.disableButton(b_Start, true);                          // disable buttons
        myButtons.disableButton(b_Menu, true);

        preheating(EEPROM.read(cleanTemp_adr) * 10, false);              // preheating, do not store temperature log
        myGLCD.print("pre-heated in     min", 0, 130);                   // print results info
        myGLCD.printNumI(pass_minutes, 225, 130);

        termostat(EEPROM.read(cleanTemp_adr) * 10, EEPROM.read(cleanTime_adr), false);       // termostating, do not store temperature log
        myGLCD.print("Cleaning done", 0, 130);                           // termostating done
        tone(BUZZER_PIN, 1047, 1000);

        myButtons.enableButton(b_Start, true);                           // Enable buttons
        myButtons.enableButton(b_Menu, true);
      }
    }
  }
}
void mashing_setup() {
  int b_TimeMinus[6], b_TimePlus[6], b_TempMinus[6], b_TempPlus[6], b_Menu, b_Mash, b_RestMinus, b_RestPlus, b_Next; //buttons IDs
  int i;                                                                 //counter
  topic = "MASH stp";                                                    //topic
  myGLCD.clrScr();                                                       //clear screen
  myButtons.deleteAllButtons();                                          //delete old buttons
  myButtons.setTextFont(BigFont);                                        //buttons font
  myGLCD.setFont(BigFont);                                               //font big  --============ drawing menu for choosing N of rests ===========--
  myGLCD.setBackColor(0, 0, 0);                                          //back color black
  myGLCD.setColor(255, 255, 255);                                        //color white
  myGLCD.print("N of Rests", 0, 50);                                     //print info
  myGLCD.printNumI(EEPROM.read(rNum_adr), 253, 50, 2);
  b_RestMinus = myButtons.addButton(230, 50 + i * 22, 20, 20, "-");      // buttons initialisation
  b_RestPlus  = myButtons.addButton(290, 50 + i * 22, 20, 20, "+");
  b_Menu      = myButtons.addButton(30, 195, 120, 40, "Menu");
  b_Next      = myButtons.addButton(170, 195, 120, 40, "Next");
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE); // buttons color
  myButtons.drawButtons();                                                     // draw all buttons
  while (true) {                                                               // reading buttons for choosing N of rests
    printHeader();                                                          // print time, topic and temperature
    if (myTouch.dataAvailable()) {                                          // if something touched
      pressed_button = myButtons.checkButtons();                            // read button
      if (pressed_button == b_RestMinus and EEPROM.read(rNum_adr) > 1 ) {                    // - REST pressed
        EEPROM.write(rNum_adr, EEPROM.read(rNum_adr) - 1);
        myGLCD.printNumI(EEPROM.read(rNum_adr), 253, 50, 2);
      }
      if (pressed_button == b_RestPlus and EEPROM.read(rNum_adr) < 6 ) {                     // + REST pressed
        EEPROM.write(rNum_adr, EEPROM.read(rNum_adr) + 1);
        myGLCD.printNumI(EEPROM.read(rNum_adr), 253, 50, 2);
      }
      if (pressed_button == b_Menu) {
        mainmenu();  // MENU pressed
      }
      if (pressed_button == b_Next) {
        goto MashingNext;  // NEXT pressed
      }
    }
  }
MashingNext:                                                         // if NEXT pessed
  myGLCD.clrScr();                                                   // clear screen
  myGLCD.setColor(255, 255, 255);                                    // color
  myButtons.deleteAllButtons();                                      // delete old buttons
  for (i = 0; i < EEPROM.read(rNum_adr); i++) {                      // -----=========  drawing menu for choosing time and temperature of rests =====-------
    myGLCD.print("Rest", 0, 50 + i * 22);                            //printing string by string
    myGLCD.printNumI(i + 1, 64, 50 + i * 22);
    myGLCD.printNumI(EEPROM.read(rTime_startAdr+i), 115, 50 + i * 22, 3);
    myGLCD.printNumI(EEPROM.read(rTemp_startAdr+i), 253, 50 + i * 22, 2);
    b_TimeMinus[i]   = myButtons.addButton(90, 50 + i * 22, 20, 20, "-"); // + - buttons initialisation string by string
    b_TimePlus[i]    = myButtons.addButton(165, 50 + i * 22, 20, 20, "+");
    b_TempMinus[i]   = myButtons.addButton(230, 50 + i * 22, 20, 20, "-");
    b_TempPlus[i]    = myButtons.addButton(290, 50 + i * 22, 20, 20, "+");
  }
  myGLCD.setColor(50, 50, 255);                                         // text color Blue
  myGLCD.print("     minutes    *C  ", 0, 30);                          // print

  b_Menu        = myButtons.addButton(30, 195, 120, 40, "Menu");        // MENU button initialisation
  b_Mash        = myButtons.addButton(170, 195, 120, 40, "Mash");       // MASH buttons initialisation
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE); // buttons color
  myButtons.drawButtons();                                              // draw buttons
  while (true) {                                                        //read the buttons for choosing time and temperature of rests
    printHeader();
    if (myTouch.dataAvailable()) {
      pressed_button = myButtons.checkButtons();
      for (i = 0; i < EEPROM.read(rNum_adr); i++) {                                    //read string by string
        if (pressed_button == b_TimeMinus[i] and EEPROM.read(rTime_startAdr + i) > 0 ) {   // - Time pressed
          EEPROM.write(rTime_startAdr + i, EEPROM.read(rTime_startAdr + i) - 1);
          myGLCD.printNumI(EEPROM.read(rTime_startAdr + i), 115, 50 + i * 22, 3);
        }
        if (pressed_button == b_TimePlus[i] and EEPROM.read(rTime_startAdr + i) < 120 ) {  // + Time pressed
          EEPROM.write(rTime_startAdr + i, EEPROM.read(rTime_startAdr + i) + 1);
          myGLCD.printNumI(EEPROM.read(rTime_startAdr + i), 115, 50 + i * 22, 3);
        }
        if (pressed_button == b_TempMinus[i] and EEPROM.read(rTemp_startAdr + i) > 30 ) {  // - temp pressed
          EEPROM.write(rTemp_startAdr + i, EEPROM.read(rTemp_startAdr + i) - 1);
          myGLCD.printNumI(EEPROM.read(rTemp_startAdr + i), 253, 50 + i * 22, 2);
        }
        if (pressed_button == b_TempPlus[i] and EEPROM.read(rTemp_startAdr + i) < 99 ) {  // + temp pressed
          EEPROM.write(rTemp_startAdr + i, EEPROM.read(rTemp_startAdr + i) + 1);
          myGLCD.printNumI(EEPROM.read(rTemp_startAdr + i), 253, 50 + i * 22, 2);
        }
      }
      if (pressed_button == b_Menu) {
        mainmenu();  // MENU pressed
      }
      if (pressed_button == b_Mash) {
        mash();  // MASH pressed
      }
    }
  }
}
void mash() {
  int b_Menu, b_Boiling, i, stopLog_adr, t_drop, n, tn, c;    // Buttons IDs and counter, local variables
  topic = "Mashing";                         // Topic
  myGLCD.clrScr();                           // clear screen
  myButtons.deleteAllButtons();              // delete old buttons
  myButtons.setTextFont(BigFont);            // buttons font
  myGLCD.setFont(BigFont);                   // text font
  myGLCD.setBackColor(0, 0, 0);              // back color black
  myGLCD.setColor(255, 255, 255);            // color white

  if ( rwi2c.get(trace_adr) == 3){goto restore_mashing; }
  
  myGLCD.print(" Preheatin of water ", 0, 110);
  myGLCD.print(" before adding malt ", 0, 130);
  preheating(EEPROM.read(rTemp_startAdr) + EEPROM.read(preheatDelta_adr), false);    // preheating before adding malt, do not store temperature log
  myGLCD.setColor(255, 100, 100);            // color red
  myGLCD.print("Please add malt     ", 0, 110);  // show malt add inviting message
  myGLCD.print("then tap the screen ", 0, 130);

  c=0;
  while (myTouch.dataAvailable() != true) {  // read buttons
    ++c;
    if (c> 5) {tone(BUZZER_PIN, 1047,100); c=0;}
    printHeader();
  }
  
  myGLCD.print("                    ", 0, 110);
  myGLCD.setColor(50, 50, 255);              // text color Blue
  myGLCD.print("     minutes    *C  ", 0, 30); // print text
  myGLCD.setColor(255, 255, 255);            // color white

  for ( i = startLog_adr ; i < startLog_adr + 300 ; i++) {
    EEPROM.update(i, 0); //clear temperature log for 5 h duration
  }

  EEPROM.put(startLogTime_adr, rtc.getUnixTime(rtc.getTime()));         // save mashing start time
  EEPROM.update(startLog_adr, temp);                                    // save first logged temperature
  lastLogTime = rtc.getUnixTime(rtc.getTime());                         // time of last log data
  log_adr = startLog_adr + 1;                                           // addres for next logging

  rwi2c.put(trace_adr, 3);                            // remember mashing procedure in case of power shootdown
  rwi2c.put( i_adr,0);    Serial.println("save i to RTC register");  // save i=0 in to RTC register for restore after power shootdown     
  
  for (i = 0; i < EEPROM.read(rNum_adr); i++) {       // rest by rest preheting and termostating
restore_mashing:                                   //MASHING RESTORE START
    if (restore_flag != 0){   
      Serial.println("MASHING RESTORE START");  
      myGLCD.setColor(50, 50, 255);                // text color Blue
      myGLCD.print("     minutes    *C  ", 0, 30); // print top row
      myGLCD.setColor(255, 255, 255);              // color white
          
      i=rwi2c.get(i_adr);                          // restore i
      Serial.print("restored i=");         Serial.println(i);
      myGLCD.print("Rest", 0, 50);
      myGLCD.printNumI(i + 1, 64, 50);             // rest #
      myGLCD.printNumI(EEPROM.read(rTime_startAdr + i), 115, 50, 3);  //duration of rest
      myGLCD.printNumI(EEPROM.read(rTemp_startAdr + i), 253, 50, 2);  //temperature of rest

     stopLog_adr = startLog_adr;                              // let's look for end of log.
     while (EEPROM.read(stopLog_adr)!=0) { ++stopLog_adr; }   // Now stopLog_adr keeps addres of first empty log cell.
     Serial.print("Конец лога. Лог=0 на минуте = "); Serial.println(stopLog_adr-startLog_adr);
     log_adr = stopLog_adr + nopower_time;                    // address for continue loging after blackout
     gettemp();                                               // let's filing blackout log gap with approximation
     t_drop = EEPROM.read(stopLog_adr-1)-temp;            
     for ( n = 0; n < nopower_time; ++n){
        tn = temp + t_drop - (t_drop/nopower_time)*(n+1);
        EEPROM.update(stopLog_adr+n, tn );
     }

      pass_minutes= rwi2c.get(pass_minutes_adr);  
      if (nopower_time < (EEPROM.read(rTime_startAdr + i)-pass_minutes)) {     //finish mashing step
          termostat(EEPROM.read(rTemp_startAdr + i), EEPROM.read(rTime_startAdr + i)- nopower_time - pass_minutes, true);  //termostating on the rest temperature, store temperature log
      }    
      restore_flag=0;         //restoring of mash process is finished
    }  
    else {                    // normal operation
      myGLCD.print("Rest", 0, 50);
      myGLCD.printNumI(i + 1, 64, 50);                                    // rest #
      myGLCD.printNumI(EEPROM.read(rTime_startAdr + i), 115, 50, 3);      //duration of rest
      myGLCD.printNumI(EEPROM.read(rTemp_startAdr + i), 253, 50, 2);      //temperature of rest 
      rwi2c.put( i_adr,i);    Serial.println("save i to RTC register");   // save i in to RTC register for restore after power shootdown 	  
      preheating(EEPROM.read(rTemp_startAdr + i), true);                  //preheating to the rest temperature, store temperature log        
      termostat(EEPROM.read(rTemp_startAdr + i), EEPROM.read(rTime_startAdr + i), true);  //termostating on the rest temperature, store temperature log 
    }  
  }

  //myGLCD.print("Draining mash , pre-", 0, 110);
  //myGLCD.print("-heating for boiling", 0, 130);  
  //boilingTemp = EEPROM.read(twiceBoilTemp_adr) * 0.5;  // float variable boilingTemp read from EEPROM
  //preheating(boilingTemp-1, true);                     // PRE-HEATING before boiling,  store temperature log

  rwi2c.put(trace_adr, 0);                             // indicate sucsessfull done of mashing process for power shootdown manager
  
  myGLCD.print("    Mashing done.    ", 0, 110);       // finished all mash steps
  myGLCD.print("U can rinse the malt", 0, 130); 

  b_Menu    = myButtons.addButton(30, 195, 120, 40, "Menu");     // buttons initialisation
  b_Boiling = myButtons.addButton(160, 195, 150, 40, "Boiling"); 
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();                                    // draw buttons

  c=0;
  while (true) {                                              // read buttons
    ++c;
    if(c>50){tone(BUZZER_PIN, 1047,1000); c=0;}
    printHeader();
    if (myTouch.dataAvailable()) {
      pressed_button = myButtons.checkButtons();
      if (pressed_button == b_Menu)   {
        mainmenu();       // MENU pressed
      }
      if (pressed_button == b_Boiling) {
        boiling_setup();  // BOILING pressed
      }
    }
  }
}
void boiling_setup() {
  int b_TimeMinus[6], b_TimePlus[6], b_Menu, b_Boil, b_hopMinus, b_hopPlus, b_boiltMinus, b_boiltPlus, b_prehDeMinus, b_prehDePlus, b_Next; //buttons IDs
  
  int i;                                                   // counter
  topic = "BOIL stp";                                      // Topic
  myGLCD.clrScr();                                         // clear screen
  myButtons.deleteAllButtons();                            // delete old buttons
  myButtons.setTextFont(BigFont);                          // buttons font
  myGLCD.setFont(BigFont);                                 // text font
  myGLCD.setBackColor(0, 0, 0);                            // back color black
  myGLCD.setColor(255, 255, 255);                          // text color white
  myGLCD.print("N of Hops", 0, 50);                        // drawing menu for choosing N of hops
  myGLCD.print("Preheat delt", 0, 100);
  myGLCD.print("Boiling temp", 0, 130);
  
  myGLCD.printNumI(EEPROM.read(hNum_adr), 250, 50, 2);
  myGLCD.printNumI(EEPROM.read(preheatDelta_adr), 250, 100);
  boilingTemp = EEPROM.read(twiceBoilTemp_adr) * 0.5;      //read boil temp form EEPROM
  myGLCD.printNumF(boilingTemp, 1, 222, 130, '.', 4);
  
  b_hopMinus     = myButtons.addButton(197, 50 , 20, 20, "-");
  b_hopPlus      = myButtons.addButton(300, 50 , 20, 20, "+");
  b_prehDeMinus  = myButtons.addButton(197, 100, 20, 20, "-");
  b_prehDePlus   = myButtons.addButton(300, 100, 20, 20, "+");
  b_boiltMinus   = myButtons.addButton(197, 130, 20, 20, "-");
  b_boiltPlus    = myButtons.addButton(300, 130, 20, 20, "+");
  b_Menu         = myButtons.addButton(30, 195, 120, 40, "Menu");
  b_Next         = myButtons.addButton(170, 195, 120, 40, "Next");
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();

  while (true) {                                         //read buttons for choosing N of hops
    printHeader();
    if (myTouch.dataAvailable()) {
      pressed_button = myButtons.checkButtons();
      if (pressed_button == b_hopMinus and EEPROM.read(hNum_adr) > 1 ) { // - HOP pressed
        EEPROM.write(hNum_adr, EEPROM.read(hNum_adr) - 1);
        myGLCD.printNumI(EEPROM.read(hNum_adr), 250, 50, 2);
      }
      if (pressed_button == b_hopPlus and EEPROM.read(hNum_adr) < 5 ) {  // + HOP pressed
        EEPROM.write(hNum_adr, EEPROM.read(hNum_adr) + 1);
        myGLCD.printNumI(EEPROM.read(hNum_adr), 250, 50, 2);
      }
      if (pressed_button == b_prehDeMinus and EEPROM.read(preheatDelta_adr) > 0 ) { // - preheat delta temp pressed
        EEPROM.write(preheatDelta_adr, EEPROM.read(preheatDelta_adr) - 1);
        myGLCD.printNumI(EEPROM.read(preheatDelta_adr), 250, 100);
      }
      if (pressed_button == b_prehDePlus and EEPROM.read(preheatDelta_adr) < 5 ) { // + preheat delta temp pressed
        EEPROM.write(preheatDelta_adr, EEPROM.read(preheatDelta_adr) + 1);
        myGLCD.printNumI(EEPROM.read(preheatDelta_adr), 250, 100);
      }
      if (pressed_button == b_boiltMinus and boilingTemp > 95 ) { // - boil temp pressed
        boilingTemp = boilingTemp - 0.5;
        myGLCD.printNumF(boilingTemp, 1, 222, 130, '.', 4);
        EEPROM.update(twiceBoilTemp_adr, round(boilingTemp * 2));
      }
      if (pressed_button == b_boiltPlus and boilingTemp < 105 ) { // + boil temp pressed
        boilingTemp = boilingTemp + 0.5;
        myGLCD.printNumF(boilingTemp, 1, 222, 130, '.', 4);
        EEPROM.update(twiceBoilTemp_adr, round(boilingTemp * 2));
      }	  
	  if (pressed_button == b_Menu) {
        mainmenu();           // MENU pressed
      }
      if (pressed_button == b_Next) {
        goto BoilingSetNext;  // NEXT pressed
      }
    }
  }
BoilingSetNext:
  myGLCD.clrScr();                                      // clear screen
  myButtons.deleteAllButtons();                         // delete old buttons
  myGLCD.setColor(255, 255, 255);                       // text color
  for (i = 0; i <= EEPROM.read(hNum_adr); i++) {        // drawing string by string to choose hop times
    myGLCD.print("Hop", 0, 50 + i * 22);                // HOP info string by string
    myGLCD.printNumI(i + 1, 48, 50 + i * 22);
    myGLCD.printNumI(EEPROM.read(hTime_startAdr + i), 135, 50 + i * 22, 3);
    b_TimeMinus[i]   = myButtons.addButton(110, 50 + i * 22, 20, 20, "-"); // + - buttons string by string
    b_TimePlus[i]    = myButtons.addButton(185, 50 + i * 22, 20, 20, "+");
  }
  myGLCD.print("Finish", 0, 50 + EEPROM.read(hNum_adr) * 22);                     // reprint last string because last hop time is finish time
  myGLCD.setColor(50, 50, 255);                                  // text color Blue
  myGLCD.print("     on minute      ", 0, 30);                   // print
  b_Menu        = myButtons.addButton(30, 195, 120, 40, "Menu"); // MENU button initialisation
  b_Boil        = myButtons.addButton(170, 195, 120, 40, "Boil"); // BOIL button initialisation
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();                                       // draw buttons
  while (true) {                                                 //read buttons for choosing time for hops and finish time
    printHeader();
    if (myTouch.dataAvailable()) {
      pressed_button = myButtons.checkButtons();
      for (i = 0; i <= EEPROM.read(hNum_adr); i++) {                             //string by string
        if (pressed_button == b_TimeMinus[i] and EEPROM.read(hTime_startAdr + i) > 0 ) { // - Time pressed
          EEPROM.write(hTime_startAdr + i, EEPROM.read(hTime_startAdr + i) - 1);
          myGLCD.printNumI(EEPROM.read(hTime_startAdr + i), 135, 50 + i * 22, 3);
        }
        if (pressed_button == b_TimePlus[i] and EEPROM.read(hTime_startAdr + i) < 120 ) { // + Time pressed
          EEPROM.write(hTime_startAdr + i, EEPROM.read(hTime_startAdr + i) + 1);
          myGLCD.printNumI(EEPROM.read(hTime_startAdr + i), 135, 50 + i * 22, 3);
        }
      }
      if (pressed_button == b_Menu) { mainmenu(); }
      if (pressed_button == b_Boil) { boil();     }
    }
  }
}
void boil() {
  int b_Menu, b_Cool,b_boiltMinus, b_boiltPlus, i, c;  // buttons IDs and counter
  unsigned long starttime;                  // start time of boilind
  unsigned int pass_minutes = 0;            // minutes passed since starting boiling
  unsigned int time_penalty=0;              // additional secunds in case of waiting adding hop
  unsigned long penalty_start;
  
  topic = "Boiling";
  myGLCD.clrScr();
  myGLCD.setFont(BigFont);                   // text font
  myGLCD.setBackColor(0, 0, 0);              // back color
  myGLCD.setColor(255, 255, 255);            // text color white
    
  myButtons.deleteAllButtons();
  myButtons.setTextFont(BigFont);            // buttons font 
  b_boiltMinus = myButtons.addButton(0, 180, 120, 50, "-0.5*C");
  b_boiltPlus  = myButtons.addButton(140, 180, 120, 50, "+0.5*C");
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();
    
  boilingTemp = EEPROM.read(twiceBoilTemp_adr) * 0.5;      // float variable boilingTemp read from EEPROM
  
  if ( rwi2c.get(trace_adr) == 2){goto restore_boiling; }
  preheating(boilingTemp, false);                          // PRE-HEATING, do not store temperature log
  
  starttime = rtc.getUnixTime(rtc.getTime());              // fix start time of boilind
  EEPROM.put(starttime_adr, starttime);                   // save boiling start time
          Serial.println("save boiling starttime to EEPROM");  
  rwi2c.put(trace_adr, 2);                                // remember boiling stage in case of power shootdown
  Serial.print("(boiling process)Power monitor trace ="); Serial.println(rwi2c.get(trace_adr),HEX);
  Serial.println("Starttime saved in EEPROM  ");
 // !!!!  Boiling. Next stage execution will be automaticaly continued by power monitor in case of power shootdown and restore !!!!!!  
  
  for (i = 0; i < EEPROM.read(hNum_adr) + 1; ++i) {   // repite hNum (N of Hops) + finish time
    rwi2c.put( i_adr,i);    Serial.println("save i to RTC register");                    // save i in to RTC register
restore_boiling:
    if (restore_flag != 0){   //BOILING RESTORE START
      i=rwi2c.get(i_adr);
      EEPROM.get(starttime_adr, starttime); 
      pass_minutes=rwi2c.get(pass_minutes_adr);
      time_penalty=rtc.getUnixTime(rtc.getTime())-starttime-pass_minutes*60;
      restore_flag=0;
   }

    myGLCD.print("Boiling at        *C", 0, 90 );     
    myGLCD.printNumF( boilingTemp , 1 , 200 , 90 );
    
    myButtons.enableButton(b_boiltMinus, true);
    myButtons.enableButton(b_boiltPlus, true);
    
    while (EEPROM.read(hTime_startAdr + i) > pass_minutes) {        // check time for next hop portion
      printHeader();
	  icon();
        if (myTouch.dataAvailable()) {
              pressed_button = myButtons.checkButtons();
                   if (pressed_button == b_boiltMinus /*and boilingTemp > 95*/ ) { // - boil temp pressed
                       boilingTemp = boilingTemp - 0.5;
                       myGLCD.printNumF(boilingTemp,1,200,90);
                       EEPROM.update(twiceBoilTemp_adr, round(boilingTemp * 2));
                       }
                   if (pressed_button == b_boiltPlus and boilingTemp < 105 ) { // + boil temp pressed
                       boilingTemp = boilingTemp + 0.5;
                       myGLCD.printNumF(boilingTemp,1,200,90);
                       EEPROM.update(twiceBoilTemp_adr, round(boilingTemp * 2));
                       }
         }    
            if (temp < boilingTemp) {
                  digitalWrite(HEAT_BOILER_PIN, HIGH);    //temperature less than target --> switch-ON the heater
				  heaterStatus=1;
            } else {
                  digitalWrite(HEAT_BOILER_PIN, LOW);    //temperature >= target --> switch-off the heater
				  heaterStatus=0;
            }
            myGLCD.setColor(255, 255, 255);
            myGLCD.print("Passed    of    mins", CENTER, 130); //
            myGLCD.printNumI(pass_minutes, 96, 130, 3);      // print passed time
            myGLCD.printNumI(EEPROM.read(hTime_startAdr + EEPROM.read(hNum_adr)), 192, 130, 3);  // print total time of boiling
     
	 pass_minutes = (rtc.getUnixTime(rtc.getTime()) - starttime - time_penalty) / 60;    // recalculate passed time     
     rwi2c.put(pass_minutes_adr, pass_minutes);              // save pass_minutes to RTC register
    }
    myGLCD.print("                     ", 0, 110);
    digitalWrite(HEAT_BOILER_PIN, LOW);                     // time for next hop, switch-off heater
    heaterStatus=0;
    myGLCD.print("Hop #   on    minute", 0, 50);
    myGLCD.printNumI(i + 1, 80, 50);
    myGLCD.printNumI(EEPROM.read(hTime_startAdr + i), 160, 50, 3);
    myGLCD.setColor(255, 100, 100);                        // text color red
    myGLCD.print(" Please add the hop ", 0, 90);      // show hop add inviting message
    myGLCD.print("then tap the screan ", 0, 130);

    myButtons.disableButton(b_boiltMinus, true);
    myButtons.disableButton(b_boiltPlus, true);
    penalty_start=millis();

    c=0;
    while (i < EEPROM.read(hNum_adr) and myTouch.dataAvailable() != true) {  // waiting for touch
      ++c;
      if(c>5){tone(BUZZER_PIN, 1047,100); c=0;}
      printHeader();

      }  
    myGLCD.setColor(255, 255, 255);
    myGLCD.print("      Hop added     ", 0, 90);
    myGLCD.print("                    ", 0, 130);
    delay(1000);
    time_penalty = time_penalty + (millis()-penalty_start)/1000;                      // add waiting period to time_penalty 
    pass_minutes = (rtc.getUnixTime(rtc.getTime()) - starttime - time_penalty) / 60;  // recalculate passed minutes
    rwi2c.put(pass_minutes_adr, pass_minutes);       // save pass_minutes to RTC register
  }
  rwi2c.put(trace_adr, 0);                           // clear cooling flag for power monitor
  
  myGLCD.clrScr();
  myButtons.deleteAllButtons();
  myGLCD.print("    BOILING DONE    ", 0, 110);

  b_Menu = myButtons.addButton(30, 195, 120, 40, "Menu"); //final buttons after boiling done
  b_Cool = myButtons.addButton(170, 195, 120, 40, "Cool");

  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();

  c=0;
  while (true) {                                               // reading of buttons
    ++c;
    if(c>50){tone(BUZZER_PIN, 1047, 1000); c=0;}
    printHeader();
    if (myTouch.dataAvailable()) {
      pressed_button = myButtons.checkButtons();
      if (pressed_button == b_Menu) {
        mainmenu();  // MENU pressed
      }
      if (pressed_button == b_Cool) {
        cool_setup();  // COOL pressed
      }
    }
  }
}
void cool_setup() {
  int b_TempMinus, b_TempPlus, b_Next, b_desTimeMinus, b_desTimePlus, b_Menu;   // buttons IDs
  topic = "COOL stp";                                   // topic
  myGLCD.clrScr();                                      // clear screen
  myButtons.deleteAllButtons();                         // delete old buttons
  myGLCD.setFont(BigFont);                              // text font
  myGLCD.setColor(255, 255, 255);                       // text color white
  myGLCD.setBackColor(0, 0, 0);                         // back color black
  myGLCD.print("Target temprtr", 0, 50);                 // print cooling target temp
  myGLCD.printNumI(EEPROM.read(coolTemp_adr), 250, 50, 2);
  myGLCD.print("Desinfct time", 0, 80);                 // print chiller desinfection time
  myGLCD.printNumI(EEPROM.read(desinfTime_adr), 250, 80, 2);

  myButtons.setTextFont(BigFont);                       // buttons font
  b_TempMinus = myButtons.addButton(225, 50, 20, 20, "-"); // buttons initialisation
  b_TempPlus = myButtons.addButton(285, 50, 20, 20, "+");
  b_desTimeMinus = myButtons.addButton(225, 80, 20, 20, "-");
  b_desTimePlus = myButtons.addButton(285, 80, 20, 20, "+");
  b_Next     = myButtons.addButton(170, 195, 120, 40, "Next");
  b_Menu     = myButtons.addButton(30, 195, 120, 40, "Menu");

  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();                              // draw buttons
  while (true) {                                        // reading buttons
    printHeader();
    if (myTouch.dataAvailable()) {
      pressed_button = myButtons.checkButtons();
      if (pressed_button == b_TempMinus and EEPROM.read(coolTemp_adr) > 20 ) { // - temp pressed
        EEPROM.update(coolTemp_adr, EEPROM.read(coolTemp_adr) - 1);
        myGLCD.printNumI(EEPROM.read(coolTemp_adr), 250, 50, 2);
      }
      if (pressed_button == b_TempPlus and EEPROM.read(coolTemp_adr) < 37 ) { // + temp pressed
        EEPROM.update(coolTemp_adr, EEPROM.read(coolTemp_adr) + 1);
        myGLCD.printNumI(EEPROM.read(coolTemp_adr), 250, 50, 2);
      }
      if (pressed_button == b_desTimeMinus and EEPROM.read(desinfTime_adr) > 1 ) { // - desinfection time pressed
        EEPROM.update(desinfTime_adr, EEPROM.read(desinfTime_adr) - 1);
        myGLCD.printNumI(EEPROM.read(desinfTime_adr), 250, 80, 2);
      }
      if (pressed_button == b_desTimePlus and EEPROM.read(desinfTime_adr) < 20 ) { // + desinfection time pressed
        EEPROM.update(desinfTime_adr, EEPROM.read(desinfTime_adr) + 1);
        myGLCD.printNumI(EEPROM.read(desinfTime_adr), 250, 80, 2);
      }
      if (pressed_button == b_Menu) {
        mainmenu();  // MENU pressed
      }
      if (pressed_button == b_Next) {  Cooling();  }
    }
  }
}
void Cooling() {
  int c;
  unsigned long starttime;
  float starttemp;
  float gradient;
  topic = "Cooling";
  if (  rwi2c.get(trace_adr)==1){ goto restore_cooling; }
  myButtons.deleteAllButtons();                         // delete buttons
  myGLCD.setBackColor(0, 0, 0);                         // back color black
  myGLCD.clrScr();                                      // clear screen
  myGLCD.setColor(255, 255, 255);
  myGLCD.print("    DESINFECTION    ", 0, 90);
  myGLCD.print("Dip the chiller then", 0, 110);         // DIP THE CHILLER invitation
  myGLCD.print("    tap the screen  ", 0, 130);

  c=0;
  while (myTouch.dataAvailable() != true) {  // read buttons
    ++c;
    if(c>5){tone(BUZZER_PIN, 1047,100); c=0;}
    printHeader();
  }
  myGLCD.print("                    ", 0, 110);
  myGLCD.print("                    ", 0, 130);

  boilingTemp = EEPROM.read(twiceBoilTemp_adr) * 0.5;         // float variable boilingTemp read from EEPROM
  preheating(boilingTemp, false);                             // PRE-HEATING, do not store temperature log
  termostat(boilingTemp, EEPROM.read(desinfTime_adr), false); // termostating  on boiling temp with desinfection time, do not store temperature log
  myGLCD.print("  DESINFECTION DONE ", 0, 90);                // DONE
  myGLCD.print("Pls,open cooler valv", 0, 110);
  myGLCD.print(" than tap the screen", 0, 130);

  c=0; 
  while (myTouch.dataAvailable() != true) {                   // waiting for touch
    ++c;
    if(c>5){tone(BUZZER_PIN, 1047,100); c=0;}
    printHeader();
  }

  // !!!!!  Cooling. Next stage execution will be automaticaly continued by power monitor in case of power shootdown and restore !!!!!!  
  rwi2c.put(trace_adr, 1);          // remember cooling stage in case of power shootdown

restore_cooling:
  myGLCD.clrScr(); 
  topic = "Cooling";
  printHeader();
  myGLCD.setColor(255, 255, 255);
  myGLCD.print("COOLING IN PROGRESS.", 0, 50);
  myGLCD.print("Target temperature: ", 0, 110);
  myGLCD.print("           *C       ", 0, 130);
  myGLCD.printNumI(EEPROM.read(coolTemp_adr), 144, 130, 2);

  while ((temp - EEPROM.read(coolTemp_adr)) > 0) {
    printHeader();
    starttime = rtc.getUnixTime(rtc.getTime());          //starting time. local variable
    starttemp = temp;                                    //starting temperature. local variable
    delay(5000);
    printHeader();
    gradient = 12 * (starttemp - temp) / (rtc.getUnixTime(rtc.getTime()) - starttime);
    myGLCD.print("chil speed:    *C/mn", 0, 170);
    myGLCD.printNumF(gradient, 1, 180, 170);
  }
  rwi2c.put(trace_adr, 0);                          // clear cooling flag for power monitor

  myGLCD.print("   COOLING DONE     ", 0, 50);
  myGLCD.print(" Tap the screen for ", 0, 110);
  myGLCD.print(" return to main menu", 0, 130);
  myGLCD.print("                    ", 0, 170);

  c=0;
  while (myTouch.dataAvailable() != true) {             // waiting for touch
    ++c;
    if(c>50){tone(BUZZER_PIN, 1047,1000); c=0;}
    printHeader();
  }
  mainmenu();
}
void service() {
  int b_HeaterOn, b_HeaterOff, b_PumpOn, b_PumpOff, b_Menu;  // buttons IDs
  topic = "Service";                             // topic
  myGLCD.clrScr();                               // clear screen
  myButtons.deleteAllButtons();                  // delete old buttons
  myGLCD.setFont(BigFont);                       // text font
  //myGLCD.setColor(50, 50, 255);                  // text color Blue
  myGLCD.setBackColor(0, 0, 0);                  // back color black
  //myGLCD.print("Unit    Satus Action", 0, 40);   // print
  myGLCD.setColor(255, 255, 255);                // text color White
  myGLCD.print("LEVEL", 0, 40);
  // myGLCD.print("Bott lvl ", 0, 80);
  // myGLCD.print("Heater     ", 0, 120); 
  // myGLCD.print("Pump       ", 0, 160);
  myGLCD.print("HEATER     ", 0, 90); 
  myGLCD.print("PUMP       ", 0, 140); 
  
  myButtons.setTextFont(BigFont);                          // buttons font
  b_HeaterOn  = myButtons.addButton(201, 75, 54, 45, "On"); // buttons initialisation
  b_HeaterOff = myButtons.addButton(265, 75, 54, 45, "Off");
  b_PumpOn    = myButtons.addButton(201, 130, 54, 45, "On");
  b_PumpOff   = myButtons.addButton(265, 130, 54, 45, "Off");
  b_Menu     = myButtons.addButton(60, 195, 120, 40, "Menu");
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();                              // draw buttons
  while (true) {
    printHeader();
	icon();
    if (digitalRead(LEVEL_TOP_PIN) == LOW) {
		myGLCD.setColor(50, 50, 255);                // text color Blue
		myGLCD.print("LOW ", 144, 40);
    } else {
		myGLCD.setColor(255, 0, 0);                  // text color Red
		myGLCD.print("HIGH", 144, 40);
        digitalWrite(PUMP_PIN, LOW);				// disable pump because overflow
		pumpStatus=0;		
    }
	myGLCD.setColor(50, 50, 255);                // text color Blue
    if (myTouch.dataAvailable()) {                  //reading buttons
      pressed_button = myButtons.checkButtons();
      if (pressed_button == b_HeaterOn ) {
        digitalWrite(HEAT_BOILER_PIN, HIGH);
		heaterStatus=1;
      }
      if (pressed_button == b_HeaterOff) {
        digitalWrite(HEAT_BOILER_PIN, LOW);
		heaterStatus=0;
      }
      if (pressed_button == b_PumpOn and digitalRead(LEVEL_TOP_PIN) == LOW ) {
        digitalWrite(PUMP_PIN, HIGH);
		pumpStatus=1;
      }
      if (pressed_button == b_PumpOff) {
        digitalWrite(PUMP_PIN, LOW);
		pumpStatus=0;
      }
      if (pressed_button == b_Menu) {
        digitalWrite(HEAT_BOILER_PIN, LOW);
		heaterStatus=0;
        digitalWrite(PUMP_PIN, LOW);
		pumpStatus=0;
        mainmenu();
      }
    }
	if(heaterStatus==0){
	    myGLCD.setColor(50, 50, 255);              //Blue
	    myGLCD.print("OFF", 144, 90);
	  }else{
	    myGLCD.setColor(255, 0, 0);                //Red
	    myGLCD.print("ON ", 144, 90);}
	if(pumpStatus==0){
      myGLCD.setColor(50, 50, 255);              //Blue
	    myGLCD.print("OFF", 144, 140);
	  }else{
      myGLCD.setColor(255, 0, 0);                //Red
	    myGLCD.print("ON ", 144, 140);}
  }
}
void systemS() {
  Serial.println("System Setup");

  int b_hMinus, b_hPlus, b_mMinus, b_mPlus, b_boiltMinus, b_boiltPlus, b_Menu, b_prehDeMinus, b_prehDePlus; //buttons IDs
  // b_calibrMinus, b_calibrPlus,

  topic = "System";                                    // Topic
  myGLCD.clrScr();                                     // clear screen
  myButtons.deleteAllButtons();                        // delete old buttons
  myButtons.setTextFont(BigFont);                      // buttons font
  myGLCD.setFont(BigFont);                             // text font
  myGLCD.setBackColor(0, 0, 0);                        // back color black
  myGLCD.setColor(255, 255, 255);                      // text color white

  myGLCD.print("Time, hours ", 0, 40);
  myGLCD.print("Time, minuts", 0, 70);
  myGLCD.print("Preheat delt", 0, 100);
  myGLCD.print("Boiling temp", 0, 130);
  //  myGLCD.print("Temp calibrt",0,160);

  t = rtc.getTime();
  myGLCD.printNumI(t.hour, 246, 40, 2, '0');
  myGLCD.printNumI(t.min, 246, 70, 2, '0');
  myGLCD.printNumI(EEPROM.read(preheatDelta_adr), 250, 100);
  boilingTemp = EEPROM.read(twiceBoilTemp_adr) * 0.5;      //read boil temp form EEPROM
  myGLCD.printNumF(boilingTemp, 1, 222, 130, '.', 4);
  // myGLCD.printNumF(tempCalibr,1,230,100);

  b_hMinus       = myButtons.addButton(197, 40, 20, 20, "-"); // buttons
  b_hPlus        = myButtons.addButton(300, 40, 20, 20, "+");
  b_mMinus       = myButtons.addButton(197, 70, 20, 20, "-");
  b_mPlus        = myButtons.addButton(300, 70, 20, 20, "+");
  b_prehDeMinus  = myButtons.addButton(197, 100, 20, 20, "-");
  b_prehDePlus   = myButtons.addButton(300, 100, 20, 20, "+");
  b_boiltMinus   = myButtons.addButton(197, 130, 20, 20, "-");
  b_boiltPlus    = myButtons.addButton(300, 130, 20, 20, "+");
  b_Menu         = myButtons.addButton(100, 195, 120, 40, "Menu");
  myButtons.setButtonColors(VGA_WHITE, VGA_GRAY, VGA_BLUE, VGA_RED, VGA_BLUE);
  myButtons.drawButtons();                                       // draw buttons
  while (true) {                                                 //read buttons for choosing time for hops and finish time
    printHeader();
    if (myTouch.dataAvailable()) {
      pressed_button = myButtons.checkButtons();
      if (pressed_button == b_hMinus and t.hour > 0 ) { // - hour pressed
        hour = t.hour - 1;
        myGLCD.printNumI(hour, 246, 40, 2, '0');
      }
      if (pressed_button == b_hMinus and t.hour == 0 ) { // - hour pressed and hour=0
        hour = 23;
        myGLCD.printNumI(hour, 246, 40, 2, '0');
      }
      if (pressed_button == b_hPlus and t.hour < 23 ) { // + hour pressed
        hour = t.hour + 1;
        myGLCD.printNumI(hour, 246, 40, 2, '0');
      }
      if (pressed_button == b_hPlus and t.hour == 23 ) { // + hour pressed and hour=23
        hour = 0;
        myGLCD.printNumI(hour, 246, 40, 2, '0');
      }
      if (pressed_button == b_mMinus and t.min > 0 ) { // - minute pressed
        minute = t.min - 1;
        myGLCD.printNumI(minute, 246, 70, 2, '0');
      }
      if (pressed_button == b_mMinus and t.min == 0 ) { // - minute pressed and minute=0
        minute = 59;
        myGLCD.printNumI(minute, 246, 70, 2, '0');
      }
      if (pressed_button == b_mPlus and t.min < 59 ) { // + minute pressed
        minute = t.min + 1;
        myGLCD.printNumI(minute, 246, 70, 2, '0');
      }
      if (pressed_button == b_mPlus and t.min == 59 ) { // + minute pressed and minute=59
        minute = 0;
        myGLCD.printNumI(minute, 246, 70, 2, '0');
      }
      if (pressed_button == b_prehDeMinus and EEPROM.read(preheatDelta_adr) > 0 ) { // - preheat delta temp pressed
        EEPROM.write(preheatDelta_adr, EEPROM.read(preheatDelta_adr) - 1);
        myGLCD.printNumI(EEPROM.read(preheatDelta_adr), 250, 100);
      }
      if (pressed_button == b_prehDePlus and EEPROM.read(preheatDelta_adr) < 5 ) { // + preheat delta temp pressed
        EEPROM.write(preheatDelta_adr, EEPROM.read(preheatDelta_adr) + 1);
        myGLCD.printNumI(EEPROM.read(preheatDelta_adr), 250, 100);
      }
      if (pressed_button == b_boiltMinus and boilingTemp > 95 ) { // - boil temp pressed
        boilingTemp = boilingTemp - 0.5;
        myGLCD.printNumF(boilingTemp, 1, 222, 130, '.', 4);
        EEPROM.update(twiceBoilTemp_adr, round(boilingTemp * 2));
      }
      if (pressed_button == b_boiltPlus and boilingTemp < 105 ) { // + boil temp pressed
        boilingTemp = boilingTemp + 0.5;
        myGLCD.printNumF(boilingTemp, 1, 222, 130, '.', 4);
        EEPROM.update(twiceBoilTemp_adr, round(boilingTemp * 2));
      }
      if (pressed_button == b_Menu) {
        mainmenu();  // MENU pressed
      }
      rtc.setTime(hour, minute, sec);
    }
  }
}
void icon(){
	Serial.println("draw Icon");
	int xPreset = 280; // Max 320  (+40)
	int yPreset = 200; // Max 240  (+40)
	myGLCD.setColor(0, 0, 0);                                // draw color Black	
	myGLCD.fillRect(xPreset,yPreset,xPreset+40,yPreset+40);  // clear drowing area
// draw external pot
	myGLCD.setColor(255, 255, 255);                              //  White
	myGLCD.drawRect(xPreset+10,yPreset+9,xPreset+37,yPreset+34); 
	myGLCD.setColor(0, 0, 0);                                   //  Black
	myGLCD.drawLine(xPreset+11, yPreset+9, xPreset+36, yPreset+9);
// draw internal pot
	myGLCD.setColor(255, 255, 255);                              //  White
	myGLCD.drawRect(xPreset+13,yPreset+7,xPreset+34,yPreset+31); 
	myGLCD.setColor(0, 0, 0);                                   //  Black
	myGLCD.drawLine(xPreset+14, yPreset+7, xPreset+33, yPreset+7);
// draw hose
	myGLCD.setColor(150, 150, 150);                              //  Gray
	myGLCD.drawRect(xPreset+3,yPreset+31,xPreset+10,yPreset+32); 
	myGLCD.drawRect(xPreset+3,yPreset+0,xPreset+25,yPreset+1); 
	myGLCD.drawRect(xPreset+3,yPreset+2,xPreset+4,yPreset+30); 
	
	
// Pump
	if(pumpStatus==0){
		myGLCD.setColor(0, 0,0);                              // Black
		myGLCD.fillRect(xPreset+20,yPreset+2,xPreset+28,yPreset+6);
   
    myGLCD.setColor(150, 150, 150);                              //  Gray
    myGLCD.drawRect(xPreset+3,yPreset+31,xPreset+10,yPreset+32); 
    myGLCD.drawRect(xPreset+3,yPreset+0,xPreset+25,yPreset+1); 
    myGLCD.drawRect(xPreset+3,yPreset+2,xPreset+4,yPreset+30); 
   
	}else{
		myGLCD.setColor(0, 0,255);                              // draw color Blue
		myGLCD.drawLine(xPreset+24, yPreset+2, xPreset+24, yPreset+6);
		myGLCD.drawLine(xPreset+24, yPreset+2, xPreset+20, yPreset+6);
		myGLCD.drawLine(xPreset+24, yPreset+2, xPreset+28, yPreset+6);
    
    myGLCD.drawRect(xPreset+3,yPreset+31,xPreset+10,yPreset+32); 
    myGLCD.drawRect(xPreset+3,yPreset+0,xPreset+25,yPreset+1); 
    myGLCD.drawRect(xPreset+3,yPreset+2,xPreset+4,yPreset+30); 
	}
// HEATER
	if(heaterStatus==0){
		myGLCD.setColor(150, 150,150);                              // draw color Gray
		myGLCD.fillRect(xPreset+13,yPreset+35,xPreset+34,yPreset+37);
	}else{
		myGLCD.setColor(255, 0,0);                              // draw color Red
		myGLCD.fillRect(xPreset+13,yPreset+35,xPreset+34,yPreset+37);		
	}
//Uper level
	if(digitalRead(LEVEL_TOP_PIN) == LOW ){
		myGLCD.setColor(0, 0,0);                              // draw color Black
		myGLCD.fillRect(xPreset+14,yPreset+7,xPreset+33,yPreset+16);
		myGLCD.setColor(0, 0,255);                              // draw color Blue
		myGLCD.fillRect(xPreset+14,yPreset+17,xPreset+33,yPreset+30);
	}else{
		myGLCD.setColor(0, 0,255);                              // draw color Blue
		myGLCD.fillRect(xPreset+14,yPreset+7,xPreset+33,yPreset+30);
	}
}
void seeLog() {
  Serial.println("Log");
  int i;                                                      //counter
  myGLCD.clrScr();                                            // Clear LCD
  myButtons.deleteAllButtons();                               // Clear old buttons
  topic = "log";                                              // Topic

  myGLCD.setFont(SmallFont);                                    // text font
  myGLCD.setBackColor(0, 0, 0);                                 // back color black
  myGLCD.setColor(100, 100, 255);                               // text color Blue
  for (i = 20; i < 320; i = i + 60) {
    myGLCD.drawLine(i, 30, i, 239); // draw vertical grid
  }
  for (i = 40; i < 240; i = i + 20) {
    myGLCD.drawLine(20, i, 319, i); // draw horisontal grid
  }
  myGLCD.setColor(255, 255, 255);                               // text color white
  for (i = 220; i >= 40; i = i - 20) {
    myGLCD.printNumI((240 - i) / 2, 0, i - 6); // print temperature grid
  }
  for (i = 20; i < 320; i = i + 60) {                           // print time grid
    myGLCD.printNumI((i - 20) / 60, i, 225);
    myGLCD.print("h", i + 8, 225);
  }
  for (i = 20; i < 320; i++)   {
    myGLCD.drawPixel(i, 240 - 2 * EEPROM.read(startLog_adr - 20 + i));  //draw temperature log
  }
  while (myTouch.dataAvailable() != true) {
    printHeader();  // waiting touch
  }
  mainmenu();
}
void setup() {
  int i; 
  // Initialisation of inputs/outputs. exept temperature pins
  pinMode(LEVEL_TOP_PIN, INPUT);
  pinMode(HEAT_BOILER_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  myGLCD.InitLCD(LANDSCAPE);              // Initialisation of LCD
  myGLCD.clrScr();
  myGLCD.setBackColor(0, 0, 0);           
  myTouch.InitTouch();                    // initialisation of Touch
  myTouch.setPrecision(PREC_MEDIUM);

  Serial.begin(9600);      Serial.println("Serial.begin(9600)");
  
  t_sensor.begin();                       // start dallas library
  t_sensor.setResolution(9);              // set temperaturesensors resolution 9 bite, 0.5C
  t_sensor.requestTemperatures();         // Send the commands for first recalculation of temp

  byte adrI2C = rwi2c.begin();   Serial.print("I2C device found on 0x");  Serial.println(adrI2C,HEX);
  
  rtc.begin();                            // start Real Time Clock communication
  rtc.setDate(28, 8, 2019);              // set up date. is not nessesary
  
  // ============ Power monitor ==============
  if (rwi2c.get(trace_adr) != 0){
      t = rtc.getTime();
      int last_time_spot=rwi2c.get(last_hour_adr)*60+rwi2c.get(last_min_adr); //last time spot in minutes
      int cur_time_spot=t.hour*60+t.min;

      if (last_time_spot>cur_time_spot){
        nopower_time = cur_time_spot+1440-last_time_spot;
      }else{
        nopower_time = cur_time_spot-last_time_spot;
      }
      
      myGLCD.setFont(BigFont);
      myGLCD.setColor(255, 100, 100);
      myGLCD.print("POWER FAILURE    min", 0, 30); myGLCD.printNumI(nopower_time, 224, 30,3);
      
      switch (  rwi2c.get(trace_adr) ){
          case 1:
              myGLCD.print("during cooling procs", 0, 50);
              
          break;
          case 2:
              myGLCD.print("during boiling procs", 0, 50);
              restore_flag = 2;
          break;
          case 3:
              myGLCD.print("during mashing procs", 0, 50);
              restore_flag = 3;
      }
      
      myGLCD.setColor(255, 255, 255);    
      myGLCD.print("Hold screen for menu", 0, 100);
      myGLCD.print("Automatic renewal of", 0, 150);
      myGLCD.print("the process in   sec", 0, 170);
      for ( i=9 ; i>0 ; i-- ){
          myGLCD.printNumI(i, 240, 170);
          tone(BUZZER_PIN, 523+(10-i)*44, 500);
          delay(1000);  
          if (myTouch.dataAvailable() == true) { mainmenu(); }
          }     

     switch (rwi2c.get(trace_adr)){
          case 1: Cooling(); break;
          case 2: boil(); break;
          case 3: mash(); break;
     }
  }
   
  // Initial EEPROM values in case of SW upload in to new board

  if (EEPROM.read(rNum_adr) > 6)       {
    EEPROM.write(rNum_adr, 3); // number of rests
  }
  if (EEPROM.read(rTemp_startAdr) > 100) {
    EEPROM.write(rTime_startAdr,   15);              //time of rests
    EEPROM.write(rTime_startAdr + 1, 10);
    EEPROM.write(rTime_startAdr + 2, 60);
    EEPROM.write(rTime_startAdr + 3, 20);
    EEPROM.write(rTime_startAdr + 4, 5);
    EEPROM.write(rTime_startAdr + 5, 0);

    EEPROM.write(rTemp_startAdr,   40);             //temperature of rests
    EEPROM.write(rTemp_startAdr + 1, 52);
    EEPROM.write(rTemp_startAdr + 2, 66);
    EEPROM.write(rTemp_startAdr + 3, 71);
    EEPROM.write(rTemp_startAdr + 4, 77);
    EEPROM.write(rTemp_startAdr + 5, 77);
  }
  if (EEPROM.read(hNum_adr) > 6)       {
    EEPROM.write(hNum_adr, 3); // number of hops
  }
  if (EEPROM.read(hTime_startAdr) > 200) {
    EEPROM.write(hTime_startAdr,   0);             //time for hops
    EEPROM.write(hTime_startAdr + 1, 5);
    EEPROM.write(hTime_startAdr + 2, 30);
    EEPROM.write(hTime_startAdr + 3, 60);
    EEPROM.write(hTime_startAdr + 4, 60);
    EEPROM.write(hTime_startAdr + 5, 60);
  }
  if (EEPROM.read(preheatDelta_adr) > 5)  {
    EEPROM.write(preheatDelta_adr, 1); //overheating temperature delta before adding malt
  }

  if (EEPROM.read(cleanTime_adr) > 30) {
    EEPROM.write(cleanTime_adr, 10);
  }
  if (EEPROM.read(cleanTemp_adr)<1 or EEPROM.read(cleanTemp_adr)>10) {
    EEPROM.update(cleanTemp_adr, 9); //target temperature for cleaning devided to 10, so 10 mean 100*C
  }

  if (EEPROM.read(coolTemp_adr)<20 or EEPROM.read(coolTemp_adr)>37) {
    EEPROM.update(coolTemp_adr, 30); //target temperature for cooling
  }
  if (EEPROM.read(desinfTime_adr)<1 or EEPROM.read(desinfTime_adr)>20) {
    EEPROM.update(desinfTime_adr, 5); //target time for desinfection of chiller
  }

  //boilingTemp = 30;          //--------==========  temporary code  ==========-------------
  //EEPROM.update(twiceBoilTemp_adr, round(boilingTemp * 2));

  boilingTemp= EEPROM.read(twiceBoilTemp_adr)*0.5; //-------===========  main code  ============------------
  if (boilingTemp<95 or boilingTemp>105){
       boilingTemp=100;
       EEPROM.update(twiceBoilTemp_adr, round(boilingTemp*2));
   }

  Serial.println("setup routine done");
}
void loop() {
  mainmenu();
}
