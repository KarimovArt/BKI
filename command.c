
#define PING      0x01
#define RESET     0x02
#define RD_FAULT  0x03
#define RD_ARCH_1 0x04
#define RD_ARCH_2 0x05
#define PROG      0x06


void ping(void);
signed char send_cmd(unsigned char addr,unsigned char cmd);
signed char send_read_arch(unsigned char part,unsigned char addr,unsigned char index);
static inline signed char checkTOUT(unsigned char addr);
signed char aks_for_logic(unsigned char addr);


void ping(void)
{
	unsigned char data[7];

	for(unsigned char i=1;i<MAXQDEV;i++) inSysBDZ[i].flags=1<<FLT;	//перед посылкой команды обнуляем все флаги.Установленный флаг INL будет говорить о том,что ответ принят
	data[0]=PING;
	//синхронизация времени для БДЗ
	data[1]=rtc.day;
	data[2]=rtc.month;
	data[3]=rtc.year;
	data[4]=rtc.hours;
	data[5]=rtc.minutes;
	data[6]=rtc.seconds;

	CAN_loadTXbuf(0,7,data,CAN_TX_PRIORITY_3 & CAN_SID_FRAME);
}

signed char send_read_arch(unsigned char part,unsigned char addr,unsigned char index)
{
	unsigned char data[2]={(part==1)?(RD_ARCH_1):(RD_ARCH_2)};

	inSysBDZ[addr].flags=1<<FLT;	//перед посылкой команды обнуляем все флаги.Установленный флаг INL будет говорить о том,что ответ принят
	data[1]=index;

	CAN_loadTXbuf((unsigned long int)addr,2,data,CAN_TX_PRIORITY_3 & CAN_SID_FRAME);
	return 	checkTOUT(addr);
}

signed char send_prog(unsigned char addr,volatile unsigned char *param)
{
	unsigned char data[6]={PROG};

	if(param != NULL) for(unsigned char i=0;i<6;i++)  data[i]=*(param+i);
	//перед посылкой команды обнуляем флаги ALM,INL,устанавливаем FLT.Установленный флаг INL будет говорить о том,что ответ принят
	inSysBDZ[addr].flags=1<<FLT;
	CAN_loadTXbuf((unsigned long int)addr,(param != NULL)?(6):(1),data,CAN_TX_PRIORITY_3 & CAN_SID_FRAME);

	//если программируем широковещательно то ответа не будет
	return (addr==0)?(1):(checkTOUT(addr));
}

signed char aks_for_logic(unsigned char addr)
{
	unsigned char data[6]={PROG};

	//перед посылкой команды обнуляем флаги ALM,INL,устанавливаем FLT.Установленный флаг INL будет говорить о том,что ответ принят
	inSysBDZ[addr].flags=1<<FLT;
	CAN_loadTXbuf((unsigned long int)addr,2,data,CAN_TX_PRIORITY_3 & CAN_SID_FRAME);

	//если программируем широковещательно то ответа не будет
	return (addr==0)?(1):(checkTOUT(addr));
}

signed char send_cmd(unsigned char addr,unsigned char cmd)
{
	//перед посылкой команды обнуляем флаги ALM,INL,устанавливаем FLT.Установленный флаг INL будет говорить о том,что ответ принят
	inSysBDZ[addr].flags=1<<FLT;
	CAN_loadTXbuf((unsigned long int)addr,1,&cmd,CAN_TX_PRIORITY_3 & CAN_SID_FRAME);

	if(cmd==RESET) return 1;	//не требует ответа
	return 	checkTOUT(addr);
}

static inline signed char checkTOUT(unsigned char addr)
{
	CAN_timeout=CAN_TOUT;
	while(chkBit(inSysBDZ[addr].flags,INL)==0 && (CAN_timeout !=0));	//ждем ответа
	return (CAN_timeout ==0 || CAN_ERR !=0)?(-1):(1);					//дождались таймаута или ошибки CAN вернули -1
}


//чтение принятого сообщения из буфера MCP2515 (CAN)
ISR(INT0_vect)
{

#define BDZ_ID (tmpBuf.ID & (unsigned long int)0x7F)
	struct
	{
		unsigned long int ID;
		unsigned char dataLength;
		unsigned char data[8];
		unsigned char msg_flags;
	}tmpBuf;

	CAN_readRXbuf(&tmpBuf.ID, tmpBuf.data, &tmpBuf.dataLength, &tmpBuf.msg_flags);
	if(chkBit(sys_state,SCAN)) inSysBDZ[BDZ_ID].data[7]=BDZ_ID;								//если производится сканирование сохраняем откликнувшиеся ID в inSysBDZ[BDZ_ID].data[7]

	if(readID(BDZ_ID) != BDZ_ID) {ERROR=ID;return;}											//проверка валидности (если ID не зарегистрирован)

	setBit(inSysBDZ[BDZ_ID].flags,INL);														//ставим флаг инлайн
	if(chkBit(tmpBuf.ID,10)==1) setBit(inSysBDZ[BDZ_ID].flags,CONF);
	if(chkBit(tmpBuf.ID,9) ==0) setBit(inSysBDZ[BDZ_ID].flags,ALM);							//если есть авариz ставим флаг в БДЗ
	if(chkBit(tmpBuf.ID,8) ==1) clrBit(inSysBDZ[BDZ_ID].flags,FLT);							//если нету неисправности обнуляем флаг в БДЗ
	//inSysBDZ[BDZ_ID].data[0] = tmpBuf.ID>>8;
	for(unsigned char i=0;i<tmpBuf.dataLength;i++)inSysBDZ[BDZ_ID].data[i] = tmpBuf.data[i];//копируем данные
}
