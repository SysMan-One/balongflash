/*  
**  FACILITY: Huawei Balong V7 modem and LTE router flashing utility.
**
**  DESCRIPTION: A set of routines to works with partitions in the file
**
**  AUTHOR: Uknown Hero  ( see https://github.com/forth32/balongflash/)
**
**  CREATION DATE:
**
**  MODIFICATION HISTORY:
**
**	13-APR-2019	RRL	Reformating shit code to be more readable.
**
*/

#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include	<string.h>
#include	<stdlib.h>
#else
#include	<windows.h>
#endif

#include	"zlib.h"

#include	"ptable.h"
#include	"hdlcio.h"
#include	"util.h"
#include	"signver.h"

int32_t lzma_decode(uint8_t* inbuf,uint32_t fsize,uint8_t* outbuf);

//******************************************************
//*  поиск символического имени раздела по его коду
//******************************************************

void  find_pname(unsigned int id,unsigned char* pname) 
{

unsigned int j;

struct {
	  char name[20];
	  int code;
	} pcodes[] = { 
	  {"M3Boot",		0x20000}, 
	  {"M3Boot-ptable",	0x10000}, 
	  {"M3Boot_R11",	0x200000}, 
	  {"Ptable",		0x10000},
	  {"Ptable_ext_A",	0x480000},
	  {"Ptable_ext_B",	0x490000},
	  {"Fastboot",		0x110000},
	  {"Logo",		0x130000},
	  {"Kernel",		0x30000},
	  {"Kernel_R11",	0x90000},
	  {"DTS_R11",		0x270000},
	  {"VxWorks",		0x40000},
	  {"VxWorks_R11",	0x220000},
	  {"M3Image",		0x50000},
	  {"M3Image_R11",	0x230000},
	  {"DSP",		0x60000},
	  {"DSP_R11",		0x240000},
	  {"Nvdload",		0x70000},
	  {"Nvdload_R11",	0x250000},
	  {"Nvimg",		0x80000},
	  {"System",		0x590000},
	  {"System",		0x100000},
	  {"APP",		0x570000}, 
	  {"APP",		0x5a0000}, 
	  {"APP_EXT_A",		0x450000}, 
	  {"APP_EXT_B",		0x460000},
	  {"Oeminfo",		0xa0000},
	  {"CDROMISO",		0xb0000},
	  {"Oeminfo",		0x550000},
	  {"Oeminfo",		0x510000},
	  {"Oeminfo",		0x1a0000},
	  {"WEBUI",		0x560000},
	  {"WEBUI",		0x5b0000},
	  {"Wimaxcfg",		0x170000},
	  {"Wimaxcrf",		0x180000},
	  {"Userdata",		0x190000},
	  {"Online",		0x1b0000},
	  {"Online",		0x5d0000},
	  {"Online",		0x5e0000},
	  {"Ptable_R1",		0x100},
	  {"Bootloader_R1",	0x101},
	  {"Bootrom_R1",	0x102},
	  {"VxWorks_R1",	0x550103},
	  {"Fastboot_R1",	0x104},
	  {"Kernel_R1",		0x105},
	  {"System_R1",		0x107},
	  {"Nvimage_R1",	0x66},
	  {"WEBUI_R1",		0x113},
	  {"APP_R1",		0x109},
	  {"HIFI_R11",		0x280000},
	  {"Modem_fw",		0x1e0000},
	  {"Teeos",		0x290000},

	  {0,0}
};

	for( j = 0; pcodes[j].code && (pcodes[j].code != id); j++);

	if ( pcodes[j].code ) 
		strcpy(pname, pcodes[j].name);	// имя найдено - копируем его в структуру
	else	sprintf(pname, "U%08x", id);	// имя не найдено - подставляем псевдоимя Uxxxxxxxx в тупоконечном формате
}

//*******************************************************************
// Вычисление размера блока контрольных сумм раздела
//*******************************************************************
uint32_t crcsize(int n) 
{ 
	return ptable[n].hd.hdsize - sizeof(struct pheader);   
}

//*******************************************************************
// получение размера образа раздела
//*******************************************************************
uint32_t psize(int n) 
{ 
	return ptable[n].hd.psize;   
}

//*******************************************************
//*  Вычисление блочной контрольной суммы заголовка
//*******************************************************
void calc_hd_crc16(int n) 
{ 
	ptable[n].hd.crc = 0;   // очищаем старую CRC-сумму
	ptable[n].hd.crc = crc16((uint8_t*)&ptable[n].hd,sizeof(struct pheader));   
}


//*******************************************************
//*  Вычисление блочной контрольной суммы раздела 
//*******************************************************
void calc_crc16(int n) 
{
uint32_t csize, // размер блока сумм в 16-битных словах
	off, len, i, blocksize = ptable[n].hd.blocksize; // размер блока, охватываемого суммой

uint16_t*	csblock;  // указатель на создаваемый блок


	// определяем размер и создаем блок
	csize = psize(n) / blocksize;
	if ( psize(n) % blocksize ) 
		csize++; // Это если размер образа не кратен blocksize

	csblock = malloc(csize*2);

	// цикл вычисления сумм
	for (i = 0; i < csize; i++) 
		{
		off = i* blocksize; // смещение до текущего блока 
		len = blocksize;

		if ( (ptable[n].hd.psize - off) < blocksize) 
			len = ptable[n].hd.psize-off; // для последнего неполного блока 

		csblock[i] = crc16(ptable[n].pimage + off, len);
		} 

	// вписываем параметры в заголовок
	if ( ptable[n].csumblock ) 
		free(ptable[n].csumblock); // уничтожаем старый блок, если он был

	ptable[n].csumblock = csblock;
	ptable[n].hd.hdsize = csize*2 + sizeof(struct pheader);

	// перевычисляем CRC заголовка
	calc_hd_crc16(n);
}


//*******************************************************************
//* Извлечение раздела из файла и добавление его в таблицу разделов
//*
//  in - входной файл прошивки
//  Позиция в файле соответствует началу заголовка раздела
//*******************************************************************
void extract(FILE* in)  
{
uint16_t	hcrc,crc, *crcblock;
int	crcblocksize, zlen, res;
char	*zbuf;

	ptable[npart].zflag = 0; 

	// читаем заголовок в структуру
	ptable[npart].offset=ftell(in);
	fread(&ptable[npart].hd, 1, sizeof(struct pheader), in); // заголовок

	//  Ищем символическое имя раздела по таблице 
	find_pname(ptable[npart].hd.code, ptable[npart].pname);

	// загружаем блок контрольных сумм
	ptable[npart].csumblock=0;  // пока блок не создан
	crcblock = (uint16_t*)malloc(crcsize(npart)); // выделяем временную память под загружаемый блок
	crcblocksize = crcsize(npart);
	fread(crcblock, 1, crcblocksize,in);

	// загружаем образ раздела
	ptable[npart].pimage = malloc(psize(npart));
	fread(ptable[npart].pimage, 1, psize(npart),in);

	// проверяем CRC заголовка
	hcrc = ptable[npart].hd.crc;
	ptable[npart].hd.crc = 0;  // старая CRC в рассчете не учитывается

	crc = crc16((uint8_t*)&ptable[npart].hd,sizeof(struct pheader));

	if (crc != hcrc) 
		{
		fprintf(stdout, "\n! Раздел %s (%02x) - ошибка контрольной суммы заголовка",ptable[npart].pname,ptable[npart].hd.code>>16);
		errflag=1;
		}  

	ptable[npart].hd.crc = crc;  // восстанавливаем CRC

	// вычисляем и проверяем CRC раздела
	calc_crc16(npart);

	if (crcblocksize != crcsize(npart)) 
		{
		fprintf(stdout, "\n! Раздел %s (%02x) - неправильный размер блока контрольных сумм",ptable[npart].pname,ptable[npart].hd.code>>16);
		errflag = 1;
		}    
	else if ( memcmp(crcblock,ptable[npart].csumblock,crcblocksize) ) 
		{
		fprintf(stdout, "\n! Раздел %s (%02x) - неправильная блочная контрольная сумма",ptable[npart].pname,ptable[npart].hd.code>>16);
		errflag=1;
		}  
  
	free(crcblock);


	ptable[npart].ztype=' ';
	// Определение zlib-сжатия


	if ( (*(uint16_t*)ptable[npart].pimage) == 0xda78 ) 
		{
		ptable[npart].zflag = ptable[npart].hd.psize;  // сохраняем сжатый размер 
		zlen = 52428800;
		zbuf = malloc(zlen);  // буфер в 50М

		// распаковываем образ раздела
		if ( Z_OK != (res = uncompress (zbuf, &zlen, ptable[npart].pimage, ptable[npart].hd.psize)) )
			{
			fprintf(stdout, "\n! Ошибка распаковки раздела %s (%02x)\n",ptable[npart].pname,ptable[npart].hd.code>>16);
			errflag = 1;
			}

		  // создаем новый буфер образа раздела и копируем в него рапаковынные данные
		  free(ptable[npart].pimage);

		ptable[npart].pimage = malloc(zlen);
		memcpy(ptable[npart].pimage, zbuf, zlen);
		ptable[npart].hd.psize = zlen;
		free(zbuf);

		// перерассчитываем контрольные суммы
		calc_crc16(npart);
		ptable[npart].hd.crc = crc16((uint8_t*)&ptable[npart].hd, sizeof(struct pheader));
		ptable[npart].ztype = 'Z';
		}

	// Определение lzma-сжатия
	if ( (ptable[npart].pimage[0] == 0x5d) && (*(uint64_t*)(ptable[npart].pimage + 5) == 0xffffffffffffffff)) 
		{
		ptable[npart].zflag=ptable[npart].hd.psize;  // сохраняем сжатый размер 
		zlen = 52428800;
		zbuf = malloc(zlen);  // буфер в 50М
		// распаковываем образ раздела
		zlen = lzma_decode(ptable[npart].pimage, ptable[npart].hd.psize, zbuf);
		if (zlen > 52428800) 
			{
			fprintf(stdout, "Превышен размер буфера\n");
			exit(1);
			}  

		if (res == -1) 
			{
			fprintf(stdout, "\n! Ошибка распаковки раздела %s (%02x)\n",ptable[npart].pname,ptable[npart].hd.code>>16);
			errflag=1;
			}

		// создаем новый буфер образа раздела и копируем в него рапаковынные данные
		free(ptable[npart].pimage);
		ptable[npart].pimage=malloc(zlen);
		memcpy(ptable[npart].pimage,zbuf,zlen);
		ptable[npart].hd.psize=zlen;
		free(zbuf);

		// перерассчитываем контрольные суммы
		calc_crc16(npart);
		ptable[npart].hd.crc=crc16((uint8_t*)&ptable[npart].hd,sizeof(struct pheader));
		ptable[npart].ztype='L';
		}
  
  	// продвигаем счетчик разделов
	npart++;

	// отъезжаем, если надо, вперед на границу слова
	res = ftell(in);
	if ( (res & 3)) 
		fseek(in, (res + 4) & (~3), SEEK_SET);
}


//*******************************************************
//*  Поиск разделов в файле прошивки
//* 
//* возвращает число найденных разделов
//*******************************************************
int	findparts	(FILE* in) 
{
// буфер префикса BIN-файла
char	prefix[0x5c];
int	signsize, hd_dload_id, i;

	// поиск начала цепочки разделов в файле
	while ( fread(&i, 1, 4, in) == 4 ) 
		{
		if (i == PTABLE$K_MAGIC) 
			break;
		}

	if ( feof(in) ) 
		{
		fprintf(stdout, "В файле не найдены разделы - файл не содержит образа прошивки\n");
		exit(0);
		}  

	// текущая позиция в файле должна быть не ближе 0x60 от начала - размер заголовка всего файла
	if (ftell(in) < 0x60) 
		{
		fprintf(stdout, "Заголовок файла имеет неправильный размер\n");
		exit(0);
		}    

	fseek(in, -0x60, SEEK_CUR); // отъезжаем на начало BIN-файла

	// вынимаем префикс
	fread(prefix, 0x5c, 1, in);
	hd_dload_id = *((uint32_t*)&prefix[0]);

	// если принудительно dload_id не установлен - выбираем его из заголовка
	if (dload_id == -1) 
		dload_id=hd_dload_id;

	if ( dload_id > 0xf ) 
		{
		fprintf(stdout, "Неверный код типа прошивки (dload_id) в заголовке - %x",dload_id);
		exit(0);
		}  

	fprintf(stdout, "Код файла прошивки: %x (%s)\n", hd_dload_id, fw_description(hd_dload_id));

	// поиск остальных разделов
	while ( 1 )
		{
		fprintf(stdout, "\r Поиск раздела # %i", npart); 
		fflush(stdout);	

		if ( fread(&i,1,4,in) != 4 )  
			break;			// конец файла

		if ( i != PTABLE$K_MAGIC) 
			break;			// образец не найден - конец цепочки разделов

		fseek(in, -4, SEEK_CUR);	// отъезжаем назад, на начало заголовка
		extract(in);                    // извлекаем раздел
		}

	fprintf(stdout, "\r                                 \r");

	// ищем цифровую подпись
	if ( 0 > (signsize = search_sign()) )
		fprintf(stdout, "D|I|G|I|T|A|L Signature has not found");
	else    {
		fprintf(stdout, "D|I|G|I|T|A|L Signature %i octets", signsize);
		fprintf(stdout, "Hash of the public key: %s", signver_hash);
		}

	if ( ((signsize == -1) && (dload_id>7)) || ((signsize != -1) && (dload_id < 8))) 
		fprintf(stdout, "! ВНИМАНИЕ: Наличие цифровой подписи не соответствует коду типа прошивки: %02x", dload_id);

	return npart;
}


//*******************************************************
//* Поиск разделов в многофайловом режиме
//*******************************************************
void findfiles (char* fdir) 
{
char filename[200];  
FILE* in;
  
	fprintf(stdout, "Поиск файлов-образов разделов...\n\n ##   Размер      ID        Имя          Файл\n-----------------------------------------------------------------\n");

	for (npart = 0; npart < 30; npart++) 
		{
		if ( !find_file(npart, fdir, filename, &ptable[npart].hd.code, &ptable[npart].hd.psize) ) 
			break; // конец поиска - раздела с таким ID не нашли

		// получаем символическое имя раздела
		find_pname(ptable[npart].hd.code, ptable[npart].pname);
		fprintf(stdout, "%02i  %8i  %08x  %-14.14s  %s", npart, ptable[npart].hd.psize, ptable[npart].hd.code, ptable[npart].pname, filename);
		fflush(stdout);
    
		// распределяем память под образ раздела
		if ( !(ptable[npart].pimage=malloc(ptable[npart].hd.psize)) )
			{
			fprintf(stdout, "\n! Ошибка распределения памяти, раздел #%i, размер = %i байт\n", npart, ptable[npart].hd.psize);
			exit(0);
			}
    
		// читаем образ в буфер
		if ( !(in = fopen(filename, "r")) )
			{
			fprintf(stdout, "Ошибка открытия файла %s", filename);
			return;
			} 

		fread(ptable[npart].pimage,ptable[npart].hd.psize,1,in);

		fclose(in);
		}

	if ( !npart ) 
		{
		fprintf(stdout, "\n! Не найдено ни одного файла с образом раздела в каталоге %s", fdir);
		exit(0);
		} 
}