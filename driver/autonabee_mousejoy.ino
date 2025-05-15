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


EEPROM MEMORY
0 int DX OFFSET
2 int DY OFFSET
4 int DEADZONE
6 int MOVEFLAG
8 float SPEED
12 float LOGSPEED
16 float LOGW

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


const int DEFAULT_OFFSET = 1024/2;
const int DEFAULT_DEADZONE = 50;
const int DEFAULT_MOVEFLAG = 0;
const float DEFAULT_SPEED = 8.0;
const float DEFAULT_LOGSPEED = 1.0;
const float DEFAULT_LOGW = 1024*9;

int dx_ofs = 0;
int dy_ofs = 0;
int deadzone = 0;
int moveflag = 0;
float speed = 0;
float logspeed = 0;
float logw = 0;


void load_values_from_EEPROM() {
  EEPROM.get(ADR_DX_OFS, dx_ofs);
  EEPROM.get(ADR_DY_OFS, dy_ofs);
  EEPROM.get(ADR_DEADZONE, deadzone);
  EEPROM.get(ADR_MOVEFLAG, moveflag);
  EEPROM.get(ADR_SPEED, speed);
  EEPROM.get(ADR_LOGSPEED, logspeed);
  EEPROM.get(ADR_LOGW, logw);
}

void write_defaults_to_EEPROM() {
  EEPROM.put(ADR_DX_OFS, DEFAULT_OFFSET);
  EEPROM.put(ADR_DY_OFS, DEFAULT_OFFSET);
  EEPROM.put(ADR_DEADZONE, DEFAULT_DEADZONE);
  EEPROM.put(ADR_MOVEFLAG, DEFAULT_MOVEFLAG);
  EEPROM.put(ADR_SPEED, DEFAULT_SPEED);
  EEPROM.put(ADR_LOGSPEED, DEFAULT_LOGSPEED);
  EEPROM.put(ADR_LOGW, DEFAULT_LOGW);
  load_values_from_EEPROM();
  Serial.println("RESET TO DEFAULTS SETTINGS");
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

void print_tick(String name, String s) {
      Serial.print(name);
      Serial.print(": '");
      Serial.print(s);
      Serial.println("'");
}

const int iter_size = 5;
const int adr_iter[] = {
  ADR_DEADZONE,
  ADR_MOVEFLAG,
  ADR_SPEED,
  ADR_LOGSPEED,
  ADR_LOGW,
};

const String commands_iter[] = {
  "DEADZONE",
  "MOVEFLAG",
  "SPEED",
  "LOGSPEED",
  "LOGW",
};

const bool is_int_iter[] = {
  true,
  true,
  false,
  false,
  false,
};

// Parses and handles commands.
void handle_serial_communication() {
  while (Serial.available() > 0) {
    String command = Serial.readString();
    //Serial.print("received: ");
    //Serial.println(command);
    
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
      }
      
      String variable = rest.substring(0, space);
      String value = rest.substring(space + 1);
      
      int int_value = value.toInt();
      float float_value = value.toFloat();
      
      if (int_value == 0) {
        Serial.println("Value may be invalid: using 0.");
      } else if (float_value == 0.0) {
        Serial.println("Value may be invalid: using 0.");
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



int all_signals_length = 4;
int all_signals[] = { A2, A3, A4, A5 }; // used by the switches
int buttons_debounce[] = {0, 0, 0, 0}; // used to store the debounce timer
const int debounce_delay = 300; // in milliseconds

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

void setup() {
  load_values_from_EEPROM();
  while (!Serial) {}
  print_tick("X offset", String(dx_ofs));
  print_tick("Y offset", String(dy_ofs));
  print_tick("Speed", String(speed));
  print_tick("Deadzone", String(deadzone));
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

int print_debounce = 0;
int mouse_report_debounce = 0;
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
            Keyboard.press(KEY_LEFT_SHIFT);
            digitalWrite(A_LED_SCROLL, HIGH);
            Serial.println("activate switch scroll");
          }
          else { 
            Keyboard.release(KEY_LEFT_SHIFT);
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

  float dx = -(analogRead(A1) - dx_ofs) ;
  float dy = -(analogRead(A0) - dy_ofs) ;
  if (abs(dx) <= deadzone ) {dx = 0.0;}
  if (abs(dy) <= deadzone ) {dy = 0.0;}
  
  if (print_debounce < millis()) {
    print_debounce = millis() + 2000;
    Serial.print("dx: ");
    Serial.print(analogRead(A0));
    Serial.print(" -> ");
    Serial.println(dx);
    
    Serial.print("dy: ");
    Serial.print(analogRead(A1));
    Serial.print(" -> ");
    Serial.println(dy);
    
  }

  if (mouse_report_debounce < millis()) {
    mouse_report_debounce = millis() + 100;

    if (scroll_mode)  {
      if (moveflag == 0) {
        Mouse.move(
          dx * speed,
          dy * speed
        );
      }
      else {
        Mouse.move(
          logspeed * signOf(dx) * log10(1 + abs(dx * logw)),
          logspeed * signOf(dy) * log10(1+ abs(dy * logw))
        );
      }
      delay(100);
    }    
    else {
      if (dy > 0) {
        Mouse.move(0,0, -2.0);
      }
      else if (dy < 0) {
        Mouse.move(0,0, 2.0);
        }
//        delay(100);
    }
  }
}

int signOf(int i) {
  if (i < 0) { return -1; }
  return +1;
}
