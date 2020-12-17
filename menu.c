#include <stdio.h>
#include "russtring.h"

//тест история
 struct menu
{
//	Каждый пункт меню (любого уровня) имеет имя, функцию
	PGM_P name;
	void (*pFunc)(void);
};
#define NUMOFMENUSTRING 3	//количество строк меню(не считая заголовка)

 static void novoe(void);
 static void in_line(void);
 static void alarm(void);
 static void fault(void);
 static void archieve(void);
 static void nastroyka(void);
 static void datavremya(void);
 static void scansys(void);
 static void progsys(void);
 static void vremya_mtz(void);
 static void vremya_urov(void);
 static void adres(void);
 static void svyaz(void);
 static void rezhim(void);
 static void skorost(void);
 static void chetnost(void);
 static void change_param(unsigned char nParam);
// static void dlya_vseh(void);
// static void po_odnomu(void);

 static inline unsigned char printSubMenu(const char *flashName,unsigned char flag);
 static inline void printTimeStr(volatile unsigned char *data, unsigned char x,unsigned char y);
 static inline void printIOStr(volatile unsigned char *data);
 static inline void printMTZStr(unsigned int MTZon,unsigned int MTZoff);
 static inline void printTOUT(void);
 static inline void printUNAVALIABLE(void);
 static inline unsigned char whileKey(void);
 static unsigned char passwd(void);
 static unsigned char numm(void);

 static void reset(unsigned char);
 static unsigned char viewArch(unsigned char currBDZaddr,unsigned char index);

// struct menu LVL_dlya_vseh[]=
// {
//	{DLYA_VSEH,NULL},
//	{VREMYA_MTZ,NULL},
//	{VREMYA_UROV,NULL},
//	{NULL,NULL}	//затычка
// };
//
// struct menu LVL_po_odnomu[]=
//  {
// 	{PO_ODNOMU,NULL},
// 	{ADRES,NULL},
// 	{VREMYA_MTZ,NULL},
// 	{VREMYA_UROV,NULL},
// 	{NULL,NULL}	//затычка
//  };

struct menu LVL_prog[]=
{
	{PROGRAMMIROVANIE,NULL},
	{ADRES,adres},
	{VREMYA_MTZ,vremya_mtz},
	{VREMYA_UROV,vremya_urov},
	{NULL,NULL}		//затычка
};

 struct menu LVL_nastroyka[]=
 {
	 {NASTROYKA,NULL},
	 {DATAVREMYA,datavremya},
	 {SCANIROVANIE,scansys},
	 {PROGRAMMIROVANIE,progsys},
	 {SVYAZ,svyaz},
	 {NOVOE, novoe},
	 {NULL,NULL}	//затычка
 };
 struct menu LVL_svyaz[]=
  {
 	 {SVYAZ,NULL},
 	 {REZHIM,rezhim},
 	 {SKOROST,skorost},
 	 {CHETNOST,chetnost},
 	 {NULL,NULL}	//затычка
  };


/*********************************** УРОВЕНЬ 0 ***********************************/
struct menu LVL_main[]=	////кол-во пунктов уровня 0 (основных) +затычка
{
	{MENU,NULL},			//название пункта
	{NA_SVYAZI,in_line},
	{AVARIYA,alarm},
	{NEISPRAVNOST,fault},
	{ARHIV,archieve},
	{NASTROYKA,nastroyka},
	{NULL,NULL}	//затычка
};
static void novoe(void)
{


}

static inline void printMenuHeader(PGM_P name)
{
	//прорисовываем заголовок "NAME:"
	LCD_clr();
	LCD_puts_P(name,strlen_P(name));
	LCD_putchar(0x3A);
}

static inline void printMenuPunkt(struct menu *level)
{
	char lowerCase[20];

	for(unsigned char i=0;i<3;i++)
	{
		if((level+i)->name == NULL)return;	//если вместо имени заглушка(пункта не существует)
		for(unsigned char j=0;j<19;j++)		//преобразуем названия пунктов меню в нижний регистр
		{
			lowerCase[j]=(pgm_read_byte(&(level+i)->name[j]) >= 0xC0)?( pgm_read_byte(&(level+i)->name[j])+0x20 ):(pgm_read_byte(&(level+i)->name[j]));
		}
		LCD_gotoXY(1,i+1);
		LCD_puts(lowerCase,19);	//прорисовали пункты меню:
	}
}

void naviMenu(struct menu *level)
{
	unsigned char cursorPos=1;			//указатель на текущий пункт
	unsigned char screenPos=0;			//указатель на текущий пункт
	unsigned char numPunkt,currPunkt=1;	//счетчик пунктов меню,

	for(numPunkt=0;(level+1+numPunkt)->name != NULL;numPunkt++);	//считаем кол-во пунктов в подменю (level -заголовок; level+1  -первый пункт)

	while(1)
	{
		printMenuHeader(level->name);		//рисуем заголовок
		printMenuPunkt(level+screenPos+1);	//пункты меню
		LCD_gotoXY(0,cursorPos);			//и курсор (символ >)
		LCD_putchar(0x84);

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case DOWN:
		{
			if(currPunkt < numPunkt)
			{
				++currPunkt;
				(cursorPos < NUMOFMENUSTRING)?(cursorPos++):(screenPos++);
			}
		}
		break;
		case UP:
		{
			if(currPunkt > 1 )
			{
				--currPunkt;
				(cursorPos > 1)?(cursorPos--):(screenPos--);
			}
		}
		break;
		case ENT:(level+currPunkt)->pFunc();	//-запускаем функцию выбранного пункта	(она также может вызвать naviMenu -следующий подуровень)
		break;
		case ESC:return;	//выход по ESC
		break;
		case NOKEY:asm("jmp 0");	//автовыход
		break;
		}
	}
}



static inline unsigned char printSubMenu(const char *flashName,unsigned char flag)
{
	unsigned char addr[MAXQDEV]={0};
	unsigned char cursorPos=1;	//указатель на текущий пункт
	unsigned char screenPos=0;	//указатель на позицию экрана
	unsigned char numPunkt=0,currPunkt=0;	//счетчик пунктов меню,
	char tmpStr[4];

	//считываем адреса зареганых девайсов с интересующими нас установленными флагами (inLine,fault,alarm...)
	for(unsigned char i=1;i<MAXQDEV;i++) if(readID(i)==i && chkBit(inSysBDZ[i].flags,flag)) addr[numPunkt++]=readID(i);

	while(1)
	{	//рисуем: "NAME:XX БДЗ"
		printMenuHeader(flashName);
		itoa(numPunkt,tmpStr,10);
		LCD_puts(tmpStr,strlen(tmpStr)+1);
		LCD_puts_P(BDZ,3);

		for(unsigned char i=0;(i<NUMOFMENUSTRING) && (addr[i+screenPos]);i++)		//прорисовываем не более NUMOFMENUSTRING строк меню
		{
			LCD_gotoXY(1,i+1);LCD_puts_P(BDZ,4);
			LCD_puts(itoa(addr[i+screenPos],tmpStr,10),3);
		}
		LCD_gotoXY(0,cursorPos);LCD_putchar(0x84);			//...и курсор (символ >)

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case DOWN:
		{
			if(currPunkt < numPunkt-1)
			{
				++currPunkt;
				(cursorPos < NUMOFMENUSTRING)?(cursorPos++):(screenPos++);
			}
		}
		break;
		case UP:
		{
			if(currPunkt > 0 )
			{
				--currPunkt;
				(cursorPos > 1)?(cursorPos--):(screenPos--);
			}
		}
		break;
		case ENT:return addr[currPunkt];	//возвращаем адрес блока с кот.надо че-то сделать
		break;
		case ESC:return 0;	//выход по ESC
		break;
		case NOKEY:asm("jmp 0");//автовыход
		break;
		default:continue;	//любая другая клавиша
		break;
		}
	}
	return NOKEY;
}

static void in_line(void)
{
	printSubMenu(NA_SVYAZI,INL);
}

static void alarm(void)
{
	unsigned char currBDZaddr = 0;
	unsigned char index=0;
	unsigned int minMTZ_ontime=0xFFFF,MTZ_offtime=0xFFFF;
	unsigned char Ndev=0,tmpAddr=0;
	char tmpStr[5];

	printMenuHeader(AVARIYA);//рисуем название пункта':'

	for(unsigned char i=1;i<MAXQDEV;i++)
	{
		if(readID(i)==i && chkBit(inSysBDZ[i].flags,ALM))	//обнаружили девайс с флагом авария
		{
			if(tmpAddr==0)tmpAddr=i;						//на случай если не найдется БДЗ,зафиксировавщих время сраб.МТЗ
			Ndev++;											//добавляем кол-во обнаруженных девайсов
			if(send_read_arch(2,i,0) <0) printTOUT();		//запрашиваем чась архива со значениями МТЗ.Если получен таймаут...
			else if(inSysBDZ[i].data[0]==RD_ARCH_2)			//получили тот ответ который надо
			{
				unsigned int tmpTime=(unsigned int)inSysBDZ[i].data[3]<<8 | inSysBDZ[i].data[4];

				if(tmpTime < minMTZ_ontime)
				{
					minMTZ_ontime=tmpTime;				//если полученное время меньше сохраненного минимального-перезаписываем
					MTZ_offtime = (unsigned int)inSysBDZ[i].data[5]<<8 | inSysBDZ[i].data[6];	//время отпускания
					tmpAddr=i;								//сохраняем адрес с мин значением МТЗ
				}
			}
		}
	}

	LCD_puts(itoa(Ndev,tmpStr,10),strlen(tmpStr)+2);LCD_puts_P(BDZ,4);		//ХХX	БД3
	if(tmpAddr !=0)
	{
		if(send_read_arch(1,tmpAddr,0) <0)printTOUT();		//запрашиваем часть архива со значениями RTC. Если получен таймаут...
		else if(inSysBDZ[tmpAddr].data[0]==RD_ARCH_1)
		{
			//строка дата/время
			printTimeStr(&inSysBDZ[tmpAddr].data[1],0,1);
			//строки вкл/откл МТЗ
			printMTZStr(minMTZ_ontime,MTZ_offtime);
		}
	}
LOOP:
	switch(whileKey())	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
	{
	case ESC:	//нажат ESC посылаем общий сброс
	{
		if(Ndev !=0) reset(0);
		return;
	}
	break;
	case ENT:if(Ndev==0)return;	//нажат ENT подолжаем далее(если есть аварии)
	break;
	case NOKEY:return;	//автовыход
	break;
	default:goto LOOP;	//любая другая клавиша
	break;
	}

	currBDZaddr = printSubMenu(AVARIYA,ALM);
	if(currBDZaddr<1)return;

	while(1)
	{
		switch(viewArch(currBDZaddr,index))
		{
		case UP:if(index>0)index--;
		break;
		case DOWN:if(index<31)index++;
		break;
		case ESC:return;	//выход по ESC
		break;
		case NOKEY:return;	//автовыход
		break;
		default:continue;	//любая другая клавиша
		break;
		}
	}
}

static void fault(void)
{
	char tmpStr[4];
	unsigned char currBDZaddr = printSubMenu(NEISPRAVNOST,FLT);	//получили адрес выбранного БДЗ
//LCD_gotoXY(0,0);
//LCD_putchar(currBDZaddr+'0');
//_delay_ms(2000);
	if(currBDZaddr<1)return;

	printMenuHeader(NEISPRAVNOST);

	LCD_puts_P(BDZ,4);
	LCD_puts(itoa(currBDZaddr,tmpStr,10),3);

	if(send_cmd(currBDZaddr,RD_FAULT) <0) printTOUT();	//послали команду,дождались таймаута -"нет связи"
	else if(inSysBDZ[currBDZaddr].data[0]==RD_FAULT)	//иначе если принятое сообщени это ответ на этот запрос
	{
//		data[1] -состояния входов	 <7>ЦВХ3; <6>ЦВХ2; <5>ЦВХ1; <4>ЦВХ0; <3>ОВХ3; <2>ОВХ2; <1>ОВХ1; <0>ОВХ0  (неисправный вход ==1)
//		data[2] -код ошибки {TEST=1,DATACRC,WATCHDOG,HARDMEM,SOFTHAND,SOFTTIM,UART,RTC,CAN,MSG}
		printIOStr(&inSysBDZ[currBDZaddr].data[1]);

		LCD_gotoXY(0,3);
		LCD_puts_P(OSHIBKA,7);
		LCD_puts(itoa(inSysBDZ[currBDZaddr].data[2],tmpStr,10),2);
	}
	else return;							//если получен не тот ответ выходим
	if( whileKey() ==ESC)reset(currBDZaddr);//а если ответ был правильный предлагаем ресетнуть

}

static void archieve(void)
{
	unsigned char currBDZaddr = printSubMenu(ARHIV,INL); /*выполнить просмотр архива инлайновых*/
	unsigned char index=0;

	if(currBDZaddr<1)return;

	while(1)
	{
		switch(viewArch(currBDZaddr,index))
		{
		case UP:if(index>0)index--;
		break;
		case DOWN:if(index<31)index++;
		break;
		default:return;
		}
	}
}

static void nastroyka(void)
{
	if(passwd() != 0)naviMenu(LVL_nastroyka);
}

static void datavremya(void)
{
	char buffer[20],max[]={31,12,99,23,59,59},min;
	unsigned char cursorPos=0;
	unsigned char *ptr[] = {&rtc.day,&rtc.month,&rtc.year,&rtc.hours,&rtc.minutes,&rtc.seconds};

	printMenuHeader(DATAVREMYA);

	if(rtc_get(&rtc) < 0) {ERROR=RTC;/*return -1;*/}


	while(1)
	{
		sprintf(buffer," %02u.%02u.%02u %02u:%02u:%02u",*ptr[0],*ptr[1],*ptr[2],*ptr[3],*ptr[4],*ptr[5]);
		LCD_gotoXY(0,3);
		LCD_puts(buffer,18);
		LCD_gotoXY(cursorPos*3,3);
		LCD_putchar(0x84);
		min=(cursorPos < 2)?(1):(0);

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case RIGHT: if(cursorPos < 5) cursorPos++;
		break;
		case LEFT: if(cursorPos >0) cursorPos--;
		break;
		case UP:(*ptr[cursorPos] < max[cursorPos])?((*ptr[cursorPos])++):(*ptr[cursorPos]=min);
		break;
		case DOWN:(*ptr[cursorPos] > min)?((*ptr[cursorPos])--):(*ptr[cursorPos]=max[cursorPos]);
		break;
		case ENT:{rtc_set(&rtc);return;}
		default: return;
		}
	}


}

static void scansys(void)
{
	unsigned char i,j=0;
	char tmpStr[4];

	sys_state |= 1<<SCAN;
	for(i=1;i<MAXQDEV;i++) {eeprom_write_byte(&inSysBDZaddr[i],0);inSysBDZ[i].data[7]=0;}
	while(!eeprom_is_ready());
	ping();
	_delay_ms(100);
	LCD_clr();
	LCD_puts_P(SCANIROVANIE,12);
	for(i=0;i<8;i++){LCD_putchar(0x2E);_delay_ms(500);}

	sys_state =0;
	for(i=1;i<MAXQDEV;i++) if(inSysBDZ[i].data[7] == i) {writeID(i);j++;}


	LCD_gotoXY(0,2);
	LCD_puts_P(OBNARUZHENO,16);
	LCD_puts(itoa(j,tmpStr,10),3);
	ping();
	_delay_ms(2000);
	menuExit=EXITMENU;
	ERROR=0;

	return ;
}

static void progsys(void)
{
	naviMenu(LVL_prog);
}

static void vremya_mtz(void)
{
	change_param(1);
}

static void vremya_urov(void)
{
	change_param(2);
}

static void adres(void)
{
	change_param(0);
}

static void svyaz(void)
{
	naviMenu(LVL_svyaz);
	MODB_init(eeprom_read_byte(&mode),eeprom_read_byte(&parity),eeprom_read_dword(&speed));
}

static void rezhim(void)
{
	printMenuHeader(REZHIM);
	unsigned char mmode=eeprom_read_byte(&mode);

	while(1)
	{
		LCD_gotoXY(10,0);
		(mmode==RTU)?(LCD_puts_P(RTU_M,5)):(LCD_puts_P(ASCII_M,5));

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case UP:if(mmode==RTU)mmode=ASCII;
		break;
		case DOWN:if(mmode==ASCII)mmode=RTU;
		break;
		case ENT:
		{
			eeprom_write_byte(&mode,mmode);
			return;
		}
		default: return;
		}
	}
}

static void skorost(void)
{

//	unsigned int speeds_table[SPEED_INDEX]={9600,19200,38400,76800};
	unsigned long int mspeed=eeprom_read_dword(&speed);
	char buf[7];

	printMenuHeader(SKOROST);

	while(1)
	{
		LCD_gotoXY(10,0);
		LCD_puts(ltoa(mspeed,buf,10),5);


		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case UP:
		{
			if(mspeed<76800) mspeed*=2;
		}
		break;
		case DOWN:
		{
			if(mspeed>9600) mspeed/=2;
		}

		break;
		case ENT:{eeprom_write_dword(&speed,mspeed);return;}
		default: return;
		}
	}
}

static void chetnost(void)
{
	printMenuHeader(CHETNOST);
	unsigned char mparity=eeprom_read_byte(&parity);
	PGM_P ptr=0;

	while(1)
	{
		LCD_gotoXY(10,0);
		switch(mparity)
		{
		case NONE:ptr=C_NONE;
		break;
		case EVEN:ptr=C_EVEN;
		break;
		case ODD:ptr=C_ODD;
		break;
		}
		LCD_puts_P(ptr,5);

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case UP:
		{
			if(mparity==EVEN) mparity=ODD;
			else if(mparity==ODD)mparity=NONE;
		}
		break;
		case DOWN:
		{
			if(mparity==NONE) mparity=ODD;
			else if(mparity==ODD)mparity=EVEN;
		}
		break;
		case ENT:
		{
			eeprom_write_byte(&parity,mparity);
			return;
		}
		default: return;
		}
	}
}

static void change_param(unsigned char nParam)
{
	signed char numBDZ=0;	//кол-во инлайновых БДЗ;
	unsigned char addr=0xFF,step;
	signed int param=0,max,staroe=0;

	scansys();
	//считаем кол-во БДЗ инлайн
	for(unsigned char i=1;i<MAXQDEV;i++) if(readID(i)==i && chkBit(inSysBDZ[i].flags,INL)) {numBDZ++;/*addr=readID(i);*/}

	addr=numm();//адрес БДЗ в котором будем менять, если addr=0 значит запрос широковещательный

	if(nParam==0 && addr==0) {printUNAVALIABLE();_delay_ms(2000);return;} //нельзя менять адрес во всех БДЗ разом
	if (addr!=0) //если запрос не широковещательный, то считываем показания БДЗ под № "addr"
	{
		if((send_prog(addr,NULL) <0)||(inSysBDZ[addr].data[0] !=PROG))	{printTOUT();_delay_ms(2000);return;}
	}

	switch(nParam)
	{
	//изменение адреса
	case 0:{staroe=inSysBDZ[addr].data[1];max=99;step=1;}
	break;
	//время МТЗ:если на связи 1 БДЗ пишем его время МТЗ, если больше то 0мс
	case 1:
	{

		staroe=(signed int)inSysBDZ[addr].data[2]<<8 | inSysBDZ[addr].data[3];
		//param=(numBDZ==1)?((signed int)inSysBDZ[addr].data[2]<<8 | inSysBDZ[addr].data[3]):(0);
		inSysBDZ[addr].data[4]=inSysBDZ[addr].data[5]=-1;	//в неизменяемый параметр пишем -1
		max=30000;
		step=10;
	}
	break;
	//время УРОВ:если на связи 1 БДЗ пишем его время УРОВ, если больше то 0мс
	case 2:
	{
		staroe=(signed int)inSysBDZ[addr].data[4]<<8 | inSysBDZ[addr].data[5];
		//param=(numBDZ==1)?((signed int)inSysBDZ[addr].data[4]<<8 | inSysBDZ[addr].data[5]):(0);
		inSysBDZ[addr].data[2]=inSysBDZ[addr].data[3]=-1;	//в неизменяемый параметр пишем -1
		max=30000;
		step=10;
	}
	break;
	default:return;
	}

	while(1)
	{
		char str[6];
		LCD_clr();
		if(addr!=0)
		{
			LCD_gotoXY(0,0);
			LCD_puts_P(STAROE_ZNACHENIE,16);
			LCD_putchar(0x3A);
			sprintf(str,"%d",staroe);
			LCD_gotoXY(0,1);
			LCD_puts(str,strlen(str));
			if(nParam!=0)LCD_puts_P(MS,2);
		}

		LCD_gotoXY(0,2);
		LCD_puts_P(NOVOE_ZNACHENIE,strlen_P(NOVOE_ZNACHENIE));
		LCD_putchar(0x3A);
		sprintf(str,"%d",param);
		LCD_gotoXY(0,3);
		LCD_puts(str,strlen(str));
		if(nParam!=0)LCD_puts_P(MS,2);
		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case DOWN:if(param>(nParam==0)?(1):(0))param-=step;
		break;
		case UP:if(param<max)param+=step;
		break;
		case ENT:goto SEND;	//выход по ENT (отсылка новых параметров)
		break;
		case ESC:return;	//выход по ESC
		break;
		case NOKEY:asm("jmp 0");
		break;
		}
	}
SEND:
	//отсылка новых параметров
	//if(eeprom_read_byte(&inSysBDZaddr[addr])!=0) {LCD_clr(); LCD_gotoXY(0,0);LCD_puts_P(OSHIBKA_ADDR,20);_delay_ms(2000);return;}

	inSysBDZ[addr].data[0]=PROG;
	switch(nParam)
	{
	case 0:	if(readID(param)!=0) {LCD_clr(); LCD_gotoXY(0,0);LCD_puts_P(OSHIBKA_ADDR,20);_delay_ms(2000);return;}
			inSysBDZ[addr].data[1]=param;
	break;
	case 1:{inSysBDZ[addr].data[2]=param >>8; inSysBDZ[addr].data[3]=param & 0x00FF;}
	break;
	case 2:{inSysBDZ[addr].data[4]=param >>8; inSysBDZ[addr].data[5]=param & 0x00FF;}
	break;
	default:return;
	}
	if((send_prog(addr,inSysBDZ[addr].data)<0) && (param!=0)){printTOUT();_delay_ms(2000);return;}
	scansys();

	return;
}

static void reset(unsigned char currBDZaddr)
{
	whileKey();	//ждем нажатия/отпускания кнопки или автовыхода

	LCD_clr();
	LCD_puts_P(SBROSIT,20);

	if(whileKey()==ENT)send_cmd(currBDZaddr,RESET);
}

static unsigned char viewArch(unsigned char currBDZaddr,unsigned char index)
{
	char tmpStr[7];
	unsigned char tmpData[14];
	unsigned char i,page=0;

	printMenuHeader(ARHIV);

	LCD_puts_P(BDZ,4);
	LCD_puts(itoa(currBDZaddr,tmpStr,10),3);
	LCD_gotoXY(18,0);
	LCD_puts(itoa(index+1,tmpStr,10),2);

	//первая часть архива
	if(send_read_arch(1,currBDZaddr,index) <0)printTOUT();												//дождались таймаута третья строка-"нет связи"
	else if(inSysBDZ[currBDZaddr].data[0]==RD_ARCH_1) for(i=0;i<7;i++) tmpData[i]=inSysBDZ[currBDZaddr].data[i];	//скопировали данные
	else return 0;

	//вторая часть архива
	if(send_read_arch(2,currBDZaddr,index) <0)printTOUT();												//дождались таймаута третья строка-"нет связи"
	else if(inSysBDZ[currBDZaddr].data[0]==RD_ARCH_2) for(i=7;i<14;i++) tmpData[i]=inSysBDZ[currBDZaddr].data[i-7];	//скопировали данные
	else return 0;

	while(1)
	{
		char buffer[20];

		if(tmpData[3]==0xFF) {LCD_gotoXY(0,2);LCD_puts_P(PUSTO,7);}	//если год 0xFF строка 0 "пусто"
		else
		{
			//строка 1 дата/время
			printTimeStr(&tmpData[1],0,1);

			switch(page)
			{
			case 0:
			{
				//строка ВОД1234 ДВХ1234
				printIOStr(&tmpData[8]);

				//строка 3 "РЕЛЕ:"
				LCD_gotoXY(0,3);
				strcpy_P(buffer,RELE);strcat(buffer,":");	//РЕЛЕ:
				if((tmpData[9]& 0x0F) ==0) strcat_P(buffer,NET);	//"нет"
				else for(i=0;i<4;i++) if( chkBit(tmpData[9],i) ) strcat(buffer,itoa(i+1,tmpStr,10));	//рисуем номера неисправных
				LCD_puts(buffer,20);
			}
			break;
			case 1:
			{
				//строки вкл/откл МТЗ
				printMTZStr((unsigned int)tmpData[10]<<8 | tmpData[11],(unsigned int)tmpData[12]<<8 | tmpData[13]);
			}
			break;
			}//switch

		}

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case UP:return UP;
		case DOWN:return DOWN;
		case RIGHT:if(page<1)page++;
		break;
		case LEFT:if(page>0)page--;
		break;
		case ESC:return ESC;	//выход по ESC
		break;
		case NOKEY:asm("jmp 0");	//автовыход
		break;
		default:continue;	//любая другая клавиша
		break;
		}
	}
	return 0;
}

static inline void printTimeStr(volatile unsigned char *data, unsigned char x,unsigned char y)
{
	char buffer[20];

	sprintf(buffer,"%02u.%02u.%02u  %02u:%02u:%02u",*data,*(data+1),*(data+2),*(data+3),*(data+4),*(data+5));
	LCD_gotoXY(x,y);
	LCD_puts(buffer,20);
}

static inline void printIOStr(volatile unsigned char *data)
{
	char buffer[20];
	char tmpStr[7];
	unsigned char i;

	//строка 2 "ВОД:  ДВХ:"
	LCD_gotoXY(0,2);
	strcpy_P(buffer,VOD);strcat(buffer,":");										//ВОД:
	if((*data & 0x0F) ==0) strcat_P(buffer,NET);									//"нет"
	else for(i=0;i<4;i++) if( chkBit(*data,i) ) strcat(buffer,itoa(i+1,tmpStr,10));	//или номера
	LCD_puts(buffer,20);
	LCD_gotoXY(9,2);
	strcpy_P(buffer,DVX);strcat(buffer,":");	//ДВХ:
	if((*data & 0xF0) ==0) strcat_P(buffer,NET);	//"нет"
	else for(i=4;i<8;i++) if( chkBit(*data,i) ) strcat(buffer,itoa(i-4+1,tmpStr,10));	//рисуем номера неисправных
	LCD_puts(buffer,20);
}

static inline void printMTZStr(unsigned int MTZon,unsigned int MTZoff)
{
	char buffer[20];
	char tmpStr[7];
	//строка 2 "вкл.МТЗ"
	LCD_gotoXY(0,2);
	strcpy_P(buffer,VKL_MTZ);
	if(MTZon==0xFFFF)strcat_P(buffer,NET);
	else {strcat(buffer,itoa(MTZon,tmpStr,10));strcat_P(buffer,MS);}
	LCD_puts(buffer,20);

	//строка 3 "откл.МТЗ"
	LCD_gotoXY(0,3);
	strcpy_P(buffer,OTKL_MTZ);
	if(MTZoff==0xFFFF)strcat_P(buffer,NET);
	else{strcat(buffer,itoa(MTZoff,tmpStr,10));strcat_P(buffer,MS);}
	LCD_puts(buffer,20);

}

static inline unsigned char whileKey(void)
{
	unsigned char tmpKey;

	while((tmpKey=scanKBD())==NOKEY) if(menuExit==0)return NOKEY;	//ждем нажатия кнопки или автовыхода
	while(scanKBD()!=NOKEY);										//...отпускания кнопки

	return tmpKey;
}

static inline void printTOUT(void)
{
	LCD_gotoXY(0,2);LCD_puts_P(NET_SVYAZI,20);
}

static inline void printUNAVALIABLE(void)
{
	LCD_gotoXY(0,2);LCD_puts_P(NEDOSTUPNO,20);
}

static unsigned char passwd(void)
{
	signed int currPasswd=0,step=1;
	char str[5];
	unsigned char cursorPos=3;

	while(1)
	{
		printMenuHeader(PAROL);	//прорисовываем заголовок
		LCD_gotoXY(0,2);
		sprintf(str,"%04d",currPasswd);
		LCD_puts(str,4);
		LCD_gotoXY(cursorPos,2);
		LCD_cursorBlink();

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case DOWN:
		{
			currPasswd=((currPasswd - step)>=0)?(currPasswd-step):(9999);
		}
		break;
		case UP:
		{
			currPasswd=((currPasswd + step)>=9999)?(0):(currPasswd+step);
		}
		break;
		case LEFT:
		{
			(step < 1000)?(step *= 10):(step=1);
			(cursorPos>0)?(cursorPos--):(cursorPos=3);
		}
		break;
		case RIGHT:
		{
			(step > 1)?(step /= 10):(step=1000);
			(cursorPos<3)?(cursorPos++):(cursorPos=0);
		}
		break;
		case ENT:
		{
			LCD_cursorOff();
			if(eeprom_read_word(&password) == currPasswd)return 1;	//
			else return 0;
		}
		break;
		case ESC:{LCD_cursorOff();return 0;}	//выход по ESC
		break;
		case NOKEY:asm("jmp 0");	//автовыход
		break;
		}
	}

	return 0;
}

static unsigned char numm(void)
{
	unsigned char i=0;
	printMenuHeader(NUMM);	//прорисовываем заголовок
	while (1)
	{
		char str[2];
		LCD_gotoXY(0,2);
		sprintf(str,"%02d",i);
		LCD_puts(str,4);

		switch( whileKey() )	//висим тут пока не нажмется-отпустится кнопка (или автовыход)
		{
		case DOWN:if(i==0) i=99; else i--;
		break;
		case UP:if(i==99) i=0; else i++;
		break;
		case ENT:return i;	//выход по ENT (отсылка новых параметров)
		break;
		case NOKEY:asm("jmp 0");
		break;
		}
	}
}
