#include "com.h"
#include "config.h"
#include "language.h"
//#include "SD.h"
#include "SdFat.h"
#include "draw.h"
#include "setupTxt.h"
#include "nunchuk.h"
#include <WiFi.h> 
#include "telnet.h"
#include "cmd.h"
#include "log.h"
#include <Preferences.h>
#include "telnetgrbl.h"
#include "BluetoothSerial.h"
#include "bt.h"
#include <sstream>
#include <iomanip>


// we can also get messages: They are enclosed between [....] ; they are currently discarded but stored in the log. 
// used to decode the status and position sent by GRBL
#define GET_GRBL_STATUS_CLOSED 0
#define GET_GRBL_STATUS_START 1
#define GET_GRBL_STATUS_WPOS_HEADER 2
#define GET_GRBL_STATUS_WPOS_DATA 3
#define GET_GRBL_STATUS_HEADER 4
#define GET_GRBL_STATUS_F_DATA 5
#define GET_GRBL_STATUS_WCO_DATA 6
#define GET_GRBL_STATUS_ALARM 7
#define GET_GRBL_STATUS_BF_DATA 8
#define GET_GRBL_STATUS_MESSAGE 9
#define GET_GRBL_STATUS_OV_DATA 10
  
#define WAIT_OK_SD_TIMEOUT 120000



uint8_t grblLink = GRBL_LINK_SERIAL ;
uint8_t wposIdx = 0 ;
uint8_t wcoIdx = 0 ;
uint8_t fsIdx = 0 ;
uint8_t bfIdx = 0 ;
uint8_t ovIdx = 0 ;
uint8_t getGrblPosState = GET_GRBL_STATUS_CLOSED ;
float feedSpindle[2] ;  // first is FeedRate, second is Speed
float bufferAvailable[2] ;  // first is number of blocks available in planner, second is number of chars available in serial buffer
float overwritePercent[3] ; // first is for feedrate, second for rapid (G0...), third is for RPM
float wcoXYZA[4] ;           // 4 because we can support 4 axis
float wposXYZA[4] ;
float mposXYZA[4] ;

extern Preferences preferences ; // object from ESP32 lib used to save/get data in flash 

char modalAbsRel[4] = {0}; // store the modal G20/G21 in a message received from grbl
char modalMmInch[4] = {0} ; // store the modal G90/G91 in a message received from grbl
char G30SavedX[12] = "0.0" ; // store the value of G30 X offset in a message received from grbl;
char G30SavedY[12] = "0.0" ; // store the value of G30 Y offset in a message received from grbl;

char printString[250] = {0} ;       // contains a command to be send from a string; wait for OK after each 0x13 char
char * pPrintString = printString ;
uint8_t lastStringCmd ;

char strGrblBuf[STR_GRBL_BUF_MAX_SIZE] ; // this buffer is used to store a few char received from GRBL before decoding them
uint8_t strGrblIdx ;
extern char grblLastMessage[STR_GRBL_BUF_MAX_SIZE] ;
extern boolean grblLastMessageChanged;

extern int8_t jogDistX ;
extern int8_t jogDistY ;
extern int8_t jogDistZ ;
extern int8_t jogDistA ;

extern float moveMultiplier ;
// used by nunchuck
extern uint8_t jog_status  ;
extern boolean jogCancelFlag ;
extern boolean jogCmdFlag  ; 
extern uint32_t startMoveMillis ;

extern volatile boolean waitOk ;
extern boolean newGrblStatusReceived ;
extern volatile uint8_t statusPrinting  ;
extern char machineStatus[9];
extern char lastMsg[80] ;        // last message to display
extern uint16_t lastMsgColor ;        // last message color
extern boolean lastMsgChanged ;
extern M_pLabel mGrblErrors[_MAX_GRBL_ERRORS] ;
extern M_pLabel mAlarms[_MAX_ALARMS]; 


extern boolean updateFullPage ;

//        Pour sendToGrbl
//extern File fileToRead ;
extern SdBaseFile aDir[DIR_LEVEL_MAX] ; 
extern int8_t dirLevel ;
extern uint8_t cmdToSend ; // cmd to be send
extern uint32_t sdNumberOfCharSent ;

extern WiFiClient telnetClient;
extern BluetoothSerial SerialBT;

uint8_t wposOrMpos ;
uint32_t waitOkWhenSdMillis ;  // timeout when waiting for OK while sending form SD

uint8_t parseGrblFilesStatus = PARSING_FILE_NAMES_BLOCKED ;  // status to know if we are reading [FILES: lines from GRBL or if it is just done 

extern char grblDirFilter[100] ; // contains the name of the directory to be filtered; "/" for the root; last char must be "/"
extern char grblFileNames[GRBLFILEMAX][40]; // contain n filename or directory name

extern int grblFileIdx ; // index in the array where next file name being parse would be written
uint32_t millisLastGetGBL = 0 ;
extern int8_t errorGrblFileReading ; // store the error while reading grbl files (0 = no error)
float decodedFloat[4] ; // used to convert 4 floats comma delimited when parsing a status line
float runningPercent ; // contains the percentage of char being sent to GRBL from SD card on GRBL_ESP32; to check if it is valid
boolean runningFromGrblSd = false ; // indicator saying that we are running a job from the GRBL Sd card ; is set to true when status line contains SD:


// ----------------- fonctions pour lire de GRBL -----------------------------
void getFromGrblAndForward( void ) {   //get char from GRBL, forward them if statusprinting = PRINTING_FROM_PC and decode the data (look for "OK", for <xxxxxx> sentence
                                       // fill machineStatus[] and wposXYZA[]
  #define MAX_LINE_LENGTH_FROM_GRBL 200
  
  static char lineToDecode[MAX_LINE_LENGTH_FROM_GRBL] = {'\0'} ;
  static uint8_t c;
  //static uint32_t millisLastGetGBL = 0 ;
  static uint16_t lineToDecodeIdx = 0 ; // position where to write the next char received
  while (fromGrblAvailable() ) { // check if there are some char from grbl (Serial or Telnet or Bluetooth)
      c=fromGrblRead() ; // get the char from grbl (Serial or Telnet or Bluetooth)
//#define DEBUG_RECEIVED_CHAR
#ifdef DEBUG_RECEIVED_CHAR      
      Serial.print( (char) c) ;
      //if  (c == 0x0A || c == 0x0C ) Serial.println(millis()) ;
#endif      
      if ( statusPrinting == PRINTING_FROM_USB  ) {
        Serial.print( (char) c) ;                         // forward characters from GRBL to PC when PRINTING_FROM_USB
      }
      sendViaTelnet((char) c) ;                   // forward characters from GRBL to telnet when PRINTING_FROM_TELNET
      if ( c == 0x0A || c == 0x0C ) {
        lineToDecode[lineToDecodeIdx] = 0 ; // add a 0 to close the string
        //Serial.print("To decode="); Serial.println(lineToDecode);
        decodeGrblLine(lineToDecode) ; // decode the line
        lineToDecodeIdx =  0; // reset the position
        lineToDecode[lineToDecodeIdx] = '\0' ;
      } else {                // store the char
        lineToDecode[lineToDecodeIdx] = c;
        if (lineToDecodeIdx < (MAX_LINE_LENGTH_FROM_GRBL - 2) ) lineToDecodeIdx++ ;  
      }
  } // end while available
  if ( (millis() - millisLastGetGBL ) > 2500 ) {           // if we did not get a GRBL status since 2500 ms, status become "?"
    machineStatus[0] = '?' ; machineStatus[1] = '?' ; machineStatus[2] = 0 ; 
    //Serial.print( "force reset ") ; Serial.print( millis()) ; Serial.print( " LG> = ") ; Serial.print( millis()) ;
    millisLastGetGBL = millis() ;
    newGrblStatusReceived = true ;                            // force a redraw if on info screen  
  }
}  

/* Response Messages: Normal send command and execution response acknowledgement. Used for streaming.

ok : Indicates the command line received was parsed and executed (or set to be executed).
error:x : Indicated the command line received contained an error, with an error code x, and was purged. See error code section below for definitions.
Push Messages:

< > : Enclosed chevrons contains status report data.
Grbl X.Xx ['$' for help] : Welcome message indicates initialization.
ALARM:x : Indicates an alarm has been thrown. Grbl is now in an alarm state.
$x=val and $Nx=line indicate a settings printout from a $ and $N user query, respectively.
[MSG:] : Indicates a non-queried feedback message.
[GC:] : Indicates a queried $G g-code state message.
[HLP:] : Indicates the help message.
[G54:], [G55:], [G56:], [G57:], [G58:], [G59:], [G28:], [G30:], [G92:], [TLO:], and [PRB:] messages indicate the parameter data printout from a $# user query.
[VER:] : Indicates build info and string from a $I user query.
[echo:] : Indicates an automated line echo from a pre-parsed string prior to g-code parsing. Enabled by config.h option.
>G54G20:ok : The open chevron indicates startup line execution. The :ok suffix shows it executed correctly without adding an unmatched ok response on a new line.
 */

void decodeGrblLine(char * line){  // decode a full line when CR or LF is received ; line is in lineToDecode 
  int lengthLine = strlen(line) ;
  if ( lengthLine == 0) return ;  // Exit if the line is empty
  if (line[0] == 'o' && line[1] == 'k' && lengthLine == 3){ // for OK, 
    waitOk = false ;
    //cntOk++;
    return;  // avoid to log this line  
  } else if (line[0] == '[') {      // for [ whe have to check for FILE and SD in a different way
     parseMsgLine(line) ;
     return ;  // avoid to log this line ; when it is a line to Log it is already done while parsing.
  } else if (line[0] == '<') {     // decode a status line   
    parseSatusLine(line);  
    return ; // avoid to log this line
  } else if ( strncmp(line, "error:", strlen("error:")) == 0 ) {
    parseErrorLine(line);
  } else if ( strncmp(line, "ALARM:", strlen("ALARM:")) == 0 ) {
    parseAlarmLine(line);
  }
  // discard other cases (but still put them in log buffer (it can e.g. be ALARM:1 or error:10)
  logBufferWriteLine( line );    // keep trace of the line (in most of case)
}

void parseErrorLine(const char * line){ // extract error code, convert it in txt
  int errorNum = atoi( &line[6]) ;
  int errorNumCorr;
  errorNumCorr = errorNum ;
  if (errorNum < 1 || errorNum > 70 ) errorNumCorr = 0 ;
  if (errorNum >=40 && errorNum <= 59 ) errorNumCorr = 0 ; //there are no num in range 40/59
  if (errorNum >=60) errorNumCorr -= 20;  // there are no num in range 40/59; so we avoided those in the 
  memccpy ( lastMsg , mGrblErrors[errorNumCorr].pLabel , '\0' , 79); // fill Message ; note: it is also added to Log
  lastMsgColor = SCREEN_ALERT_TEXT ;
  lastMsgChanged = true ;
  if ( errorNum >= 60 && errorNum <= 69 ) {
    errorGrblFileReading = errorNum +20; // save the grbl error (original value)
    parseGrblFilesStatus = PARSING_FILE_NAMES_DONE ; // inform main loop that callback function must be executed
  }
}

void parseAlarmLine(const char * line){
  int alarmNum = atoi( &line[6]) ; // search from pos 6 
  if (alarmNum < 1 || alarmNum > 9 ) alarmNum = 0 ;
  // char alarmTxt[80] ;
  //int lenLine = strlen(line) ;
  //memcpy(alarmTxt, line , lenLine);
  //strncpy(alarmTxt + lenLine , alarmArrayMsg[alarmNum] , 79-lenLine) ;
  //alarmTxt[79] = 0 ; // for safety we add a end of string  
  memccpy ( lastMsg , mAlarms[alarmNum].pLabel , '\0' , 79); // fill Message ; note: it is also added to Log
  lastMsgColor = SCREEN_ALERT_TEXT ;
  lastMsgChanged = true ;
}

void parseMsgLine(char * line) {  // parse Msg line from GRBL
     char * pSearch ;
     char * pEndNumber1 ;
     char * pEndNumber2 ;
     if ( strncmp(line, "[FILE:", strlen("[FILE:")) == 0 ) {
        parseFileLine( line + strlen("[FILE:"));
        return; // do not log the File lines
     
     } else if ( strncmp(line , "[SD Free:", strlen("[SD Free:")) == 0 ) {
        if ( parseGrblFilesStatus == PARSING_FILE_NAMES_RUNNING ) {
          parseGrblFilesStatus = PARSING_FILE_NAMES_DONE ; // mark that all lines have been read ; it allows main loop to handle the received list
          //Serial.println("End of lile list");
          //Serial.print("Nr of entries=");Serial.println(grblFileIdx);
          int i = 0; 
          for ( i  ; i < grblFileIdx ; i++) {
            //Serial.print("file ="); Serial.println(grblFileNames[i]);
          }
          
        }
        return; // do not log the SD lines
     
     } else if ( strncmp(line , "[GC:", strlen("[GC:") ) == 0 )   {
          pSearch = strstr( line , "G2" ) ; // can be G20 or G21
          if (pSearch != NULL ) memcpy( modalAbsRel , pSearch , 3) ;
          pSearch = strstr( line , "G90" ) ;
          if (pSearch != NULL ) memcpy( modalMmInch , pSearch , 3) ;
          pSearch = strstr( line , "G91" ) ;
          if (pSearch != NULL ) memcpy( modalMmInch , pSearch , 3) ;
     
     } else if ( strncmp(line , "[G30:", strlen("[G30:")) == 0 )  { // GRBL reply to $# with e.g. [G28:1.000,2.000,0.000] or [G30:4.000,6.000,0.000]
          char * pBuf =  line + strlen("[G30:" ) ;
          pEndNumber1 = strchr(pBuf , ',') ; // find the position of the first ','
          if (pEndNumber1 != NULL ) { 
              memcpy( G30SavedX , pBuf , pEndNumber1 - pBuf) ;
              G30SavedX[pEndNumber1 - pBuf] = 0 ;
              pEndNumber1++ ; // point to the first char of second number
              pEndNumber2 = strchr(pEndNumber1 , ',') ; // find the position of the second ','
              if (pEndNumber2 != NULL ) {
                memcpy( G30SavedY , pEndNumber1 , pEndNumber2 - pEndNumber1) ;
                G30SavedY[pEndNumber2 - pEndNumber1] = 0 ;
              }
          }    
     }
     logBufferWriteLine( line ); 
}

  
void parseFileLine(char * line ){  // parse file line from GRBL; "[FILE:" is alredy removed
  // line looks like [FILE:/dir1/TestDir3.nc|SIZE:5]  [FILE: is alredy removed
  // grblDirFilter is supposed to contains "/" or "/abc/" or "/abc/de/" (so last char = "/"
  char * sizePtr ;
  char * fileNameOrDirPtr ;
  char * firstNextDirPtr ;
  int grblDirFilterLen = strlen(grblDirFilter) ; 
  sizePtr = strchr(line , '|' );  // search for | to separate name from to size
  //if (parseGrblFilesStatus == PARSING_FILE_NAMES_RUNNING) {      // process line only if requested
    if ( sizePtr == NULL) return ; // discard the line if it does not contains | separator between name and size
    *sizePtr = '\0' ;  //Replace | with string terminator
    if ( strncmp(line , grblDirFilter , grblDirFilterLen ) != 0 )  { // discard the line if it does not contains the dirFilter
      return;  
    }
    if ( strncmp(line , "/System Volume Information" , strlen("/System Volume Information") ) == 0 ) { // discard if the line is a system information
      return;
    }
    fileNameOrDirPtr = line + strlen(grblDirFilter) ; // point to the first char of file or dir name
    firstNextDirPtr =  strchr(fileNameOrDirPtr  , '/' ) ; // search first '/' after the dirFilter
    if (firstNextDirPtr != NULL ) { // it means that we have a dir name. We have to extract the first directory and put it in the array with a / as last char.
                                // if NULL, there are no / and so we can copy the file name
      //firstNextDirPtr++ ;  //point to the first char after /
      fileNameOrDirPtr--;     //Point one char before and
      *fileNameOrDirPtr = '/' ; // Replace first char with '/' instead of '\0'
      *firstNextDirPtr = '\0' ;  //Replace '/' at the end by string terminator
      if ( grblFileIdx > 0) {
        if (strcmp (fileNameOrDirPtr ,  grblFileNames[grblFileIdx-1] ) == 0 ) { // if dirname = previous entry, discard
          return ;
        }
      }
    }
    if (grblFileIdx == (GRBLFILEMAX - 1) ) { // discard if we reach the max number of files that can be registered
        strcpy(grblFileNames[grblFileIdx] , "Etc...")  ; // put etc if we reach the max and discard the file
    }else {  
        strncpy(grblFileNames[grblFileIdx] , fileNameOrDirPtr , 39) ; //copy the filename (no "/" at the end)
        grblFileNames[grblFileIdx] [39] = '\0' ;     // add end of string for safety 
        grblFileIdx++ ;
    }
}


void parseSatusLine(char * line) {
// GRBL status are : Idle, Run, Hold, Jog, Alarm, Door, Check, Home, Sleep
// a message should look like (note : GRBL sent or WPOS or MPos depending on grbl parameter : to get WPos, we have to set "$10=0"
//  <Jog|WPos:1329.142,0.000,0.000|Bf:32,254|FS:2000,0|Ov:100,100,100|A:FM>
//  <Idle|WPos:0.000,0.000,0.000|FS:0.0,0> or e.g. <Idle|MPos:0.000,0.000,0.000|FS:0.0,0|WCO:0.000,0.000,0.000|Ov:100,100,100>
//CLOSED
//   START
//        WPOS_HEADER
//            WPOS_DATA
//                               HEADER
//                                 F_DATA
//                                   F_DATA
//                                                                                      WCO_DATA
// note : status can also be with a ":" like Hold:1 or Door:2
// outside <...>, we can get "Ok" and "error:12" followed by an CR and/or LF
// Note: when a job started from a sd card on GRBL_ESP32 is running the line contains also % of execution and file name like this:
//<Idle|MPos:-10.000,0.000,0.000|Bf:15,0|FS:0,0|SD:14.29,/X4move100.gcode>


   char * pBegin ;
   char * pEndType ;
   uint8_t i = 0 ;
   //Serial.print("line len") ; Serial.println(strlen(line)); 
   //Serial.print("line[len-2]= "); Serial.println(line[strlen(line) -2]);
   if ( line[strlen(line) -2] != '>' ) return ; // discard if last char is not '>'
      pBegin = line + 1;
      pEndType = strchr( pBegin , '|' ) ;
      *pEndType = '\0' ; // replace | by 0 in order to let memccpy copy end of string too 
      memccpy( machineStatus , pBegin , '\0', 9);  // copy the status
      pBegin = strchr(pBegin , '\0') + 1 ;  // point to first Char after the status
      char MPosOrWPos = ' ' ;
      runningFromGrblSd = false ; // reset the indicator saying we are printing from Grbl Sd card
      while ( true ) {                     // handle each section up to the end of line
          pEndType = strchr(pBegin , ':') ;
          if (pEndType ) {
              decodeFloat(pEndType+1) ;
              i = 0 ;
              if ( strncmp( pBegin, "MPos:" , strlen("MPos:") ) == 0 ) {
                  for (i ; i<4 ; i++) {
                    mposXYZA[i] = decodedFloat[i] ;
                    wposXYZA[i] = mposXYZA[i] - wcoXYZA[i] ;
                    MPosOrWPos = 'M' ;
                  }      
              } else if ( strncmp( pBegin, "WPos:" , strlen("WPos:") )== 0 ) {
                  for (i ; i<4 ; i++) {
                    wposXYZA[i] = decodedFloat[i] ;
                    mposXYZA[i] = wposXYZA[i] + wcoXYZA[i] ;
                    MPosOrWPos = 'W' ;
                  }  
              } else if ( strncmp( pBegin, "WCO:" , strlen("WCO:") ) ==0 ){
                  for (i ; i<4 ; i++) {
                      wcoXYZA[i] =  decodedFloat[i] ;
                      if ( MPosOrWPos == 'W') {                  // we previously had a WPos so we update MPos
                        mposXYZA[i] = wposXYZA[i] + wcoXYZA[i] ;  
                      } else {                                   // we previously had a MPos so we update WPos
                        wposXYZA[i] = mposXYZA[i] - wcoXYZA[i] ;
                      }
                  }
              } else if ( strncmp( pBegin, "Bf:" , strlen("Bf:") ) == 0 ) {
                  bufferAvailable[0] = decodedFloat[0] ;
                  bufferAvailable[1] = decodedFloat[1] ;
              } else if ( strncmp( pBegin, "F:" , strlen("F:") ) ==  0 ) {
                  feedSpindle[0] = decodedFloat[0] ;
              } else if ( strncmp( pBegin, "FS:" , strlen("FS:") ) == 0 ){
                  feedSpindle[0] = decodedFloat[0] ;
                  feedSpindle[1] = decodedFloat[1] ;
              } else if ( strncmp( pBegin, "Ov:" , strlen("Ov:") ) == 0 ){
                  overwritePercent[0] = decodedFloat[0] ;
                  overwritePercent[1] = decodedFloat[1] ;
                  overwritePercent[2] = decodedFloat[2] ;
              } else if ( strncmp( pBegin, "SD:" , strlen("SD:") ) == 0 ){
                  runningPercent = decodedFloat[0] ;
                  runningFromGrblSd = true; 
              } // end testing different fields} // end testing different fields
              // now, update pBegin to nextField
              pBegin = strchr(pBegin , '|' ) ; //search begin of next section
              if (pBegin) {
                pBegin++;
                //Serial.print("remaining text=") ; Serial.println(pBegin);   
              } else {
                break; // exit while when there are no other section
          }
      } // exit While
      millisLastGetGBL = millis();
      newGrblStatusReceived = true;     
   } // end if
}

void decodeFloat(char * pSection) { // decode up to 4 float numbers comma delimited in a section
  decodedFloat[0] = 0;
  decodedFloat[1] = 0;
  decodedFloat[2] = 0;
  decodedFloat[3] = 0;
  char * pEndNum ;
  int i = 0 ;
  pEndNum = strpbrk ( pSection , ",|>") ; // cherche un dernier caractère valide après le nombre
  while ( (i < 4) && pEndNum) { // decode max 4 floats
      decodedFloat[i] = atof (pSection) ;
      if ( *pEndNum == ',') { // if last char is ',', then we loop
        pSection =  pEndNum + 1;
        i++; 
        pEndNum = strpbrk ( pSection , ",|>") ; // search first char that end the section and return the position of this end
      } else {
        i=4; // force to exit while when the last char was not a comma
      }
  }    
}

/*
void getFromGrblAndForward2( void ) {   //get char from GRBL, forward them if statusprinting = PRINTING_FROM_PC and decode the data (look for "OK", for <xxxxxx> sentence
                                       // fill machineStatus[] and wposXYZA[]
  static uint8_t c;
  static uint8_t lastC = 0 ;
  static uint32_t millisLastGetGBL = 0 ;
  //uint8_t i = 0 ;
  static int cntOk = 0 ;
  while (fromGrblAvailable() ) { // check if there are some char from grbl (Serial or Telnet or Bluetooth)
  //  while (Serial2.Available() ) {
#ifdef DEBUG_TO_PC
    //Serial.print(F("s=")); Serial.print( getGrblPosState ); Serial.println() ;
#endif    
    c=fromGrblRead() ; // get the char from grbl (Serial or Telnet or Bluetooth)
    //c=Serial2.read();
//#define DEBUG_RECEIVED_CHAR
#ifdef DEBUG_RECEIVED_CHAR      
      Serial.print( (char) c) ;
      //if  (c == 0x0A || c == 0x0C ) Serial.println(millis()) ;
#endif      
    if ( statusPrinting == PRINTING_FROM_USB  ) {
      Serial.print( (char) c) ;                         // forward characters from GRBL to PC when PRINTING_FROM_USB
    }
    sendViaTelnet((char) c) ;                   // forward characters from GRBL to telnet when PRINTING_FROM_TELNET
    switch (c) {                                // parse char received from grbl
    case 'k' :
      if ( lastC == 'o' ) {
        waitOk = false ;
        cntOk++;
        getGrblPosState = GET_GRBL_STATUS_CLOSED ;
        break ; 
      }
      //break ;   // avoid break here because 'k' can be part of another message

    case '\r' : // CR is sent after "error:xx"
      if( getGrblPosState == GET_GRBL_STATUS_CLOSED ) {
        if (  strGrblBuf[0] == 'e' && strGrblBuf[1] == 'r' )   {  // we got an error message or an ALARM message
          fillErrorMsg( strGrblBuf );           // save the error or ALARM
        } else if  ( strGrblBuf[0] == 'A' && strGrblBuf[1] == 'L' )  {
          fillAlarmMsg( strGrblBuf );  
        }
        
      }
      getGrblPosState = GET_GRBL_STATUS_CLOSED ;
      strGrblIdx = 0 ;                                        // reset the buffer
      strGrblBuf[strGrblIdx] = 0 ;
      break ;
 
    case '<' :
      getGrblPosState = GET_GRBL_STATUS_START ;
      strGrblIdx = 0 ;
      strGrblBuf[strGrblIdx] = 0 ;
      wposIdx = 0 ;
      millisLastGetGBL = millis();
      //Serial.print("LG< =") ; Serial.println( millisLastGetGBL ) ;
#ifdef DEBUG_TO_PC
      //Serial.print(" <") ; 
#endif      
      break ;
      
    case '>' :                     // end of grbl status 
      handleLastNumericField() ;  //wposXYZA[wposIdx] = atof (&strGrblBuf[0]) ;
      getGrblPosState = GET_GRBL_STATUS_CLOSED ;
      strGrblIdx = 0 ;
      strGrblBuf[strGrblIdx] = 0 ;
      newGrblStatusReceived = true;
#ifdef DEBUG_TO_PC
      //Serial.print("X=") ; Serial.print(wposXYZA[0]) ; Serial.print(" Y=") ; Serial.print(wposXYZA[1]) ;Serial.print(" Z=") ; Serial.println(wposXYZA[2]) ; 
      //Serial.println(">");
#endif      
      break ;
    case '|' :
      if ( getGrblPosState == GET_GRBL_STATUS_START ) {
         getGrblPosState = GET_GRBL_STATUS_WPOS_HEADER ; 
         memccpy( machineStatus , strGrblBuf , '\0', 9);
         strGrblIdx = 0;
         strGrblBuf[strGrblIdx] = 0 ;        
#ifdef DEBUG_TO_PC
         //Serial.print( "ms= " ) ; Serial.println( machineStatus ) ;
#endif          
      } else if (  getGrblPosState == GET_GRBL_STATUS_MESSAGE ) {
        if ( strGrblIdx < (STR_GRBL_BUF_MAX_SIZE - 1) 
        ) {
          strGrblBuf[strGrblIdx++] = c ;
          strGrblBuf[strGrblIdx] = 0 ;
        } 
      } else { 
        handleLastNumericField() ;
        getGrblPosState = GET_GRBL_STATUS_HEADER ;
        strGrblIdx = 0 ;  
      }
      break ;
    case ':' :
      if ( getGrblPosState == GET_GRBL_STATUS_WPOS_HEADER ) { // separateur entre field name et value 
         getGrblPosState = GET_GRBL_STATUS_WPOS_DATA ; 
         wposOrMpos = strGrblBuf[0] ;       // save the first char of the string that should be MPos or WPos
         strGrblIdx = 0 ;
         strGrblBuf[strGrblIdx] = 0 ;
         wposIdx = 0 ;
      } else if ( (getGrblPosState == GET_GRBL_STATUS_START || getGrblPosState == GET_GRBL_STATUS_CLOSED || getGrblPosState == GET_GRBL_STATUS_MESSAGE )
                && strGrblIdx < (STR_GRBL_BUF_MAX_SIZE - 1) ) {    // save the : as part of the text (for error 
         strGrblBuf[strGrblIdx++] = c ; 
         strGrblBuf[strGrblIdx] = 0 ;
      } else if ( getGrblPosState == GET_GRBL_STATUS_HEADER ) {                     // for other Header, check the type of field
        if ( strGrblBuf[0] == 'F' ) {               // start F or FS data
          getGrblPosState = GET_GRBL_STATUS_F_DATA ; 
          strGrblIdx = 0 ;
          fsIdx = 0 ;
        } else if ( strGrblBuf[0] == 'W' ) {     // start WCO data
          getGrblPosState = GET_GRBL_STATUS_WCO_DATA ; 
          strGrblIdx = 0 ;
          wcoIdx = 0 ;
        } else if ( strGrblBuf[0] == 'B' ) {     // start Bf data
          getGrblPosState = GET_GRBL_STATUS_BF_DATA ; 
          strGrblIdx = 0 ;
          bfIdx = 0 ;
        } else if ( strGrblBuf[0] == 'O' ) {     // start OV data
          getGrblPosState = GET_GRBL_STATUS_OV_DATA ; 
          strGrblIdx = 0 ;
          ovIdx = 0 ;
        }   
      }
      break ;
    case ',' :                                              // séparateur entre 2 chiffres
      if ( getGrblPosState != GET_GRBL_STATUS_MESSAGE ) {
        handleLastNumericField() ;                            // check that we are in data to be processed; if processed, reset strGrblIdx = 0 ; 
      } else if ( strGrblIdx < (STR_GRBL_BUF_MAX_SIZE - 1) ) {
        strGrblBuf[strGrblIdx++] = c ;
        strGrblBuf[strGrblIdx] = 0 ;
      }
      
      break ;  
    case '[' :                                              // annonce un message
      if( getGrblPosState == GET_GRBL_STATUS_CLOSED ) {
        getGrblPosState = GET_GRBL_STATUS_MESSAGE ;                            // check that we are in data to be processed; if processed, reset strGrblIdx = 0 ; 
        strGrblIdx = 0 ;                                        // reset the buffer
        strGrblBuf[strGrblIdx] = '[' ;
        strGrblIdx++ ;
      }
      break ;  
    case ' ' :                                              // 
      if( ( getGrblPosState == GET_GRBL_STATUS_MESSAGE ) && strGrblIdx < (STR_GRBL_BUF_MAX_SIZE - 1) ){
        strGrblBuf[strGrblIdx++] = ' ' ;
      }
      break ;  
    
    case ']' :                                              // ferme le message
      if( getGrblPosState == GET_GRBL_STATUS_MESSAGE ) {
        getGrblPosState = GET_GRBL_STATUS_CLOSED ;                            // check that we are in data to be processed; if processed, reset strGrblIdx = 0 ; 
        strGrblBuf[strGrblIdx++] = ']' ;
        strGrblBuf[strGrblIdx] = 0 ;
        memccpy( grblLastMessage , strGrblBuf , '\0', STR_GRBL_BUF_MAX_SIZE - 1);
        storeGrblState() ;                             // to do Store part of message in memory (G20, G21, ...)
        strGrblIdx = 0 ;                                        // reset the buffer
        strGrblBuf[strGrblIdx] = 0 ;
        grblLastMessageChanged = true ;
      }
      break ;  
    default :
      if (  strGrblIdx < ( STR_GRBL_BUF_MAX_SIZE - 1)) {
        if ( ( c =='-' || (c>='0' && c<='9' ) || c == '.' ) ) { 
          if ( getGrblPosState == GET_GRBL_STATUS_WPOS_DATA || getGrblPosState == GET_GRBL_STATUS_F_DATA || getGrblPosState == GET_GRBL_STATUS_BF_DATA || 
                  getGrblPosState == GET_GRBL_STATUS_WCO_DATA || getGrblPosState == GET_GRBL_STATUS_START || getGrblPosState == GET_GRBL_STATUS_CLOSED ||
                  getGrblPosState == GET_GRBL_STATUS_MESSAGE || getGrblPosState == GET_GRBL_STATUS_OV_DATA ){
            strGrblBuf[strGrblIdx++] = c ;
            strGrblBuf[strGrblIdx] = 0 ;
          }
        } else if ( ( (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ) && ( getGrblPosState == GET_GRBL_STATUS_START || getGrblPosState == GET_GRBL_STATUS_HEADER ||
                  getGrblPosState == GET_GRBL_STATUS_CLOSED || getGrblPosState == GET_GRBL_STATUS_MESSAGE  || getGrblPosState == GET_GRBL_STATUS_WPOS_HEADER ) ) { 
           strGrblBuf[strGrblIdx++] = c ;
           strGrblBuf[strGrblIdx] = 0 ;
        }
      }
    } // end switch 
    parseToLog(c , lastC ) ;
    lastC = c ;
  } // end while
  
  if ( (millis() - millisLastGetGBL ) > 2500 ) {           // if we did not get a GRBL status since 2500 ms, status become "?"
    machineStatus[0] = '?' ; machineStatus[1] = '?' ; machineStatus[2] = 0 ; 
    //Serial.print( "force reset ") ; Serial.print( millis()) ; Serial.print( " LG> = ") ; Serial.print( millis()) ;
    millisLastGetGBL = millis() ;
    newGrblStatusReceived = true ;                            // force a redraw if on info screen  
  }
}
*/
/*
void parseToLog(uint8_t c , uint8_t lastC) {   // do not store in log the OK and the status message.
  //Serial.print("To Parse "); Serial.print( (char) c , HEX) ; Serial.print(" , "); Serial.println( (char) c) ;
  if ( getGrblPosState == GET_GRBL_STATUS_CLOSED ) {
    if (c == 'o' && lastC != 'o') {
      return ; // skip 'o' because next char can be K (for an OK) 
    }
    if ( lastC == 'o' ) {
      if ( c == 'k' ) {
        return ; // skip k as part of OK
      } else {
        logBufferWrite(lastC) ; // save previous 'o' when not part of OK
      }  
    } 
    if ( c == '>' ) {
      return ; // skip '>' as begin of status
    }
    if ( c == '\r' ) {
      if ( lastC == '>' || lastC == 'k' ) {
        // skip return after a status line or a OK
      } else {
        logBufferWrite( BUFFER_EOL) ; // replace "new line " by 0 for string handling
      }  
    } else {
      logBufferWrite( c) ;
    } 
  } else if ( getGrblPosState == GET_GRBL_STATUS_MESSAGE ) {
    logBufferWrite( c) ;
  } 
}
*/
/*
void handleLastNumericField(void) { // decode last numeric field
  float temp = atof (&strGrblBuf[0]) ;
  if (  getGrblPosState == GET_GRBL_STATUS_WPOS_DATA && wposIdx < 4) {
          if ( wposOrMpos == 'W') {                  // we got a WPos
            wposXYZA[wposIdx] = temp ;
            mposXYZA[wposIdx] = wposXYZA[wposIdx] + wcoXYZA[wposIdx] ;
          } else {                                   // we got a MPos
            mposXYZA[wposIdx] = temp ;
            wposXYZA[wposIdx] = mposXYZA[wposIdx] - wcoXYZA[wposIdx] ;
          }
          wposIdx++ ;
          strGrblIdx = 0 ; 
  } else if (  getGrblPosState == GET_GRBL_STATUS_F_DATA && fsIdx < 2) {
          feedSpindle[fsIdx++] = temp ;
          strGrblIdx = 0 ;
  } else if (  getGrblPosState == GET_GRBL_STATUS_BF_DATA && bfIdx < 2) {   // save number of available block or char in GRBL buffer
          bufferAvailable[bfIdx++] = temp ;
          strGrblIdx = 0 ; 
  } else if (  getGrblPosState == GET_GRBL_STATUS_WCO_DATA && wcoIdx < 4) {
          wcoXYZA[wcoIdx] = temp ;
          if ( wposOrMpos == 'W') {                  // we previously had a WPos so we update MPos
            mposXYZA[wcoIdx] = wposXYZA[wcoIdx] + wcoXYZA[wcoIdx] ;  
          } else {                                   // we previously had a MPos so we update WPos
            wposXYZA[wcoIdx] = mposXYZA[wcoIdx] - wcoXYZA[wcoIdx] ;
          }
          
          wcoIdx++ ;
          strGrblIdx = 0 ;
  } else if (  getGrblPosState == GET_GRBL_STATUS_OV_DATA && ovIdx < 3) {   // save number of available block or char in GRBL buffer
          overwritePercent[ovIdx++] = temp ;
          strGrblIdx = 0 ; 
  } 
}
*/
/*
void storeGrblState(void) { // search for some char in message
  char * pBuf =  strGrblBuf + 5 ;
  char * pSearch ;
  char * pEndNumber1 ; // point to the first ','
  char * pEndNumber2 ; // point to the second ','
  if ( strGrblBuf[1] == 'G' && strGrblBuf[2] == 'C' && strGrblBuf[3] == ':'  ) { // GRBL reply to a $G with [GC:G20 ....]
    pSearch = strstr( pBuf , "G2" ) ; // can be G20 or G21
    if (pSearch != NULL ) memcpy( modalAbsRel , pSearch , 3) ;
    pSearch = strstr( pBuf , "G90" ) ;
    if (pSearch != NULL ) memcpy( modalMmInch , pSearch , 3) ;
    pSearch = strstr( pBuf , "G91" ) ;
    if (pSearch != NULL ) memcpy( modalMmInch , pSearch , 3) ;
    //Serial.print("AbsRel= " ) ; Serial.println( modalAbsRel) ;
    //Serial.print("MmInch= " ) ; Serial.println( modalMmInch) ;  
  } else if (strGrblBuf[1] == 'G' && strGrblBuf[2] == '3' && strGrblBuf[3] == '0'  ) {  // GRBL reply to $# with e.g. [G28:1.000,2.000,0.000] or [G30:4.000,6.000,0.000]
    pEndNumber1 = strstr(pBuf , ",") ; // find the position of the first ','
    if (pEndNumber1 != NULL ) { 
      memcpy( G30SavedX , pBuf , pEndNumber1 - pBuf) ;
      G30SavedX[pEndNumber1 - pBuf] = 0 ;
      pEndNumber1++ ; // point to the first char of second number
      pEndNumber2 = strstr(pEndNumber1 , ",") ; // find the position of the second ','
      if (pEndNumber2 != NULL ) {
        memcpy( G30SavedY , pEndNumber1 , pEndNumber2 - pEndNumber1) ;
        G30SavedY[pEndNumber2 - pEndNumber1] = 0 ;
      }
    }  
  }
}
*/

void resetWaitOkWhenSdMillis() {    // after a resume (after a pause), we reset this time to avoid wrong warning
  waitOkWhenSdMillis = millis()  + WAIT_OK_SD_TIMEOUT ;  // wait for 2 min before generating the message again
}

//-----------------------------  Send ------------------------------------------------------
void sendToGrbl( void ) {   
                                    // set statusPrinting to PRINTING_STOPPED if eof; exit if we wait an OK from GRBL; 
                                    // if statusPrinting = PRINTING_FROM_USB or PRINTING_FRM_TELNET, then get char from USB or Telnet and forward it to GRBL.
                                    // if statusPrinting = PRINTING_FROM_SD, PRINTING_CMD or PRINTING_STRING, get the char and send
                                    // if statusprinting is NOT PRINTING_FROM_USB or TELNET, then send "?" to grbl to ask for status and position every x millisec
  int sdChar ;
  static uint32_t nextSendMillis = 0 ;
  uint32_t currSendMillis  ;
  if ( waitOk && ( statusPrinting == PRINTING_FROM_SD || statusPrinting == PRINTING_CMD || statusPrinting == PRINTING_STRING ) ) {
      if ( millis() > waitOkWhenSdMillis ) {
        fillMsg(_MISSING_OK_WHEN_SENDING_FROM_SD ) ;   // give an error if we have to wait to much to get an OK from grbl
        waitOkWhenSdMillis = millis()  + WAIT_OK_SD_TIMEOUT ;  // wait for 2 min before generating the message again
      }
  } else {
      switch ( statusPrinting ) {
          case PRINTING_FROM_SD :
            sendFromSd() ;
            break; 
          case PRINTING_FROM_USB :
            while ( Serial.available() && statusPrinting == PRINTING_FROM_USB ) {
              sdChar = Serial.read() ;
              toGrbl( (char) sdChar ) ;
              //Serial2.print( (char) sdChar ) ;
            } // end while 
            break ;
          case PRINTING_FROM_TELNET :
            while ( telnetClient.available() && statusPrinting == PRINTING_FROM_TELNET ) {
              sdChar = telnetClient.read() ;
              sdChar = Serial.read() ;
              //Serial2.print( (char) sdChar ) ;
            } // end while       
            break ;
          case PRINTING_CMD :
            sendFromCmd() ;
            break ;
          case PRINTING_STRING :
            sendFromString() ;
            break ;
      } // end switch
  } // end else if  
  if ( statusPrinting == PRINTING_STOPPED || statusPrinting == PRINTING_PAUSED || statusPrinting == PRINTING_FROM_GRBL_PAUSED) {   // process nunchuk cancel and commands
     sendJogCancelAndJog() ;
  }  // end of nunchuk process
  if ( statusPrinting != PRINTING_FROM_USB && statusPrinting != PRINTING_FROM_TELNET) {     // when PC is master, it is the PC that asks for GRBL status
    currSendMillis = millis() ;                   // ask GRBL current status every X millis sec. GRBL replies with a message with status and position
    if ( currSendMillis > nextSendMillis) {
       nextSendMillis = currSendMillis + 300 ;
       toGrbl('?') ; 
    }
  }
  if( statusPrinting != PRINTING_FROM_TELNET ) {               // clear the telnet buffer when not in use
    while ( telnetClient.available() && statusPrinting != PRINTING_FROM_TELNET ) {
      sdChar = telnetClient.read() ;
    } // end while  
  }
}  

void sendFromSd() {        // send next char from SD; close file at the end
      int sdChar ;
      waitOkWhenSdMillis = millis()  + WAIT_OK_SD_TIMEOUT ;  // set time out on 
      while ( aDir[dirLevel+1].available() > 0 && (! waitOk) && statusPrinting == PRINTING_FROM_SD && Serial2.availableForWrite() > 2 ) {
          sdChar = aDir[dirLevel+1].read() ;
          if ( sdChar < 0 ) {
            statusPrinting = PRINTING_ERROR  ;
            updateFullPage = true ;           // force to redraw the whole page because the buttons haved changed
          } else {
            sdNumberOfCharSent++ ;
            if( sdChar != 13 && sdChar != ' ' ){             // 13 = carriage return; do not send the space.
                                                             // to do : skip the comments
              toGrbl((char) sdChar ) ;
              //Serial2.print( (char) sdChar ) ;
            }
            if ( sdChar == '\n' ) {        // n= new line = line feed = 10 decimal
               waitOk = true ;
            }
          }
      } // end while
      if ( aDir[dirLevel+1].available() == 0 ) { 
        aDir[dirLevel+1].close() ; // close the file when all bytes have been sent.
        statusPrinting = PRINTING_STOPPED  ; 
        updateFullPage = true ;           // force to redraw the whole page because the buttons haved changed
        //Serial2.print( (char) 0x18 ) ; //0x85) ;   // cancel jog (just for testing); must be removed
        toGrbl( (char) 10 ) ;
        //Serial2.print( (char) 10 ) ; // sent a new line to be sure that Grbl handle last line.
      }
} 

void sendFromCmd() {
    int sdChar ;
    waitOkWhenSdMillis = millis()  + WAIT_OK_SD_TIMEOUT ;  // set time out on 
    while ( spiffsAvailableCmdFile() > 0 && (! waitOk) && statusPrinting == PRINTING_CMD && Serial2.availableForWrite() > 2 ) {
      sdChar = (int) spiffsReadCmdFile() ;
      if( sdChar != 13){
          toGrbl((char) sdChar ) ;
          //Serial2.print( (char) sdChar ) ;
        }
      if ( sdChar == '\n' ) {
           waitOk = true ;
        }
    } // end while
    if ( spiffsAvailableCmdFile() == 0 ) { 
      statusPrinting = PRINTING_STOPPED  ; 
      updateFullPage = true ;           // force to redraw the whole page because the buttons haved changed
      toGrbl((char) 0x0A ) ;
      //Serial2.print( (char) 0x0A ) ; // sent a new line to be sure that Grbl handle last line.
    }      
}

void sendFromString() {
char strChar;
float savedWposXYZA[4];
waitOkWhenSdMillis = millis() + WAIT_OK_SD_TIMEOUT;

while (*pPrintString != 0 && (!waitOk) && statusPrinting == PRINTING_STRING) {
    strChar = *pPrintString++;
    if (strChar == '%') {
        strChar = *pPrintString++;
        switch (strChar) {
            case 'z':
                preferences.putFloat("wposZ", wposXYZA[2]);
                break;
            case 'X':
                toGrbl(G30SavedX);
                break;
            case 'Y':
                toGrbl(G30SavedY);
                break;
            case 'Z':
                savedWposXYZA[2] = preferences.getFloat("wposZ", 0);
                {
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(3) << savedWposXYZA[2];
                    std::string floatToString = ss.str();
                    const char* charPtr = floatToString.c_str();
                    toGrbl(charPtr);
                }
                break;
            case 'M':
                toGrbl(modalAbsRel);
                toGrbl(modalMmInch);
                break;
        }
    } else {
        if (strChar != 13 && strChar != ' ') {
            toGrbl((char)strChar);
        }
        if (strChar == '\n') {
            waitOk = true;
        }
    }
}

if (*pPrintString == 0) {
    statusPrinting = PRINTING_STOPPED;
    fillStringExecuteMsg(lastStringCmd);
    updateFullPage = true;
    toGrbl((char)0x0A);
}

}

void sendJogCancelAndJog(void) {
    static uint32_t exitMillis ;
    if ( jogCancelFlag ) {
      if ( jog_status == JOG_NO ) {
        //Serial.println("send a jog cancel");
        toGrbl((char) 0x85) ; toGrbl("G4P0\n\r") ;
        //Serial2.print( (char) 0x85) ; Serial2.print("G4P0") ; Serial2.print( (char) 0x0A) ;    // to be execute after a cancel jog in order to get an OK that says that grbl is Idle.
        while (Serial2.availableForWrite() != 0x7F ) ;                        // wait that all char are sent 
        //Serial2.flush() ;             // wait that all outgoing char are really sent.!!! in ESP32 it also clear the RX buffer what is not expected in arduino logic
        
        waitOk = true ;
        jog_status = JOG_WAIT_END_CANCEL ;
        exitMillis = millis() + 500 ; //expect a OK before 500 msec
        //Serial.println(" send cancel code");     
      } else if ( jog_status == JOG_WAIT_END_CANCEL  ) {
        if ( !waitOk ) {
          jog_status = JOG_NO ;
          jogCancelFlag = false ;
           
        } else {
          if ( millis() >  exitMillis ) {  // si on ne reçoit pas le OK dans le délai maximum prévu
            jog_status = JOG_NO ; // reset all parameters related to jog .
            jogCancelFlag = false ;
            jogCmdFlag = false ;
            //if(lastMsg[0] || (lastMsg[0] == 32) ) fillMsg(_CAN_JOG_MISSING_OK  ) ; // put a message if there was no message (e.g. alarm:)
          }
        }
      } 
    } // end of jogCancelFlag
    
    if ( jogCmdFlag ) {
      //Serial.print("jog_status"); Serial.println(jog_status);
      if ( jog_status == JOG_NO ) {
        //Serial.print("bufferAvailable=");Serial.println( bufferAvailable[0] ) ;
        if (bufferAvailable[0] > 5) {    // tests shows that GRBL gives errors when we fill to much the block buffer  
          if ( sendJogCmd(startMoveMillis) ) { // if command has been sent
            waitOk = true ;
            jog_status = JOG_WAIT_END_CMD ;
            exitMillis = millis() + 500 ; //expect a OK before 500 msec
          }
        }  
              
      } else if ( jog_status == JOG_WAIT_END_CMD  ) {
        if ( !waitOk ) {
          jog_status = JOG_NO ;
          jogCmdFlag = false ;    // remove the flag because cmd has been processed
        } else {
          if ( millis() >  exitMillis ) {  // si on ne reçoit pas le OK dans le délai maximum prévu
            jog_status = JOG_NO ; // reset all parameters related to jog .
            jogCancelFlag = false ;
            jogCmdFlag = false ;
            //Serial.println("no OK received within 500msec for Jog");
            if(lastMsg[0] || (lastMsg[0] == 32) ) fillMsg(_CMD_JOG_MISSING_OK  ) ; // put a message if there was no message (e.g. alarm:)
          }
        }
      } 
    }
} // end of function    





boolean sendJogCmd(uint32_t startTime) {
#define MINDIST 0.01    // mm
#define MINSPEED 10     // mm
#define MAXSPEEDXY MAX_XY_SPEED_FOR_JOGGING // mm/sec
#define MAXSPEEDZ 400   // mm/sec
#define DELAY_BEFORE_REPEAT_MOVE 500 //msec
#define DELAY_BETWEEN_MOVE 100       //msec
#define DELAY_TO_REACH_MAX_SPEED 2000 // msec
        float distanceMove ;
        char sdistanceMove[20];
        uint32_t speedMove ;
        char sspeedMove[20];
        int32_t counter = millis() - startTime ;
        //Serial.print("counter=") ; Serial.println(counter);
        if ( counter < 10 ) {
          distanceMove = MINDIST ;
          speedMove = MINSPEED ;
        } else {
          counter = counter - DELAY_BEFORE_REPEAT_MOVE ;
          if (counter < 0) {
            //Serial.println("counter neg");
            return false ;              // do not send a move; // false means that cmd has not been sent
          }
          if ( counter > (  DELAY_TO_REACH_MAX_SPEED - DELAY_BEFORE_REPEAT_MOVE) ) {
            counter = DELAY_TO_REACH_MAX_SPEED - DELAY_BEFORE_REPEAT_MOVE ;
          }
          if (jogDistZ ) {
            speedMove = MAXSPEEDZ ;
          } else {
            speedMove = MAXSPEEDXY ;
          } 
          speedMove = speedMove * counter / ( DELAY_TO_REACH_MAX_SPEED  - DELAY_BEFORE_REPEAT_MOVE ) ;
          if (speedMove < MINSPEED) {
            speedMove = MINSPEED;
          }
          distanceMove = speedMove * DELAY_BETWEEN_MOVE / 60000.0 * 1.2;   // speed is in mm/min and time in millisec.  1.2 is to increase a little the distance to be sure buffer is filled 
        }
        sprintf(sdistanceMove, "%.2f" , distanceMove); // convert to string
        //
        //Serial.println("send a jog") ;  
        bufferise2Grbl("$J=G91 G21" , 'b');
        //Serial2.print("$J=G91 G21") ;
        if (jogDistX > 0) {
          bufferise2Grbl(" X") ;
          //Serial2.print(" X") ;
        } else if (jogDistX ) {
          bufferise2Grbl(" X-") ;
          //Serial2.print(" X-") ;
        }
        if (jogDistX ) {
          bufferise2Grbl(sdistanceMove) ;
          //Serial2.print(distanceMove) ;
        }  
        if (jogDistY > 0) {
          bufferise2Grbl(" Y") ;
          //Serial2.print(" Y") ;
        } else if (jogDistY ) {
          bufferise2Grbl(" Y-") ;
          //Serial2.print(" Y-") ;
        }
        if (jogDistY ) {
          //Serial2.print(moveMultiplier) ;
          bufferise2Grbl(sdistanceMove) ;
          //Serial2.print(distanceMove) ;
        }
        if (jogDistZ > 0) {
          bufferise2Grbl(" Z") ;
          //Serial2.print(" Z") ;
        } else if (jogDistZ ) {
          bufferise2Grbl(" Z-") ;
          //Serial2.print(" Z-") ;
        }
        if (jogDistZ ) {
          bufferise2Grbl(sdistanceMove) ;
          //Serial2.print(distanceMove) ;
        }
        if (jogDistA > 0) {
          bufferise2Grbl(" A") ;
          //Serial2.print(" A") ;
        } else if (jogDistA ) {
          bufferise2Grbl(" A-") ;
          //Serial2.print(" A-") ;
        }
        if (jogDistA ) {
          bufferise2Grbl(sdistanceMove) ;
          //Serial2.print(distanceMove) ;
        }
        //Serial2.print(" F2000");  Serial2.print( (char) 0x0A) ;
        sprintf(sspeedMove, "%u" , speedMove); // convert to string integer
                
        bufferise2Grbl(" F"); bufferise2Grbl(sspeedMove);bufferise2Grbl("\n\r",'s') ;
        //Serial2.print(" F"); Serial2.print(speedMove); Serial2.print( (char) 0x0A) ;
        while (Serial2.availableForWrite() != 0x7F ) ;                        // wait that all char are sent 
        //Serial2.flush() ;       // wait that all char are really sent
        
        //Serial.print("Send cmd jog " ); Serial.print(distanceMove) ; Serial.print(" " ); Serial.print(speedMove) ;Serial.print(" " ); Serial.println(millis() - startTime );
        //Serial.print(prevMoveX) ; Serial.print(" " ); Serial.print(prevMoveY) ; Serial.print(" " ); Serial.print(prevMoveZ) ;Serial.print(" ") ; Serial.println(millis()) ;
        return true ; // true means that cmd has been sent
}

/*
void fillErrorMsg( const char * errorMsg ) {   // errorMsg contains "Error:xx"
   int errorNum = atoi( &errorMsg[6]) ;
   if (errorNum < 1 || errorNum > 38 ) errorNum = 0 ;
   fillMsg( errorArrayMsg[errorNum]  ) ;
}
void fillAlarmMsg( const char * alarmMsg ) {   //alarmMsg contains "ALARM:xx"
  int alarmNum = atoi( &alarmMsg[6]) ;
   if (alarmNum < 1 || alarmNum > 9 ) alarmNum = 0 ;
   fillMsg( alarmArrayMsg[alarmNum] ) ;
}
*/
void fillStringExecuteMsg( uint8_t buttonMessageIdx ) {   // buttonMessageIdx contains the number of the button
   //Serial.print("param= ") ; Serial.println(buttonMessageIdx ) ;  // to debug
   if ( buttonMessageIdx >= _SETX || buttonMessageIdx <= _GO_PROBE) {
      buttonMessageIdx -= _SETX ;
      //fillMsg( stringExecuteMsg[buttonMessageIdx] , BUTTON_TEXT ) ;
      fillMsg( buttonMessageIdx - _SETX +  _SETX_EXECUTED , SCREEN_NORMAL_TEXT ) ;
   } else {
      //buttonMessageIdx = _GO_PROBE - _SETX + 1 ;
      //fillMsg( stringExecuteMsg[buttonMessageIdx] ) ;
      fillMsg( _UNKNOWN_BTN_EXECUTED , SCREEN_NORMAL_TEXT ) ;
   }
   //Serial.print("index of table= ") ; Serial.println(buttonMessageIdx ) ;  // to debug
   //Serial.print("message= ") ; Serial.println( stringExecuteMsg[buttonMessageIdx] ) ; // to debug
}


void toGrbl(char c){  // send only one char to GRBL on Serial, Bluetooth or telnet 
  switch (grblLink) {
    case GRBL_LINK_SERIAL:
      Serial2.print(c);
      //Serial.print("send char="); Serial.println(c);
      break;
    case GRBL_LINK_BT :
      toBt(c);
      break;
    case GRBL_LINK_TELNET :
      toTelnet(c);
      break;
  }
}
void toGrbl(const char * data){ // send one string to GRBL on Serial, Bluetooth or telnet 
 switch (grblLink) {
    case GRBL_LINK_SERIAL:
      Serial2.print(data);
      Serial.print("send buffer="); Serial.println(data);
      break;
    case GRBL_LINK_BT :
      toBt(data);
      break;
    case GRBL_LINK_TELNET :
      toTelnet(data);
      //Serial.print("send buffer="); Serial.println(data);
      break;
  } 
}

void bufferise2Grbl(const char * data , char beginEnd){  // group data in a buffer before sending to grbl (via Serial, Bluetooth ot telnet)
                                                          //if beginEnd = 'b', clear the fuffer before writing
                                                          // if 's', send after bufferising
                                                          // if 'w' send after bufferising and wait that it has been sent (not implemented yet
                                                          // other, just write
                                                          // to do; return a bool to say if done or not
  static char buffer[255];
  static uint16_t bufferIdx = 0;
  if (beginEnd == 'b') {
    bufferIdx = 0;
    buffer[bufferIdx] = '\0' ;
    //Serial.println("begin to buffer");
    //delay(100);
  }
  uint8_t i = 0;
  while ( ( data[i] != '\0') && ( i < 254 ) && (bufferIdx < 255 )) {
    buffer[bufferIdx] = data[i];
    bufferIdx++;
    buffer[bufferIdx] = '\0' ;
    i++;
  }
  //Serial.print("buffer=") ; Serial.println(buffer);
  //delay(100);
  if (beginEnd == 's') {
    Serial.print("sending buf="); Serial.print(buffer); Serial.println("EndOfText");
    toGrbl(buffer) ;
  }
}


int fromGrblAvailable(){    // return the number of char in the read buffer
  switch (grblLink) {
    case GRBL_LINK_SERIAL:
      //Serial.print("serial2 Avail="); Serial.println(Serial2.available());
      return Serial2.available();
      break;
    case GRBL_LINK_BT :
      //Serial.println("BT available?");
      return SerialBT.available();
      break;
    case GRBL_LINK_TELNET :
      return fromTelnetAvailable();
      break;
  } 
}

int fromGrblRead(){       // return the first character in the read buffer
  switch (grblLink) {
    case GRBL_LINK_SERIAL:
      
      return Serial2.read();
    case GRBL_LINK_BT :
      //Serial.println("read BT"); 
      return SerialBT.read();
    case GRBL_LINK_TELNET :
      return fromTelnetRead();
  }
}


void startGrblCom(uint8_t comMode){
  preferences.putChar("grblLink", comMode) ;
  // First stop BT or Telnet
  if (grblLink == GRBL_LINK_BT){
    //Serial.println("in startGrblCom we will stop bt");
    //delay(200);
    btGrblStop();
    //Serial.println("in startGrblCom bt has been stopped");
    //delay(200); 
  } else if (grblLink == GRBL_LINK_TELNET) {
    telnetGrblStop();
  }
  if (comMode == GRBL_LINK_SERIAL) {
    grblLink = GRBL_LINK_SERIAL ;
    while (Serial2.available()>0) Serial2.read() ; // clear the serial2 buffer
    //fillMsg(_GRBL_SERIAL_CONNECTED);  
  } else if (comMode == GRBL_LINK_BT) {
    grblLink = GRBL_LINK_BT ;
    //Serial.println("in startGrblCom we call btgrblInit");
    //delay(200);
    btGrblInit();
  } else if (comMode == GRBL_LINK_TELNET) {
    grblLink = GRBL_LINK_TELNET ;
    telnetGrblInit();
  }  
}


/*

BluetoothSerial SerialBT;
int i = 0;


//String MACadd = "AA:BB:CC:11:22:33";
//uint8_t address[6]  = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
////uint8_t address[6]  = {0x00, 0x1D, 0xA5, 0x02, 0xC3, 0x22};
String name = "ESP32_BT"; //                  <------- set this to be the name of the other ESP32!!!!!!!!!
//char *pin = "1234"; //<- standard pin would be provided by default
bool btConnected;

void btInit() {
  SerialBT.begin("ESP32testm", true); 
  //SerialBT.setPin(pin);
  Serial.println("The device started in master mode, make sure remote BT device is on!");
  
  // connect(address) is fast (upto 10 secs max), connect(name) is slow (upto 30 secs max) as it needs
  // to resolve name to address first, but it allows to connect to different devices with the same name.
  // Set CoreDebugLevel to Info to view devices bluetooth address and device names
  btConnected = SerialBT.connect(name);
  //connected = SerialBT.connect(address);
  
  if(btConnected) {
    Serial.println("Connected Succesfully in BT!");
  } else {
    while(!SerialBT.connected(10000)) {
      Serial.println("Failed to connect. Make sure remote device is available and in range, then restart app."); 
    }
  }
  // disconnect() may take upto 10 secs max
  if (SerialBT.disconnect()) {
    Serial.println("Disconnected Succesfully from BT!");
  }
  // this would reconnect to the name(will use address, if resolved) or address used with connect(name/address).
  SerialBT.connect();
}
*/
