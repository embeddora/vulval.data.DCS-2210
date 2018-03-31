#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <linux/rtc.h>
#include <linux/ioctl.h>
#if defined( PLAT_DM355 )
	#include <sound/oss/Dm355_audio_wm8978.h>
#endif

//#define LOCAL_DEBUG
#include <debug.h>
#include <bool.h>
#include <sysconf.h>
#include <storage.h>
#include <ApproDrvMsg.h>
#include <sys_msg_drv.h>
#include <Appro_interface.h>

#include "verify.h"
#include "ioport.h"
#include "gio_util.h"
#ifdef SUPPORT_VISCA
#include <libvisca.h>
#endif
//#define	TRUE		1
//#define	FALSE		0

/***************************************************************************
 *																																				 *
 ***************************************************************************/
#define LED_CONTROL_PATH            "/proc/appro/net_led"  // add by Alex, 2009.05.12
//#define	SD_OK(sysinfo)			( ( gio_read( 30 ) == 0 ) && ( (sysinfo)->osd_mmc == SD_READY ) )
/***************************************************************************
 *																																				 *
 ***************************************************************************/
static int ledtest_pid       = 0;
static int alarmout1test_pid = 0;
static int rs232test_pid = 0;

extern SysInfo *sys_info;    // add by Alex, 2009.05.11
/***************************************************************************
 *																																				 *
 ***************************************************************************/
void colorbar_on( void )
{
	int data;

	if ( ioport_init() < 0 )
		return;
	
#if defined( PLAT_DM365 )	
	ioport_read_data( 0x01C71e08, &data );
	data |= 0x100;
	ioport_write_data( 0x01C71e08, &data );
#else
	ioport_read_data( 0x01C70408, &data );
	data |= 0x100;
	ioport_write_data( 0x01C70408, &data );
#endif
	ioport_exit();
}
/***************************************************************************
 *																																				 *
 ***************************************************************************/
void colorbar_off( void )
{
	int data;

	if ( ioport_init() < 0 )
		return;
	
#if defined( PLAT_DM365 )
	ioport_read_data( 0x01C71e08, &data );
	data &= ~ 0x100;
	ioport_write_data( 0x01C71e08, &data );
#else
	ioport_read_data( 0x01C70408, &data );
	data |= 0x100;
	ioport_write_data( 0x01C70408, &data );
#endif
	ioport_exit();
}
/***************************************************************************
 *																																				 *
 ***************************************************************************/
int sd_verify( void )
{
	int fd;
	int loop;
	char data;

	if ( ( fd = open( "/mnt/mmc/test.txt", O_CREAT | O_TRUNC | O_WRONLY ) ) < 0 )
		return FALSE;
	for ( loop = 0, data = 0; loop < 512; loop++, data++ )
		if ( write( fd, &data, 1 ) != 1 )
		{
			close( fd );
			return FALSE;
		}
	close( fd );
	if ( ( fd = open( "/mnt/mmc/test.txt", O_RDONLY ) ) < 0 )
		return FALSE;
	for ( loop = 0, data = 0; loop < 512; loop++ )
		if ( ( read( fd, &data, 1 ) != 1 ) || ( data != ( loop & 0xFF ) ) )
		{
			close( fd );
			return FALSE;
		}
	close( fd );
	return TRUE;
}
/***************************************************************************
 *																																				 *
 ***************************************************************************/
int System_Verify_Switch( MODULE_ID id, int status )
{
	int fd, ret;
	char data[] = {'1', '0'};
	
	switch(id)
	{
	case MODULE_LED:
		fd = open(LED_CONTROL_PATH, O_WRONLY);
		if(fd < 0){
			DBG("open "LED_CONTROL_PATH" error \n");
			return -1;				
		}

		ret = write(fd, &data[status], 1);
		if(ret <= 0) {
			DBGERR("write "LED_CONTROL_PATH" error !!\n");
			close(fd);
			return -1;
		}

		close(fd);
		break;
		
	case MODULE_DN_ONOFF:
		if(status == 1) {
			sys_info->status.dnc_state = -1;
			//sys_info->config.lan_config.dnc_mode = DNC_MODE_NIGHT;
			sys_info->ipcam[DNC_MODE].config.u8= DNC_MODE_NIGHT;
			sys_info->cmd.dn_control |= DNC_CMD_MODE;
		}
		else {
			//sys_info->config.lan_config.dnc_mode = DNC_MODE_DAY;
			sys_info->ipcam[DNC_MODE].config.u8 = DNC_MODE_DAY;
			sys_info->cmd.dn_control |= DNC_CMD_MODE;
		}
		break;
	default:
		break;
	}
	
	sys_info->status.verify[id] = status;

	return 0;
}
/***************************************************************************
 *																																				 *
 ***************************************************************************/
int System_Verify( unsigned code, unsigned *status )
{
	int nRet;
#ifdef SUPPORT_VISCA
	pid_t visca_pid;
	char ptz_mode[8];
	char ptz_arg[64];
#endif
	switch( code )
	{
		/* component chip */
		case VERIFY_IC_IRAM:
			{
				int handler;
				
				if ( ( handler = open( "/proc/appro/iram_test", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( read( handler, (char *) status, 1 ) == 1 )
				{
					*status = ( *(char *) status == '1' ) ? TRUE : FALSE;
					close( handler );
					return TRUE;
				}
				else
				{
					close( handler );
					return FALSE;
				}
			}

		case VERIFY_IC_SRAM:
			return FALSE;

		case VERIFY_IC_SDRAM_01:
			{
				int handler;

				if ( ( handler = open( "/proc/appro/sdram_test", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( read( handler, (char *) status, 1 ) == 1 )
				{
					*status = ( *(char *) status == '1' ) ? TRUE : FALSE;
					close( handler );
					return TRUE;
				}
				else
				{
					close( handler );
					return FALSE;
				}
			}

		case VERIFY_IC_SDRAM_02:
			return FALSE;

		case VERIFY_IC_FLASH:
			{
				int handler;

				if ( ( handler = open( "/proc/appro/nand_test", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( read( handler, (char *) status, 1 ) == 1 )
				{
					*status = ( *(char *) status == '1' ) ? TRUE : FALSE;
					close( handler );
					return TRUE;
				}
				else
				{
					close( handler );
					return FALSE;
				}
			}

		case VERIFY_IC_EXT_FLASH:
			return FALSE;

		case VERIFY_IC_CCD:
			{

#if defined( PLAT_DM365 )

				FILE * fd = NULL;
				char buff[1024] = "";
				char * ptr = NULL;
				long long int_cnt = 0, int_cnt_old = 0;
				
				if ( ( fd = fopen( "/proc/interrupts", "r" ) ) == NULL ) {
					DBGERR("open %s failed !!", "/proc/interrupts");
					return FALSE;
				}

				fread(buff, sizeof(buff) - 1, 1, fd);
				ptr = strstr(buff, "0:");
				fclose( fd );
				if(ptr) {
					ptr += 3;
					int_cnt_old = atoll(ptr);
					dbg("int_cnt_old = %lld", int_cnt_old);
				}
				else {
					return FALSE;
				}

				sleep(1);

				if ( ( fd = fopen( "/proc/interrupts", "r" ) ) == NULL ) {
					DBGERR("open %s failed !!", "/proc/interrupts");
					return FALSE;
				}
				
				fread(buff, sizeof(buff) - 1, 1, fd);
				ptr = strstr(buff, "0:");
				fclose( fd );
				if(ptr) {
					ptr += 3;
					int_cnt = atoll(ptr);
					dbg("int_cnt = %lld", int_cnt);
				}
				else {
					return FALSE;
				}

				if(int_cnt > int_cnt_old) {
					*status = TRUE;
					return TRUE;
				}

				return FALSE;
#elif defined( PLAT_DM355 )


				int handler;
				int idx;
				char s[11];
				static int second1 = -1, second2;

				if ( ( handler = open( "/proc/appro/ccd_serial", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( ( second1 == -1 ) || ( ( time( 0 ) - second2 ) > 1 ) )
				{
					for ( idx = 0; idx < 10; idx++ )
						if ( read( handler, &s[idx], 1 ) <= 0 )
							break;
					s[idx] = 0;
					close( handler );
					if ( ( idx = atoi( s ) ) == 0 )
					{
						*status = FALSE;
						return TRUE;
					}
					if ( second1 == -1 )
					{
						second1 = idx;
						second2 = time( 0 );
						return FALSE;
					}
					else
					{
						*status = ( ( idx -  second1 ) > 0 ) ? TRUE : FALSE;
						second1 = -1;
						return TRUE;
					}	
				}
				else
				{
					close( handler );
					return FALSE;
				}
#else
			return TRUE;
#endif
			}
		case VERIFY_DEVICE_LIGHT_SEN:
//#if defined( PLAT_DM365 )
			*status = gio_read( DNC_DETECT_GIO );
//#else
//			*status = gio_read( 7 );
//#endif
			return TRUE;
		case VERIFY_IC_DSP_01:
			{
				int SerialBook;
				AV_DATA av_data;
				int testcount;

				if ( GetAVData( AV_OP_GET_MJPEG_SERIAL, -1, &av_data ) != RET_NO_VALID_DATA )
				{
					if ( av_data.serial < 0 )
						return FALSE;
					SerialBook = av_data.serial;
					for ( *status = FALSE, testcount = 0; testcount < 10000; testcount++ )
						if ( GetAVData( AV_OP_LOCK_MJPEG, SerialBook, &av_data ) == RET_SUCCESS )
						{
							GetAVData( AV_OP_UNLOCK_MJPEG, SerialBook, &av_data );
							*status = TRUE;
							break;
						}
				}
				else
					if ( GetAVData( AV_OP_GET_MPEG4_SERIAL, -1, &av_data ) != RET_NO_VALID_DATA )
					{
						if ( av_data.serial < 0 )
							return FALSE;
						SerialBook = av_data.serial;
						for ( *status = FALSE, testcount = 0; testcount < 10000; testcount++ )
							if ( GetAVData( AV_OP_LOCK_MP4, SerialBook, &av_data ) == RET_SUCCESS )
							{
								GetAVData( AV_OP_UNLOCK_MP4, SerialBook, &av_data );
								*status = TRUE;
								break;
							}
					}
					else
						*status = FALSE;
				return TRUE;
			}
			*status = TRUE;
			return TRUE;

		case VERIFY_IC_DSP_02:
			return FALSE;

		case VERIFY_IC_DSP_03:
			return FALSE;

		case VERIFY_IC_DSP_04:
			return FALSE;

		case VERIFY_IC_ETHER1:
			{
				int handler;
				if ( ( handler = open( "/proc/appro/nic_detect", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( read( handler, (char *) status, 1 ) == 1 )
				{
					*status = ( *(char *) status == '1' ) ? TRUE : FALSE;
					close( handler );
					return TRUE;
				}
				else
				{
					close( handler );
					return FALSE;
				}
			}

		case VERIFY_IC_ETHER2:
			return FALSE;

		case VERIFY_IC_RTC:
			{
				
#if defined( PLAT_DM365 )

				int handler;
				
				if ( ( handler = open( "/proc/appro/rtc_test", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( read( handler, (char *) status, 1 ) == 1 )
				{
					*status = ( *(char *) status == '1' ) ? TRUE : FALSE;
					close( handler );
					return TRUE;
				}
				else
				{
					close( handler );
					return FALSE;
				}

#elif defined( PLAT_DM355 )
				int handler;
				static int second1 = -1, second2;
				struct tm tmdata;

				if ( ( handler = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( ( second1 == -1 ) || ( ( time( 0 ) - second2 ) > 1 ) )
				{
					if ( ioctl( handler, RTC_RD_TIME, &tmdata ) )
					{
						close( handler );
						return FALSE;
					}
					close( handler );
					if ( second1 == -1 )
					{
						second1 = tmdata.tm_sec;
						second2 = time( 0 );
						return FALSE;
					}
					else
					{
						*status = ( tmdata.tm_sec != second1 ) ? TRUE : FALSE;
						second1 = -1;
						return TRUE;
					}	
				}
				else
				{
					close( handler );
					return FALSE;
				}
#else
				return TRUE;
#endif
			}

		case VERIFY_IC_RTC_BATTERY:
			return FALSE;

		case VERIFY_IC_16550:
			return FALSE;

		case VERIFY_IC_AUDIO_01:
#ifdef PLAT_DM355
			{
				int handler;

				if ( ( handler = open( "/dev/dsp", O_WRONLY ) ) < 0 )
					return FALSE;
				ioctl( handler, Audio_Wm8978_BYPASS_AUDIO, 1 );
				close( handler );
				*status = TRUE;
				return TRUE;
			}
			*status = TRUE;
			return TRUE;
#elif PLAT_DM365
				sys_info->status.verify[MODULE_AUDIOBTPASS] = 1;
				*status = TRUE;
				return TRUE;
#else
			return FALSE;
#endif

		case VERIFY_IC_AUDIO_02:
			return FALSE;

		case VERIFY_IC_AUDIO_03:
			return FALSE;

		case VERIFY_IC_AUDIO_04:
			return FALSE;

		case VERIFY_IC_AUDIO_05:
			return FALSE;

		case VERIFY_IC_AUDIO_06:
			return FALSE;

		case VERIFY_IC_AUDIO_07:
			return FALSE;

		case VERIFY_IC_AUDIO_08:
			return FALSE;

		case VERIFY_IC_AUDIO_01_OFF:
#ifdef PLAT_DM355
			{
				int handler;

				if ( ( handler = open( "/dev/dsp", O_WRONLY ) ) < 0 )
					return FALSE;
				ioctl( handler, Audio_Wm8978_BYPASS_AUDIO, 0 );
				close( handler );
				*status = TRUE;
				return TRUE;
			}
#elif PLAT_DM365
				sys_info->status.verify[MODULE_AUDIOBTPASS] = 0;
				*status = TRUE;
				return TRUE;
#else
				
				return FALSE;
#endif
		case VERIFY_IC_AUDIO_02_OFF:
			return FALSE;

		case VERIFY_IC_AUDIO_03_OFF:
			return FALSE;

		case VERIFY_IC_AUDIO_04_OFF:
			return FALSE;

		case VERIFY_IC_AUDIO_05_OFF:
			return FALSE;

		case VERIFY_IC_AUDIO_06_OFF:
			return FALSE;

		case VERIFY_IC_AUDIO_07_OFF:
			return FALSE;

		case VERIFY_IC_AUDIO_08_OFF:
			return FALSE;

		case VERIFY_IC_DECODER_01:
		{
#ifdef PLAT_DM355
			if( sensor_read(0x80) == 0x51 && sensor_read(0x81) == 0x50) {
				*status = TRUE;
				return TRUE;
			}
#endif
			return FALSE;
		}

		case VERIFY_IC_DECODER_02:
			return FALSE;

		case VERIFY_IC_DECODER_03:
			return FALSE;

		case VERIFY_IC_DECODER_04:
			return FALSE;

		case VERIFY_IC_DECODER_05:
			return FALSE;

		case VERIFY_IC_DECODER_06:
			return FALSE;

		case VERIFY_IC_DECODER_07:
			return FALSE;

		case VERIFY_IC_DECODER_08:
			return FALSE;

		case VERIFY_IC_DECODER_09:
			return FALSE;

		case VERIFY_IC_DECODER_10:
			return FALSE;

		case VERIFY_IC_DECODER_11:
			return FALSE;

		case VERIFY_IC_DECODER_12:
			return FALSE;

		case VERIFY_IC_DECODER_13:
			return FALSE;

		case VERIFY_IC_DECODER_14:
			return FALSE;

		case VERIFY_IC_DECODER_15:
			return FALSE;

		case VERIFY_IC_DECODER_16:
			return FALSE;

		case VERIFY_IC_COP_01:
			{
				FILE * file_1264 = NULL;
				char buff[1024] = "";
				char * ptr = NULL;
				long long int_cnt = 0, int_cnt_old = 0;

				if ( ( file_1264 = fopen( "/proc/interrupts", "r" ) ) == NULL ) {
					DBGERR("open %s failed !!", "/proc/interrupts");
					return FALSE;
				}

				fread(buff, sizeof(buff) - 1, 1, file_1264);
				ptr = strstr(buff, "45:");
				fclose( file_1264 );
				if(ptr) {
					ptr += 3;
					int_cnt_old = atoll(ptr);
					dbg("int_cnt_old = %lld", int_cnt_old);
				}
				else {
					return FALSE;
				}

				sleep(1);

				if ( ( file_1264 = fopen( "/proc/interrupts", "r" ) ) == NULL ) {
					DBGERR("open %s failed !!", "/proc/interrupts");
					return FALSE;
				}
				
				fread(buff, sizeof(buff) - 1, 1, file_1264);
				ptr = strstr(buff, "45:");
				fclose( file_1264 );
				if(ptr) {
					ptr += 3;
					int_cnt = atoll(ptr);
					dbg("int_cnt = %lld", int_cnt);
				}
				else {
					return FALSE;
				}

				if(int_cnt > int_cnt_old) {
					*status = TRUE;
					return TRUE;
				}

				return FALSE;
			}

		case VERIFY_IC_COP_02:
			return FALSE;

		case VERIFY_IC_COP_03:
			return FALSE;

		case VERIFY_IC_COP_04:
			return FALSE;

		case VERIFY_IC_COP_05:
			return FALSE;

		case VERIFY_IC_COP_06:
			return FALSE;

		case VERIFY_IC_COP_07:
			return FALSE;

		case VERIFY_IC_COP_08:
			return FALSE;

		case VERIFY_IC_COP_09:
			return FALSE;

		case VERIFY_IC_COP_10:
			return FALSE;

		case VERIFY_IC_COP_11:
			return FALSE;

		case VERIFY_IC_COP_12:
			return FALSE;

		case VERIFY_IC_COP_13:
			return FALSE;

		case VERIFY_IC_COP_14:
			return FALSE;

		case VERIFY_IC_COP_15:
			return FALSE;

		case VERIFY_IC_COP_16:
			return FALSE;

		case VERIFY_IC_PROTECT:
			{
				int handler;

				if ( ( handler = open( "/proc/appro/pic_detect", O_RDONLY ) ) < 0 )
					return FALSE;
				if ( read( handler, (char *) status, 1 ) == 1 )
				{
					*status = ( *(char *) status == '1' ) ? TRUE : FALSE;
					close( handler );
					return TRUE;
				}
				else
				{
					close( handler );
					return FALSE;
				}
			}

		case VERIFY_IC_MIC0:
		case VERIFY_IC_MIC1:
			{
				int handler;
				unsigned delta;
				char *ptr;
				int diff;
				int SerialBook;
				AV_DATA av_data;
				int loop;
#ifdef PLAT_DM355

				if ( ( handler = open( "/dev/dsp", O_WRONLY ) ) < 0 )
					return FALSE;
#if ! defined( MODEL_LC7315 )
				if ( code == VERIFY_IC_MIC0 )
					ioctl( handler, Audio_Wm8978_SWAP_LR, 0 );
				else
#endif
					ioctl( handler, Audio_Wm8978_SWAP_LR, 1 );
				close( handler );
				GetAVData( AV_OP_GET_ULAW_SERIAL, -1, &av_data );
				if ( av_data.serial < 0 )
					return FALSE;
				SerialBook = av_data.serial;
				for ( loop = 0; loop < 8; loop++ )
				{
					while ( GetAVData( AV_OP_LOCK_ULAW, SerialBook, &av_data ) != RET_SUCCESS )
						;
					GetAVData( AV_OP_UNLOCK_ULAW, SerialBook, &av_data );
					SerialBook = av_data.serial + 1;
				}
				while ( GetAVData( AV_OP_LOCK_ULAW, SerialBook, &av_data ) != RET_SUCCESS )
					;
				for ( delta = 0, ptr = av_data.ptr;
					(unsigned) ptr < (unsigned) ( av_data.ptr + ( av_data.size - 1 ) ); ptr++ )
				{
					diff = ( *ptr & 0x7F ) - ( *( ptr + 1 ) & 0x7F );
					if ( diff < 0 )
						diff = -diff;
					if ( diff > 4 )
						delta += diff;
				}
				GetAVData( AV_OP_UNLOCK_ULAW, SerialBook, &av_data );
				*status = ( delta > av_data.size * 4 ) ? TRUE : FALSE;
				return TRUE;
			
#elif PLAT_DM365

				GetAVData( AV_OP_GET_ULAW_SERIAL, -1, &av_data );
				if ( av_data.serial < 0 )
					return FALSE;
				SerialBook = av_data.serial;
				for ( loop = 0; loop < 8; loop++ )
				{
					while ( GetAVData( AV_OP_LOCK_ULAW, SerialBook, &av_data ) != RET_SUCCESS )
						;
					GetAVData( AV_OP_UNLOCK_ULAW, SerialBook, &av_data );
					SerialBook = av_data.serial + 1;
				}
				while ( GetAVData( AV_OP_LOCK_ULAW, SerialBook, &av_data ) != RET_SUCCESS )
					;
				for ( delta = 0, ptr = av_data.ptr;
					(unsigned) ptr < (unsigned) ( av_data.ptr + ( av_data.size - 1 ) ); ptr++ )
				{
					diff = ( *ptr & 0xFF ) - ( *( ptr + 1 ) & 0xFF );
					if ( diff < 0 )
						diff = -diff;
					if ( diff > 4 )
						delta += diff;

				}

				GetAVData( AV_OP_UNLOCK_ULAW, SerialBook, &av_data );
				*status = ( delta > av_data.size ) ? TRUE : FALSE;
				return TRUE;
#else
				return FALSE; 
#endif
			}
        case VERIFY_IC_MIC2:
            {
				sys_info->status.verify[MODULE_AUDIOBTPASS] = 1;
				*status = TRUE;
				return TRUE;
            }
        case VERIFY_IC_MIC2_OFF:
            {
				sys_info->status.verify[MODULE_AUDIOBTPASS] = 0;
				*status = TRUE;
				return TRUE;
            }
		/* input device */
		case VERIFY_IO_INPUT_01:
			{
				#ifdef SUPPORT_VISCA
				*status = ( sys_info->status.alarm1 == 1 ) ? TRUE : FALSE;
				return TRUE;
				#else
				static int cache1 = -1, cache2 = -1;
				int data;

				if ( cache1 == -1 )
				{
					System_Verify_Switch( MODULE_ALARM_IN, 1 );
					cache1 = gio_read( GIO_ALARMIN_1 );
				}
				*status = ( ( data = gio_read( GIO_ALARMIN_1 ) ) != 0 ) ? TRUE : FALSE;
				if ( ( cache2 == -1 ) && ( data != cache1 ) )
					cache2 = data;
				if ( ( cache2 != -1 ) && ( data != cache2 ) )
				{
					cache1 = cache2 = -1;
					System_Verify_Switch( MODULE_ALARM_IN, 0 );
				}
				return TRUE;
				
				#endif
			}

		case VERIFY_IO_INPUT_02:
			{
				#ifdef SUPPORT_VISCA
				*status = ( sys_info->status.alarm2 == 1 ) ? TRUE : FALSE;
				return TRUE;
				#else
				static int cache1 = -1, cache2 = -1;
				int data;

				if ( cache1 == -1 )
				{
					System_Verify_Switch( MODULE_ALARM_IN, 1 );
					cache1 = gio_read( GIO_ALARMIN_2 );
				}
				*status = ( ( data = gio_read( GIO_ALARMIN_2 ) ) != 0 ) ? TRUE : FALSE;
				if ( ( cache2 == -1 ) && ( data != cache1 ) )
					cache2 = data;
				if ( ( cache2 != -1 ) && ( data != cache2 ) )
				{
					cache1 = cache2 = -1;
					System_Verify_Switch( MODULE_ALARM_IN, 0 );
				}
				return TRUE;
				#endif
			}

		case VERIFY_IO_INPUT_03:
			#ifdef SUPPORT_VISCA
			*status = ( sys_info->status.alarm3 == 1 ) ? TRUE : FALSE;
				return TRUE;
			#else
			return FALSE;
			#endif	
		case VERIFY_IO_INPUT_04:
			#ifdef SUPPORT_VISCA
			*status = ( sys_info->status.alarm4 == 1 ) ? TRUE : FALSE;
				return TRUE;
			#else
			return FALSE;
			#endif

		case VERIFY_IO_INPUT_05:
			#ifdef SUPPORT_VISCA
			*status = ( sys_info->status.alarm5 == 1 ) ? TRUE : FALSE;
				return TRUE;
			#else
			return FALSE;
			#endif

		case VERIFY_IO_INPUT_06:
			#ifdef SUPPORT_VISCA
			*status = ( sys_info->status.alarm6 == 1 ) ? TRUE : FALSE;
				return TRUE;
			#else
			return FALSE;
			#endif

		case VERIFY_IO_INPUT_07:
			#ifdef SUPPORT_VISCA
			*status = ( sys_info->status.alarm7 == 1 ) ? TRUE : FALSE;
				return TRUE;
			#else
			return FALSE;
			#endif

		case VERIFY_IO_INPUT_08:
			#ifdef SUPPORT_VISCA
			*status = ( sys_info->status.alarm8 == 1 ) ? TRUE : FALSE;
				return TRUE;
			#else
			return FALSE;
			#endif

		case VERIFY_IO_INPUT_09:
			return FALSE;

		case VERIFY_IO_INPUT_10:
			return FALSE;

		case VERIFY_IO_INPUT_11:
			return FALSE;

		case VERIFY_IO_INPUT_12:
			return FALSE;

		case VERIFY_IO_INPUT_13:
			return FALSE;

		case VERIFY_IO_INPUT_14:
			return FALSE;

		case VERIFY_IO_INPUT_15:
			return FALSE;

		case VERIFY_IO_INPUT_16:
			return FALSE;

		case VERIFY_IO_RESET_01:
			{
				static int cache1 = -1, cache2 = -1;
				int data;

				if ( cache1 == -1 )
				{
					System_Verify_Switch( MODULE_ALARM_RST, 1 );
					cache1 = gio_read( 101 );
				}
				*status = ( ( data = gio_read( 101 ) ) != 0 ) ? TRUE : FALSE;
				if ( ( cache2 == -1 ) && ( data != cache1 ) )
					cache2 = data;
				if ( ( cache2 != -1 ) && ( data != cache2 ) )
				{
					cache1 = cache2 = -1;
					System_Verify_Switch( MODULE_ALARM_RST, 0 );
				}
				return TRUE;
			}

		case VERIFY_IO_RESET_02:
			return FALSE;

		case VERIFY_IO_RESET_03:
			return FALSE;

		case VERIFY_IO_RESET_04:
			return FALSE;

		case VERIFY_IO_RESET_05:
			return FALSE;

		case VERIFY_IO_RESET_06:
			return FALSE;

		case VERIFY_IO_RESET_07:
			return FALSE;

		case VERIFY_IO_RESET_08:
			return FALSE;

		case VERIFY_IO_RESET_09:
			return FALSE;

		case VERIFY_IO_RESET_10:
			return FALSE;

		case VERIFY_IO_RESET_11:
			return FALSE;

		case VERIFY_IO_RESET_12:
			return FALSE;

		case VERIFY_IO_RESET_13:
			return FALSE;

		case VERIFY_IO_RESET_14:
			return FALSE;

		case VERIFY_IO_RESET_15:
			return FALSE;

		case VERIFY_IO_RESET_16:
			return FALSE;

		case VERIFY_IO_OUTPUT_01:
			#ifdef SUPPORT_VISCA
				sprintf(ptz_mode, "%d", CAM_ALARM_OUTPUT_ENABLE);
				sprintf(ptz_arg, "0");
				System_Verify_Switch( MODULE_ALARM_OUT, 1 );
				if ( !( visca_pid = fork() ) ) {
					/* the child */
					
					execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
					DBGERR("exec "VISCA_EXEC_PATH" failed\n");
					_exit(0);
				}
				else if (visca_pid < 0) {
					DBGERR("fork "VISCA_EXEC_PATH" failed\n");
					return -1;
				}
				
				*status = TRUE;
				return TRUE;
			#else
			if ( ! alarmout1test_pid )
			{
				System_Verify_Switch( MODULE_ALARM_OUT, 1 );
				if ( ! ( alarmout1test_pid = fork() ) )
					execl( "/opt/ipnc/tester", "tester", "alarm1", 0 );
			}
			*status = TRUE;
			return TRUE;
			#endif
		case VERIFY_IO_OUTPUT_02:
			return FALSE;

		case VERIFY_IO_OUTPUT_03:
			return FALSE;

		case VERIFY_IO_OUTPUT_04:
			return FALSE;

		case VERIFY_IO_OUTPUT_05:
			return FALSE;

		case VERIFY_IO_OUTPUT_06:
			return FALSE;

		case VERIFY_IO_OUTPUT_07:
			return FALSE;

		case VERIFY_IO_OUTPUT_08:
			return FALSE;

		case VERIFY_IO_OUTPUT_09:
			return FALSE;

		case VERIFY_IO_OUTPUT_10:
			return FALSE;

		case VERIFY_IO_OUTPUT_11:
			return FALSE;

		case VERIFY_IO_OUTPUT_12:
			return FALSE;

		case VERIFY_IO_OUTPUT_13:
			return FALSE;

		case VERIFY_IO_OUTPUT_14:
			return FALSE;

		case VERIFY_IO_OUTPUT_15:
			return FALSE;

		case VERIFY_IO_OUTPUT_16:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_01:
			#ifdef SUPPORT_VISCA
				sprintf(ptz_mode, "%d", CAM_ALARM_OUTPUT_DISABLE);
				sprintf(ptz_arg, "0");
				if ( !( visca_pid = fork() ) ) {
					/* the child */
					
					execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
					DBGERR("exec "VISCA_EXEC_PATH" failed\n");
					_exit(0);
				}
				else if (visca_pid < 0) {
					DBGERR("fork "VISCA_EXEC_PATH" failed\n");
					return -1;
				}
				System_Verify_Switch( MODULE_ALARM_OUT, 0 );
				*status = TRUE;
				return TRUE;
			#else
			if ( alarmout1test_pid )
			{
				kill( alarmout1test_pid, SIGUSR1 );
				alarmout1test_pid = 0;
				System_Verify_Switch( MODULE_ALARM_OUT, 0 );
			}
			*status = TRUE;
			return TRUE;
			#endif
		case VERIFY_IO_OUTPUT_OFF_02:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_03:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_04:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_05:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_06:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_07:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_08:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_09:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_10:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_11:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_12:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_13:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_14:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_15:
			return FALSE;

		case VERIFY_IO_OUTPUT_OFF_16:
			return FALSE;

		/* LED */
		case VERIFY_LED_ON:
			if ( ! ledtest_pid )
			{
				System_Verify_Switch( MODULE_LED, 1 );
				if ( ! ( ledtest_pid = fork() ) )
					execl( "/opt/ipnc/tester", "tester", "led", 0 );
			}
			*status = TRUE;
			return TRUE;

		case VERIFY_LED_OFF:
			if ( ledtest_pid )
			{
				kill( ledtest_pid, SIGUSR1 );
				ledtest_pid = 0;
				System_Verify_Switch( MODULE_LED, 0 );
			}
			*status = TRUE;
			return TRUE;

		case VERIFY_LED_01:
			return FALSE;

		case VERIFY_LED_02:
			return FALSE;

		case VERIFY_LED_03:
			return FALSE;

		case VERIFY_LED_04:
			return FALSE;

		case VERIFY_LED_05:
			return FALSE;

		case VERIFY_LED_06:
			return FALSE;

		case VERIFY_LED_07:
			return FALSE;

		case VERIFY_LED_08:
			return FALSE;

		case VERIFY_LED_OFF_01:
			return FALSE;

		case VERIFY_LED_OFF_02:
			return FALSE;

		case VERIFY_LED_OFF_03:
			return FALSE;

		case VERIFY_LED_OFF_04:
			return FALSE;

		case VERIFY_LED_OFF_05:
			return FALSE;

		case VERIFY_LED_OFF_06:
			return FALSE;

		case VERIFY_LED_OFF_07:
			return FALSE;

		case VERIFY_LED_OFF_08:
			return FALSE;

		/* DIP Switch */
		case VERIFY_DIP_01:
		{
			static int cache1 = -1, cache2 = -1;
			int data;

			if ( cache1 == -1 )
			{
				System_Verify_Switch( MODULE_DIP1_STATUS, 1 );
				//#if defined( PLAT_DM365 )
				cache1 = gio_read( GIO_IRIS_SWITCH );
				//#else
				//cache1 = gio_read( 5 );
				//#endif
			}
			//#if defined( PLAT_DM365 )
			*status = ( ( data = gio_read( GIO_IRIS_SWITCH ) ) != 0 ) ? TRUE : FALSE;
			//#else
			//*status = ( ( data = gio_read( 5 ) ) != 0 ) ? TRUE : FALSE;
			//#endif
			if ( ( cache2 == -1 ) && ( data != cache1 ) )
				cache2 = data;
			if ( ( cache2 != -1 ) && ( data != cache2 ) )
			{
				cache1 = cache2 = -1;
				System_Verify_Switch( MODULE_DIP1_STATUS, 0 );
			}
			return TRUE;
		}

		case VERIFY_DIP_02:
		{
			static int cache1 = -1, cache2 = -1;
			int data;

			if ( cache1 == -1 )
			{
				System_Verify_Switch( MODULE_DIP2_STATUS, 1 );
				//#if defined( PLAT_DM365 )
				cache1 = gio_read( GIO_DHCP );
				//#else
				//cache1 = gio_read( 3 );
				//#endif
			}
			//#if defined( PLAT_DM365 )
			*status = ( ( data = gio_read( GIO_DHCP ) ) != 0 ) ? TRUE : FALSE;
			//#else
			//*status = ( ( data = gio_read( 3 ) ) != 0 ) ? TRUE : FALSE;
			//#endif
			if ( ( cache2 == -1 ) && ( data != cache1 ) )
				cache2 = data;
			if ( ( cache2 != -1 ) && ( data != cache2 ) )
			{
				cache1 = cache2 = -1;
				System_Verify_Switch( MODULE_DIP2_STATUS, 0 );
			}
			return TRUE;
		}

		case VERIFY_DIP_03:
			return FALSE;

		case VERIFY_DIP_04:
			return FALSE;

		case VERIFY_DIP_05:
			return FALSE;

		case VERIFY_DIP_06:
			return FALSE;

		case VERIFY_DIP_07:
			return FALSE;

		case VERIFY_DIP_08:
			return FALSE;

		case VERIFY_DIP_RESET:
		{
			static int cache1 = -1, cache2 = -1;
			int data;
			int count, loop;

			if ( cache1 == -1 )
			{
				System_Verify_Switch( MODULE_LOAD_DEFAULT, 1 );
				for ( loop = 0, count = 0; loop < 7; loop++ )
					//#if defined( PLAT_DM365 )
					//if ( gio_read( 32 ) )
					//#else
					if ( gio_read( GIO_RST_DEFAULT ) )
					//#endif
						count++;
				*status = cache1 = ( count > 3 ) ? TRUE : FALSE;
				return TRUE;
			}
			for ( loop = 0, count = 0; loop < 7; loop++ )
				//#if defined( PLAT_DM365 )
					//if ( gio_read( 32 ) )
					//#else
					if ( gio_read( GIO_RST_DEFAULT ) )
					//#endif
					count++;
			*status = data = ( count > 3 ) ? TRUE : FALSE;
			if ( ( cache2 == -1 ) && ( data != cache1 ) )
				cache2 = data;
			if ( ( cache2 != -1 ) && ( data != cache2 ) )
			{
				cache1 = cache2 = -1;
				System_Verify_Switch( MODULE_LOAD_DEFAULT, 0 );
			}
			return TRUE;
		}
		case VERIFY_DIP_WPS:
		{
			static int cache1 = -1, cache2 = -1;
			int data;
			int count, loop;

			if ( cache1 == -1 )
			{
				System_Verify_Switch( MODULE_WPS, 1 );
				for ( loop = 0, count = 0; loop < 7; loop++ )
					//#if defined( PLAT_DM365 )
					//if ( gio_read( 32 ) )
					//#else
					if ( gio_read( 22 ) )
					//#endif
						count++;
				*status = cache1 = ( count > 3 ) ? TRUE : FALSE;
				return TRUE;
			}
			for ( loop = 0, count = 0; loop < 7; loop++ )
				//#if defined( PLAT_DM365 )
					//if ( gio_read( 32 ) )
					//#else
					if ( gio_read( 22 ) )
					//#endif
					count++;
			*status = data = ( count > 3 ) ? TRUE : FALSE;
			if ( ( cache2 == -1 ) && ( data != cache1 ) )
				cache2 = data;
			if ( ( cache2 != -1 ) && ( data != cache2 ) )
			{
				cache1 = cache2 = -1;
				System_Verify_Switch( MODULE_WPS, 0 );
			}
			return TRUE;
		}

		/* status */
		case VERIFY_STATUS_VLOSS_01:
		{
			*status = sys_info->videoloss;
			return TRUE;
		}


		case VERIFY_STATUS_VLOSS_02:
			return FALSE;

		case VERIFY_STATUS_VLOSS_03:
			return FALSE;

		case VERIFY_STATUS_VLOSS_04:
			return FALSE;

		case VERIFY_STATUS_VLOSS_05:
			return FALSE;

		case VERIFY_STATUS_VLOSS_06:
			return FALSE;

		case VERIFY_STATUS_VLOSS_07:
			return FALSE;

		case VERIFY_STATUS_VLOSS_08:
			return FALSE;

		case VERIFY_STATUS_VLOSS_09:
			return FALSE;

		case VERIFY_STATUS_VLOSS_10:
			return FALSE;

		case VERIFY_STATUS_VLOSS_11:
			return FALSE;

		case VERIFY_STATUS_VLOSS_12:
			return FALSE;

		case VERIFY_STATUS_VLOSS_13:
			return FALSE;

		case VERIFY_STATUS_VLOSS_14:
			return FALSE;

		case VERIFY_STATUS_VLOSS_15:
			return FALSE;

		case VERIFY_STATUS_VLOSS_16:
			return FALSE;

		case VERIFY_STATUS_ELOSS_01:
			return FALSE;

		case VERIFY_STATUS_ELOSS_02:
			return FALSE;

		/* series */
		case VERIFY_UART_232:
			#ifdef SUPPORT_VISCA
			if ( ! rs232test_pid )
			{
				
				if ( !( rs232test_pid = fork() ) ) {
					/* the child */
					
					execl("/opt/ipnc/rs232test", "rs232test", NULL);
					DBGERR("exec failed\n");
					_exit(0);
				}
				else if (rs232test_pid < 0) {
					DBGERR("fork  failed\n");
					return -1;
				}
			}
			*status = TRUE;
			return TRUE;
			
			#else
			return FALSE;
			#endif
			

		case VERIFY_UART_485:
			return FALSE;

		/* sd card */
		case VERIFY_SD_STATUS:
			*status = !!(sys_info->status.mmc & SD_READY); //SD_OK( sys_info );
			return TRUE;

		case VERIFY_SD_PROTECT:
			*status = !!(sys_info->status.mmc & SD_LOCK);
			return TRUE;
			/*	// Modified by Alex, 2010.03.04
			if ( SD_OK( sys_info ) )
			{
				*status = ( gio_read( 29 ) == 1 ) ? TRUE : FALSE;
				return TRUE;
			}
			else
				return FALSE;
			*/

		case VERIFY_SD_ERROR:
			return FALSE;

		case VERIFY_SD_ACCESS:
            //if ( SD_OK( sys_info ) && ( gio_read( 89 ) == 0 ) )
            if( (sys_info->status.mmc & SD_READY) && !(sys_info->status.mmc & SD_LOCK) ) 
            {
                *status = sd_verify();
                return TRUE;
            }
            else
                return FALSE;

		/* cf card */
		case VERIFY_CF_STATUS:
			return FALSE;

		case VERIFY_CF_ERROR:
			return FALSE;

		case VERIFY_CF_ACCESS:
			return FALSE;

		/* wireless */
		case VERIFY_WIRELESS_STATUS:
			return FALSE;

		case VERIFY_WIRELESS_ERROR:
			return FALSE;

		/* usb */
		case VERIFY_USB_STATUS:
			{
				int handler;

				if ( ( handler = open( "/proc/usb_appro", O_RDONLY ) ) < 0 )
					*status = FALSE;
				else
				{
					*status = TRUE;
					close( handler );
				}
				return TRUE;
			}

		case VERIFY_USB_ERROR:
			return FALSE;

		/* external memory */
		case VERIFY_EXTMEN_ERROR:
			return FALSE;

		case VERIFY_EXTMEN_ACCESS:
			return FALSE;

		case VERIFY_MISC_COLORBAR_ON:
			colorbar_on();
			*status = TRUE;
			return TRUE;

		case VERIFY_MISC_COLORBAR_OFF:
			colorbar_off();
			*status = TRUE;
			return TRUE;

        case VERIFY_MISC_FAN_ON:
            ManualCtrlFan(0x04);
            *status = TRUE;
            return TRUE;

        case VERIFY_MISC_FAN_OFF:
            ManualCtrlFan(0);
            *status = TRUE;
            return TRUE;

        case VERIFY_MISC_HEATER_01_ON:
            ManualCtrlHeater(0x02);
            *status = TRUE;
            return TRUE;

        case VERIFY_MISC_HEATER_01_OFF:
            ManualCtrlHeater(0);
            *status = TRUE;
            return TRUE;

        case VERIFY_MISC_HEATER_02_ON:
            ManualCtrlHeater(0x03);
            *status = TRUE;
            return TRUE;

        case VERIFY_MISC_HEATER_02_OFF:
            ManualCtrlHeater(0);
            *status = TRUE;
            return TRUE;

        case VERIFY_MISC_TEMPERATUE_01:
            *status = GetAfTemperature();
            return TRUE;

        case VERIFY_MISC_AF_ZOOM_WIDE:
        {
            int zoom = 0;
            SetAfZoomPosition(zoom);
            *status = TRUE;
            return TRUE;
        }
            
        case VERIFY_MISC_AF_ZOOM_TELE:
        {
            int zoom = MAX_AF_ZOOM;
            SetAfZoomPosition(zoom);
            *status = TRUE;
            return TRUE;
        }

        case VERIFY_MISC_AF_FOCUS_FAR:
        {
            int focus = MAX_AF_FOCUS;
            SetAfFocusPosition(focus);
            *status = TRUE;
            return TRUE;
        }

        case VERIFY_MISC_AF_FOCUS_NEAR:
        {
            int focus = 0;
            SetAfFocusPosition(focus);
            *status = TRUE;
            return TRUE;
        }

        case VERIFY_MISC_AF_SELFTEST:
        {
            int weighing = 5;
            int autofocus = 1;
            int zoom = 0;
            static int cache1 = -1;

            if(cache1 == -1) {
                sys_info->af_status.debug = 1;  // Enable AF OSD debug message.
                SetAfWeighting(weighing);
                SetAfZoomPosition(zoom);
                SetAfEnable(autofocus);
                cache1 = 0;
            }

            if(GetAfEnable() == AF_MANUAL) {
                *status = (GetAfFocusPosition() > 70) ? FALSE : TRUE;
                cache1 = -1;
            }
            else {
                *status = 2;
            }
            return TRUE;
        }

		case VERIFY_DEVICE_DN_TEST:
			System_Verify_Switch( MODULE_DN_MOTOR, 1 );
			{
				int pid;

				if ( ! ( pid = fork() ) )
					execl( "/opt/ipnc/tester", "tester", "dnmotor", 0 );
				waitpid( pid, &nRet, 0 );
			}
			System_Verify_Switch( MODULE_DN_MOTOR, 0 );
			*status = TRUE;
			return TRUE;

		case VERIFY_DEVICE_DN_SCAN:
			return FALSE;

		case VERIFY_DEVICE_DN_ON:
			System_Verify_Switch( MODULE_DN_ONOFF, 1 );
			*status = TRUE;
			return TRUE;

		case VERIFY_DEVICE_DN_OFF:
			System_Verify_Switch( MODULE_DN_ONOFF, 0 );
			*status = TRUE;
			return TRUE;

		case VERIFY_IMAGE_MIRROR_ON:
			return FALSE;

		case VERIFY_IMAGE_MIRROR_OFF:
			return FALSE;

		case VERIFY_IMAGE_FLIP_ON:
			return FALSE;

		case VERIFY_IMAGE_FLIP_OFF:
			return FALSE;
			
		case VERIFY_PTZ_HOME:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_PANTILT_HOME);
			sprintf(ptz_arg, "0");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_PAN_RIGHT:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_PANTILT_RIGHT);
			sprintf(ptz_arg, "8");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_PAN_LEFT:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_PANTILT_LEFT);
			sprintf(ptz_arg, "8");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_TILT_DOWN:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_PANTILT_DOWN);
			sprintf(ptz_arg, "8");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_TILT_UP:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_PANTILT_UP);
			sprintf(ptz_arg, "8");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_TILT_STOP:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_PANTILT_STOP);
			sprintf(ptz_arg, "0");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_ZOOM_TELE:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_ZOOM_TELE);
			sprintf(ptz_arg, "0");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_ZOOM_WIDE:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_ZOOM_WIDE);
			sprintf(ptz_arg, "0");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
		case VERIFY_PTZ_ZOOM_STOP:
			#ifdef SUPPORT_VISCA
			sprintf(ptz_mode, "%d", CAM_ZOOM_STOP);
			sprintf(ptz_arg, "0");
			if ( !( visca_pid = fork() ) ) {
				/* the child */
				
				execl(VISCA_EXEC_PATH, VISCA_EXEC, "-m", ptz_mode, "-n", ptz_arg, NULL);
				DBGERR("exec "VISCA_EXEC_PATH" failed\n");
				_exit(0);
			}
			else if (visca_pid < 0) {
				DBGERR("fork "VISCA_EXEC_PATH" failed\n");
				return -1;
			}
			*status = TRUE;
			return TRUE;
			#else
			return FALSE;
			#endif
	}
	return FALSE;
}
/***************************************************************************
 *																																				 *
 ***************************************************************************/
