//////////////////////////// Step sequencer ///////////////////////////////////////////////
////                                      v1.1
////    Changes:
////
////    -Added 16 steps
////    -Steps can be changed trough 8 buttons, where there is a switch that selects betwen 
////      steps 1-8 and 9-16, this reflects on the 8 leds
////    -Added a second key that can be pressed at the sime time for each step
////    -Added a second encoder to control what key is beying pressed
////    -If you press the encoder button while pressing a step button, it will erase the note
////    -If you press the two encoder switches at the same time, all keys of all steps will
////      be erased
////
////
///////////////////////////////////////////////////////////////////////////////////////////
////     Libraries
#include <Arduino.h>
#include <Keyboard.h>
#include <EEPROM.h>


//////////////////////////// Configurations ///////////////////////////////////////////////
////     Pins definitions
#define encoder1CLK       13                //Encoder 1 output CLK
#define encoder1DT        14                //Encoder 1 output DT
#define encoder1SW        15                //Encoder 1 output Switch
#define encoder2CLK       16                //Encoder 2 output CLK
#define encoder2DT        17                //Encoder 2 output DT
#define encoder2SW        18                //Encoder 2 output Switch

#define button8_16        12                //Switch for selecting steps 1-8/9-16 
#define buttonsAnalogIn   A1                //Step buttons input
#define BPMAnalogIn       A2                //Potentiometer for speed control
#define playButton        11                //Button for Play / Stop
#define ledBPM            10                //Led for BPM indication
const uint8_t ledStep[] = {
  9, 8, 7, 6, 5, 4, 3, 2,                   //Led Step pinouts, from Step 1 to Step 8
  9, 8, 7, 6, 5, 4, 3, 2                    //Led Step pinouts, from Step 9 to Step 16
};


////     User definitions
#define maxBPM            840               //Max step speed, in BPM
#define minBPM            60                //Min step speed, in BPM
#define numberOfSteps     16                //Total of steps

////     Keyboard keys to be defined in Steps
const char Keys[] = {
  '\0',                                     //Don't change this from initial from the code, as it means erased key
  '1', '2', '3', '4', '5',
  '6', '7', '8', '9', '0',
  'a', 'b', 'c', 'd', 'e',
  'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'm', 'n', 'o',
  'p', 'q', 'r', 's', 't',
  'u', 'v', 'w', 'x', 'y',
  'z', ',', '.', '['
};


//////////////////////////// Program definitions and variables ////////////////////////////
////    Definitions
#define encoderUnchanged  0
#define encoderCW         1
#define encoderCCW        2

#define Step1             0
#define Step2             1
#define Step3             2
#define Step4             3
#define Step5             4
#define Step6             5
#define Step7             6
#define Step8             7
#define Step9             8
#define Step10            9
#define Step11            10
#define Step12            11
#define Step13            12
#define Step14            13
#define Step15            14
#define Step16            15
#define none              100

////    Timer Variables
unsigned int sequencingTime;
unsigned int readingBPMTimer;
unsigned int debouncingTime;
unsigned int StepSequencerTime;
unsigned int programSaveTime;

uint8_t      analogReadingBPM;
unsigned int BeatsPerMinute;
uint8_t      analogReadingStep;
int          lastAnalogReading;

////    Buttons variables

uint8_t buttonPressed;
bool    buttonStillPressed;

bool    playButtonToogle;
bool    playButtonLastState = true;
uint8_t playButtonSteps;

bool    button8_16LastState;

bool    ledBPMindicator;

bool    notePlayed;
bool    PlaySequencer;
uint8_t StepSequencer;

bool    programChanged;

typedef struct {
  bool         onSequence = true;
  uint8_t      keyNumber1;
  uint8_t      keyNumber2;
  unsigned int keyTempo1 = 120;
  unsigned int keyTempo2 = 120;
} SequencerSteps;

class PlayNotes {
  private:
    unsigned int Tempo;
    uint8_t      Note;
    uint8_t      lastNote;
    uint8_t      encoderCLK;
    uint8_t      encoderDT;
    uint8_t      encoderSW;
    bool         encState;
    bool         encLastState;

    uint8_t      encoderRead();

  public:
    uint8_t      PlayingNote;
    unsigned int Timer;

    PlayNotes(uint8_t encoderCLK, uint8_t encoderDT, uint8_t encoderSW);
    void         PlayNote(uint8_t Note, unsigned int Tempo);
    void         Management();
    uint8_t      keyAssignment(uint8_t StepSequence, uint8_t keyNumber, unsigned int Tempo);

};

SequencerSteps Step[numberOfSteps];

PlayNotes playKey1(encoder1CLK, encoder1DT, encoder1SW);
PlayNotes playKey2(encoder2CLK, encoder2DT, encoder2SW);

void    readProgram();
void    saveProgram();
void    saveProgramChanged();
uint8_t encoderRead();
uint8_t keyAssignment(uint8_t StepSequence);
uint8_t buttonsRead();
bool    playButtonRead();
void    setBpm();
void    ledOnChangeButton8_16();
void    eraseAllKeys();

void setup() {

  //  Timer1 setup
  cli();                                                    //  Disable global interrupts
  TCCR1A = 0;                                               //  Set entire TCCR1A register to 0
  TCCR1B = 0;                                               //  Same for TCCR1B
  OCR1A = 15999;                                            //  Set compare match register to desired timer count
  TCCR1B |= (1 << WGM12);                                   //  Turn on CTC mode:
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10);        //  Set CS10 and CS12 bits for 1024 prescaler
  TIMSK1 |= (1 << OCIE1A);                                  //  Enable timer compare interrupt
  sei();                                                    //  Enable global interrupts

  readProgram();

  ////    Pin definition
  pinMode(encoder1CLK, INPUT_PULLUP);
  pinMode(encoder1DT, INPUT_PULLUP);
  pinMode(encoder1SW, INPUT_PULLUP);
  pinMode(encoder2CLK, INPUT_PULLUP);
  pinMode(encoder2DT, INPUT_PULLUP);
  pinMode(encoder2SW, INPUT_PULLUP);
  pinMode(button8_16, INPUT_PULLUP);
  pinMode(playButton, INPUT);
  
  pinMode(ledBPM, OUTPUT);
  for (byte i = 0; i <= 15; i++)
    pinMode(ledStep[i], OUTPUT);

  ////    EEPROM reading of last used program
  for (byte i = 0; i <= 7; i++) {
    digitalWrite(ledStep[i], HIGH);
    delay(100);
  }
  if (digitalRead (button8_16))
    for (byte i = 0; i <= 7; i++) {
      digitalWrite(ledStep[i], Step[i].onSequence);
      delay(100);
    }
  else
    for (byte i = 8; i <= 15; i++) {
      digitalWrite(ledStep[i], Step[i].onSequence);
      delay(100);
    }

  int analogReading = analogRead(BPMAnalogIn);
  BeatsPerMinute = map(analogReading, 0, 1022, minBPM, maxBPM);
  BeatsPerMinute = (unsigned long)60000 / BeatsPerMinute;
  lastAnalogReading = analogReading;
}

////    Timer 1 for accurate BPM
ISR(TIMER1_COMPA_vect) {
  playKey1.Timer++;
  playKey2.Timer++;
  debouncingTime++;
  sequencingTime++;
  readingBPMTimer++;
  StepSequencerTime++;
  programSaveTime++;
}


void loop() {
  uint8_t buttonStep = buttonsRead();
  PlaySequencer = playButtonRead();
  playKey1.Management();
  playKey2.Management();
  saveProgram();


  ////  If it's on Play mode
  if (PlaySequencer) {
    if (Step[StepSequencer].onSequence) {                                               //If step is on sequence
      if (!notePlayed) {                                                                //Send command to play note once
        playKey1.PlayNote(Step[StepSequencer].keyNumber1, Step[StepSequencer].keyTempo1);
        playKey2.PlayNote(Step[StepSequencer].keyNumber2, Step[StepSequencer].keyTempo2);
        digitalWrite(ledStep[StepSequencer], LOW);                                      //Turn off led the time note is being played
        notePlayed = true;
      }
      if (playKey1.PlayingNote == 0 && playKey2.PlayingNote == 0)
        digitalWrite(ledStep[StepSequencer], HIGH);                                     //Turn on led when it's done playing
    }

    if (buttonStep != none) {                                                           //Check if any button is being pressed
      if (!buttonStillPressed) {
        Step[buttonStep].onSequence = !Step[buttonStep].onSequence;                     //Set the Step on or off the sequence

        if ((StepSequencer < 8 && buttonStep < 8) || (StepSequencer >= 8 && buttonStep >= 8)) {
          if (Step[buttonStep].onSequence)
            digitalWrite(ledStep[buttonStep], HIGH);                                      //Turn on led if it's on sequence
          else
            digitalWrite(ledStep[buttonStep], LOW);                                       //Turn off led if it's not on the sequence
        }

        saveProgramChanged();
        buttonStillPressed = true;
      }
      else {
        Step[buttonStep].keyNumber1 = playKey1.keyAssignment(buttonStep, Step[buttonStep].keyNumber1, Step[buttonStep].keyTempo1);                //If button are still pressed, enable function to designate key to the button that is still pressed
        Step[buttonStep].keyNumber2 = playKey2.keyAssignment(buttonStep, Step[buttonStep].keyNumber2, Step[buttonStep].keyTempo2);                //If button are still pressed, enable function to designate key to the button that is still pressed
        //TODO function to change time of each Step
      }
    }
    else {                                                                              //If none of the buttons was pressed
      if (Step[StepSequencer].onSequence) {
        if (playKey1.PlayingNote == 0 && playKey2.PlayingNote == 0)
          digitalWrite(ledStep[StepSequencer], HIGH);                                   //Turn on led if it's on sequence
      }
      eraseAllKeys();
      setBpm();                                                                         //Function to set BPM
    }

    if (StepSequencerTime >= BeatsPerMinute) {                   //Time reached for the next step
      if (Step[StepSequencer].onSequence)
        digitalWrite(ledStep[StepSequencer], HIGH);                                     //Turn on led if it's on sequence (intentional redundance)
      StepSequencer++;                                                                  //Ready for the next sequence
      notePlayed = false;                                                               //Ready to play next note
      StepSequencerTime = 0;                                                            //Reset Step Timer
      ledBPMindicator = !ledBPMindicator;                                               //Led flashes with BPM
      digitalWrite(ledBPM, ledBPMindicator);

      if (StepSequencer >= numberOfSteps)                                               //Reset the sequence
        StepSequencer = Step1;

      if (StepSequencer == Step1)
        for (byte i = Step1; i <= Step8; i++)                                           //Redundance for led activations (intentional)
          digitalWrite(ledStep[i], Step[i].onSequence);
      if (StepSequencer == Step9)
        for (byte i = Step9; i <= Step16; i++)                                          //Redundance for led activations (intentional)
          digitalWrite(ledStep[i], Step[i].onSequence);
    }
  }


  ////  If it's on Stop mode
  else {
    if (StepSequencerTime >= BeatsPerMinute) {                   //Time of BPM reached
      notePlayed = false;                                                               //Ready to play next note
      StepSequencerTime = 0;                                                            //Reset Step Timer
      ledBPMindicator = !ledBPMindicator;
      digitalWrite(ledBPM, ledBPMindicator);
    }
    if (buttonStep != none) {                                                           //Check if any button is being pressed
      if (!buttonStillPressed) {
        Step[buttonStep].onSequence = !Step[buttonStep].onSequence;                     //Set the Step on or off the sequence

        bool button8_16State = digitalRead(button8_16);
        if ((button8_16State && buttonStep < 8) || (!button8_16State && buttonStep >= 8)) {
          if (Step[buttonStep].onSequence)
            digitalWrite(ledStep[buttonStep], HIGH);                                      //Turn on led if it's on sequence
          else
            digitalWrite(ledStep[buttonStep], LOW);                                       //Turn off led if it's not on the sequence
        }

        saveProgramChanged();
        buttonStillPressed = true;
      }
      else {
        Step[buttonStep].keyNumber1 = playKey1.keyAssignment(buttonStep, Step[buttonStep].keyNumber1, Step[buttonStep].keyTempo1);                //If button are still pressed, enable function to designate key to the button that is still pressed
        Step[buttonStep].keyNumber2 = playKey2.keyAssignment(buttonStep, Step[buttonStep].keyNumber2, Step[buttonStep].keyTempo2);                //If button are still pressed, enable function to designate key to the button that is still pressed
        //TODO function to change time of each Step
      }
    }
    else {                                                                              //If none of the buttons was pressed
      eraseAllKeys();
      setBpm();                                                                         //Function to set BPM
    }
    ledOnChangeButton8_16();
  }
}


void eraseAllKeys() {
  if (!digitalRead(encoder1SW) && !digitalRead(encoder2SW)) {
    for (int i = 0; i < numberOfSteps; i++) {
      Step[i].keyNumber1 = 0;
      Step[i].keyNumber2 = 0;
    }
  }
}

void setBpm() {
  int analogReading = analogRead(BPMAnalogIn);

  switch (analogReadingBPM) {
    case Step1:
      if (analogReading - lastAnalogReading > 15 || analogReading - lastAnalogReading < -15) {
        readingBPMTimer = 0;
        analogReadingBPM = Step2;
      }
      break;

    case Step2:
      BeatsPerMinute = map(analogReading, 0, 1022, minBPM, maxBPM);
      BeatsPerMinute = (unsigned long)60000 / BeatsPerMinute;
      
      if (analogReading - lastAnalogReading > 15 || analogReading - lastAnalogReading < -15) {
        readingBPMTimer = 0;
        lastAnalogReading = analogReading;
      }

      if (readingBPMTimer > 500) {
        lastAnalogReading = analogReading;
        analogReadingBPM = Step1;
      }
      break;
  }
}

bool playButtonRead() {
  bool playButtonState = digitalRead(playButton);

  switch (playButtonSteps) {
    case 0:
      if (playButtonState  != playButtonLastState) {                                        //Line executed just once
        bool button8_16State = digitalRead(button8_16);
        playButtonSteps     = 1;
        playButtonLastState = playButtonState;
        playButtonToogle    = !playButtonToogle;
        StepSequencer = Step1;                                                              //Sequence goes automatically to Step 1
        StepSequencerTime = 0;                                                              //Step Timer is set to 0
        if (playButtonToogle || button8_16State)
          for (byte i = 0; i <= 7; i++)                                                       //Redundance for led activations (intentional)
            digitalWrite(ledStep[i], Step[i].onSequence);
        if (!button8_16State && !playButtonToogle)
          for (byte i = 8; i <= 15; i++)                                                       //Redundance for led activations (intentional)
            digitalWrite(ledStep[i], Step[i].onSequence);
      }
      break;

    case 1:
      if (playButtonState  != playButtonLastState) {
        playButtonLastState = playButtonState;
        playButtonSteps     = 0;
      }
      break;
  }

  return playButtonToogle;
}


void ledOnChangeButton8_16() {
  bool button8_16State = digitalRead(button8_16);

  if (button8_16State  != button8_16LastState) {                                          //Line executed just once
    button8_16LastState = button8_16State;
    if (button8_16State)
      for (byte i = 0; i <= 7; i++)                                                       //Redundance for led activations (intentional)
        digitalWrite(ledStep[i], Step[i].onSequence);
    else
      for (byte i = 8; i <= 15; i++)                                                      //Redundance for led activations (intentional)
        digitalWrite(ledStep[i], Step[i].onSequence);
  }
}


uint8_t buttonsRead() {
  unsigned int analogReading  = analogRead(buttonsAnalogIn);
  bool         digitalReading = digitalRead(button8_16);

  switch (analogReadingStep) {
    case Step1:
      if (analogReading >= 980) {
        debouncingTime = 0;
        return none;
      }

      if (analogReading < 960 && debouncingTime > 40) {
        analogReadingStep = Step2;


        if ((analogReading > 90) && (analogReading < 150)) {
          if (digitalReading) {
            buttonPressed = Step1;
            return Step1;
          }
          else {
            buttonPressed = Step9;
            return Step9;
          }
        }

        if ((analogReading > 210) && (analogReading < 270)) {
          if (digitalReading) {
            buttonPressed = Step2;
            return Step2;
          }
          else {
            buttonPressed = Step10;
            return Step10;
          }
        }

        if ((analogReading > 320) && (analogReading < 380)) {
          if (digitalReading) {
            buttonPressed = Step3;
            return Step3;
          }
          else {
            buttonPressed = Step11;
            return Step11;
          }
        }

        if ((analogReading > 440) && (analogReading < 500)) {
          if (digitalReading) {
            buttonPressed = Step4;
            return Step4;
          }
          else {
            buttonPressed = Step12;
            return Step12;
          }
        }

        if ((analogReading > 550) && (analogReading < 610)) {
          if (digitalReading) {
            buttonPressed = Step5;
            return Step5;
          }
          else {
            buttonPressed = Step13;
            return Step13;
          }
        }

        if ((analogReading > 660) && (analogReading < 720)) {
          if (digitalReading) {
            buttonPressed = Step6;
            return Step6;
          }
          else {
            buttonPressed = Step14;
            return Step14;
          }
        }

        if ((analogReading > 770) && (analogReading < 830)) {
          if (digitalReading) {
            buttonPressed = Step7;
            return Step7;
          }
          else {
            buttonPressed = Step15;
            return Step15;
          }
        }

        if ((analogReading > 880) && (analogReading < 940)) {
          if (digitalReading) {
            buttonPressed = Step8;
            return Step8;
          }
          else {
            buttonPressed = Step16;
            return Step16;
          }
        }
      }
      break;

    case Step2:
      if (analogReading >= 980) {
        analogReadingStep  = Step1;
        buttonStillPressed = false;
        buttonPressed      = none;
      }
      return buttonPressed;
      break;
  }
}


PlayNotes::PlayNotes(uint8_t encoderCLK, uint8_t encoderDT, uint8_t encoderSW) {
  this->encoderCLK   = encoderCLK;
  this->encoderDT    = encoderDT;
  this->encoderSW    = encoderSW;
  this->encState     = digitalRead(encoderCLK);
  this->encLastState = this->encState;
}

void PlayNotes::PlayNote(uint8_t Note, unsigned int Tempo) {
  this->Tempo  = Tempo;
  this->Note   = Note;
  this->Timer  = 0;
  this->PlayingNote  = 1;
}

void PlayNotes::Management() {
  switch (this->PlayingNote) {
    case 1:
      Keyboard.release(Keys[this->lastNote]);
      Keyboard.press(Keys[this->Note]);
      this->lastNote = this->Note;
      this->PlayingNote = 2;
      break;
    case 2:
      unsigned int Tempo = this->Tempo;
      if (this->Timer > Tempo) {
        this->PlayingNote = 0;
        Keyboard.release(Keys[this->Note]);
      }
      break;
  }
}

uint8_t PlayNotes::keyAssignment(uint8_t StepSequence, uint8_t keyNumber, unsigned int Tempo) {

  if (!digitalRead(encoderSW)){
    saveProgramChanged();
    return 0;
  }

  switch (encoderRead()) {
    case encoderCW:
      if (keyNumber >= sizeof(Keys) - 1)
        keyNumber = 1;
      else
        keyNumber++;

      playKey1.PlayNote(keyNumber, Tempo);                        ///////////////////////////////////CHECAR
      saveProgramChanged();
      break;

    case encoderCCW:
      if (keyNumber <= 1)
        keyNumber = sizeof(Keys) - 1;
      else
        keyNumber--;

      playKey1.PlayNote(keyNumber, Tempo);                        ///////////////////////////////////CHECAR
      saveProgramChanged();
      break;

    case encoderUnchanged:
      break;
  }

  return keyNumber;
}

uint8_t PlayNotes::encoderRead() {
  encState = digitalRead(encoderCLK);

  if (encState != encLastState) {
    encLastState = encState;
    if (digitalRead(encoderDT) != encState)
      return encoderCCW;
    else
      return encoderCW;
  }
  else
    return encoderUnchanged;
}

void saveProgramChanged() {
  programChanged  = true;
  programSaveTime = 0;
}

void saveProgram() {
  if (programChanged == true)
    if (programSaveTime > 13000) {
      for (int i = 0; i < numberOfSteps; i++)
        EEPROM.update(i, Step[i].onSequence);
      for (int i = 0; i < numberOfSteps; i++)
        EEPROM.update(i + numberOfSteps, Step[i].keyNumber1);
      for (int i = 0; i < numberOfSteps; i++)
        EEPROM.update(i + numberOfSteps * 2, Step[i].keyNumber2);
      programChanged = false;
    }
}

void readProgram() {
  for (int i = 0; i < numberOfSteps; i++)
    Step[i].onSequence = EEPROM.read(i);
  for (int i = 0; i < numberOfSteps; i++) {
    Step[i].keyNumber1 = EEPROM.read(i + numberOfSteps);
    if (Step[i].keyNumber1 > sizeof(Keys))
      Step[i].keyNumber1 = i + 11;
  }
  for (int i = 0; i < numberOfSteps; i++) {
    Step[i].keyNumber2 = EEPROM.read(i + numberOfSteps * 2);
    if (Step[i].keyNumber2 > sizeof(Keys))
      Step[i].keyNumber2 = 0;
  }
}
