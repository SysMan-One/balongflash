#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#include "getopt.h"
#include "buildno.h"
#endif

#include "hdlcio.h"
#include "ptable.h"
#include "flasher.h"
#include "util.h"

#define true 1
#define false 0


//***************************************************
//* Хранилище кода ошибки
//***************************************************
int	errcode;


//***************************************************
//* Вывод кода ошибки команды
//***************************************************
void	printerr	(void) 
{
	if (errcode == -1) 
		fprintf(stdout, " - таймаут команды\n");
	else	fprintf(stdout, " - код ошибки %02x\n", errcode);
}

//***************************************************
// Отправка команды начала раздела
// 
//  code - 32-битный код раздела
//  size - полный размер записываемого раздела
// 
//*  результат:
//  false - ошибка
//  true - команда принята модемом
//***************************************************
int	dload_start	(
		uint32_t code,
		uint32_t size
			) 
{
uint32_t iolen;  
uint8_t replybuf[4096];
  
#ifndef WIN32
static struct __attribute__ ((__packed__))  {
#else
#pragma pack(push, 1)
static struct {
#endif
	uint8_t	cmd;
	uint32_t code, size;
	uint8_t pool[3];
} cmd_dload_init =  {0x41,0,0,{0,0,0}};
#ifdef WIN32
#pragma pack(pop)
#endif

	cmd_dload_init.code = htonl(code);
	cmd_dload_init.size = htonl(size);

	iolen = send_cmd((uint8_t*)&cmd_dload_init,sizeof(cmd_dload_init),replybuf);
	errcode = replybuf[3];

	if ( (iolen == 0) || (replybuf[1] != 2) ) 
		{
		if ( !iolen ) 
			errcode = -1;

		return false;
		}

	return true;
}  

//***************************************************
// Отправка блока раздела
// 
//  blk - # блока
//  pimage - адрес начала образа раздела в памяти
// 
//*  результат:
//  false - ошибка
//  true - команда принята модемом
//***************************************************
int	dload_block	(
		uint32_t	part, 
		uint32_t	blk, 
		uint8_t		*pimage
			) 
{
uint32_t res, blksize, iolen;
uint8_t	replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
	  uint8_t	cmd;
	  uint32_t	blk, bsize;
	  uint8_t	data[fblock];
	} cmd_dload_block;  
#ifdef WIN32
#pragma pack(pop)
#endif

	blksize = fblock; // начальное значение размера блока
	if ( fblock > (res = ptable[part].hd.psize-blk*fblock) )  // размер оставшегося куска до конца файла
		blksize = res;  // корректируем размер последнего блока

	cmd_dload_block.cmd=0x42;		// код команды	
	cmd_dload_block.blk=htonl( blk + 1);	// номер блока
	cmd_dload_block.bsize = htons(blksize);	// размер блока
	// порция данных из образа раздела
	memcpy(cmd_dload_block.data, pimage + (blk * fblock), blksize);

	// отсылаем блок в модем
	iolen = send_cmd((uint8_t*)&cmd_dload_block, sizeof(cmd_dload_block) - fblock + blksize, replybuf); // отсылаем команду

	errcode = replybuf[3];

	if ((iolen == 0) || (replybuf[1] != 2))  
		{
		if (iolen == 0) 
			errcode=-1;
		return false;
		}

	return true;
}

  
//***************************************************
// Завершение записи раздела
// 
//  code - код раздела
//  size - размер раздела
// 
//*  результат:
//  false - ошибка
//  true - команда принята модемом
//***************************************************
int dload_end	(
		uint32_t	code,
		uint32_t	size
		)
{
uint32_t	iolen;
uint8_t		replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
	  uint8_t	cmd;
	  uint32_t	size;
	  uint8_t	garbage[3];
	  uint32_t	code;
	  uint8_t	garbage1[11];
	} cmd_dload_end;
#ifdef WIN32
#pragma pack(pop)
#endif

	cmd_dload_end.cmd=0x43;
	cmd_dload_end.code=htonl(code);
	cmd_dload_end.size=htonl(size);

	iolen=send_cmd((uint8_t*)&cmd_dload_end,sizeof(cmd_dload_end),replybuf);

	errcode = replybuf[3];

	if ( (iolen == 0) || (replybuf[1] != 2) ) 
		{
		if (iolen == 0) 
			errcode=-1;

		return false;
		}  

	return	true;
}  


//***************************************************
//* Запись в модем всех разделов из таблицы
//***************************************************
void	flash_all	(void) 
{
int32_t		part;
uint32_t	blk, maxblock;

	fprintf(stdout, "\n##  ---- Имя раздела ---- записано");

	/* Run over partitions table */
	for(part = 0; part < npart; part++) 
		{
		fprintf(stdout, "\n");  
		//  fprintf(stdout, "\n02i %s)",part,ptable[part].pname);
		// команда начала раздела

		if ( !dload_start(ptable[part].hd.code, ptable[part].hd.psize) ) 
			{
			fprintf(stdout, "\r! Отвергнут заголовок раздела %i (%s)", part, ptable[part].pname);
			printerr();
			exit(-2);
			}  
    
		maxblock = (ptable[part].hd.psize+(fblock-1))/fblock; // число блоков в разделе

		// Поблочный цикл передачи образа раздела
		for(blk = 0; blk < maxblock; blk++) 
			{
			// Вывод процента записанного
			fprintf(stdout, "\r%02i  %-20s  %i%%", part, ptable[part].pname, (blk+1) * 100 / maxblock); 

			// Отсылаем очередной блок
			if ( !dload_block(part,blk,ptable[part].pimage) ) 
				{
				fprintf(stdout, "\n! Отвергнут блок %i раздела %i (%s)", blk, part, ptable[part].pname);
				printerr();
				exit(-2);
				}  
			}    

		// закрываем раздел
		if ( !dload_end(ptable[part].hd.code,ptable[part].hd.psize) ) 
			{
			fprintf(stdout, "\n! Ошибка закрытия раздела %i (%s)",part,ptable[part].pname);
			printerr();
			exit(-2);
			}  
		} // конец цикла по разделам
}
