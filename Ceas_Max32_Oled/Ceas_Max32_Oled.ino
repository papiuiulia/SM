

/**************************************************
 * Atentie!
 Functiile utilitare si majoritatea declaratiilor de constante
 se gasesc in Ceas_Utility.ino
 In sketch - ul prezent sunt functiile: 
	- Intreruprerea de timer
	- Task-ul care verifica butoanele si state machine aferente
	- State machine pentru meniul de ceas
 ***************************************************/

/****************************************
 * Incluziuni
 ****************************************/
#include <IOShieldOled.h>
#include <String.h>
#include <Time.h>
#include "TimeLib.h" //trebuie inclus explicit, desi il include Time.h
/****************************************
 * Definitii de constante
 ****************************************/
//Timer0 interupt este cel mai prioritar, acesta va da baza de timp
//Va fi setat la o perioada de 100uS (frecventa de 10KHz)
//De aici trebuie divizat la 1S
#define TIMER_TICK_FREQUENCY_HZ 10000
#define TIMEBASE_1HZ_DIVIDE 10000
//Stabilim frecventa Blink a ":"  la 2Hz (sa se aprinda cand se modifica secunda)
#define RUNNING_BLINK_DIVIDE 5000

//Stabilim frecventa Blink a elementelor de ajustat la 4Hz (sa se aprinda cand se modifica secunda)
#define ADJUST_BLINK_DIVIDE 2500

//Randul si coloana de unde incepe afisarea orei si datei
const uint8_t cTimeDispRow  = 1;
const uint8_t cTimeDispCol  = 4;
const uint8_t cDateDispRow  = 2;
const uint8_t cDateDispCol  = 0;
 
//Pinii corespunzatori butoanelor de pe IOShield, conectat la MAX32, BTN1 primul
//atributul : Compilatorul le va stoca in memoria program (FLASH), unde este mai mult loc
const uint8_t Btn_IOs[4]  = {4,78,80,81};
const uint8_t MODE_BTN  = Btn_IOs[2];
const uint8_t SET_BTN  = Btn_IOs[1];
const uint8_t CAL_BTN  = Btn_IOs[3];

//Stari ale automatelor secventiale
//Butoane
#define MODE_RELEASED 0
#define MODE_PRESSED 1
#define MODE_CLICK 2

#define SET_RELEASED 0
#define SET_PRESSED 1
#define SET_CLICK 2

//Ceasul are doar doua stari: Rulare normala si meniul de ajustare
#define CLOCK_RUNNING 0
#define CLOCK_ADJ_MODE 1

typedef struct {
   uint8_t row;
   uint8_t column;
   uint8_t len;
} ElementPosLen;

//Ceasul este afisat in forma      "    HH:MM:SS    "
//Pozitiile                         0123456789111111
//                                            012345
//Calendarul estea afisat [n forma "zzz, ZZ/LL/AAAA "
//Pozitiile                         0123456789111111
//                                            012345
const ElementPosLen DispPositions[8] PROGMEM =
{
    {cTimeDispRow,10, 2}, //Sec
    {cTimeDispRow, 7, 2}, //Min
    {cTimeDispRow, 4, 2}, //Hr
    {cDateDispRow, 0, 3}, //Wday
    {cDateDispRow, 5, 2}, //Day
    {cDateDispRow, 8, 2}, //Month
    {cDateDispRow,11, 4}, //Year
    {cTimeDispRow, 6, 1}  //":", ultimul element din lista
};
const uint8_t BlinkingColonPos = 7;


//Tabelul pentru indexul elementelor de timp
//Acesta determina, care element e selectat si in ce ordine
const uint8_t TblElIndex[8]  = {0, 2, 1, 4, 5, 6, 3, 7};
uint8_t ElIndex = 7; //Blink va fi la inceput pe ":", adica rand 0, coloana 6


/****************************************
 * Definitii de variabile
 ****************************************/
//Variabilele de automat secvential pot fi modificate
//in mai multe locuri in program, deci sunt declarate volatile
volatile uint8_t MODE_STATE = MODE_RELEASED;
volatile uint8_t SET_STATE = SET_RELEASED;

volatile uint8_t CLOCK_STATE = CLOCK_RUNNING;

//Variabile pentru baza de timp si blink, volatile, deoarece se modifica
//in intreruperea de timer
volatile unsigned int TimeBase_Divider =0;
volatile uint8_t TimeBase_Tick = 0; //Flag pentru a semnaliza ca trebuie incrementate secundele

volatile unsigned int Blink_Divider =0;
volatile uint8_t Blink_VAL = 0; //Daca e 0, trebuie afisat elementul, daca e 1, trebuie stins
volatile uint8_t Blink_Tick = 0; //Flag pentru a semnaliza ca a venit timpul sa stingem vreun element, brin blink


int i = 0;

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
static tmElements_t TimeDate;
tmElements_t * pTimeDate = &TimeDate;

static tmElements_t SecondTimeDate;


//Sirurile de afisat pe IOShieldOled
static String TimeDispStr;
static String DateDispStr;
String * pTimeStr = &TimeDispStr;
String * pDateStr = &DateDispStr;

static String WkDisplayStrLn1 = "              ";
static String WkDisplayStrLn2 = "              ";
String * pWkDspStrLn1 = &WkDisplayStrLn1;
String * pWkDspStrLn2 = &WkDisplayStrLn2;

//Task id pentru task
int ChkBtnStateMachine_id;

//Variabila pentru task
//Nu va fi folosit explicit, dar trebuie sa rezervam spatiu in memorie
unsigned long ChkBtnStateMachine_var;


/****************************************
 * Prototipuri de functii
 * Majoritatea acestor functii se gasesc
 * in Ceas_Utility.ino
 ***************************************/

//Initializam perifericele
void Init_Peripherals(); 

//Functia care seteaza ceasul la timpul compilarii
void SetDateandTime(const char* date, const char* time, tmElements_t * pTmDt);
 
//Aceasta functie formeaza si afiseaza sirurile de caractere care trebuie afisate
void FormatandDispStr (String * pDtTmStr, tmElements_t * pTmDt);

//Aceasta functie afiseaza cu Blink elementul reglat sau pe ":" la functionare normala
//De fapt, va stinge doar elementul de "blinkat"
void BlinkSelectedElement (uint8_t index);

//Vom folosi o functie separata pentru a determina
//numarul zilelor din luna, pe baza monthDays[]
uint8_t MonthLength(tmElements_t * pTmDt);

//Aceasta functie incrementeaza timpul, in mers normal
void incDateTime (tmElements_t * pTmDt);

//Aceste functii incrementeaza elementele de timp si calendar
//in meniul de setare
void SetSec(tmElements_t * pTmDt);
void SetMin(tmElements_t * pTmDt);
void SetHR(tmElements_t * pTmDt);
void SetWday(tmElements_t * pTmDt);
void SetDay(tmElements_t * pTmDt);
void SetMonth(tmElements_t * pTmDt);
void SetYear(tmElements_t * pTmDt);

//Pentru a verifica si a corecta ziua din luna curenta
//de ex. daca ziua e 31 si s-a schimbat luna de pe 3 pe 4
//sau daca ziua e 29, luna e Februarie si s-a schimbat anul de pe bisect pe ne-bisect
void  CheckandCorrectDate(tmElements_t * pTmDt); 

//Citirea butoanelor vom face un task-uri,
//iar pentru baza de timp folosim intreruperile de timer

//Functia care configureaza si porneste Timer 4
void ConfigureandStartTimer4();

//Functia de intrerupere, care genereaza baza de timp
void __USER_ISR Timer4Handler(void);

//Functia care testeaza butoanele 
//si ruleaza automatele secventiale corespunzatoare
//va fi rulat ca task din 10 in 10 milisecunde
void ChkBtnStateMachine_task(int id, void * tptr);


//Automatul secvential pentru ceas - 2 stari: CLOCK_RUNNING si CLOCK_ADJ_MODE
void StateMachine_WATCH();

//Cu aceasta functie afisez calendarul
void FormatandDisplayCalendar(String * pWkStrLn1, String * pWkStrLn2, tmElements_t * pTmDt);


void setup()
{
//Initializam butoanele si OLed
Init_Peripherals();

//Serial.begin(9600);

//Cream task-ul, care verifica starea butoanelor
//si ruleaza automatele secventiale corespunzatoare
//Ajunge sa ruleze odata la 1/10 secunde
  //createTask format: int createTask(taskFunc task, unsigned long period in ms, unsigned short stat, void * var)
  //task    :  Pointer to the task function
  //period   :  Scheduling interval in milliseconds
  //state    :  Task initial enable state
  //var    :  Initial value for task variable
ChkBtnStateMachine_id = createTask(ChkBtnStateMachine_task, 10, TASK_ENABLE, &ChkBtnStateMachine_var);

//Setam valoare initiala a ceasului la timpul compilarii
SetDateandTime(__DATE__, __TIME__, pTimeDate);

//Pornim Timer4
ConfigureandStartTimer4();

//Afisam prima data ceasul si calendarul
FormatandDispStr(pTimeStr, pDateStr, pTimeDate);

}

void loop()
{

 
//Aici doar afisam timpul
uint8_t DisplayNewTime = ( (TimeBase_Tick) || ( Blink_Tick && (!Blink_VAL)) );
uint8_t BlinkElement = ( (Blink_Tick) && (Blink_VAL) );

if (digitalRead(CAL_BTN)){

      FormatandDisplayCalendar( pWkDspStrLn1, pWkDspStrLn2, pTimeDate);
}
else{
      if (DisplayNewTime){//Afisam ori cand se schimba timpul  - la fiecare secunda
      					// Ori cand trebuie reafisat un element, dupa ce a fost stins cu blink
      	FormatandDispStr(pTimeStr, pDateStr, pTimeDate);
      	if (TimeBase_Tick) TimeBase_Tick = 0;
      	if (Blink_Tick) Blink_Tick = 0;
      }
      
      if (BlinkElement){//Stingem elementul pe care se face blink
      	BlinkSelectedElement(TblElIndex[ElIndex]);
      	if (Blink_Tick) Blink_Tick = 0;
      }
}
}



//Functia de intrerupere, care genereaza baza de timp
void __USER_ISR Timer4Handler(void){
    //Incrementam divizorul de baza de timp
    //Daca FAST_BTN este apasat, baza de timp creste de 20 ori
    //if (digitalRead(FAST_BTN)) TimeBase_Divider = TimeBase_Divider + 20;
   TimeBase_Divider++;
    
    if (TimeBase_Divider >= TIMEBASE_1HZ_DIVIDE){// a trecut o secunda
        TimeBase_Divider = 0;
        Blink_Divider = 0; //resincronizam Blink
        Blink_VAL = 0;
        incDateTime(pTimeDate); //incrementam ceasul    
        TimeBase_Tick = 1; //Anuntam ca trebuie afisat noul timp
    }
    
    //Incrementam divizorul pentru blink
    Blink_Divider++;
    if (CLOCK_STATE == CLOCK_ADJ_MODE){
        if (Blink_Divider >= ADJUST_BLINK_DIVIDE){
            Blink_Divider = 0;
            Blink_VAL = !Blink_VAL; //schimbam: daca s-a afisat, trebuie stins si invers
            Blink_Tick = 1; //Anuntam ca trebuie afisat din nou pentru blink
        }
    }
    else if (CLOCK_STATE == CLOCK_RUNNING){
        if (Blink_Divider >= RUNNING_BLINK_DIVIDE){
            Blink_Divider = 0;
            Blink_VAL = !Blink_VAL; //schimbam: daca s-a afisat, trebuie stins si invers
            Blink_Tick = 1; //Anuntam ca trebuie afisat din nou pentru blink
        }
    }
    
    //Interrupt acknowledge: Stergem flag-ul de intrerupere
    clearIntFlag(_TIMER_4_IRQ);
}

void ChkBtnStateMachine_task(int id, void * tptr){

uint8_t MODE_BTN_VAL = digitalRead(MODE_BTN);
uint8_t SET_BTN_VAL = digitalRead(SET_BTN);

//Automatul secvential pentru butonul MODE
    switch (MODE_STATE){
    case MODE_RELEASED : if (MODE_BTN_VAL) MODE_STATE = MODE_PRESSED;
                         break;
    case MODE_PRESSED  : if (!MODE_BTN_VAL) MODE_STATE = MODE_CLICK;
                         break;
    case MODE_CLICK    : //Trebuie chemat automatul secvential de ceas
                        StateMachine_WATCH();
                        MODE_STATE = MODE_RELEASED;
                        break;                  
    default: MODE_STATE = MODE_RELEASED;
             break;
    }

    switch (SET_STATE){
    case SET_RELEASED : if (SET_BTN_VAL) SET_STATE = SET_PRESSED;
                        break;
    case SET_PRESSED  : if (!SET_BTN_VAL) SET_STATE = SET_CLICK;
                        break;
    case SET_CLICK    : //Trebuie chemat automatul secvential de ceas
                        StateMachine_WATCH();
                        SET_STATE = SET_RELEASED;
                        break;                  
    default: SET_STATE = SET_RELEASED;
             break;
    }

}


//Automatul secvential pentru ceas - 2 stari: CLOCK_RUNNING si CLOCK_ADJ_MODE
void StateMachine_WATCH(){
 unsigned long CurrentSeconds;
 
    switch(CLOCK_STATE){
    case CLOCK_RUNNING:  if (MODE_STATE == MODE_CLICK){
                         CLOCK_STATE = CLOCK_ADJ_MODE;
                         ElIndex = 0; //Intram in modul de ajustare, va incepe de la secunde
                         }
                    break;
    case CLOCK_ADJ_MODE: if (MODE_STATE == MODE_CLICK){//Schimbam elementul de ajustat
							                ElIndex++;
                              if (TblElIndex[ElIndex] == BlinkingColonPos){//Ne intoarcem in ceas normal
                                CheckandCorrectDate(pTimeDate); //Daca s-a reglat 31 pentru o luna care are mai putine zile
                                //Adjust day of the week 
								CurrentSeconds = makeTime(TimeDate);
								breakTime(CurrentSeconds, TimeDate);
	
								CLOCK_STATE = CLOCK_RUNNING;    
					                    }
                          }
                          else if (SET_STATE == SET_CLICK){
                 						switch( TblElIndex[ElIndex] ){
                						case	0:  SetSec(pTimeDate);
                								break;
                						case	1:	SetMin(pTimeDate);
                								break;
                						case	2:	SetHR(pTimeDate);
                								break;
                						case	3:	SetWday(pTimeDate);
                								break;
                						case	4:	SetDay(pTimeDate);
                								break;
                						case	5:	SetMonth(pTimeDate);
                								break;
                						case	6:	SetYear(pTimeDate);
                								break;							
                							default: break;
                						}
                					}
                     break;
    default: CLOCK_STATE = CLOCK_RUNNING;
                    break;
      }
}




