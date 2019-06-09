#include <bluefruit.h>
#include <stdlib.h>

#define BUZZER_PIN                12
#define CONNECTED_LED_PIN         15
#define SPEED_LED_PIN             27
#define BUTTON_PIN                16
#define HALL_EFFECT_SENSOR_PIN    25

BLEDis bledis;
BLEHidAdafruit blehid;

#define BUTTON_NOT_PRESSED        0u
#define BUTTON_QUICK_PRESS        1u
#define BUTTON_HOLD_PRESS         2u
#define BUTTON_HOLD_NOTIFICATION  3u

#define QUICK_PRESS_MS                 100u
#define LONG_PRESS_MS                  1000u
#define BUTTON_HOLD_NOTIF_TIME_MS      150u
int button_pressed = BUTTON_NOT_PRESSED;

#define SPEED_VAL_ONE_MS           1000u
#define SPEED_VAL_TWO_MS           800u
#define SPEED_VAL_THREE_MS         700u
#define SPEED_VALUE_FOUR_MS        600u
#define SPEED_VALUE_FIVE_MS        500u
#define NUM_OF_SPEEDS              5u

const int speed_array_ms[] = {SPEED_VAL_ONE_MS,
                              SPEED_VAL_TWO_MS,
                              SPEED_VAL_THREE_MS,
                              SPEED_VALUE_FOUR_MS,
                              SPEED_VALUE_FIVE_MS};

int current_speed = 0;



#define HALL_EFFECT_DEBOUNCE 1u

#define ROT_WARN_UP_COUNT   1
#define ROT_WARN_DN_COUNT   10

#define ROT_WARN_MAX        50
#define ROT_WARN_MIN        -510

#define FIRST_WARNING       -200
#define SECOND_WARNING      -400
#define BOOT_OUT            -500

#define WARNING_NO_WARN       0u
#define WARNING_SOUND_ONLY    1u
#define WARNING_EXIT_WINDOW   2u

bool firstWarningActive = false;
bool secondWarningActive = false;
bool exerciseActive = false;

int warningCount = 0;
int warning = WARNING_NO_WARN;

#define MINOR_BUZZ_TIME_MS 200u
#define MAJOR_BUZZ_TIME_MS  2000u

int rotationCounter = 0;

void setup() {
  setPinModes();
  startBluetooth();

  while(!Bluefruit.connected()) {delay(1);}
  digitalWrite(CONNECTED_LED_PIN, HIGH);
}

void loop() {
  buttonPressEvent();
  setRotationSpeed();
  warningCount = determineRotationCount();
  warning = underspeedWarning();
  setBuzzer();
  blinkSpeedLED();
  sendRotationData();
  exitComputerWindow();
}

void exitComputerWindow() {
     if(warning == WARNING_EXIT_WINDOW) {
          //MODIFIER IS LEFT ALT KEY WHICH IS 0xE2 IN USB HID
          uint8_t Modifier = byte(0xE2);
          //KEY PRESS IS F4 WHICH IS 0x3D IN USB HID
          uint8_t Keys[6] = {byte(0x3D),
                                        byte(0x00),
                                        byte(0x00),
                                        byte(0x00),
                                        byte(0x00),
                                        byte(0x00)};
                                        
          blehid.keyboardReport(Modifier, Keys);
          delay(10); 

          //NOW UNDO IT
          Modifier = byte(0x00);
          Keys[0] = byte(0x00);
          blehid.keyboardReport(Modifier, Keys);
          delay(10); 
        }  
        } 

void sendRotationData() {
    if(button_pressed == BUTTON_HOLD_PRESS) {
    const char staticString[] = "YOU DID THIS MANY ROTATIONS: ";
    char dynamicString[6] = {0};
    itoa(rotationCounter, dynamicString, 10);
    
    for(int i = 0; i < sizeof(staticString); i++) {
          blehid.keyPress(staticString[i]);
          delay(10); }
    for(int i = 0; i < sizeof(dynamicString); i++) {
         
          blehid.keyPress(dynamicString[i]);
          delay(50); 
          blehid.keyRelease(); 
          delay(50); } //EXTRA DELAY FOR REPEATING DIGITS.
          
    blehid.keyRelease();      
    } }

void setBuzzer() {
     if(button_pressed == BUTTON_HOLD_NOTIFICATION) {
        buzz(BUTTON_HOLD_NOTIF_TIME_MS, 1000); }
     
     else if(warning == WARNING_SOUND_ONLY) {
        buzz(MINOR_BUZZ_TIME_MS, 1000); }

     else if(warning == WARNING_EXIT_WINDOW) {
        buzz(MAJOR_BUZZ_TIME_MS, 1000); }
}

int underspeedWarning() {
  int _warning = WARNING_NO_WARN;

  if(warningCount > 0) {
       firstWarningActive = true;
       secondWarningActive = true; }

  if((warningCount <= FIRST_WARNING) && firstWarningActive) {
     firstWarningActive = false;
     _warning = WARNING_SOUND_ONLY;
  }

 if((warningCount <= SECOND_WARNING) && secondWarningActive) {
     secondWarningActive = false;
     _warning = WARNING_SOUND_ONLY;
  }

 if((warningCount <= BOOT_OUT)) {
     _warning = WARNING_EXIT_WINDOW;
  }
  
  return _warning;
}

int determineRotationCount() {
  static int _warningCount = 0;
  static bool rotationSeenInTimeWindow = false;
  static unsigned long _previous_millis = 0;
  static unsigned long _debounce_counter_on = 0;
  static unsigned long _debounce_counter_off = 0;
  static unsigned long _rotation_time_window = 0;
  unsigned long _delta_millis = 0;
  unsigned long _current_millis = 0;

  static bool hall_effect_set = false;
  static bool _prev_hall_effect_set = false;
  
  //SET PREVIOUS HALL_EFFECT_SET
  _prev_hall_effect_set = hall_effect_set;
  
  //DETERMINE DELTA MILLIS
  //if _previous_millis = 0, its the first time through and should skip.
  if(_previous_millis == 0){ _previous_millis = millis(); return 0; }
  
  _current_millis = millis();
  _delta_millis = _current_millis - _previous_millis;

  _previous_millis = _current_millis;
  
  //DEBOUNCE THE HALL EFFECT SENSOR. HALL EFFECT IS OPEN DRAIN. GROUND IS SET.
  //IF CURRENT IS SET AND PREVIOUS IS FALSE, INCREMENT DEBOUNCE ON COUNTER
  //IF CURRENT IS NOT SET AND PREVIOUS IS TRUE, INCREMENT DEBOUNCE OFF COUNTER
  
  if((!digitalRead(HALL_EFFECT_SENSOR_PIN)) && !hall_effect_set) {
    _debounce_counter_on += _delta_millis;
    if(_debounce_counter_on > HALL_EFFECT_DEBOUNCE) {
      _debounce_counter_on = 0;
      hall_effect_set = true; 
      rotationCounter++;} }

  if((digitalRead(HALL_EFFECT_SENSOR_PIN)) && hall_effect_set) {
    _debounce_counter_off += _delta_millis;
    if(_debounce_counter_off > HALL_EFFECT_DEBOUNCE) {
      _debounce_counter_off = 0;
      hall_effect_set = false; } }

  //IF HALL EFFECT HAS CHANGED, SET FLAG TO TRUE
  if(_prev_hall_effect_set != hall_effect_set){rotationSeenInTimeWindow = true;}

  //INCREMENT TIME WINDOW. IF OVER MAXIMUM RESET WINDOW, CHANGE COUNTER, AND RESET ROTATION SEEN FLAG.
  _rotation_time_window += _delta_millis;

  if(_rotation_time_window > speed_array_ms[current_speed]) {
    _rotation_time_window = 0;

    if(rotationSeenInTimeWindow) {_warningCount += ROT_WARN_UP_COUNT; }
    else if (exerciseActive) {_warningCount -= ROT_WARN_DN_COUNT;}

    rotationSeenInTimeWindow = false;
  }

  //SET ROTATION COUNTER LIMITS
  _warningCount = min(_warningCount, ROT_WARN_MAX);
  _warningCount = max(_warningCount, ROT_WARN_MIN);

  if(_warningCount == ROT_WARN_MAX) {exerciseActive = true;}
  if(_warningCount == ROT_WARN_MIN) {
    exerciseActive = false;
    _warningCount = 0;}
   
  //SET SEEN FLAG IF SEEN
  return _warningCount;
}

void blinkSpeedLED() {
  //Get Remainder of Millis divided by Speed. This is the full period of an LED Cycle. If it's in the later half of that, set to TRUE;
  unsigned long mod_remainder = millis() % speed_array_ms[current_speed];
  bool led_state = (mod_remainder > speed_array_ms[current_speed] / 2);

  digitalWrite(SPEED_LED_PIN, led_state); }

void setRotationSpeed() {
    if(button_pressed == BUTTON_QUICK_PRESS) {
      current_speed++;
      if(current_speed == NUM_OF_SPEEDS) {current_speed = 0;} } }

void buttonPressEvent() {
  static unsigned long _previous_millis = 0;
  static unsigned long _debounce_counter = 0;
  unsigned long _delta_millis = 0;
  unsigned long _current_millis = 0;

  //if _previous_millis = 0, its the first time through and should skip.
  if(_previous_millis == 0){ _previous_millis = millis(); return; }
  
  _current_millis = millis();
  _delta_millis = _current_millis - _previous_millis;

  //Add Debounce Counter if Button Is Pressed. Then if It's Pressed for greater than long time, set that event.
  if(!digitalRead(BUTTON_PIN)) {
    _debounce_counter += _delta_millis;
    if(_debounce_counter > LONG_PRESS_MS) {button_pressed = BUTTON_HOLD_NOTIFICATION;}
  }
  //If Button is Not Pressed, If Counter was greater than short or long press set that event. Else set no event. Reset counter.
  else {
    if(_debounce_counter > LONG_PRESS_MS) {button_pressed = BUTTON_HOLD_PRESS;}
    else if(_debounce_counter > QUICK_PRESS_MS) {button_pressed = BUTTON_QUICK_PRESS;}
    else{button_pressed = BUTTON_NOT_PRESSED;}
    _debounce_counter = 0;
  }

  _previous_millis = _current_millis;
}

void setPinModes() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(CONNECTED_LED_PIN, OUTPUT);  
  pinMode(SPEED_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(HALL_EFFECT_SENSOR_PIN, INPUT_PULLUP);  

  digitalWrite(BUZZER_PIN, false);
  digitalWrite(CONNECTED_LED_PIN, false);
  digitalWrite(SPEED_LED_PIN, false); }


void startBluetooth() {
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("VGC_Bike"); 
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather 52");
  bledis.begin();
  blehid.begin();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);  
  Bluefruit.Advertising.setFastTimeout(30);      
  Bluefruit.Advertising.start(0);            }

void buzz(long duration_ms, long frequency) {
    long delay_micro = 1000000 / frequency / 2;
    long duration_count = duration_ms * 1000 / (delay_micro * 2);
    
    for(long x = 0; x < duration_count; x++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delayMicroseconds(delay_micro);
      digitalWrite(BUZZER_PIN, LOW);
      delayMicroseconds(delay_micro); }
}
