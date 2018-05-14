#include <avr/io.h> //biblioteka zawieraj¹ca funkcje steruj¹ce portami
#include <avr/interrupt.h> //biblioteka zawieraj¹ca funkcje obs³ugi przerwan
#include <util/delay.h> //biblioteka zawieraj¹ca funkcjê opóŸniaj¹c¹

#define RS 2 //pin PA2 pod³¹czony do wyswietlacza do wyjsca RS, nadajemy nazwe RS  aby kod programu by³ bardziej przejrzysty dla nas
#define EN 3 //pin PA3
#define D4 4 //pin PA4
#define D5 5 //pin PA5
#define D6 6 //pin PA6
#define D7 7 //pin PA7
#define LCD_PORT PORTA //PORTA
#define LCD_DDR DDRA //DDRA

static volatile int czas = 0; //zmienna do obslugi przerwania
static volatile int i = 0; //zmienna do obslugi przerwania

/////funkcje dotycz¹ce polaczenia z PC przez RS232
void usart_init(){
	
	//UBRRH/UBRRL-Rejestry do ustawiania prêdkoœci przesy³u danych.
    UBRRH=0x00;
    UBRRL=	12;//12 dla	1Mhz; 155 dla 12Mhz; 103 dla 8Mhz
    UCSRA=(0<<RXC) | (0<<TXC) | (0<<UDRE) | (0<<FE) | (0<<DOR) | (1<<U2X) | (0<<MPCM); //rejestr kontrolny A
    UCSRB=(0<<RXCIE) | (0<<TXCIE) | (0<<UDRIE) | (1<<RXEN) | (1<<TXEN) | (0<<UCSZ2) | (0<<RXB8) | (0<<TXB8); //rejestr kontrolny B - w³¹czenie interfejsu szeregowego, RXEN-odbiornik, TXEN-nadajnik
    UCSRC=(1<<URSEL) | (0<<UMSEL) | (0<<UPM1) | (0<<UPM0) | (0<<USBS) | (1<<UCSZ1) | (1<<UCSZ0) | (0<<UCPOL); //rejestr kontrolny C - ustawia tryb pracy w asynchroniczny rejestrami URSEL i UMSEL, ustawia bit parzystoœci, ilosc bitow danych i ilosc bitow stopu
}
void usart_transmit (char data){ // sprawdzanie stanu poprzedniej transmisjo
    while (!( UCSRA & (1<<UDRE))); //sprawdzenie czy flaga UDRE ma wartosc 1, jesli ma to wtedy bufor jest gotowy przyjaæ nowe dane
    UDR = data; //UDR-rejestr danych, ladowanie danych do rejestru
}
void termnal_string(const char *str){ //funkcja wysy³aj¹ca/wypisujaca dane

    unsigned char s=0;
    while (str[s]!=0){
        usart_transmit (str[s]);
        s++;
    }
}
////koniec funkcji polaczenia z PC przez RS232

//// przerwanie obs³uguj¹ce ultradzwiekowy czujnik odleg³oœci HC-SR04
ISR(INT0_vect){
    if (i==1){
        TCCR1B=0; //ustawienie calego rejestru na 0
        czas=TCNT1;
        TCNT1=0; //przeladowanie wartosci
        i=0;
    }
    if (i==0){
        TCCR1B|=(1<<CS10); //bez prescalera
        i=1;
    }
}

//LCD_Send odpowiada za wysy³anie pojedynczego znaku do wyswietlacza
void LCD_Send(char znak) {
	LCD_PORT |= (1<<EN); //wlaczenie linii E która umozliwia zapis do rejestrow
	LCD_PORT = (znak & 0xF0)|(LCD_PORT & 0x0F); //wys³anie 4 starszych bitów
	LCD_PORT &= ~(1<<EN); //potwierdzenie wys³¹nia  danych poprzez opadniecie E
	asm volatile("nop");
	LCD_PORT |= (1<<EN);
	LCD_PORT = ((znak & 0x0F)<<4)|(LCD_PORT & 0x0F);//wys³anie 4 mlodszych bitów
	LCD_PORT &= ~(1<<EN);
}

//LCD_char przesy³a do funkcji LCD_Send znak uprzednio
//ustawiaj¹c tryb na koncówce RS odpowiadaj¹cy na przesyl danych
void LCD_Char(char a) {
	LCD_PORT |= 1<<RS;
	LCD_Send(a);
	_delay_ms(1);
}

//LCD_Cmd przesyla do funkcji LCD_Send komende uprzednio ustawiajac
//tryb na koncowce RS  odpowiadajacy za przesyl komend
void LCD_Cmd(char a) {
	LCD_PORT &= ~(1<<RS);
	LCD_Send(a);
	_delay_ms(1);
}

//LCD_Clear odpowiada za wyczyszczenie i
//ustawienie kursora na pierwszym miejscu
void LCD_Clear() {
	LCD_PORT &= ~(1<<RS); //przestawia na linii RS wartosc na 0 aby wysy³aæ komendê a nie dane
	LCD_Send(1);
	LCD_PORT |= 1<<RS;
	_delay_ms(1.64); //maksymalny czas trwania instrukcji 1.62ms
}

//LCD_Init odpowiada za  skonfugurowanie ukladu LCD tak aby byl wlaczony
//wyczyszczony z niewidocznym i niemigajacym kursorem
//w ukladzie dwuliniowym,
//przesylem czterobitowym z przemieszczajacym sie kursorem
void LCD_Init() {
	LCD_Cmd(0b00000010); //ustawienie kursora na pozycji pierwszego znaku
	LCD_Cmd(0b00001100); // kursor miganie, podwietlenie lub w tym przypadku brak
	LCD_Cmd(0b00000110); //okreœlenie trybu pracy kursora/okna wyœwietlacza
	LCD_Cmd(0b00101000); //okreslenie trybu czterobitowego, liczby wierszy(2) i wymiaru znaku (5x8)
	LCD_Clear();
}
// LCD_String odpowiada za wyœwietlenie podanej mu tablicy
// przesy³aj¹c po jednym znaku z pomoc¹ funkcji LCD_Char,
// dodatkowo pilnuje on przejœcia do nowej linii oraz koryguje
// moment ten dodatkow¹ zmienn¹ „space” tak by przez puste znaki
// na pocz¹tku wiersza znaki z koñca nie wyskakiwa³y poza zakres 0
// wyœwietlacza.
void LCD_String(unsigned char *text, char space) {
	int len = strlen(text);
	unsigned char t = 0;
	for(int i; i<len; i++) {
		if(i>15-space && t == 0) {
			LCD_Cmd(0b11000000);//
			t++;
		}
		LCD_Char(text[i]);
	}
}

void LCD_Switch() { //przemieszcza kursor o jeden znak
LCD_Cmd(0b00010100);
}
/////koniec funkcji obslugujacych LCD 2x16

int main(void){
	
	DDRA = 0xFF;
	DDRD = 0xFB;

    char temp [16]; //potrzebne do zamiany z int na char
	int16_t odl = 0; //odleglosc
    
    _delay_ms(50);

    GICR|=(1<<INT0); // w³¹czenie przerwania INT0
    MCUCR|=(1<<ISC00); //zmiana stanu logicznego na iNT0 powduje przerwanie

    usart_init(); //zainicjowanie po³aczenia z PC
    LCD_Init(); //zainicjowanie wyswietlacza LCD
    LCD_Clear();
  
    sei(); //wyzwolenie przerwañ
	termnal_string("Odleglosc: \r");
    while(1) {
        //*podawanie napiecia na TRIG przez 10us zgodnie z dokumentacj¹
        PORTD|=(1<<PIND3);
        _delay_us(10);
        PORTD &=~(1<<PIND3);
        //*

        //Wyswietlanie odleglosci na LCD
        LCD_Switch();
        LCD_Switch();
        LCD_Switch();
        LCD_String("Odleglosc: ",2);
        odl = (czas/58)*10; //obliczenie odleglosci
        itoa(odl,temp,10);
        LCD_String(" ",16);
        LCD_Switch();
        LCD_Switch();
        LCD_Switch();
        LCD_Switch();
        LCD_String(temp,2);
        LCD_String(" mm",2);
        _delay_ms(100);
        // koniec wyœwietlania

        /////wyœwietlenie w Virtual Terminalu oraz  PC po³¹czonego przez RS232
        termnal_string(temp);
        termnal_string(" mm\r");
		_delay_ms(500);
        /////
		
        LCD_Clear();
    }
}