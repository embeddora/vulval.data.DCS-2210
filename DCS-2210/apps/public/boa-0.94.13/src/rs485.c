
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <strings.h>  // for bzero
#include <unistd.h>   // for read
#include <string.h>   // for strlen
#include <errno.h>
#include <stdlib.h>


#define  LOCAL_DEBUG
#include <debug.h>
#include <sysconf.h>
#include <sysinfo.h>
#include <gio_util.h>

#include "boa.h"
#include "rs485.h"
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
static int fd_485 = -1;
static int rsflag;
static struct termios oldtio;
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void sigio_handler(int dummy)
{
	static char test_str[] = {0x41, 0x01, 0x20, 0x02, 0x02, 0x00, 0x4f};
	static char resp_str[] = {0x41, 0x20, 0x01, 0x02, 0x02, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f};
	int iLen;
	char buffer[MAX_INPUT_SIZE];

	usleep(100);
	iLen = read(fd_485, buffer, MAX_INPUT_SIZE);
	if (iLen > 0) {
        
		//if ( (iLen == 1) && (buffer[0] == 0))
		//	return;
        
		if (memcmp(buffer, test_str, sizeof(test_str)) == 0) {
			__u16 year = sys_info->tm_now.tm_year+1900;
			resp_str[6] = year & 0xff;
			resp_str[7] = year >> 8;
			resp_str[8] = sys_info->tm_now.tm_mon+1;
			resp_str[9] = sys_info->tm_now.tm_mday;
			resp_str[10] = sys_info->tm_now.tm_hour;
			resp_str[11] = sys_info->tm_now.tm_min;
			resp_str[12] = sys_info->tm_now.tm_sec;
			memcpy(buffer, resp_str, sizeof(resp_str));
			iLen = sizeof(resp_str);
		}
		
		sys_info->osd_uart = 1;
		gio_write(RS485_TX_ENABLE, 1);
		write(fd_485, buffer, iLen);
		tcdrain(fd_485);
		gio_write(RS485_TX_ENABLE, 0);

#if 0
		int i;
		for (i=0; i<iLen; i++)
			fprintf(stderr, "%02x ", buffer[i]);
		fprintf(stderr, "\n");
#endif
	}
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int rs485_init(SysInfo * pInfo)
{
  struct termios newtio;

  if(pInfo == NULL)     return -1;

  //sys_info = pInfo;

  fd_485 = open(RS485_DEVICE, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd_485 < 0) {
    DBGERR(RS485_DEVICE);
    return -1;
  }else
	{
		fprintf( stderr, "rs485_init: fd_485 = %d\n", fd_485 );
  }

  if(tcgetattr(fd_485, &oldtio) < 0) /* save current port settings */
    return -1;

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = CLOCAL | CREAD ;
  newtio.c_iflag = 0;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0;
  newtio.c_cc[VMIN]  = 0;

  /* allow the process to receive SIGIO */
  fcntl(fd_485, F_SETOWN, getpid());

  if((rsflag = fcntl(fd_485, F_GETFL, 0)) < 0)
    return -1;

  if(fcntl(fd_485, F_SETFL, rsflag | FASYNC | O_NONBLOCK ) < 0 )
    return -1;

  tcflush(fd_485, TCIOFLUSH);

  if( tcsetattr(fd_485, TCSANOW, &newtio) == -1) {
    DBGERR("tcsetattr failed.");
    return -1;
  }

  rs485_set_speed(pInfo->ipcam[RS485_SPEED].config.u8);
  rs485_set_data(pInfo->ipcam[RS485_DATA].config.u8);
  rs485_set_stop(pInfo->ipcam[RS485_STOP].config.u8);
  rs485_set_parity(pInfo->ipcam[RS485_PARITY].config.u8);
  //rs485_set_speed(pInfo->config.rs485.speed);
  //rs485_set_data(pInfo->config.rs485.data);
  //rs485_set_stop(pInfo->config.rs485.stop);
  //rs485_set_parity(pInfo->config.rs485.parity);
  //rs485_set_type(pInfo);

  gio_write(RS485_TX_ENABLE, 0);

  dbg("RS485 Initial finish \n");

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int rs485_free(void)
{
  if(fd_485 < 0)    return -1;

  fcntl(fd_485, F_SETFL, rsflag );

  tcsetattr(fd_485, TCSANOW, &oldtio);

  close(fd_485);

  fd_485 = -1;

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int string_to_ascii(char * src, char * buff)
{
  char *ptr;
  int  len = strlen(src);
  ptr = (char*)malloc(len + 1);
  int  i, j;
  
  memset(ptr, 0, len + 1);
  strncpy(ptr, src, len);

  if(len > RS485_BUFF_SIZE - 1 )
    len = RS485_BUFF_SIZE - 1;

  for(i = 0; i < len; i++)
  {
    if( *(ptr + i) >= '0' && *(ptr + i) <= '9')  /*  between '0' ~ '9'  */
      *(ptr + i) -= 0x30;
    else if( *(ptr + i) >= 'A' && *(ptr + i) <= 'F')  /*  between 'A' ~ 'F'  */
      *(ptr + i) -= 0x37;
    else if( *(ptr + i) >= 'a' && *(ptr + i) <= 'f')  /*  between 'a' ~ 'f'  */
      *(ptr + i) -= 0x57;
    else
      *(ptr + i) = 0x00;
  }

  j = 0;
  for(i = 0; i < len ; i+=2)
  {
    buff[j] =  *(ptr + i) << 4;
    buff[j] |= *(ptr + i + 1);
    j++;
  }
  
  free(ptr);

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int rs485_write_data(char * string)
{
  int len;

  if(string == NULL)        return -1;
  if(strlen(string) < 0)    return -1;
  if(fd_485 < 0)    return -1;

  char writ_buff[RS485_BUFF_SIZE];
  memset(writ_buff, 0, RS485_BUFF_SIZE);
  

  if(string_to_ascii(string, writ_buff) == -1)
    return -1;
    //dbg("string=%s\n", string);
	len = strlen(string);

	len = strlen(string) >> 1;


	sys_info->osd_uart = 1;
	gio_write(RS485_TX_ENABLE, 1);
	write(fd_485, writ_buff, len);
	tcdrain(fd_485);
	gio_write(RS485_TX_ENABLE, 0);

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int rs485_set_speed(int speed)
{
  struct termios tio;

  if(tcgetattr(fd_485, &tio) < 0)
    return -1;

#ifdef CONFIG_BRAND_DLINK
  switch(speed)
  {
  default:
  case  0:    cfsetospeed(&tio, B2400);   cfsetispeed(&tio, B2400);      break;
  case  1:    cfsetospeed(&tio, B4800);   cfsetispeed(&tio, B4800);      break;
  case  2:    cfsetospeed(&tio, B9600);   cfsetispeed(&tio, B9600);      break;
  case  3:    cfsetospeed(&tio, B19200);  cfsetispeed(&tio, B19200);     break;
  }
#else
  switch(speed)
  {
  default:
  case  0:    cfsetospeed(&tio, B2400);   cfsetispeed(&tio, B2400);      break;
  case  1:    cfsetospeed(&tio, B4800);   cfsetispeed(&tio, B4800);      break;
  case  2:    cfsetospeed(&tio, B9600);   cfsetispeed(&tio, B9600);      break;
  case  3:    cfsetospeed(&tio, B19200);  cfsetispeed(&tio, B19200);     break;
  case  4:    cfsetospeed(&tio, B38400);  cfsetispeed(&tio, B38400);     break;
  case  5:    cfsetospeed(&tio, B57600);  cfsetispeed(&tio, B57600);     break;
  case  6:    cfsetospeed(&tio, B115200); cfsetispeed(&tio, B115200);    break;
  }
#endif

  if( tcsetattr(fd_485, TCSANOW, &tio) == -1) {
    DBGERR("tcsetattr failed.");
    return -1;
  }

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int rs485_set_data(int data)
{
  struct termios tio;

  if(tcgetattr(fd_485, &tio) < 0)
    return -1;
    
  tio.c_cflag &= ~CSIZE;  // clear data bits setting
  
  if(data == 1)
    tio.c_cflag |= CS8;
  else
    tio.c_cflag |= CS7;

  if( tcsetattr(fd_485, TCSANOW, &tio) == -1) {
    DBGERR("tcsetattr failed.");
    return -1;
  }

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int rs485_set_parity(int parity)
{
  struct termios tio;

  if(tcgetattr(fd_485, &tio) < 0)
    return -1;
    
  if(parity == 0)       // no parity
    tio.c_cflag &= ~PARENB;
  else if(parity == 1)  // parity even
    tio.c_cflag |= PARENB;
  else if(parity == 2)  // parity odd
    tio.c_cflag |= PARENB | PARODD;

  if( tcsetattr(fd_485, TCSANOW, &tio) == -1) {
    DBGERR("tcsetattr failed.");
    return -1;
  }

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int rs485_set_stop(int stop)
{
  struct termios tio;

  if(tcgetattr(fd_485, &tio) < 0)
    return -1;

  if(stop == 1) 
    tio.c_cflag |= CSTOPB;
  else
    tio.c_cflag &= ~CSTOPB;
  
  if( tcsetattr(fd_485, TCSANOW, &tio) == -1) {
    DBGERR("tcsetattr failed.");
    return -1;
  }

  return 0;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/

