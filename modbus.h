/*
 * modbus.h
 *
 *  Created on: 1 марта 2018 г.
 *      Author: kostya
 */


//

#define TXENPIN 6
#define TXENPORT PORTD
#define TXENDDR DDRD
#define MODB_MESSSIZE 40
//#define MODB_RX_COMPLETE (UCSRA &= 1<<RXC)	//по желанию (если не использовать прерывание по приему)

#ifndef MODBUS_H_
#define MODBUS_H_

//константы проверки на четность
#define NONE 0x00
#define EVEN 0x20
#define ODD  0x30

//управление ногой драйвера 485 RX/TX
#define TXENABLE TXENPORT |= 1<<TXENPIN
#define TXDISABLE TXENPORT &= ~(1<<TXENPIN)

//режимы модбаса
#define RTU 0
#define ASCII 1

//переменные
extern unsigned char modb_message[MODB_MESSSIZE];			//модбас-буфер.здесь хранится сообщение (принятое или для передачи)
extern volatile unsigned long int MODB_timeout;				//cчетчик таймаута (us)
extern volatile unsigned char rx_buffer[MODB_MESSSIZE];		//буфер UART
extern volatile unsigned char wr_index,rx_counter;
extern unsigned char rd_index;

//ошибки: таймаут,переполнение приемного буфера,аппаратная ошибка,не тот формат смс,ошибка контрольки
typedef volatile enum {MODBNOERR,MODBTOUT,MODBBUFOVF,MODBHW,MODBMSG,MODBXRC}FAULT;
extern FAULT MODB_ERR;

//функции
//Инициализация.Выглядит примерно так:  MODB_init(RTU,EVEN,9600);
extern void MODB_init(unsigned char type,unsigned char parity,unsigned long int speed);
//получить сообщение из приемного буфера УАРТа. В итоге-в модбас-буфере RTU сообщение,вернули длину полученного сообщения или -1 (ошибка)
extern signed char getModbMsg(void);
//отправить сообщение из модбас буфера. На входе RTU сообщение, передаем его длину
extern void putModbMsg(unsigned char lenght);
//прием символа и помещение в приемный буфер. Вставляется,например в прерывание
inline void read_raw_data(void)
{
	char status=UCSRA,data=UDR;

	if ((status & (1<<FE | 1<<PE | 1<<DOR))!=0) {MODB_ERR=MODBHW;return;}
	rx_buffer[wr_index]=data;
	if (++wr_index == MODB_MESSSIZE) wr_index=0;
	if (++rx_counter == MODB_MESSSIZE) {rx_counter=0;MODB_ERR=MODBBUFOVF;return;}
}


#endif /* MODBUS_H_ */
