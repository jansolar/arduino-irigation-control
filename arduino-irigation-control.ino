
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

#define measureBufferSize 10
#define displayLength 16
#define displayRows 2

#define freezingTemp 0

#define scheduleHour 22
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

  long distanceReadings [measureBufferSize];
  long distanceAverage;
  int lastSecond = 0;
  int arrayIndex = 0;
  int lastState = 0;
  int initCycle = displayLength;
  int currentTemp;
  char statusText;
  int releMode;
  long currentSec;
  long stopSec;
  int tempBlocker = 0;
  int permBlocker = 0;
  int drainer = 0;
  long morningSec=static_cast<long>(morningHour)*3600+morningMinute*60+morningSecond;
  long eveningSec=static_cast<long>(eveningHour)*3600+eveningMinute*60+eveningSecond;
  long scheduleSec=static_cast<long>(scheduleHour)*3600+scheduleMinute*60+scheduleSecond;
  long waterLevel;
  long waterAmount;
  long waterLevelPerc;
  long blockerEnd;

  long minS = 1000;
  long maxS = 0;
  
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
    lcd.print ("Thermo not found");
    Serial.println("Thermo not found");
    Serial.println("Check the connection");
    delay(2000);
  } else {
    lcd.print ("Thermo found! OK.");
    Serial.println("Thermo found");
    delay(500);
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
    if (minS > distanceAverage) {
      minS = distanceAverage;
    }

    if (maxS < distanceAverage) {
      maxS = distanceAverage;
    }

    currentTemp = static_cast<int>(temp.temperature);
    currentSec = static_cast<long>(datumCas.hour) * 3600 + datumCas.minute * 60 + datumCas.second;
    waterLevel = totalWaterDepth - (distanceAverage - zeroLevelDepth);
    waterAmount = static_cast<int> (cmVolume * (waterLevel - minWaterDepth));
    if ( waterAmount < 0 ) { 
      waterAmount = 0;
    }
    waterLevelPerc = static_cast<long> (waterLevel * 100 / totalWaterDepth ) ;
////////////////////////////////////////////    
// Evaluate status
////////////////////////////////////////////    

    if ( waterLevelPerc < forcedDrainPercStop ) {
      drainer = 0;
    }

    if ( waterLevelPerc >= forcedDrainPercStart ) {
      drainer = 1;
    }

    if ( blocker == 1 && currentSec > blockerEnd ) {
      blocker = 0;
      blockerEnd = 0;
    }


// Freezing
    if ( currentTemp < freezingTemp ) {
      statusText = 'F';
      releMode = 0;
      blocker = 1;
      if ( blockerEnd < currentSec + freezerSec) {
        blockerEnd = currentSec + freezerSec ;
      }

// Night
    } else if ( currentSec < morningSec || currentSec >= eveningSec ) {
      statusText = 'N';
      releMode = 0;
      blocker = 0;
      blockerEnd = 0;

// Empty
    } else if ( waterAmount == 0 ) {      
      statusText = 'E';
      releMode = 0;
      blocker = 1;
      if ( blockerEnd < currentSec + freezerSec) {
        blockerEnd = currentSec + freezerSec ;
      }

// Draining
    } else if ( drainer == 1 ) {
      statusText = 'D';
      releMode = 1;

// Watering    
    } else if ( currentSec >= scheduleSec && currentSec < scheduleSec + scheduleLengthSec) {
      statusText = 'W';
      releMode = 1;

// Idle
    } else {
      statusText = 'I';
      releMode = 0;
    }

// BLOCKER    
    if (releMode == 1 && blocker == 1) {
      statusText = 'B';
      releMode = 0;
    }


////////////////////////////////////////////    
// Print on display
////////////////////////////////////////////    

    lcd.clear();
    lcd.setCursor ( 0, 0 );

    if ( currentSec % 6 < 2 ) {
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
      lcd.print (" V=");
      lcd.print (waterAmount);

      
      lcd.print("L");
      lcd.setCursor ( 0, 1 );
      if ( releMode == 0 ) {
        lcd.print("OFF - ");
      } else {
        lcd.print("ON - ");
      }

      switch (statusText) {
        case 'D':
          lcd.print("Draining.");
          break;

        case 'E':
          lcd.print("Empty.");
          break;

        case 'N':
          lcd.print("Night.");
          break;

        case 'F':
          lcd.print("Freezing.");
          break;

        case 'W':
          lcd.print("Watering.");
          break;

        case 'B':
          lcd.print("Blocked.");
          break;

        case 'I':
          lcd.print("Idle.");
          break;

        default:
          lcd.print("???.");
          break;
      }
      
    } else if ( currentSec % 6 < 4 ){
// Print data      
      lcd.print ("CM: ");
      lcd.print(waterLevel);
      lcd.print(" s:");
      lcd.print(distanceAverage);

      lcd.setCursor ( 0, 1 );
      lcd.print ("T=");
       if (currentTemp >= 0) {
         lcd.print ("+");
       } else {
         lcd.print ("-");
       }
      lcd.print(currentTemp); lcd.print("C");
      lcd.print(" ");
      lcd.print(waterLevelPerc);
      lcd.print("%");
       
    } else {
      lcd.print("Min: ");
      lcd.print(totalWaterDepth - (maxS - zeroLevelDepth));
      lcd.print("/");
      lcd.print(maxS);
      lcd.setCursor ( 0, 1 );
      lcd.print("Max: ");
      lcd.print(totalWaterDepth - (minS - zeroLevelDepth));
      lcd.print("/");
      lcd.print(minS);
      
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
