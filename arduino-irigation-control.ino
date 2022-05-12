
#include <NewPing.h>
#include <Wire.h>
#include <DS3231.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_SHT4x.h"
 


// Define Sonar pins
#define pinTrigger    3
#define pinEcho       4
#define pinRele       7

// Define water levels
#define maxVzdalenost 450

// Vzdalenost sonaru od Max hladiny
#define zeroLevelDepth 95
#define totalWaterDepth 135
#define minWaterDepth 6
#define cmVolume 31.4159
#define forcedDrainPercStart 95
#define forcedDrainPercStop 90
#define errorStateMarginCM 10
#define morningHour 6
#define morningMinute 0
#define morningSecond 0
#define eveningHour 23
#define eveningMinute 0
#define eveningSecond 0
#define freezingTemp 0
#define measureBufferSize 10
#define displayLength 16
#define displayRows 2

#define scheduleHour 6
#define scheduleMinute 15
#define scheduleSecond 0

#define freezerSec 300

#define scheduleLengthSec 120


// inicializace měřícího modulu z knihovny PING
NewPing sonar(pinTrigger, pinEcho, maxVzdalenost);

// nastavení adresy I2C (0x27 v mém případě),
// a dále počtu znaků a řádků LCD, zde 20x4
LiquidCrystal_I2C lcd(0x3F, displayLength, displayRows);

// inicializace RTC z knihovny
DS3231 rtc;
// vytvoření proměnné pro práci s časem
RTCDateTime datumCas;

  int distanceReadings [measureBufferSize];
  int distanceAverage;
  int lastSecond = 0;
  int arrayIndex = 0;
  int lastState = 0;
  int initCycle = displayLength;
  int currentTemp;
  String statusText;
  int releMode;
  int currentSec;
  int stopSec;
  int tempBlocker = 0;
  int permBlocker = 0;
  int drainer = 0;
  int morningSec=morningHour*3600+morningMinute*60+morningSecond;
  int eveningSec=eveningHour*3600+eveningMinute*60+eveningSecond;
  int scheduleSec=scheduleHour*3600+scheduleMinute*60+scheduleSecond;
  int waterLevel;
  int waterAmount;
  int waterLevelPerc;
  int blockerEnd;
  int blocker;

Adafruit_SHT4x sht4 = Adafruit_SHT4x();


void setup() {
  // zahájení komunikace po sériové lince
  // rychlostí 9600 baud
  Serial.begin(9600);
  // zahájení komunikace s RTC obvodem
  rtc.begin();
  // nastavení času v RTC podle času kompilace programu,
  // stačí nahrát jednou
  //rtc.setDateTime(__DATE__, __TIME__);
  // přímé nastavení času pro RTC
  //rtc.setDateTime(__DATE__, "12:34:56");

  // Setup Digital pin 7 output of RELAY
  pinMode(7,OUTPUT); 

  // inicializace LCD
  lcd.begin();
  lcd.backlight();

  // inicializace THERMO
  lcd.setCursor ( 0, 0 );  
  if (! sht4.begin()) 
  {
    lcd.print ("SHT4x not found");
    Serial.println("SHT4x not found");
    Serial.println("Check the connection");
    delay(1000);
  } else {
    lcd.print ("SHT4x found! OK.");
    Serial.println("SHT4x found");
    delay(1000);
  }
  lcd.clear();

  if (initCycle < measureBufferSize) {
      initCycle = measureBufferSize;
  }

  for (int i = 0; i < initCycle; i++) {
    if (i % displayLength == 0) {
      lcd.clear();
      lcd.print("Starting.");
      lcd.setCursor ( 0, 1 );
    }
    lcd.print(".");
    if ( i < measureBufferSize ) {
      distanceReadings [i] = sonar.ping_cm();
    }
    delay(50);
  }
  lcd.clear();
  // nastavení kurzoru na první znak, druhý řádek
  // veškeré číslování je od nuly, poslední znak je tedy 19, 3


  
  sht4.setPrecision(SHT4X_HIGH_PRECISION); // highest resolution
  sht4.setHeater(SHT4X_NO_HEATER); // no heater
 
}


void loop() {
  sensors_event_t humidity, temp; // temperature and humidity variables
  sht4.getEvent(&humidity, &temp);

  // načtení vzdálenosti v centimetrech do vytvořené proměnné vzdalenost
  distanceReadings [arrayIndex] = sonar.ping_cm();
  datumCas = rtc.getDateTime();
  if (datumCas.second != lastSecond) {
    lastSecond=datumCas.second;
    distanceAverage = 0;
    for (int i = 0; i < measureBufferSize; i++) {
      distanceAverage += distanceReadings [i];
    }
    distanceAverage = distanceAverage / measureBufferSize;
    currentTemp = static_cast<int>(temp.temperature);
    currentSec = datumCas.hour * 3600 + datumCas.minute * 60 + datumCas.second;
    waterLevel = totalWaterDepth - (distanceAverage - zeroLevelDepth);
    waterAmount = static_cast<int> (cmVolume * (waterLevel - minWaterDepth));
    if ( waterAmount < 0 ) { 
      waterAmount = 0;
    }
    waterLevelPerc = static_cast<int> (waterLevel / totalWaterDepth * 100) ;
////////////////////////////////////////////    
// Evaluate status
////////////////////////////////////////////    

    if ( waterLevel < totalWaterDepth * forcedDrainPercStop / 100 ) {
      drainer = 0;
    }

    if ( waterLevel >= totalWaterDepth * forcedDrainPercStart / 100 ) {
      drainer = 1;
    }

    if ( blocker == 1 && currentSec > blockerEnd ) {
      blocker = 0;
      blockerEnd = 0;
    }


// Freezing
    if ( currentTemp < freezingTemp ) {
      statusText = 'OFF - Freezing!';
      releMode = 0;
      blocker = 1;
      if ( blockerEnd < currentSec + freezerSec) {
        blockerEnd = currentSec + freezerSec ;
      }

// Night
    } else if ( currentSec < morningSec || currentSec >= eveningSec ) {
      statusText = 'OFF - Night!';
      releMode = 0;
      blocker = 0;
      blockerEnd = 0;

// Empty
    } else if ( waterAmount = 0 ) {      
      statusText = 'OFF - Empty!';
      releMode = 0;
      blocker = 1;
      if ( blockerEnd < currentSec + freezerSec) {
        blockerEnd = currentSec + freezerSec ;
      }

// Draining
    } else if ( drainer == 1 ) {
      statusText = 'ON - Draining!';
      releMode = 1;

// Watering    
    } else if ( currentSec >= scheduleSec && scheduleSec < scheduleSec + scheduleLengthSec) {
      statusText = 'ON - Watering!';
      releMode = 1;

// Idle
    } else {
      statusText = 'OFF - idle.';
      releMode = 0;
    }

// BLOCKER    
    if (releMode == 1 && blocker == 1) {
      statusText = 'OFF - blocked.';
      releMode = 0;
    }


////////////////////////////////////////////    
// Print on display
////////////////////////////////////////////    

    lcd.clear();
    lcd.setCursor ( 0, 0 );

    if ( currentSec % 4 < 2 ) {
// Print status
      if (datumCas.hour < 10) {   
        lcd.print("0");
      }
      lcd.print(datumCas.hour);   lcd.print(":");
      if (datumCas.minute < 10) {   
        lcd.print("0");
      }
      lcd.print(datumCas.minute); lcd.print(":");
      if (datumCas.second < 10) {   
        lcd.print("0");
      }
      lcd.print(datumCas.second);
      lcd.setCursor ( 0, 1 );
      lcd.print(statusText);
    } else {
// Print data      
      lcd.print ("CM: ");
      lcd.print(waterLevel);
      lcd.print(" s:");
      lcd.print(distanceAverage);
      lcd.print(" ");
      lcd.print(waterLevelPerc);
      lcd.print("%");

      lcd.setCursor ( 0, 1 );
      lcd.print ("V=");
      lcd.print (waterAmount);
      lcd.print ("l  Temp=");
       if (currentTemp >= 0) {
         lcd.print ("+");
       } else {
         lcd.print ("-");
       }
       lcd.print(currentTemp); lcd.print("C");
    }

////////////////////////////////////////////    
// Set the rele
////////////////////////////////////////////    

    if ( releMode == 0 ) {
      digitalWrite(pinRele,HIGH);
    } else {
      digitalWrite(pinRele,LOW);
    }
  }
  
  delay(50);

  arrayIndex++;
  
  if (arrayIndex==measureBufferSize) {
    arrayIndex = 0;
  }

}
