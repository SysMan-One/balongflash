#ifndef __SIGVER_H__
#define __SIGVER_H__	1

char signver_hash[];


void glist();
void gparm(char* sparm);
void dparm(char* sparm);
void send_signver();
char* fw_description(uint8_t code);
int32_t search_sign();


extern char *fwtypes[];

#endif	/* __SIGVER_H__ */