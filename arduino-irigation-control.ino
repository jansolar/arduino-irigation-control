
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

#define scheduleRecords 3
#define eventMaxRecords 20
#define eventTypes 9

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



// Afternoon + waterin
#define minAfternoonHour 15
#define heatThreshold 25
#define heatThresholdAdditionPercent 100

#define daysNoRainThreshold 5
#define daysNoRainThresholdPercent 50
#define initNoRainDaysCM 90

#define waterConservationThresholdCM 50
#define waterCriticalThresholdCM 25


#define freezingTemp 0


#define freezerSec 300

#define scheduleLengthSec 300

#define rainDetectCM 3


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
int scheduleModifier = 0;
int maxAfternoonTemp = -100;
int resetAfternoonTemp = 1;
int addDaysFromRain = 1;
int daysFromRain = -100;
//int waterLevelAfterWatering = -100;
int topEvent;
int maxPriority;
long currentSec;
//long stopSec;
//int tempBlocker = 0;
//int permBlocker = 0;
int drainer = 0;
long morningSec=static_cast<long>(morningHour)*3600+morningMinute*60+morningSecond;
long eveningSec=static_cast<long>(eveningHour)*3600+eveningMinute*60+eveningSecond;
long scheduleSec;
long waterLevel;
long lastWaterLevel=-1;
long waterAmount;
long waterLevelPerc;
long blockerEnd;
long wateringTimeModifier;


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

char degreeChar=223;

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
  scheduleMinute[1] = 0;
  scheduleSecond[1] = 0;
  scheduleLength[1] = 150;
  
  scheduleHour[2] = 6;
  scheduleMinute[2] = 30;
  scheduleSecond[2] = 0;
  scheduleLength[2] = 120;


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

// Event: Rain
  eventType [6]= 'R';
  eventPriority [6] = 45;
  eventValue [6] = 0;

// Event: Watering schedule
  eventType [7]= 'W';
  eventPriority [7] = 40;
  eventValue [7] = 1;

// Event: Idle
  eventType [8]= 'I';
  eventPriority [8] = 30;
  eventValue [8] = 0;

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

// At the Init, if there is not enough water, then assume it did not rain at least for the threshold
    if ( lastWaterLevel < 0  && waterLevel < initNoRainDaysCM ) {
      daysFromRain=daysNoRainThreshold+1;
    }


// At the Init, assume it's one day from rain and set lastWaterLevel
    if ( daysFromRain < 0 ) {
      daysFromRain = 1;
    }

    if ( lastWaterLevel < 0 ) {
      lastWaterLevel = waterLevel;
    }
          

// Stop draining if under limit
    if ( waterLevelPerc < forcedDrainPercStop ) {
      drainer = 0;
    }

// Start draining if over limit
    if ( waterLevelPerc >= forcedDrainPercStart ) {
      drainer = 1;
    }

// End of blocking
    if ( blockerEnd > 0 && currentSec > blockerEnd ) {
      blockerEnd = 0;
    }

// Rain detection
    if (waterLevel < lastWaterLevel ) {
      lastWaterLevel = waterLevel;
    } else if ( waterLevel > lastWaterLevel + rainDetectCM) {
      daysFromRain = 0;
      lastWaterLevel=waterLevel;
    }

// Afternoon (Find max afternoon temp)
    if ( datumCas.hour >= minAfternoonHour && currentSec < eveningSec ) {
      if ( resetAfternoonTemp == 1 ) {
        maxAfternoonTemp = currentTemp;
        resetAfternoonTemp = 0;
      } else if (currentTemp > maxAfternoonTemp) {
        maxAfternoonTemp = currentTemp;
      }
    }

// Calculate watering modifier
    wateringTimeModifier = 100;
    if ( waterLevel < waterCriticalThresholdCM ) {
      wateringTimeModifier=60;
    } else if ( waterLevel > waterConservationThresholdCM ) {    // No modification between conservation and critical values
      if ( daysFromRain > daysNoRainThreshold ) {
        wateringTimeModifier = wateringTimeModifier + daysNoRainThresholdPercent;
      }
      if ( maxAfternoonTemp > heatThreshold ) {
        wateringTimeModifier = wateringTimeModifier + heatThresholdAdditionPercent;
      }
    }

// Midnight (cleanup)
    if ( datumCas.hour == 0 &&  datumCas.minute == 0 ) {
      resetAfternoonTemp = 1;
      if ( addDaysFromRain = 1 ) {
        addDaysFromRain = 0;
        daysFromRain++;
      }
    } else {
      addDaysFromRain = 1;
    }

// Night 
    if ( currentSec < morningSec || currentSec >= eveningSec ) {
      setEvent ('N');
      blockerEnd = 0;
    }


// Freezing
    if ( currentTemp < freezingTemp ) {
      setEvent('F');
      setBlocker (freezerSec);
    }


// Empty
    if ( waterAmount == 0 ) {
      setEvent ('E');
      setBlocker (freezerSec);
    }

// Draining
    if ( drainer == 1 ) {
      setEvent('D');
      daysFromRain = 0; // Assume it rained
    }


// Rain detected
    if ( daysFromRain == 0 ) {
      setEvent('R');
    }

// Watering    
    for (int i = 0; i < scheduleRecords; i++) {
       scheduleSec=scheduleHour[i]*3600+scheduleMinute[i]*60+scheduleSecond[i];
       if ( currentSec >= scheduleSec && currentSec < scheduleSec + scheduleLength[i]*wateringTimeModifier/100) {
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
  maxPriority = 0;
  for (int i = 0; i < eventCounter; i++) {
    if (eventBufferPriority[i] > maxPriority) {
      topEvent = i;
      maxPriority = eventBufferPriority[i];
    }
  }



////////////////////////////////////////////    
// Print on display
////////////////////////////////////////////    

//    lcd.clear();
    lcd.setCursor ( 0, 0 );


// Print status - First line - always the same

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


    if (currentTemp >= 0) {
      lcd.print (" +");
    } else {
      lcd.print (" -");
    }
    lcd.print(currentTemp); 
    lcd.print(degreeChar);
    lcd.print("C ");

    lcd.setCursor ( 15, 0 );
    lcd.print(eventBufferType[topEvent]);
    

// Print status - Second line - Fixed part

    lcd.setCursor ( 0, 1 );
    lcd.print(waterLevel);
    lcd.print ("cm  ");      
    lcd.setCursor ( 6, 1 );
    lcd.print ("- ");

// Print status - Second line - variable part
    
    if ( currentSec % 10 < 2 ) {
      lcd.print(totalWaterDepth - (maxSAvg - zeroLevelDepth));
      lcd.print ("..");
      lcd.print(totalWaterDepth - (minSAvg - zeroLevelDepth));
      lcd.print("      ");
    } else if ( currentSec % 10 < 4 ) {
      lcd.print ("V=");
      lcd.print (waterAmount);
      lcd.print ("Lt  ");
    } else if ( currentSec % 10 < 6 ) {
      lcd.print (waterLevelPerc);
      lcd.print ("% Vol.");
    } else if ( currentSec % 10 < 8 ) {
      lcd.print ("Max");
      if (maxAfternoonTemp >= 0) {
        lcd.print ("+");
      } else {
        lcd.print ("-");
      }
      if ( maxAfternoonTemp == -100 ) {
        lcd.print("??");
      } else {
        lcd.print (maxAfternoonTemp);
      }
      lcd.print(degreeChar);
      lcd.print("C ");
    } else {
      lcd.print("Rain+");
      lcd.print(daysFromRain);
      lcd.print("   ");
    }

////////////////////////////////////////////    
// Set the rele
////////////////////////////////////////////    

    if ( eventBufferValue [topEvent] == 0 ) {
      digitalWrite(pinRele,HIGH);  // WATERING OFF
    } else {
      digitalWrite(pinRele,LOW);   // WATERING ON
      lastWaterLevel = waterLevel;
    }
  }
  
  delay(sonarDelay);

  arrayIndex++;
  
  if (arrayIndex==measureBufferSize) {
    arrayIndex = 0;
  }

}
