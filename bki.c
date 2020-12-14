#include <avr/eeprom.h>
#include <stdio.h>

//43534534534534
#define setBit(byte,bit) (byte |= 1<<bit)
#define clrBit(byte,bit) (byte &= ~(1<<bit))
#define chkBit(byte,bit) ((byte & 1<<bit)>>bit) //возвращает 1 если бит установлен

#define MAXQDEV 99		//максимальное кол-во БДЗ в системе
#define BUFSIZE 30		//размер буфера приема/передачи
#define EXITMENU 30000 	//время автовыхода из меню (ms, max=65535)
#define PINGTIME 5000 	//интервал пинга (ms, max=65535)

//клавиатура
#define NOKEY 0xFF
#define ENT 2
#define ESC 3
#define LEFT  4
#define UP    5
#define DOWN  6
#define RIGHT 7
//#define BEEP 4
//
////реле
//#define OUT4 0
//#define OUT3 1
//#define OUT2 2
//#define OUT1 3

//флаги
#define ALM 0	//для БДЗ
#define FLT 1	//для БДЗ
#define INL 2	//для БДЗ
#define ERR 3	//для  БКИ
#define SCAN 4	//для  БКИ (флаг сканирования)
#define CONF 5 //флаг подтвержденной аварии
#define NEW 6
#define readIDnew(n) eeprom_read_byte(&new[n])
#define readID(i) eeprom_read_byte(&inSysBDZaddr[i])
#define writeID(i) eeprom_write_byte(&inSysBDZaddr[i],i)

EEMEM unsigned char new[MAXQDEV]={13};
EEMEM unsigned char inSysBDZaddr[MAXQDEV]={0};	//сохраненные адреса подключенных БДЗ
EEMEM unsigned int password=300;				//пароль системы
volatile struct									//флаги состояния инлайновых БДЗ
{
	unsigned char data[8];
	unsigned char flags;
}inSysBDZ[MAXQDEV];

static unsigned char sys_state;			//состояние системы в целом (WRK==0, 1<<ALM, 1<<FLT, 1<<ERR)
static unsigned char ind_state;			//для надписи на экране

volatile unsigned int menuExit;			//флаг (авто)выхода из меню
volatile unsigned int pingTime;			//флаг пинга

enum{RTC=2,CAN=3,ID=4,MODB=5}ERROR=0;	//коды ошибок
struct RTC rtc;							//часы реального времени

#define BKI_STATE 0
#define BKI_OUTS 1
#define BDZ_INSYS 2
#define BDZ_FAULT 3
#define BDZ_ALARM 4
#define TIME_YYMM 5
#define TIME_DDHH 6
#define TIME_MMSS 7
#define MAX_QUANT_MODB_REG	8	//максимальное кол-во регистров модбаса

//
#define MODB_ADDR modb_message[0]
#define MODB_FUNC modb_message[1]
#define MODB_START_REG ((unsigned int )modb_message[2]<<8 | modb_message[3])
#define MODB_WR_DATAH modb_message[4]
#define MODB_WR_DATAL modb_message[5]
#define MODB_QUANT_REG ((unsigned int )modb_message[4]<<8 | modb_message[5])
#define MOBD_BYTE_COUNT modb_message[2]
#define MOBD_START_DATA 3
#define MOBD_ERROR_CODE modb_message[2]
#define ILLEGAL_FUNCTION 01
#define ILLEGAL_DATA_ADDR 02
#define ILLEGAL_DATA_VAL 03
//размер буфера модбаса- самое длинное сообщение это ответ на чтение всех регистров в АСКИ режиме т.е. ':'addr'func'byte_count'numreg*4'LRC'0D'0A' ==38 c запасом 40
unsigned int modbusRegisters[MAX_QUANT_MODB_REG];	//регистры модбаса
EEMEM unsigned char bki_addr=0x01;


unsigned char scanID(unsigned char i);
unsigned char scanKBD(void);
static inline void beep(unsigned int);
static unsigned char modbErr(unsigned char errCode);
static signed char modbDecoder(void);

#include "bki_init.c"
#include "command.c"
#include "menu.c"



int main(void)
{
	bki_init();


//writeID(9);
//writeID(7);
//writeID(6);
//writeID(4);
//writeID(1);
//writeID(3);
//writeID(5);
//writeID(2);

	while(1)
	{
		char buffer[20];
		//PGM_P title=RABOTA;
		PGM_P title;

		if(!(chkBit(sys_state,ALM))&&!(chkBit(sys_state,FLT))) title=RABOTA;
		//заголовок
		(ind_state==0)?(ind_state++):(ind_state=0);
		if(chkBit(sys_state,ALM) && ind_state==0)  title=AVARIYA;
		if(chkBit(sys_state,FLT) && ind_state==1)  title=NEISPRAVNOST;
		if(chkBit(sys_state,ERR))                  title=NET_GOTOVNOSTI;
		LCD_gotoXY(0,0);LCD_puts_P(title,20);

		//читаем время
		rtc_get(&rtc);
		sprintf(buffer,"%02u.%02u.%02u %02u:%02u:%02u",rtc.day,rtc.month,rtc.year,rtc.hours,rtc.minutes,rtc.seconds);
		//обновляем модбас
		modbusRegisters[TIME_YYMM]=(unsigned int)rtc.year<<8 | rtc.month;
		modbusRegisters[TIME_DDHH]=(unsigned int)rtc.day<<8 | rtc.hours;
		modbusRegisters[TIME_MMSS]=(unsigned int)rtc.minutes<<8 | rtc.seconds;

		//экран:код ошибки или строка RTC
		LCD_gotoXY(0,3);
		if(ERROR)
		{
			setBit(sys_state,ERR);
			LCD_puts_P(OSHIBKA,20);
			LCD_gotoXY(7,3);
			LCD_putchar(ERROR+'0');
		}
		else
		{
			LCD_puts(buffer,20);
		}


		sprintf(buffer, "%u", sys_state);
		LCD_gotoXY(0,1);
		LCD_puts(buffer,20);

		/*sprintf(buffer, "%u", inSysBDZ[13].data[0]);
		LCD_gotoXY(0,2);
		LCD_puts(buffer,20);*/

		//новое в версии
		if(TWI_ERR)ERROR=RTC;
		if(MODB_ERR)ERROR=MODB;
		if(CAN_ERR)ERROR=CAN;



		//сканируем блоки на предмет аварий и неисправностей
		sys_state=0;																		//обнуляем все ошибки и тд
		modbusRegisters[BDZ_INSYS]=modbusRegisters[BDZ_FAULT]=modbusRegisters[BDZ_ALARM]=0;//обнуляем регистры.
		for(unsigned char i=1;i<MAXQDEV;i++)	//если БДЗ зареган
		{
			if(readID(i)==i)	//если блок есть в списке
			{
				modbusRegisters[BDZ_INSYS]++;																//счетчик инлайн блоков
				if(chkBit(inSysBDZ[i].flags,CONF)) setBit(sys_state,CONF);
				if(chkBit(inSysBDZ[i].flags,ALM))
				{
					modbusRegisters[BDZ_ALARM]++;
					setBit(sys_state,ALM);
					if (scanID(i)) setBit(sys_state,NEW);
				}	//счетчик аварийных блоков и установка флага общей аварии
				if(chkBit(inSysBDZ[i].flags,FLT)) {modbusRegisters[BDZ_FAULT]++; setBit(sys_state,FLT);}	//счетчик неисправный блоков и установка флага общей неисправности
				//if(new==i) setBit(sys_state,NEW);
				//sys_state |= inSysBDZ[i].flags & 0x03;				//два мл.бита-это флаги аварии и неисправности
			}

			(chkBit(sys_state,CONF))?(PORTA|=1<<OUT2):(PORTA &= ~(1<<OUT2));
			(chkBit(sys_state,ALM))?(PORTA|=1<<OUT1):(PORTA &= ~(1<<OUT1));	//обнаружили аварию ОТКЛЮЧАЕМ
			(chkBit(sys_state,NEW))?(PORTA|=1<<OUT3):(PORTA &= ~(1<<OUT3));
			(chkBit(sys_state,FLT) || ERROR)?(clrBit(PORTA,OUT4)):(setBit(PORTA,OUT4));
		}
		//сохраняем ошибку, статус системы и состояния выходов
		modbusRegisters[BKI_STATE]=(unsigned int)ERROR<<8 | sys_state;
		modbusRegisters[BKI_OUTS]= PINA & 0x0F;

		//здесь можно пингануть.После пинга пройдет "пауза со сканированием"-за это время должны прийти все отклики
		if(pingTime ==0)
		{
			ping();
			pingTime=PINGTIME;
			LCD_gotoXY(19,0);
			LCD_putchar('*');
		}



		//пауза со сканированием клавы.Если появился RX прерываемся
		for(unsigned int i=0;(i<1000) && (!rx_counter);i++)
		{
			_delay_ms(1);
			//нажат ENT-висим пока не отпустят-по выходу из меню очищаем экран,обнуляем статус
			if(scanKBD()==ENT){while(scanKBD()!=NOKEY);naviMenu(LVL_main);LCD_clr();sys_state=0;}
		}

		//отработка модбаса.если что то есть в буфере читаем сообщение
		if(rx_counter)
		{
			signed char a=getModbMsg();
			if(a<1)ERROR=MODB;
			else //удачный прием.сбрасываем ошибки,декодируем,отвечаем
			{
				PORTA &= ~(1<<5);	//гасим СД
				if(ERROR==MODB)ERROR=0;
				if((a=modbDecoder()) >0)putModbMsg(a);
				PORTA |= 1<<5;	//зажигаем СД
			}
		}


	}
}




static signed char modbDecoder(void)
{
	//адрес не наш
	if(MODB_ADDR != eeprom_read_byte(&bki_addr)) return -1;
	//неправильный регистр или их кол-во
	if((MODB_START_REG+MODB_QUANT_REG) > MAX_QUANT_MODB_REG) return modbErr(ILLEGAL_DATA_ADDR);
	//обрабатываем функцию
	switch(MODB_FUNC)
	{
	case 0x03:	//чтение регистров
	{
		unsigned char j=MOBD_START_DATA;
		unsigned int  s=MODB_START_REG;
		unsigned int  e=MODB_QUANT_REG;

		for(unsigned int i=0; i<e; i++)
		{
			modb_message[j++] =modbusRegisters[i+s] >>8;		//старший байт данных
			modb_message[j++] =modbusRegisters[i+s] & 0x00FF;	//младший
		}
		MOBD_BYTE_COUNT = j-MOBD_START_DATA;				//счетчик байт в сообщении
		return MOBD_BYTE_COUNT+3;
	}
	break;
//до лучших времен...
//	case 0x06:	//запись одного регистра
//	{
//		if(MODB_START_REG < 0x05) return modbErr(ILLEGAL_DATA_ADDR);
//		//год,месяц-проверка на максимально/минимальное знаение
//		if( (MODB_START_REG == 0x05) && ((MODB_WR_DATAH > 99) || (MODB_WR_DATAL > 12) || (MODB_WR_DATAL < 1)) ) return modbErr(ILLEGAL_DATA_VAL);
//		//день,час-проверка на максимально/минимальное знаение
//		if( (MODB_START_REG == 0x06) && ((MODB_WR_DATAH > 31) || (MODB_WR_DATAH < 1) || (MODB_WR_DATAL > 23)) ) return modbErr(ILLEGAL_DATA_VAL);
//		//мин,сек-проверка на максимально/минимальное знаение
//		if( (MODB_START_REG == 0x07) && ((MODB_WR_DATAH > 59) || (MODB_WR_DATAL > 59)) ) return modbErr(ILLEGAL_DATA_VAL);
//
//	}
//	break;
	default:modbErr(ILLEGAL_FUNCTION);
	break;
	}
	return 0;
}

static unsigned char modbErr(unsigned char errCode)
{
	MODB_FUNC |= 0x80;			//установили флаг ошибки
	MOBD_ERROR_CODE = errCode; 	//код ошибки
//	LCD_putchar('g');
//	while(1);
	return 3;					//длина сообщения об ошибке фиксирована
}

unsigned char scanID(unsigned char i)
{
	for(unsigned char n=0;n<MAXQDEV;n++)
		if(readIDnew(n)==i) return 0;
	return 1;
}

unsigned char scanKBD(void)
{
	unsigned char kbd = (((PIND&0x38)>>1) | (PINC&0xE0)) & 0xFC;
	static unsigned char prevkbd;

	if((kbd)==0xFC){prevkbd=0xFC; return NOKEY;}	//клавиша не нажата,вернули FF
//	beep(10);					//пикалка + антидребезг

	if(prevkbd != kbd)
	{
		beep(10);					//пикалка + антидребезг
		prevkbd = kbd;
	}

	menuExit=EXITMENU;						//взводим таймер автовыхода
	for(char i=2;i<8;i++)
	{
		if(chkBit(kbd,i) ==0)return i;	//найдена нажатая кнопка.вернули код кнопки
	}
	return NOKEY;						//кнопка не нашлась.типа помеха.
}

static inline void beep(unsigned int lenght)
{
	setBit(PORTA,BEEP);					//пикалка + антидребезг
	for(unsigned int i=0;i<lenght;i++)_delay_ms(1);
	clrBit(PORTA,BEEP);
}

//таймер 1ms
ISR(TIMER1_COMPA_vect)
{
	if(CAN_timeout)CAN_timeout--;
	if(SPI_timeout)SPI_timeout--;
	if(menuExit)menuExit--;
	if(pingTime)pingTime--;

	if(MODB_timeout)MODB_timeout--;

}

//чтение данный UART
ISR(USART_RXC_vect)
{
	read_raw_data();
}



