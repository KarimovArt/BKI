#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <DS1307.h>


#include "modbus.h"

#define BEEP 4

//����
#define OUT4 0
#define OUT3 1
#define OUT2 2
#define OUT1 3

// �������� ����������� ���
#define LCD_RS	2 	//define MCU pin connected to LCD RS
#define LCD_RW	3 	//define MCU pin connected to LCD R/W
#define LCD_E	4	//define MCU pin connected to LCD E
#define LCD_DB4	3	//define MCU pin connected to LCD D3
#define LCD_DB5	2	//define MCU pin connected to LCD D4
#define LCD_DB6	1	//define MCU pin connected to LCD D5
#define LCD_DB7	0	//define MCU pin connected to LCD D6
#define LDP PORTB	//define MCU port connected to LCD data pins
#define LCP PORTC	//define MCU port connected to LCD control pins
#define LDDR DDRB	//define MCU direction register for port connected to LCD data pins
#define LCDR DDRC	//define MCU direction register for port connected to LCD control pins
#include <lcd_new.h>

#define CAN_DEFAULT
//#define CAN_RX_BUFFER_SIZE 0	//������ ��������� ������ �� ���������
//#define CAN_TOUT 1000			//����� ��������, ����
#include <canspi.h>

//��������� �������
EEMEM unsigned long int speed=19200;
EEMEM unsigned char parity=EVEN;
EEMEM unsigned char mode=RTU;



void interrupt_init(void)
{
	//���� ��� ���������� �� CAN
	DDRD &= ~(1<<DDD2);	//PORTD.2 ����
	PORTD |= 1<<DDD2;	//pull up

	MCUCR &= 0xFC;	//���������� �� ������� ������
	GICR |= 0x40;	//INT0 ���
}

void timers_init(void)
{
	// Mode: CTC top=OCR1A
	// OC1A output: Discon.
	// OC1B output: Discon.
	// Noise Canceler: Off
	// Input Capture on Falling Edge
	// Timer 1 Overflow Interrupt: Off
	// Input Capture Interrupt: Off
	// Compare A Match Interrupt: On
	// Compare B Match Interrupt: Off
	TCCR1A=0x00;
	TCCR1B=0x0A; 	// Clock value: 2000 kHz
	TCNT1H=0x00;
	TCNT1L=0x00;
	ICR1H=0x00;
	ICR1L=0x00;
	OCR1AH=0x07;	//���������� 1 ��
	OCR1AL=0xD0;
	OCR1BH=0x00;	//
	OCR1BL=0x00;

	// Timer(s)/Counter(s) Interrupt(s) initialization
	TIMSK |= 1<<OCIE1A;// | 1<<OCIE1B;
}

signed char CAN_init(unsigned char mode)
  {
  signed char result;

  SPI_init();


  //��������� ����������
  //DDRB &= ~(1<<2);	//PORTB.2 ����
  //PORTB |= 1<<2;		//pull up
  //GICR=0x00;	//���������� �� ������� ������

  CSPINDDR |= 1<<CSPIN; 	//��� CS �����
  CSPINPORT |= 1<<CSPIN;	// Hi-level

  CAN_reset();

  //��������� �������� (125 kib/s)
    CAN_bitModify(CNF2,0x80,0x80); //���������� ���������� ���������������� PHSEG2 (BTLMODE==1)
    CAN_write(CNF1,0x05);// SJW==1Tq, BRP==6;
    CAN_write(CNF2,0x89);// BTLMODE==1,SAM==0,PRSEG2==2Tq,PHSEG1==2Tq ;
    CAN_write(CNF3,0x02);// PHSEG2==3Tq ;

  //��������� �������� (10 kib/s)
//  CAN_bitModify(CNF2,0x80,0x80);	//���������� ���������� ���������������� PHSEG2 (BTLMODE==1)
//  CAN_write(CNF1,31);				// SJW==1Tq, BRP==31; 	=> Tq==8us.��� 10kib/s ����� ���� ~12Tq
//  	  	  	  	  	  	  	  	  	//Typically, the sampling of the bit should take place at about 60-70% of the bit time
//  	  	  	  	  	  	  	  	    // => 8Tq �� ������� � 4Tq �����
//  	  	  	  	  	  	  	  	    //SyncSeg = 1Tq PropSeg = 4Tq PHSEG1=3Tq PHSEG2=4Tq
//  CAN_write(CNF2,0b10010011);		// BTLMODE==1,SAM==0
//  CAN_write(CNF3,0x03);				//


   #ifdef BDZ0
  //������ � ����� ��� ������ 0
  CAN_setMask(CAN_MASK_0,0x000007FF);  //��������� ������ SID
  CAN_setFilter(CAN_FILTER_0,0x000007FF,CAN_SID_FRAME);  // SID ������ ���� 7FF
  //������� � ����� ��� ������ 1
  CAN_setMask(CAN_MASK_1,0xFFFFFFFF);
  CAN_setFilter(CAN_FILTER_5,0x00000000,CAN_SID_FRAME);

  //��������� ������� 0 ��� SID
  CAN_bitModify(RXB0CTRL,0b01100111,0b00100000);
  //��������� ������� 5 ��� SID
  CAN_bitModify(RXB1CTRL,0b01100111,0b00100101);
  #endif

  //��������� ���������� �� ������ ����� �������
  CAN_bitModify(CANINTE,0x03,0x03);


  //��������� ������ ������
  result = CAN_setOpMode(mode);


//  Additionally, the RXB0CTRL register can be configured
//  such that, if RXB0 contains a valid message and
//  another valid message is received, an overflow error
//  will not occur and the new message will be moved into
//  RXB1, regardless of the acceptance criteria of RXB1.


  //set rollover-mode
  CAN_bitModify(RXB0CTRL,0x04,0x04);

  //set one-shot-mode
  //CAN_bitModify(CANCTRL,0x08,0x08);

  return result;
  }

void bki_init(void)
{
	asm("cli");
	//������������� ����������
	DDRC &= ~((1<<7) | (1<<6) | (1<<5));//�����
	PORTC |= (1<<7) | (1<<6) | (1<<5);	//Pull Up
	DDRD &= ~((1<<5) | (1<<4) | (1<<3));//�����
	PORTD |= (1<<5) | (1<<4) | (1<<3);	//Pull Up
	DDRA |= (1<<BEEP);					//�������
	PORTA &= ~(1<<BEEP);
	DDRA |= 1<<5;						//��� ����������
	PORTA |= 1<<5;
	DDRA |= (1<<OUT1) | (1<<OUT2) | (1<<OUT3) | (1<<OUT4);	//����
	PORTA |= (1<<OUT1) | (1<<OUT2) | (1<<OUT3) | (1<<OUT4);	//��������

	//������������� LCD
	LCD_init();
	LCD_visible();
	LCD_clr();

	//����
	TWI_init();
	rtc_get(&rtc);
	if(rtc_check()<0){ERROR=RTC;/*return;*/}

	MODB_init(eeprom_read_byte(&mode),eeprom_read_byte(&parity),eeprom_read_dword(&speed));

UCSRB |= 1<<RXCIE;	//���������� UARTRX

	interrupt_init();
	timers_init();
	asm("sei");

	_delay_ms(500);
	if(CAN_init(CAN_MODE_NORMAL)<1){ERROR=CAN;return;}


//	_
//_delay_ms(500);


}




