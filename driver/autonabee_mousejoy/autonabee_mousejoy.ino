#include "HID-Project.h"
#include "EEPROM.h"

/*
Script to use with an arduino micro, hooked to 4 switches and a joystick.

the goal is to emulate a mouse, two buttons will behave as left and right mouse clicks,
two buttons will toggle shift mode (left key pressed) or scroll mode (moving the joystick will scroll instead of moving the mouse).

The behaviour can be customized by sending commands to the board via tty or with the helper python script (TODO).
EXPECTED COMMANDS:
- `CALIBRATE` to send while leaving the joystick untouched while the systems calibrate.
- `DEFAULTS`
- `HELP`
- `SET <X> <Y>` where Y is an integer or a float and X must be one of
  - int DEADZONE: a value between 0 and 512, all joystick input under this range will be ignored.
  - int MOVEFLAG: 0 indicates to move the mouse using a linear scale, 1 in a logarithmic scale.
  - float SPEED : used if MOVEMOD = 0, determines the speed of the mouse. SPEED * analog signal from joystick
  - float LOGSPEED: used if MOVEMOD = 1, LOGSPEED * signOf(signal) * log10(1 + abs(signal)* LOGW)
  - float LOGW: used if MOVEMOD = 1, see LOGSPEED
  scroll delay is used instead of scroll speed for technical reasons. it's formula is MINDELAYSCROLL + SCROLLOFFSET/1+SCROLLSPEED*signal
  - float MINDELAYSCROLL, the minimum delay between two polls of scroll.
  - float SCROLLOFFSET, the maximum additional scroll, see scroll.

EEPROM MEMORY
0 int DX OFFSET
2 int DY OFFSET
4 int DEADZONE
6 int MOVEFLAG
8 float SPEED
12 float LOGSPEED
16 float LOGW
20 float MINDELAY
24 float SCROLLOFFSET
28 float SCROLLSPEED

*/

#define CALIBRATE "CALIBRATE"
// ints
const int ADR_DX_OFS = 0;
const int ADR_DY_OFS = 2;
const int ADR_DEADZONE = 4;
const int ADR_MOVEFLAG = 6;
// floats
const int ADR_SPEED = 8;
const int ADR_LOGSPEED = 12;
const int ADR_LOGW = 16;
const float ADR_MINDELAY = 20;
const float ADR_SCROLLOFFSET = 24 ;
const float ADR_SCROLLSPEED = 28;

// Defaults
const int DEFAULT_OFFSET = 1024/2;
const int DEFAULT_DEADZONE = 50;
const int DEFAULT_MOVEFLAG = 0;
const float DEFAULT_SPEED = 8.0;
const float DEFAULT_LOGSPEED = 30.0;
const float DEFAULT_LOGW = 7.0;
const float DEFAULT_MINDELAY = 20;
const float DEFAULT_SCROLLOFFSET = 300 ;
const float DEFAULT_SCROLLSPEED = 1.0/1054*10;

// variable used in the program, loaded from the eeprom.
int dx_ofs = 0;
int dy_ofs = 0;
int deadzone = 0;
int moveflag = 0;
float speed = 0;
float logspeed = 0;
float logw = 0;
float min_delay_scroll = 0;
float scroll_off = 0;
float scroll_speed = 0;

// related to handling of switches and toggles
int all_signals_length = 4;
int all_signals[] = { A2, A3, A4, A5 }; // used by the switches

int button_lengths = 2;
int button_signals[] = {A2, A3};
bool button_pressed[] = {false, false};
int buttons_id[] = {MOUSE_LEFT, MOUSE_RIGHT};

int switches_lengths = 2;
int switch_scroll_idx = 0;
int switches_signals[] = {A4, A5};
bool switches_toggled[] = {false, false};

#define SwitchScroll 0
#define SwitchShift 1
int switches_id[] = {
  SwitchScroll,
  SwitchShift
};
const int A_LED_SCROLL = 6;
const int A_LED_SHIFT = 7;


// debouncing variables
unsigned long print_debounce = 0;
const unsigned long print_debounce_delay = 1000;
unsigned long mouse_report_debounce = 0;
unsigned long mouse_report_debounce_delay = 20;
unsigned long scroll_report_debounce = 0;
unsigned long buttons_debounce[] = {0, 0, 0, 0}; // used to store the debounce timer
const unsigned long debounce_delay = 300; // in milliseconds

void setup() {
  load_values_from_EEPROM();
  long max_delay = 2000;
  while (!Serial) {
    if (millis() > max_delay) {break;}
  }

  print_tick("X offset", String(dx_ofs));
  print_tick("Y offset", String(dy_ofs));
  print_tick("Speed", String(speed));
  print_tick("Deadzone", String(deadzone));
  print_tick("Logw", String(logw));
  print_tick("Logspeed", String(logspeed));
  print_tick("MoveFlag", String(moveflag));

  // we init all the inputs.
  for (int i = 0; i < all_signals_length; i++) {
    pinMode(all_signals[i], INPUT_PULLUP); // pas besoin de resistances pour les switchs.
  }
  pinMode(A_LED_SCROLL, OUTPUT);
  pinMode(A_LED_SHIFT, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("Arduino joy mouse starts!");

  // Sends a clean report to the host.
  Mouse.begin();
  Keyboard.begin();
}


void loop() {
  handle_serial_communication();
  
  for (int i = 0; i < button_lengths; i++) {
    if (!digitalRead(button_signals[i]) and not button_pressed[i] and buttons_debounce[i] < millis()) {
      buttons_debounce[i] = millis() + debounce_delay;
      button_pressed[i] = true;
      Mouse.press(buttons_id[i]);
      Serial.print("button press: ");
      Serial.print(i);
      Serial.print("\n");
    }
    else if (digitalRead(button_signals[i]) and button_pressed[i]) {
      button_pressed[i] = false;
      
      Mouse.release(buttons_id[i]);
      Serial.print("release: ");
      Serial.print(i);
      Serial.print("\n");
    }
  }


  
  bool scroll_mode;
  for (int i = 0; i < switches_lengths; i++) {
      if (!digitalRead(switches_signals[i]) && buttons_debounce[i + 2] < millis() ) { // i + 2 because buttons_debounce arrays starts with two other buttons
        buttons_debounce[i + 2] = millis() + debounce_delay;
        switches_toggled[i] = !switches_toggled[i]; // we toggle the value
        
        if (switches_id[i] == SwitchScroll) {
          scroll_mode = switches_toggled[i];
          if (!switches_toggled[i]) {
            digitalWrite(A_LED_SCROLL, HIGH);
            Serial.println("activate switch scroll");
          }
          else { 
            digitalWrite(A_LED_SCROLL, LOW);
            Serial.println("deactivate switch scroll");
          }
        }
        else if (switches_id[i] == SwitchShift) {
          if (switches_toggled[i]) {
            Keyboard.press(KEY_LEFT_SHIFT);
            digitalWrite(A_LED_SHIFT, HIGH);
            Serial.println("activate switch shift");
          }
          else { 
            Keyboard.release(KEY_LEFT_SHIFT);
            digitalWrite(A_LED_SHIFT, LOW);
            Serial.println("deactivate switch shift");
          }
        }
      }
  }


  bool dx_dead = false;
  bool dy_dead = false;
  char dx = get_mouse_dz(analogRead(A1), dx_ofs, deadzone, dx_dead, speed);
  char dy = get_mouse_dz(analogRead(A0), dy_ofs, deadzone, dy_dead, speed);
  
  if (print_debounce < millis()) {
    print_debounce = millis() + print_debounce_delay;
    if (dx_dead) {Serial.print("D");} else{Serial.print("A");}
    Serial.print("dx: ");
    Serial.print(analogRead(A1));
    Serial.print(" -> ");
    Serial.print(int(dx));

    Serial.print(" || ");

    if (dy_dead) {Serial.print("D");} else{Serial.print("A");}
    Serial.print("dy: ");
    Serial.print(analogRead(A0));
    Serial.print(" -> ");
    Serial.println(int(dy));
    
  }

  

  if (scroll_mode)  {
    if (moveflag == 0) {
      if (mouse_report_debounce < millis()) {
        mouse_report_debounce = millis() + mouse_report_debounce_delay;
        Mouse.move(
          dx,
          dy
        );          
      }
    }
    else {
      if (mouse_report_debounce < millis()) {
        mouse_report_debounce = millis() + mouse_report_debounce_delay;
        Mouse.move(
          logspeed * signOf(dx) * (exp(abs(float(dx) * logw / 100.0)) - 1.0 ),
          logspeed * signOf(dy) * (exp(abs(float(dy) * logw / 100.0)) - 1.0 )
        );
      }
    }
  }    
  else {
    if (scroll_report_debounce < millis()) {
      scroll_report_debounce = millis() + 100;
      
      char dz = get_mouse_dz(analogRead(A0), 0, deadzone, true, scroll_speed);
      float scroll_delay = min_delay_scroll + scroll_off/1.0+dz;
      scroll_report_debounce = millis() + scroll_delay;
      Mouse.move(0,0, signOf(dy));
    }
  }
}

// parsing related
const int iter_size = 5;
const int adr_iter[] = {
  ADR_DEADZONE,
  ADR_MOVEFLAG,
  ADR_SPEED,
  ADR_LOGSPEED,
  ADR_LOGW,
  ADR_MINDELAY,
  ADR_SCROLLOFFSET,
  ADR_SCROLLSPEED,
};

const String commands_iter[] = {
  "DEADZONE",
  "MOVEFLAG",
  "SPEED",
  "LOGSPEED",
  "LOGW",
  "MINDELAY",
  "SCROLLOFFSET",
  "SCROLLSPEED"
};

const bool is_int_iter[] = {
  true,
  true,
  false,
  false,
  false,
  false,
  false,
  false
};


// Parses and handles commands.
void handle_serial_communication() {
  while (Serial.available() > 0) {
    String command = Serial.readString();
    
    if (command.indexOf(CALIBRATE) != -1) {
        calibrate();
    }
    else if (command.indexOf("DEFAULTS") != -1) {
      write_defaults_to_EEPROM();
    }
    else {
      int space = command.indexOf(" ");
      if (space == -1) {
          Serial.print("Command invalid: ");
          Serial.println(command);
      }
      String set_part = command.substring(0, space);
      String rest = command.substring(space + 1); // we skip the space itself

      if (!set_part.equals("SET")) {
          Serial.print("EXPECTED SET, FOUND: ");
          Serial.println(set_part);
          continue;
      }

      space = rest.indexOf(" ");
      if (space == -1) {
          Serial.println("Expected VARIABLE VALUE");
          Serial.print("received: ");
          Serial.println(command);
          continue;
      }
      
      String variable = rest.substring(0, space);
      String value = rest.substring(space + 1);
      
      int int_value = value.toInt();
      float float_value = value.toFloat();
      
      if (int_value == 0) {
        Serial.println("Value may be invalid: using 0.");
        Serial.println(String("[") + value + String("]"));
      } 
      if (float_value == 0.0) {
        Serial.println("Value may be invalid: using 0.0.");
        Serial.println(String("[") + value + String("]"));
      }

      bool matched = false;
      for (int i = 0; i < iter_size; i++) {
        int addr = adr_iter[i];
        String command = commands_iter[i];
        bool is_int = is_int_iter[i];
        if (variable.equals(command)) {
          Serial.print("Set ");
          Serial.print(command);
          Serial.print(" to ");

          if (is_int) {
            EEPROM.put(addr, int_value);
            Serial.println(int_value);
          }
          else {
            EEPROM.put(addr, float_value);
            Serial.println(float_value);
          }
          matched = true;
          load_values_from_EEPROM();
          break;
        }
      }
      if (!matched) {
        Serial.print("Expected ");
        for (int i = 0; i < iter_size ;i++) {
          if (i != 0) {Serial.print("or ");}
          Serial.print(commands_iter[i]);
        }
        Serial.print("received: ");
        Serial.println(variable);
      }
    }
  }
}

// expects a value between 0-1023
// necessary because Mouse.move requires char as input.
char get_mouse_dz(int value, int offset, int deadzone, bool dz_dead, float speed) {
  float normalized = (float)(value - offset);
  
  if (abs(normalized ) <= (float)deadzone ) {normalized = 0.0; dz_dead = true;}
  normalized *= speed / 1024.0;
  
  int max_value = pow(2, sizeof(char) * 7);
  
  if (normalized > max_value) { normalized =  max_value; }
  if (normalized <  - max_value) { normalized = - max_value; }
  
  return (char)normalized;
}

void calibrate() {
  EEPROM.put(ADR_DX_OFS, analogRead(A1));
  EEPROM.put(ADR_DY_OFS, analogRead(A0));

  Serial.print("Calibrated with ");
  Serial.print(analogRead(A1));
  Serial.print("-");
  Serial.println(analogRead(A0));
  load_values_from_EEPROM();
}

void load_values_from_EEPROM() {
  EEPROM.get(ADR_DX_OFS, dx_ofs);
  EEPROM.get(ADR_DY_OFS, dy_ofs);
  EEPROM.get(ADR_DEADZONE, deadzone);
  EEPROM.get(ADR_MOVEFLAG, moveflag);
  EEPROM.get(ADR_SPEED, speed);
  EEPROM.get(ADR_LOGSPEED, logspeed);
  EEPROM.get(ADR_LOGW, logw);
  EEPROM.get(ADR_MINDELAY, min_delay_scroll);
  EEPROM.get(ADR_SCROLLOFFSET, scroll_off);
  EEPROM.get(ADR_SCROLLSPEED, scroll_speed);
}

void write_defaults_to_EEPROM() {
  EEPROM.put(ADR_DX_OFS, DEFAULT_OFFSET);
  EEPROM.put(ADR_DY_OFS, DEFAULT_OFFSET);
  EEPROM.put(ADR_DEADZONE, DEFAULT_DEADZONE);
  EEPROM.put(ADR_MOVEFLAG, DEFAULT_MOVEFLAG);
  EEPROM.put(ADR_SPEED, DEFAULT_SPEED);
  EEPROM.put(ADR_LOGSPEED, DEFAULT_LOGSPEED);
  EEPROM.put(ADR_LOGW, DEFAULT_LOGW);
  EEPROM.put(ADR_MINDELAY, DEFAULT_MINDELAY);
  EEPROM.put(ADR_SCROLLOFFSET, DEFAULT_SCROLLOFFSET);
  EEPROM.put(ADR_SCROLLSPEED, DEFAULT_SCROLLSPEED);

  load_values_from_EEPROM();
  Serial.println("RESET TO DEFAULTS SETTINGS");
}

void print_tick(String name, String s) {
  Serial.print(name);
  Serial.print(": '");
  Serial.print(s);
  Serial.println("'");
}

int signOf(int i) {
  if (i < 0) { return -1; }
  return +1;
}


int signOf(float i) {
  if (i < 0.0) { return -1; }
  return +1;
}
