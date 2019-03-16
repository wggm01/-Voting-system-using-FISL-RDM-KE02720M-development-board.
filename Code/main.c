#include "MKE02Z2.h"


/*--Variables, Constantes e inicializaciones*/

//----LCD-------
#define DataBusPort 	GPIOB_PDOR
#define ControlBusPort	GPIOB_PDOR
#define DataBusDirReg	GPIOB_PDDR
#define ControlBusDirReg	GPIOB_PDDR

void DataWrite(char dat);
void sendNibble(char nibble);

#define LCD_RS	8 	//PTF0
#define LCD_RW	10  //PTF2
#define LCD_EN	9 //PTF1

#define LCD_D4 0
#define LCD_D5 1
#define LCD_D6 2
#define LCD_D7 3

#define LCD_ctrlBusMask   ((1<<LCD_RS)|(1<<LCD_RW)|(1<<LCD_EN))
#define LCD_dataBusMask   ((1<<LCD_D4)|(1<<LCD_D5)|(1<<LCD_D6)|(1<<LCD_D7))
//----------------------------------------------------------------------

//----Salidas-------
#define clk_freq 8000000
#define IO_DIR_REG GPIOA_PDDR
#define IO_VAL_REG GPIOA_PDOR
#define voting_LED 8             /*PTB0, alto activo*/
#define voted_LED  0             /*PTA0, alto activo*/
#define showing_results_LED 29   /*PTD5, alto activo*/
#define buzzer 24                /*PTD0, alto activo*/
//-----------

//Banderas
int flip[4] = {0,0,0,0}; //y1-shwr-rst-shwvot(once)
int z[4] = {0,0,0,0}; //Contadores para los candidatos z[0] -z1 , z[1] - z2, z[2] - z3 - z[3] - z4
static int i = 0;

/*--------------------------------------------------*/

/*Funcion encargada de los retrasos*/

void delay(int tiempo){ /*Tiempo en milisegundos*/
	long period = tiempo*(clk_freq/1000);
	while (period>0){
		period = period - 1;
	}}

/*-----------------------------------*/

/*Funciones para el funcionamiento de la LCD*/


void sendNibble(char nibble)
{
    DataBusPort &= ~(LCD_dataBusMask);
    DataBusPort |= (((nibble >>0x00) & 0x01) << LCD_D4);
    DataBusPort |= (((nibble >>0x01) & 0x01) << LCD_D5);
    DataBusPort |= (((nibble >>0x02) & 0x01) << LCD_D6);
    DataBusPort |= (((nibble >>0x03) & 0x01) << LCD_D7);
}

void Lcd_DataWrite(char dat)
{
    sendNibble((dat >> 0x04) & 0x0F);  //Send higher nibble
    ControlBusPort |= (1<<LCD_RS);  // Send HIGH pulse on RS pin for selecting data register
    ControlBusPort &= ~(1<<LCD_RW); // Send LOW pulse on RW pin for Write operation
    ControlBusPort |= (1<<LCD_EN);  // Generate a High-to-low pulse on EN pin
    delay(1);
    ControlBusPort &= ~(1<<LCD_EN);

    delay(10);

    sendNibble(dat & 0x0F);            //Send Lower nibble
    ControlBusPort |= (1<<LCD_RS);  // Send HIGH pulse on RS pin for selecting data register
    ControlBusPort &= ~(1<<LCD_RW); // Send LOW pulse on RW pin for Write operation
    ControlBusPort |= (1<<LCD_EN);  // Generate a High-to-low pulse on EN pin
    delay(1);
    ControlBusPort &= ~(1<<LCD_EN);

    delay(10);
}

void Lcd_CmdWrite(char cmd)
{
    sendNibble((cmd >> 0x04) & 0x0F);  //Send higher nibble
    ControlBusPort &= ~(1<<LCD_RS); // Send LOW pulse on RS pin for selecting Command register
    ControlBusPort &= ~(1<<LCD_RW); // Send LOW pulse on RW pin for Write operation
    ControlBusPort |= (1<<LCD_EN);  // Generate a High-to-low pulse on EN pin
    delay(1);
    ControlBusPort &= ~(1<<LCD_EN);

    delay(10);

    sendNibble(cmd & 0x0F);            //Send Lower nibble
    ControlBusPort &= ~(1<<LCD_RS); // Send LOW pulse on RS pin for selecting Command register
    ControlBusPort &= ~(1<<LCD_RW); // Send LOW pulse on RW pin for Write operation
    ControlBusPort |= (1<<LCD_EN);  // Generate a High-to-low pulse on EN pin
    delay(1);
    ControlBusPort &= ~(1<<LCD_EN);

    delay(10);
}

void Inicio_LCD(void){

    Lcd_CmdWrite(0x02);                   // Incializar LCD en modo de 4 bits
    Lcd_CmdWrite(0x28);                   // Modo 5x7
    Lcd_CmdWrite(0x0C);                   // Encender display y colocar cursor
    Lcd_CmdWrite(0x01);                   // Limpiar pantalla
    Lcd_CmdWrite(0x80);                   // Colocar cursor en la primera linea
}

/*-------------------------------------*/


/*--Inicializaciones*/

void initClk(){
	ICS_C1 |= 0x04; //Configuracion del reloj de Referencia
	ICS_C2 |= 0x20; /*Divisor 16 //tarjeta 20M- 0x80*/
	OSC_CR = 0x00; //APAGAR OSCILADOR
}

void gpioLcdinit (){
	DataBusDirReg |= LCD_dataBusMask;  // Configura pines de LCD
	ControlBusDirReg |= LCD_ctrlBusMask;
	GPIOA_PDDR |= 1<<13;    /* Inicializacion de Pin como salida de propï¿½sito general*/
    GPIOA_PDOR |= 1<<13;	/*Salida de Datos del Puerto*/
    GPIOA_PCOR |= 1<<13;
    Lcd_CmdWrite(0x01);

}

void init_pins(void){
	IO_DIR_REG |= (1<<voting_LED)|(1<<voted_LED)|(1<<showing_results_LED)|(1<<buzzer);      /*Configurar como salidas*/
	IO_VAL_REG &= ~(((1<<voting_LED)|(1<<voted_LED)|(1<<showing_results_LED)|(1<<buzzer))); /*Inicializar en apagado*/
}

int inputs_outputs(void)
{
	GPIOA_PDDR&=~((1<<23)|(1<<22)|(1<<21)|(1<<19)|(1<<18)|(1<<17)|(1<<16)); // Puertos PTC 0,1,2,3,5,6,7 como ENTRADAS [Botones]
	GPIOA_PIDR&=~((1<<23)|(1<<22)|(1<<21)|(1<<19)|(1<<18)|(1<<17)|(1<<16)); // Habilitar puertos entradas [Botones]
	PORT_PUEL|=(1<<23)|(1<<22)|(1<<21)|(1<<19)|(1<<18)|(1<<17)|(1<<16); // Activar resistencias de pullup PTC 0,1,2,3,5,6,7

}

/*-----------------------*/

/* Funciones para mostrar puntaje de cada candidato: z1-b z2-c z3-d z4-t*/

void candz1(int b){
 	int millares=b/1000;
 	int centenas=(b-(millares*1000))/100;
 	int decenas=(b- (millares*1000 + centenas*100))/10;
 	int unidades=b-(millares*1000 + centenas*100 + decenas*10 );
 	Inicio_LCD();
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_DataWrite(centenas +48);
 	Lcd_DataWrite(decenas +48);
 	Lcd_DataWrite(unidades +48);
}

void candz2(int c){
 	int millares=c/1000;
 	int centenas=(c-(millares*1000))/100;
 	int decenas=(c- (millares*1000 + centenas*100))/10;
 	int unidades=c-(millares*1000 + centenas*100 + decenas*10 );
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_DataWrite(centenas +48);
 	Lcd_DataWrite(decenas +48);
 	Lcd_DataWrite(unidades +48);
}

void candz3(int d){
 	int millares=d/1000;
 	int centenas=(d-(millares*1000))/100;
 	int decenas=(d- (millares*1000 + centenas*100))/10;
 	int unidades=d-(millares*1000 + centenas*100 + decenas*10 );
 	Lcd_CmdWrite(0xC0); //Movimiento de cursor hacia segunda fila
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_DataWrite(centenas +48);
 	Lcd_DataWrite(decenas +48);
 	Lcd_DataWrite(unidades +48);
}

void candz4(int t){
 	int millares=t/1000;
 	int centenas=(t-(millares*1000))/100;
 	int decenas=(t- (millares*1000 + centenas*100))/10;
 	int unidades=t-(millares*1000 + centenas*100 + decenas*10 );
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
	Lcd_CmdWrite(0x14);//Movimiento de cursor un espacio a derecha
 	Lcd_DataWrite(centenas +48);
 	Lcd_DataWrite(decenas +48);
 	Lcd_DataWrite(unidades +48);
}

/* -------------------------------*/


/*Control de voto*/

void outputs_alloff(void){ // Desactiva salida audiovisuales
	IO_VAL_REG &= ~(((1<<voting_LED)|(1<<voted_LED)|(1<<showing_results_LED)|(1<<buzzer)));
}

void killvote(){
// cambio estado de flip[0] para evitar la activacion del monitoreo de los botones
	flip[0]= !flip[0];
//borrar pantalla lcd
	Lcd_CmdWrite(0x01);
//quito alimentacion de lampara lcd
	GPIOA_PCOR |= 1<<13;/*Apagado de Pantalla LCD*/
//Salidas apagadas
	outputs_state(4);

}

void rstvote(){
// cambio estado de flip para evitar la activacion de los botones, reseteo o mostrar resultados
	flip[0]= 0;
	flip[1]= 0;
	flip[2]= 0;
	flip[3]= 0;
//reinicio de contadores
	z[0]=0;z[1]=0;z[2]=0;z[3]=0;
//enviar string en blanco a lcd
	Lcd_CmdWrite(0x01);
//quito alimentacion de backlight lcd
	GPIOA_PCOR |= 1<<13;/*Apagado de Pantalla LCD*/
//Salidas apagadas
	outputs_alloff();
}

void mssgVote (){
	char i,a[]={"Votacion en / Proceso ..."};
	Inicio_LCD();
	for(i=0;a[i]!=0;i++){
		if (a[i]=='/'){
			Lcd_CmdWrite(0xC0);
		    Lcd_CmdWrite(0x10);}
		else
		    {Lcd_DataWrite(a[i]);}}
}

void shwResults (int z1, int z2, int z3, int z4){ //Recibe los valores actuales de los contadores
	GPIOA_PSOR |= 1<<13;/*Encendido de Pantalla LCD M3*/
	Inicio_LCD();
	candz1(z[0]);
	candz2(z[1]);
	candz3(z[2]);
	candz4(z[3]);
}


void outputs_state(char op){ //Con esta funcion se decide cuales salidas se encenderan
	switch(op){
	case 1: /*Votación en proceso(Verde)*/
		outputs_alloff();
		IO_VAL_REG |= (1<<voting_LED);
	break;
	case 2: /*Votación Realizada(Rojo)*/
		outputs_alloff();
		IO_VAL_REG |= (1<<voted_LED)|(1<<buzzer);
		delay(250);
		IO_VAL_REG &= ~(1<<buzzer);
	break;
	case 3: /*Muestra de resultados(Azul)*/
		outputs_alloff();
		IO_VAL_REG |= (1<<showing_results_LED)|(1<<buzzer);
		delay(100);
		IO_VAL_REG &= ~(1<<buzzer);
		delay(100);
		IO_VAL_REG |= (1<<buzzer);
		delay(100);
		IO_VAL_REG &= ~(1<<buzzer);
	break;
	case 4: /*Espera(Amarillo)*/
			outputs_alloff();
			delay(150);
			IO_VAL_REG |= (1<<voted_LED)|(1<<voting_LED);
		break;
	default:
		outputs_alloff();
	break;
	}
}

/*-----------------------------------*/

/*Control de Botones*/
int pressed_button(void) // Esta funcion esta anidada a buttons() y define las condiciones para obtener una respuesta cuando se deja de presionar el boton
{
	if ((GPIOA_PDIR & 0xEF0000)== 0xEE0000) // PTC0 - Z1
	{return 1;}
	else if ((GPIOA_PDIR & 0xEF0000)== 0xED0000) // PTC1 - Z2
	{return 2;}
	else if ((GPIOA_PDIR & 0xEF0000)== 0xEB0000) // PTC2 - Z3
	{return 3;}
	else if ((GPIOA_PDIR & 0xEF0000)== 0xE70000) // PTC3 - Z4
	{return 4;}
	else if ((GPIOA_PDIR & 0xEF0000)== 0xCF0000) // PTC5 - Y
	{return 5;}
	else if ((GPIOA_PDIR & 0xEF0000)== 0xAF0000) // PTC6 - SHWR
	{return 6;}
	else if ((GPIOA_PDIR & 0xEF0000)== 0x6F0000) // PTC7 - RST
	{return 7;}
	else {return 0;}
}

int btn(void) // funcion a la cual tendrias que llamar en el Control
{
    int button = 0;
    int button0 = 0;
    button = pressed_button();
    return button;
}
/*----------------------------------*/




int main(void)
{
	//Llamar funciones de inicializacion de sistema.
	int bot = 0;
	initClk(); //RELOJ A 1 MHz
	gpioLcdinit(); //Pines para LCD
	init_pins(); // Pines para salida
	inputs_outputs();//Pines de entrada
	rstvote(); //KEDATE KIETO
	outputs_state(4);

    for (;;) {
    	bot = btn();
    	//Si el boton y1 es presionado pasa lo siguiente:
    	if (bot==5){
    		flip[0]= !flip[0];flip[3]= !flip[3];} //Mantiene estado cada vez que se presiona el boton

    	if(flip[0] == 1){

    	if(flip[3]==1){
    		//Mensaje en lcd = "Voto en progreso" y LED de votacion encendido
    		outputs_state(1);
    		Lcd_CmdWrite(0x01);
			GPIOA_PSOR |= 1<<13;/*Encendido de Pantalla LCD M3*/
    		mssgVote(); // Mensaje de votacion en progreso...
    		flip[3]= 0;
    		}

    		//Monitorizacion de botones------------------------------------------
    	switch(bot){
    		case 1: //Candidato 1
    			z[0] += 1; //Contador de votos
    			outputs_state(2); //Activo buzzer y Led de verificacion de voto
    			killvote();
    			break;

    		case 2: //candidato 2
    			z[1] += 1;//Contador de votos
    			outputs_state(2); //Activo buzzer y Led de verificacion de voto
    			killvote();
    			break;

    		case 3: //candidato 3
    			z[2] += 1;//Contador de votos
    			outputs_state(2); //Activo buzzer y Led de verificacion de voto
    			killvote();
    			break;

    		case 4: //candidato 4
    			z[3] += 1;//Contador de votos
    			outputs_state(2); //Activo buzzer y Led de verificacion de voto
    			killvote();
    			break;}}

    	//-------------------------------------------------------------------------

    	//Toma de decision si reinciar o mostrar datos (show=1 reiniciar = 0)

    		if (bot== 6){ //Mostrar resultados(shwr) (no se deberia presionar durante votacion)
    			flip[1]= !flip[1];}

    		if (flip[1] == 1){
    			outputs_state(3); //LED indica que se esta mostrando los resultados
    			shwResults (z[0],z[1],z[2],z[3]);flip[1]= !flip[1];} //Manda valores de cada contador para mostrar en lcd

    	    //---------------------------------------------------------------------------------
    		i++;
    }

    return 0;
}
