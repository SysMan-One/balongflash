#ifndef __PTABLE_H__
#define __PTABLE_H__	1

#define	PTABLE$K_MAGIC	0xa55aaa55

/*
** Partition Header structure
*/
#pragma	pack(push, 1)
typedef struct pheader {
	int32_t		magic;		// 0xa55aaa55
	uint32_t	hdsize;		// размер заголовка
	uint32_t	hdversion;
	uint8_t		unlock[8];
	uint32_t	code;		// тип раздела
	uint32_t	psize;		// разме поля данных
	uint8_t		date[16];
	uint8_t		time[16];	// дата-время сборки прошивки
	uint8_t		version[32];	// версия пршоивки
	uint16_t	crc;		// CRC заголовка
	uint32_t	blocksize;	// размер блока CRC образа прошивки
} PHEADER; 
#pragma pack(pop)

/*
** Partition Table structure
*/
typedef struct ptb_t {
	unsigned char	pname[20];	// буквенное имя раздела
	struct pheader	hd;		// образ заголовка
	uint16_t	*csumblock;	// блок контрольных сумм
	uint8_t		*pimage;	// образ раздела
	uint32_t	offset;		// смещение в файле до начала раздела
	uint32_t	zflag;		// признак сжатого раздела  
	uint8_t		ztype;		// тип сжатия
} PTABLE;

//******************************************************
//*  Внешние массивы для хранения таблицы разделов
//******************************************************
extern	struct ptb_t ptable[];
extern	int npart; // число разделов в таблице

extern uint32_t errflag;

int	findparts	(FILE* in);
void	find_pname(unsigned int id,unsigned char* pname);
void	findfiles (char* fdir);
uint32_t psize(int n);

extern int dload_id;

#endif // !__PTABLE_H__
