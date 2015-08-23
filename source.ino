//Provides appropriate NOTE_ constants; from Arduino example projects.
#include "pitches.h"

//Arduino Pins
const int INPUT1 = A0;
const int INPUT2 = A1;
const int OUTPUT1 = 5;
const int OUTPUT2 = 9;
const int PULSE_IN = 0;
const int MUSIC_OUT = 1;

//Mode constants
const int S_INITIALIZATION = 1;
const int S_COORDINATION = 2;
const int S_PERFORMANCE = 3;

//Message Constants
const int M_REJECT = 170;
const int M_CONFIRM = 200;
const int MESSAGE_TOLERANCE = 15;
const int POSITION_VALS[] = {20,50,80,110,140};
const int POSITION_TYPES = 5;

//Musical Role Constants
const int P_NONE = 0;
const int P_SOPRANO = 1;
const int P_ALTO = 2;
const int P_TENOR = 3;
const int P_BASS = 4;
const int P_PERCUSSION = 5;

//Performance Mode Constants
const int PMODE_PLAYING = 1;
const int PMODE_WAITING = 2;

//Environment Info
int numAgents = 3;

//Proportion information to determine how we choose a role. Here, we want one agent each for
//soprano, bass, and percussion.
int propTable[] = {1,0,0,1,1};
int propThreshold = 0; //In the demo, no tolerance is allowed in choosing roles.

// Notes in the score; used in performance mode. For this demo, alto score is
// the same as soprano and tenor is the same as bass.
int sopranoScore[] = {
  NOTE_E4, NOTE_D4, NOTE_C4, NOTE_B3, NOTE_A3, NOTE_G3, NOTE_A3, NOTE_B3};
int bassScore[] = {
  NOTE_C4, NOTE_G3, NOTE_A3, NOTE_E3, NOTE_F3, NOTE_C3, NOTE_F3, NOTE_G3};
int percussionWeights[] = {NOTE_D3,NOTE_D2,NOTE_G2,NOTE_D2};

//Timing values used in performance mode.
int tonePulses = 32;
int waitPulses = 8;
int percussionHitPulses = 2;
int percussionWaitPulses = 8;
int performancePulses = 0;
int currentTone = 0;

//A timer is used to skip a pulse before and after broadcasting a message.
//This ensures that we won't read in our own message as a new input.
int broadcastMessageTimer = 0;
const int BROADCAST_TICKS = 3;
int broadcastMessage[] = {0,0};

//Received message data structures
int thisPulseInput[] = {0, 0};
int mostRecentMessage[] = {0, 0};
const int noiseLevel = 5; //Around 5 units of noise generally present in received messages.
const float inputDivision = 4.0117647058823529411764705882353; //The value of 1023/255

//Some pulses are initially skipped to allow agents lower in the hierarchy to prepare to
//receive the top agent's initial message.
int skipPulses = 3+numAgents-myID;

//Values used to keep backtracking position; these track whose turn it is, and when we should move.
int whoseTurn = 0;
boolean rejectTable[] = {false,false,false,false,false};
int pulseVal = LOW;

//These values may change for each agent in this demo application, so they've been separated out.
int prefTable[] = {0,0,0,0,9}; //Values in range 1-10; 0 means exclude completely
int myID = 2;
int viewTable[] = {P_NONE,P_NONE,P_NONE};
int stage = S_COORDINATION;
int performanceMode = PMODE_PLAYING;

void setup()
{
  pinMode(INPUT1, INPUT);
  pinMode(INPUT2, INPUT);
  pinMode(OUTPUT1, OUTPUT);
  pinMode(OUTPUT2, OUTPUT);
  pinMode(PULSE_IN, INPUT);
  pinMode(MUSIC_OUT, OUTPUT);
  analogWrite(OUTPUT1,0);
  analogWrite(OUTPUT2,0);
  analogWrite(MUSIC_OUT,0);
}

void loop()
{
  int newPulseVal = digitalRead(PULSE_IN);
  if(pulseVal != newPulseVal)
  {
   pulseVal = newPulseVal;
   if(skipPulses > 0) skipPulses--;
   else
   {
    if(stage == S_INITIALIZATION)
    {
      //Nothing for this implementation... values are provided at initialization.
      delay(10);
    }
    else if(stage == S_COORDINATION)
    {
      if(broadcastMessageTimer == 2)
      {
        if(thisPulseInput[0] <= 5) //Broadcast only if no-one else already is.
        {
          broadcastMessageTimer--;
          analogWrite(OUTPUT1, broadcastMessage[0]);
          analogWrite(OUTPUT2, broadcastMessage[1]);
          if(broadcastMessage[1] == M_CONFIRM)
            stage = S_PERFORMANCE; //wait until we've sent it!
        }
      }
      else if(broadcastMessageTimer == 1 || broadcastMessageTimer == 3)
      {
        analogWrite(OUTPUT1, 0);
        analogWrite(OUTPUT2, 0);
        broadcastMessageTimer--;
      }
      else
      {
        analogWrite(OUTPUT1, 0);
        analogWrite(OUTPUT2, 0);
        thisPulseInput[0] = analogRead(INPUT1);
        thisPulseInput[0] = (thisPulseInput[0]-noiseLevel)/inputDivision;
        thisPulseInput[1] = analogRead(INPUT2);
        thisPulseInput[1] = (thisPulseInput[1]-noiseLevel)/inputDivision;
     
        if(thisPulseInput[1] > POSITION_VALS[0] - MESSAGE_TOLERANCE) //A message
        {
          mostRecentMessage[0] = thisPulseInput[0];
          mostRecentMessage[1] = thisPulseInput[1];
        }
        
        if(thisPulseInput[1] > 5) //Got a message (ignore noise)
        {
          if(whoseTurn < myID) //Higher-priority agent acting
          {
              //Choice message
              if(thisPulseInput[1] > POSITION_VALS[0]-MESSAGE_TOLERANCE &&
                 thisPulseInput[1] < POSITION_VALS[4]+MESSAGE_TOLERANCE)
              {
                viewTable[whoseTurn] = interpretChoiceMessage(thisPulseInput[1]);
                whoseTurn++;
              }
              else if(thisPulseInput[1] > M_REJECT - MESSAGE_TOLERANCE &&
                      thisPulseInput[1] < M_REJECT + MESSAGE_TOLERANCE)
              {
                whoseTurn--;
                viewTable[whoseTurn] = P_NONE;
              }
          }
          else if (whoseTurn > myID) //Lower-priority agent
          {
            if(thisPulseInput[1] > M_REJECT - MESSAGE_TOLERANCE &&
               thisPulseInput[1] < M_REJECT + MESSAGE_TOLERANCE)
            {
              if(whoseTurn == myID+1)
                rejectTable[viewTable[myID]-1] = true;
              whoseTurn--;
            }
            else if (thisPulseInput[1] > POSITION_VALS[0] - MESSAGE_TOLERANCE &&
                     thisPulseInput[1] < POSITION_VALS[4] + MESSAGE_TOLERANCE)
            {
              viewTable[whoseTurn] = interpretChoiceMessage(thisPulseInput[1]);
              whoseTurn++;
            }
            else if (thisPulseInput[1] > M_CONFIRM - MESSAGE_TOLERANCE &&
                     thisPulseInput[1] < M_CONFIRM + MESSAGE_TOLERANCE)
            {
              stage = S_PERFORMANCE;
            }
          }
        }
        if(whoseTurn == myID)
        {
          viewTable[myID] = chooseBestValue();
          if(viewTable[myID] == P_NONE) //failure
          {
            whoseTurn--;
            broadcast(M_REJECT);
            for(int i = 0; i < sizeof(rejectTable)/sizeof(int); i++)
              rejectTable[i] = false;
          }
          else //success
          {
            if(myID == numAgents-1)
            {
              broadcast(M_CONFIRM);
            }
            else
            {
              broadcast(POSITION_VALS[0]);
              whoseTurn++;
            }
          }
        }
      }
    }
    else //stage == S_PERFORMANCE
    {
      analogWrite(OUTPUT1, 0);
      analogWrite(OUTPUT2, 0); 
      if(viewTable[myID] == P_PERCUSSION)
      {
        playPercussion();
      }
      else if(viewTable[myID] != P_NONE &&
              viewTable[myID] != P_PERCUSSION &&
              currentTone < sizeof(sopranoScore)/sizeof(int))
      {
        playNote();
      }
      delay(10);
    }
   }
 }
 delay(3);
}

void broadcast(int messageType)
{
  if(broadcastMessageTimer == 0)
  {
    broadcastMessageTimer = BROADCAST_TICKS;
    broadcastMessage[0] = myID;
    if(messageType == POSITION_VALS[0])
      broadcastMessage[1] = POSITION_VALS[viewTable[myID]-1];
    else if(messageType == M_CONFIRM)
      broadcastMessage[1] = M_CONFIRM;
    else if (messageType == M_REJECT)
      broadcastMessage[1] = M_REJECT;
  }
}
int interpretChoiceMessage(int messageVal)
{
  if(messageVal < POSITION_VALS[0]-MESSAGE_TOLERANCE || messageVal > POSITION_VALS[4]+MESSAGE_TOLERANCE)
    return P_NONE;
  else if (messageVal < POSITION_VALS[1]-MESSAGE_TOLERANCE)
    return P_SOPRANO;
  else if (messageVal < POSITION_VALS[2]-MESSAGE_TOLERANCE)
    return P_ALTO;
  else if (messageVal < POSITION_VALS[3]-MESSAGE_TOLERANCE)
    return P_TENOR;
  else if (messageVal < POSITION_VALS[4]-MESSAGE_TOLERANCE)
    return P_BASS;
  else// if (messageVal <= POSITION_VALS[4]+MESSAGE_TOLERANCE)
    return P_PERCUSSION;
}
int chooseBestValue() //Use prefTable, viewTable, rejectTable
{
  int spotsTable[] = {propTable[0],propTable[1],propTable[2],propTable[3],propTable[4]};
  for(int i = 0; i < myID; i++) spotsTable[viewTable[i]-1] -= 1;
  
  int choiceTable[] = {prefTable[0],prefTable[1],prefTable[2],prefTable[3],prefTable[4]}; 
  
  int maxIdx = 0;
  int maxVal = 0;
  for(int i = 0; i < 5; i++)
  {
    if(spotsTable[i] <= 0 || rejectTable[i] == true)
      choiceTable[i] = 0;
    else if (choiceTable[i] > maxVal)
    {
      maxIdx = i;
      maxVal = choiceTable[i];
    }
  }
  
  if(maxVal == 0) return P_NONE;
  else if(maxIdx == 0) return P_SOPRANO;
  else if(maxIdx == 1) return P_ALTO; 
  else if(maxIdx == 2) return P_TENOR;
  else if(maxIdx == 3) return P_BASS;
  else if(maxIdx == 4) return P_PERCUSSION;
  else return P_NONE;
}
void playNote()
{
  performancePulses++;
  if(performanceMode == PMODE_PLAYING && performancePulses >= tonePulses)
  {
    noTone(MUSIC_OUT);
    performanceMode = PMODE_WAITING;
    performancePulses = 0;
    currentTone++;
  }
  else if (performanceMode == PMODE_WAITING && performancePulses >= waitPulses)
  {
    int nextNote = 0;
    if(viewTable[myID]==P_SOPRANO)
      nextNote = sopranoScore[currentTone];
    else if (viewTable[myID]==P_ALTO) //tones not implemented here
      nextNote = sopranoScore[currentTone];
    else if (viewTable[myID]==P_TENOR) //tones not implemented here
      nextNote = bassScore[currentTone];
    else if (viewTable[myID]==P_BASS)
      nextNote = bassScore[currentTone];

    tone(MUSIC_OUT, nextNote, 10000);
    performanceMode = PMODE_PLAYING;
    performancePulses = 0;
  }
}
void playPercussion()
{
  performancePulses++;
  if(performanceMode == PMODE_PLAYING && performancePulses >= percussionHitPulses)
  {
    noTone(MUSIC_OUT);
    performanceMode = PMODE_WAITING;
    performancePulses = 0;
    currentTone = (currentTone+1)%4;
  }
  else if (performanceMode == PMODE_WAITING && performancePulses >= percussionWaitPulses)
  {
    tone(MUSIC_OUT, percussionWeights[currentTone], 10000);
    performanceMode = PMODE_PLAYING;
    performancePulses = 0;
  }
}