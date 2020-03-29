/*
 * INTENTIONAL CLOCK by Marcela Mancino (@mardefronteira)
 * 
 * Developed for the Tangible Interaction and Device course
 * taught by Ben Light and Tom Igoe
 * at NYU - ITP 
 * 
 * Spring 2020
 */


// libraries
#include <Encoder.h>
#include <RTCZero.h>
#include <Adafruit_GFX.h>
#include<Adafruit_SSD1306.h>

// pins constants
const int buttonPin = 8;
const int tiltPin = 12;
const int encoderPushPin = 4;
const int speakerPin = 5;
//const int speakerPin2 = 11;


const int displayResetPin = 7;

// these pins have to support PWM
const int rPin = 9;
const int gPin = 6;
const int bPin = 10;

// display constants
const int displayWidth = 128;
const int displayHeight = 64;
Adafruit_SSD1306 display(displayWidth, displayHeight, &Wire, displayResetPin);

// gates
bool buttonGate = true;
bool encoderGate = true;
bool screenGate = true;
bool alarmGate = true;

// clock controls
bool setHourMode = false;
bool setMinuteMode = false;
bool setAlarmMode = false;

bool screenIsOn = false;

int mode = 0;

int restingTime = 0;
int screenTolerance = 10000;

// encoder variables
Encoder encoder(3,2);
int lastEnc = 0;
int encDirection = 0;

int encTime = 0;
int encVal = 255;

// clock variables
RTCZero rtc;
int hourNow = 0;
int minNow = 0;
int secNow = 0;

// alarm variables
int startTime[3];
int alarmDuration = 2;
bool alarmIsOn = false;
bool soundIsOn = false;

// speaker variables
int noteWait = 0;
int lastNote = 0;

// color variables
int rgb[3];

void setup() {
  // begin serial communication
  Serial.begin(9600);

  // set up clock
  rtc.begin();

  rtc.setHours(0);
  rtc.setMinutes(0);
  rtc.setSeconds(0);
  
  // set pin modes
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(tiltPin, INPUT_PULLUP);
  pinMode(encoderPushPin, INPUT_PULLUP);
  pinMode(speakerPin, OUTPUT);

  pinMode(rPin, OUTPUT);
  pinMode(gPin, OUTPUT);
  pinMode(bPin, OUTPUT);

// what is this?========================================================================
// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
}
void testdrawstyles(void) {
  display.clearDisplay();

  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(F("Hello, world!"));

  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
  display.println(3.141592);

  display.setTextSize(2);             // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.print(F("0x")); display.println(0xDEADBEEF, HEX);

  display.display();
  delay(2000);
}

void loop() {
  
  // debounce gets rid of hardware imprecisions during the click
  // gate makes sure the command is sent only once per click, since
  // the arduino keeps receiving HIGH values on the pin
  if (debounceInputUp(buttonPin)) {
    if (buttonGate == true) {

      // turn the alarm on/off if it is not buzzing
      if (soundIsOn){
        alarmIsOn = true;
      } else {
        alarmIsOn = !alarmIsOn;
      }

      Serial.print("the button has been pressed, alarm state is ");
      Serial.println(alarmIsOn);
      buttonGate = false;
    }
  } else {
    buttonGate = true;
  }

  // if device is being held (if tilt sensor is active) screen is on,
  // otherwise wait the tolerance time and turn off
  if (debounceInput(tiltPin)) {
    Serial.println("the device has been tilted");
    screenIsOn = true;

    
    if(soundIsOn) {
      soundIsOn = false;
      alarmIsOn = false;
    }

  } else {
    if(screenIsOn){
      if (screenGate) {
        restingTime = millis();
        screenGate = false;
      }
      if (millis() > restingTime + screenTolerance) {
        screenIsOn = false;
        screenGate = true;
      }
    }
  }

  // if encoder button is pressed, change the clock mode
  // debounce gets rid of hardware imprecisions during the click
  // gate makes sure the command is sent only once per click, since
  // the arduino keeps receiving HIGH values on the pin
  if (debounceInputUp(encoderPushPin)) {
    Serial.println("aqui");
    if (encoderGate == true) {
      /* alternate between 4 modes:
       *  0 = no action
       *  1 = set alarm time
       *  2 = set hour
       *  3 = set minute
       */
      if (mode < 3){
        mode++;
      } else {
        mode = 0;
      }

      // reset encoder
      encoder.write(0);
      
      encoderGate = false;
  
      // for visual feedback while testing the system
      Serial.print("the encoder has been pressed, the mode is now ");
      Serial.println(mode);
//      ledColor(255,0,0);
    }
  } else {
    encoderGate = true;
  }

// make this local
  encVal = encoder.read();
  
if (lastEnc != encVal) {
   Serial.print("the encoder has been twisted! ");
   Serial.println(encDirection);
   encDirection = encVal-lastEnc;
   encDirection = encDirection/abs(encDirection);
   lastEnc = encVal;
} else {
 encDirection = 0;
}

  

  // screen control
  if (screenIsOn) {
    mainDisplay();
  } else {
    display.clearDisplay();
    display.display();
  }

  // get current time
  hourNow = rtc.getHours();
  minNow = rtc.getMinutes();
  secNow = rtc.getSeconds();


  // set what the encoder is doing based on the mode variable
  modeAction();

  // set LED color based on time or alarm
  setLedColor();

  // check if the alarm is on and set it, or else stop the sound
  if (alarmIsOn) {
    startAlarm();
  } else {
    soundIsOn = false;
    alarmGate = true;
  }


  // ====================================== END OF LOOP
}

void modeAction() {
  if (mode != 0) {
    screenIsOn = true;
  }
  if (mode == 1) {
    // set alarm duration
    if (alarmDuration + encDirection < 5) {
      alarmDuration = 12;
    } else if (alarmDuration + encDirection > 12) {
      alarmDuration = 5;
    } else {
      alarmDuration += encDirection;
    }
    
  } else if (mode == 2) {
    // set hours
    int newHour;
    if (hourNow + encDirection > 23) {
      newHour = 0;
    } else if (hourNow + encDirection < 0) {
      newHour = 23;
    } else {
      newHour = hourNow + encDirection;
    }
    rtc.setHours(newHour);
    
  } else if (mode == 3) {

    // set minutes
    int newMin;
    if (minNow + encDirection > 59) {
      newMin = 0;
    } else if (minNow + encDirection < 0) {
      newMin = 59;
    } else {
      newMin = minNow + encDirection;
    }
    rtc.setMinutes(newMin);
    rtc.setSeconds(0);
  }
}

// display setup
void mainDisplay(){
  display.clearDisplay();

  // display hours
  if (mode == 2) {
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  } else {
  display.setTextColor(SSD1306_WHITE);
  }
  display.setTextSize(4);
  display.setCursor(6,5);
  if (hourNow < 10) {
     display.print(0);
  }
  display.print(hourNow);

  // display :
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(55,5);
  display.print(F(":"));

  // display minutes
  if (mode == 3) {
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  }
   if (minNow < 10) {
     display.print(0);
  }
  display.println(minNow);

  //display alarm
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,48);
  display.print(F("alarm duration: "));
  display.setCursor(80,57);
   if (mode == 1) {
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  }
  if(alarmDuration < 10) {
    display.print(0);
  }
  display.print(alarmDuration);
  display.setTextColor(SSD1306_WHITE);
  display.println(F(" hours"));

  display.display();
}

// alarm set up
void startAlarm() {
  // record the time that the button was clicked
  if (alarmGate) {
    startTime[0] = hourNow;
    startTime[1] = minNow;
    startTime[2] = secNow;
    alarmGate = false;
  }

  // check if the alarm has run for the whole duration
  
  if (hourNow >= startTime[0] + alarmDuration && minNow >= startTime[1] && secNow >= startTime[2]) {
//if (hourNow >= startTime[0] + alarmDuration && minNow >= startTime[1] && secNow >= startTime[2]) {
    soundIsOn = true;
  }

  if (soundIsOn) {
//    Serial.println("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

    playNote(262, 100, 200);
    playNote(392, 200, 100);
    playNote(330,100,100);
    playNote(349,50,50);
    playNote(262,100,300);
    playNote(392,100,300);
    playNote(330,200,200);
    playNote(494,500,300);
    playNote(330,100,200);
    playNote(330,100,200);
    playNote(494,100,200);
    playNote(494,100,200);
    playNote(392,70,50);
    playNote(392,70,30);
    playNote(392,70,30);
  }
}

void playNote(int freq, int dur, int dTime) {
 if (millis() > lastNote + noteWait) {
   tone(speakerPin, freq, dur);
//   tone(speakerPin2, freq, dur);
   noteWait = dTime;
   lastNote = millis();
 } else {
   playNote(freq, dur, dTime);
 }
}

// debouncing
bool debounceInput(int pinToTest) {
  bool result = false;
  // if given pin is set to high, wait a few milliseconds and check again
  // if it is still high, pass on the result
  if (digitalRead(pinToTest) == HIGH) {
    delay(4);
    if (digitalRead(pinToTest) == HIGH) {
      result = true;
    } else {
      result = false;
    }
  }
  return result;
}
bool debounceInputUp(int pinToTest) {
  bool result = false;
  // if given pin is set to high, wait a few milliseconds and check again
  // if it is still high, pass on the result
  if (digitalRead(pinToTest) == LOW) {
    delay(4);
    if (digitalRead(pinToTest) == LOW) {
      result = true;
    } else {
      result = false;
    }
  }
  return result;
}

// RGB LED light control

// Light changes from red (midnight) to white (6h) to yellow (12h) to amber (18). When alarm is on, it is blue.
void setLedColor() {
  int mult;
  if (alarmIsOn) {
    hsi2rgb(200,0.5,0.5,rgb);
  } else {
    // white to yellow
    if (hourNow >= 6 && hourNow <= 12) {
      hsi2rgb(60, 0.16*hourNow, 1, rgb);
    }
    // yellow to amber to red
    if (hourNow > 12 && hourNow <= 23) {
      mult = (int)(hourNow-6)*5;
      hsi2rgb(60-mult, 1, 1, rgb);      
    }  
    // red to white
    if (hourNow >= 0 && hourNow <= 5) {
      mult = map(hourNow, 19, 23, 1, 5);
      hsi2rgb(0, 1-(0.16*mult), 1, rgb);    
    }
  }
   ledColor(rgb[0],rgb[1],rgb[2]);
}


void ledColor (int rVal, int gVal, int bVal) {
  analogWrite(rPin, rVal);
  analogWrite(gPin, gVal);
  analogWrite(bPin, bVal);
}

/*
 * The code below is an RGB to HSI converter, and was taken from this link:
 * https://blog.saikoled.com/post/43693602826/why-every-led-light-should-be-using-hsi
 */
 
// Function example takes H, S, I, and a pointer to the 
// returned RGB colorspace converted vector. It should
// be initialized with:
//
// int rgb[3];
//
// in the calling function. After calling hsi2rgb
// the vector rgb will contain red, green, and blue
// calculated values.

void hsi2rgb(float H, float S, float I, int* rgb) {
  int r, g, b;
  H = fmod(H,360); // cycle H around to 0-360 degrees
  H = 3.14159*H/(float)180; // Convert to radians.
  S = S>0?(S<1?S:1):0; // clamp S and I to interval [0,1]
  I = I>0?(I<1?I:1):0;
    
  // Math! Thanks in part to Kyle Miller.
  if(H < 2.09439) {
    r = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    g = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    b = 255*I/3*(1-S);
  } else if(H < 4.188787) {
    H = H - 2.09439;
    g = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    b = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    r = 255*I/3*(1-S);
  } else {
    H = H - 4.188787;
    b = 255*I/3*(1+S*cos(H)/cos(1.047196667-H));
    r = 255*I/3*(1+S*(1-cos(H)/cos(1.047196667-H)));
    g = 255*I/3*(1-S);
  }
  
  /* 
   *  these values were edited from source because my LEDs are common-anode,
   *  and the values sent to them are opposite from the ones in the LEDs used
   *  in the reference. 
   *  this likely messed up with the intensity calculation, so I would recommend
   *  correcting the math up there, or leaving these just as rgb[0] = r; etc
   *  [marcela mancino]
   */
  rgb[0]=255-r;
  rgb[1]=255-g;
  rgb[2]=255-b;
}
