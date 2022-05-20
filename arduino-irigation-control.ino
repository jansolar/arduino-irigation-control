
#include <NewPing.h>
#include <Wire.h>
#include <DS3231.h>
#include <LiquidCrystal_I2C.h>
#include "Adafruit_SHT4x.h"
 


// Define Sonar pins
#define pinTrigger    3
#define pinEcho       4
#define pinRele       7

// Technical config
#define measureBufferSize 32
#define sonarDelay 100
#define ignoredBoundaryValues 6
#define displayLength 16
#define displayRows 2
#define maxVzdalenost 450
#define errorStateMarginCM 10 //Unused so far

#define scheduleRecords 2
#define eventMaxRecords 20
#define eventTypes 8

// Water storager parameters
#define zeroLevelDepth 35     // Vzdalenost sonaru od Max hladiny
#define totalWaterDepth 140
#define minWaterDepth 6
#define cmVolume 31.4159

// Water draining parameters
#define forcedDrainPercStart 90
#define forcedDrainPercStop 80

// Night
#define morningHour 6
#define morningMinute 0
#define morningSecond 0
#define eveningHour 23
#define eveningMinute 0
#define eveningSecond 0

#define freezingTemp 0


#define freezerSec 300

#define scheduleLengthSec 300


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
long sortedDistanceReadings [measureBufferSize];
long tempValue;
long distanceAverage;
int lastSecond = 0;
int arrayIndex = 0;
int lastState = 0;
int initCycle = displayLength;
int currentTemp;
//char statusText;
//int releMode;
int eventCounter = 0;
int topEvent;
long currentSec;
//long stopSec;
//int tempBlocker = 0;
//int permBlocker = 0;
int drainer = 0;
long morningSec=static_cast<long>(morningHour)*3600+morningMinute*60+morningSecond;
long eveningSec=static_cast<long>(eveningHour)*3600+eveningMinute*60+eveningSecond;
long scheduleSec;
long waterLevel;
long waterAmount;
long waterLevelPerc;
long blockerEnd;

long minS = 1000;
long maxS = 0;

long minSAvg = 1000;
long maxSAvg = 0;

// Irigation scheduler
long scheduleHour [scheduleRecords];
long scheduleMinute [scheduleRecords];
long scheduleSecond [scheduleRecords];
long scheduleLength [scheduleRecords];

// Event buffer
char eventBufferType [eventMaxRecords];
int  eventBufferPriority [eventMaxRecords];
int  eventBufferValue [eventMaxRecords];

// Events buffer
char eventType [eventMaxRecords];
int  eventPriority [eventMaxRecords];
int  eventValue [eventMaxRecords];

Adafruit_SHT4x sht4 = Adafruit_SHT4x();


void setEvent(char event){
  for (int i = 0; i < eventTypes; i++) {
    if (eventType[i] == event) {
      eventBufferType[eventCounter] = event;
      eventBufferPriority [eventCounter] = eventPriority[i];
      eventBufferValue [eventCounter] = eventValue[i];
      eventCounter++;
    }
  }
}


void setBlocker(int blockerSec){
  if ( blockerEnd < currentSec + blockerSec) {
    blockerEnd = currentSec + blockerSec ;
  }
}




void setup() {

// Irigation schedule
  scheduleHour[0] = 22;
  scheduleMinute[0] = 15;
  scheduleSecond[0] = 0;
  scheduleLength[0] = 150;

  scheduleHour[1] = 22;
  scheduleMinute[1] = 00;
  scheduleSecond[1] = 0;
  scheduleLength[1] = 150;
  

// Event definitions


// Event: Empty
  eventType [0]= 'E';
  eventPriority [0] = 110;
  eventValue [0] = 0;

// Event: Freezing
  eventType [1]= 'F';
  eventPriority [1] = 100;
  eventValue [1] = 0;

// Event: Night
  eventType [2]= 'N';
  eventPriority [2] = 90;
  eventValue [2] = 0;

// Event: Blocker
  eventType [3]= 'B';
  eventPriority [3] = 80;
  eventValue [3] = 0;

// Event: Manual
  eventType [4]= 'M';
  eventPriority [4] = 60;
  eventValue [4] = 1;

// Event: Drainer
  eventType [5]= 'D';
  eventPriority [5] = 50;
  eventValue [5] = 1;

// Event: Watering schedule
  eventType [6]= 'W';
  eventPriority [6] = 40;
  eventValue [6] = 1;

// Event: Idle
  eventType [7]= 'I';
  eventPriority [7] = 30;
  eventValue [7] = 0;

// Set default event Idle
  setEvent ('I');
  
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

  // Setup Digital pin output of RELAY
  pinMode(pinRele,OUTPUT); 

  // Turn off RELAY
  digitalWrite(pinRele,HIGH);
  
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
    delay(sonarDelay);
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
    eventCounter = 1;
    for (int i = 0; i < measureBufferSize; i++) {
      sortedDistanceReadings[i] = distanceReadings [i];
    }

    for (int i = 1; i < measureBufferSize; i++) {
      for (int j = 1; j < measureBufferSize-i; j++) {
        if (sortedDistanceReadings[j] > sortedDistanceReadings[j+1]) {
          tempValue = sortedDistanceReadings[j];
          sortedDistanceReadings[j] = sortedDistanceReadings[j+1];
          sortedDistanceReadings[j+1]=tempValue;
        }
      }
    }
    
    for (int i = ignoredBoundaryValues; i < measureBufferSize-ignoredBoundaryValues; i++) {
      distanceAverage += sortedDistanceReadings [i];
    }
    distanceAverage = distanceAverage / (measureBufferSize - 2 * ignoredBoundaryValues);
    if (minS > distanceReadings [arrayIndex]) {
      minS = distanceReadings [arrayIndex];
    }

    if (maxS < distanceReadings [arrayIndex]) {
      maxS = distanceReadings [arrayIndex];
    }

    if (minSAvg > distanceAverage) {
      minSAvg = distanceAverage;
    }

    if (maxSAvg < distanceAverage) {
      maxSAvg = distanceAverage;
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

    if ( blockerEnd > 0 && currentSec > blockerEnd ) {
      blockerEnd = 0;
    }


// Freezing
    if ( currentTemp < freezingTemp ) {
      setEvent('F');
      setBlocker (freezerSec);
    }

// Night (clears blockers)
    if ( currentSec < morningSec || currentSec >= eveningSec ) {
      setEvent ('N');
      blockerEnd = 0;
    }

// Empty
    if ( waterAmount == 0 ) {
      setEvent ('E');
      setBlocker (freezerSec);
    }

// Draining
    if ( drainer == 1 ) {
      setEvent('D');
    }

// Watering    
    for (int i = 0; i < scheduleRecords; i++) {
       scheduleSec=scheduleHour[i]*3600+scheduleMinute[i]*60+scheduleSecond[i];
       if ( currentSec >= scheduleSec && currentSec < scheduleSec + scheduleLength [i]) {
         setEvent ('W');
       }
    }

// BLOCKER    
    if (blockerEnd > 0) {
      setEvent('B');
    }

// Idle - is set by default


////////////////////////////////////////////    
// Find the event with the highest priority
////////////////////////////////////////////    

  for (int i = 0; i < eventCounter; i++) {
    int maxPriority = 0;
    if (eventBufferPriority[i] > maxPriority) {
      topEvent = i;
      maxPriority = eventBufferPriority[i];
    }
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
      if ( eventBufferValue [topEvent] == 0 ) {
        lcd.print("OFF - ");
      } else {
        lcd.print("ON - ");
      }

      switch (eventBufferType[topEvent]) {
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
      lcd.print(maxSAvg);
      lcd.print("/");
      lcd.print(maxS);
      lcd.setCursor ( 0, 1 );
      lcd.print("Max: ");
      lcd.print(totalWaterDepth - (minS - zeroLevelDepth));
      lcd.print("/");
      lcd.print(minSAvg);
      lcd.print("/");
      lcd.print(minS);
      
    }

////////////////////////////////////////////    
// Set the rele
////////////////////////////////////////////    

    if ( eventBufferValue [topEvent] == 0 ) {
      digitalWrite(pinRele,HIGH);
    } else {
      digitalWrite(pinRele,LOW);
    }
  }
  
  delay(sonarDelay);

  arrayIndex++;
  
  if (arrayIndex==measureBufferSize) {
    arrayIndex = 0;
  }

}
