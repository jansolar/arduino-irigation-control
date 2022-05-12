// Ultrazvukový modul HY-SRF05 pro měření vzdálenosti

// připojení potřebné knihovny
#include <NewPing.h>
#include <Wire.h>
#include <DS3231.h>

#include <LiquidCrystal_I2C.h>
#include "Adafruit_SHT4x.h"
 


// nastavení propojovacích pinů
#define pinTrigger    3
#define pinEcho       4
#define maxVzdalenost 450

// inicializace měřícího modulu z knihovny PING
NewPing sonar(pinTrigger, pinEcho, maxVzdalenost);

// nastavení adresy I2C (0x27 v mém případě),
// a dále počtu znaků a řádků LCD, zde 20x4
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// inicializace RTC z knihovny
DS3231 rtc;
// vytvoření proměnné pro práci s časem
RTCDateTime datumCas;

  int distanceReadings [10] = {0,0,0,0,0,0,0,0,0,0};
  int distanceAverage;
  int lastSecond = 0;
  int arrayIndex = 0;
  int lastState = 0;
  int currentTemp;


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

  
  lcd.print("Starting.");
  lcd.setCursor ( 0, 1 );
  for (int i = 0; i < 16; i++) {
    lcd.print(".");
    if ( i< 10 ) {
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
    for (int i = 0; i < 10; i++) {
      distanceAverage += distanceReadings [i];
    }
    distanceAverage = distanceAverage / 10;

// PRINT DISTANCE

    lcd.setCursor ( 0, 0 );
    lcd.print ("CM: ");
    if (distanceAverage > 0) {
      lcd.print(distanceAverage);
      lcd.print("  ");
      lcd.setCursor ( 8, 0 );
      if (distanceAverage < 50 ) {
        digitalWrite(7,HIGH);
        lcd.print("- X -");
      } else {
        digitalWrite(7,LOW);
        lcd.print("-----");
      }
    }  else {
      lcd.print ("???");
    }
    lcd.print("  ");
    //lcd.print(arrayIndex);
// PRINT TIME

    lcd.setCursor ( 0, 1 );
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
     currentTemp = static_cast<int>(temp.temperature);
     lcd.print(" T:"); 
     if (currentTemp > 0) {
       lcd.print ("+");
     } else {
       lcd.print ("-");
     }
     lcd.print(currentTemp); lcd.print("C");

  }

  
  delay(50);

  arrayIndex++;
  
  if (arrayIndex==10) {
    arrayIndex = 0;
  }

}
