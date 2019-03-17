
/**************************************************
 In acest sketch se gasesc functiile utilitare 
 si majoritatea declaratiilor de constante
 pentru proiectul Ceas
 ***************************************************/
 
 
//Pentru Timer
#define TIMER_ON_BIT 15

//Din TimeLib.cpp:
// leap year calulator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)     ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )


#define cs   10
#define dc   9
#define rst  8

//Numarul zilele din fiecare luna
const uint8_t monthDays[]  ={31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0

//Pentru a nu include DateString.cpp, acolo este definit
const char WeekdayNames[]  = "ErrSunMonTueWedThuFriSat";
const uint8_t cWkDyNmLngth = 3;


//Variabilele de automat secvential pot fi modificate
//in mai multe locuri in program, deci sunt declarate volatile
extern volatile uint8_t MODE_STATE;
extern volatile uint8_t SET_STATE;

extern volatile uint8_t CLOCK_STATE;

//Variabile pentru baza de timp si blink, volatile, deoarece se modifica
//in intreruperea de timer
extern volatile unsigned int TimeBase_Divider;
extern volatile uint8_t TimeBase_Tick; //Flag pentru a semnaliza ca trebuie incrementate secundele

extern volatile unsigned int Blink_Divider;
extern volatile uint8_t Blink_VAL; //Daca e 0, trebuie afisat elementul, daca e 1, trebuie stins
extern volatile uint8_t Blink_Tick; //Flag pentru a semnaliza ca a venit timpul sa stingem vreun element, brin blink



//Structura tmElements este definit in TimeLib.h inclus de Time.h
/*
typedef struct  { 
  uint8_t Second; 
  uint8_t Minute; 
  uint8_t Hour; 
  uint8_t Wday;   // day of week, sunday is day 1
  uint8_t Day;
  uint8_t Month; 
  uint8_t Year;   // offset from 1970; 
}   tmElements_t, TimeElements, *tmElementsPtr_t;
*/

//Aici tinem minte ceasul si calendarul
extern tmElements_t TimeDate;
extern tmElements_t * pTimeDate;


//Sirurile de afisat pe IOShieldOled
extern String TimeDispStr;
extern String DateDispStr;
extern String * pTimeStr;
extern String * pDateStr;


/****************************************
 * Corpurile functiilor utilitare
  ***************************************/

void Init_Peripherals(){
		
//Btn va fi intrare
for ( i = 0; i < 4; i++) pinMode(Btn_IOs[i], INPUT);
  IOShieldOled.begin();
  //Clear the virtual buffer
  IOShieldOled.clearBuffer();
  //Turn automatic updating off
  IOShieldOled.setCharUpdate(0);
}  

//Din RtcDateTime.cpp
//Transformam string in uint8_t
unsigned int StringToUint(const char* pString)
{
    unsigned int value = 0;

    // skip leading 0 and spaces
    while ('0' == *pString || *pString == ' ')
    {
        pString++;
    }

    // calculate number until we hit non-numeral char
    while ('0' <= *pString && *pString <= '9')
    {
        value *= 10;
        value += *pString - '0';
        pString++;
    }
    return value;
}


//Setam ceasul
void SetDateandTime(const char* date, const char* time, tmElements_t* pTmDt)
{
unsigned int CurrentYear;	
unsigned long CurrentSeconds;
	
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    CurrentYear = StringToUint(date + 7);
  	pTmDt->Year = CalendarYrToTm(CurrentYear);

	// Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (date[0])
    {
    case 'J':
        if ( date[1] == 'a' )
	        pTmDt->Month = 1;
	    else if ( date[2] == 'n' )
	        pTmDt->Month = 6;
	    else
	        pTmDt->Month = 7;
        break;
    case 'F':
        pTmDt->Month = 2;
        break;
    case 'A':
        pTmDt->Month = date[1] == 'p' ? 4 : 8;
        break;
    case 'M':
        pTmDt->Month = date[2] == 'r' ? 3 : 5;
        break;
    case 'S':
        pTmDt->Month = 9;
        break;
    case 'O':
        pTmDt->Month = 10;
        break;
    case 'N':
        pTmDt->Month = 11;
        break;
    case 'D':
        pTmDt->Month = 12;
        break;
    }
    pTmDt->Day = StringToUint(date + 4);
    pTmDt->Hour = StringToUint(time);
    pTmDt->Minute = StringToUint(time + 3);
    pTmDt->Second = StringToUint(time + 6);

//Adjust day of the week 
	CurrentSeconds = makeTime(TimeDate);
	breakTime(CurrentSeconds, TimeDate);
	
}


  
//Aceasta functie formeaza si afiseaza sirurile de caractere care trebuie afisate
void FormatandDispStr (String * pTmStr, String * pDtStr, tmElements_t * pTmDt){

    //Daca ora < 9, nu afisam zeroul din fata
    String TempHrStr = (pTmDt->Hour < 10) ? " " + String(pTmDt->Hour, DEC) : String(pTmDt->Hour, DEC);
    //La minute si secunte afisam 0 in fata
    String TempMinStr = (pTmDt->Minute < 10) ? "0" + String(pTmDt->Minute, DEC) : String(pTmDt->Minute, DEC);
    String TempSecStr = (pTmDt->Second < 10) ? "0" + String(pTmDt->Second, DEC) : String(pTmDt->Second, DEC);
    
    //Afisam zeroul din fata pentru ziua sia luna
    String TempDayStr = (pTmDt->Day < 10) ? "0" + String(pTmDt->Day, DEC) : String(pTmDt->Day, DEC);
    String TempMonthStr = (pTmDt->Month < 10) ? "0" + String(pTmDt->Month, DEC) : String(pTmDt->Month, DEC);
    
    //Pentru ziua saptamanii trebuie sa cautam caracterul corespunzator din WeekdayNames[]
    //Folosim functia Substring
    String TempWkdayNames = String(WeekdayNames);
    //de la 3 * numarul zilei din saptamanii pana la (3 * numarul zilei din saptamana +1) -1) 
    String TempWkdayName = TempWkdayNames.substring(cWkDyNmLngth * pTmDt->Wday, (cWkDyNmLngth * (pTmDt->Wday + 1)));

    //tmYearToCalendar(Y) definit in TimeLib.h - ii adauga 1970
    String TempYearStr = String (tmYearToCalendar(pTmDt->Year), DEC);

    //Asamblam sirurile de afisat, prima data orele
    *pTmStr = TempHrStr + ":" + TempMinStr + ":" + TempSecStr;
    //Apoi data
    *pDtStr = TempWkdayName + ", " + TempDayStr + "/" + TempMonthStr + "/" + TempYearStr; 

    //Si acum, afisam
    IOShieldOled.clearBuffer();
    IOShieldOled.setCursor(cTimeDispCol, cTimeDispRow);
    IOShieldOled.putString((char *)pTmStr->c_str());
    IOShieldOled.setCursor(cDateDispCol, cDateDispRow);
    IOShieldOled.putString((char *)pDtStr->c_str());
    IOShieldOled.updateDisplay();    
  
}


//Cu aceasta functie afisez calendarul
void FormatandDisplayCalendar(String * pWkStrLn1, String * pWkStrLn2, tmElements_t * pTmDt){



SecondTimeDate = TimeDate;
tmElements_t * pSecTimeDate = &SecondTimeDate;

char DayStr[2];
char * pDayStr = DayStr;
unsigned long CurrentSeconds;

* pWkStrLn1 ="              ";

* pWkStrLn2 ="              ";

while (SecondTimeDate.Wday > 1)
{
        CurrentSeconds = makeTime(SecondTimeDate);
        CurrentSeconds = CurrentSeconds - SECS_PER_DAY;
        breakTime(CurrentSeconds, SecondTimeDate);
}
//Acuma suntem in ziua de duminica


for(i = 0; i <7; i++)
{

    String TempDayStr = (pSecTimeDate->Day < 10) ? " " + String(pSecTimeDate->Day, DEC) : String(pSecTimeDate->Day, DEC);
    for (int i = 0; i < 2; i++) *(pDayStr +i) = * ((char *)TempDayStr.c_str()+ i);
    pWkStrLn1->setCharAt(((pSecTimeDate->Wday - 1) * 2), DayStr[0]);
    pWkStrLn1->setCharAt(((pSecTimeDate->Wday - 1) * 2) + 1, DayStr[1]);

    CurrentSeconds = makeTime(SecondTimeDate);
    CurrentSeconds = CurrentSeconds + SECS_PER_DAY;
    breakTime(CurrentSeconds, SecondTimeDate);
}
//Acum suntem in ziua de duminica a doua saptamana



while (SecondTimeDate.Wday > 1)
{
        CurrentSeconds = makeTime(SecondTimeDate);
        CurrentSeconds = CurrentSeconds - SECS_PER_DAY;
        breakTime(CurrentSeconds, SecondTimeDate);
}

for(i = 0; i <7; i++)
{

    String TempDayStr = (pSecTimeDate->Day < 10) ? " " + String(pSecTimeDate->Day, DEC) : String(pSecTimeDate->Day, DEC);
    for (int i = 0; i < 2; i++) *(pDayStr +i) = * ((char *)TempDayStr.c_str()+ i);
    pWkStrLn2->setCharAt(((pSecTimeDate->Wday - 1) * 2), DayStr[0]);
    pWkStrLn2->setCharAt(((pSecTimeDate->Wday - 1) * 2) + 1, DayStr[1]);

    CurrentSeconds = makeTime(SecondTimeDate);
    CurrentSeconds = CurrentSeconds + SECS_PER_DAY;
    breakTime(CurrentSeconds, SecondTimeDate);
}

   
    IOShieldOled.clearBuffer();
    IOShieldOled.setCursor(0, 0);
    IOShieldOled.putString("[");
    IOShieldOled.setCursor(1, 0);
    /*IOShieldOled.putString((char *)pWkStrLn1->charAt(0));
    IOShieldOled.putString((char *)pWkStrLn1->charAt(1));
        IOShieldOled.putString("]");
IOShieldOled.putString((char *)pWkStrLn1->c_str()+2);*/
    IOShieldOled.putString((char *)pWkStrLn1->c_str());
    IOShieldOled.setCursor(0, 1);
    IOShieldOled.putString((char *)pWkStrLn2->c_str());
    
    IOShieldOled.updateDisplay(); 
  
}



//Aceasta functie afiseaza cu Blink elementul reglat sau pe ":" la functionare normala
void BlinkSelectedElement (uint8_t index)
{ 
    //Daca Blink_VAL == 0 atunci nu avem nimic de stins
    if(!Blink_VAL) return;
    
    String TempBlank = "";

    //Creem un sir de caractere goale de lungimea elementului de stins
    for (int i = 0; i < DispPositions[index].len; i++) TempBlank = TempBlank + " ";
    
    //Si acum afisam blank pe pozitia care trebuie stinsa
    IOShieldOled.setCursor(DispPositions[index].column, DispPositions[index].row);
    IOShieldOled.putString((char *)TempBlank.c_str());
    IOShieldOled.updateDisplay(); 
  
}

/******************************************************************************
//Vom folosi o functie separata pentru a determina
//numarul zilelor din luna, pe baza monthDays[]
//La numarul zilelor din calendar trebuie tinut cont si de luna si de anul bisect
//In Time.cpp este definit 
//static  const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; 
********************************************************************************/
uint8_t MonthLength(tmElements_t * pTmDt){

uint8_t MLength = 0; //numarul zilelor dintr-o luna

    if (pTmDt->Month==2) { // februarie
      if (LEAP_YEAR(pTmDt->Year)) { MLength=29;} 
      else {MLength=28;}
    } 
    else {MLength = monthDays[pTmDt->Month - 1];}

  return MLength;
}

//Aceasta functie incrementeaza ceasul, in mers normal
void incDateTime (tmElements_t * pTmDt){

//Ar fi excelent de realizat cu breakTime (makeTime(TimeDate) +1, TimeDate), dar
//dorim sa lasam Wday sa poata fi liber ajustat, ca sa creem un calendar nereal, vezi tema de proiect 3.1

  pTmDt->Second++;
  if (pTmDt->Second == 60) {pTmDt->Second=0; pTmDt->Minute++;}
  if (pTmDt->Minute == 60) {pTmDt->Minute=0; pTmDt->Hour++;}
  if (pTmDt->Hour == 24) {pTmDt->Hour=0; pTmDt->Day++; pTmDt->Wday++;}
  if (pTmDt->Wday == 8) {pTmDt->Wday=1;}//Prima zi a saptamanii este duminica
  //Ajustam zilele din luna
  if (pTmDt->Day == (MonthLength(pTimeDate) + 1)) {pTmDt->Day=1; pTmDt->Month++;}
  if (pTmDt->Month == 13) {pTmDt->Month=1; pTmDt->Year++;}
 }

//Aceste functii incrementeaza elementele de timp si calendar in meniul de setare
void SetSec(tmElements_t * pTmDt){
  //Secundele doar se reseteaza
  pTmDt->Second = 0;
  //Resetam insa si TimeBase_Divider
  TimeBase_Divider = 0;
}
void SetMin(tmElements_t * pTmDt) {
 pTmDt->Minute++;
 if (pTmDt->Minute == 60) pTmDt->Minute=0;
}

void SetHR(tmElements_t * pTmDt){
 pTmDt->Hour++;
 if (pTmDt->Hour == 24) pTmDt->Hour=0;

}
void SetWday(tmElements_t * pTmDt){
 pTmDt->Wday++;
 if (pTmDt->Wday == 8) pTmDt->Wday=1;//Prima zi a saptamanii este duminica
}
void SetDay(tmElements_t * pTmDt){
    
    pTmDt->Day++;
    //Ajustam zilele din luna
    if (pTmDt->Day == (MonthLength(pTimeDate) + 1)) {pTmDt->Day=1;}

}
void SetMonth(tmElements_t * pTmDt){
  pTmDt->Month++;
  if (pTmDt->Month == 13) {pTmDt->Month=1;}
}
void SetYear(tmElements_t * pTmDt){
  pTmDt->Year++;

}

//Pentru a verifica si a corecta ziua din luna curenta
//de ex. daca ziua e 31 si s-a schimbat luna de pe 3 pe 4
//sau daca ziua e 29, luna e Februarie si s-a schimbat anul de pe bisect pe ne-bisect
void  CheckandCorrectDate(tmElements_t * pTmDt){
  
  if (pTmDt->Day > MonthLength(pTimeDate)){
    //Adaugam zilele pentru luna urmatoare
    pTmDt->Day=pTmDt->Day - MonthLength(pTimeDate);
    pTmDt->Month++;
    }
}
/*
void Weeks() {
  current_week

  next_week
}

void drawCalendar(){ 
   // display a full month on a calendar  
   u8g.setFont(u8g_font_profont12); 
   u8g.drawStr(2,9, "Su Mo Tu We Th Fr Sa");  
   // display this month 
   DateTime now = RTC.now(); 
   // 
   // get number of days in month 
   if (pTmDt.month() == 1 || pTmDt.month() == 3 || pTmDt.month() == 5 || pTmDt.month() == 7 || pTmDt.month() == 8 || pTmDt.month() == 10 || pTmDt.month() == 12){ 
     monthLength = 31; 
   } 
   else {monthLength = 30;} 
   if(pTmDt.month() == 2){monthLength = 28;} 
   startDay = startDayOfWeek(pTmDt.year(),pTmDt.month(),1); // Sunday's value is 0 
   // now build first week string 
  switch (startDay){ 
     case 0: 
       // Sunday 
      week1 = " 1  2  3  4  5  6  7"; 
       break; 
     case 1: 
       // Monday 
       week1 = "    1  2  3  4  5  6"; 
       break;       
      case 2: 
       // Tuesday 
       week1 = "       1  2  3  4  5"; 
       break;            
      case 3: 
       // Wednesday 
       week1 = "          1  2  3  4"; 
       break;   
      case 4: 
       // Thursday 
       week1 = "             1  2  3"; 
      break;  
      case 5: 
       // Friday 
       if(monthLength == 28 || monthLength == 30){week1 = "                1  2";}       
       if(monthLength == 31){week1 = "31              1  2";}       
       break;  
      case 6: 
       // Saturday 
       if(monthLength == 28){week1 = "                   1";} 
       if(monthLength == 30){week1 = "30                 1";}       
       if(monthLength == 31){week1 = "30 31              1";}        
        
       break;            
   } // end first week 
  newWeekStart = (7-startDay)+1; 
   const char* newWeek1 = (const char*) week1.c_str();   
   u8g.drawStr(2,19,newWeek1);  
   // display week 2 
   week2 =""; 
   for (int f = newWeekStart; f < newWeekStart + 7; f++){ 
     if(f<10){ 
       week2 = week2 +  " " + String(f) + " "; 
     }   
     else{week2 = week2 + String(f) + " ";}     
   } 
   const char* newWeek2 = (const char*) week2.c_str();   
   u8g.drawStr(2,29,newWeek2);  
   // display week 3 
   newWeekStart = (14-startDay)+1;  
   week3 =""; 
   for (int f = newWeekStart; f < newWeekStart + 7; f++){ 
    if(f<10){ 
       week3 = week3 +  " " + String(f) + " "; 
     }   
     else{week3 = week3 + String(f) + " ";}     
   } 
   const char* newWeek3 = (const char*) week3.c_str();   
   u8g.drawStr(2,39,newWeek3);      
   // display week 4 
   newWeekStart = (21-startDay)+1;  
   week4 =""; 
   for (int f = newWeekStart; f < newWeekStart + 7; f++){ 
     if(f<10){ 
       week4 = week4 +  " " + String(f) + " "; 
     }   
     else{week4 = week4 + String(f) + " ";}     
     } 
    const char* newWeek4 = (const char*) week4.c_str();   
    u8g.drawStr(2,49,newWeek4);  
    // do we need a fifth week 
    week5=""; 
    newWeekStart = (28-startDay)+1;    
    // is is February? 
    if(newWeekStart > 28 && now.month() == 2){ 
    // do nothing unless its a leap year 
      if (now.year()==(now.year()/4)*4){ // its a leap year 
        week5 = "29"; 
      }        
    } 
    else{ // print up to 30 anyway 
      if(now.month() == 2){  // its February 
        for (int f = newWeekStart; f < 29; f++){ 
          week5 = week5 + String(f) + " ";   
     }   
        // is it a leap year 
        if (now.year()==(now.year()/4)*4){ // its a leap year 
          week5 = week5 + "29"; 
        }         
      } 
      else{ 
        for (int f = newWeekStart; f < 31; f++){ 
          week5 = week5 + String(f) + " "; 
        } 
        // are there 31 days 
        if (monthLength == 31 && week5.length() <7){ 
           week5 = week5 + "31"; 
          }
        }  
      }  
    } 

*/
void ConfigureandStartTimer4(){

//Folosim Timer4 pentru baza de timp
T4CON = 0;

//Ne trebuie o perioada de 100us, adica 10KHz
//Pentru a diviza CLK sistem de 80MHz, ajunge o rata de divizare de 8000
//Deci prescaler = 1
PR4 = ((getPeripheralClock()/TIMER_TICK_FREQUENCY_HZ) -1);

//Configuram intreruperile si rutina de intrerupere pentru Timer 4
/***********************************************************
Definitiile acestor constante le gasiti in
..\portable\packages\chipKIT\tools\pic32-tools\1.43-pic32gcc\pic32mx\include\proc\p32mx795f512l.h
si ..\portable\packages\chipKIT\hardware\pic32\2.0.3\cores\pic32\System_Defs.h
************************************************************/
setIntVector(_TIMER_4_VECTOR, Timer4Handler); //Functia care sa fie chemata la intrerupere
clearIntFlag(_TIMER_4_IRQ);//Daca exista cerere de intrerupere, prima data sterge-l
setIntPriority(_TIMER_4_VECTOR, _T4_IPL_IPC, _T4_SPL_IPC); //Seteaza prioritatea si subprioritatea intreruperii
setIntEnable(_TIMER_4_IRQ); //Permite intreruperile de la Timer4

//Pornim Timer
T4CONSET = 1 << TIMER_ON_BIT;

}


