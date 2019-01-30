/*
 * This program keeps track of real time and allows a user to program
 * the current time through use of buttons put into an Arduino.
 * The program uses a LCD screen to display time and display when the
 * time food will be dropped. This current version does have a few bugs.
 * I will keep updating the versions as I fix bugs with some documentation
 * on changes. For now there is no function for actually dropping the food
 * as I have no motor choosen. I plan on getting an angular encoder to 
 * keep track of motor position, so that will be coming soon.
 * 
 * Version: 1
 * Name: Matthew Braniff
 * About: I am a junior transfer student at University of Illinois at Chicago
 * studying Electrical Engineering. I am making this project because
 * I find that I am not home very often and feel bad that my cat gets fed
 * early in the morning and late at night so I wanted to make a automated
 * cat feeder. This project is being supported through UIC's IEEE student
 * organization.
 * 
 * Date: 01/30/2019
 */


/* 
 * Here is the site where I found information on the LCD I am using 
 * for this project

//Website:www.sunfounder.com

/********************************/
// include the library code
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
 #include <stdarg.h>

//These are constants for the blink time when changing a value
const int BLINK_TIME = 30;
const int BLINK_INT = BLINK_TIME/2;

//These are the pins used for change and increase buttons
int changePin = 13;
int incPin = 12;

//The bools hold information if the buttons are released or not
bool changeRelease = true;
bool incRelease = true;
int changeCounter = 0; //Counts how many times change button is hit

//Hours, minutes, seconds
int h;
int m;
int s;

/* This is a 2D array of feed times, currently holding 1 feed time
 *  at 01:01:00 AM for testing but this will be fully programmable  
 *  when the clock is first turned on
 */
int feedTimes[][4] = {1,1,0,0}; 

//All the bools here decide if I am editing a time or if I am keeping track of time
bool counting;
bool editHours;
bool editMins;
bool editSecs;
bool editInd;

//What actually keeps track of blink time
int blinkTime;

//Holds our last reading of milliseconds so we can properly keep track of time
unsigned long previousMillis = 0;

//This is the indicator variable, A for AM and P for PM
char ind = 'A';

LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address to 0x27 for a 16 chars and 2 line display

/*
 * This function calculates feed time and changes the incoming
 * string to when the next feed time is going to be.
 * For now it only supports the first element of the 2D array
 */
void calcFeedTime(char feedTime[]){
  int dropHours; //What hour it food should drop
  int currHours; //Current hour reading
  int hDiff; //Difference in hours

  if(feedTimes[0][3] == 1){   //If our indicator value is 1 that means PM
    if(feedTimes[0][0] = 12){ //If its 12PM drop hour is 12
      dropHours = 12;
    }else{                    //Else add 12 to hour reading
      dropHours = feedTimes[0][0] + 12;
    }
  }else{ //If its AM
    if(feedTimes[0][0] == 12){//If its 12AM drop hour is 0
      dropHours = 0;
    }else{                    //Else drop hour is just the reading
      dropHours = feedTimes[0][0];
    }
  }
  
  //Same thing for current hours just using character indicators
  if(ind == 'P'){
    if(h == 12){
      currHours = 12;
    }else{
      currHours = h + 12;
    }
  }else{
    if(h == 12){
      currHours = 0;
    }else{
      currHours = h;
    }
  }

  
  hDiff = dropHours - currHours; //Calculate hour difference
  
  if(hDiff > 2){
    sprintf(feedTime,"%2dH",hDiff); //If difference is > 2 print the difference
    return;
  }else if(hDiff<0){
    hDiff = 24 + hDiff; //If difference is negative add 24
    if(hDiff == 2){
      int mDiff = (feedTimes[0][1] + 60)-m; //If hour difference is 2 see if min diff is > 60
      if(mDiff > 60){
        sprintf(feedTime,"%2dH",hDiff); //If it is then print hDiff (2)
        return;
      }else{
        sprintf(feedTime,">1H"); //If not then print >1 hour (say 1H59M)
        return;
      }
    }
  }else{ //If Hour Difference >= 0 and <= 2
    int mDiff; // Minute difference
    if(hDiff == 1){ //If hour Difference is 1
      mDiff = (feedTimes[0][1]+60)-m; //Add an hour to feed time and subract current minutes
      if(mDiff > 60){ //If min Difference is over an hour
        sprintf(feedTime,">%dH",hDiff); //Print >1 hour
        return;
      }else{
        sprintf(feedTime,"%2dM",mDiff); //Print minute difference (60 + a few seconds)
        return;
      }
    }else{ //If hour difference is 0
      mDiff = feedTimes[0][1]-m; // Set minute difference
      if(mDiff > 1){
        sprintf(feedTime,"%2dM",mDiff); //If min difference is > 1 print the difference
        return;
      }else if(mDiff < 0){
        sprintf(feedTime,"24H"); //If min Difference is negative print 24H
        return;
      }else{ //If min difference is 0 or 1
        int sDiff; //Second difference
        if(mDiff > 0){ 
          sDiff = (feedTimes[0][2]+60) - s; //If min diff = 1 add 60 to feed time seconds
          if(sDiff > 60){
            sprintf(feedTime,">%dM",mDiff); //If second diff > 60 print ">1M"
            return;
          }else{
            sprintf(feedTime,"%2dS",sDiff); //Else print second difference
            return;
          }
        }else{ //If min difference is 0
          sDiff = (feedTimes[0][2] - s); //Find second difference
          if(sDiff > 0){
            sprintf(feedTime,"%2dS",sDiff); //If second diff is > 0 print it
            return;
          }else{
            sprintf(feedTime,"24H"); //Else print 24H
            return;
          } //End of second else
        } //End of min Diff 0 else
      } //End of min Diff is 0 or 1 else
    } //End of hour Diff is 0
  } //End of hour Diff >= 0 and <=2
} //End of calcFeedTime function

//This function prints the display to the LCD screen and manages blink times
void printTime(char hours[],char mins[],char secs[],char indicator[]){
  lcd.setCursor(3,0); //Set lcd cursor to column 3 row 0
  //If not editing hours or if blinkTime > blink interval or if increase button not released
  //Print hours string
  if(!editHours || blinkTime>BLINK_INT || !incRelease){
    lcd.print(hours);
    if(editHours){
      blinkTime--; //If editing hours reduce blinkTime counter
    }
  }else{ //If blinkTime counter < blink interval and edit hours true and increase button released
    lcd.print("  "); //Print spaces for hours spot
    blinkTime--; //Reduce blinkTime counter
    if(blinkTime < 0){ //If blinkTime counter is negative
      blinkTime = BLINK_TIME; //Reset blinkTime counter
    }
  }
  
  lcd.print(":");

  //Same thing as hours just for minutes
  if(!editMins || blinkTime>BLINK_INT || !incRelease){
    lcd.print(mins);
    if(editMins){
      blinkTime--;
    }
  }else{
    lcd.print("  ");
    blinkTime--;
    if(blinkTime < 0){
      blinkTime = BLINK_TIME;
    }
  }
  
  lcd.print(":");

  //Same thing as hours just for seconds
  if(!editSecs || blinkTime>BLINK_INT || !incRelease){ 
    lcd.print(secs);
    if(editSecs){
      blinkTime--;
    }
  }else{
    lcd.print("  ");
    blinkTime--;
    if(blinkTime < 0){
      blinkTime = BLINK_TIME;
    }
  }
  
  lcd.print(" ");

  //Same thing as hours just for indicator
  if(!editInd || blinkTime>BLINK_INT || !incRelease){ 
    lcd.print(indicator);
    if(editInd){
      blinkTime--;
    }
  }else{
    lcd.print(" ");
    blinkTime--;
    if(blinkTime < 0){
      blinkTime = BLINK_TIME;
    }
  }
  
  lcd.print("M"); //End of AM or PM

  //If editing time print on bottom line what you are editing
  if(editHours){
    lcd.setCursor(2,1);
    lcd.print("Change Hours");
  }else if(editMins){
    lcd.setCursor(3,1);
    lcd.print("Change Mins");
  }else if(editSecs){
    lcd.setCursor(3,1);
    lcd.print("Change Secs");
  }else if(editInd){
    lcd.setCursor(3,1);
    lcd.print("Change ind");
  }else{
    //If not editing time print when next feed is
    lcd.setCursor(1,1);
    lcd.print("Next Feed: ");
    
    char feedTime[4];
    calcFeedTime(feedTime); //Call calcFeedTime and get when next feed is
    
    lcd.print(feedTime);
  }
}

void setup()
{
  Serial.begin(9600); //For testing with Serial output
  
  pinMode(changePin, OUTPUT); //ChangePin is an output
  pinMode(incPin, OUTPUT); //incPin is an output

  //Both change and increase pins send out high voltage
  digitalWrite(changePin,HIGH);
  digitalWrite(incPin,HIGH);

  //Clock starts counting
  counting = true;
  editHours = false;
  editMins = false;
  editSecs = false;
  editInd = false;

  blinkTime = BLINK_TIME; //Initialize blinkTime counter

  //Clock initializes at 12 AM
  h = 12;
  m = 00;
  s = 00;
  ind = 'A';
  
  lcd.init(); //initialize the lcd
  lcd.backlight(); //open the backlight 
}


void loop() 
{
  int val = digitalRead(changePin); //Get the value of Change pin
  if(changeRelease){ //If change button is released (to avoid holding a button and flipping settings too quickly)
    if(val == LOW){ //If value is LOW (button pressed)
      switch(changeCounter){ //Switch checking change button counter
        case 0: //If first time pressing change button
          changeRelease = false; //Button is down
          counting = false; //Stop tracking time
          editHours = true; //Begin editing hours
          changeCounter++; //Add 1 to change button counter
          lcd.clear(); //Clear LCD screen
          break;
        case 1:
          changeRelease = false;
          editHours = false;
          editMins = true; //Begin editing minutes
          changeCounter++;
          lcd.clear();
          break;
        case 2:
          changeRelease = false;
          editMins = false;
          editSecs = true; //Begin editing seconds
          changeCounter++;
          lcd.clear();
          break;
        case 3:
          changeRelease = false;
          editSecs = false;
          editInd = true; //Begin editing indicator
          changeCounter ++;
          lcd.clear();
          break;
        case 4: //If change button pressed a fifth time begin tracking time again
          changeRelease = false;
          editInd = false;
          counting = true;
          changeCounter = 0;
          lcd.clear();
          break;
      }
    }
  }else{
    if(val == HIGH){ //Checks if the button is released
      changeRelease = true;
    }
  }
  if(counting){ //If we are tracking time
    unsigned long currentMillis = millis(); //Get current milliseconds
    if(currentMillis - previousMillis >= 1000){ //If current milliseconds - previous >= 1000 (1 second)
      s++; //Add 1 to seconds
      previousMillis = currentMillis; 
      if(s == 60){ //If seconds is 60
        m++; //Add 1 to mins
        s=0; //Reset seconds
        if(m ==60){ //If mins is 60
          h++; //Add 1 to hours
          m=0; //Reset mins
          if(h == 12){ //If hour is 12
            if(ind == 'A'){ //If indicator is A
              ind = 'P'; //Change to P
            }else{ //If indicator is P
              ind = 'A'; //Change to A
            }
          }
          if(h == 13){//If hour is 13 reset back to 1
            h = 1;
          }
        }
      }
    }else if(currentMillis - previousMillis < 0){
      previousMillis = 0; //If there is overflow with millis() function reset previous to 0
    } //This allows clock to run as long as the battery survives without it overflowing
  }else if(editHours){ //If we are editing time
    val = digitalRead(incPin); //Record value of Increase button
    if(incRelease){ //If increase button is released
      if(val == LOW){ //If value reads LOW (button pressed)
        incRelease = false; //increase button is down
        h++; //Add to hour
        if(h > 12){ // if hour exceeds 12 reset to 1
          h = 1;
        }
      }
    }else{
      if(val == HIGH){//Checks if increase button is released
        incRelease = true;
      }
    }
  }else if(editMins){ //Same this as hours just for minutes
    val = digitalRead(incPin);
    if(incRelease){
      if(val == LOW){
        incRelease = false;
        m++;
        if(m > 59){ //If mins exceeds 59 reset to 0
          m = 0;
        }
      }
    }else{
      if(val == HIGH){
        incRelease = true;
      }
    }
  }else if(editSecs){//Same thing as hours just for seconds
    val = digitalRead(incPin);
    if(incRelease){
      if(val == LOW){
        incRelease = false;
        s++;
        if(s > 59){
          s = 0;
        }
      }
    }else{
      if(val == HIGH){
        incRelease = true;
      }
    }
  }else if(editInd){ //Finally same this as hours just for indicator
    val = digitalRead(incPin);
    if(incRelease){
      if(val == LOW){
        incRelease = false;
        if(ind == 'A'){
          ind = 'P';
        }else{
          ind = 'A';
        }
      }
    }else{
      if(val == HIGH){
        incRelease = true;
      }
    }
  }

  //Strings for hours, mins, etc. to make printing to LCD easier
  char hours[3];
  char mins[3];
  char secs[3];
  char indicator[2];
  //Format 2 spaces where empty spaces are filled with 0s for each variable
  sprintf(hours,"%02d",h)
  sprintf(mins,"%02d",m);
  sprintf(secs,"%02d",s);
  sprintf(indicator,"%c",ind);
  //Call Print time to print to LCD
  printTime(hours,mins,secs,indicator);

}
