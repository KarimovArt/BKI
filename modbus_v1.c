/*
 * modbus_v1.c
 *
 *  Created on: 17 янв. 2018 г.
 *      Author: kostya
 *
 */


#include <avr/io.h>
#include <util/delay.h>
#include "modbus.h"


unsigned char modb_message[MODB_MESSSIZE];			//модбас-буфер.здесь хранится сообщение (принятое или для передачи)
volatile unsigned long int MODB_timeout=0;			//cчетчик таймаута (us)
volatile unsigned char rx_buffer[MODB_MESSSIZE];	//буфер UART
volatile unsigned char wr_index=0,rx_counter=0;
unsigned char rd_index;
static unsigned char modb_type;						//тип (RTU,ASCII)
static unsigned long int val_timeout;				//значение таймаута
FAULT MODB_ERR=MODBNOERR;

//преобразование символа в число
static inline char char_to_dig(char sym);
//преобразование числа в символ
static inline char dig_to_char(char num);
//вычисление LRC
static unsigned char LRC(volatile unsigned char *data, unsigned char lenght);
//вычисление CRC
static unsigned int CRC(volatile unsigned char *data, unsigned char lenght);
//отсылка символа по УАРТу
static void modb_putchar(char);


//Инициализация.Выглядит примерно так:  MODB_init(RTU,EVEN,9600);
void MODB_init(unsigned char type,unsigned char parity,unsigned long int speed)
{
  unsigned char stop=(parity ==NONE)?(2):(1);	//1 стоп бит если есть паритет; 2 бита если нет паритета
  unsigned char size=(type ==ASCII)?(7):(8);	//если ASCII-7 бит данных MSB, если RTU-8 бит данных MSB
  unsigned int baud=0x7F & (unsigned int)((F_CPU/(16*speed))-1);


  size-=5;
  TXENDDR |= 1<<TXENPIN;
  TXDISABLE;

  modb_type=type;
  //длительность передачи одного бита == кол-во бит-в-символе(ASCII==10, RTU==11)/скорость
  //таймаут д.б. RTU>=3.5 символа (>38,5 бит), ASCII <=1000ms;
  //RTU = с запасом 40 бит=> 40/speed ==пауза, сек *1000 ==ms
//  val_timeout=(type ==ASCII)?(1000000):(40000000/speed);
  val_timeout=(type ==ASCII)?(1000):(2);	//при скоростях больше 19200 таймаут можно принять 2мс

  //нога ТХ
  //When the USART Transmitter is enabled, this pin is configured as an output regardless of the value of DDD1.
  PORTD |= (1<<PORTD1);//PORTD.1=1 выход с "1"
  DDRD |= (1<<DDD1);  //DDRD.1=1 выход

  //нога RX
  PORTD |= (1<<PORTD0);//PORTD.1=1 вход с притяжкой
  DDRD &= ~(1<<DDD0);  //DDD.1=0 вход

  UCSRA=0x00;
  //(7)вкл.прер.по приему, (6)откл.прер.по передаче, (5)откл.прер.по опустошению UDR, (4)вкл.передатчик и (3)приемник, (2)9бит откл.
  UCSRB=0b10011000;
  //(7)?, (6)асинхронный режим, (5:4)четность(none), (3)1стоп, (2:1)8бит, (0)=0
  //	UCSRC=0b10000110;

  UCSRC=(1<<7) | parity | ((stop==2)?(1):(0) << 3) | (size<<1);
//	//(7)?, (6)асинхронный режим, (5:4)четность(EVEN), (3)1стоп, (2:1)8бит, (0)=0
//	UCSRC=0b10100110;
  UBRRH=baud << 8;
  UBRRL=baud & 0x00FF;
//	//9600 bps
//	UBRRH=0x00;
//	UBRRL=0x67;

}

//отправить сообщение из модбас буфера. На входе RTU сообщение, передаем его длину
void putModbMsg(unsigned char lenght)
{
  switch(modb_type)
  {
  case(RTU):
    {
    unsigned int crc=CRC(modb_message,lenght);
    unsigned char index=0;

    TXENABLE;
    while(lenght--) modb_putchar(modb_message[index++]);
    modb_putchar(crc & 0x00FF);	//младшим байтом вперед
    modb_putchar(crc>>8);
    TXDISABLE;
    MODB_timeout=val_timeout;
    while(MODB_timeout);
    }
  break;
  case(ASCII):
    {
    unsigned char lrc=LRC(modb_message,lenght);
    unsigned char index=0;

    TXENABLE;
    _delay_ms(20);
    modb_putchar(':');

    while(lenght--)
      {
    	modb_putchar(dig_to_char(modb_message[index]>> 4));
		modb_putchar(dig_to_char(modb_message[index++]& 0x0F));
      }
    modb_putchar( dig_to_char(lrc>> 4) );
    modb_putchar( dig_to_char(lrc& 0x0F) );
    modb_putchar(0x0D);
    modb_putchar(0x0A);
    TXDISABLE;
    }
   break;
  }
}

//получить сообщение из приемного буфера УАРТа. В итоге-в буфере RTU сообщение,вернули длину полученного сообщения или -1 (ошибка)
signed char getModbMsg(void)
{
	unsigned char flag_tout=0,index=0,pass=0;

	while(1)																		//пока не дочитаем до конца сообщения или до ошибки крутимся тут
	{
		MODB_timeout=val_timeout;
		while (rx_counter==0)	if(MODB_timeout==0){flag_tout=1;break;} 			//ждем символ или таймаут

//////////////////////////////////если работаем в режиме RTU
		if(modb_type==RTU)
		{
			if(flag_tout)															//таймаут в случае RTU -это конец сообщения.Проверяем CRC
			{
				unsigned int crc= modb_message[index-1] <<8 | modb_message[index-2];//полученная CRC
				if(crc !=CRC(modb_message,index-2))	{MODB_ERR=MODBXRC;return -1;}	//сравниваем с фактической
				return index-2;														//длина сообщения минус 2 символа CRC
			}
			modb_message[index++]=rx_buffer[rd_index];								//если не таймаут принимаем след. символ
		}

//////////////////////////////////если работаем в режиме ASCII
		else
		{
			if(flag_tout) {MODB_ERR=MODBTOUT;return -1;}							//таймаут в случае ASCII это ошибка
			switch(rx_buffer[rd_index])
			{
				case(':'):															//начало сообщения .может стоит обнулить index?...
				{
					;
				}
				break;
				case(0x0D):															//конец сообщения. Проверяем LRC и вываливаемся
				{
					if (++rd_index == MODB_MESSSIZE) rd_index=0;					//читаем 0x0A
					asm("cli");
					--rx_counter;
					asm("sei");
					if(LRC(modb_message,index) !=0){MODB_ERR=MODBXRC;return -1;}	//для проверки ЛРЦ посылаем сообщение без ':' но с ЛРЦ
					return index-1;													// а возвращем длину без учета ЛРЦ
				}
				break;
				default:															//сборка ASCII-to-RTU
				{
					if(pass==0)														//первый проход- в буфер сообщения пишем первую тетраду
					{
						modb_message[index] = char_to_dig(rx_buffer[rd_index]) << 4;
						pass=1;
					}
					else															//второй проход- в буфер сообщения пишем вторую тетраду
					{
						modb_message[index++] |= char_to_dig(rx_buffer[rd_index]) & 0x0F;
						pass=0;
					}
				}
				break;
			}
		}

		if (++rd_index == MODB_MESSSIZE) rd_index=0;
		asm("cli");
		--rx_counter;
		asm("sei");
	}
	return -1;	//что то ваще пошло не так
}

//вычисление LRC
static unsigned char LRC(volatile unsigned char *data, unsigned char lenght)
{
  unsigned char lrc = 0;

  while(lenght--) {lrc+=*data++;}
  return ((unsigned char) (-(signed char) lrc));
}

//вычисление CRC
static unsigned int CRC(volatile unsigned char *data, unsigned char lenght)
{
  unsigned int crc=0xFFFF;

  while(lenght--)
  {
    crc ^= *data++;
    for (unsigned char i = 0; i < 8; ++i)
    {
	if (crc & 1) crc = (crc >> 1) ^ 0xA001;
	else crc = (crc >> 1);
    }
  }

  return crc;
}

//преобразование символа в число
static inline char char_to_dig(char sym)
{
  sym-='0';
  if(sym>41) return sym-39;	/* a .. f */
  if(sym>9) return sym-7;   	/* A .. F */
  return sym;            	/* 0 .. 9 */
}

//преобразование числа в символ
static inline char dig_to_char(char num)
{
  num+='0'; //0=48
  if(num>57)return num+7;
  return num;
}

//отсылка символа по УАРТу
static void modb_putchar(char c)
{
	while((UCSRA&(1<<UDRE))==0);
	UDR=c;
	while((UCSRA&(1<<TXC))==0);	//ждем окончания передачи
	UCSRA |= 1<<TXC;			//сброс флага окончания передачи
}



