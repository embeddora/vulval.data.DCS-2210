#ifndef   __RS485_H__
#define   __RS485_H__

#include <sysconf.h>

#define RS485_DEVICE      "/dev/ttyS1"
#define MAX_INPUT_SIZE		128
#define RS485_BUFF_SIZE   128

/*
#if defined ( PLAT_DM355 )
#define RS485_TX_ENABLE   27
#elif defined ( PLAT_DM365 )
#define RS485_TX_ENABLE   88
#else 
#error UNKOWN platform
#endif
*/  // Move to gio_util.h by Alex. 2010.07.02
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int   rs485_init(SysInfo * pInfo);
int   rs485_free(void);
int   rs485_set_speed(int speed);
int   rs485_set_data(int data);
int   rs485_set_stop(int stop);
int   rs485_set_parity(int parity);
//int   rs485_set_type(SysInfo * pInfo);
int   rs485_write_data(char * string);
void sigio_handler(int dummy);
int string_to_ascii(char * ptr, char * buff);
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#endif

