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
 * Version: 1.2
 * Name: Matthew Braniff
 * About: I am a junior transfer student at University of Illinois at Chicago
 * studying Electrical Engineering. I am making this project because
 * I find that I am not home very often and feel bad that my cat gets fed
 * early in the morning and late at night so I wanted to make a automated
 * cat feeder. This project is being supported through UIC's IEEE student
 * organization.
 * 
 * Date: 02/03/2019
 * 
 * Change Log:
 * Added EEPROM memory support to store values and load them if power is ever lost
 * calcFeedTime now supports any value in feedTimes array using dropPosition.
 * Added two helper functions that check if a button is pressed.
 * Added sorting functions to sort feedTimes array from least to greatest
 * Added a convert times function that converts feedTimes array to time in seconds
 * Added function that can find closest feedTime and return its index
 * Added function to fill feedTime array from EEPROM memory
 * Added functionality to change feedTimes by holding down increase button
 * 
 * This change list looks small but this is the largest update I have had!
 * So many new cool functionalities have been added so its worth reading through the comments!
 */


/* 
 * Here is the site where I found information on the LCD I am using I2C LCD1602
 * for this project
 * Website:www.sunfounder.com
/********************************/
// include the library code
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
 #include <stdarg.h>
 #include <EEPROM.h>

/*
 * Information on EEPROM key address points:
 * EEPROM_START holds the value 255 if the adruino has booted without initializing how many drop times else it reads 0
 * EEPROM_START+1 holds the value of how many drop times are in the system
 * EEPROM_START+2 only has value 0 if user has completed inputting all drop times
 * EEPROM_START+3 to 1023 hold all values of drop times. Each address can hold 1 byte so... 
 * it takes 4 addresses to fill in 1 full drop time.
 */


//The starting address I choose to keep data in the EEPROM memory
 const int EEPROM_START = 981;
 int EEPROM_Position = EEPROM_START+3; //Memory address where I begin storing feed time data

 char dropCount = 1; //How many drop times there are
 char dropPosition = 0; //The current drop time being approached

//These are constants for the blink time when changing a value
const char BLINK_TIME = 30;
const char BLINK_INT = BLINK_TIME/2;
const int MILLISPEED = 1000; //This is for testing, this should be at 1000 for normal time tracking

//These are the pins used for change and increase buttons
char changePin = 13;
char incPin = 12;

//The bools hold information if the buttons are released or not
bool changeRelease = true;
bool incRelease = true;
char changeCounter = 0; //Counts how many times change button is hit

//Hours, minutes, seconds
char h;
char m;
char s;

/* This is a 2D array of feed times, currently holding 1 feed time
 *  at 01:01:00 AM for testing but this will be fully programmable  
 *  when the clock is first turned on
 */

//All the bools here decide if I am editing a time or if I am keeping track of time
bool counting;
bool editHours;
bool editMins;
bool editSecs;
bool editInd;
bool editDrops; //Editing drop times bool
bool editDropChoice = false; //Bool used when choosing to confirm drop time edit
bool incomplete = false; //Bool used if EEPROM data is read in but is incomplete
bool increased = false; //Bool used to check if dropPosition has been increased already
//A note for above, dropPosition increases if a drop time is met say its 8:00 AM but these arduino could read
//the value 8 AM multiple times thus increasing the position multiple times. This prevents that.
bool backLight; //This tells us if backlight is on or off

//What actually keeps track of blink time
char blinkTime;

//Holds our last reading of milliseconds so we can properly keep track of time
unsigned long previousMillis = 0;
unsigned long incHeldStart = 0;

//This is the indicator variable, A for AM and P for PM
char ind = 'A';

LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address to 0x27 for a 16 chars and 2 line display

/**************************************************************************************************/
//This is the struct for feedTime, it holds an hour, min, second and indicator
typedef struct feedTime{
  char h;
  char m;
  char s;
  char ind;
}struct_feedTime;

feedTime feedTimes[10]; //Defining our array of structs (max ten)

//This is the constructor for feedTime, it just fills in the values from parameters and returns it.
feedTime initFeedTime(int h,int m,int s,char ind){
  feedTime temp;
  temp.h = h;
  temp.m = m;
  temp.s = s;
  temp.ind = ind;
  return temp;
}

/**************************************************************************************************/

//Helper function that checks if increase button is pressed. If it is, then it returns true one time until its released
bool isIncPressed(){
  int val = digitalRead(incPin); //Read incPin value
  if (incRelease){ //If the increase button is not currently pressed
    if(val == LOW){ //If the value reads LOW (Button is pressed)
      incRelease = false; //Flip incRelease bool
      return true; //Return true (button is pressed)
    }
  }else if(val == HIGH){ //Recognize this is an else if from (incRelease). If button previously was 
    incRelease = true;   //Pressed and reads HIGH (Is up) then say button is released and return false
    return false;
  }
  return false; //Return false if nothing else
}

/**************************************************************************************************/

//Helper function that checks if change button is pressed. If it is, then it returns true one time until its released
bool isChangePressed(){
  int val = digitalRead(changePin);
  if (changeRelease){
    if(val == LOW){
      changeRelease = false;
      return true;
    }
  }else if(val == HIGH){
    changeRelease = true;
    return false;
  }
  return false;
}

/**************************************************************************************************/

/*
 * This function calculates feed time and changes the incoming
 * string to when the next feed time is going to be.
 */
void calcFeedTime(char feedTime[]){
  int dropHours; //What hour the food should drop
  int currHours; //Current hour reading
  int hDiff;     //Difference in hours

  if(feedTimes[dropPosition].ind == 'P'){ //If our indicator value is P
    if(feedTimes[dropPosition].h == 12){  //If its 12PM drop hour is 12
      dropHours = 12;
    }else{                                //Else add 12 to hour reading
      dropHours = feedTimes[dropPosition].h + 12;
    }
  }else{                                  //If its AM
    if(feedTimes[dropPosition].h == 12){  //If its 12AM drop hour is 0
      dropHours = 0;
    }else{                                //Else drop hour is just the reading
      dropHours = feedTimes[dropPosition].h;
    }
  }
  
  //Same thing for current hours as above
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

  
  hDiff = dropHours - currHours;    //Calculate hour difference

  //This gets into real nitty gritty for very specific cases
  if(hDiff > 2){
    sprintf(feedTime,"%2dH",hDiff); //If difference is > 2 print the difference
    return;
  }else if(hDiff<0){
    hDiff = 24 + hDiff;             //If difference is negative add 24
    if(hDiff == 2){
      int mDiff = (feedTimes[dropPosition].m + 60)-m; //If hour difference is 2 see if min diff is > 60
      if(mDiff > 60){
        sprintf(feedTime,"%2dH",hDiff); //If it is then print hDiff (2)
        return;
      }else{
        sprintf(feedTime,">1H"); //If not then print >1 hour (say 1H59M)
        return;
      }
    }else if(hDiff == 1){ //Say its 11 PM or currHour = 23 and drop time is 12AM hDiff = 1 now.
      int mDiff = (feedTimes[dropPosition].m + 60)-m;
     sprintf(feedTime,"%2dM",mDiff); //Print mins until drop
     return; 
    }else{
      sprintf(feedTime,"%2dH",hDiff); //Else print hours to drop
      return; 
    }
  }else{                                        //If Hour Difference >= 0 and <= 2
    int mDiff;                                  // Minute difference
    if(hDiff == 1){                             //If hour Difference is 1
      mDiff = (feedTimes[dropPosition].m+60)-m; //Add an hour to feed time and subract current minutes
      if(mDiff > 60){                           //If min Difference is over an hour
        sprintf(feedTime,">%dH",hDiff);         //Print >1 hour
        return;
      }else if (mDiff > 2){
        sprintf(feedTime,"%2dM",mDiff);         //Print minute difference
        return;
      }else if(mDiff == 2){
        sprintf(feedTime,">1M");
        return;
      }else{                                    //If mDiff == 1
        int sDiff = (feedTimes[dropPosition].s + 60) - s; //Add 60 to feedTime seconds and find difference
        if(sDiff > 60){
          sprintf(feedTime,">1M");
          return;
        }else{
          sprintf(feedTime,"%02dS",sDiff);
          return; 
        }
      }
    }else if(hDiff == 2){
      mDiff = (feedTimes[dropPosition].m + 60)-m; //If hour difference is 2 see if min diff is > 60
      //Essentially this prints 2H for a minute then prints >1H
      if(mDiff > 60){
        sprintf(feedTime,"%2dH",hDiff); //If it is then print hDiff (2)
        return;
      }else{
        sprintf(feedTime,">1H"); //If not then print >1 hour (say 1H59M)
        return;
      }
    }else{//If hour difference is 0
      mDiff = feedTimes[dropPosition].m-m; // Set minute difference
      if(mDiff > 1){
        sprintf(feedTime,"%2dM",mDiff); //If min difference is > 1 print the difference
        return;
      }else if(mDiff < 0){
        sprintf(feedTime,"24H"); //If min Difference is negative print 24H
        return;
      }else{ //If min difference is 0 or 1
        int sDiff; //Second difference
        if(mDiff > 0){ 
          sDiff = (feedTimes[dropPosition].s+60) - s; //If min diff = 1 add 60 to feed time seconds
          if(sDiff > 60){
            sprintf(feedTime,">%dM",mDiff); //If second diff > 60 print ">1M"
            return;
          }else{
            sprintf(feedTime,"%2dS",sDiff); //Else print second difference
            return;
          }
        }else{ //If min difference is 0
          sDiff = (feedTimes[dropPosition].s - s); //Find second difference
          if(sDiff > 0){
            sprintf(feedTime,"%2dS",sDiff); //If second diff is > 0 print it
            return;
//**NEW** }else if(sDiff == 0 && !increased){ //If we have reached our drop time
            dropPosition++;   //Increase dropPosition
            increased = true; //dropPosition has been increased
          }else{
            sprintf(feedTime,"24H"); //Else print 24H
            return;
          } //End of second else
        } //End of min Diff 0 else
      } //End of min Diff is 0 or 1 else
    } //End of hour Diff is 0
  } //End of hour Diff >= 0 and <=2
} //End of calcFeedTime function*/

/**************************************************************************************************/

//This function prints the display to the LCD screen and manages blink times
void printTime(char hours[],char mins[],char secs[]){
  lcd.setCursor(3,0); //Set lcd cursor to column 3 row 0
  /* If not editing hours or if blinkTime > blink interval or if increase button not released
   * print hours string*/
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
    lcd.print(ind);
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
    if(EEPROM.read(EEPROM_START+2) == 255){ //If data has not been saved to EEPROM yet
      //Display Select Hour for slot dropPosition
      char temp[3];
      sprintf(temp,"%2d",dropPosition+1);
      lcd.setCursor(0,1);
      lcd.print("Select Hour")
      lcd.setCursor(12,1);
      lcd.print("SL");  
      lcd.print(temp);
    }else{ //If we aren't inputting drop times
      lcd.setCursor(2,1);
      lcd.print("Change Hours");
    }
  }else if(editMins){/*****************The rest of these are identical to hour just for min, sec and ind*/
    if(EEPROM.read(EEPROM_START+2) == 255){
      char temp[3];
      sprintf(temp,"%2d",dropPosition+1);
      lcd.setCursor(0,1);
      lcd.print("Select Mins");
      lcd.setCursor(12,1);
      lcd.print("SL");
      lcd.print(temp);
    }else{
      lcd.setCursor(3,1);
      lcd.print("Change Mins");
    }
  }else if(editSecs){
    if(EEPROM.read(EEPROM_START+2) == 255){
      char temp[3];
      sprintf(temp,"%2d",dropPosition+1);
      lcd.setCursor(0,1);
      lcd.print("Select Secs");
      lcd.setCursor(12,1);
      lcd.print("SL");
      lcd.print(temp);
    }else{
      lcd.setCursor(3,1);
      lcd.print("Change Secs");
    }
  }else if(editInd){
    if(EEPROM.read(EEPROM_START+2) == 255){
      char temp[3];
      sprintf(temp,"%2d",dropPosition+1);
      lcd.setCursor(0,1);
      lcd.print("Select Ind");
      lcd.setCursor(12,1);
      lcd.print("SL");
      lcd.print(temp);
    }else{
      lcd.setCursor(3,1);
      lcd.print("Change Ind");
    }/************************************************************************************/
  }else if (editDrops){//If user is trying to edit drop hours
    if(isIncPressed()){//If user pressed increase button
      if(editDropChoice){
        editDropChoice = false; //if drop choice is true make it false
      }else{
        editDropChoice = true;  //if drop choice is false make it true
      }
    }
    
    lcd.setCursor(1,1);
    lcd.print("Edit drop? ");
    
    if(editDropChoice){ //If drop choice is true
      if(blinkTime>BLINK_INT*3 || !incRelease){
        lcd.print("Yes"); //Print Yes
        if(incRelease){
          blinkTime--;
        }
      }else{
        lcd.print("   ");
        blinkTime--;
        if(blinkTime<10){
          blinkTime = BLINK_TIME*3;
        }
      }
    }else{
      if(blinkTime>BLINK_INT*3 || !incRelease){
        lcd.print("No");
        if(incRelease){
          blinkTime--;
        }
      }else{
        lcd.print("  ");
        blinkTime--;
        if(blinkTime<10){
          blinkTime = BLINK_TIME*3;
        }
      }
    }
  }else{
    //If not editing time print when next feed is
    lcd.setCursor(1,1);
    lcd.print("Next Feed: ");
    
    char feedTime[4];
    calcFeedTime(feedTime); //Call calcFeedTime and get when next feed is
    
    lcd.print(feedTime);
  }
}

/**************************************************************************************************/

//This is called to initialize the dropCount variable
void initFoodDropCount(){
  lcd.setCursor(0,0);
  lcd.print("How many drops");
  lcd.setCursor(0,1);
  lcd.print("per day? ");
  char dropCountStr[3];
  sprintf(dropCountStr,"%2d",dropCount);
  lcd.print(dropCountStr);
  if(isIncPressed()){ //If incraese button is pressed increase drop count
    dropCount++;
    if(dropCount > 10){//If dropCount is over 10 reset to 1
      dropCount = 1;
    }
  }
  if(isChangePressed()){//If Change button is pressed 
    EEPROM.write(EEPROM_START,0); //Save that dropCount has been initialized in EEPROM
    EEPROM.write(EEPROM_START+1,dropCount); //Save dropCount to EEPROM
    counting = false;
    editHours = true; //Begin inputting drop times
    changeCounter++;
    lcd.clear();
    incomplete = false; //Don't loop this function anymore
  }
}

/**************************************************************************************************/

//This function converts HH:MM:SS IND time to full value in seconds and stores it into a given array
void convertTimes(int Arr[],bool consider){
  int total = 0; //Total time value
  for(int i=0;i<dropCount;i++){
    if(feedTimes[i].ind == 'P'){ //If its PM
      if(feedTimes[i].h == 12){  //if its 12
        total += 12*3600;        //multiply hour value by 3600 (convert to seconds)
      }else{
        total += (feedTimes[i].h+12)*3600; //Else add 12 to hour value and multiply by 3600
      }
    }else{
      if(feedTimes[i].h == 12){  //If feed time is 12 AM
        //If current times is PM and consider is true
        if(consider && (ind == 'P')){ //Consider is true if we are converting times after sorting is complete
          total+=24*3600; //Consider 12 AM as 24 instead of 0
        }
      }else{
        total += feedTimes[i].h*3600;
      }
    }
    total += feedTimes[i].m*60; //Convert minutes to seconds and add to total
    total += feedTimes[i].s;
    Arr[i] = total; //store total value into array at index i
    total = 0; //Reset total
  }
}

/**************************************************************************************************/

//This function sorts an array of values from lowest to highest and keeps track of the index positions
void sort(int vals[],char inds[], int arrSize){
  int tMax = 0; //Total max value
  int index = 0; //Index value
  for(int i = 0;i<arrSize;i++){
    if(vals[i] > tMax){ //If current value is greater than tmax
      tMax = vals[i]; //Set tmax to current value
      index = i; //Save index of value
    }
  }
  //swap last position of array with max value
  int temp = vals[arrSize-1];
  vals[arrSize-1] = vals[index];
  vals[index] = temp;
  //swap last position of index array with max value's index
  temp = inds[arrSize-1];
  inds[arrSize-1] = index;
  inds[index] = temp;
  
  int tempSize = arrSize-1; //Decrease array size by 1
  if(tempSize >= 1){ //If array size is greater than or equal to 1
    sort(vals,inds,tempSize); //Call this function again with same arrays but updated array size
  }
}

/**************************************************************************************************/


//This function calculates and returns the dropPosition of the next closest drop time
int calcNextDrop(){
  int totalTime = 0; //Total time in seconds
  //This all converts current time to a number in seconds and adds it to total time
  if(ind == 'P'){
    if(h == 12){
      totalTime += 12*3600;
    }else{
      totalTime += (h+12)*3600;
    }
  }else{
    if(h != 12){
      totalTime += h*3600;
    }
  }
  totalTime += m*60;
  totalTime += s;

  int valueArr[10]; //Array of time values in seconds
  convertTimes(valueArr,true); //convert all drop times to seconds and store them into value array
  //At this point the value array will be already be sorted least to greatest since feedTimes array is sorted

  for(int i=0;i<dropCount;i++){
    if(totalTime < valueArr[i]){ //If current time is less than value in value array then return that index
      return i;
    }
  }
  return 0; //If we dont return an index, return the first index.
  //If there is only one drop time this can happen.
}

/**************************************************************************************************/

//This function sets up the arrays for the sorting algorithm
void sortTimes(){
  feedTime temp[10]; //A temperary array of feedTime structs
  for(int i=0;i<dropCount;i++){
    temp[i] = feedTimes[i]; //Paste all feed times into the temp array
  }
  
  int valueArr[10]; //Array of values that holds all the drop times in seconds
  convertTimes(valueArr,false); //Convert all drop times to seconds and store into value array
  char indArr[10]; //Array that holds indexes
  for(int i=0;i<10;i++){
    indArr[i] = i; //Fill index array with 0,1,2,...,9
  }
  
  sort(valueArr, indArr, dropCount); //Begin sorting the valueArray from least to greatest and save swaped index in indArr
  for(int i=0;i<dropCount;i++){
    feedTimes[i] = temp[indArr[i]];  //Use sorted indexes to reorder feedTimes array in least to greatest
  }

  dropPosition = calcNextDrop(); //Calculate current dropPosition
}

/**************************************************************************************************/

//Initiate feed time array (called if data is saved in EEPROM)
void initFeedTimeArray(){
  for(int i=0;i<dropCount;i++){
    //Fills feed time hour, min, sec and ind values from EEPROM memory
    feedTimes[i].h = EEPROM.read(EEPROM_Position);
    EEPROM_Position++;
    feedTimes[i].m = EEPROM.read(EEPROM_Position);
    EEPROM_Position++;
    feedTimes[i].s = EEPROM.read(EEPROM_Position);
    EEPROM_Position++;
    feedTimes[i].ind = EEPROM.read(EEPROM_Position);
    EEPROM_Position++;
  }
  EEPROM_Position = EEPROM_START+3; //Reset EEPROM_Position to beginning of feedTime data

  sortTimes(); //Begin sorting feedTimes array
}

/**************************************************************************************************/

//Writes data into EEPROM
void writeData(){
  for(int i=0;i<dropCount;i++){
    EEPROM.write(EEPROM_Position,feedTimes[i].h);
    EEPROM_Position++;
    EEPROM.write(EEPROM_Position,feedTimes[i].m);
    EEPROM_Position++;
    EEPROM.write(EEPROM_Position,feedTimes[i].s);
    EEPROM_Position++;
    EEPROM.write(EEPROM_Position,feedTimes[i].ind);
    EEPROM_Position++;
  }
  EEPROM.write(EEPROM_START+2,0);
}

/**************************************************************************************************/

//Setup function
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
  editDrops = false;
  backLight = true;

  blinkTime = BLINK_TIME; //Initialize blinkTime counter

  //Clock initializes at 12 AM
  h = 10;
  m = 59;
  s = 00;
  ind = 'P';

  if(EEPROM.read(EEPROM_START+2) == 0){//If data is saved in EEPROM
    dropCount = EEPROM.read(EEPROM_START+1); //Read dropCount from EEPROM and store into dropCount
    initFeedTimeArray(); //Fill feedTimes array
  }else{
    incomplete = true; //If EEPROM_START+2 reads 255 then data is incomplete
  }
  
  lcd.init(); //initialize the lcd
  lcd.backlight(); //open the backlight 
}

/**************************************************************************************************/

void loop() 
{
  if((EEPROM.read(EEPROM_START) == 255) || incomplete){ //If data is incomplete or was never initiated
    initFoodDropCount(); //Ask how many drop times there are
    return; //End loop
  }
  if(isChangePressed()){
    if(!editDrops){ //If change button is pressed and we are not editing drop times
      switch(changeCounter){ //Switch checking change button counter
        case 0: //If first time pressing change button
          counting = false; //Stop tracking time
          editHours = true; //Begin editing hours
          changeCounter++; //Add 1 to change button counter
          lcd.clear(); //Clear LCD screen
          break;
        case 1:
          editHours = false;
          editMins = true; //Begin editing minutes
          changeCounter++;
          lcd.clear();
          break;
        case 2:
          editMins = false;
          editSecs = true; //Begin editing seconds
          changeCounter++;
          lcd.clear();
          break;
        case 3:
          editSecs = false;
          editInd = true; //Begin editing indicator
          changeCounter ++;
          lcd.clear();
          break;
        case 4: //If change button pressed a fifth time begin tracking time again
          editInd = false;
          counting = true;
          changeCounter = 0;

          //If we haven't reached dropCount and data hasnt been saved in EEPROM
          if(dropPosition < dropCount && EEPROM.read(EEPROM_START+2) == 255){
            feedTimes[dropPosition] = initFeedTime(h,m,s,ind); //Save current time into feedTimes at dropPosition
            dropPosition++;                                    //Increase dropPosition
            if(dropPosition < dropCount){//If dropPosition is still less than dropCount
              changeCounter = 1; //Reset changeCounter to 1
              editHours = true; //Begin editing hours again
              counting = false;
            }else if(dropPosition == dropCount){ //If we have reached dropCount
              writeData(); //Write data into EEPROM
              dropPosition = 0; //Reset dropPosition
              changeCounter = 1;
              //Begin editing real time
              editHours = true;
              counting = false;
            }
          }
          
          dropPosition = calcNextDrop(); //Find next closest drop position
          
          lcd.clear();
          break;
      }
    }else{
      if(editDropChoice){ //If change button is pressed and edit drop choice is yes
        EEPROM.write(EEPROM_START,255); //Reset EPPROM setting data
        EEPROM.write(EEPROM_START+2,255);
      }else{
        editDrops = false; //If user decided not to change drop times go back to normal counting screen
        editDropChoice = false;
      }
    }
  }
  if(counting){ //If we are tracking time
    unsigned long currentMillis = millis(); //Get current milliseconds
    if(currentMillis - previousMillis >= MILLISPEED){ //If current milliseconds - previous >= 1000 (1 second)
      s++; //Add 1 to seconds
      if(increased){
        increased = false;
      }
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
    //If increase button is pushed while not editing time
    if(isIncPressed() && !editDrops){
      incHeldStart = currentMillis;
      if(backLight){
        backLight = false; //If backlight is on, turn it off
        lcd.noBacklight();
      }else{
        backLight = true; //If backlight is off, turn it on
        lcd.backlight();
      }
    }
    if(!incRelease){
      if(currentMillis-incHeldStart >= 5000){
        backLight = true;
        lcd.backlight();
        editDrops = true;
        lcd.clear();
      }
    }
  }else if(editHours){ //If we are editing time
    if(isIncPressed()){
      h++; //Add to hour
      if(h > 12){ // if hour exceeds 12 reset to 1
        h = 1;
      }
    }
  }else if(editMins){ //Same this as hours just for minutes
    if(isIncPressed()){
      m++;
      if(m > 59){ //If mins exceeds 59 reset to 0
        m = 0;
      }
    }
  }else if(editSecs){//Same thing as hours just for seconds
    if(isIncPressed()){
      incRelease = false;
      s++;
      if(s > 59){
        s = 0;
      }
    }
  }else if(editInd){ //Finally same this as hours just for indicator
      if(isIncPressed()){
        if(ind == 'A'){
          ind = 'P';
        }else{
          ind = 'A';
        }
      }
  }

  //Strings for hours, mins, etc. to make printing to LCD easier
  char hours[3];
  char mins[3];
  char secs[3];
  //Format 2 spaces where empty spaces are filled with 0s for each variable
  sprintf(hours,"%02d",h);
  sprintf(mins,"%02d",m);
  sprintf(secs,"%02d",s);
  //Call Print time to print to LCD
  printTime(hours,mins,secs); 
}
