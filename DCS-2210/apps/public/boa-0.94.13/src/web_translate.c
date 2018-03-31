
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/rtc.h> 
#include <arpa/inet.h> // for inet_ntoa
#include <fcntl.h>

//#define LOCAL_DEBUG
#include <debug.h>
#include <sys_log.h>         /* By Alex  */
#include <storage.h>
#include <config_range.h>
#include <utility.h>
#include <gio_util.h>
#include <net_config.h>
#include <file_msg_drv.h>
#include <system_default.h>
#include <sysctrl_struct.h>
#include <parser_name.h>
#include <libvisca.h>
#include <bios_data.h>
#include <support.h>

#include "ipnc_define.h"
#include "file_list.h"
#include "web_translate.h"

#include "sensor_lib.h"
#include <sysinfo.h>
#include <wireless.h>

#include <asm/arch/lenctrl_ioctl.h>
/*
#if defined ( PLAT_DM365 )
	#define GIO_DHCP	31
	#define GIO_IRIS_SWITCH	30
	
#elif defined ( PLAT_DM355 )
	#define GIO_DHCP	3
	#define GIO_IRIS_SWITCH	5
	
#else
#error Unkown platform
#endif
*/  // Move to gio_util.h by Alex. 2010.07.02

//static char *nicname = "eth0"; //kevinlee by check eth0 or ppp0

//SysInfo * gpSys_Info = NULL;

ARG_HASH_TABLE *arg_hash;
int profile_check(int profileid);
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int add_escape_char(char * src, char * dest, size_t size)
{
	__u32 cnt = 0;

	if(!src || !dest) {
		DBG("src = %p, dest = %p  \n", src, dest);
		return -1;
	}

	do
	{
	  switch( *src )
	  {
	  case '\'':
	  case '\"':
	  case '\\':
	   
	    /* escape character attached */
	    *dest++ = '\\';
	 
	    /* fall through */
	 
	  default:
	    /* clone the original character */
	    *dest++ = *src;
	    break;
	  }

	  cnt++;
	} while ( *src++ != 0x00 || cnt == size - 1 );

	*dest = 0x00;

	return cnt;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_date(request *req, char *data, char *arg)
{
	time_t tnow;
	struct tm *tmnow;

	tzset();
	time(&tnow);
	tmnow = localtime(&tnow);

	if ( *arg == '0' ) {
		return sprintf(data, "%d", tmnow->tm_year+1900);
	}
	else if ( *arg == '1' ) {
		return sprintf(data, "%d", tmnow->tm_mon+1);
	}
	else if ( *arg == '2' ) {
		return sprintf(data, "%d", tmnow->tm_mday);
	}	
	else if ( *arg == '\0' )
		return sprintf(data, "%04d/%02d/%02d", tmnow->tm_year+1900, tmnow->tm_mon+1, tmnow->tm_mday);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_time(request *req, char *data, char *arg)
{
	time_t tnow;
	struct tm *tmnow;

	tzset();
	time(&tnow);
	tmnow = localtime(&tnow);

	if ( *arg == '0' ) {
		return sprintf(data, "%d", tmnow->tm_hour);
	}
	else if ( *arg == '1' ) {
		return sprintf(data, "%d", tmnow->tm_min);
	}
	else if ( *arg == '2' ) {
		return sprintf(data, "%d", tmnow->tm_sec);
	}
	else if ( *arg == '\0' )
		return sprintf(data, "%02d:%02d:%02d", tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_week(request *req, char *data, char *arg)  // add by Alex, 2009.03.27
{
	time_t tnow;
	struct tm *tmnow;

	tzset();
	time(&tnow);
	tmnow = localtime(&tnow);

	return sprintf(data, "%d", tmnow->tm_wday);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sntpip(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", conf_string_read(NTP_SERVER));
	//return sprintf(data, "%s", sys_info->config.lan_config.net.ntp_server);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sntpipname(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
    if(*arg == '0')
		return sprintf(data, "%s", "ntp1.dlink.com");
	else if(*arg == '1')
		return sprintf(data, "%s", "ntp.dlink.com.tw");
	else if(*arg == 'a' || *arg == '\0')
		return sprintf(data, "%s", ";ntp1.dlink.com;ntp.dlink.com.tw");
	return -1;
#else
	return sprintf(data, "");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sntptimezone(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[NTP_TIMEZONE].config.u8);
	//return sprintf(data, "%d", sys_info->config.lan_config.net.ntp_timezone);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sntpfrequency(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[NTP_FREQUENCY].config.s8);
	//return sprintf(data, "%d", sys_info->config.lan_config.net.ntp_frequency);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dhcpenable(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[NET_DHCP_ENABLE].config.value);
	//return sprintf(data, "%d", sys_info->config.lan_config.net.dhcp_enable);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpport(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[HTTP_PORT].config.u16);
	//return sprintf(data, "%d", sys_info->config.lan_config.net.http_port);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_defaulthttpport(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", HTTP_PORT_DEFAULT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_gateway(request *req, char *data, char *arg)
{
	NET_IPV4 gateway;
	gateway.int32 = net_get_gateway(sys_info->ifname);

	if ( *arg == '0' )
		return sprintf(data, "%d", gateway.str[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%d", gateway.str[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%d", gateway.str[2]);
	else if ( *arg == '3' )
		return sprintf(data, "%d", gateway.str[3]);
	else if ( *arg == '\0' )
		return sprintf(data, "%d.%d.%d.%d", gateway.str[0], gateway.str[1], gateway.str[2], gateway.str[3]);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_netmask(request *req, char *data, char *arg)
{
	NET_IPV4 mask;
	mask.int32 = net_get_netmask(sys_info->ifname); //kevinlee by check eth0 or ppp0

	if ( *arg == '0' )
		return sprintf(data, "%d", mask.str[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%d", mask.str[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%d", mask.str[2]);
	else if ( *arg == '3' )
		return sprintf(data, "%d", mask.str[3]);
	else if ( *arg == '\0' )
		return sprintf(data, "%d.%d.%d.%d", mask.str[0], mask.str[1], mask.str[2], mask.str[3]);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_netmac(request *req, char *data, char *arg)
{
	__u8 mac[MAC_LENGTH];

	net_get_hwaddr(sys_info->ifether, mac);

	return sprintf(data, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_netip(request *req, char *data, char *arg)
{
	NET_IPV4 ip;
	ip.int32 = net_get_ifaddr(sys_info->ifname); //kevinlee by check eth0 or ppp0

	if ( *arg == '0' )
		return sprintf(data, "%d", ip.str[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%d", ip.str[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%d", ip.str[2]);
	else if ( *arg == '3' )
		return sprintf(data, "%d", ip.str[3]);
	else if ( *arg == '\0' )
		return sprintf(data, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dnsip(request *req, char *data, char *arg)
{
	NET_IPV4 dns;
	dns.int32 = net_get_dns();

	if ( *arg == '0' )
		return sprintf(data, "%d", dns.str[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%d", dns.str[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%d", dns.str[2]);
	else if ( *arg == '3' )
		return sprintf(data, "%d", dns.str[3]);
	else if ( *arg == '\0' )
		return sprintf(data, "%d.%d.%d.%d", dns.str[0], dns.str[1], dns.str[2], dns.str[3]);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_title(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.title);
	return sprintf(data, "%s", conf_string_read(TITLE));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_http_streamname1(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[0].httpstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM0_HTTPSTREAMNAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_http_streamname2(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[1].httpstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM1_HTTPSTREAMNAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_http_streamname3(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[2].httpstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM2_HTTPSTREAMNAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_http_streamname4(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[3].httpstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM3_HTTPSTREAMNAME));
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rtsp_streamname1(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[0].rtspstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM0_RTSPSTREAMNAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rtsp_streamname2(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[1].rtspstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM1_RTSPSTREAMNAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rtsp_streamname3(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[2].rtspstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM2_RTSPSTREAMNAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rtsp_streamname4(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.stream[3].rtspstreamname);
	return sprintf(data, "%s", conf_string_read(STREAM3_RTSPSTREAMNAME));
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxtitlelen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_LANCAM_TITLE_LEN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_mpeg4resolution(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.net.mpeg4resolution);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_liveresolution(request *req, char *data, char *arg)
{
	//dm270
	//return sprintf(data, "%d", sys_info->config.lan_config.net.liveresolution);
	return sprintf(data, "%d", sys_info->ipcam[LIVERESOLUTION].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_mpeg4quality(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.net.mpeg4quality);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_livequality(request *req, char *data, char *arg)
{
	//dm270
	//return sprintf(data, "%d", sys_info->config.profile_config[0].jpeg_quality);
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_livequality2(request *req, char *data, char *arg)
{
	//dm270
	//return sprintf(data, "%d", sys_info->config.profile_config[1].jpeg_quality);
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG1_JPEG_QUALITY].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxfqdnlen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_FQDN_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minnetport(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_SERVER_PORT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxnetport(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_PORT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_ftpip(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.ftp_config.servier_ip);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ftppiport(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.ftp_config.port);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ftpuser(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.ftp_config.username);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ftppassword(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.ftp_config.password);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ftppapath(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.ftp_config.foldername);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxftpuserlen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_STRING_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxftppwdlen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_STRING_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxftppathlen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_STRING_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_smtpip(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.smtp_config.servier_ip);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smtpauth(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.smtp_config.authentication);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smtpport(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.smtp_config.port);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smtpuser(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.smtp_config.username);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxsmtpuser(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_STRING_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_smtppwd(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.smtp_config.password);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxsmtppwd(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_STRING_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_smtpsender(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.smtp_config.sender_email);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxsmtpsender(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_STRING_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_emailuser(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", sys_info->config.smtp_config.receiver_email);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxemailuserlen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_STRING_LENGTH);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmpeg4(request *req, char *data, char *arg)
{
#ifdef SUPPORT_MPEG4
	return sprintf(data, "1");
#else
	return sprintf(data, "1");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_format(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.imageformat);
	return sprintf(data, "%d", sys_info->ipcam[IMAGEFORMAT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_imagesource(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->video_type);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_devicename(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.devicename);
	return sprintf(data, "%s", conf_string_read(DEVICENAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
int para_defaultstorage(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.defaultstorage);
	return sprintf(data, "%d", sys_info->ipcam[DEFAULTSTORAGE].config.u8);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sddisplay(request *req, char *data, char *arg)
{
#ifdef SUPPORT_SDDISPLAY
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdinsert(request *req, char *data, char *arg)
{
	//dbg("%d", (sys_info->status.mmc & SD_INSERTED) ? 1 : 0);
	return sprintf(data, "%d", (sys_info->status.mmc & SD_INSERTED) ? 1 : 0);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdformat(request *req, char *data, char *arg)
{
	//dbg("%d", (sys_info->status.mmc & SD_MOUNTED) ? 1 : 0);
	return sprintf(data, "%d", (sys_info->status.mmc & SD_MOUNTED) ? 1 : 0);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdprotect(request *req, char *data, char *arg)
{
	//dbg("%d", (sys_info->status.mmc & SD_LOCK) ? 1 : 0);
	return sprintf(data, "%d", (sys_info->status.mmc & SD_LOCK) ? 1 : 0);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
//sdlist add by jiahung, 20090522
extern FILELIST *pFilelist;
extern FILELIST *pFloderlist;
extern int filenum;
extern int foldernum;
extern char indir[256];
extern int pageindex;
extern int	totalpage;
extern int	sdfpp;

/*****/
extern int totalfolderpage;
extern int folderpage;
extern int foldernum_lastpage;
/*****/
extern int totalfilepage;
extern int numoffirstpage;
extern int filepage;
extern int numoflastpage;
extern int filestart;
/*****/
#if 1//defined (MODEL_LC7315)
int para_sdfolderlist(request *req, char *data, char *arg)
{
	char FILE[256];
	int i, offset;
	char *pdata = data;
	__u32 ret=0;
	//dbg("sdfolder_display=%d\n", sys_info->sd_status.sdfolder_display);
	for(i = 0 ; i <= (sys_info->sd_status.sdfolder_display - 1) ; i++){
		offset = 0;
#if 1		
		sprintf(FILE, "%s;%d;%d;null;%d;%d;%d;%ld;%d", sys_info->sdfolder[i].name, sys_info->sdfolder[i].filetype, sys_info->sdfolder[i].eventtype, 
				 sys_info->sdfolder[i].numoffiles, sys_info->sdfolder[i].property, sys_info->sdfolder[i].authority, sys_info->sdfolder[i].time, sys_info->sdfolder[i].reserve);
#else
		sprintf(FILE, "%s;%d;%d;-;%d;%d;%d;%ld;%d", sys_info->sdfolder[i].name, sys_info->sdfolder[i].filetype, sys_info->sdfolder[i].eventtype, 
				 sys_info->sdfolder[i].numoffiles, sys_info->sdfolder[i].property, sys_info->sdfolder[i].authority, sys_info->sdfolder[i].time, sys_info->sdfolder[i].reserve);
#endif
		//dbg("FILE=%s\n", FILE);
	
		if(i < (sys_info->sd_status.sdfolder_display - 1) )
				strcat(FILE, ",");
		offset += sprintf(pdata, "%s", FILE);
		ret += offset;
		pdata += offset;

	}
	//dbg("ret=%d\n", ret);
	return ret;
	//return sprintf(data, "null");
}
int para_sdfilelist(request *req, char *data, char *arg)
{
	__u32 ret = 0;
	int i, offect;
	char FILE[256];
	FILELIST *ptr = pFilelist;
	char *pdata = data;
	int displaynum;
	int cnt = 0;
	
	//if(filenum == 0 )
	//	return sprintf(data, "null");
	
	cnt = sys_info->sd_status.pageindex * sdfpp;

	if(sys_info->sd_status.pageindex ==  filestart){
		//dbg("frist\n");	
		if(sdfpp > numoffirstpage )
			displaynum = numoffirstpage;
		else
			displaynum = sdfpp;
	}else if(sys_info->sd_status.pageindex ==  totalpage){
		//dbg("last\n");
		//displaynum = numoflastpage;
		if(numoflastpage == 0){
			displaynum = sdfpp;
			ptr =  ptr + numoffirstpage + (filepage-1) * sdfpp;
		}else{
			displaynum = numoflastpage;
			ptr =  ptr + numoffirstpage + filepage * sdfpp;
		}
	}else if(sys_info->sd_status.pageindex < filestart || ((numoffirstpage == sdfpp) && (filestart == sys_info->sd_status.pageindex))){
		//dbg("no page\n");
		displaynum = 0;
	}else{
		//dbg("other\n");
		//if(foldernum == 0)
			//ptr =  ptr + numoffirstpage + (sys_info->sd_status.pageindex - 1) * sdfpp;
		//else
			ptr =  ptr + numoffirstpage + (sys_info->sd_status.pageindex - filestart - 1) * sdfpp;
		displaynum = sdfpp;
	}
	//dbg("totalfilepage = %d\n", totalfilepage);
	//dbg("numoffirstpage = %d\n", numoffirstpage);
	//dbg("filepage = %d\n", filepage);
	//dbg("numoflastpage = %d\n", numoflastpage);
	//dbg("filestart = %d\n", filestart);
	//dbg("displaynum = %d\n", displaynum);
	//dbg("ptr = %p\n", ptr);
	
	for(i = 0 ; i <= (sys_info->sd_status.sdfile_display - 1) ; i++){
			offect = 0;
#if 1
			sprintf(FILE, "%s;%d;%d;%d;%d;%d;%d;%ld;%d", sys_info->sdfile[i].name, sys_info->sdfile[i].filetype, 
				sys_info->sdfile[i].eventtype, sys_info->sdfile[i].size, sys_info->sdfile[i].numoffiles, sys_info->sdfile[i].property, sys_info->sdfile[i].authority, sys_info->sdfile[i].time, sys_info->sdfile[i].reserve);
#else
			sprintf(FILE, "%s;%d;%d;%d%s;%d;%d;%d;%ld;%d", sys_info->sdfile[i].name, sys_info->sdfile[i].filetype, 
				sys_info->sdfile[i].eventtype, sys_info->sdfile[i].size , sys_info->sdfile[i].unit, sys_info->sdfile[i].numoffiles, sys_info->sdfile[i].property, sys_info->sdfile[i].authority, sys_info->sdfile[i].time, sys_info->sdfile[i].reserve);
#endif		
			//dbg("FILE = %s\n", FILE);
			if(i < (sys_info->sd_status.sdfile_display - 1) )
				strcat(FILE, ",");
			offect += sprintf(pdata, "%s", FILE);
			ret += offect;
			pdata += offect;
	}
	//if(displaynum <= 0)
	//	return sprintf(data, "null");
	return ret;
}
#else
int para_sdfolderlist(request *req, char *data, char *arg)
{
	__u32 ret = 0;
	int i, offect;
	char FILE[128];
	FILELIST *ptrdir = pFloderlist;
	char *pdata = data;
	int displaynum;
	int cnt = 0;
	//int total = 0;
	if(foldernum == 0 )
		return sprintf(data, "null");

	//dbg("pageindex = %d\n", pageindex);
	cnt = pageindex * sdfpp;
	ptrdir += (pageindex - 1) * sdfpp;
	//dbg("cnt = %d\n", cnt);
	
	if(pageindex == 1){
		dbg("frist\n");	
		if(sdfpp > foldernum )
			displaynum = foldernum;
		else
			displaynum = sdfpp;
	}else if(pageindex ==  totalfolderpage){
		dbg("last\n");
		if(foldernum_lastpage >= sdfpp)
			displaynum = sdfpp;
		else{
			displaynum = foldernum_lastpage;
			if(foldernum_lastpage == 0)
				displaynum = sdfpp;

		}	
	}else if(pageindex > totalfolderpage){
		dbg("no page\n");
		displaynum = 0;
	}else{
		dbg("other\n");
		displaynum = sdfpp;
	}
	//dbg("totalfolderpage = %d\n", totalfolderpage);
	//dbg("folderpage = %d\n", folderpage);
	//dbg("foldernum_lastpage = %d\n", foldernum_lastpage);
	
/*	
	if(foldernum > cnt && pageindex == 1){
		dbg("frist\n");
		displaynum = sdfpp;
		if(displaynum > foldernum )
			displaynum = foldernum;
	}else if((foldernum > cnt && pageindex != 1) || foldernum == cnt){
		dbg("other\n");
		displaynum = sdfpp;
	}else if(foldernum < cnt && pageindex == (folderpage + 1)){
		dbg("last\n");
		displaynum = foldernum_lastpage;
	}else
		displaynum = 0;
*/	
	for(i = 0 ; i <= (displaynum - 1) ; i++){
			offect = 0;
			sprintf(FILE, "%s;%d;%d;%d;%d;%d;%d;%ld;%d", ptrdir->name, ptrdir->filetype, ptrdir->eventtype, ptrdir->size, ptrdir->numoffiles, ptrdir->property, ptrdir->authority, ptrdir->time, ptrdir->reserve);
			dbg("FLODER = %s\n", FILE);
			if(i < (foldernum - 1) )
				strcat(FILE, ",");
			offect += sprintf(pdata, "%s", FILE);
			ret += offect;
			ptrdir ++;
			pdata += offect;
	}

	return ret;
}


int para_sdfilelist(request *req, char *data, char *arg)
{
	__u32 ret = 0;
	int i, offect;
	char FILE[128];
	FILELIST *ptr = pFilelist;
	char *pdata = data;
	int displaynum;
	int cnt = 0;
	
	if(filenum == 0 )
		return sprintf(data, "null");
	//dbg("pageindex = %d\n", pageindex);
	cnt = pageindex * sdfpp;
	//ptr += (pageindex - 1) * sdfpp;
	//dbg("cnt = %d\n", cnt);
	
	if(pageindex ==  filestart){
		dbg("frist\n");	
		if(sdfpp > numoffirstpage )
			displaynum = numoffirstpage;
		else
			displaynum = sdfpp;
	}else if(pageindex ==  totalpage){
		dbg("last\n");
		//displaynum = numoflastpage;
		if(numoflastpage == 0){
			displaynum = sdfpp;
			ptr =  ptr + numoffirstpage + (filepage-1) * sdfpp;
		}else{
			displaynum = numoflastpage;
			ptr =  ptr + numoffirstpage + filepage * sdfpp;
		}
	}else if(pageindex < filestart || ((numoffirstpage == sdfpp) && (filestart == pageindex))){
		dbg("no page\n");
		displaynum = 0;
	}else{
		dbg("other\n");
		//if(foldernum == 0)
			//ptr =  ptr + numoffirstpage + (pageindex - 1) * sdfpp;
		//else
			ptr =  ptr + numoffirstpage + (pageindex - filestart - 1) * sdfpp;
		displaynum = sdfpp;
	}
	//dbg("totalfilepage = %d\n", totalfilepage);
	//dbg("numoffirstpage = %d\n", numoffirstpage);
	//dbg("filepage = %d\n", filepage);
	//dbg("numoflastpage = %d\n", numoflastpage);
	//dbg("filestart = %d\n", filestart);
	//dbg("displaynum = %d\n", displaynum);
	//dbg("ptr = %p\n", ptr);

	for(i = 0 ; i <= (displaynum - 1) ; i++){
			offect = 0;
			sprintf(FILE, "%s;%d;%d;%d;%d;%d;%d;%ld;%d", ptr->name, ptr->filetype, ptr->eventtype, ptr->size >> 10, ptr->numoffiles, ptr->property, ptr->authority, ptr->time, ptr->reserve);
			dbg("FILE = %s\n", FILE);
			if(i < (displaynum - 1) )
				strcat(FILE, ",");
			offect += sprintf(pdata, "%s", FILE);
			ret += offect;
			ptr ++;
			pdata += offect;
	}
	if(displaynum <= 0)
		return sprintf(data, "null");
	return ret;
}
#endif
int para_sdthispath(request *req, char *data, char *arg)
{
	//dbg("para_sdthispath = %s\n", indir);
#if 1//defined (MODEL_LC7315)
	return sprintf(data, "%s", sys_info->sd_status.indir);
#else
	return sprintf(data, "%s", indir);
#endif
	
}

int para_sdfpp(request *req, char *data, char *arg)
{
	//dbg("sdfpp = %d\n", sys_info->config.sdcard.sdfpp);
#if 1//defined (MODEL_LC7315)
	return sprintf(data, "%d", sys_info->ipcam[SDCARD_SDFPP].config.u8);
	//return sprintf(data, "%d", sys_info->config.sdcard.sdfpp);
#else
	return sprintf(data, "%d", sdfpp);
#endif
}

int para_sdfppname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "10");
	if ( *arg == '1' )
		return sprintf(data, "20");
	if ( *arg == '2' )
		return sprintf(data, "50");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "10;20;50");
	return -1;
}

int para_sdthispage(request *req, char *data, char *arg)
{
	//dbg("sdthispage = %d\n", sys_info->sd_status.pageindex);
#if 1//defined (MODEL_LC7315)
	return sprintf(data, "%d", sys_info->sd_status.pageindex);
#else
	return sprintf(data, "%d", pageindex);
#endif
}
int para_sdtotalpage(request *req, char *data, char *arg)
{
	//dbg("filenum = %d\n", sys_info->filenum);
#if 1//defined (MODEL_LC7315)
	return sprintf(data, "%d", sys_info->sd_status.totalpage);
#else
	return sprintf(data, "%d", filenum);
#endif
}

/*
int para_sdthisfilenum(request *req, char *data, char *arg)
{
	//dbg("filenum = %d\n", sys_info->filenum);
#if 1//defined (MODEL_LC7315)
	return sprintf(data, "%d", sys_info->sd_status.filenum);
#else
	return sprintf(data, "%d", filenum);
#endif
}

int para_sdthisfoldernum(request *req, char *data, char *arg)
{
	//dbg("foldernum = %d\n", sys_info->foldernum);
#if 1//defined (MODEL_LC7315)	
	return sprintf(data, "%d", sys_info->sd_status.foldernum);
#else
	return sprintf(data, "%d", foldernum);
#endif
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdlock(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 1);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cfinsert(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 0);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_defaultcardgethtm(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.net.defaultcardgethtm);
	return sprintf(data, "%s", DEFAULTCARDGETHTM_DEFAULT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_brandurl(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", sys_info->oem_data.brand_url);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_brandname(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", sys_info->oem_data.brand_name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_brandpath(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", sys_info->oem_data.brand_webdir);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supporttstamp(request *req, char *data, char *arg)
{
#ifdef SUPPORT_TSTAMP
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supporttstampcolor(request *req, char *data, char *arg)
{
#if defined ( PLAT_DM365 )
	return sprintf(data, "%d", 0);
#elif defined ( PLAT_DM355 )
	return sprintf(data, "%d", 1);		
#else
#error Unkown platform	
#endif
}

#if 0
int para_mpeg4xsize(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.Mpeg41.xsize);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg4ysize(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.Mpeg41.ysize);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_profile1xsize(request *req, char *data, char *arg)
{
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE0_XSIZE].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_profile1ysize(request *req, char *data, char *arg)
{
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	
	return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE0_YSIZE].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_socketauthority(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", req->authority);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_authoritychange(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 1);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supporttwowayaudio(request *req, char *data, char *arg)
{
#ifdef SUPPORT_TWOWAY_AUDIO
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_speaktoken(request *req, char *data, char *arg)
{
	if ( sys_info->speaktokenid == atoi(arg) )
		return sprintf(data, "%d", sys_info->speaktokentime);
	else
		return sprintf(data, "%d", 0);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmotion(request *req, char *data, char *arg)
{
#ifdef SUPPORT_MOTION
		return sprintf(data, "1");
#else
		return sprintf(data, "0");
#endif
}
int para_supportmotionmode(request *req, char *data, char *arg)
{
	/* 0:old motion, block type */
	/* 1:new motion, window type */
	return sprintf(data, "%d", SUPPORT_MOTIONMODE);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportwireless(request *req, char *data, char *arg)
{
#ifdef SUPPORT_WIRELESS
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_serviceftpclient(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", sys_info->video_type ? 1 : 0);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_servicesmtpclient(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", sys_info->video_type ? 1 : 0);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_servicepppoe(request *req, char *data, char *arg)
{
#ifdef SERVICE_PPPOE
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_servicesntpclient(request *req, char *data, char *arg)
{
#ifdef SERVICE_SNTPCLIENT
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_serviceddnsclient(request *req, char *data, char *arg)
{
#ifdef SERVICE_DDNSCLIENT
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmaskarea(request *req, char *data, char *arg)
{
#ifdef SUPPORT_MASKAREA
	return sprintf( data, "1" );
#else
	return sprintf( data, "0" );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportlinein(request *req, char *data, char *arg)
{
#if defined( SUPPORT_LINEIN )
	return sprintf( data, "1" );
#else
	return sprintf( data, "0" );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmic(request *req, char *data, char *arg)
{
#if defined ( SUPPORT_MICROPHONE )
	return sprintf( data, "1" );
#else
	return sprintf( data, "0" );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_servicevirtualclient(request *req, char *data, char *arg)
{
#ifdef SERVICE_VIRTUALSERVER
		return sprintf( data, "1" );
#else
		return sprintf( data, "0" );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxmaskarea(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.net.maxmaskarea);
	return sprintf(data, "%d", MAXMASKAREA_DEFAULT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_privacymaskenable(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.net.maskareaenable);
	return sprintf(data, "%d", sys_info->ipcam[MASKAREAENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maskareaxlimit(request *req, char *data, char *arg)
{
	int value;
	//value = sys_info->config.lan_config.profilesize[0].xsize;
	value = sys_info->ipcam[PROFILESIZE0_XSIZE].config.value;
	return sprintf(data, "%d", value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maskareaylimit(request *req, char *data, char *arg)
{
	int value;
	//value = sys_info->config.lan_config.profilesize[0].ysize;
	value = sys_info->ipcam[PROFILESIZE0_YSIZE].config.value;
	return sprintf(data, "%d", value);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int para_maskarea(request *req, char *data, char *arg)
{
	/*if ( *arg == '0' )
		return sprintf(data, "%s",sys_info->config.lan_config.net.maskarea[0].maskarea_cmd);
	else if ( *arg == '1' )
		return sprintf(data, "%s",sys_info->config.lan_config.net.maskarea[1].maskarea_cmd);
	else if ( *arg == '2' )
		return sprintf(data, "%s",sys_info->config.lan_config.net.maskarea[2].maskarea_cmd);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s",sys_info->config.lan_config.net.maskarea[0].maskarea_cmd,sys_info->config.lan_config.net.maskarea[1].maskarea_cmd,sys_info->config.lan_config.net.maskarea[2].maskarea_cmd);
	return -1;*/
	if ( *arg == '0' )
		return sprintf(data, "%s",conf_string_read(MASKAREA0_MASKAREA_CMD));
	else if ( *arg == '1' )
		return sprintf(data, "%s",conf_string_read(MASKAREA1_MASKAREA_CMD));
	else if ( *arg == '2' )
		return sprintf(data, "%s",conf_string_read(MASKAREA2_MASKAREA_CMD));
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s",conf_string_read(MASKAREA0_MASKAREA_CMD),conf_string_read(MASKAREA1_MASKAREA_CMD),conf_string_read(MASKAREA2_MASKAREA_CMD));
	return -1;
	//return sprintf(data, "%s", sys_info->config.lan_config.net.maskarea[0].maskarea_cmd);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_machinecode(request *req, char *data, char *arg)
{
	int sub_code = get_submcode_from_kernel();

	if (sub_code <= 0)
		return sprintf(data, "%04X", MACHINE_CODE);
	return sprintf(data, "%04X%04X", MACHINE_CODE, sub_code);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxchannel(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.maxchannel);
	return sprintf(data, "%d", MAXCHANNEL_DEFAULT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportrs485(request *req, char *data, char *arg)
{
#ifdef SUPPORT_RS485
    return sprintf(data, "1");
#else
    return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_485enable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.rs485.enable);
	return sprintf(data, "%d", sys_info->ipcam[RS485_ENABLE].config.u8);
}
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_485protocol(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.rs485.protocol);
	return sprintf(data, "%d", sys_info->ipcam[RS485_PROTOCOL].config.u8);
}
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_485protocolname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Pelco-D");
	else if ( *arg == '1' )
		return sprintf(data, "Pelco-P");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Pelco-D;Pelco-P");
	return -1;
}
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_485data(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.rs485.data);
	return sprintf(data, "%d", sys_info->ipcam[RS485_DATA].config.u8);
}
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_485stop(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.rs485.stop);
	return sprintf(data, "%d", sys_info->ipcam[RS485_STOP].config.u8);
}
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_485parity(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.rs485.parity);
	return sprintf(data, "%d", sys_info->ipcam[RS485_PARITY].config.u8);
}

#ifdef CONFIG_BRAND_DLINK
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_uartspeedname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "2400 bps");
	else if ( *arg == '1' )
		return sprintf(data, "4800 bps");
	else if ( *arg == '2' )
		return sprintf(data, "9600 bps");
	else if ( *arg == '3' )
		return sprintf(data, "19200 bps");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "2400 bps;4800 bps;9600 bps;19200 bps");
	return -1;

}
#else
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_uartspeedname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "2400 bps");
	else if ( *arg == '1' )
		return sprintf(data, "4800 bps");
	else if ( *arg == '2' )
		return sprintf(data, "9600 bps");
	else if ( *arg == '3' )
		return sprintf(data, "19200 bps");
	else if ( *arg == '4' )
		return sprintf(data, "38400 bps");
	else if ( *arg == '5' )
		return sprintf(data, "57600 bps");
	else if ( *arg == '6' )
		return sprintf(data, "115200 bps");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "2400 bps;4800 bps;9600 bps;19200 bps; \
			38400 bps;57600 bps;115200 bps");
	return -1;

}
#endif

/***************************************************************************
*                                                                         *
***************************************************************************/ 
int para_485type(request *req, char *data, char *arg)
{
	//if(sys_info->config.rs485.data == 1) {  // 8 bits
		//switch(sys_info->config.rs485.parity) {
	if(sys_info->ipcam[RS485_DATA].config.u8 == 1) {  // 8 bits
		switch(sys_info->ipcam[RS485_PARITY].config.u8) {

			case 0:  return sprintf(data, "%d", 0);  // 8-N-1
			case 1:  return sprintf(data, "%d", 1);  // 8-E-1
			case 2:  return sprintf(data, "%d", 2);  // 8-O-1
			default: return -1;
		}
	}
	//else if(sys_info->config.rs485.data == 0) {  // 7 bits
		//switch(sys_info->config.rs485.parity) {
	else if(sys_info->ipcam[RS485_DATA].config.u8 == 0){  // 7 bits
		switch(sys_info->ipcam[RS485_PARITY].config.u8) {

			case 0:  return sprintf(data, "%d", 3);  // 7-N-1
			case 1:  return sprintf(data, "%d", 4);  // 7-E-1
			case 2:  return sprintf(data, "%d", 5);  // 7-O-1
			default: return -1;
		}	
	}
	return -1;
	
}
/***************************************************************************
*                                                                         *
***************************************************************************/
int para_uarttypename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "8-N-1");
	else if ( *arg == '1' )
		return sprintf(data, "8-E-1");
	else if ( *arg == '2' )
		return sprintf(data, "8-O-1");
	else if ( *arg == '3' )
		return sprintf(data, "7-N-1");
	else if ( *arg == '4' )
		return sprintf(data, "7-E-1");
	else if ( *arg == '5' )
		return sprintf(data, "7-O-1");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "8-N-1;8-E-1;8-O-1;7-N-1;7-E-1;7-O-1");
	return -1;

}  
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_485id(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.rs485.id);
	return sprintf(data, "%d", sys_info->ipcam[RS485_ID].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_485speed(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.rs485.speed);
	return sprintf(data, "%d", sys_info->ipcam[RS485_SPEED].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if defined (SUPPORT_RS485) && defined(CONFIG_BRAND_DLINK)
int para_ptzpresetname(request *req, char *data, char *arg)
{
	char *ptr = data;
	__u32 cnt = 0, ret = 0;
	int i, num=0;
	
	if(isdigit(*arg)) {
		//return sprintf(ptr, "%s", sys_info->config.ptz_config.presetposition[(int)*arg].name);
		return sprintf(ptr, "%s", conf_string_read(PRESETPOSITION0_NAME+(int)*arg));
	}
	else if ( *arg == 'a' || *arg == '\0' ) {
		
		for( i = 0 ; i < MAX_PTZPRESET_NUM; i++) {
			cnt = 0;
			//if(sys_info->config.ptz_config.presetposition[i].index != 0){	
			if(sys_info->ipcam[PRESETPOSITION0_INDEX+i].config.value != 0){	
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				
				//cnt += sprintf(ptr, "%s", sys_info->config.ptz_config.presetposition[i].name);
				cnt += sprintf(ptr, "%s", conf_string_read(PRESETPOSITION0_NAME+i));
				//dbg("ptr=<%d>%s\n",i ,ptr);
				ptr+= cnt;
				ret += cnt;
			}
		}
	}
	//for(i = 0 ; i < 10; i++)
	//	dbg("[%d] = %d : %s\n", i, sys_info->config.ptz_config.presetposition[i].index, sys_info->config.ptz_config.presetposition[i].name);
	return ret;
}
int para_ptzpresetindex(request *req, char *data, char *arg)
{
	char *ptr = data;
	__u32 cnt = 0, ret = 0;
	int i, num=0;

	if(isdigit(*arg)) {
		//return sprintf(ptr, "%d", sys_info->config.ptz_config.presetposition[(int)*arg].index);
		return sprintf(ptr, "%d", sys_info->ipcam[PRESETPOSITION0_INDEX+(int)*arg].config.value);
	}
	else if ( *arg == 'a' || *arg == '\0' ) {
		for( i = 0 ; i < MAX_PTZPRESET_NUM; i++) {
			cnt = 0;
			//if(sys_info->config.ptz_config.presetposition[i].index != 0){			
			if(sys_info->ipcam[PRESETPOSITION0_INDEX+i].config.value != 0){			
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				//cnt += sprintf(ptr, "%d", sys_info->config.ptz_config.presetposition[i].index);
				cnt += sprintf(ptr, "%d", sys_info->ipcam[PRESETPOSITION0_INDEX+i].config.value);
				//dbg("ptr=<%d>%d\n",i ,sys_info->config.ptz_config.presetposition[i].index);
				//dbg(">>>>ptr=<%d>%s\n",i ,ptr);
				ptr += cnt;
				ret += cnt;
			}
		}
	}

	return ret;
}
int para_maxptzpresetnum(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_PTZPRESET_NUM);
}
int para_ptzpresetnamelen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_PTZPRESETNAME_LEN);
}
#endif // SUPPORT_RS485 && CONFIG_BRAND_DLINK
int para_supportptzzoom(request *req, char *data, char *arg)
{
#ifdef SUPPORT_PTZZOOM
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", 0);
#endif    
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportfan(request *req, char *data, char *arg)
{
#ifdef SUPPORT_FAN_CONTROL
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportrs232(request *req, char *data, char *arg)
{
#ifdef SUPPORT_RS232
		return sprintf(data, "1");
#else
		return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_ftppath(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.ftp_config.foldername);
}
#endif

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_layoutnum(request *req, char *data, char *arg)
{

	/*if ( *arg == '0' )
		return sprintf(data, "1");*/
	//return sprintf(data, "%d", sys_info->config.lan_config.net.layoutnum);
	return sprintf(data, "%d", sys_info->ipcam[LAYOUTNUM].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmui(request *req, char *data, char *arg)
{
	BIOS_DATA bios;
	bios_data_read(&bios);
	int brand;

	//dbg("get_brand_id() = %d", get_brand_id());
	brand = get_brand_id();
	if(brand == BRAND_ID_AVUE)
		return sprintf(data, "%d", 3271);
	else	
		return sprintf(data, "%d", SUPPORT_MUI);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_support_tstamp_mui(request *req, char *data, char *arg)
{	
#ifdef SUPPORT_TSTAMP_MUI
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mui(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.net.mui);
	return sprintf(data, "%d", sys_info->ipcam[MUI].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportsequence(request *req, char *data, char *arg)
{
#ifdef SUPPORT_SEQUENCE
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_quadmodeselect(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.net.quadmodeselect);
	return sprintf(data, "%d", QUADMODESELECT_DEFAULT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_serviceipfilter(request *req, char *data, char *arg)
{
#ifdef SERVICE_IPFILTER
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_oemflag0(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.net.oemflag0);
	return sprintf(data, "%d", OEMFLAG0_DEFAULT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportdncontrol(request *req, char *data, char *arg)
{
#ifdef SUPPORT_DNCONTROL
	return sprintf(data, "1");
#elif defined (MODEL_VS2415)
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportavc(request *req, char *data, char *arg)
{
#ifdef SUPPORT_AVC
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportaudio(request *req, char *data, char *arg)
{

#ifdef SUPPORT_AUDIO
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportaudioinswitch(request *req, char *data, char *arg)
{
#ifdef SUPPORT_AUDIO_SWITCH
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}

int para_supportaudioencselect(request *req, char *data, char *arg)
{
#if defined(MODEL_VS2415)
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}

int para_audioinsource(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.audioinsourceswitch);
	return sprintf(data, "%d", sys_info->ipcam[AUDIOINSOURCESWITCH].config.u8);
}
int para_audiotype(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.audio_info.codec_name);
	return sprintf(data, "%s", conf_string_read(AUDIO_INFO_CODEC_NAME));
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportptzpage(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.net.supportptzpage);
	//return sprintf(data, "%d", sys_info->ipcam[SUPPORTPTZPAGE].config.u8);
	return sprintf(data, "%d", SUPPORT_PTZPAGE);
}
int para_ptziconvisible(request *req, char *data, char *arg)
{

	return sprintf(data, "%s", "0x004001C4");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supporteptz(request *req, char *data, char *arg)
{
#if defined ( SUPPORT_EPTZ)
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dhcpconfig(request *req, char *data, char *arg)
{
#if defined ( DHCP_CONFIG )
	return sprintf(data, "1");
#else
	return sprintf(data, "%s", sys_info->ipcam[DHCPDIPSWITCH].config.value?"0":"1");
#endif
}
int para_dhcpdipswitch(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[DHCPDIPSWITCH].config.value);
} 
int para_supportdhcpdipswitch(request *req, char *data, char *arg)
{
#ifndef DHCP_CONFIG
	#ifdef CONFIG_BRAND_TYCO
		return sprintf(data, "0");
	#else
		return sprintf(data, "1");
	#endif
#else
	return sprintf(data, "0");
#endif
} 

int para_wizardinit(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[WIZARD_INIT].config.value);
}

int para_audioinenable(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
	return sprintf(data, "%d", !sys_info->ipcam[AUDIOINENABLE].config.u8);
#else
	return sprintf(data, "%d", sys_info->ipcam[AUDIOINENABLE].config.u8);
#endif
}

int para_audioamplifyratio(request *req, char *data, char *arg)
{
	return sprintf(data, "%f", sys_info->audioamplifyratio);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_audiooutenable(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
		return sprintf(data, "%d", !sys_info->ipcam[AUDIOOUTENABLE].config.u8);
#else
		return sprintf(data, "%d", sys_info->ipcam[AUDIOOUTENABLE].config.u8);
#endif

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_muteenable(request *req, char *data, char *arg)
{
	//int value = sys_info->config.lan_config.audioinenable ? 0 : 1;
	int value = sys_info->ipcam[AUDIOINENABLE].config.u8 ? 0 : 1;
	return sprintf(data, "%d", value);
}

int para_lineingain(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.lineininputgain);
	return sprintf(data, "%s", conf_string_read(LINEININPUTGAIN));
}

int para_lineingainname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, AUDIOINGAIN_1);
	else if ( *arg == '1' )
		return sprintf(data, AUDIOINGAIN_2);
	else if ( *arg == '2' )
		return sprintf(data, AUDIOINGAIN_3);
	else if ( *arg == '3' )
		return sprintf(data, AUDIOINGAIN_4);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s",AUDIOINGAIN_1,AUDIOINGAIN_2,AUDIOINGAIN_3,AUDIOINGAIN_4);
	return -1;
}

int para_externalmicgain(request *req, char *data, char *arg)
{
	return sprintf(data, "%s",conf_string_read(EXTERNALMICGAIN));
}

int para_externalmicgainname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, AUDIOINGAIN_20_DB);
	else if ( *arg == '1' )
		return sprintf(data, AUDIOINGAIN_26_DB);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s",AUDIOINGAIN_20_DB,AUDIOINGAIN_26_DB);
	return -1;
}

int para_micgain(request *req, char *data, char *arg)
{
	char str[10];
	//if (sys_info->config.lan_config.micininputgain == 0)
	if (sys_info->ipcam[MICININPUTGAIN].config.u8 == 0)
		strcpy(str, MICGAIN_1);
	//else if (sys_info->config.lan_config.micininputgain == 1)
	else if (sys_info->ipcam[MICININPUTGAIN].config.u8 == 1)
		strcpy(str, MICGAIN_2);
	return sprintf(data, "%s", str);
//	return sprintf(data, "%d", sys_info->config.lan_config.micininputgain);
}

int para_micgainname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "0 dB");
	else if ( *arg == '1' )
		return sprintf(data, "20 dB");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "0 dB;20 dB");
	return -1;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int para_audiooutvolume(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.audiooutvolume);
	return sprintf(data, "%d", sys_info->ipcam[AUDIOOUTVOLUME].config.u8);
}

int para_audiooutvolumename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )	return sprintf(data, "1");
	else if ( *arg == '1' )	return sprintf(data, "2");
	else if ( *arg == '2' )	return sprintf(data, "3");
	else if ( *arg == '3' )	return sprintf(data, "4");
	else if ( *arg == '4' )	return sprintf(data, "5");
	else if ( *arg == '5' )	return sprintf(data, "6");
	else if ( *arg == '6' )	return sprintf(data, "7");
	else if ( *arg == '7' )	return sprintf(data, "8");
	else if ( *arg == '8' )	return sprintf(data, "9");
	else if ( *arg == '9' )	return sprintf(data, "10");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1;2;3;4;5;6;7;8;9;10");
	return -1;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_enabledst(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.dst_enable);
	return sprintf(data, "%d", sys_info->ipcam[DST_ENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dstmode(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.dst_mode);
	return sprintf(data, "%d", sys_info->ipcam[DST_MODE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dstshift(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.dst_shift);
	return sprintf(data, "%d", sys_info->ipcam[DST_SHIFT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dstshiftname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "+2:00");
	else if ( *arg == '1' )
		return sprintf(data, "+1:30");
	else if ( *arg == '2' )
		return sprintf(data, "+1:00");
	else if ( *arg == '3' )
		return sprintf(data, "+0:30");
	else if ( *arg == '4' )
		return sprintf(data, "-0:30");
	else if ( *arg == '5' )
		return sprintf(data, "-1:00");
	else if ( *arg == '6' )
		return sprintf(data, "-1:30");
	else if ( *arg == '7' )
		return sprintf(data, "-2:00");	
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "+2:00;+1:30;+1:00;+0:30;-0:30;-1:00;-1:30;-2:00");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dststart(request *req, char *data, char *arg)
{
	/*return sprintf(data, "%d;%d;%d;%d;%02d", sys_info->config.lan_config.net.dststart.month, 
		sys_info->config.lan_config.net.dststart.week, sys_info->config.lan_config.net.dststart.day, 
		sys_info->config.lan_config.net.dststart.hour, sys_info->config.lan_config.net.dststart.minute);*/
	return sprintf(data, "%d;%d;%d;%02d;%02d", sys_info->ipcam[DSTSTART_MONTH].config.u8, 
		sys_info->ipcam[DSTSTART_WEEK].config.u8, sys_info->ipcam[DSTSTART_DAY].config.u8, 
		sys_info->ipcam[DSTSTART_HOUR].config.u8, sys_info->ipcam[DSTSTART_MINUTE].config.u8);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dstend(request *req, char *data, char *arg)
{
	/*return sprintf(data, "%d;%d;%d;%d;%02d", sys_info->config.lan_config.net.dstend.month, 
		sys_info->config.lan_config.net.dstend.week, sys_info->config.lan_config.net.dstend.day, 
		sys_info->config.lan_config.net.dstend.hour, sys_info->config.lan_config.net.dstend.minute);*/
	return sprintf(data, "%d;%d;%d;%02d;%02d", sys_info->ipcam[DSTEND_MONTH].config.u8, 
		sys_info->ipcam[DSTEND_WEEK].config.u8, sys_info->ipcam[DSTEND_DAY].config.u8, 
		sys_info->ipcam[DSTEND_HOUR].config.u8, sys_info->ipcam[DSTEND_MINUTE].config.u8);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_timeformat(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.time_format);
	return sprintf(data, "%d", sys_info->ipcam[TIME_FORMAT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_pppoeenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.pppoeenable);
	return sprintf(data, "%d", sys_info->ipcam[PPPOEENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg4desired(request *req, char *data, char *arg)
{
	return sprintf(data, "1");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxprealarmjpeg(request *req, char *data, char *arg)
{
	return sprintf(data, "90");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxpostalarmjpeg(request *req, char *data, char *arg)
{
	return sprintf(data, "300");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_prealarm(request *req, char *data, char *arg)
{
	return sprintf(data, "%d" , sys_info->config.ftp_config.prealarm);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_postalarm(request *req, char *data, char *arg)
{
	return sprintf(data, "%d" , sys_info->config.ftp_config.postalarm);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_aviprealarm(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d" , sys_info->config.lan_config.naviprealarm);
	return sprintf(data, "%d" , sys_info->ipcam[NAVIPREALARM].config.u32);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_avipostalarm(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d" , sys_info->config.lan_config.navipostalarm);
	return sprintf(data, "%d" , sys_info->ipcam[NAVIPOSTALARM].config.u32);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg4cenable(request *req, char *data, char *arg)
{
	return sprintf(data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int para_mpeg4cenable1(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->config.lan_config.net.mpeg4cenable1);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg4cenable2(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->config.lan_config.net.mpeg4cenable2);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
 #if 0
int para_mpeg4cvalue(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.nMpeg41bitrate/1000);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg42cvalue(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.nMpeg42bitrate/1000);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg4resname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "1280x720");
	else if ( *arg == '1' )
		return sprintf(data, "640x480");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1280x720;640x480");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg42resname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "640x360");
	else if ( *arg == '1' )
		return sprintf(data, "352x192");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "640x360;352x192");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_resolutionname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "720");
	else if ( *arg == '1' )
		return sprintf(data, "VGA");
	else if ( *arg == '2' )
		return sprintf(data, "CIF");
	else if ( *arg == '3' )
		return sprintf(data, "640x360");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "720;VGA;CIF;640x360");
	return -1;
}
/***************************************************************************
 *                                                                    *
 ***************************************************************************/
int para_videocodecresname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "M4:720,M4:CIF,JPG:352");
	else if ( *arg == '1' )
		return sprintf(data, "M4:VGA,JPG:VGA;M4:720,JPG:VGA");
	else if ( *arg == '2' )
		return sprintf(data, "M4:720,M4:CIF");
	else if ( *arg == '3' )
		return sprintf(data, "M4:720");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "M4:720,M4:CIF,JPG:352@M4:VGA,JPG:VGA;M4:720,JPG:VGA@M4:720,M4:CIF@M4:720");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_videocodecmodename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "MPEG4+MPEG4+JPEG");
	else if ( *arg == '1' )
		return sprintf(data, "MPEG4+JPEG");
	else if ( *arg == '2' )
		return sprintf(data, "MPEG4+MPEG4");
	else if ( *arg == '3' )
		return sprintf(data, "Single MPEG4");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "MPEG4+MPEG4+JPEG;MPEG4+JPEG;MPEG4+MPEG4;Single MPEG4");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_videoalce(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nVideoalc);
	return sprintf(data, "%d", sys_info->ipcam[NVIDEOALC].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_videocodecmode(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nVideocodecmode);
	return sprintf(data, "%d", sys_info->ipcam[NVIDEOCODECMODE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_videocodecres(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nVideocodecres);
	return sprintf(data, "%d", sys_info->ipcam[NVIDEOCODECRES].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg4quality1name(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Highest");
	else if ( *arg == '1' )
		return sprintf(data, "High");
	else if ( *arg == '2' )
		return sprintf(data, "Medium");
	else if ( *arg == '3' )
		return sprintf(data, "Low");
	else if ( *arg == '4' )
		return sprintf(data, "Lowest");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Highest;High;Medium;Low;Lowest");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg4quality2name(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Highest");
	else if ( *arg == '1' )
		return sprintf(data, "High");
	else if ( *arg == '2' )
		return sprintf(data, "Medium");
	else if ( *arg == '3' )
		return sprintf(data, "Low");
	else if ( *arg == '4' )
		return sprintf(data, "Lowest");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Highest;High;Medium;Low;Lowest");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_qualityname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Highest");
	else if ( *arg == '1' )
		return sprintf(data, "High");
	else if ( *arg == '2' )
		return sprintf(data, "Medium");
	else if ( *arg == '3' )
		return sprintf(data, "Low");
	else if ( *arg == '4' )
		return sprintf(data, "Lowest");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Highest;High;Medium;Low;Lowest");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_qualityname2(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Highest");
	else if ( *arg == '1' )
		return sprintf(data, "High");
	else if ( *arg == '2' )
		return sprintf(data, "Medium");
	else if ( *arg == '3' )
		return sprintf(data, "Low");
	else if ( *arg == '4' )
		return sprintf(data, "Lowest");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Highest;High;Medium;Low;Lowest");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionylimit(request *req, char *data, char *arg)
{
	return sprintf(data, "720");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ratecontrol(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nRateControl);
	return sprintf(data, "%d", sys_info->ipcam[NRATECONTROL].config.u8);
}
int para_ratecontrolname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "VBR");
	else if ( *arg == '1' )
		return sprintf(data, "CVBR");
	else if ( *arg == '2' )
		return sprintf(data, "CBR");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "VBR;CVBR;CBR");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
/*static char *night_ratename[3][3] = {{"30fps","----","----"},
									 {"----","15fps","----"},
									 {"----","----","7.5fps"}};*/
static char *night_ratename[4] = {"30","15","5","Auto"};
static char *night_ratename2[4] = {"15","7.5","2.5","Auto"};
static char *day_ratename[2][5] = 	{{"30 FPS","15 FPS","7.5 FPS","3 FPS","1 FSP"},
									 {"30 FPS","15 FPS","7.5 FPS","3 FPS","1 FPS"}};
static char *pal_ratename[5] = {"25 FPS","15 FPS","7.5 FPS","3 FPS","1 FPS"};
static char *day_ratename_mpg41[2][5] = 	{{"24 FPS","15 FPS","7.5 FPS","3 FPS","1 FPS"},
									 {"----","15 FPS","7.5 FPS","3 FPS","1 FPS"}};
static char *day_ratename_mpg42[2][5] = 	{{"24 FPS","15 FPS","7.5 FPS","3 FPS","1 FPS"},
									 {"30 FPS","15 FPS","7.5 FPS","3 FPS","1 FPS"}};
static char *pal_ratename_mpg41[5] =  {"20 FPS","15 FPS","7.5 FPS","3 FPS","1 FPS"};
static char *pal_ratename_mpg42[5] =  {"20 FPS","15 FPS","7.5 FPS","3 FPS","1 FPS"};
int para_mpeg41framerate(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.nMpeg41frmrate);
}
int para_mpeg41frameratename(request *req, char *data, char *arg)
{
	char **ratename = NULL;
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	unsigned char daynight = sys_info->config.lan_config.nDayNight;
	if (sys_info->config.lan_config.supportstream[1] == 0)
		return sprintf(data, "N/A");
	if( sys_info->video_type== 1 ){	//PAL
		ratename = pal_ratename_mpg41;
	}else	if (daynight == 0)	//NTSC
		ratename = day_ratename_mpg41[1];
	else
		ratename = day_ratename_mpg41[0];

	if ( *arg == '0' )
		return sprintf(data, "%s", ratename[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%s", ratename[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%s", ratename[2]);
	else if ( *arg == '3' && daynight == 0 )
		return sprintf(data, "%s", ratename[3]);
	else if ( *arg == 'a' || *arg == '\0' )
	{
		if (daynight == 0)
			return sprintf(data, "%s;%s;%s;%s", ratename[0], ratename[1], ratename[2], ratename[3]);
		else
			return sprintf(data, "%s;%s;%s", ratename[0], ratename[1], ratename[2]);
	}
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg42framerate(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.nMpeg42frmrate);
}
int para_mpeg42frameratename(request *req, char *data, char *arg)
{
	char **ratename = NULL;
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	unsigned char daynight = sys_info->config.lan_config.nDayNight;
	unsigned char frate = sys_info->config.lan_config.nMpeg41frmrate;
	if (sys_info->config.lan_config.supportstream[2] == 0)
		return sprintf(data, "N/A");
	if( sys_info->video_type == 1 ){	//PAL
		ratename = pal_ratename_mpg42;
	}else	if (daynight == 0) {	//NTSC
		if (sys_info->config.lan_config.nVideocodecmode == 0)
			ratename = night_ratename2;
		else
			ratename = night_ratename;
		return sprintf(data, "%s", ratename[frate]);
	}
	else if (sys_info->config.lan_config.nVideocodecmode == 0)
		ratename = day_ratename_mpg42[1];
	else
		ratename = day_ratename_mpg42[0];

	if ( *arg == '0' )
		return sprintf(data, "%s", ratename[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%s", ratename[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%s", ratename[2]);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s", ratename[0], ratename[1], ratename[2]);
	return -1;
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
 #if 0
int para_jpegframerate(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.nJpegfrmrate);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_jpegframerate2(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.nJpeg2frmrate);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_jpegframeratename(request *req, char *data, char *arg)
{
	char **ratename = NULL;
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	unsigned char daynight = sys_info->config.lan_config.nDayNight;
	unsigned char frate = sys_info->config.lan_config.nMpeg41frmrate;
	if (sys_info->config.lan_config.supportstream[0] == 0)
		return sprintf(data, "N/A");
	if( sys_info->video_type == 1 ){	//PAL
		ratename = pal_ratename;
	}else	if (daynight == 0) {	//NTSC
		if (sys_info->config.lan_config.nVideocodecmode == 0)
			ratename = night_ratename2;
		else
			ratename = night_ratename;
		return sprintf(data, "%s", ratename[frate]);
	}
	else if (sys_info->config.lan_config.nVideocodecmode == 0)
		ratename = day_ratename[1];
	else
		ratename = day_ratename[0];

	if ( *arg == '0' )
		return sprintf(data, "%s", ratename[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%s", ratename[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%s", ratename[2]);
	else if ( *arg == '3' )
		return sprintf(data, "%s", ratename[3]);
	else if ( *arg == '4' )
		return sprintf(data, "%s", ratename[4]);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s", ratename[0], ratename[1], ratename[2], ratename[3], ratename[4]);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_jpegframeratename2(request *req, char *data, char *arg)
{
	char **ratename = NULL;
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	unsigned char daynight = sys_info->config.lan_config.nDayNight;
	unsigned char frate = sys_info->config.lan_config.nMpeg41frmrate;
	if (sys_info->config.lan_config.supportstream[1] == 0)
		return sprintf(data, "N/A");
	if( sys_info->video_type == 1 ){	//PAL
		ratename = pal_ratename;
	}else	if (daynight == 0) {	//NTSC
		if (sys_info->config.lan_config.nVideocodecmode == 0)
			ratename = night_ratename2;
		else
			ratename = night_ratename;
		return sprintf(data, "%s", ratename[frate]);
	}
	else if (sys_info->config.lan_config.nVideocodecmode == 0)
		ratename = day_ratename[1];
	else
		ratename = day_ratename[0];

	if ( *arg == '0' )
		return sprintf(data, "%s", ratename[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%s", ratename[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%s", ratename[2]);
	else if ( *arg == '3' )
		return sprintf(data, "%s", ratename[3]);
	else if ( *arg == '4' )
		return sprintf(data, "%s", ratename[4]);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s", ratename[0], ratename[1], ratename[2], ratename[3], ratename[4]);
	return -1;
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_quadmodeselectname(request *req, char *data, char *arg)
{
	return sprintf(data, "30 FPS");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_videocodecname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "single codec");
	else if ( *arg == '1' )
		return sprintf(data, "dual codec");
	else if ( *arg == '2' )
		return sprintf(data, "triple codec");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "single codec;dual codec;triple codec");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_videocodec(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.nVideocodec);
	return sprintf(data, "%d", sys_info->ipcam[NVIDEOCODEC].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_videomodename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "JPEG");
	else if ( *arg == '1' )
		return sprintf(data, "MPEG");
	else if ( *arg == '2' )
		return sprintf(data, "JPEG+MPEG");
	else if ( *arg == '3' )
		return sprintf(data, "MPEG+MPEG");
	else if ( *arg == '4' )
		return sprintf(data, "JPEG+MPEG+MPEG");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "JPEG;MPEG;JPEG+MPEG;MPEG+MPEG;JPEG+MPEG+MPEG");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportonvif(request *req, char *data, char *arg)
{
#ifdef SUPPORT_ONVIF
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}

int para_videomode(request *req, char *data, char *arg)
{
	//dbg("sys_info->video_type=%d\n", sys_info->video_type);
	return sprintf(data, "%d", sys_info->video_type);
}
int para_flickless(request *req, char *data, char *arg)
{
	//dbg("sys_info->video_type=%d\n", sys_info->video_type);
	return sprintf(data, "%d", sys_info->video_type);
}
int para_powerline(request *req, char *data, char *arg)
{
	//dbg("sys_info->video_type=%d\n", sys_info->video_type);
	return sprintf(data, "%s", sys_info->video_type?"50":"60");
}

int para_supportflickless(request *req, char *data, char *arg)
{
#ifdef SUPPORT_FLICKLESS
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportpowerline(request *req, char *data, char *arg)
{
#ifdef SUPPORT_FLICKLESS
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportagc(request *req, char *data, char *arg)
{
#ifdef SUPPORT_AGC
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}


int para_supportagcctrl(request *req, char *data, char *arg)
{
#ifdef SUPPORT_AGCCTRL
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportaes(request *req, char *data, char *arg)
{
/*	use only in status_info.htm, modify by jiahung 20101125	*/
#ifdef SUPPORT_DC_IRIS_DIP_SW
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportvideoalc(request *req, char *data, char *arg)
{
#ifdef SUPPORT_VIDEOALC
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_agc(request *req, char *data, char *arg)
{
#if defined	SUPPORT_AGC
	int id = atoi(arg);

	if(EXPOSURETYPE == EXPOSUREMODE_SELECT){
		if ( *arg >= '0' && *arg <= '7') {
			//dbg("EXPOSURE_MODE%d_GAIN = %s", id, conf_string_read(EXPOSURE_INDOOR_GAIN+id));
			return sprintf(data, "%s", conf_string_read(EXPOSURE_INDOOR_GAIN+id));
		}//else if ( *arg == 'a' || *arg == '\0' ) {
			
		//}

		//dbg("EXPOSURE_MODE = %s", conf_string_read(EXPOSURE_MODE));
		if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_INDOOR_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_OUTDOOR))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_OUTDOOR_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_NIGHT))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_NIGHT_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_MOVING))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_MOVING_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_LOW_NOISE))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_LOWNOISE_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_CUSTOMIZE1_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_CUSTOMIZE2_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_CUSTOMIZE3_GAIN));
		else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_AUTO))
			return sprintf(data, "%s", conf_string_read(EXPOSURE_OUTDOOR_GAIN));
		//else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_SCHEDULE))
		//	return sprintf(data, "%s", conf_string_read(EXPOSURE_MODE7_GAIN));
		else 
			DBG("UNKNOWN EXPOSURE_MODE(%s)", conf_string_read(EXPOSURE_MODE));
		return -1;
	}else
		return sprintf(data, "%s", conf_string_read(NMAXAGCGAIN));
#else
		return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_agcname(request *req, char *data, char *arg)
{
#if defined	SUPPORT_AGC
	int i;
	if (*arg == 'a' || *arg == '\0') {
		int len = sprintf(data, "%s", sys_info->api_string.agc[0]);
		for (i=1; i<MAX_AGC; i++){
			if(!strcmp(sys_info->api_string.agc[i], ""))
				break;
			len += sprintf(data+len, ";%s", sys_info->api_string.agc[i]);
		}
		return len;
	}
	for (i=0; i<MAX_AGC; i++)
		if (*arg == '0'+i)
			return sprintf(data, "%s", sys_info->api_string.agc[i]);
	return -1;

#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_colorkiller(request *req, char *data, char *arg)
{
	return sprintf(data, "128");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_fluorescent(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.nFluorescent);
	return sprintf(data, "%d", sys_info->ipcam[NFLUORESCENT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_fluorescentname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "60Hz");
	else if ( *arg == '1' )
		return sprintf(data, "50Hz");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "60Hz;50Hz");
	return -1;
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_mirror(request *req, char *data, char *arg)  //add by mikewang 20080807
{

#ifdef MIRROR_FLIP_INV
	return sprintf(data, "%d", !sys_info->ipcam[NMIRROR].config.u8);
#else
	return sprintf(data, "%d", sys_info->ipcam[NMIRROR].config.u8);
#endif

}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_gammaname(request *req, char *data, char *arg)  //add by mikewang 20090204
{
	if ( *arg == '0' )
		return sprintf(data, "Gamma1");
	else if ( *arg == '1' )
		return sprintf(data, "Gamma2");
	else if ( *arg == '2' )
		return sprintf(data, "Gamma3");
	else if ( *arg == '3' )
		return sprintf(data, "Gamma4");
	else if ( *arg == '4' )
		return sprintf(data, "WDR Off");
	else if ( *arg == '5' )
		return sprintf(data, "Auto1");
	else if ( *arg == '6' )
		return sprintf(data, "Auto2");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Gamma1;Gamma2;Gamma3;Gamma4;WDR Off;Auto1;Auto2");
	return -1;
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_supportgamma(request *req, char *data, char *arg)  //add by mikewang 20090212
{
#ifdef SUPPORT_GAMMA
	return sprintf( data, "1" );
#else
	return sprintf( data, "0" );
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_flipmirrorname(request *req, char *data, char *arg)  //add by mikewang 20090204
{
#ifdef MODEL_LC7311
	if ( *arg == '0' )
		return sprintf(data, "Normal");
	else if ( *arg == '1' )
		return sprintf(data, "Flip");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Normal;Flip");	
	return -1;
#else
	if ( *arg == '0' )
		return sprintf(data, "Normal");
	else if ( *arg == '1' )
		return sprintf(data, "Horizontal");
	else if ( *arg == '2' )
		return sprintf(data, "Vertical");
	else if ( *arg == '3' )
		return sprintf(data, "BOTH");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Normal;Horizontal;Vertical;Both");
	return -1;
#endif // MODEL_LC7311
}

/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_gamma(request *req, char *data, char *arg)  //add by mikewang 20090204
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nGamma);
	return sprintf(data, "%d", sys_info->ipcam[NGAMMA].config.u8);
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_flip(request *req, char *data, char *arg)  //add by mikewang 20090204
{

#ifdef MIRROR_FLIP_INV
	return sprintf(data, "%d", !sys_info->ipcam[NFLIP].config.u8);
#else
	return sprintf(data, "%d", sys_info->ipcam[NFLIP].config.u8);
#endif

}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_antiflicker(request *req, char *data, char *arg)  //add by rennyhsu 20110425
{

#ifdef SUPPORT_ANTIFLICKER
		return sprintf(data, "%d", sys_info->ipcam[ANTIFLICKER].config.u8);
#else
		return -1;
#endif

}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_aeshutter(request *req, char *data, char *arg)  //add by Alex. 2009.04.27
{
	//return sprintf(data, "%d", sys_info->config.lan_config.aeshutter);
	return sprintf(data, "%d", sys_info->ipcam[AESHUTTER].config.u8);
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_supportflip(request *req, char *data, char *arg)  //add by mikewang 20090204
{
#ifdef SUPPORT_FLIP
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_aes(request *req, char *data, char *arg)  //add by Alex. 2009.04.27
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nAESswitch);
#ifdef SUPPORT_DC_IRIS_DIP_SW
	return sprintf(data, "%d", gio_get_value( GIO_IRIS_SWITCH ));
#else
 #ifdef DC_IRIS_DEFAULT
	return sprintf(data, "0");
 #else
	return sprintf(data, "1");
 #endif
#endif
}
/***************************************************************************
 *																			 *
 ***************************************************************************/
int para_upnpport(request *req, char *data, char *arg)  //add by mikewang 20080821
{
	return sprintf(data, "%d", sys_info->ipcam[UPNPPORT].config.u16);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_upnpssdpport(request *req, char *data, char *arg)  //add by mikewang 20080822
{
	return sprintf(data, "%d", sys_info->ipcam[UPNPSSDPPORT].config.u16);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_upnpenable(request *req, char *data, char *arg)  //add by mikewang 20080821
{
	return sprintf(data, "%d", sys_info->ipcam[UPNPENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_upnpcenabl(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[UPNPCENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_upnpcextport(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[UPNPCEXTPORT].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_upnpforwardingport(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[UPNPCEXTPORT].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_upnpssdpage(request *req, char *data, char *arg)  //add by mikewang 20080821
{
	return sprintf(data, "%d", sys_info->ipcam[UPNPSSDPAGE].config.u16);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tstampcolor(request *req, char *data, char *arg)  //add by mikewang 20080813
{
	return sprintf(data, "%d", sys_info->ipcam[TSTAMPCOLOR].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_pppoeactive(request *req, char *data, char *arg)  //add by mikewang 20080825
{
	return sprintf(data, "%d", sys_info->pppoe_active);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_pppoeaccount(request *req, char *data, char *arg)  //add by mikewang 20080825
{
	return sprintf(data, "%s", conf_string_read(PPPOEACCOUNT));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_pppoepwd(request *req, char *data, char *arg)  //add by mikewang 20080825
{
	return sprintf(data, "%s", conf_string_read(PPPOEPASSWORD));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxpppoeusrlen(request *req, char *data, char *arg)  //add by mikewang 20080825
{
	return sprintf(data, "%d", MAX_PPPOE_ID_LEN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxpppoepwdlen(request *req, char *data, char *arg)  //add by mikewang 20080825
{
	return sprintf(data, "%d", MAX_PPPOE_PWD_LEN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tstamploc(request *req, char *data, char *arg)  //add by mikewang 20080813
{
	return sprintf(data, "%d", sys_info->ipcam[TSTAMPLOCATION].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_blc(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[NBACKLIGHTCONTROL].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_kelvin(request *req, char *data, char *arg)
{
	return sprintf(data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supporthue(request *req, char *data, char *arg)
{
#ifdef SUPPORT_HUE
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportfluorescent(request *req, char *data, char *arg)
{
#ifdef SUPPORT_FLUORESCENT
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmirror(request *req, char *data, char *arg)
{
#ifdef SUPPORT_MIRROR
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportkelvin(request *req, char *data, char *arg)
{
#ifdef SUPPORT_KELVIN
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_senseup(request *req, char *data, char *arg)
{
	return sprintf(data, "1");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportsenseup(request *req, char *data, char *arg)
{
#ifdef SUPPORT_SENSEUP
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmaxagcgain(request *req, char *data, char *arg)
{
#ifdef SUPPORT_MAXAGCGAIN
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportblcmode(request *req, char *data, char *arg)
{
#ifdef SUPPORT_BLCMODE
		return sprintf(data, "1");
#else
		return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_blcmode(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nBacklightControl);
	return sprintf(data, "%d", sys_info->ipcam[NBACKLIGHTCONTROL].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supporthspeedshutter(request *req, char *data, char *arg)
{
#ifdef SUPPORT_HSPEEDSHUTTER
			return sprintf(data, "1");
#else
			return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_hspeedshutter(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.nHspeedshutter);
	return sprintf(data, "%d", sys_info->ipcam[NHSPEEDSHUTTER].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_awbname(request *req, char *data, char *arg)
{
	int i;
	if (*arg == 'a' || *arg == '\0') {
		int len = sprintf(data, "%s", sys_info->api_string.awb[0]);
		for (i=1; i<MAX_AWB; i++){
			if(!strcmp(sys_info->api_string.awb[i], ""))
				break;
			len += sprintf(data+len, ";%s", sys_info->api_string.awb[i]);
		}
		return len;
	}
	for (i=0; i<MAX_AWB; i++)
		if (*arg == '0'+i)
			return sprintf(data, "%s", sys_info->api_string.awb[i]);
	return -1;

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_awb(request *req, char *data, char *arg)
{

	//return sprintf(data, "%s", sys_info->config.lan_config.nWhiteBalance);
	return sprintf(data, "%s", conf_string_read(NWHITEBALANCE));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rgain(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.rgain);
	return sprintf(data, "%d", sys_info->ipcam[RGAIN].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_bgain(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.bgain);
	return sprintf(data, "%d", sys_info->ipcam[BGAIN].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tvcable(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.nTVcable);
	return sprintf(data, "%d", sys_info->ipcam[NTVCABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
char *binskip[3] = {"binning","skipping","4x binning"};
int para_binningname(request *req, char *data, char *arg)
{

	/*if (pSysInfo->lan_config.nVideocodecmode == 1 &&
		pSysInfo->lan_config.nVideocodecres == 1)
		binname = binskip[1];
	else
		binname = binskip[0];*/
	if ( *arg == '0' )
		return sprintf(data, "%s", binskip[0]);
	else if ( *arg == '1' )
		return sprintf(data, "%s", binskip[1]);
	else if ( *arg == '2' )
		return sprintf(data, "%s", binskip[2]);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s", binskip[0], binskip[1], binskip[2]);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_binning(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.nBinning);
	return sprintf(data, "%d", sys_info->ipcam[NBINNING].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxagcgainname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "30dB");
	else if ( *arg == '1' )
		return sprintf(data, "24dB");
	else if ( *arg == '2' )
		return sprintf(data, "16dB");
	else if ( *arg == '3' )
		return sprintf(data, "8dB");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "30dB;24dB;16dB;8dB");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxagcgain(request *req, char *data, char *arg)
{

	//return sprintf(data, "%s", sys_info->config.lan_config.nMaxAGCGain);
	return sprintf(data, "%s", conf_string_read(NMAXAGCGAIN));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_agcctrl(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.agcctrl);
	return sprintf(data, "%d", sys_info->ipcam[AGCCTRL].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_daynightname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "night");
	else if ( *arg == '1' )
		return sprintf(data, "day");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "night;day");
	return -1;
}
/***************************************************************************
 * dan                                                                     *
 ***************************************************************************/
int para_daynight(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.lan_config.nDayNight);
	return sprintf(data, "%d", sys_info->ipcam[NDAYNIGHT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_blcmodename(request *req, char *data, char *arg)
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 )		//AICHI
	#ifndef MODEL_LC7315
		if ( *arg == '0' )
			return sprintf(data, "Disable");
		else if ( *arg == '1' )
			return sprintf(data, "Fixed Gain");
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, "Disable;Fixed Gain");
		return -1;
	#endif
#else	
	if ( *arg == '0' )
		return sprintf(data, "Disable");
	else if ( *arg == '1' )
		return sprintf(data, "Weighted");
	else if ( *arg == '2' )
		return sprintf(data, "Fixed Gain");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Disable;Weighted;Fixed Gain");
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_backlight(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nBackLight);
	return sprintf(data, "%d", sys_info->ipcam[NBACKLIGHT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_brightness(request *req, char *data, char *arg)
{
	#ifdef IMAGE_8_ORDER
		return sprintf(data, "%d", sys_info->ipcam[NBRIGHTNESS].config.u8/29);
	#else
		return sprintf(data, "%d", sys_info->ipcam[NBRIGHTNESS].config.u8);
	#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minbrightness(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_BRIGHTNESS);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxbrightness(request *req, char *data, char *arg)
{
	#ifdef IMAGE_8_ORDER
		return sprintf(data, "%d", 8);
	#else
		return sprintf(data, "%d", MAX_BRIGHTNESS);
	#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minsharpness(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_SHARPNESS);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxsharpness(request *req, char *data, char *arg)
{
	#ifdef IMAGE_8_ORDER
		return sprintf(data, "%d", 8);
	#else
		return sprintf(data, "%d", MAX_SHARPNESS);
	#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_exposureschedulemodename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, EXPOSURETIME_INDOOR);
	else if ( *arg == '1' )
		return sprintf(data, EXPOSURETIME_OUTDOOR);
	else if ( *arg == '2' )
		return sprintf(data, EXPOSURETIME_NIGHT);
	else if ( *arg == '3' )
		return sprintf(data, EXPOSURETIME_MOVING);
	else if ( *arg == '4' )
		return sprintf(data, EXPOSURETIME_LOW_NOISE);
	else if ( *arg == '5' )
		return sprintf(data, EXPOSURETIME_CUSTOMIZE1);
	else if ( *arg == '6' )
		return sprintf(data, EXPOSURETIME_CUSTOMIZE2);
	else if ( *arg == '7' )
		return sprintf(data, EXPOSURETIME_CUSTOMIZE3);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s;%s;%s;%s", EXPOSURETIME_INDOOR, EXPOSURETIME_OUTDOOR, EXPOSURETIME_NIGHT, EXPOSURETIME_MOVING, 
			EXPOSURETIME_LOW_NOISE, EXPOSURETIME_CUSTOMIZE1, EXPOSURETIME_CUSTOMIZE2, EXPOSURETIME_CUSTOMIZE3);

	return -1;
}

int para_maxshuttername(request *req, char *data, char *arg)
{
	int i;
	if (*arg == 'a' || *arg == '\0') {
		int len = sprintf(data, "%s", sys_info->api_string.maxshutter[0]);
		for (i=1; i<MAX_EXPOSURETIME; i++){
			if(!strcmp(sys_info->api_string.maxshutter[i], ""))
				break;
			len += sprintf(data+len, ";%s", sys_info->api_string.maxshutter[i]);
		}
		return len;
	}
	for (i=0; i<MAX_EXPOSURETIME; i++)
		if (*arg == '0'+i)
			return sprintf(data, "%s", sys_info->api_string.maxshutter[i]);
	return -1;
}
int para_minshuttername(request *req, char *data, char *arg)
{
	int i;
	if (*arg == 'a' || *arg == '\0') {
		int len = sprintf(data, "%s", sys_info->api_string.minshutter[0]);
		for (i=1; i<MAX_EXPOSURETIME; i++){
			if(!strcmp(sys_info->api_string.minshutter[i], ""))
				break;
			len += sprintf(data+len, ";%s", sys_info->api_string.minshutter[i]);
		}
		return len;
	}
	for (i=0; i<MAX_EXPOSURETIME; i++)
		if (*arg == '0'+i)
			return sprintf(data, "%s", sys_info->api_string.minshutter[i]);
	return -1;
}

int para_exposuretimename(request *req, char *data, char *arg)
{
	int i;
	if (*arg == 'a' || *arg == '\0') {
		int len = sprintf(data, "%s", sys_info->api_string.exposure_time[0]);
		for (i=1; i<MAX_EXPOSURETIME; i++){
			if(!strcmp(sys_info->api_string.exposure_time[i], ""))
				break;
			len += sprintf(data+len, ";%s", sys_info->api_string.exposure_time[i]);
		}
		return len;
	}
	for (i=0; i<MAX_EXPOSURETIME; i++)
		if (*arg == '0'+i)
			return sprintf(data, "%s", sys_info->api_string.exposure_time[i]);
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportexposuretime(request *req, char *data, char *arg)
{
#ifdef SUPPORT_EXPOSURETIME
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
	//return sprintf(data, "%d", sys_info->config.support_config.supportexposuretime);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportexposuretimemode(request *req, char *data, char *arg)
{
#ifdef SUPPORT_EXPOSURETIME
	return sprintf(data, "%d", EXPOSURETYPE);
#else
	return sprintf(data, "0");
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_exposuretime(request *req, char *data, char *arg)
{
	if(EXPOSURETYPE == EXPOSUREMODE_SELECT)
		return sprintf(data, "%s", conf_string_read(EXPOSURE_MODE));
	else if(EXPOSURETYPE == EXPOSUREMODE_SELECT)
		return sprintf(data, "%s", conf_string_read(A_EXPOSURE_TIME));
	else if(EXPOSURETYPE == EXPOSURETIME_SELECT)
		return sprintf(data, "%s", conf_string_read(A_EXPOSURE_TIME));
	else
		return -1;
}
int para_maxshutter(request *req, char *data, char *arg)
{
	int id = atoi(arg);
	if ( *arg >= '0' && *arg <= '2') {
		//dbg("MAXSHUTTER%d = %s", id, conf_string_read(MAXSHUTTER1+id*2));
		return sprintf(data, "%s", conf_string_read(MAXSHUTTER1+id*2));
	}

	//dbg("conf_string_read(EXPOSURE_MODE) = %s", conf_string_read(EXPOSURE_MODE));
	if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1))
		return sprintf(data, "%s", conf_string_read(MAXSHUTTER1));
	else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2))
		return sprintf(data, "%s", conf_string_read(MAXSHUTTER2));
	else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3))
		return sprintf(data, "%s", conf_string_read(MAXSHUTTER3));
	else
		return -1;
}
int para_minshutter(request *req, char *data, char *arg)
{
	int id = atoi(arg);
	if ( *arg >= '0' && *arg <= '2') {
		//dbg("MINSHUTTER%d = %s", id, conf_string_read(MINSHUTTER1+id*2));
		return sprintf(data, "%s", conf_string_read(MINSHUTTER1+id*2));
	}
	
	if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1))
		return sprintf(data, "%s", conf_string_read(MINSHUTTER1));
	else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2))
		return sprintf(data, "%s", conf_string_read(MINSHUTTER2));
	else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3))
		return sprintf(data, "%s", conf_string_read(MINSHUTTER3));
	else
		return -1;
}
int para_exposureschedule(request *req, char *data, char *arg)
{
	int i, ret = 0, idx = 0;

	if ( *arg >= '0' && *arg <= '3') {
		int id = atoi(arg);
		idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*id;
		ret += sprintf(data, "%d:%d:%s:%02d%02d:%02d%02d", id, 
			sys_info->ipcam[EXPOSURE_SCHEDULE0_EN+idx].config.value,
			conf_string_read(EXPOSURE_SCHEDULE0_MODE+idx),
			sys_info->ipcam[EXPOSURE_SCHEDULE0_START_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_START_MIN+idx].config.value,
			sys_info->ipcam[EXPOSURE_SCHEDULE0_END_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_END_MIN+idx].config.value);
	}
	else if ( *arg == 'a' || *arg == '\0' ) {
		for(i = 0 ; i < EXPOSURE_EX_MAX_NUM; i++) {
			idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*i;
			ret += sprintf(data+ret, "%d:%d:%s:%02d%02d:%02d%02d", i, 
				sys_info->ipcam[EXPOSURE_SCHEDULE0_EN+idx].config.value,
				conf_string_read(EXPOSURE_SCHEDULE0_MODE+idx),
				sys_info->ipcam[EXPOSURE_SCHEDULE0_START_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_START_MIN+idx].config.value,
				sys_info->ipcam[EXPOSURE_SCHEDULE0_END_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_END_MIN+idx].config.value);

		}
	}

	return ret;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int para_meteringmethod(request *req, char *data, char *arg)
{
	if(sys_info->ipcam[METERINGMETHOD].config.value == METERING_AVERAGE)
		return sprintf(data, "%s", "Average");
	else if(sys_info->ipcam[METERINGMETHOD].config.value == METERING_CENTER_WEIGHTED)
		return sprintf(data, "%s", "Center-weighted");
	else if(sys_info->ipcam[METERINGMETHOD].config.value == METERING_SPOT)
		return sprintf(data, "%s", "Spot");
	else
		return -1;
}

int para_meteringmethodname(request *req, char *data, char *arg)
{
	//if ( *arg == '0' )
		//return sprintf(data, "Average");
	if ( *arg == '0' )
		return sprintf(data, "Center-weighted");
	else if ( *arg == '1' )
		return sprintf(data, "Spot");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Center-weighted;Spot");
	return -1;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_conflicexposure(request *req, char *data, char *arg)
{
#if defined ( SENSOR_USE_MT9V136 )
    // Only for sensor MT9V136
    // If the exposuretime setting not in the 'auto' status, blc and brightness will disable.
    return sprintf(data, "1");
#else
    return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_exposuretimeex(request *req, char *data, char *arg)
{
#if (SUPPORT_EXPOSURE_EX == 1)
	int i, ret = 0;
	if ( *arg >= '0' && *arg <= '3') {
		int id = atoi(arg);
		ret += sprintf(data, "%d:%d:%s:%02d%02d:%02d%02d", id, 
			sys_info->config.exposure_ex[id].enable,
			sys_info->config.exposure_ex[id].value,
			sys_info->config.exposure_ex[id].start.nHour, sys_info->config.exposure_ex[id].start.nMin,
			sys_info->config.exposure_ex[id].end.nHour, sys_info->config.exposure_ex[id].end.nMin);

	}
	else if ( *arg == 'a' || *arg == '\0' ) {
		for(i = 0 ; i < (EXPOSURE_EX_MAX_NUM + 1); i++) {
			ret += sprintf(data, "%d:%d:%s:%02d%02d:%02d%02d", i, 
				sys_info->config.exposure_ex[i].enable,
				sys_info->config.exposure_ex[i].value,
				sys_info->config.exposure_ex[i].start.nHour, sys_info->config.exposure_ex[i].start.nMin,
				sys_info->config.exposure_ex[i].end.nHour, sys_info->config.exposure_ex[i].end.nMin);
		}
	}

	return ret;
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_wdrenable(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[WDR_EN].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_wdrlevel(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[WDR_LEVEL].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minwdrlevel(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_WDRLEVEL);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxwdrlevel(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_WDRLEVEL);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportwdrlevel(request *req, char *data, char *arg)
{
#ifdef SUPPORT_WDRLEVEL
	return sprintf(data, "1");
#elif defined SUPPORT_VISCA
	if(sys_info->vsptz.wdr)
		return sprintf(data, "1");
	else
		return sprintf(data, "0");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportdenoise(request *req, char *data, char *arg)
{
#ifdef SUPPORT_DENOISE
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_denoise(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[NDENOISE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mindenoise(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_DENOISE);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxdenoise(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_DENOISE);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportevcomp(request *req, char *data, char *arg)
{
#ifdef SUPPORT_EVCOMP
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_evcomp(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[NEVCOMP].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minevcomp(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_EVCOMP);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxevcomp(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_EVCOMP);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportdre(request *req, char *data, char *arg)
{
#ifdef SUPPORT_DREMODE
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportmeteringmethod(request *req, char *data, char *arg)
{
#ifdef SUPPORT_METERINGMETHOD
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportcapturepriority(request *req, char *data, char *arg)
{
#ifdef SUPPORT_CAPTUREPRIORITY
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dremode(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[NDREMODE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dremodename(request *req, char *data, char *arg)
{
#ifdef SUPPORT_DREMODE
	if ( *arg == '0' )
		return sprintf(data, "None");
	else if ( *arg == '1' )
		return sprintf(data, "WDR");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "None;WDR");
	return -1;
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mincontrast(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_CONTRAST);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxcontrast(request *req, char *data, char *arg)
{
	#ifdef IMAGE_8_ORDER
		return sprintf(data, "%d", 8);
	#else
		return sprintf(data, "%d", MAX_CONTRAST);
	#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minsaturation(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MIN_SATURATION);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxsaturation(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", 255);
	return sprintf(data, "%d", MAX_SATURATION);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_contrast(request *req, char *data, char *arg)
{
	#ifdef IMAGE_8_ORDER
		return sprintf(data, "%d", sys_info->ipcam[NCONTRAST].config.u8/29);
	#else
		return sprintf(data, "%d", sys_info->ipcam[NCONTRAST].config.u8);
	#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_hue(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nHue);
	return sprintf(data, "%d", sys_info->ipcam[NHUE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_saturation(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nSaturation);
	return sprintf(data, "%d", sys_info->ipcam[NSATURATION].config.u8);
}
/***************************************************************************
 *here                                                                     *
 ***************************************************************************/
int para_stream1xsize(request *req, char *data, char *arg)
{
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(1) == SPECIAL_PROFILE)
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE0_XSIZE].config.value);
	else{
#ifdef SUPPORT_EPTZ
		int x, y;
		//sscanf(sys_info->config.profile_config[0].eptzWindowSize, "%dx%d", &x, &y);
		sscanf(conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE), "%dx%d", &x, &y);
		return sprintf(data, "%d", x);
#else
		//return sprintf(data, "%d", sys_info->config.lan_config.profilesize[0].xsize);
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE0_XSIZE].config.value);
#endif
	}

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream1ysize(request *req, char *data, char *arg)
{
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(1) == SPECIAL_PROFILE)
			return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE0_YSIZE].config.value);
	else{
#ifdef SUPPORT_EPTZ
		int x, y;
		//sscanf(sys_info->config.profile_config[0].eptzWindowSize, "%dx%d", &x, &y);
		sscanf(conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE), "%dx%d", &x, &y);
		return sprintf(data, "%d", y);
#else
		//return sprintf(data, "%d", sys_info->config.lan_config.profilesize[0].ysize);
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE0_YSIZE].config.value);
#endif
	}
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream3xsize(request *req, char *data, char *arg)
{
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(3) == SPECIAL_PROFILE)
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE2_XSIZE].config.value);
	else{
#ifdef SUPPORT_EPTZ
		int x, y;
		//sscanf(sys_info->config.profile_config[2].eptzWindowSize, "%dx%d", &x, &y);
		sscanf(conf_string_read(PROFILE_CONFIG2_EPTZWINDOWSIZE), "%dx%d", &x, &y);
		return sprintf(data, "%d", x);
#else
		//return sprintf(data, "%d", sys_info->config.lan_config.profilesize[2].xsize);
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE2_XSIZE].config.value);
#endif
	}
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream3ysize(request *req, char *data, char *arg)
{
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(3) == SPECIAL_PROFILE)
			return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE2_YSIZE].config.value);
	else{
#ifdef SUPPORT_EPTZ
		int x, y;
		//sscanf(sys_info->config.profile_config[2].eptzWindowSize, "%dx%d", &x, &y);
		sscanf(conf_string_read(PROFILE_CONFIG2_EPTZWINDOWSIZE), "%dx%d", &x, &y);
		return sprintf(data, "%d", y);
#else
		//return sprintf(data, "%d", sys_info->config.lan_config.profilesize[2].ysize);
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE2_YSIZE].config.value);
#endif
	}
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream4xsize(request *req, char *data, char *arg)
{
	//NO EPTZ
	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(4) == SPECIAL_PROFILE)
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE3_XSIZE].config.value);
	else
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE3_XSIZE].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream4ysize(request *req, char *data, char *arg)
{
	//NO EPTZ
	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(4) == SPECIAL_PROFILE)
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE3_YSIZE].config.value);
	else
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE3_YSIZE].config.value);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream2xsize(request *req, char *data, char *arg)
{
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(2) == SPECIAL_PROFILE)
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE1_XSIZE].config.value);
	else{
#ifdef SUPPORT_EPTZ
		int x, y;
		//sscanf(sys_info->config.profile_config[1].eptzWindowSize, "%dx%d", &x, &y);
		sscanf(conf_string_read(PROFILE_CONFIG1_EPTZWINDOWSIZE), "%dx%d", &x, &y);
		return sprintf(data, "%d", x);
#else
		//return sprintf(data, "%d", sys_info->config.lan_config.profilesize[1].xsize);
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE1_XSIZE].config.value);
#endif
	}
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream2ysize(request *req, char *data, char *arg)
{
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	else if(profile_check(2) == SPECIAL_PROFILE)
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE1_YSIZE].config.value);
	else{
#ifdef SUPPORT_EPTZ
		int x, y;
		//sscanf(sys_info->config.profile_config[1].eptzWindowSize, "%dx%d", &x, &y);
		sscanf(conf_string_read(PROFILE_CONFIG1_EPTZWINDOWSIZE), "%dx%d", &x, &y);
		return sprintf(data, "%d", y);
#else
		//return sprintf(data, "%d", sys_info->config.lan_config.profilesize[1].ysize);
		return sprintf(data, "%d", sys_info->ipcam[PROFILESIZE1_YSIZE].config.value);
#endif
	}
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream1name(request *req, char *data, char *arg)
{
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;

#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined( SENSOR_USE_IMX035 ) || defined( PLAT_DM365 )
	//return sprintf(data, "Profile-1(%dx%d)",	sys_info->config.lan_config.profilesize[0].xsize,
										//sys_info->config.lan_config.profilesize[0].ysize);
	return sprintf(data, "Profile-1(%dx%d)",	sys_info->ipcam[PROFILESIZE0_XSIZE].config.value,
										sys_info->ipcam[PROFILESIZE0_YSIZE].config.value);
#else 
#error Unknown sensor type
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream2name(request *req, char *data, char *arg)
{
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;

#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined( SENSOR_USE_IMX035 ) || defined( PLAT_DM365 )
	//return sprintf(data, "Profile-2(%dx%d)",	sys_info->config.lan_config.profilesize[1].xsize,
	//										sys_info->config.lan_config.profilesize[1].ysize);
	return sprintf(data, "Profile-2(%dx%d)",	sys_info->ipcam[PROFILESIZE1_XSIZE].config.value,
										sys_info->ipcam[PROFILESIZE1_YSIZE].config.value);
#else
#error Unknown sensor type
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_stream3name(request *req, char *data, char *arg)
{
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;

#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined( SENSOR_USE_IMX035 ) || defined( PLAT_DM365 )
	//return sprintf(data, "Profile-3(%dx%d)",	sys_info->config.lan_config.profilesize[2].xsize,
	//										sys_info->config.lan_config.profilesize[2].ysize);
	return sprintf(data, "Profile-3(%dx%d)",	sys_info->ipcam[PROFILESIZE2_XSIZE].config.value,
										sys_info->ipcam[PROFILESIZE2_YSIZE].config.value);
#else
#error Unknown sensor type
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
int para_stream4name(request *req, char *data, char *arg)
{
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined( SENSOR_USE_IMX035 ) || defined( PLAT_DM365 )
	//return sprintf(data, "Profile-4(%dx%d)",	sys_info->config.lan_config.profilesize[3].xsize,
	//										sys_info->config.lan_config.profilesize[3].ysize);
	return sprintf(data, "Profile-4(%dx%d)",	sys_info->ipcam[PROFILESIZE3_XSIZE].config.value,
										sys_info->ipcam[PROFILESIZE3_YSIZE].config.value);

#else
#error Unknown sensor type
#endif
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_quality1mpeg4(request *req, char *data, char *arg)
{
	return sprintf(data, "%d",	sys_info->config.lan_config.net.quality1mpeg4);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_quality2mpeg4(request *req, char *data, char *arg)
{
	return sprintf(data, "%d",	sys_info->config.lan_config.net.quality2mpeg4);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dmsxsize(request *req, char *data, char *arg)
{
	int x = 0;
	for( x = 0 ; x < 3 ; x++ ){
		if( sys_info->profile_config.profile_codec_fmt[x] == SEND_FMT_JPEG ){
			//return sprintf(data, "%d" ,	sys_info->config.lan_config.profilesize[x].xsize );
			return sprintf(data, "%d" ,	sys_info->ipcam[PROFILESIZE0_XSIZE+x*2].config.value );
			break;
		}
	}
	return sprintf( data, "%d" , 0 );
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dmsysize(request *req, char *data, char *arg)
{
	int x = 0;
	for( x = 0 ; x < 3 ; x++ ){
		if( sys_info->profile_config.profile_codec_fmt[x] == SEND_FMT_JPEG ){
			//return sprintf(data, "%d" ,	sys_info->config.lan_config.profilesize[x].ysize );
			return sprintf(data, "%d" ,	sys_info->ipcam[PROFILESIZE0_YSIZE+x*2].config.value );
			break;
		}
	}
	return sprintf( data, "%d" , 0 );
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sharpness(request *req, char *data, char *arg)
{
	#ifdef IMAGE_8_ORDER
		return sprintf(data, "%d", sys_info->ipcam[NSHARPNESS].config.u8/29);
	#else
		return sprintf(data, "%d", sys_info->ipcam[NSHARPNESS].config.u8);
	#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_timezonename(request *req, char *data, char *arg)
{
	static char *TZname[] = {
		"GMT-12 Eniwetok, Kwajalein",
		"GMT-11 Midway Island, Samoa",
		"GMT-10 Hawaii",
		"GMT-09 Alaska",
		"GMT-08 Pacific Time (US & Canada), Tijuana",
		"GMT-07 Mountain Time (US & Canada), Arizona",
		"GMT-06 Central Time (US & Canada), Mexico City, Tegucigalpa, Saskatchewan",
		"GMT-05 Eastern Time (US & Canada), Indiana(East), Bogota, Lima",
		"GMT-04 Atlantic Time (Canada), Caracas, La Paz",
		"GMT-03 Brasilia, Buenos Aires, Georgetown",
		"GMT-02 Mid-Atlantic",
		"GMT-01 Azores, Cape Verdes Is.",
		"GMT+00 GMT, Dublin, Edinburgh, London, Lisbon, Monrovia, Casablanca",
		"GMT+01 Berlin, Stockholm, Rome, Bern, Brussels, Vienna, Paris, Madrid, Amsterdam, Prague, Warsaw, Budapest",
		"GMT+02 Athens, Helsinki, Istanbul, Cairo, Eastern Europe, Harare, Pretoria, Israel",
		"GMT+03 Baghdad, Kuwait, Nairobi, Riyadh, Moscow, St. Petersburg, Kazan, Volgograd",
		"GMT+04 Abu Dhabi, Muscat, Tbilisi",
		"GMT+05 Islamabad, Karachi, Ekaterinburg, Tashkent",
		"GMT+06 Alma Ata, Dhaka",
		"GMT+07 Bangkok, Jakarta, Hanoi",
		"GMT+08 Taipei, Beijing, Chongqing, Urumqi, Hong Kong, Perth, Singapore",
		"GMT+09 Tokyo, Osaka, Sapporo, Seoul, Yakutsk",
		"GMT+10 Brisbane, Melbourne, Sydney, Guam, Port Moresby, Vladivostok, Hobart",
		"GMT+11 Magadan, Solomon Is., New Caledonia",
		"GMT+12 Fiji, Kamchatka, Marshall Is., Wellington, Auckland"
	};

	if ( *arg == '\0' ) {
		//if (sys_info->config.lan_config.net.ntp_timezone <= 24)
		//	return sprintf(data, "%s", TZname[sys_info->config.lan_config.net.ntp_timezone]);
		if (sys_info->ipcam[NTP_TIMEZONE].config.u8 <= 24)
			return sprintf(data, "%s", TZname[sys_info->ipcam[NTP_TIMEZONE].config.u8]);
		return -1;
	}
	else {
		int tz = atoi(arg);
		if ((tz >= 0) && (tz <= 24))
			return sprintf(data, "%s", TZname[tz]);
		return -1;
	}
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_timeformatname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "YYYY/MM/DD");
	else if ( *arg == '1' )
		return sprintf(data, "MM/DD/YYYY");
	else if ( *arg == '2' )
		return sprintf(data, "DD/MM/YYYY");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "YYYY/MM/DD;MM/DD/YYYY;DD/MM/YYYY");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxnamelen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_USER_SIZE );
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxpwdlen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", MAX_PASS_SIZE );
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_authorityadmin(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", AUTHORITY_ADMIN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_authorityoperator(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", AUTHORITY_OPERATOR);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_authorityviewer(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", AUTHORITY_VIEWER);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minnamelen(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 4);
}
int para_minpwdlen(request *req, char *data, char *arg)
{
	BIOS_DATA bios;
	bios_data_read(&bios);
	int brand;
	
	//dbg("get_brand_id() = %d", get_brand_id());
	brand = get_brand_id();
	if(brand == BRAND_ID_FMM)
		return sprintf(data, "%d", 8);
	else	
		return sprintf(data, "%d", MIN_PASS_SIZE);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_user(request *req, char *data, char *arg)
{
	int i, count = 0, arg_i;
	//USER_INFO *account = sys_info->config.acounts;
	arg_i = atoi(arg);

	if ( *arg == 'a' || *arg == '\0' ) {
		for(i = 0; i< ACOUNT_NUM;i++)
			//count += sprintf(data + count, "user%d:%s\n", i + 1, account[i].name);
			count += sprintf(data + count, "user%d:%s\n", i + 1, conf_string_read(ACOUNTS0_NAME+i*3));
		return count;
	}

	if ( arg_i >= 0 && arg_i < ACOUNT_NUM )
		//return sprintf(data, "%s", account[*arg - '0'].name);
		return sprintf(data, "%s", conf_string_read(ACOUNTS0_NAME+arg_i*3));

	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_authority(request *req, char *data, char *arg)
{
	int i, count = 0, arg_i;
	//USER_INFO *account = sys_info->config.acounts;
	arg_i = atoi(arg);
	
	if ( *arg == 'a' || *arg == '\0' ){
		for(i = 0; i< ACOUNT_NUM;i++)
			//count += sprintf(data + count, "authority%d:%d\n", i+1, account[i].authority);
			count += sprintf(data + count, "authority%d:%d\n", i+1, sys_info->ipcam[ACOUNTS0_AUTHORITY+i*3].config.u8);
		return count;
	}

	if ( arg_i >= 0 && arg_i < ACOUNT_NUM )
		//return sprintf(data, "%d", account[*arg - '0'].authority);
		return sprintf(data, "%d", sys_info->ipcam[ACOUNTS0_AUTHORITY+arg_i*3].config.u8);
	
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_recordtype(request *req, char *data, char *arg)
{
	// CAN NOT WORK TEMPORARILY
	// Alex. 2009.05.27
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_mpeg42resolution(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.lan_config.net.mpeg42resolution);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0 //disenable by jiahung 2010.01.06

int para_schedule(request *req, char *data, char *arg)
{
	Schedule_t *pSchedule;
	int ret = 0, i, arg_i = atoi(arg);

	if ( *arg == 'a' || *arg == '\0' ){
		for(i = 0; i < SCHDULE_NUM; i ++){
			pSchedule = &(sys_info->config.lan_config.aSchedules[i]);
			ret += sprintf(data + ret,
				"Schedule %d Enable %d Day %02d Strat time(%02d%02d%02d) Duration (%02d%02d%02d)\n", i,
				pSchedule -> bStatus, pSchedule -> nDay,
				pSchedule -> tStart.nHour, pSchedule -> tStart.nMin,
				pSchedule -> tStart.nSec, pSchedule -> tDuration.nHour,
				pSchedule -> tDuration.nMin, pSchedule -> tDuration.nSec);
		}
		return ret;
	} else if (arg_i >= 0 && arg_i < SCHDULE_NUM){
		pSchedule = &(sys_info->config.lan_config.aSchedules[arg_i]);
		return sprintf(data, "%1d%02d%02d%02d%02d%02d%02d%02d",
				pSchedule -> bStatus, pSchedule -> nDay,
				pSchedule -> tStart.nHour, pSchedule -> tStart.nMin,
				pSchedule -> tStart.nSec, pSchedule -> tDuration.nHour,
				pSchedule -> tDuration.nMin, pSchedule -> tDuration.nSec);
	}
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdformatName(request *req, char *data, char *arg)
{
  int temp;
  /*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;
	else {*/
	      //temp=sys_info->config.lan_config.nVideocodecmode;
	      temp=sys_info->ipcam[NVIDEOCODECMODE].config.u8;
  //  }
    if((temp==2)||(temp==3)){
	 if ( *arg == '0' )
		return sprintf(data, "MPEG4");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "MPEG4");
	}
	else{
        if ( *arg == '0' )
		    return sprintf(data, "MPEG4");
	    else if ( *arg == '1' )
		    return sprintf(data, "JPEG");
	    else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "MPEG4;JPEG");
	}
	return -1;

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_formatName(request *req, char *data, char *arg)
{
  int temp;
  /*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
		
  //temp = sys_info->config.lan_config.nVideocodecmode;
  temp = sys_info->ipcam[NVIDEOCODECMODE].config.u8;


/*
	if(sys_info->config.sdcard_config.sdinsert==1){
		if( *arg =='0')
			return sprintf(data, "JPEG");
		else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "JPEG");
	}
*/  /* disable by Alex, 2008.10.06  */

  if((temp == 2) || ( temp == 3)) {
    if ( *arg == '0' )
      return sprintf(data, "MPEG4");
    else if ( *arg == 'a' || *arg == '\0' )
      return sprintf(data, "MPEG4");
  }
  else {
    if ( *arg == '0' )
      return sprintf(data, "MPEG4");
    else if ( *arg == '1' )
      return sprintf(data, "JPEG");
    else if ( *arg == 'a' || *arg == '\0' )
      return sprintf(data, "MPEG4;JPEG");
  }

  return -1;
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_privacywindowsize(request *req, char *data, char *arg)
{
	//no used
#ifdef SUPPORT_PROFILE_NUMBER
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_EPTZWINDOWSIZE));
#else
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_EPTZWINDOWSIZE));
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionwindowsize(request *req, char *data, char *arg)
{
	//no used
#ifdef SUPPORT_PROFILE_NUMBER
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_EPTZWINDOWSIZE));
#else
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_EPTZWINDOWSIZE));
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_profile1coordinate(request *req, char *data, char *arg)
{
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG0_EPTZCOORDINATE));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_profile2coordinate(request *req, char *data, char *arg)
{
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG1_EPTZCOORDINATE));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_profile3coordinate(request *req, char *data, char *arg)
{
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG2_EPTZCOORDINATE));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
int para_sdfileformat(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[SDCARD_FILEFORMAT].config.u8);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
 int para_contfreq(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[SDCARD_JPG_RECRATE].config.u8);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
 int para_contfreqname(request *req, char *data, char *arg)
{	
	if ( *arg == '0' )
		return sprintf(data, "1 IPS");
	else if ( *arg == '1' )
		return sprintf(data, "1/2 IPS");
	else if ( *arg == '2' )
		return sprintf(data, "1/4 IPS");
	else if ( *arg == '3' )
		return sprintf(data, "1/8 IPS");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1 IPS;1/2 IPS;1/4 IPS;1/8 IPS");
	return -1;
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_ftpfileformat(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.ftp_config.ftpfileformat);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ftpmode(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->config.ftp_config.mode);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ftpjpegname(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%s", sys_info->config.ftp_config.jpegname);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_attfileformat(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.smtp_config.attfileformat);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_aviduration(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.aviduration);
	return sprintf(data, "%d", sys_info->ipcam[AVIDURATION].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_avidurationname(request *req, char *data, char *arg)
{
  	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	
	/* Alex, 2008.09.23, keep avi duration can be selected.
	if(pSysInfo->config.sdcard_config.sdinsert==1){
		if( *arg == '0' )		
		return sprintf(data, "--");
		else if( *arg == 'a' || *arg == '\0' )
			return sprintf(data, "--");
	}
	*/
	
	if ( *arg == '0' )
		return sprintf(data, "1");
	else if ( *arg == '1' )
		return sprintf(data, "5");
	else if ( *arg == '2' )
		return sprintf(data, "10");
	else if ( *arg == '3' )
		return sprintf(data, "20");
	else if ( *arg == '4' )
		return sprintf(data, "30");
	else if ( *arg == '5' )
		return sprintf(data, "60");
	else if ( *arg == '6' )
		return sprintf(data, "100");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1;5;10;20;30;60;100");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_aviformat(request *req, char *data, char *arg)
{
	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	//return sprintf(data, "%d", sys_info->config.lan_config.aviformat);
	return sprintf(data, "%d", sys_info->ipcam[AVIFORMAT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_aviformatname(request *req, char *data, char *arg)
{
  	/*SysInfo* pSysInfo = GetSysInfo();
	if(pSysInfo == NULL)
		return -1;*/
	int codecmode;
	int codecres;
	//codecmode=sys_info->config.lan_config.nVideocodecmode;
	//codecres=sys_info->config.lan_config.nVideocodecres;
	codecmode=sys_info->ipcam[NVIDEOCODECMODE].config.u8;
	codecres=sys_info->ipcam[NVIDEOCODECRES].config.u8;
	if(codecmode==3){
		if ( *arg == '0' )
		return sprintf(data, "MPEG4(720)");
		else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "MPEG4(720)");
	}
	else if(codecmode==2){
		if ( *arg == '0' )
		return sprintf(data, "MPEG4(720)");
		else if ( *arg == '1' )
		return sprintf(data, "MPEG4(CIF)");
		else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "MPEG4(720);MPEG4(CIF)");
	}
	else if(codecmode==1){
		if(codecres==1){
			if ( *arg == '0' )
				return sprintf(data, "MPEG4(720)");			
			else if ( *arg == 'a' || *arg == '\0' )
				return sprintf(data, "MPEG4(720)");
		}
		else if(codecres==0){
			if ( *arg == '0' )
				return sprintf(data, "MPEG4(VGA)");			
			else if ( *arg == 'a' || *arg == '\0' )
				return sprintf(data, "MPEG4(VGA)");
		}
	}
	else if(codecmode==0){
		if ( *arg == '0' )
			return sprintf(data, "MPEG4(720)");
		else if ( *arg == '1' )
			return sprintf(data, "MPEG4(CIF)");			
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, "MPEG4(720);MPEG4(CIF)");
	}	
	return -1;
}
#endif

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdleft(request *req, char *data, char *arg)
{
  long long freespace;
  freespace = GetDiskfreeSpace(SD_MOUNT_PATH);
  return sprintf(data, "%lld", freespace);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdused(request *req, char *data, char *arg)
{
  long long usedspace;
  usedspace = GetDiskusedSpace(SD_MOUNT_PATH);
  return sprintf(data, "%lld", usedspace);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_cardrewrite(request *req, char *data, char *arg)
{
  //return sprintf(data, "%d", sys_info->config.sdcard.rewrite);
  return sprintf(data, "%d", sys_info->ipcam[SDCARD_REWRITE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdduration(request *req, char *data, char *arg)
{
	// This is NOT correct. It should RENAME the parser 'aviduration' to 'sdduration' .
	// Alex . 2009.05.27
	//return sprintf(data, "%d", sys_info->config.lan_config.aviduration);
	return sprintf(data, "%d", sys_info->ipcam[AVIDURATION].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdrecordtype(request *req, char *data, char *arg)
{
	// This is NOT correct. It should RENAME the parser 'sdfileformat' to 'sdrecordtype' .
	// Alex . 2009.05.27
	//return sprintf(data, "%d", sys_info->config.sdcard.fileformat);
	return sprintf(data, "%d", sys_info->ipcam[SDCARD_FILEFORMAT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdrate(request *req, char *data, char *arg)
{
	// This is NOT correct. It should RENAME the parser 'jpegcontfreq' to 'sdrate' .
	// Alex . 2009.05.27
	//return sprintf(data, "%d", sys_info->config.sdcard.jpg_recrate);
	return sprintf(data, "%d", sys_info->ipcam[SDCARD_JPG_RECRATE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdcount(request *req, char *data, char *arg)
{
	// CAN NOT WORK TEMPORARILY
	// Alex . 2009.05.27
	return sprintf(data, "%d", 0);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxsdrecord(request *req, char *data, char *arg)
{
	// CAN NOT WORK TEMPORARILY
	// Alex . 2009.05.27
	return sprintf(data, "%d", 0);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sddurationname(request *req, char *data, char *arg)
{
	// This is NOT correct. It should RENAME the parser 'avidurationname' to 'sddurationname' .
	// Alex . 2009.05.27
	if ( *arg == '0' )
		return sprintf(data, "1");
	else if ( *arg == '1' )
		return sprintf(data, "5");
	else if ( *arg == '2' )
		return sprintf(data, "10");
	else if ( *arg == '3' )
		return sprintf(data, "20");
	else if ( *arg == '4' )
		return sprintf(data, "30");
	else if ( *arg == '5' )
		return sprintf(data, "60");
	else if ( *arg == '6' )
		return sprintf(data, "100");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1;5;10;20;30;60;100");
	return -1;

}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionblock(request *req, char *data, char *arg)
{

	//return sprintf(data, "%s", sys_info->config.motion_config.motionblock);
	return sprintf(data, "%s", conf_string_read(MOTION_CONFIG_MOTIONBLOCK));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionarea(request *req, char *data, char *arg)
{
	int i, num = 0;
	__u32 ret = 0, cnt = 0;
	char start_x[5]="", start_y[5]="", end_x[5]="", end_y[5]="";
	char temp[17];
	char *ptr = data;
	for(i=0;i<10;i++){
		sscanf(conf_string_read(MOTION_CONFIG_MOTIONREGION0+i),"%s", temp);
		strncpy(start_x, temp, 4);
		strncpy(start_y, temp+4, 4);
		strncpy(end_x, temp+8, 4);
		strncpy(end_y, temp+12, 4);
		cnt = sprintf(ptr, "%s:%s:%s:%s", start_x, start_y, end_x, end_y);
		ptr += cnt;
		ret += cnt;
		if(i == 9)
			break;
		if(num == 0){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		num++;
	}
	return ret;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionenable(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.motion_config.motionenable);
	return sprintf(data, "%d", sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motiondrawenable(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.motion_config.motionenable);
	return sprintf(data, "%d", sys_info->ipcam[MOTION_CONFIG_MOTION_DRAW_ENABLE].config.u8);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionlevel(request *req, char *data, char *arg)
{
   return sprintf(data, "%d", 0);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionpercentage(request *req, char *data, char *arg)
{
   //return sprintf(data, "%d", sys_info->config.motion_config.motionpercentage);
   return sprintf(data, "%d", sys_info->ipcam[MOTION_CONFIG_MOTIONPERCENTAGE].config.u8);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionsensitivity(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.motion_config.motionlevel);
	return sprintf(data, "%d", sys_info->ipcam[MOTION_CONFIG_MOTIONLEVEL].config.u8);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Lowest");
	else if ( *arg == '1' )
		return sprintf(data, "Low");
	else if ( *arg == '2' )
		return sprintf(data, "Medium");
	else if ( *arg == '3' )
		return sprintf(data, "High");
	else if ( *arg == '4' )
		return sprintf(data, "Highest");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Lowest;Low;Medium;High;Highest");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motioncenable(request *req, char *data, char *arg)
{

	//return sprintf(data, "%d", sys_info->config.motion_config.motioncenable);
	return sprintf(data, "%d", sys_info->ipcam[MOTION_CONFIG_MOTIONCENABLE].config.u8);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ipncptz(request *req, char *data, char *arg)
{
	return 0;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motioncvalue(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.motion_config.motioncvalue);
	return sprintf(data, "%d", sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].config.u16);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_minmotionthreshold(request *req, char *data, char *arg)
{
	return sprintf(data, "1");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxmotionthreshold(request *req, char *data, char *arg)
{
	return sprintf(data, "99");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int para_lostalarm(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.lostalarm);
	return sprintf(data, "%d", sys_info->ipcam[LOSTALARM].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdaenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.bSdaEnable);
	return sprintf(data, "%d", sys_info->ipcam[BSDAENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_aftpenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.bAFtpEnable);
	return sprintf(data, "%d", sys_info->ipcam[BAFTPENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_asmtpenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.bASmtpEnable);
	return sprintf(data, "%d", sys_info->ipcam[BASMTPENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_recordduration(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "0 seconds");
	else if ( *arg == '1' )
		return sprintf(data, "30 seconds");
	else if ( *arg == '2' )
		return sprintf(data, "1 minute");
	else if ( *arg == '3' )
		return sprintf(data, "5 minutes");
	else if ( *arg == '4' )
		return sprintf(data, "10 minutes");
	else if ( *arg == '5' )
		return sprintf(data, "Non-Stop");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "0 seconds;30 seconds;1 minute;5 minutes;10 minutes;Non-Stop");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smtpminattach(request *req, char *data, char *arg)
{
  return sprintf(data, "1");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smtpmaxattach(request *req, char *data, char *arg)
{
  return sprintf(data, "20");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_asmtpattach(request *req, char *data, char *arg)
{
    /*SysInfo* pSysInfo = GetSysInfo();

	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.smtp_config.attachments);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rftpenable(request *req, char *data, char *arg)
{
    /*SysInfo* pSysInfo = GetSysInfo();

	if(pSysInfo == NULL)
		return -1;*/
	return sprintf(data, "%d", sys_info->config.ftp_config.rftpenable);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_sdrenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.sdcard.enable_rec);
	return sprintf(data, "%d", sys_info->ipcam[SDCARD_ENABLE_REC].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_alarmduration(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.nAlarmDuration);
	return sprintf(data, "%d", sys_info->ipcam[NALARMDURATION].config.u8);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionxlimit(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 1280);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionxblock(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 12);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motionyblock(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 8);
}

/***************************************************************************
*
*	Includes special profile.
*
***************************************************************************/
int para_supportstream1(request *req, char *data, char *arg)
{
#if (MAX_PROFILE_NUM >= 1)
#ifdef SUPPORT_PROFILE_NUMBER
		if(sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value >= 1)
			return sprintf(data, "1");
		else
			return sprintf(data, "0");
#else
		return sprintf(data, "1");
#endif
#else
		return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportstream2(request *req, char *data, char *arg)
{
#if (MAX_PROFILE_NUM >= 2)
#ifdef SUPPORT_PROFILE_NUMBER
		if(sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value >= 2)
			return sprintf(data, "1");
		else
			return sprintf(data, "0");
#else
		return sprintf(data, "1");
#endif

#else
		return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportstream3(request *req, char *data, char *arg)
{
#if (MAX_PROFILE_NUM >= 3)
#ifdef SUPPORT_PROFILE_NUMBER
		if(sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value >= 3)
			return sprintf(data, "1");
		else
			return sprintf(data, "0");
#else
		return sprintf(data, "1");
#endif

#else
		return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportstream4(request *req, char *data, char *arg)
{
#if (MAX_PROFILE_NUM >= 4)
#ifdef SUPPORT_PROFILE_NUMBER
	if(sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value >= 4)
		return sprintf(data, "1");
	else
		return sprintf(data, "0");
#else
	return sprintf(data, "1");
#endif
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_numprofile(request *req, char *data, char *arg)
{
#if SUPPORT_MG1264
	return sprintf(data, "%d", MAX_PROFILE_NUM+1);
#else
#ifdef SUPPORT_PROFILE_NUMBER
	return sprintf(data, "%d", sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value);
#else
	return sprintf(data, "%d", MAX_PROFILE_NUM);
#endif
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_webprofilenum(request *req, char *data, char *arg)
{
#ifdef SUPPORT_PROFILE_NUMBER
	return sprintf(data, "%d", sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value);
#else
	return sprintf(data, "%d", MAX_WEB_PROFILE_NUM);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
extern unsigned char gl_TIflag;

int para_image2a(request *req, char *data, char *arg)
{
	//unsigned char tmp = sys_info->config.lan_config.nAEWswitch;
	unsigned char tmp = sys_info->ipcam[NAEWSWITCH].config.u8;
	if (tmp >= 4)
		tmp = tmp % 4;
	return sprintf(data, "%d", tmp);
}
int para_img2aname(request *req, char *data, char *arg)
{
	//if (sys_info->config.lan_config.nAEWswitch >= 3 && gl_TIflag == 0)
	if (sys_info->ipcam[NAEWSWITCH].config.u8 >= 3 && gl_TIflag == 0)
		gl_TIflag = 1;
	if ( *arg == '0' )
		return sprintf(data, "OFF");
	else if ( *arg == '1' )
		return sprintf(data, "APPRO");
	else if ( *arg == '2' )
		return sprintf(data, "OTHERS");
	else if ( *arg == '3' && gl_TIflag == 1 )
		return sprintf(data, "TI");
	else if ( *arg == 'a' || *arg == '\0' ) {
		if (gl_TIflag == 1)
			return sprintf(data, "OFF;APPRO;OTHERS;TI");
		else
			return sprintf(data, "OFF;APPRO;OTHERS");
	}
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_kernelversion(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", get_kernel_version());
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_biosversion(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", get_bios_version());
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_compiletime(request *req, char *data, char *arg)
{
	return sprintf(data, __DATE__ ", " __TIME__);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_waitserver(request *req, char *data, char *arg)
{
	return sprintf(data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportcolorkiller(request *req, char *data, char *arg)
{
#ifdef SUPPORT_COLORKILLER
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportAWB(request *req, char *data, char *arg)
{
#ifdef SUPPORT_AWB
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportbrightness(request *req, char *data, char *arg)
{
#ifdef SUPPORT_BRIGHTNESS
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportcontrast(request *req, char *data, char *arg)
{
#ifdef SUPPORT_CONTRAST
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportsaturation(request *req, char *data, char *arg)
{
#ifdef SUPPORT_SATURATION
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportbacklight(request *req, char *data, char *arg)
{
#ifdef SUPPORT_BACKLIGHT
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
int para_supportsharpness(request *req, char *data, char *arg)
{
#ifdef SUPPORT_SHARPNESS
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_motion_blk(request *req, char *data, char *arg)
{
	//strncpy(data, sys_info->config.motion_config.motionblock,MOTION_BLK_LEN);
	strncpy(data, conf_string_read(MOTION_CONFIG_MOTIONBLOCK), MAX_MOTION_BLK);
	return strlen(data);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportgiooutalwayson(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", 1);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int para_supportgioin(request *req, char *data, char *arg)
{

	return sprintf(data, "%d", GIOIN_NUM);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportgioout(request *req, char *data, char *arg)
{

	return sprintf(data, "%d", GIOOUT_NUM);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
int para_allgioinenable(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->config.lan_config.gioin_main_en);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
int para_allgiooutenable(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->config.lan_config.gioout_main_en);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_gioinenable(request *req, char *data, char *arg)
{
	int value = atoi(arg);
	
	//if(sys_info->config.support_config.supportgioin == 0)
	//if(sys_info->ipcam[SUPPORTGIOIN].config.u8 == 0)
	if(GIOIN_NUM == 0)
		return -1;
	
	if ( *arg == '\0')
		return sprintf(data, "%d", sys_info->ipcam[GIOIN1_ENABLE].config.u16);
		//return sprintf(data, "%d", sys_info->config.lan_config.gioin[1].enable);

	//if(value > 0 && value <= sys_info->config.support_config.supportgioin)
		//return sprintf(data, "%d", sys_info->config.lan_config.gioin[value].enable);
	if(value > 0 && value <= GIOIN_NUM)
		return sprintf(data, "%d", sys_info->ipcam[GIOIN0_ENABLE+value*4].config.u16);

	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_giointype(request *req, char *data, char *arg)
{
	int value = atoi(arg);

	//if(sys_info->config.support_config.supportgioin == 0)
	//if(sys_info->ipcam[SUPPORTGIOIN].config.u8 == 0)
	if(GIOIN_NUM == 0)
		return -1;	

	if ( *arg == '\0')
		return sprintf(data, "%d", sys_info->ipcam[GIOIN1_TYPE].config.u16);
		//return sprintf(data, "%d", sys_info->config.lan_config.gioin[1].type);

	//if(value > 0 && value <= sys_info->config.support_config.supportgioin)
		//return sprintf(data, "%d", sys_info->config.lan_config.gioin[value].type);
	if(value > 0 && value <= GIOIN_NUM)	
		return sprintf(data, "%d", sys_info->ipcam[GIOIN0_TYPE+value*4].config.u16);
	
	
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_gioinname(request *req, char *data, char *arg)
{
#if defined( IO_USE_LOGIC )
	if ( *arg == '0' )
		return sprintf(data, "Low");
	else if ( *arg == '1' )
		return sprintf(data, "High");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Low;High");
#elif defined( IO_USE_RELAY )
	if ( *arg == '0' )
		return sprintf(data, "N.O.");
	else if ( *arg == '1' )
		return sprintf(data, "N.C.");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "N.O.;N.C.");
#else
#error Please Define IO_USE_LOGIC or IO_USE_RELAY
#endif
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_gioinstatus(request *req, char *data, char *arg)
{
#ifdef SUPPORT_VISCA
	if ( *arg == '\0' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin1 );
	else if ( *arg == '2' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin2 );
	else if ( *arg == '3' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin3 );
	else if ( *arg == '4' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin4 );
	else if ( *arg == '5' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin5 );
	else if ( *arg == '6' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin6 );
	else if ( *arg == '7' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin7 );
	else if ( *arg == '8' )
		return sprintf( data, "%d", sys_info->vsalarm.alarmin8 );
	else
		return -1;
#else
    // 0 : Close
    // 1 : Open
    int i;
    int gio[GIOIN_NUM] = GIOIN_ARRAYLIST;

    if ( arg && *arg )
    {
        if ( ( i = atoi( arg ) ) == 0 )
            return -1;
    }
    else
        i = 1;
    if ( i <= GIOIN_NUM )
        return sprintf( data, "%d", gio_read( gio[i - 1] ) ^ !GIOIN_INV );
    else
        return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_giooutstatus(request *req, char *data, char *arg)
{
    int i;
    int gio[GIOOUT_NUM] = GIOOUT_ARRAYLIST;

    if ( arg && *arg )
    {
        if ( ( i = atoi( arg ) ) == 0 )
            return -1;
    }
    else
        i = 1;
    if ( i <= GIOOUT_NUM )
        return sprintf( data, "%d", gio_read( gio[i - 1] ) ^ GIOIN_INV );
    else
        return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_giooutenable(request *req, char *data, char *arg)
{
	int value = atoi(arg);

	//if(sys_info->config.support_config.supportgioout == 0)
	//if(sys_info->ipcam[SUPPORTGIOOUT].config.u8 == 0)
	if(GIOOUT_NUM == 0)
		return -1;

	if ( *arg == '\0' )
		//return sprintf(data, "%d", sys_info->config.lan_config.gioout[1].enable);
		return sprintf(data, "%d", sys_info->ipcam[GIOOUT1_ENABLE].config.u16);

	//if(value > 0 && value < sys_info->config.support_config.supportgioout)
	if(value > 0 && value <= GIOOUT_NUM)
		//return sprintf(data, "%d", sys_info->config.lan_config.gioout[value].enable);
		return sprintf(data, "%d", sys_info->ipcam[GIOOUT0_ENABLE+value*4].config.u16);

	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_gioouttype(request *req, char *data, char *arg)
{
	int value = atoi(arg);

	//if(sys_info->config.support_config.supportgioout == 0)
	//if(sys_info->ipcam[SUPPORTGIOOUT].config.u8 == 0)
	if(GIOOUT_NUM == 0)
		return -1;	
	
	if ( *arg == '\0' )
	  //return sprintf(data, "%d", sys_info->config.lan_config.gioout[1].type);
	  return sprintf(data, "%d", sys_info->ipcam[GIOOUT1_TYPE].config.u16);

	//if(value > 0 && value < sys_info->config.support_config.supportgioout)
	//	return sprintf(data, "%d", sys_info->config.lan_config.gioout[value].type);
	if(value > 0 && value <= GIOOUT_NUM)
		return sprintf(data, "%d", sys_info->ipcam[GIOOUT0_TYPE+value*4].config.u16);
	
	return -1;

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_giooutname(request *req, char *data, char *arg)
{
#if defined( IO_USE_LOGIC )
	if ( *arg == '0' )
		return sprintf(data, "Low");
	else if ( *arg == '1' )
		return sprintf(data, "High");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Low;High");
#elif defined( IO_USE_RELAY )
	if ( *arg == '0' )
		return sprintf(data, "N.O.");
	else if ( *arg == '1' )
		return sprintf(data, "N.C.");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "N.O.;N.C.");
#else
#error Please Define IO_USE_LOGIC or IO_USE_RELAY
#endif
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_softwareversion(request *req, char *data, char *arg)
{
	return sprintf(data, "%d.%02d", APP_VERSION_MAJOR, APP_VERSION_MINOR);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_fullversion(request *req, char *data, char *arg)
{
	if (get_brand_id() == BRAND_ID_TYCO)
		return sprintf(data, "%d.%02d", APP_VERSION_MAJOR, APP_VERSION_MINOR);
	return sprintf(data, "%d.%02d.%02d", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_SUB_MINOR);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tstampenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.tstampenable);
	return sprintf(data, "%d", sys_info->ipcam[TSTAMPENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tstampcolorname(request *req, char *data, char *arg)
{
  if ( *arg == '0' )
		return sprintf(data, "WHITE");
	if ( *arg == '1' )
		return sprintf(data, "BLACK");
	if ( *arg == '2' )
		return sprintf(data, "GREEN");
	if ( *arg == '3' )
		return sprintf(data, "BLUE");
	if ( *arg == '4' )
		return sprintf(data, "ORANGE");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "WHITE;BLACK;GREEN;BLUE;ORANGE");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_pppoemodename(request *req, char *data, char *arg)
{
  if ( *arg == '0' )
		return sprintf(data, "OFF");
	if ( *arg == '1' )
		return sprintf(data, "ON");
	if ( *arg == '2' )
		return sprintf(data, "TEST");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "OFF;ON;TEST");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnsenable(request *req, char *data, char *arg)
{
    //return sprintf(data, "%d", sys_info->config.ddns.enable);
    return sprintf(data, "%d", sys_info->ipcam[DDNS_ENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnstype(request *req, char *data, char *arg)
{
/*
#ifdef CONFIG_BRAND_DLINK
	//if(sys_info->config.ddns.type >= 0)
		//return sprintf(data, "%d", sys_info->config.ddns.type + 1);
	if(sys_info->ipcam[DDNS_TYPE].config.u8 >= 0)
		return sprintf(data, "%d", sys_info->ipcam[DDNS_TYPE].config.u8 + 1);
	else
		return sprintf(data, "%d", 1);
#else
	//return sprintf(data, "%d", sys_info->config.ddns.type);
*/
    return sprintf(data, "%d", sys_info->ipcam[DDNS_TYPE].config.u8);

//#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnstypename(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
	if ( *arg == '0' )
		return sprintf(data, ";www.dlinkddns.com");
	if ( *arg == '1' )
		return sprintf(data, ";www.DynDNS.org");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, ";www.dlinkddns.com;www.DynDNS.org");
#else
	if ( *arg == '0' )
		return sprintf(data, ";www.DynDNS.org");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, ";www.DynDNS.org");
#endif
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnsinterval(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.ddns.interval);
	return sprintf(data, "%d", sys_info->ipcam[A_DDNS_INTERVAL].config.u32);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxddnsusrlen(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", MAX_DDNS_ID_LEN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxddnspwdlen(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", MAX_DDNS_PWD_LEN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnshostname(request *req, char *data, char *arg)
{
  //return sprintf(data, "%s", sys_info->config.ddns.hostname);
  return sprintf(data, "%s", conf_string_read(DDNS_HOSTNAME));
}
#if 0  /* Disable by Alex, 2009.2.19 */
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnswebname(request *req, char *data, char *arg)
{
    /*SysInfo* pSysInfo = GetSysInfo();
    if(pSysInfo == NULL)
        return -1;*/
        
    return sprintf(data, "%s", sys_info->config.ddns.webname);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ipfilterenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.ipfilterenable);
	return sprintf(data, "%d", sys_info->ipcam[IPFILTERENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ipfilterdefaultpolicy(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.ipfilterdefaultpolicy);
	return sprintf(data, "%d", sys_info->ipcam[IPFILTERDEFAULTPOLICY].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxipfilterpolicy(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.maxipfilterpolicy);
	return sprintf(data, "%d", IPFILTER_MAX_POLICY);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ipfilterpolicy(request *req, char *data, char *arg)
{
#ifndef SUPPORT_NEW_IPFILTER
  if ( *arg == '0' )
		return sprintf(data, "%s%s%s",sys_info->ipcam[IP_FILTER0_POLICY].config.value == -1 ? "" : (sys_info->ipcam[IP_FILTER0_POLICY].config.value == 0 ? "deny" : "allow") ,
			 sys_info->ipcam[IP_FILTER0_POLICY].config.value == -1 ? "":":" ,
			 	 sys_info->ipcam[IP_FILTER0_POLICY].config.value == -1 ? "": conf_string_read(IP_FILTER0_IP));

	if ( *arg == '1' )
		return sprintf(data, "%s%s%s",sys_info->ipcam[IP_FILTER1_POLICY].config.value == -1 ? "" : (sys_info->ipcam[IP_FILTER1_POLICY].config.value == 0 ? "deny" : "allow") ,
			 sys_info->ipcam[IP_FILTER1_POLICY].config.value == -1 ? "":":" ,
			 	 sys_info->ipcam[IP_FILTER1_POLICY].config.value == -1 ? "": conf_string_read(IP_FILTER1_IP));

	if ( *arg == '2' )
		return sprintf(data, "%s%s%s",sys_info->ipcam[IP_FILTER2_POLICY].config.value == -1 ? "" : (sys_info->ipcam[IP_FILTER2_POLICY].config.value == 0 ? "deny" : "allow") ,
			 sys_info->ipcam[IP_FILTER2_POLICY].config.value == -1 ? "":":" ,
			 	 sys_info->ipcam[IP_FILTER2_POLICY].config.value == -1 ? "": conf_string_read(IP_FILTER2_IP));

	if ( *arg == '3' )
		return sprintf(data, "%s%s%s",sys_info->ipcam[IP_FILTER3_POLICY].config.value == -1 ? "" : (sys_info->ipcam[IP_FILTER3_POLICY].config.value == 0 ? "deny" : "allow") ,
			 sys_info->ipcam[IP_FILTER3_POLICY].config.value == -1 ? "":":" ,
			 	 sys_info->ipcam[IP_FILTER3_POLICY].config.value == -1 ? "": conf_string_read(IP_FILTER3_IP));

	if ( *arg == '4' )
		return sprintf(data, "%s%s%s",sys_info->ipcam[IP_FILTER4_POLICY].config.value == -1 ? "" : (sys_info->ipcam[IP_FILTER4_POLICY].config.value == 0 ? "deny" : "allow") ,
			 sys_info->ipcam[IP_FILTER4_POLICY].config.value == -1 ? "":":" ,
			 	 sys_info->ipcam[IP_FILTER4_POLICY].config.value == -1 ? "": conf_string_read(IP_FILTER4_IP));

	if ( *arg == '5' )
		return sprintf(data, "%s%s%s",sys_info->ipcam[IP_FILTER5_POLICY].config.value == -1 ? "" : (sys_info->ipcam[IP_FILTER5_POLICY].config.value == 0 ? "deny" : "allow") ,
			 sys_info->ipcam[IP_FILTER5_POLICY].config.value == -1 ? "":":" ,
			 	 sys_info->ipcam[IP_FILTER5_POLICY].config.value == -1 ? "": conf_string_read(IP_FILTER5_IP));

	if ( *arg == '6' )
		return sprintf(data, "%s%s%s",sys_info->ipcam[IP_FILTER6_POLICY].config.value == -1 ? "" : (sys_info->ipcam[IP_FILTER6_POLICY].config.value == 0 ? "deny" : "allow") ,
			 sys_info->ipcam[IP_FILTER6_POLICY].config.value == -1 ? "":":" ,
			 	 sys_info->ipcam[IP_FILTER6_POLICY].config.value == -1 ? "": conf_string_read(IP_FILTER6_IP));

#if 0
  if ( *arg == '0' )
		return sprintf(data, "%s%s%s",sys_info->config.lan_config.net.ip_filter_table[0].policy == -1 ? "" : (sys_info->config.lan_config.net.ip_filter_table[0].policy == 0 ? "deny" : "allow") ,
			 sys_info->config.lan_config.net.ip_filter_table[0].policy == -1 ? "":":" ,
			 	 sys_info->config.lan_config.net.ip_filter_table[0].policy == -1 ? "":sys_info->config.lan_config.net.ip_filter_table[0].ip);

	if ( *arg == '1' )
		return sprintf(data, "%s%s%s",sys_info->config.lan_config.net.ip_filter_table[1].policy == -1 ? "" : (sys_info->config.lan_config.net.ip_filter_table[1].policy == 0 ? "deny" : "allow") ,
			 sys_info->config.lan_config.net.ip_filter_table[1].policy == -1 ? "":":" ,
			 	 sys_info->config.lan_config.net.ip_filter_table[1].policy == -1 ? "":sys_info->config.lan_config.net.ip_filter_table[1].ip);

	if ( *arg == '2' )
		return sprintf(data, "%s%s%s",sys_info->config.lan_config.net.ip_filter_table[2].policy == -1 ? "" : (sys_info->config.lan_config.net.ip_filter_table[2].policy == 0 ? "deny" : "allow") ,
			 sys_info->config.lan_config.net.ip_filter_table[2].policy == -1 ? "":":" ,
			 	 sys_info->config.lan_config.net.ip_filter_table[2].policy == -1 ? "":sys_info->config.lan_config.net.ip_filter_table[2].ip);

	if ( *arg == '3' )
		return sprintf(data, "%s%s%s",sys_info->config.lan_config.net.ip_filter_table[3].policy == -1 ? "" : (sys_info->config.lan_config.net.ip_filter_table[3].policy == 0 ? "deny" : "allow") ,
			 sys_info->config.lan_config.net.ip_filter_table[3].policy == -1 ? "":":" ,
			 	 sys_info->config.lan_config.net.ip_filter_table[3].policy == -1 ? "":sys_info->config.lan_config.net.ip_filter_table[3].ip);

	if ( *arg == '4' )
		return sprintf(data, "%s%s%s",sys_info->config.lan_config.net.ip_filter_table[4].policy == -1 ? "" : (sys_info->config.lan_config.net.ip_filter_table[4].policy == 0 ? "deny" : "allow") ,
			 sys_info->config.lan_config.net.ip_filter_table[4].policy == -1 ? "":":" ,
			 	 sys_info->config.lan_config.net.ip_filter_table[4].policy == -1 ? "":sys_info->config.lan_config.net.ip_filter_table[4].ip);

	if ( *arg == '5' )
		return sprintf(data, "%s%s%s",sys_info->config.lan_config.net.ip_filter_table[5].policy == -1 ? "" : (sys_info->config.lan_config.net.ip_filter_table[5].policy == 0 ? "deny" : "allow") ,
			 sys_info->config.lan_config.net.ip_filter_table[5].policy == -1 ? "":":" ,
			 	 sys_info->config.lan_config.net.ip_filter_table[5].policy == -1 ? "":sys_info->config.lan_config.net.ip_filter_table[5].ip);

	if ( *arg == '6' )
		return sprintf(data, "%s%s%s",sys_info->config.lan_config.net.ip_filter_table[6].policy == -1 ? "" : (sys_info->config.lan_config.net.ip_filter_table[6].policy == 0 ? "deny" : "allow") ,
			 sys_info->config.lan_config.net.ip_filter_table[6].policy == -1 ? "":":" ,
			 	 sys_info->config.lan_config.net.ip_filter_table[6].policy == -1 ? "":sys_info->config.lan_config.net.ip_filter_table[6].ip);
#endif
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, " ");
	return -1;
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ipfilterallow(request *req, char *data, char *arg)
{
#ifdef SUPPORT_NEW_IPFILTER
	int i;
	__u32 cnt = 0, ret = 0;;
	char *ptr = data;
	char ip_start[16] = "";
	char ip_end[16] = "";
	//IP_FILTER_TABLE * ptable;

	// *arg = '0' ~ '9'
	if(isdigit(*arg)) {
		// conver ascii to integer
		int id = *arg - 0x30;
		//ptable = &sys_info->config.lan_config.net.allow_table[id];
		
		//if( ptable->start.s_addr && ptable->end.s_addr ) {
			//strncpy(ip_start, inet_ntoa(ptable->start), sizeof(ip_start)-1);
			//strncpy(ip_end, inet_ntoa(ptable->end), sizeof(ip_end)-1);
			
		if( sys_info->ipcam[ALLOW_TABLE0_START+id*2].config.ip.s_addr && sys_info->ipcam[ALLOW_TABLE0_END+id*2].config.ip.s_addr ) {
			strncpy(ip_start, inet_ntoa(sys_info->ipcam[ALLOW_TABLE0_START+id*2].config.ip), sizeof(ip_start)-1);
			strncpy(ip_end, inet_ntoa(sys_info->ipcam[ALLOW_TABLE0_END+id*2].config.ip), sizeof(ip_end)-1);
			return sprintf(data, "%s ~ %s", ip_start, ip_end);
		}
		else {
			return -1;
		}
	}
	else if ( *arg == 'a' || *arg == '\0' ) {
		
		for( i = 0 ; i < MAX_IPFILTER_NUM; i++) {
			//ptable = &sys_info->config.lan_config.net.allow_table[i];
			//if( !ptable->start.s_addr && !ptable->end.s_addr )
			if( !sys_info->ipcam[ALLOW_TABLE0_START+i*2].config.ip.s_addr && !sys_info->ipcam[ALLOW_TABLE0_START+i*2].config.ip.s_addr  )
				continue;

			if(ret && i < MAX_IPFILTER_NUM) {
				*ptr++ = ';';
				ret++;
			}

			strncpy(ip_start, inet_ntoa(sys_info->ipcam[ALLOW_TABLE0_START+i*2].config.ip), sizeof(ip_start)-1);
			strncpy(ip_end, inet_ntoa(sys_info->ipcam[ALLOW_TABLE0_END+i*2].config.ip), sizeof(ip_end)-1);
			//strncpy(ip_start, inet_ntoa(ptable->start), sizeof(ip_start)-1);
			//strncpy(ip_end, inet_ntoa(ptable->end), sizeof(ip_end)-1);
			cnt = sprintf(ptr, "%s ~ %s", ip_start, ip_end);
			ptr += cnt;
			ret += cnt;
		}
	}

	return ret;

#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ipfilterdeny(request *req, char *data, char *arg)
{
#ifdef SUPPORT_NEW_IPFILTER
	int i;
	__u32 cnt = 0, ret = 0;;
	char *ptr = data;
	char ip_start[16] = "";
	char ip_end[16] = "";
	
	//IP_FILTER_TABLE * ptable;

	// *arg = '0' ~ '7'
	if(isdigit(*arg)) {
		// conver ascii to integer
		int id = *arg - 0x30;
		//ptable = &sys_info->config.lan_config.net.deny_table[id];
		
		//if( ptable->start.s_addr && ptable->end.s_addr ) {
			//strncpy(ip_start, inet_ntoa(ptable->start), sizeof(ip_start));
			//strncpy(ip_end, inet_ntoa(ptable->end), sizeof(ip_end));
		if( sys_info->ipcam[DENY_TABLE0_START+id*2].config.ip.s_addr && sys_info->ipcam[DENY_TABLE0_END+id*2].config.ip.s_addr ) {
			strncpy(ip_start, inet_ntoa(sys_info->ipcam[DENY_TABLE0_START+id*2].config.ip), sizeof(ip_start));
			strncpy(ip_end, inet_ntoa(sys_info->ipcam[DENY_TABLE0_END+id*2].config.ip), sizeof(ip_end));
			return sprintf(data, "%s ~ %s", ip_start, ip_end);
		}
		else {
			return -1;
		}
	}
	else if ( *arg == 'a' || *arg == '\0' ) {
		
		for( i = 0 ; i < MAX_IPFILTER_NUM; i++) {
			//ptable = &sys_info->config.lan_config.net.deny_table[i];
			//if( !ptable->start.s_addr && !ptable->end.s_addr )
			if( !sys_info->ipcam[DENY_TABLE0_START+i*2].config.ip.s_addr && !sys_info->ipcam[DENY_TABLE0_END+i*2].config.ip.s_addr )
				continue;

			if(ret && i < MAX_IPFILTER_NUM) {
				*ptr++ = ';';
				ret++;
			}

			strncpy(ip_start, inet_ntoa(sys_info->ipcam[DENY_TABLE0_START+i*2].config.ip), sizeof(ip_start)-1);
			strncpy(ip_end, inet_ntoa(sys_info->ipcam[DENY_TABLE0_END+i*2].config.ip), sizeof(ip_end)-1);
			//strncpy(ip_start, inet_ntoa(ptable->start), sizeof(ip_start)-1);
			//strncpy(ip_end, inet_ntoa(ptable->end), sizeof(ip_end)-1);
			cnt = sprintf(ptr, "%s ~ %s", ip_start, ip_end);
			ptr += cnt;
			ret += cnt;
		}
	}
	return ret;

#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxquotainbound(request *req, char *data, char *arg)
{
    return sprintf(data, "%d", MAX_TC_IN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxquotaoutbound(request *req, char *data, char *arg)
{
    return sprintf(data, "%d", MAX_TC_OUT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_quotainbound(request *req, char *data, char *arg)
{
    return sprintf(data, "%d", sys_info->ipcam[QUOTAINBOUND].config.u32);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_quotaoutbound(request *req, char *data, char *arg)
{
    return sprintf(data, "%d", sys_info->ipcam[QUOTAOUTBOUND].config.u32);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_virtaulprotocol(request *req, char *data, char *arg)
{
  if ( *arg == '0' )
		return sprintf(data, "TCP");
	if ( *arg == '1' )
		return sprintf(data, "UDP");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, " ");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_virtauldata(request *req, char *data, char *arg)
{
	int id = *arg - '0';

	  if ( *arg >= '0' && *arg <= '0'){
		if( sys_info->ipcam[LVS_TABLE0_PUBLICPORT+id*LVS_TABLE_STRUCT_SIZE].config.u32 )
				return sprintf(data, "%s:%s:%s:%d:%d", conf_string_read(LVS_TABLE0_NAME+id*LVS_TABLE_STRUCT_SIZE) ,
				conf_string_read(LVS_TABLE0_LVS+id*LVS_TABLE_STRUCT_SIZE),
				sys_info->ipcam[LVS_TABLE0_LVS_PROTOCOL+id*LVS_TABLE_STRUCT_SIZE].config.u8 ? "UDP" : "TCP",
				sys_info->ipcam[LVS_TABLE0_PRIVATEPORT+id*LVS_TABLE_STRUCT_SIZE].config.u32,
				sys_info->ipcam[LVS_TABLE0_PUBLICPORT+id*LVS_TABLE_STRUCT_SIZE].config.u32);
			else
				return sprintf(data, "0");
		}
		
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, " ");
		return -1;
		

#if 0
	VIRTUAL_SERVER_TABLE *tmp_table = sys_info->config.lan_config.net.lvs_table;
  /*SysInfo* pSysInfo = GetSysInfo();*/
  if ( *arg == '0' ){
  	if( tmp_table[0].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[0].name ,tmp_table[0].LVS,tmp_table[0].LVS_protocol ? "UDP" : "TCP",tmp_table[0].privateport,tmp_table[0].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '1' ){
		if( tmp_table[1].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[1].name ,tmp_table[1].LVS,tmp_table[1].LVS_protocol ? "UDP" : "TCP",tmp_table[1].privateport,tmp_table[1].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '2' ){
		if( tmp_table[2].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[2].name ,tmp_table[2].LVS,tmp_table[2].LVS_protocol ? "UDP" : "TCP",tmp_table[2].privateport,tmp_table[2].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '3' ){
		if( tmp_table[3].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[3].name ,tmp_table[3].LVS,tmp_table[3].LVS_protocol ? "UDP" : "TCP",tmp_table[3].privateport,tmp_table[3].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '4' ){
		if( tmp_table[4].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[4].name ,tmp_table[4].LVS,tmp_table[4].LVS_protocol ? "UDP" : "TCP",tmp_table[4].privateport,tmp_table[4].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '5' ){
		if( tmp_table[5].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[5].name ,tmp_table[5].LVS,tmp_table[5].LVS_protocol ? "UDP" : "TCP",tmp_table[5].privateport,tmp_table[5].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '6' ){
		if( tmp_table[6].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[6].name ,tmp_table[6].LVS,tmp_table[6].LVS_protocol ? "UDP" : "TCP",tmp_table[6].privateport,tmp_table[6].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '7' ){
		if( tmp_table[7].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[7].name ,tmp_table[7].LVS,tmp_table[7].LVS_protocol ? "UDP" : "TCP",tmp_table[7].privateport,tmp_table[7].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '8' ){
		if( tmp_table[8].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[8].name ,tmp_table[8].LVS,tmp_table[8].LVS_protocol ? "UDP" : "TCP",tmp_table[8].privateport,tmp_table[8].publicport);
		else
			return sprintf(data, "0");
	}
	if ( *arg == '9' ){
		if( tmp_table[9].publicport )
			return sprintf(data, "%s:%s:%s:%d:%d", tmp_table[9].name ,tmp_table[9].LVS,tmp_table[9].LVS_protocol ? "UDP" : "TCP",tmp_table[9].privateport,tmp_table[9].publicport);
		else
			return sprintf(data, "0");
	}
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, " ");
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_virtaulenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.net.LVS_enable);
	return sprintf(data, "%d", sys_info->ipcam[LVS_ENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnsaccount(request *req, char *data, char *arg)
{
        
    //return sprintf(data, "%s", sys_info->config.ddns.account);
    return sprintf(data, "%s", conf_string_read(DDNS_ACCOUNT));

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_ddnspassword(request *req, char *data, char *arg)
{
        
    //return sprintf(data, "%s", sys_info->config.ddns.password);
    return sprintf(data, "%s", conf_string_read(DDNS_PASSWORD));

}

#if 0 // NO MORE USED. disable by Alex. 2009.12.15 
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbenrecord(request *req, char *data, char *arg)
{  
#if SUPPORT_SAMBA
	return sprintf(data, "%d", sys_info->config.samba.enable_rec);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbauthority(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%d", sys_info->config.samba.authority);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbusername(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%s", sys_info->config.samba.username);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbpassword(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%s", sys_info->config.samba.password);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbserver(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%s", sys_info->config.samba.server);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbpath(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%s", sys_info->config.samba.path);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbprofile(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%d", 0);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbrewrite(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%d", sys_info->config.samba.rewrite);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbtype(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%d", sys_info->config.samba.source);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_smbeventtype(request *req, char *data, char *arg)
{
#if SUPPORT_SAMBA
	return sprintf(data, "%d", 0);
#else
	return -1;
#endif
}
#endif // NO MORE USED. disable by Alex. 2009.12.15 

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_testtimeout(request *req, char *data, char *arg)
{
#ifdef SUPPORT_EVENT_2G
	return sprintf(data, "20");
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_replytest(request *req, char *data, char *arg)
{
#ifdef SUPPORT_EVENT_2G
	return sprintf(data, "%d", sys_info->testserver);
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_recresolutionname(request *req, char *data, char *arg)
{
#if 1
	char* ptr = data;
	int ret=0, i, num=0, cnt;
	
	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 1)
			if ( *arg == '0' )
				return sprintf(data, "Profile 1");
	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 2)
			if ( *arg == '1' )
				return sprintf(data, "Profile 2");
	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 3)
			if ( *arg == '2' )
				return sprintf(data, "Profile 3");
	if(sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 4)
		dbg("sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >= 4");
	
	if ( *arg == 'a' || *arg == '\0' ){
		for( i = 0 ; i < sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value; i++) {
			cnt = 0;

			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}

			cnt += sprintf(ptr, "Profile %d", i+1);
			dbg("ptr=<%d>%s\n",i ,ptr);
			ptr+= cnt;
			ret += cnt;
			
		}
		return ret;
	}
	return -1;
#else

#if MAX_WEB_PROFILE_NUM == 2
    if ( *arg == '0' )
        return sprintf(data, "Profile 1");
    if ( *arg == '1' )
        return sprintf(data, "Profile 2");
    if ( *arg == 'a' || *arg == '\0' )
        return sprintf(data, "Profile 1;Profile 2");

#elif MAX_WEB_PROFILE_NUM == 3
      if ( *arg == '0' )
          return sprintf(data, "Profile 1");
      if ( *arg == '1' )
          return sprintf(data, "Profile 2");
      if ( *arg == '2' )
          return sprintf(data, "Profile 3");
      if ( *arg == 'a' || *arg == '\0' )
          return sprintf(data, "Profile 1;Profile 2;Profile 3");
      
#elif MAX_WEB_PROFILE_NUM == 4
    if ( *arg == '0' )
        return sprintf(data, "Profile 1");
    if ( *arg == '1' )
        return sprintf(data, "Profile 2");
    if ( *arg == '2' )
        return sprintf(data, "Profile 3");
    if ( *arg == '3' )
        return sprintf(data, "Profile 4");
    if ( *arg == 'a' || *arg == '\0' )
        return sprintf(data, "Profile 1;Profile 2;Profile 3;Profile 4");
#endif

      return -1;
#endif
}

/*#ifdef SUPPORT_EVENT_2G
int para_trgoutmotion(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_ALARM].en_motion);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
int para_trgoutdin1(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_ALARM].en_alarm1);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
int para_trgoutdin2(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_ALARM].en_alarm2);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
//#endif //SUPPORT_EVENT_2G



/* // NO MORE USED. disable by Alex. 2009.12.15 
#ifdef MODEL_LC7315
int para_trgoutenable(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.lan_config.gioout_main_en);
}

int para_trgjpgmotion(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_JPG].en_motion);
}

int para_trgjpgdin1(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_JPG].en_alarm1);
}

int para_trgjpgdin2(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_JPG].en_alarm2);
}

int para_trgavimotion(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_AVI].en_motion);
}

int para_trgavidin1(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_AVI].en_alarm1);
}


int para_trgavidin2(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.trigger[TRIGGER_AVI].en_alarm2);
}

int para_ensnapshot(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.lan_config.jpg_rec_main_en);
} 

int para_recresolution(request *req, char *data, char *arg)
{
  return sprintf(data, "%d", sys_info->config.sdcard.source);
}  

int para_recrewrite(request *req, char *data, char *arg)
{
      return sprintf(data, "%d", sys_info->config.sdcard.rewrite);
}


int para_record(request *req, char *data, char *arg)
{
      return sprintf(data, "%d", sys_info->config.lan_config.avi_rec_main_en);
}

int para_keeprecord(request *req, char *data, char *arg)
{
      return sprintf(data, "%d", sys_info->config.lan_config.avi_rec_mode & AVI_RECMODE_NOSTOP);
}

int para_eventrecord(request *req, char *data, char *arg)
{
      return sprintf(data, "%d", (sys_info->config.lan_config.avi_rec_mode & AVI_RECMODE_EVENT) >> 4 );
}

int para_schedulerec(request *req, char *data, char *arg)
{
      return sprintf(data, "%d", (sys_info->config.lan_config.avi_rec_mode & AVI_RECMODE_SCHEDULE) >> 8 );
}

int para_freespace(request *req, char *data, char *arg)
{
      return sprintf(data, "%d", sys_info->config.sdcard.disklimit);
}  

#endif
*/  // NO MORE USED. disable by Alex. 2009.12.15 

/*AICHI*/


  int para_rtspport(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%d", sys_info->config.lan_config.net.rtsp_port);
	return sprintf(data, "%d", sys_info->ipcam[RTSP_PORT].config.u16);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  int para_defaultrtspport(request *req, char *data, char *arg)
  {
	return sprintf(data, "%d", RTSP_PORT_DEFAULT);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_seconddnsip(request *req, char *data, char *arg)
  {
	struct in_addr ipaddr[MAX_DNS_NUM];

	memset(ipaddr, 0, sizeof(ipaddr));
	net_get_multi_dns(ipaddr);
	return sprintf(data, "%s", inet_ntoa(ipaddr[1]));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_setupnpenable(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%d", sys_info->config.lan_config.net.upnpenable);
	return sprintf(data, "%d", sys_info->ipcam[UPNPENABLE].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_setupnpcexport(request *req, char *data, char *arg)
  {
	return sprintf(data, "%d", 8554);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int papa_upnpforwardingactive(request *req, char *data, char *arg)
  {
  	//dbg("upnpfowd_status = %d\n", sys_info->config.lan_config.net.upnpfowd_status);
	//if(sys_info->config.lan_config.net.upnpfowd_status != 0)
	return sprintf(data, "%d", sys_info->status.upnpportforewarding);
	/*if(sys_info->ipcam[UPNPFOWD_STATUS].config.value == 0)
		return sprintf(data, "%d", 1);
	else
		return sprintf(data, "%d", 0);*/
  }

  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_supportprofile(request *req, char *data, char *arg)
  {
#ifdef SUPPORT_MULTIPROFILE
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1format(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG0_CODECNAME));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1formatname(request *req, char *data, char *arg)
  { 
	if ( *arg == '0' )
  		return sprintf(data, JPEG_PARSER_NAME);
	if ( *arg == '1' )
  		return sprintf(data, MPEG4_PARSER_NAME);

#if defined( PLAT_DM365 )

	if ( *arg == '2' )
  		return sprintf(data, H264_PARSER_NAME);
	else if ( *arg == 'a' || *arg == '\0' )
	#if defined RESOLUTION_TYCO_720P || defined RESOLUTION_TYCO_D2 || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
			return sprintf(data, "%s;%s",JPEG_PARSER_NAME,H264_PARSER_NAME);
	#else
  		return sprintf(data, "%s;%s;%s",JPEG_PARSER_NAME,MPEG4_PARSER_NAME,H264_PARSER_NAME);
  #endif
#elif defined( PLAT_DM355 )
  	else if ( *arg == 'a' || *arg == '\0' )
  		return sprintf(data, "%s;%s",JPEG_PARSER_NAME,MPEG4_PARSER_NAME);
#else
#	error Unknown platform
#endif
	return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf1res(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG0_RESOLUTION));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1resname(request *req, char *data, char *arg)
  {
  int i;
	if (*arg == 'a' || *arg == '\0') {
		int len = sprintf(data, "%s", sys_info->api_string.resolution_list[0]);
		for (i=1; i<MAX_RESOLUTIONS; i++){
			if(strlen(sys_info->api_string.resolution_list[i]) == 0)
				break;
			len += sprintf(data+len, ";%s", sys_info->api_string.resolution_list[i]);
		}
		return len;
	}
	for (i=0; i<MAX_RESOLUTIONS; i++)
		if (*arg == '0'+i)
			return sprintf(data, "%s", sys_info->api_string.resolution_list[i]);
	return -1;

  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf1rate(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG0_FRAMERATE));
  }

int para_allframeratename(request *req, char *data, char *arg)
{

	int framerate = (sys_info->video_type == 1) ? 25 : 30;
	//if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , JPEG_PARSER_NAME ) == 0 ){
		if ( *arg == '0' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '1' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '2' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '3' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '4' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '5' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '6' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '7' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '8' )
			return sprintf(data,"%d", framerate--);
		if ( *arg == '9' )
			return sprintf(data,"%d", framerate--);		
		else if ( *arg == 'a' || *arg == '\0' ){
			if( sys_info->video_type == 1 )
				return sprintf(data, "25;24;23;22;21;20;19;18;17;16;15;14;13;12;11;10;9;8;7;6;5;4;3;2;1");
			else
				return sprintf(data, "30;29;28;27;26;25;24;23;22;21;20;19;18;17;16;15;14;13;12;11;10;9;8;7;6;5;4;3;2;1");
		}
	//}
	return -1;
}

  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1ratename(request *req, char *data, char *arg)
  {
#if defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P) || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
	if ( *arg == '0' )
		return sprintf(data, ( sys_info->video_type == 1 ) ? FRAMERATE_8 : FRAMERATE_1);
	if ( *arg == '1' )
		return sprintf(data, FRAMERATE_2);
	if ( *arg == '2' )
		return sprintf(data, FRAMERATE_3);
	if ( *arg == '3' )
		return sprintf(data, FRAMERATE_4);
	if ( *arg == '4' )
		return sprintf(data, FRAMERATE_5);
	if ( *arg == '5' )
		return sprintf(data, FRAMERATE_6);
	if ( *arg == '6' )
		return sprintf(data, FRAMERATE_7);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s;%s;%s",(( sys_info->video_type == 1 ) ? FRAMERATE_8 : FRAMERATE_1),FRAMERATE_2,FRAMERATE_3,FRAMERATE_4,FRAMERATE_5,FRAMERATE_6,FRAMERATE_7);
#else
	if ( *arg == '0' )
		return sprintf(data, ( sys_info->video_type == 1 ) ? FRAMERATE_6 : FRAMERATE_1);
	if ( *arg == '1' )
		return sprintf(data, FRAMERATE_2);
	if ( *arg == '2' )
		return sprintf(data, FRAMERATE_3);
	if ( *arg == '3' )
		return sprintf(data, FRAMERATE_4);
	if ( *arg == '4' )
		return sprintf(data, FRAMERATE_5);

	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s",(( sys_info->video_type == 1 ) ? FRAMERATE_6 : FRAMERATE_1),FRAMERATE_2,FRAMERATE_3,FRAMERATE_4,FRAMERATE_5);
#endif
	return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf2bpsname(request *req, char *data, char *arg)
  {
//#if defined( MODEL_LC7317 ) || defined( MODEL_LC7337 ) || defined( MODEL_LC7357 ) || defined( MODEL_LC7317S ) || defined( MODEL_LC7337S )|| defined( SENSOR_USE_TVP5150 )
#if defined( SENSOR_USE_MT9V136) || defined( SENSOR_USE_TVP5150 ) || defined(RESOLUTION_TYCO_D2) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )||defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
		if ( *arg == '0' )
		  return sprintf(data, "4M");
	  if ( *arg == '1' )
		  return sprintf(data, "2M");
	  if ( *arg == '2' )
		  return sprintf(data, "1M");
	  if ( *arg == '3' )
		  return sprintf(data, "512K");
	  if ( *arg == '4' )
		  return sprintf(data, "256K");
	  if ( *arg == '5' )
		  return sprintf(data, "200K");
	  if ( *arg == '6' )
		  return sprintf(data, "128K");
	  if ( *arg == '7' )
		  return sprintf(data, "64K");
	  if ( *arg == '8' )
		  return sprintf(data, "50");
	  if ( *arg == '9' )
		  return sprintf(data, "40K");
	  if ( *arg == 0x3A )
		  return sprintf(data, "30K");
	  if ( *arg == 0x3B )
		  return sprintf(data, "20K");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "4M;2M;1M;512K;256K;200K;128K;64K");
	  return -1;
#else
	  if ( *arg == '0' )
		  return sprintf(data, MP4_BITRATE_1);
	  if ( *arg == '1' )
		  return sprintf(data, MP4_BITRATE_2);
	  if ( *arg == '2' )
		  return sprintf(data, MP4_BITRATE_3);
	  if ( *arg == '3' )
		  return sprintf(data, MP4_BITRATE_4);
	  if ( *arg == '4' )
		  return sprintf(data, MP4_BITRATE_5);
	  if ( *arg == '5' )
		  return sprintf(data, MP4_BITRATE_6);
	  if ( *arg == '6' )
		  return sprintf(data, MP4_BITRATE_7);
	  if ( *arg == '7' )
		  return sprintf(data, MP4_BITRATE_8);
	  if ( *arg == '8' )
		  return sprintf(data, MP4_BITRATE_9);
	  if ( *arg == '9' )
		  return sprintf(data, MP4_BITRATE_10);
	  if ( *arg == 0x3A )
		  return sprintf(data, "50K");
	  if ( *arg == 0x3B )
		  return sprintf(data, "40K");
	  if ( *arg == 0x3C )
		  return sprintf(data, "30K");
	  if ( *arg == 0x3D )
		  return sprintf(data, "20K");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
		  MP4_BITRATE_1,MP4_BITRATE_2,MP4_BITRATE_3,MP4_BITRATE_4,MP4_BITRATE_5,
		  MP4_BITRATE_6,MP4_BITRATE_7,MP4_BITRATE_8,MP4_BITRATE_9,MP4_BITRATE_10);
	  return -1;
#endif
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
/*
  int para_prf1rtsp(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%s", sys_info->config.profile_config[0].rtspurl);
	return sprintf(data, "%s",   conf_string_read(PROFILE_CONFIG0_RTSPURL));
  }
*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1qmode(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG0_QMODE].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_profile1keyframeintervalname(request *req, char *data, char *arg)
  {
		if ( *arg == '0' )
			return sprintf(data,"%d", 0);
		if ( *arg == '1' )
			return sprintf(data,"%d", 1);
		if ( *arg == '2' )
			return sprintf(data,"%d", 2);
		if ( *arg == '3' )
			return sprintf(data,"%d", 3);
		if ( *arg == '4' )
			return sprintf(data,"%d", 4);
		if ( *arg == '5' )
			return sprintf(data,"%d", 5);
		if ( *arg == '6' )
			return sprintf(data,"%d", 6);
		if ( *arg == '7' )
			return sprintf(data,"%d", 7);
		if ( *arg == '8' )
			return sprintf(data,"%d", 8);
		if ( *arg == '9' )
			return sprintf(data,"%d", 9);		
		else if ( *arg == 'a' || *arg == '\0' ){
			return sprintf(data, "30;29;28;27;26;25;24;23;22;21;20;19;18;17;16;15;14;13;12;11;10;9;8;7;6;5;4;3;2;1");
		}
	 return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_audiotypename(request *req, char *data, char *arg)
  {
	if ( *arg == '0' )
		return sprintf(data, AUDIO_CODEC_G711);
	if ( *arg == '1' )
		return sprintf(data, AUDIO_CODEC_G726);
	//if ( *arg == '2' )
		//return sprintf(data, AUDIO_CODEC_AAC);
	else if ( *arg == 'a' || *arg == '\0' )
		//return sprintf(data, "%s;%s;%s",AUDIO_CODEC_G711,AUDIO_CODEC_G726,AUDIO_CODEC_AAC);
		return sprintf(data, "%s;%s",AUDIO_CODEC_G711,AUDIO_CODEC_G726);
	 return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_profile1keyframeinterval(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG0_MPEG4KEYFRAMEINTERVAL].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_profile2keyframeinterval(request *req, char *data, char *arg)
  {
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG1_MPEG4KEYFRAMEINTERVAL].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_profile3keyframeinterval(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG2_MPEG4KEYFRAMEINTERVAL].config.u8);
  }
  
  int para_profile4keyframeinterval(request *req, char *data, char *arg)
  {
	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d",sys_info->ipcam[PROFILE_CONFIG3_MPEG4KEYFRAMEINTERVAL].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
/*
  int para_profile1customize(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%d", sys_info->config.profile_config[0].mpeg4qualityRange);
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG0_MPEG4QUALITYRANGE].config.u8);
  }
*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1view(request *req, char *data, char *arg)
  {
      if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
  #ifdef SUPPORT_EPTZ
        //return sprintf(data, "%s", sys_info->config.profile_config[0].eptzWindowSize);
        return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE));
  #else
  	return -1;
  #endif
  }
/*
  int para_profile2customize(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%d", sys_info->config.profile_config[1].mpeg4qualityRange);
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG1_MPEG4QUALITYRANGE].config.u8);
  }
*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf2view(request *req, char *data, char *arg)
  {
  if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
  
  #ifdef SUPPORT_EPTZ
      //return sprintf(data, "%s", sys_info->config.profile_config[1].eptzWindowSize);
      return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG1_EPTZWINDOWSIZE));
  #else
      return -1;
  #endif
  }
/*
  int para_profile3customize(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%d", sys_info->config.profile_config[2].mpeg4qualityRange);
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG2_MPEG4QUALITYRANGE].config.u8);
  }
*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3view(request *req, char *data, char *arg)
  {
  if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
  
  #ifdef SUPPORT_EPTZ
      //return sprintf(data, "%s", sys_info->config.profile_config[2].eptzWindowSize);
      return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG2_EPTZWINDOWSIZE));
  #else
      return -1;
  #endif
  }
	int  para_supportajax1(request *req, char *data, char *arg)
	{
	//In the 2011.02.14 by jiahung modified. Non-jpeg mode, the second to send a picture(used /dms?nowprofileid='X').
	if(1)//( sys_info->profile_codec_fmt[0] == SEND_FMT_JPEG ) 
		return sprintf(data, "1");
	else
		return sprintf(data, "0");
	}
	int para_ajax1name(request *req, char *data, char *arg)
	{
		if(profile_check(1) < SPECIAL_PROFILE)
			return -1;
		return sprintf(data, "Profile-1(%dx%d)", sys_info->ipcam[PROFILESIZE0_XSIZE].config.value, sys_info->ipcam[PROFILESIZE0_YSIZE].config.value);
	} 
/***************************************************************************
*																		 *
***************************************************************************/
	int  para_supportajax2(request *req, char *data, char *arg)
	{
		if(1)//( sys_info->profile_codec_fmt[1] == SEND_FMT_JPEG )
			return sprintf(data, "1");
		else
			return sprintf(data, "0");
	}
	int para_ajax2name(request *req, char *data, char *arg)
	{
		if(profile_check(2) < SPECIAL_PROFILE)
			return -1;
		return sprintf(data, "Profile-2(%dx%d)", sys_info->ipcam[PROFILESIZE1_XSIZE].config.value, sys_info->ipcam[PROFILESIZE1_YSIZE].config.value);
	} 
	/***************************************************************************
	*																		 *
	***************************************************************************/
	int  para_supportajax3(request *req, char *data, char *arg)
	{
		if(1)//( sys_info->profile_codec_fmt[2] == SEND_FMT_JPEG )
			return sprintf(data, "1");
		else
			return sprintf(data, "0");
	}
	int para_ajax3name(request *req, char *data, char *arg)
	{
		if(profile_check(3) < SPECIAL_PROFILE)
			return -1;
		return sprintf(data, "Profile-3(%dx%d)", sys_info->ipcam[PROFILESIZE2_XSIZE].config.value, sys_info->ipcam[PROFILESIZE2_YSIZE].config.value);
	} 
 /***************************************************************************
  * 																	   *
  ***************************************************************************/

  int para_prf2qmode(request *req, char *data, char *arg)
  {
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG1_QMODE].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3qmode(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG2_QMODE].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_prf4qmode(request *req, char *data, char *arg)
  {
	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG3_QMODE].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_supportprofilenumber(request *req, char *data, char *arg)
  {
#if defined SUPPORT_PROFILE_NUMBER
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", 0);
#endif
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_supportaspectratio(request *req, char *data, char *arg)
  {
#if defined SUPPORT_ASPECT_RATIO
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", 0);
#endif
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_profilenumber(request *req, char *data, char *arg)
  {
	return sprintf(data, "%d", sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_aspectratio(request *req, char *data, char *arg)
  {
	return sprintf(data, "%s", conf_string_read( IMG_ASPECT_RATIO ));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_profilenumbername(request *req, char *data, char *arg)
  {
#if 1
	char* ptr = data;
	int ret=0, i, num=0, cnt;

	#if (MAX_WEB_PROFILE_NUM >= 1)
		if ( *arg == '0' )
			return sprintf(data, "1");
	#endif
	#if (MAX_WEB_PROFILE_NUM >= 2)
		if ( *arg == '1' )
			return sprintf(data, "2");
	#endif
	#if (MAX_WEB_PROFILE_NUM >= 3)
		if ( *arg == '2' )
			return sprintf(data, "3");
	#endif
	#if (MAX_WEB_PROFILE_NUM >= 4)
	#warning MAX_WEB_PROFILE_NUM >= 4
	#endif
	if ( *arg == 'a' || *arg == '\0' ){
		for( i = 0 ; i < MAX_WEB_PROFILE_NUM; i++) {
			cnt = 0;
			
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}

			cnt += sprintf(ptr, "%d", i+1);
			//dbg("ptr=<%d>%s\n",i ,ptr);
			ptr+= cnt;
			ret += cnt;
			
		}
		return ret;
	}
	return -1;
#else
#if defined RESOLUTION_IMX036_3M || defined RESOLUTION_TYCO_3M
	if ( *arg == '0' )
		return sprintf(data, "1");
	if ( *arg == '1' )
		return sprintf(data, "2");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1;2");
#else
	if ( *arg == '0' )
		return sprintf(data, "1");
	if ( *arg == '1' )
		return sprintf(data, "2");
	if ( *arg == '2' )
		return sprintf(data, "3");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1;2;3");
#endif
#endif
	return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_aspectrationame(request *req, char *data, char *arg)
  {
#ifdef SUPPORT_ASPECT_RATIO
	if ( *arg == '0' )
		return sprintf(data, IMG_ASPECT_RATIO_4_3);
	else if ( *arg == '1' )
		return sprintf(data, IMG_ASPECT_RATIO_16_9);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s",IMG_ASPECT_RATIO_4_3,IMG_ASPECT_RATIO_16_9);
	return -1;
#else
	return -1;
#endif
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf2format(request *req, char *data, char *arg)
  {	
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG1_CODECNAME));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf2formatname(request *req, char *data, char *arg)
  {	
	if ( *arg == '0' )
  		return sprintf(data, JPEG_PARSER_NAME);
	if ( *arg == '1' )
  		return sprintf(data, MPEG4_PARSER_NAME);
#if defined( PLAT_DM365 )
	if ( *arg == '2' )
  		return sprintf(data, H264_PARSER_NAME);
	else if ( *arg == 'a' || *arg == '\0' )
	#if defined RESOLUTION_TYCO_720P || defined RESOLUTION_TYCO_D2 || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
			return sprintf(data, "%s;%s",JPEG_PARSER_NAME,H264_PARSER_NAME);
	#else
  		return sprintf(data, "%s;%s;%s",JPEG_PARSER_NAME,MPEG4_PARSER_NAME,H264_PARSER_NAME);
  #endif
#elif defined( PLAT_DM355 )
  	else if ( *arg == 'a' || *arg == '\0' )
  		return sprintf(data, "%s;%s",JPEG_PARSER_NAME,MPEG4_PARSER_NAME);
#else
#	error Unknown platform
#endif
  	return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf2res(request *req, char *data, char *arg)
  {
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG1_RESOLUTION));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
/*  int para_prf2resname(request *req, char *data, char *arg)
  {
#if defined(RESOLUTION_APPRO_SXGA) || defined(RESOLUTION_APPRO_1080P)
	if ( *arg == '0' )
	  return sprintf(data, sys_info->api_string.resolution_list[0]);
	if ( *arg == '1' )
	  return sprintf(data, sys_info->api_string.resolution_list[1]);
	if ( *arg == '2' )
	  return sprintf(data, sys_info->api_string.resolution_list[2]);
	if ( *arg == '3' )
	  return sprintf(data, sys_info->api_string.resolution_list[3]);
	if ( *arg == '4' )
	  return sprintf(data, sys_info->api_string.resolution_list[4]);
	else if ( *arg == 'a' || *arg == '\0' )
	  return sprintf(data, "%s;%s;%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2],sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4]);
	return -1;
#elif defined (SENSOR_USE_MT9V136)
	if (sys_info->video_type == 0)  // NTSC
	{  
	if ( *arg == '0' )
	    return sprintf(data, sys_info->api_string.resolution_list[0]);
	if ( *arg == '1' )
	    return sprintf(data, sys_info->api_string.resolution_list[1]);
	if ( *arg == '2' )
	    return sprintf(data, sys_info->api_string.resolution_list[2]);
	else if ( *arg == 'a' || *arg == '\0' )
	    return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
	}
	else { // PAL
	if ( *arg == '0' )
	    return sprintf(data, sys_info->api_string.resolution_list[3]);
	if ( *arg == '1' )
	    return sprintf(data, sys_info->api_string.resolution_list[4]);
	if ( *arg == '2' )
	    return sprintf(data, sys_info->api_string.resolution_list[5]);
	else if ( *arg == 'a' || *arg == '\0' )
	    return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
	}

	return -1;
#elif defined (SENSOR_USE_TVP5150)
	if (sys_info->video_type == 0)
	{  
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[0]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[1]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[2]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
		return -1;
	}else
	{
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[3]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[4]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[5]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
		return -1;
	}
#elif defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P)
	if (sys_info->video_type == 0)
	{  
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[6]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[0]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[1]);
		if ( *arg == '3' )
		  return sprintf(data, sys_info->api_string.resolution_list[2]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
		return -1;
	}else
	{
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[6]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[3]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[4]);
		if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[5]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
		return -1;
	}
#else
#error Unknown product model
#endif

  }*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf2rate(request *req, char *data, char *arg)
  {
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG1_FRAMERATE));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf2ratename(request *req, char *data, char *arg)
  {
  	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
#if defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P) || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
	if ( *arg == '0' )
		return sprintf(data, ( sys_info->video_type == 1 ) ? FRAMERATE_8 : FRAMERATE_1);
	if ( *arg == '1' )
		return sprintf(data, FRAMERATE_2);
	if ( *arg == '2' )
		return sprintf(data, FRAMERATE_3);
	if ( *arg == '3' )
		return sprintf(data, FRAMERATE_4);
	if ( *arg == '4' )
		return sprintf(data, FRAMERATE_5);
	if ( *arg == '5' )
		return sprintf(data, FRAMERATE_6);
	if ( *arg == '6' )
		return sprintf(data, FRAMERATE_7);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s;%s;%s",(( sys_info->video_type == 1 ) ? FRAMERATE_8 : FRAMERATE_1),FRAMERATE_2,FRAMERATE_3,FRAMERATE_4,FRAMERATE_5,FRAMERATE_6,FRAMERATE_7);
#else
	if ( *arg == '0' )
		return sprintf(data, ( sys_info->video_type == 1 ) ? FRAMERATE_6 : FRAMERATE_1);
	if ( *arg == '1' )
		return sprintf(data, FRAMERATE_2);
	if ( *arg == '2' )
		return sprintf(data, FRAMERATE_3);
	if ( *arg == '3' )
		return sprintf(data, FRAMERATE_4);
	if ( *arg == '4' )
		return sprintf(data, FRAMERATE_5);

	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s",(( sys_info->video_type == 1 ) ? FRAMERATE_6 : FRAMERATE_1),FRAMERATE_2,FRAMERATE_3,FRAMERATE_4,FRAMERATE_5);
#endif
	return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1bps(request *req, char *data, char *arg)
  {
	  if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s",  conf_string_read(PROFILE_CONFIG0_CBR_QUALITY));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf2bps(request *req, char *data, char *arg)
  {
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s",  conf_string_read(PROFILE_CONFIG1_CBR_QUALITY));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf1bpsname(request *req, char *data, char *arg)
  {
//#if defined( MODEL_LC7317 ) || defined( MODEL_LC7337 ) || defined( MODEL_LC7357 ) || defined( MODEL_LC7317S ) || defined( MODEL_LC7337S ) || defined( SENSOR_USE_TVP5150 )
#if defined( SENSOR_USE_MT9V136) || defined( SENSOR_USE_TVP5150 ) || defined(RESOLUTION_TYCO_D2) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )||defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
		if ( *arg == '0' )
		  return sprintf(data, "4M");
	  if ( *arg == '1' )
		  return sprintf(data, "2M");
	  if ( *arg == '2' )
		  return sprintf(data, "1M");
	  if ( *arg == '3' )
		  return sprintf(data, "512K");
	  if ( *arg == '4' )
		  return sprintf(data, "256K");
	  if ( *arg == '5' )
		  return sprintf(data, "200K");
	  if ( *arg == '6' )
		  return sprintf(data, "128K");
	  if ( *arg == '7' )
		  return sprintf(data, "64K");
	  if ( *arg == '8' )
		  return sprintf(data, "50");
	  if ( *arg == '9' )
		  return sprintf(data, "40K");
	  if ( *arg == 0x3A )
		  return sprintf(data, "30K");
	  if ( *arg == 0x3B )
		  return sprintf(data, "20K");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "4M;2M;1M;512K;256K;200K;128K;64K");
	  return -1;
	#else
	  if ( *arg == '0' )
		  return sprintf(data, MP4_BITRATE_1);
	  if ( *arg == '1' )
		  return sprintf(data, MP4_BITRATE_2);
	  if ( *arg == '2' )
		  return sprintf(data, MP4_BITRATE_3);
	  if ( *arg == '3' )
		  return sprintf(data, MP4_BITRATE_4);
	  if ( *arg == '4' )
		  return sprintf(data, MP4_BITRATE_5);
	  if ( *arg == '5' )
		  return sprintf(data, MP4_BITRATE_6);
	  if ( *arg == '6' )
		  return sprintf(data, MP4_BITRATE_7);
	  if ( *arg == '7' )
		  return sprintf(data, MP4_BITRATE_8);
	  if ( *arg == '8' )
		  return sprintf(data, MP4_BITRATE_9);
	  if ( *arg == '9' )
		  return sprintf(data, MP4_BITRATE_10);
	  if ( *arg == 0x3A )
		  return sprintf(data, "50K");
	  if ( *arg == 0x3B )
		  return sprintf(data, "40K");
	  if ( *arg == 0x3C )
		  return sprintf(data, "30K");
	  if ( *arg == 0x3D )
		  return sprintf(data, "20K");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
		  MP4_BITRATE_1,MP4_BITRATE_2,MP4_BITRATE_3,MP4_BITRATE_4,MP4_BITRATE_5,
		  MP4_BITRATE_6,MP4_BITRATE_7,MP4_BITRATE_8,MP4_BITRATE_9,MP4_BITRATE_10);
	  return -1;
	#endif
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf1quality(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	/*if( !strcmp( sys_info->config.profile_config[0].codecname , MPEG4_PARSER_NAME) || !strcmp( sys_info->config.profile_config[0].codecname , H264_PARSER_NAME ))
		return sprintf(data, "%d", sys_info->config.profile_config[0].Fixed_quality);
	else
		return sprintf(data, "%d", sys_info->config.profile_config[0].jpeg_quality);*/

	if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0 )
		return sprintf( data, "%d", sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY].config.u8);
	else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY].config.u8);
	else
		return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_profile1qualityjpeg(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_profile1qualitympeg4(request *req, char *data, char *arg)
  {
	if(profile_check(1) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s",  conf_string_read(PROFILE_CONFIG0_CBR_QUALITY));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_profile2qualityjpeg(request *req, char *data, char *arg)
  {
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d",  sys_info->ipcam[PROFILE_CONFIG1_JPEG_QUALITY].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_profile2qualitympeg4(request *req, char *data, char *arg)
  {
	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s",  conf_string_read(PROFILE_CONFIG1_CBR_QUALITY));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_profile3qualityjpeg(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d",  sys_info->ipcam[PROFILE_CONFIG2_JPEG_QUALITY].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_profile3qualitympeg4(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s",  conf_string_read(PROFILE_CONFIG2_CBR_QUALITY));
  }
   /***************************************************************************
   *																		 *
   ***************************************************************************/   
   int para_profile4qualityjpeg(request *req, char *data, char *arg)
  {
	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG3_JPEG_QUALITY].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_profile4qualitympeg4(request *req, char *data, char *arg)
  {
	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_CBR_QUALITY));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf2quality(request *req, char *data, char *arg)
  {
  	if(profile_check(2) < SPECIAL_PROFILE)
		return -1;
	/*if( !strcmp( sys_info->config.profile_config[1].codecname , MPEG4_PARSER_NAME) || !strcmp( sys_info->config.profile_config[1].codecname , H264_PARSER_NAME ))
		return sprintf(data, "%d", sys_info->config.profile_config[1].Fixed_quality);
	else
		return sprintf(data, "%d", sys_info->config.profile_config[1].jpeg_quality);*/
	if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0 )
		return sprintf( data, "%d", sys_info->ipcam[PROFILE_CONFIG1_FIXED_QUALITY].config.u8);
	else if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG1_JPEG_QUALITY].config.u8);
	else
		return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3quality(request *req, char *data, char *arg)
  {
  	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	/*if( !strcmp( sys_info->config.profile_config[2].codecname , MPEG4_PARSER_NAME) || !strcmp( sys_info->config.profile_config[2].codecname , H264_PARSER_NAME ))
		return sprintf(data, "%d", sys_info->config.profile_config[2].Fixed_quality);
	else
		return sprintf(data, "%d", sys_info->config.profile_config[2].jpeg_quality);*/
	if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0 )
		return sprintf( data, "%d", sys_info->ipcam[PROFILE_CONFIG2_FIXED_QUALITY].config.u8);
	else if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG2_JPEG_QUALITY].config.u8);
	else
		return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf4quality(request *req, char *data, char *arg)
  {
  	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	/*if( !strcmp( sys_info->config.profile_config[3].codecname , MPEG4_PARSER_NAME) || !strcmp( sys_info->config.profile_config[3].codecname , H264_PARSER_NAME ))
		return sprintf(data, "%d", sys_info->config.profile_config[3].jpeg_quality);
	else if( strcmp( sys_info->config.profile_config[3].codecname , MPEG4_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->config.profile_config[3].Fixed_quality);*/
	if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0 )
		return sprintf( data, "%d", sys_info->ipcam[PROFILE_CONFIG3_FIXED_QUALITY].config.u8);
	else if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG3_JPEG_QUALITY].config.u8);
	else
		return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int profile4qualityavc(request *req, char *data, char *arg)
  {
  	if(profile_check(4) < SPECIAL_PROFILE)
		return -1;
	/*if( strcmp( sys_info->config.profile_config[3].codecname , H264_PARSER_NAME ) == 0 )
		return sprintf( data, "%d", sys_info->config.lan_config.avc_quality );
	else if( strcmp( sys_info->config.profile_config[3].codecname , JPEG_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->config.profile_config[3].jpeg_quality);
	else if( strcmp( sys_info->config.profile_config[3].codecname , MPEG4_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->config.profile_config[3].Fixed_quality);*/
  	if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME), H264_PARSER_NAME ) == 0 )
		return sprintf( data, "%d", sys_info->ipcam[CONFIG_AVC_QUALITY].config.u8);
	else if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG3_JPEG_QUALITY].config.u8);
	else if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 )
		return sprintf(data, "%d", sys_info->ipcam[PROFILE_CONFIG3_FIXED_QUALITY].config.u8);
	else
		return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
/*
  int para_prf2rtsp(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%s", sys_info->config.profile_config[1].rtspurl);
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG1_RTSPURL));
  } 
*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3format(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG2_CODECNAME));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3formatname(request *req, char *data, char *arg)
  {
  	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	if ( *arg == '0' )
  		return sprintf(data, JPEG_PARSER_NAME);
	if ( *arg == '1' )
  		return sprintf(data, MPEG4_PARSER_NAME);
#if defined( PLAT_DM365 )
	if ( *arg == '2' )
  		return sprintf(data, H264_PARSER_NAME);
	else if ( *arg == 'a' || *arg == '\0' )
	#if defined RESOLUTION_TYCO_720P || defined RESOLUTION_TYCO_D2
			return sprintf(data, "%s;%s",JPEG_PARSER_NAME,H264_PARSER_NAME);
	#else
  		return sprintf(data, "%s;%s;%s",JPEG_PARSER_NAME,MPEG4_PARSER_NAME,H264_PARSER_NAME);
  #endif
#elif defined( PLAT_DM355 )
  	else if ( *arg == 'a' || *arg == '\0' )
  		return sprintf(data, "%s;%s",JPEG_PARSER_NAME,MPEG4_PARSER_NAME);
#else
#	error Unknown platform
#endif
  	return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf3res(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG2_RESOLUTION));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
/*  int para_prf3resname(request *req, char *data, char *arg)
  {
#if defined(RESOLUTION_APPRO_SXGA) || defined(RESOLUTION_APPRO_1080P)
	if ( *arg == '0' )
	  return sprintf(data, sys_info->api_string.resolution_list[0]);
	if ( *arg == '1' )
	  return sprintf(data, sys_info->api_string.resolution_list[1]);
	if ( *arg == '2' )
	  return sprintf(data, sys_info->api_string.resolution_list[2]);
	if ( *arg == '3' )
	  return sprintf(data, sys_info->api_string.resolution_list[3]);
	if ( *arg == '4' )
	  return sprintf(data, sys_info->api_string.resolution_list[4]);
	else if ( *arg == 'a' || *arg == '\0' )
	  return sprintf(data, "%s;%s;%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2],sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4]);
	return -1;
#elif defined (SENSOR_USE_MT9V136)
	if (sys_info->video_type == 0)  // NTSC
	{  
	if ( *arg == '0' )
	    return sprintf(data, sys_info->api_string.resolution_list[0]);
	if ( *arg == '1' )
	    return sprintf(data, sys_info->api_string.resolution_list[1]);
	if ( *arg == '2' )
	    return sprintf(data, sys_info->api_string.resolution_list[2]);
	else if ( *arg == 'a' || *arg == '\0' )
	    return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
	}
	else { // PAL
	if ( *arg == '0' )
	    return sprintf(data, sys_info->api_string.resolution_list[3]);
	if ( *arg == '1' )
	    return sprintf(data, sys_info->api_string.resolution_list[4]);
	if ( *arg == '2' )
	    return sprintf(data, sys_info->api_string.resolution_list[5]);
	else if ( *arg == 'a' || *arg == '\0' )
	    return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
	}

	return -1;

#elif defined (SENSOR_USE_TVP5150)
	if (sys_info->video_type == 0)
	{  
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[0]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[1]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[2]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
		return -1;
	}else
	{
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[3]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[4]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[5]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
		return -1;
	}
#elif defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P)
	if (sys_info->video_type == 0)
	{  
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[6]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[0]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[1]);
		if ( *arg == '3' )
		  return sprintf(data, sys_info->api_string.resolution_list[2]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
		return -1;
	}else
	{
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[6]);
	  if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[3]);
	  if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[4]);
		if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[5]);
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
		return -1;
	}
#else
#error Unknown product model
#endif

  }*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_prf3rate(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG2_FRAMERATE));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3ratename(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
#if defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P) || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M)||defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
	if ( *arg == '0' )
		return sprintf(data, ( sys_info->video_type == 1 ) ? FRAMERATE_8 : FRAMERATE_1);
	if ( *arg == '1' )
		return sprintf(data, FRAMERATE_2);
	if ( *arg == '2' )
		return sprintf(data, FRAMERATE_3);
	if ( *arg == '3' )
		return sprintf(data, FRAMERATE_4);
	if ( *arg == '4' )
		return sprintf(data, FRAMERATE_5);
	if ( *arg == '5' )
		return sprintf(data, FRAMERATE_6);
	if ( *arg == '6' )
		return sprintf(data, FRAMERATE_7);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s;%s;%s",(( sys_info->video_type == 1 ) ? FRAMERATE_8 : FRAMERATE_1),FRAMERATE_2,FRAMERATE_3,FRAMERATE_4,FRAMERATE_5,FRAMERATE_6,FRAMERATE_7);
#else
	if ( *arg == '0' )
		return sprintf(data, ( sys_info->video_type == 1 ) ? FRAMERATE_6 : FRAMERATE_1);
	if ( *arg == '1' )
		return sprintf(data, FRAMERATE_2);
	if ( *arg == '2' )
		return sprintf(data, FRAMERATE_3);
	if ( *arg == '3' )
		return sprintf(data, FRAMERATE_4);
	if ( *arg == '4' )
		return sprintf(data, FRAMERATE_5);

	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s;%s",(( sys_info->video_type == 1 ) ? FRAMERATE_6 : FRAMERATE_1),FRAMERATE_2,FRAMERATE_3,FRAMERATE_4,FRAMERATE_5);
#endif
	return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3bps(request *req, char *data, char *arg)
  {
	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG2_CBR_QUALITY));
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_prf3bpsname(request *req, char *data, char *arg)
  {
  	if(profile_check(3) < SPECIAL_PROFILE)
		return -1;
//#if defined( MODEL_LC7317 ) || defined( MODEL_LC7337 ) || defined( MODEL_LC7357 ) || defined( MODEL_LC7317S ) || defined( MODEL_LC7337S ) || defined( SENSOR_USE_TVP5150 )
#if defined( SENSOR_USE_MT9V136) || defined( SENSOR_USE_TVP5150 ) || defined (RESOLUTION_TYCO_D2) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )||defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
		if ( *arg == '0' )
		  return sprintf(data, "4M");
	  if ( *arg == '1' )
		  return sprintf(data, "2M");
	  if ( *arg == '2' )
		  return sprintf(data, "1M");
	  if ( *arg == '3' )
		  return sprintf(data, "512K");
	  if ( *arg == '4' )
		  return sprintf(data, "256K");
	  if ( *arg == '5' )
		  return sprintf(data, "200K");
	  if ( *arg == '6' )
		  return sprintf(data, "128K");
	  if ( *arg == '7' )
		  return sprintf(data, "64K");
	  if ( *arg == '8' )
		  return sprintf(data, "50");
	  if ( *arg == '9' )
		  return sprintf(data, "40K");
	  if ( *arg == 0x3A )
		  return sprintf(data, "30K");
	  if ( *arg == 0x3B )
		  return sprintf(data, "20K");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "4M;2M;1M;512K;256K;200K;128K;64K");
	  return -1;
#else
	  if ( *arg == '0' )
		  return sprintf(data, MP4_BITRATE_1);
	  if ( *arg == '1' )
		  return sprintf(data, MP4_BITRATE_2);
	  if ( *arg == '2' )
		  return sprintf(data, MP4_BITRATE_3);
	  if ( *arg == '3' )
		  return sprintf(data, MP4_BITRATE_4);
	  if ( *arg == '4' )
		  return sprintf(data, MP4_BITRATE_5);
	  if ( *arg == '5' )
		  return sprintf(data, MP4_BITRATE_6);
	  if ( *arg == '6' )
		  return sprintf(data, MP4_BITRATE_7);
	  if ( *arg == '7' )
		  return sprintf(data, MP4_BITRATE_8);
	  if ( *arg == '8' )
		  return sprintf(data, MP4_BITRATE_9);
	  if ( *arg == '9' )
		  return sprintf(data, MP4_BITRATE_10);
	  if ( *arg == 0x3A )
		  return sprintf(data, "50K");
	  if ( *arg == 0x3B )
		  return sprintf(data, "40K");
	  if ( *arg == 0x3C )
		  return sprintf(data, "30K");
	  if ( *arg == 0x3D )
		  return sprintf(data, "20K");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "%s;%s;%s;%s;%s;%s;%s;%s;%s;%s",
		  MP4_BITRATE_1,MP4_BITRATE_2,MP4_BITRATE_3,MP4_BITRATE_4,MP4_BITRATE_5,
		  MP4_BITRATE_6,MP4_BITRATE_7,MP4_BITRATE_8,MP4_BITRATE_9,MP4_BITRATE_10);
	  return -1;
	#endif
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
/*
  int para_prf3rtsp(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%s", sys_info->config.profile_config[2].rtspurl);
	return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG2_RTSPURL));
  }
*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/
	int para_prf4format(request *req, char *data, char *arg)
	{
		if(profile_check(4) < SPECIAL_PROFILE)
			return -1;
		return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_CODECNAME));
//		return sprintf( data, "0" );
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/
	int para_prf4formatname(request *req, char *data, char *arg)
	{
		if(profile_check(4) < SPECIAL_PROFILE)
			return -1;
		if ( *arg == '0' )
			return sprintf(data, H264_PARSER_NAME);
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, H264_PARSER_NAME);
		return -1;
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/ 	
	int para_prf4res(request *req, char *data, char *arg)
	{
		if(profile_check(4) < SPECIAL_PROFILE)
			return -1;
		return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_RESOLUTION));
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/
	int para_prf4resname(request *req, char *data, char *arg)
	{
	#if 0
#if defined( PLAT_DM365 )
	#if defined(RESOLUTION_APPRO_SXGA) || defined(RESOLUTION_APPRO_1080P) || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_IMX036_2M) || defined(RESOLUTION_IMX036_3M) || defined(RESOLUTION_OV9715_1M)
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[0]);
		if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[1]);
		if ( *arg == '2' )
		  return sprintf(data, sys_info->api_string.resolution_list[2]);
		if ( *arg == '3' )
		  return sprintf(data, sys_info->api_string.resolution_list[3]);
		//if ( *arg == '4' )
		  //return sprintf(data, sys_info->api_string.resolution_list[4]);
		else if ( *arg == 'a' || *arg == '\0' )
		  //return sprintf(data, "%s;%s;%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2],sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4]);
		  return sprintf(data, "%s;%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2],sys_info->api_string.resolution_list[3]);
		return -1;
#elif defined(RESOLUTION_TYCO_OV2715_D1) || defined (RESOLUTION_TYCO_IMX076_D1)
	int i;
	if (*arg == 'a' || *arg == '\0') {
		int len = sprintf(data, "%s", sys_info->api_string.resolution_list[0]);
		for (i=1; i<MAX_RESOLUTIONS; i++)
			len += sprintf(data+len, ";%s", sys_info->api_string.resolution_list[i]);
		return len;
	}
	for (i=0; i<MAX_RESOLUTIONS; i++)
		if (*arg == '0'+i)
			return sprintf(data, "%s", sys_info->api_string.resolution_list[i]);
	return -1;

	#elif defined (SENSOR_USE_MT9V136)
		if ( *arg == '0' )
		  return sprintf(data, sys_info->api_string.resolution_list[0]);
		if ( *arg == '1' )
		  return sprintf(data, sys_info->api_string.resolution_list[1]);
		//if ( *arg == '2' )
		  //return sprintf(data, sys_info->api_string.resolution_list[2]);
		else if ( *arg == 'a' || *arg == '\0' )
		  //return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
		  return sprintf(data, "%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1]);
		return -1;

	#elif defined (SENSOR_USE_TVP5150) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined (SENSOR_USE_TVP5150_MDIN270)
		if (sys_info->video_type == 0)
		{  
			if ( *arg == '0' )
				return sprintf(data, sys_info->api_string.resolution_list[0]);
			if ( *arg == '1' )
				return sprintf(data, sys_info->api_string.resolution_list[1]);
			 // if ( *arg == '2' )
				//return sprintf(data, sys_info->api_string.resolution_list[2]);
			else if ( *arg == 'a' || *arg == '\0' )
				//return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
				 return sprintf(data, "%s;%s",sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1]);
			return -1;
		}else
		{
			if ( *arg == '0' )
				return sprintf(data, sys_info->api_string.resolution_list[3]);
			if ( *arg == '1' )
				return sprintf(data, sys_info->api_string.resolution_list[4]);
			  //if ( *arg == '2' )
				//return sprintf(data, sys_info->api_string.resolution_list[5]);
			else if ( *arg == 'a' || *arg == '\0' )
				//return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
				 return sprintf(data, "%s;%s",sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4]);
			return -1;
		}
	#elif defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P)
		if (sys_info->video_type == 0)
		{  
			if ( *arg == '0' )
			  return sprintf(data, sys_info->api_string.resolution_list[6]);
		  if ( *arg == '1' )
			  return sprintf(data, sys_info->api_string.resolution_list[0]);
		  if ( *arg == '2' )
			  return sprintf(data, sys_info->api_string.resolution_list[1]);
			//if ( *arg == '3' )
			  //return sprintf(data, sys_info->api_string.resolution_list[2]);
		  else if ( *arg == 'a' || *arg == '\0' )
			  //return sprintf(data, "%s;%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1],sys_info->api_string.resolution_list[2]);
			  return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[0],sys_info->api_string.resolution_list[1]);
			return -1;
		}else
		{
			if ( *arg == '0' )
			  return sprintf(data, sys_info->api_string.resolution_list[6]);
		  if ( *arg == '1' )
			  return sprintf(data, sys_info->api_string.resolution_list[3]);
		  if ( *arg == '2' )
			  return sprintf(data, sys_info->api_string.resolution_list[4]);
			//if ( *arg == '2' )
			  //return sprintf(data, sys_info->api_string.resolution_list[5]);
		  else if ( *arg == 'a' || *arg == '\0' )
			  //return sprintf(data, "%s;%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4],sys_info->api_string.resolution_list[5]);
			  return sprintf(data, "%s;%s;%s",sys_info->api_string.resolution_list[6],sys_info->api_string.resolution_list[3],sys_info->api_string.resolution_list[4]);
			return -1;
		}
	#else
	#error Unknown product model
	#endif
#else
	if (sys_info->video_type == 0)
	{  
		if ( *arg == '0' )
			return sprintf(data, "720x480");
		if ( *arg == '1' )
			return sprintf(data, "352x240");
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, "720x480;352x240");
		return -1;
	}else
	{
		if ( *arg == '0' )
			return sprintf(data, "720x576");
		if ( *arg == '1' )
			return sprintf(data, "352x288");
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, "720x576;352x288");
		return -1;
	}
#endif
#endif
	return -1;
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/ 	
	int para_prf4rate(request *req, char *data, char *arg)
	{
		if(profile_check(4) < SPECIAL_PROFILE)
			return -1;
		return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_FRAMERATE));
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/
	int para_prf4ratename(request *req, char *data, char *arg)
	{
		if(profile_check(4) < SPECIAL_PROFILE)
			return -1;
		if ( *arg == '0' ){
			if ( sys_info->video_type == 1 )
				return sprintf(data, "25");	//25
			else
				return sprintf(data, "30");	//25
		}
		if ( *arg == '1' )
			return sprintf(data, "24");
		if ( *arg == '2' )
			return sprintf(data, "15");
		if ( *arg == '3' )
			return sprintf(data, "5");
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, ( sys_info->video_type == 1 ) ? "25;24;15;5" : "30;24;15;5" );
		return -1;
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/
	int para_prf4bps(request *req, char *data, char *arg)
	{
		if(profile_check(4) < SPECIAL_PROFILE)
			return -1;
/*		if ( sys_info->config.lan_config.avc_cvalue		  == 4096 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_1, HTTP_OPTION_LEN );
		else if  ( sys_info->config.lan_config.avc_cvalue == 2048 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_2,	HTTP_OPTION_LEN );
		else if  ( sys_info->config.lan_config.avc_cvalue == 1024 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_3, HTTP_OPTION_LEN );
		else if  ( sys_info->config.lan_config.avc_cvalue ==  512 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_4, HTTP_OPTION_LEN );
		else if  ( sys_info->config.lan_config.avc_cvalue ==  256 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_5, HTTP_OPTION_LEN );
		else if  ( sys_info->config.lan_config.avc_cvalue ==  200 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_6, HTTP_OPTION_LEN );
		else if  ( sys_info->config.lan_config.avc_cvalue ==  128 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_7, HTTP_OPTION_LEN );
		else if  ( sys_info->config.lan_config.avc_cvalue ==   64 )	strncpy( sys_info->config.profile_config[3].CBR_quality, MP4_BITRATE_8, HTTP_OPTION_LEN );
*/
/*		if ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 == 4096 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_1, HTTP_OPTION_LEN );
		else if  ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 == 2048 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_2, HTTP_OPTION_LEN );
		else if  ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 == 1024 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_3, HTTP_OPTION_LEN );
		else if  ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 ==  512 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_4, HTTP_OPTION_LEN );
		else if  ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 ==  256 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_5, HTTP_OPTION_LEN );
		else if  ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 ==  200 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_6, HTTP_OPTION_LEN );
		else if  ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 ==  128 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_7, HTTP_OPTION_LEN );
		else if  ( sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32 ==   64 ) strncpy( conf_string_read(PROFILE_CONFIG3_CBR_QUALITY), MP4_BITRATE_8, HTTP_OPTION_LEN );*/
		return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_CBR_QUALITY));
	//return sprintf(data, "%s", sys_info->config.profile_config[3].CBR_quality);
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/
	int para_prf4bpsname(request *req, char *data, char *arg)
	{
		if(profile_check(4) < SPECIAL_PROFILE)
			return -1;
#if defined( MODEL_LC7317 ) || defined( MODEL_LC7337 ) || defined( MODEL_LC7357 ) || defined( MODEL_VS2311B ) || defined( MODEL_VS2311T ) || defined( MODEL_VS2325HB ) || defined (RESOLUTION_TYCO_D2)
/*		if ( *arg == '0' )
			return sprintf( data, "HIGHEST" );
		if ( *arg == '1' )
			return sprintf( data, "HIGH" );
		if ( *arg == '2' )
			return sprintf( data, "NORMAL" );
		if ( *arg == '3' )
			return sprintf( data, "LOW" );
		if ( *arg == '4' )
			return sprintf( data, "LOWEST" );
		if ( *arg == 'a' || *arg == '\0' )
			return sprintf( data, "HIGHEST;HIGH;NORMAL;LOW;LOWEST" );
		return -1;
*/
	if ( *arg == '0' )
		return sprintf(data, "4M");
	if ( *arg == '1' )
		return sprintf(data, "2M");
	if ( *arg == '2' )
		return sprintf(data, "1M");
	if ( *arg == '3' )
		return sprintf(data, "512K");
	if ( *arg == '4' )
		return sprintf(data, "256K");
	if ( *arg == '5' )
		return sprintf(data, "200K");
	if ( *arg == '6' )
		return sprintf(data, "128K");
	if ( *arg == '7' )
		return sprintf(data, "64K");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "4M;2M;1M;512K;256K;200K;128K;64K");
	return -1;

#else
		if ( *arg == '0' )
			return sprintf(data, "8M");
		if ( *arg == '1' )
			return sprintf(data, "6M");
		if ( *arg == '2' )
			return sprintf(data, "4M");
		if ( *arg == '3' )
			return sprintf(data, "2M");
		if ( *arg == '4' )
			return sprintf(data, "1M");
		if ( *arg == '5' )
			return sprintf(data, "512K");
		if ( *arg == '6' )
			return sprintf(data, "256K");
		if ( *arg == '7' )
			return sprintf(data, "200K");
		if ( *arg == '8' )
			return sprintf(data, "128K");
		if ( *arg == '9' )
			return sprintf(data, "64K");
		if ( *arg == 0x3A )
			return sprintf(data, "50K");
		if ( *arg == 0x3B )
			return sprintf(data, "40K");
		if ( *arg == 0x3C )
			return sprintf(data, "30K");
		if ( *arg == 0x3D )
			return sprintf(data, "20K");
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, "8M;6M;4M;2M;1M;512K;256K;200K;128K;64K");
		return -1;
#endif
	}
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/ 	
/*
	int para_prf4rtsp(request *req, char *data, char *arg)
	{
		//return sprintf(data, "%s", sys_info->config.profile_config[3].rtspurl);
		return sprintf(data, "%s", conf_string_read(PROFILE_CONFIG3_RTSPURL));
	}
*/
	/***************************************************************************
	 *																		 *
	 ***************************************************************************/
  int para_ledstatus(request *req, char *data, char *arg)
  {
	//return sprintf(data, "%d", sys_info->config.lan_config.net.led_status);
	return sprintf(data, "%d", sys_info->ipcam[LED_STATUS].config.u8);
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_guest(request *req, char *data, char *arg)
  {
	  char *ptr = data;
	  
      // except admin
      if(*arg >= '1' && *arg <= '9') {
	  	//return sprintf(data, sys_info->config.acounts[*arg - 0x30].name);
	  	return sprintf(data, conf_string_read(ACOUNTS0_NAME+(*arg - 0x30)*3));
      }
	  else if ( *arg == 'a' || *arg == '\0' ) {
	  	int i;

	  	for(i = 1; i < ACOUNT_NUM ; i++) {
			//if( *(sys_info->config.acounts[i].name) != 0)
			if( *(conf_string_read(ACOUNTS0_NAME+i*3)) != 0)
				ptr += sprintf(ptr, ";%s", conf_string_read(ACOUNTS0_NAME+i*3));
				//ptr += sprintf(ptr, ";%s", sys_info->config.acounts[i].name);
	  	}

		return (ptr - data);
	  }
	  
	  return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_mtwzconfig(request *req, char *data, char *arg)
  {
  	if(sys_info->ipcam[E2G_EVENT0_INFO_VALID].config.u8) {
		if(sys_info->ipcam[E2G_EVENT0_INFO_ENABLE].config.u8) {
			if(sys_info->ipcam[E2G_EVENT0_NORMAL_TRIGGER_TYPE].config.u8 == TRIG_BY_MOTION) {
				if( !(sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA].config.u32 & 0x0000F) ) {
					dbg("action_media = %05X", sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA].config.u32);
					return sprintf(data, "0");
				}
			}
		}
	}
  	/*EVENT_2G_TYPE *pevent = &sys_info->config.e2g_event[0];

	if(pevent->info.valid) {
		if(pevent->info.enable) {
			if(pevent->normal.trigger_type == TRIG_BY_MOTION) {
				if( !(pevent->normal.action_media & 0x0000F) ) {
					dbg("action_media = %05X", pevent->normal.action_media);
					return sprintf(data, "0");
				}
			}
		}
	}
*/
	return sprintf(data, "1");
  }  
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_audioinname(request *req, char *data, char *arg)
  {
	  if ( *arg == '0' )
		  return sprintf(data, "0");
	  if ( *arg == '1' )
		  return sprintf(data, "3");
	  if ( *arg == '2' )
		  return sprintf(data, "6");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "0;3;6");
	  return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/
  int para_audiooutname(request *req, char *data, char *arg)
  {
	  if ( *arg == '0' )
		  return sprintf(data, "0");
	  if ( *arg == '1' )
		  return sprintf(data, "-3");
	  if ( *arg == '2' )
		  return sprintf(data, "-6");
	  if ( *arg == '3' )
		  return sprintf(data, "-12");
	  else if ( *arg == 'a' || *arg == '\0' )
		  return sprintf(data, "0;-3;-6;-12");
	  return -1;
  }
  /***************************************************************************
   *																		 *
   ***************************************************************************/   
  int para_wintimezone(request *req, char *data, char *arg)
  {
    // FIX ME: 
	//return sprintf(data, "%d", sys_info->config.lan_config.net.ntp_timezone);
	return sprintf(data, "%d", sys_info->ipcam[NTP_TIMEZONE].config.u8);
  }   
  /***************************************************************************
   *																		 *
   ***************************************************************************/  
int para_sensorcropresolution(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", conf_string_read(SENSOR_CROP_RES));
}

int para_sensorcropresolutionname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
  		return sprintf(data, IMG_SIZE_1280x1024);
	if ( *arg == '1' )
  		return sprintf(data, IMG_SIZE_1280x720);
 	if ( *arg == '2' )
  		return sprintf(data, IMG_SIZE_640x480);
 	else if ( *arg == 'a' || *arg == '\0' )
  		return sprintf(data, "%s;%s;%s",IMG_SIZE_1280x1024,IMG_SIZE_1280x720,IMG_SIZE_640x480);
	else
		return -1;
}
int para_sensorstartx(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[SENSOR_CROP_STARTX].config.u32);
}
int para_sensorstarty(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->ipcam[SENSOR_CROP_STARTY].config.u32);
}
  /***************************************************************************
   *																		 *
   ***************************************************************************/ 

int para_minoutvolume(request *req, char *data, char *arg)
{
  //return sprintf(data, "%d", sys_info->config.lan_config.audiooutvolume);
  return sprintf(data, "%d", sys_info->ipcam[AUDIOOUTVOLUME].config.u8);
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
 

int para_powerout(request *req, char *data, char *arg)
{
  //return sprintf(data, "%d", sys_info->config.lan_config.powerout);
  return sprintf(data, "%d", sys_info->ipcam[POWEROUT].config.u8);
}

int para_powerschedule(request *req, char *data, char *arg)
{
	return sprintf(data, "%06d%06d", sys_info->ipcam[POWERSCHEDULE_START].config.u32, sys_info->ipcam[POWERSCHEDULE_END].config.u32);
  //return sprintf(data, "%06d%06d", sys_info->config.lan_config.powerschedule_start, sys_info->config.lan_config.powerschedule_end);
}

/***************************************************************************
 *																		   *
 ***************************************************************************/
int para_tstamplocname(request *req, char *data, char *arg)
{
  if ( *arg == '0' )
		return sprintf(data, "UPPER LEFT");
	if ( *arg == '1' )
		return sprintf(data, "UPPER RIGHT");
	if ( *arg == '2' )
		return sprintf(data, "BOTTOM LEFT");
	if ( *arg == '3' )
		return sprintf(data, "BOTTOM RIGHT");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "UPPER LEFT;UPPER RIGHT;BOTTOM LEFT;BOTTOM RIGHT");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tstampformatname(request *req, char *data, char *arg)
{
  if ( *arg == '0' )
		return sprintf(data, "YYYY/MM/DD");
	else if ( *arg == '1' )
		return sprintf(data, "MM/DD/YYYY");
	else if ( *arg == '2' )
		return sprintf(data, "DD/MM/YYYY");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "YYYY/MM/DD;"
						   "MM/DD/YYYY;"
						   "DD/MM/YYYY");
#if 0
	else if ( *arg == '3' )
		return sprintf(data, "YYYY/MM/DD TITLE");
	else if ( *arg == '4' )
		return sprintf(data, "MM/DD/YYYY TITLE");
	else if ( *arg == '5' )
		return sprintf(data, "DD/MM/YYYY TITLE");
	else if ( *arg == '6' )
		return sprintf(data, "TITLE YYYY/MM/DD");
	else if ( *arg == '7' )
		return sprintf(data, "TITLE MM/DD/YYYY");
	else if ( *arg == '8' )
		return sprintf(data, "TITLE DD/MM/YYYY");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "YYYY/MM/DD;"
						   "MM/DD/YYYY;"
						   "DD/MM/YYYY;"
						   "YYYY/MM/DD TITLE;"
						   "MM/DD/YYYY TITLE;"
						   "DD/MM/YYYY TITLE;"
						   "TITLE YYYY/MM/DD;"
						   "TITLE MM/DD/YYYY;"
						   "TITLE DD/MM/YYYY");
#endif
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tstampformat(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.tstampformat);
	return sprintf(data, "%d", sys_info->ipcam[TSTAMPFORMAT].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_tstamplabel(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.lan_config.tstamplabel);
	return sprintf(data, "%s", conf_string_read(TSTAMPLABEL));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_audioinmax(request *req, char *data, char *arg)
{
   	return sprintf(data, "%d", 4);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_audioinvolume(request *req, char *data, char *arg)
{
    /*SysInfo* pSysInfo = GetSysInfo();

	if(pSysInfo == NULL)
		return -1;*/
	//return sprintf(data, "%d", sys_info->config.lan_config.audioinvolume);
	return sprintf(data, "%d", sys_info->ipcam[AUDIOINVOLUME].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontrolmode(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.dnc_mode);
	return sprintf(data, "%d", sys_info->ipcam[DNC_MODE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if SUPPORT_DNC_EX
int para_dncontrolscheduleex(request *req, char *data, char *arg)
{
	//return sprintf(data, "%06d%06d", sys_info->config.lan_config.dnc_schedule_start, sys_info->config.lan_config.dnc_schedule_end);
	return sprintf(data, "%06d%06d", sys_info->ipcam[DNC_SCHEDULE_START].config.u32, sys_info->ipcam[DNC_SCHEDULE_END].config.u32);
}
#else
int para_dncontrolschedule(request *req, char *data, char *arg)
{
	//return sprintf(data, "%06X", sys_info->config.lan_config.dnc_schedule);
	return sprintf(data, "%06X", sys_info->ipcam[DNC_SCHEDULE].config.u32);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontrolsen(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.dnc_sensitivity);
	return sprintf(data, "%d", sys_info->ipcam[DNC_SENSITIVITY].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontrold2n(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.dnc_d2n);
	return sprintf(data, "%d", sys_info->ipcam[DNC_D2N].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontroln2d(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.lan_config.dnc_n2d);
	return sprintf(data, "%d", sys_info->ipcam[DNC_N2D].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontrold2nmin(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", DNC_D2N_MIN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontrold2nmax(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", DNC_D2N_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontroln2dmin(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", DNC_N2D_MIN);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontroln2dmax(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", DNC_N2D_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontrolsenname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Lowest");
	if ( *arg == '1' )
		return sprintf(data, "Low");
	if ( *arg == '2' )
		return sprintf(data, "Medium");
	if ( *arg == '3' )
		return sprintf(data, "High");
	if ( *arg == '4' )
		return sprintf(data, "Highest");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Lowest;Low;Medium;High;Highest");
	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_dncontrolmodename(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
	if ( *arg == '0' )
		return sprintf(data, "On Color");
	if ( *arg == '1' )
		return sprintf(data, "On Mono");
	if ( *arg == '2' )
		return sprintf(data, "Auto");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "On Color;On Mono;Auto");
#else
	if ( *arg == '0' )
		return sprintf(data, "Auto");
	if ( *arg == '1' )
		return sprintf(data, "Day mode");
	if ( *arg == '2' )
		return sprintf(data, "Night mode");
	if ( *arg == '3' )
		return sprintf(data, "Schedule");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Auto;Day mode;Night mode;Schedule");
#endif

	return -1;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpsenable(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.https.https_enable);
	return sprintf(data, "%d", sys_info->ipcam[HTTPS_ENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpscountry(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.https.country_name);
	return sprintf(data, "%s", conf_string_read(HTTPS_COUNTRY_NAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxhttpscountry(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", COUNTRY_NAME_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpsstate(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.https.state_name);
	return sprintf(data, "%s", conf_string_read(HTTPS_STATE_NAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxhttpsstate(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", STATE_NAME_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpslocality(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.https.locality_name);
	return sprintf(data, "%s", conf_string_read(HTTPS_LOCALITY_NAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxhttpslocality(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", LOCALITY_NAME_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpsorganization(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.https.organization_name);
	return sprintf(data, "%s", conf_string_read(HTTPS_ORGANIZATION_NAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxhttpsorganization(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", ORGANIZATION_NAME_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpsorgunit(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.https.organization_unit);
	return sprintf(data, "%s", conf_string_read(HTTPS_ORGANIZATION_UNIT));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxhttpsorgunit(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", ORGANIZATION_UNIT_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpscommon(request *req, char *data, char *arg)
{
	//return sprintf(data, "%s", sys_info->config.https.common_name);
	return sprintf(data, "%s", conf_string_read(HTTPS_COMMON_NAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxhttpscommon(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", COMMON_NAME_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpsvaliddays(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.https.validity_days);
	return sprintf(data, "%d", sys_info->ipcam[HTTPS_VALIDITY_DAYS].config.u32);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxhttpsvaliddays(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", VALIDITY_DAYS_MAX);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpscheckkey(request *req, char *data, char *arg)
{
	struct stat statbuf;

	if(stat(SSL_KEY_PATH, &statbuf) == 0)
		if (S_ISREG(statbuf.st_mode))
			return sprintf(data, "1");
	return sprintf(data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpscheckcsr(request *req, char *data, char *arg)
{
	struct stat statbuf;

	if(stat(SSL_CSR_PATH, &statbuf) == 0)
		if (S_ISREG(statbuf.st_mode))
			return sprintf(data, "1");
	return sprintf(data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpscheckcert(request *req, char *data, char *arg)
{
	struct stat statbuf;

	if(stat(SSL_CERT_PATH, &statbuf) == 0)
		if (S_ISREG(statbuf.st_mode))
			return sprintf(data, "1");
	return sprintf(data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpsport(request *req, char *data, char *arg)
{
	//return sprintf(data, "%d", sys_info->config.https.port);
	return sprintf(data, "%d", sys_info->ipcam[HTTPS_PORT].config.u16);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_defaulthttpsport(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", HTTPS_PORT_DEFAULT);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_wifi_support(request *req, char *data, char *arg)
{
#ifdef SUPPORT_WIFI
	return sprintf(data, "1");
#else
	return sprintf(data, "0");
#endif
}

#ifdef SUPPORT_WIFI
int para_wifi_enable(request *req, char *data, char *arg)
{
	return sprintf(data, "%u", sys_info->ipcam[WIFI_ENABLE].config.u32);
}

int para_wifi_ssid(request *req, char *data, char *arg)
{
	char *ssid, buf[80];

	ssid = conf_string_read(WIFI_SSID);
	base64_encode((unsigned char *)ssid, (unsigned char *)buf, strlen(ssid));

	return sprintf(data, "%s", buf);
}

int para_wifi_mode(request *req, char *data, char *arg)
{
	return sprintf(data, "%u", sys_info->ipcam[WIFI_MODE].config.u32);
}

int para_wifi_channel(request *req, char *data, char *arg)
{
	return sprintf(data, "%u", sys_info->ipcam[WIFI_CHANNEL].config.u32);
}

int para_wifi_auth(request *req, char *data, char *arg)
{
	return sprintf(data, "%u", sys_info->ipcam[WIFI_AUTH].config.u32);
}

int para_wifi_encrypt(request *req, char *data, char *arg)
{
	return sprintf(data, "%u", sys_info->ipcam[WIFI_ENCRYPT].config.u32);
}

int para_wpa_key(request *req, char *data, char *arg)
{
	return sprintf(data, DEFAULT_WPA_KEY);
}

int para_wep_index(request *req, char *data, char *arg)
{
	return sprintf(data, "%u", sys_info->ipcam[WIFI_WEP_INDEX].config.u32);
}

int para_wep_key1(request *req, char *data, char *arg)
{
	return sprintf(data, DEFAULT_WEP_KEY1);
}

int para_wep_key2(request *req, char *data, char *arg)
{
	return sprintf(data, DEFAULT_WEP_KEY2);
}

int para_wep_key3(request *req, char *data, char *arg)
{
	return sprintf(data, DEFAULT_WEP_KEY3);
}

int para_wep_key4(request *req, char *data, char *arg)
{
	return sprintf(data, DEFAULT_WEP_KEY4);
}

int para_wifi_sitelist(request *req, char *data, char *arg)
{
	return sprintf(data, "===SSID List===");
}

int para_wifi_modename(request *req, char *data, char *arg)
{
	return sprintf(data, "Infrastructure;Ad-Hoc");
}

int para_wifi_minchannel(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->wifi_minchannel);
}

int para_wifi_maxchannel(request *req, char *data, char *arg)
{
	return sprintf(data, "%d", sys_info->wifi_maxchannel);
}

int para_wifi_channelname(request *req, char *data, char *arg)
{
	if (sys_info->wifi_maxchannel == 11)
		return sprintf(data, "1;2;3;4;5;6;7;8;9;10;11");
	if (sys_info->wifi_maxchannel == 13)
		return sprintf(data, "1;2;3;4;5;6;7;8;9;10;11;12;13");
	return sprintf(data, "1;2;3;4;5;6;7;8;9;10;11;12;13;14");
}

int para_wifi_authname(request *req, char *data, char *arg)
{
	return sprintf(data, "Open;Shared;WPA-PSK;WPA2-PSK");
}

int para_wifi_encryptname(request *req, char *data, char *arg)
{
	return sprintf(data, "Disable;WEP;TKIP,AES");
}

#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_event_location(request *req, char *data, char *arg)
{
	extern unsigned int event_location;
	return sprintf(data, "%d", event_location);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_get_event(request *req, char *data, char *arg)
{
	extern unsigned int event_location;
#ifndef MSGLOGNEW
	SYSLOG tLog;
#endif
	int i = atoi(arg) + event_location;
	/* struct tm* ptm; */
	
	if( (i - event_location) == 20)
		event_location = 0;

	if(i < 0) {
		return -1;
	}
#ifdef MSGLOGNEW																//ryan
		syslog_string_new(i, data, sys_info->ipcam[TIME_FORMAT].config.u8, 0);	//ryan
		//strcpy(data, "\0");														//ryan
#else
	if(GetSysLog(i, 0, &tLog) == -1) {
		return -1;
	}
	
	//if( syslog_string(&tLog, data, sys_info->config.lan_config.net.time_format) == NULL) {
	if( syslog_string(&tLog, data, sys_info->ipcam[TIME_FORMAT].config.u8) == NULL) {
		return -1;
	}
#endif	
	return strlen(data);

/*
	ptm = &tLog.time;
	return sprintf(data, "%d-%02d-%02d %02d:%02d:%02d %s",
					ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
					ptm->tm_hour, ptm->tm_min, ptm->tm_sec, tLog.event);
*/  /* By Alex  */
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_support_bonjour(request *req, char *data, char *arg)
{
#ifdef CONFIG_BRAND_DLINK
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", 0);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportaf(request *req, char *data, char *arg)
{
#ifdef SUPPORT_AF
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", 0);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_supportantiflicker(request *req, char *data, char *arg)
{
#ifdef SUPPORT_ANTIFLICKER
	return sprintf(data, "%d", 1);
#else
	return sprintf(data, "%d", 0);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_OEMModel(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", sys_info->oem_data.model_name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_OEMMaker(request *req, char *data, char *arg)
{
	return sprintf(data, "%s", sys_info->oem_data.company_name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#define	FM4005_SUPPORT		1
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rtc_value( request *req, char *data, char *arg )
{
#if FM4005_SUPPORT == 1
	struct rtc_pll_info rtcpllinfo;
	int fd;

	if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
		return -1;
	rtcpllinfo.pll_clock = 0;
	if ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) )
	{
		close( fd );
		return -1;
	}
	else
	{
		close ( fd );
		return sprintf( data, "%d", rtcpllinfo.pll_value );
	}
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rtc_status( request *req, char *data, char *arg )
{
#if FM4005_SUPPORT == 1
	struct rtc_pll_info rtcpllinfo;
	int fd;

	if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
		return -1;
	rtcpllinfo.pll_clock = 1;
	if ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) )
	{
		close( fd );
		return -1;
	}
	else
	{
		close ( fd );
		return sprintf( data, "%d", rtcpllinfo.pll_ctrl );
	}
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_event_server( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
	/*	SMTP : 0;0;S1;admin;9999;admin@gmail.com;25;user@yahoo.com;smtp@hotmail.com;;;;
		FTP :   1;1;S2;user;9999;user.com;21;;;;1;;
		samba : 3;3;S4;admin;9999;\nas_name\volumn1\myfile;;;;;;;			*/
		
	int i;
	__u32 ret = 0, cnt = 0;
	char *ptr = data;
	//EVENT_2G_SERVER *pserver;
	char buf[MAX_FQDN_LENGTH + 1] = "";
	int page = 0;
	
	for(i = 0; i < E2G_SERVER_MAX_NUM; i++) {

		//pserver = &sys_info->config.e2g_server[i];
		page = SERVER_STRUCT_SIZE*i;
		//if(pserver->info.valid == 0) {
		if(sys_info->ipcam[E2G_SERVER0_INFO_VALID+page].config.u8 == 0) {
			strcpy(ptr, "null");
			ptr += 4;
			ret += 4;
		}
		else {
			//switch(pserver->info.type) {
			switch(sys_info->ipcam[E2G_SERVER0_INFO_TYPE+page].config.u16) {
				cnt = 0;	

				case ESERVER_TYPE_SMTP:
					cnt = sprintf(ptr, "%d;%d;%s;%s;%s;%s;%d;%s;%s;;;;;%d", sys_info->ipcam[E2G_SERVER0_INFO_ID+page].config.u8, 0, conf_string_read(E2G_SERVER0_INFO_NAME+page),
						conf_string_read(E2G_SERVER0_SMTP_USERNAME+page), conf_string_read(E2G_SERVER0_SMTP_PASSWORD+page), conf_string_read(E2G_SERVER0_SMTP_FQDN+page), sys_info->ipcam[E2G_SERVER0_SMTP_PORT+page].config.u16,	
						conf_string_read(E2G_SERVER0_SMTP_FROM_MAIL+page), conf_string_read(E2G_SERVER0_SMTP_TO_MAIL+page), sys_info->ipcam[E2G_SERVER0_SMTP_ENCRYPTION+page].config.u8);
					break;
				
				case ESERVER_TYPE_FTP:
					cnt = sprintf(ptr, "%d;%d;%s;%s;%s;%s;%d;;;%s;%d;;;", sys_info->ipcam[E2G_SERVER0_INFO_ID+page].config.u8, 1, conf_string_read(E2G_SERVER0_INFO_NAME+page),
						conf_string_read(E2G_SERVER0_FTP_USERNAME+page), conf_string_read(E2G_SERVER0_FTP_PASSWORD+page), conf_string_read(E2G_SERVER0_FTP_FQDN+page), sys_info->ipcam[E2G_SERVER0_FTP_PORT+page].config.u16,
						conf_string_read(E2G_SERVER0_FTP_PATH+page), sys_info->ipcam[E2G_SERVER0_FTP_PASSIVE_MODE+page].config.u8);
					break;

				//case ESERVER_TYPE_HTTP:
				//	break;

				case ESERVER_TYPE_SAMBA:
					if( add_escape_char(conf_string_read(E2G_SERVER0_SAMBA_FQDN+page), buf, sizeof(buf)) == -1 )
						strcpy(conf_string_read(E2G_SERVER0_SAMBA_FQDN+page), "");

					cnt = sprintf(ptr, "%d;%d;%s;%s;%s;%s;;;;;;%s;%s;", sys_info->ipcam[E2G_SERVER0_INFO_ID+page].config.u8, 3, conf_string_read(E2G_SERVER0_INFO_NAME+page),
						conf_string_read(E2G_SERVER0_SAMBA_USERNAME+page), conf_string_read(E2G_SERVER0_SAMBA_PASSWORD+page), buf, conf_string_read(E2G_SERVER0_SAMBA_WORKGROUP+page),
						inet_ntoa(sys_info->ipcam[E2G_SERVER0_SAMBA_WINS+page].config.ip));
					break;

				case ESERVER_TYPE_SD:
					cnt = sprintf(ptr, "%d;%d;%s;;;;;;;%s;;;;", sys_info->ipcam[E2G_SERVER0_INFO_ID+page].config.u8, 4, conf_string_read(E2G_SERVER0_INFO_NAME+page), conf_string_read(E2G_SERVER0_SD_PATH+page));

					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("UNKNOWN EVENT SERVER TYPE %04X \n", sys_info->ipcam[E2G_SERVER0_INFO_TYPE+page].config.u16);
					break;
					
/*
				case ESERVER_TYPE_SMTP:
					cnt = sprintf(ptr, "%d;%d;%s;%s;%s;%s;%d;%s;%s;;;;", pserver->info.id, 0, pserver->info.name,
						pserver->smtp.username, pserver->smtp.password, pserver->smtp.fqdn, pserver->smtp.port,	
						pserver->smtp.from_mail, pserver->smtp.to_mail);
					break;

				case ESERVER_TYPE_FTP:
					cnt = sprintf(ptr, "%d;%d;%s;%s;%s;%s;%d;;;%s;%d;;", pserver->info.id, 1, pserver->info.name,
						pserver->ftp.username, pserver->ftp.password, pserver->ftp.fqdn, pserver->ftp.port,
						pserver->ftp.path, pserver->ftp.passive_mode);
					break;

				//case ESERVER_TYPE_HTTP:
				//	break;

				case ESERVER_TYPE_SAMBA:
					if( add_escape_char(pserver->samba.fqdn, buf, sizeof(buf)) == -1 )
						strcpy(pserver->samba.fqdn, "");

					cnt = sprintf(ptr, "%d;%d;%s;%s;%s;%s;;;;;;%s;%s", pserver->info.id, 3, pserver->info.name,
						pserver->samba.username, pserver->samba.password, buf, pserver->samba.workgroup,
						inet_ntoa(pserver->samba.wins));
					break;

				case ESERVER_TYPE_SD:
					cnt = sprintf(ptr, "%d;%d;%s;;;;;;;%s;;;", pserver->info.id, 4, pserver->info.name, pserver->sd.path);
					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("UNKNOWN EVENT SERVER TYPE %04X \n", pserver->info.type);
					break;*/
			}

			ptr += cnt;
			ret += cnt;
		}

		if(i < E2G_SERVER_MAX_NUM - 1) {
			//strcat(ptr, ",");
			//ptr += 1;
			*(ptr++) = ',';
			ret++;
		}
	}

	return ret;

#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_event_media( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G

	int i;
	__u32 ret = 0, cnt = 0;
	char *ptr = data;
	//EVENT_2G_MEDIA *pmedia;
	int page = 0;
	
	for(i = 0; i < E2G_MEDIA_MAX_NUM; i++) {
		//pmedia = &sys_info->config.e2g_media[i];
		page = MEDIA_STRUCT_SIZE*i;
		
		//if(pmedia->info.valid == 0) {
		if(sys_info->ipcam[E2G_MEDIA0_INFO_VALID+page].config.u8 == 0) {
			strcpy(ptr, "null");
			ptr += 4;
			ret += 4;
		}
		else {

			switch(sys_info->ipcam[E2G_MEDIA0_INFO_TYPE+page].config.u16) {
				cnt = 0;
				case EMEDIA_TYPE_PIC:
					cnt = sprintf(ptr, "%d;%d;%s;%d;%d;%d;%d;;%s", sys_info->ipcam[E2G_MEDIA0_INFO_ID+page].config.u8, 0, conf_string_read(E2G_MEDIA0_INFO_NAME+page),
						sys_info->ipcam[E2G_MEDIA0_PIC_SOURCE+page].config.u8, sys_info->ipcam[E2G_MEDIA0_PIC_PRE_EVENT+page].config.u16, sys_info->ipcam[E2G_MEDIA0_PIC_POST_EVENT+page].config.u16, 
						sys_info->ipcam[E2G_MEDIA0_PIC_FILENAME_SUFFIX+page].config.u8, conf_string_read(E2G_MEDIA0_PIC_FILENAME+page));
					break;

				case EMEDIA_TYPE_VIDEO:
					cnt = sprintf(ptr, "%d;%d;%s;%d;%d;%d;%d;%u;%s", sys_info->ipcam[E2G_MEDIA0_INFO_ID+page].config.u8, 1, conf_string_read(E2G_MEDIA0_INFO_NAME+page),
						sys_info->ipcam[E2G_MEDIA0_VIDEO_SOURCE+page].config.u8, sys_info->ipcam[E2G_MEDIA0_VIDEO_PRE_EVENT+page].config.u16, sys_info->ipcam[E2G_MEDIA0_VIDEO_DURATION+page].config.u8, 
						sys_info->ipcam[E2G_MEDIA0_VIDEO_FILENAME_SUFFIX+page].config.u8, sys_info->ipcam[E2G_MEDIA0_VIDEO_MAXSIZE+page].config.u32, conf_string_read(E2G_MEDIA0_VIDEO_FILENAME+page));
					break;
					
				case EMEDIA_TYPE_LOG:
					cnt = sprintf(ptr, "%d;%d;%s;;;;;;", sys_info->ipcam[E2G_MEDIA0_INFO_ID+page].config.u8, 2, conf_string_read(E2G_MEDIA0_INFO_NAME+page));
					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("NUMBDER %d UNKNOWN MEDIA TYPE %04X ", i, sys_info->ipcam[E2G_MEDIA0_INFO_TYPE+page].config.u16);
					break;
			}
			/*
			switch(pmedia->info.type) {
				cnt = 0;	
				case EMEDIA_TYPE_PIC:
					cnt = sprintf(ptr, "%d;%d;%s;%d;%d;%d;%d;;%s", pmedia->info.id, 0, pmedia->info.name,
						pmedia->pic.source, pmedia->pic.pre_event, pmedia->pic.post_event, 
						pmedia->pic.filename_suffix, pmedia->pic.filename);
					break;

				case EMEDIA_TYPE_VIDEO:
					cnt = sprintf(ptr, "%d;%d;%s;%d;%d;%d;%d;%u;%s", pmedia->info.id, 1, pmedia->info.name,
						pmedia->video.source, pmedia->video.pre_event, pmedia->video.duration, 
						pmedia->video.filename_suffix, pmedia->video.maxsize, pmedia->video.filename);
					break;
					
				case EMEDIA_TYPE_LOG:
					cnt = sprintf(ptr, "%d;%d;%s;;;;;;", pmedia->info.id, 2, pmedia->info.name);
					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("UNKNOWN MEDIA TYPE %04X \n", pmedia->info.type);
					break;
			}*/

			ptr += cnt;
			ret += cnt;
		}

		if(i < E2G_MEDIA_MAX_NUM - 1) {
			//strcat(ptr, ",");
			//ptr += 1;
			*(ptr++) = ',';
			ret++;
		}
	}

	return ret;

#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_event_type( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G

	int i;
	__u32 ret = 0, cnt = 0;
	char *ptr = data;
	//EVENT_2G_TYPE *pevent;
	int page = 0;
	
	for(i = 0; i < E2G_EVENT_MAX_NUM; i++) {
		//pevent = &sys_info->config.e2g_event[i];
		page = EVENT_STRUCT_SIZE*i;
		
		//if(pevent->info.valid == 0) {
		if(sys_info->ipcam[E2G_EVENT0_INFO_VALID+page].config.u8 == 0) {
			strcpy(ptr, "null");
			ptr += 4;
			ret += 4;
		}
		else {
			
			switch(sys_info->ipcam[E2G_EVENT0_INFO_TYPE+page].config.u16) {
				cnt = 0;	
				case EVENT_TYPE_NORMAL:
					#ifdef SUPPORT_VISCA
					cnt = sprintf(ptr, "%d;%d;%d;%d;%d;%d;%s;%02x;%d;%02d%02d%02d%02d%02d%02d;%d;%d;%02x;%05x;%04x;%d;%d;%d", sys_info->ipcam[E2G_EVENT0_INFO_ID+page].config.u8, sys_info->ipcam[E2G_EVENT0_INFO_ENABLE+page].config.u8,
						sys_info->ipcam[E2G_EVENT0_NORMAL_PRIORITY+page].config.u8, sys_info->ipcam[E2G_EVENT0_NORMAL_DELAY+page].config.u16, sys_info->ipcam[E2G_EVENT0_NORMAL_TRIGGER_TYPE+page].config.u16,
						sys_info->ipcam[E2G_EVENT0_NORMAL_PERIODIC_TIME+page].config.u32, conf_string_read(E2G_EVENT0_INFO_NAME+page), sys_info->ipcam[E2G_EVENT0_SCHEDULE_DAY +page].config.u16, sys_info->ipcam[E2G_EVENT0_SCHEDULE_MODE+page].config.u16, 
						sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NHOUR+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NMIN+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NSEC+page].config.value,
						sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NHOUR+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NMIN+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NSEC+page].config.value,
						sys_info->ipcam[E2G_EVENT0_NORMAL_GIOOUT_TRIG+page].config.u16, sys_info->ipcam[E2G_EVENT0_NORMAL_GIOOUT_DURATION+page].config.u32, 
						sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_SERVER+page].config.u16, sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA+page].config.u32, sys_info->ipcam[E2G_EVENT0_NORMAL_GIOIN_TRIG+page].config.u16,
						sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_PTZ_EN+page].config.u8, sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_PTZ_MODE+page].config.u8, sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_PTZ_M_ARG+page].config.u8+1);
					#else
					cnt = sprintf(ptr, "%d;%d;%d;%d;%d;%d;%s;%02x;%d;%02d%02d%02d%02d%02d%02d;%d;%d;%02x;%05x;%04x", sys_info->ipcam[E2G_EVENT0_INFO_ID+page].config.u8, sys_info->ipcam[E2G_EVENT0_INFO_ENABLE+page].config.u8,
						sys_info->ipcam[E2G_EVENT0_NORMAL_PRIORITY+page].config.u8, sys_info->ipcam[E2G_EVENT0_NORMAL_DELAY+page].config.u16, sys_info->ipcam[E2G_EVENT0_NORMAL_TRIGGER_TYPE+page].config.u16,
						sys_info->ipcam[E2G_EVENT0_NORMAL_PERIODIC_TIME+page].config.u32, conf_string_read(E2G_EVENT0_INFO_NAME+page), sys_info->ipcam[E2G_EVENT0_SCHEDULE_DAY +page].config.u16, sys_info->ipcam[E2G_EVENT0_SCHEDULE_MODE+page].config.u16, 
						sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NHOUR+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NMIN+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NSEC+page].config.value,
						sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NHOUR+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NMIN+page].config.value, sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NSEC+page].config.value,
						sys_info->ipcam[E2G_EVENT0_NORMAL_GIOOUT_TRIG+page].config.u16, sys_info->ipcam[E2G_EVENT0_NORMAL_GIOOUT_DURATION+page].config.u32, 
						sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_SERVER+page].config.u16, sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA+page].config.u32, sys_info->ipcam[E2G_EVENT0_NORMAL_GIOIN_TRIG+page].config.u16);
					#endif

					dbg("ptr = %s ", ptr);

					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("NUMBER %d UNKNOWN EVENT TYPE %04X ", i, sys_info->ipcam[E2G_EVENT0_INFO_TYPE].config.u16);
					break;
			}
			/*switch(pevent->info.type) {
				cnt = 0;	

				case EVENT_TYPE_NORMAL:
					cnt = sprintf(ptr, "%d;%d;%d;%d;%d;%d;%s;%02x;%d;%02d%02d%02d%02d%02d%02d;%d;%d;%02x;%05x", pevent->info.id, pevent->info.enable,
						pevent->normal.priority, pevent->normal.delay, pevent->normal.trigger_type,
						pevent->normal.periodic_time, pevent->info.name, pevent->schedule.day, pevent->schedule.mode, 
						pevent->schedule.start.nHour, pevent->schedule.start.nMin, pevent->schedule.start.nSec,
						pevent->schedule.end.nHour, pevent->schedule.end.nMin, pevent->schedule.end.nSec,
						pevent->normal.gioout_trig, pevent->normal.gioout_duration[0], 
						pevent->normal.action_server, pevent->normal.action_media );
					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("UNKNOWN EVENT TYPE %04X \n", pevent->info.type);
					break;
			}*/

			ptr += cnt;
			ret += cnt;
		}

		if(i < E2G_EVENT_MAX_NUM - 1) {
			//strcat(ptr, ",");
			//ptr += 1;
			*(ptr++) = ',';
			ret++;
		}
	}

	return ret;

#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_event_record( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G

	int i;
	__u32 ret = 0, cnt = 0;
	char *ptr = data;
	//EVENT_2G_RECORD *precord;
	int page = 0;
	
	for(i = 0; i < E2G_RECORD_MAX_NUM; i++) {
		//precord = &sys_info->config.e2g_record[i];
		page = RECORD_STRUCT_SIZE*i;

		//if(precord->info.valid == 0) {
		if(sys_info->ipcam[E2G_RECORD0_INFO_VALID+page].config.u8 == 0) {
			strcpy(ptr, "null");
			ptr += 4;
			ret += 4;
		}
		else {
			cnt = 0;
			dbg("page = %d ", page);
			dbg("sys_info->ipcam[E2G_RECORD0_INFO_TYPE+page].config.u16 = %d ", sys_info->ipcam[E2G_RECORD0_INFO_TYPE+page].config.u16);
			switch(sys_info->ipcam[E2G_RECORD0_INFO_TYPE+page].config.u16) {
				case ERECORD_TYPE_NORMAL:
					cnt = sprintf(ptr, "%d;%d;%d;%02d;%s;%02x;%d;%02d%02d%02d%02d%02d%02d;%d;%u;%u;%s", 
						sys_info->ipcam[E2G_RECORD0_INFO_ID+page].config.u8, sys_info->ipcam[E2G_RECORD0_INFO_ENABLE+page].config.u8, sys_info->ipcam[E2G_RECORD0_NORMAL_PRIORITY+page].config.u8, 
						sys_info->ipcam[E2G_RECORD0_NORMAL_SOURCE+page].config.u8, conf_string_read(E2G_RECORD0_INFO_NAME+page), 
						sys_info->ipcam[E2G_RECORD0_SCHEDULE_DAY+page].config.u16, sys_info->ipcam[E2G_RECORD0_SCHEDULE_MODE+page].config.u16, 
						sys_info->ipcam[E2G_RECORD0_SCHEDULE_START_NHOUR+page].config.value, sys_info->ipcam[E2G_RECORD0_SCHEDULE_START_NMIN+page].config.value, sys_info->ipcam[E2G_RECORD0_SCHEDULE_START_NSEC+page].config.value,
						sys_info->ipcam[E2G_RECORD0_SCHEDULE_END_NHOUR+page].config.value, sys_info->ipcam[E2G_RECORD0_SCHEDULE_END_NMIN+page].config.value, sys_info->ipcam[E2G_RECORD0_SCHEDULE_END_NSEC+page].config.value,
						sys_info->ipcam[E2G_RECORD0_NORMAL_DEST_SERVER+page].config.u16, sys_info->ipcam[E2G_RECORD0_NORMAL_REWRITE_LIMIT+page].config.u32, sys_info->ipcam[E2G_RECORD0_NORMAL_FILE_MAX_SIZE+page].config.u32, 
						conf_string_read(E2G_RECORD0_NORMAL_FILENAME+page));
					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("NUMBER %d UNKNOWN RECORD TYPE %04X ", i, sys_info->ipcam[E2G_RECORD0_INFO_VALID+page].config.u16);
					break;
			}
			/*switch(precord->info.type) {
				case ERECORD_TYPE_NORMAL:
					cnt = sprintf(ptr, "%d;%d;%d;%02d;%s;%02x;%d;%02d%02d%02d%02d%02d%02d;%d;%u;%u;%s", 
						precord->info.id, precord->info.enable, precord->normal.priority, 
						precord->normal.source, precord->info.name, 
						precord->schedule.day, precord->schedule.mode, 
						precord->schedule.start.nHour, precord->schedule.start.nMin, precord->schedule.start.nSec,
						precord->schedule.end.nHour, precord->schedule.end.nMin, precord->schedule.end.nSec,
						precord->normal.dest_server, precord->normal.rewrite_limit, precord->normal.file_max_size, 
						precord->normal.filename);
					break;

				default:
					strcpy(ptr, "null");
					ptr += 4;
					ret += 4;
					DBG("UNKNOWN RECORD TYPE %04X ", precord->info.type);
					break;
			}*/

			ptr += cnt;
			ret += cnt;
		}

		if(i < E2G_RECORD_MAX_NUM - 1) {
			//strcat(ptr, ",");
			//ptr += 1;
			*(ptr++) = ',';
			ret++;
		}
	}

	return ret;

#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_empty_smb( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
	int i;
	for(i = 0; i < E2G_SERVER_MAX_NUM; i++) {
		//if(sys_info->config.e2g_server[i].info.type == ESERVER_TYPE_SAMBA)
		if(sys_info->ipcam[E2G_SERVER0_INFO_TYPE+SERVER_STRUCT_SIZE*i].config.u16 == ESERVER_TYPE_SAMBA)
			return sprintf( data, "0" );
	}
#endif
	return sprintf( data, "1" );
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_empty_sd( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
	int i;
	for(i = 0; i < E2G_SERVER_MAX_NUM; i++) {
		//if(sys_info->config.e2g_server[i].info.type == ESERVER_TYPE_SD)
		if(sys_info->ipcam[E2G_SERVER0_INFO_TYPE+SERVER_STRUCT_SIZE*i].config.u16 == ESERVER_TYPE_SD)
			return sprintf( data, "0" );
	}
#endif
	return sprintf( data, "1" );
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_support_netlost( request *req, char *data, char *arg )
{
#ifdef SUPPORT_NETWOK_LOST
	return sprintf( data, "1");
#else
	return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_support_videolost( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VIDEO_LOST
	return sprintf( data, "1");
#else
	return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_support_pir( request *req, char *data, char *arg )
{
#ifdef SUPPORT_PIR
	return sprintf( data, "1");
#else
	return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_pic_pre_max( request *req, char *data, char *arg )
{
    // event media snapshot maximum images of pre-event
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MAX_E2G_PIC_PRE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_pic_pre_min( request *req, char *data, char *arg )
{
    // event media snapshot minimum images of pre-event
    return sprintf( data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_pic_post_max( request *req, char *data, char *arg )
{
    // event media snapshot maximum images of post-event
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MAX_E2G_PIC_POST);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_pic_post_min( request *req, char *data, char *arg )
{
    // event media snapshot minimum images of post-event
    return sprintf( data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_vid_pre_max( request *req, char *data, char *arg )
{
    // event media videoclip maximum duration of pre-event
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MAX_E2G_VID_PRE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_vid_pre_min( request *req, char *data, char *arg )
{
    // event media videoclip minimum duration of pre-event
    return sprintf( data, "0");
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_vid_post_max( request *req, char *data, char *arg )
{
    // event media videoclip maximum duration of post-event
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MAX_E2G_VID_DUR);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_vid_post_min( request *req, char *data, char *arg )
{
    // event media videoclip minimum duration of post-event
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "1");
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_vid_maxsize( request *req, char *data, char *arg )
{
// event media video maximum file size
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MAX_E2G_VID_FILESIZE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_m_vid_minsize( request *req, char *data, char *arg )
{
// event media video minimum file size
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MIN_E2G_VID_FILESIZE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_r_vid_maxsize( request *req, char *data, char *arg )
{
// event media video maximum file size
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MAX_E2G_REC_FILESIZE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_r_vid_minsize( request *req, char *data, char *arg )
{
// event media video minimum file size
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MIN_E2G_REC_FILESIZE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rewrite_maxsize( request *req, char *data, char *arg )
{
// event media video maximum file size
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MAX_E2G_REWRITE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rewrite_minsize( request *req, char *data, char *arg )
{
// event media video minimum file size
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "%d", MIN_E2G_REWRITE);
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxactionmedia( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
    return sprintf( data, "5");
#else
    return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxeventserver( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
	return sprintf( data, "%d", E2G_SERVER_MAX_NUM);
#else
	return sprintf( data, "0");
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxeventmedia( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
	return sprintf( data, "%d", E2G_MEDIA_MAX_NUM);
#else
	return sprintf( data, "0");
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxeventnumber( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
	return sprintf( data, "%d", E2G_EVENT_MAX_NUM);
#else
	return sprintf( data, "0");
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_maxrecording( request *req, char *data, char *arg )
{
#ifdef SUPPORT_EVENT_2G
	return sprintf( data, "%d", E2G_RECORD_MAX_NUM);
#else
	return sprintf( data, "0");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_event_actionchoice( request *req, char *data, char *arg )
{
#ifdef EVENT_ACTION_CHOICE
	return sprintf( data, "0");		//single choice
#else
	return sprintf( data, "1");		//multiple choice
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_rate( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_rate
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_duration( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_duration
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_count( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_count
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_maxrecord( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_maxrecord
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_left( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_left
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_used( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_used
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_durationname( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_durationname
#else
	return -1;
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_cf_recordtype( request *req, char *data, char *arg )
{
#if SUPPORT_CF_CARD == 1
#error NOT DEFINED para_cf_durationname
#else
	return -1;
#endif
}

int para_savingmode( request *req, char *data, char *arg )
{
	//return sprintf( data, "%d", (sys_info->config.power_mode & POWER_MODE_SAVING_ALL)==POWER_MODE_SAVING_ALL );
	return sprintf( data, "%d", (sys_info->ipcam[POWER_MODE].config.value & POWER_MODE_SAVING_ALL)==POWER_MODE_SAVING_ALL );
}

int para_analog_output( request *req, char *data, char *arg )
{
	//return sprintf( data, "%d", (sys_info->config.power_mode & POWER_MODE_ANALOG_OUT)==POWER_MODE_ANALOG_OUT );
	return sprintf( data, "%d", (sys_info->ipcam[POWER_MODE].config.value & POWER_MODE_ANALOG_OUT)==POWER_MODE_ANALOG_OUT );
}
/*
int para_supportpower( request *req, char *data, char *arg )
{
	return sprintf( data, "%d" , SUPPORT_DCPOWER);
}
*/
int para_supportdcpower( request *req, char *data, char *arg )
{
#ifdef PLAT_DM365	//by jiahung
	#if defined( MODEL_LC7415 ) || defined( MODEL_LC7415B )|| defined( MODEL_LC7515 )
	return sprintf( data, "%d", SUPPORT_DCPOWER);
	#else
	return sprintf( data, "0" );
	#endif
#else
	return sprintf( data, "%d" , SUPPORT_DCPOWER);
#endif
}

int para_supportirled( request *req, char *data, char *arg )
{
#ifdef SUPPORT_IR
	return sprintf( data, "1" );
#else
	return sprintf( data, "0" );
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if defined SENSOR_USE_TVP5150 || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined SENSOR_USE_TVP5150_MDIN270
int get_video_loss_status( request *req, char *data, char *arg  )
{
	return sprintf(data, "%d", sys_info->videoloss);
}
#endif

int para_supporttvout( request *req, char *data, char *arg  )
{
	return sprintf( data, "%d", SUPPORT_TVOUT_OFF );
}

int para_supportled( request *req, char *data, char *arg  )
{
	return sprintf( data, "%d", SUPPORT_LED );
}

int para_machinetype( request *req, char *data, char *arg  )
{
#if defined( MACHINE_TYPE_LANCAM ) || defined(MODEL_VS2415)
	return sprintf( data, "0" );
#elif defined( MACHINE_TYPE_VIDEOSERVER )
	return sprintf( data, "1" );
#else
#error Must define MACHINE_TYPE into Lancam or Video Server
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_rtspstreamnum( request *req, char *data, char *arg )
{
#ifdef SUPPORT_PROFILE_NUMBER
	return sprintf( data, "%d",sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value);
#else
	return sprintf( data, "%d",MAX_WEB_PROFILE_NUM);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_httpstreamnum( request *req, char *data, char *arg )
{
#ifdef SUPPORT_PROFILE_NUMBER
	return sprintf( data, "%d",sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value);
#else
	return sprintf( data, "%d",MAX_WEB_PROFILE_NUM);
#endif

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_profilecheckmode( request *req, char *data, char *arg )
{
    // 0 is no-block mode
    // 1 is for dm355 series
#ifdef PLAT_DM365
    return sprintf( data, "0");
#else
    return sprintf( data, "1");
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int para_afmaxzoomstep( request *req, char *data, char *arg )
{

	return sprintf( data, "%d", MAX_AF_ZOOM);

}
int para_afmaxfocusstep( request *req, char *data, char *arg )
{

   return sprintf( data, "%d", MAX_AF_FOCUS);
}
int para_autofocusmode( request *req, char *data, char *arg )
{
	//dbg("sys_info->af_status.mode = %d\n", sys_info->af_status.mode);
	//dbg("sys_info->ipcam[AUTOFOCUS_MODE].config.value = %d\n", sys_info->ipcam[AUTOFOCUS_MODE].config.value);
    return sprintf( data, "%d", sys_info->ipcam[AUTOFOCUS_MODE].config.value);
}
int para_autofocuslocation( request *req, char *data, char *arg )
{
	
    //dbg("sys_info->ipcam[AUTOFOCUS_LOCATION].config.value = %d\n", sys_info->ipcam[AUTOFOCUS_LOCATION].config.value);
    return sprintf( data, "%d", sys_info->ipcam[AUTOFOCUS_LOCATION].config.value);
}

int para_afzoomposition( request *req, char *data, char *arg )
{

    //dbg("AUTOFOCUS_ZOOM_POSTITION = %d\n", (MAX_AF_ZOOM - sys_info->ipcam[AUTOFOCUS_ZOOM_POSTITION].config.value));
    return sprintf( data, "%d", (MAX_AF_ZOOM - sys_info->af_status.zoom));
}
int para_affocusposition ( request *req, char *data, char *arg )
{

      //dbg("AUTOFOCUS_FOCUS_POSTITION = %d\n", sys_info->ipcam[AUTOFOCUS_FOCUS_POSTITION].config.value);
    return sprintf( data, "%d", sys_info->af_status.focus);
}
int para_lensspeed ( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[LENS_SPEED].config.value);
}
int para_eptzspeed ( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[EPTZ_SPEED].config.value);
}
int para_eptzstreamid ( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[EPTZ_STREAMID].config.value);
}

/*int para_eptzpresetlist ( request *req, char *data, char *arg )
{

	char *ptr = data;

	int tmp;
	char temp[32];
	int index= -1;
	int i, num=0;
	char buf[256];
	__u32 ret = 0, cnt = 0;
	int page = 0;
	
	if ( *arg == 'a' || *arg == '\0' ) {
		
	page = (sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<20;i++)
	{
		*temp = '\0';
		cnt = 0;
		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%s", &tmp, &tmp, &tmp, &tmp, temp);
		
		if(strcmp(temp, "") != 0){
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			cnt += sprintf(ptr, "%s", temp);
			ptr += cnt;
			ret += cnt;
			//dbg("ptr=%s", ptr);
			
			
		}
	}
		}
	return ret;
}*/
int para_eptzpresetlist1 ( request *req, char *data, char *arg )
{

	char *ptr = data;

	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	
	if ( *arg == 'a' || *arg == '\0' ) {

		cnt += sprintf(ptr, "--Preset List--");
		ptr += cnt;
		ret += cnt;
		
		page = 0;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
		for(i=0;i<MAX_EPTZPRESET;i++)
		{
			*temp = '\0';
			cnt = 0;
			sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, temp);
			
			if(strcmp(temp, "") != 0){
				if(num == 0){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%s", temp);
				ptr += cnt;
				ret += cnt;
				//dbg("ptr=%s", ptr);
			}
		}
	}
	
	if(ret != 0)
		return ret;
	else
		 return sprintf( data, " ");
}
int para_eptzpresetlist2 ( request *req, char *data, char *arg )
{

	char *ptr = data;

	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	
	if ( *arg == 'a' || *arg == '\0' ) {

			cnt += sprintf(ptr, "--Preset List--");
		ptr += cnt;
		ret += cnt;
		
	page = 1*20;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_EPTZPRESET;i++)
	{
		*temp = '\0';
		cnt = 0;
		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, temp);
		
		if(strcmp(temp, "") != 0){
			if(num == 0){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			cnt += sprintf(ptr, "%s", temp);
			ptr += cnt;
			ret += cnt;
			//dbg("ptr=%s", ptr);
			
			
		}
	}
		}

	if(ret != 0)
		return ret;
	else
		 return sprintf( data, " ");
}
int para_eptzpresetlist3 ( request *req, char *data, char *arg )
{

	char *ptr = data;

	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	
	if ( *arg == 'a' || *arg == '\0' ) {

			cnt += sprintf(ptr, "--Preset List--");
		ptr += cnt;
		ret += cnt;
		
	page = 2*20;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_EPTZPRESET;i++)
	{
		*temp = '\0';
		cnt = 0;
		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i), "%d:%d:%d:%d:%d:%d:%s", &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, temp);
		
		if(strcmp(temp, "") != 0){
			if(num == 0){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			cnt += sprintf(ptr, "%s", temp);
			ptr += cnt;
			ret += cnt;
			//dbg("ptr=%s", ptr);
			
			
		}
	}
		}
	if(ret != 0)
		return ret;
	else
		 return sprintf( data, " ");
}


int para_sequencename1 ( request *req, char *data, char *arg )
{

	char *ptr = data;

	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	int idx = 0;
	
	if ( *arg == 'a' || *arg == '\0' ) {
		
	page = 0;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_SEQUENCE;i++)
	{

		if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value != 0){
			//conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i)
		
		
			*temp = '\0';
			cnt = 0;
			sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value - 1)), "%d:%d:%d:%d:%d:%d:%s", &tmp, &idx, &tmp, &tmp, &tmp, &tmp, temp);
			dbg("<%d>=%d",i,idx);
			//if(idx == sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value){
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%s", temp);
				ptr += cnt;
				ret += cnt;
				//dbg("ptr=%s", ptr);
				
				
			//}
		}
	}
		}
	return ret;
}
int para_sequencename2 ( request *req, char *data, char *arg )
{

	char *ptr = data;

	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	int idx = 0;
	
	if ( *arg == 'a' || *arg == '\0' ) {
		
	page = 1*20;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_SEQUENCE;i++)
	{

		if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value != 0){
			//conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i)
		
		
			*temp = '\0';
			cnt = 0;
			sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value - 1)), "%d:%d:%d:%d:%d:%d:%s", &tmp, &idx, &tmp, &tmp, &tmp, &tmp, temp);
			dbg("<%d>=%d",i,idx);
			//if(idx == sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value){
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%s", temp);
				ptr += cnt;
				ret += cnt;
				//dbg("ptr=%s", ptr);
				
				
			//}
		}
	}
		}
	return ret;
}
int para_sequencename3 ( request *req, char *data, char *arg )
{

	char *ptr = data;

	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	int idx = 0;
	
	if ( *arg == 'a' || *arg == '\0' ) {
		
	page = 2*20;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_SEQUENCE;i++)
	{

		if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value != 0){
			//conf_string_read((EPTZPRESET_STREAM1_ID01+page)+i)
		
		
			*temp = '\0';
			cnt = 0;
			sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value - 1)), "%d:%d:%d:%d:%d:%d:%s", &tmp, &idx, &tmp, &tmp, &tmp, &tmp, temp);
			dbg("<%d>=%d",i,idx);
			//if(idx == sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value){
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%s", temp);
				ptr += cnt;
				ret += cnt;
				//dbg("ptr=%s", ptr);
				
				
			//}
		}
	}
		}
	return ret;
}

int para_sequencetime1 ( request *req, char *data, char *arg )
{

	char *ptr = data;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	int seq_time = 10;
	
	if ( *arg == 'a' || *arg == '\0' ) {
		
	page = 0;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_SEQUENCE;i++)
	{
		if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value != 0){
			*temp = '\0';
			cnt = 0;
			//sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value - 1)), "%d:%d:%d:%d:%d:%d:%s", &tmp, &idx, &seq_time, &tmp, &tmp, &tmp, temp);
			seq_time = sys_info->ipcam[SEQUENCE1_TIME01+i].config.value;
			//if(idx != 0){
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%d", seq_time);
				ptr += cnt;
				ret += cnt;
				//dbg("ptr=%s", ptr);
				
				
			//}

		}
	}
		}
	return ret;
}
int para_sequencetime2 ( request *req, char *data, char *arg )
{

	char *ptr = data;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	int seq_time = 10;
	
	if ( *arg == 'a' || *arg == '\0' ) {
		
	page = 1*20;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_SEQUENCE;i++)
	{
		if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value != 0){
			*temp = '\0';
			cnt = 0;
			//sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value - 1)), "%d:%d:%d:%d:%d:%d:%s", &tmp, &idx, &seq_time, &tmp, &tmp, &tmp, temp);
			seq_time = sys_info->ipcam[SEQUENCE2_TIME01+i].config.value;
			//if(idx != 0){
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%d", seq_time);
				ptr += cnt;
				ret += cnt;
				//dbg("ptr=%s", ptr);
				
				
			//}

		}
	}
		}
	return ret;
}
int para_sequencetime3 ( request *req, char *data, char *arg )
{

	char *ptr = data;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page = 0;
	int seq_time = 10;
	
	if ( *arg == 'a' || *arg == '\0' ) {
		
	page = 2*20;//(sys_info->ipcam[EPTZ_STREAMID].config.value-1)*20;
	for(i=0;i<MAX_SEQUENCE;i++)
	{
		if(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value != 0){
			*temp = '\0';
			cnt = 0;
			//sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+page)+(sys_info->ipcam[(EPTZPRESET_STREAM1_SEQ01+page)+i].config.value - 1)), "%d:%d:%d:%d:%d:%d:%s", &tmp, &idx, &seq_time, &tmp, &tmp, &tmp, temp);
			seq_time = sys_info->ipcam[SEQUENCE3_TIME01+i].config.value;
			//if(idx != 0){
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%d", seq_time);
				ptr += cnt;
				ret += cnt;
				//dbg("ptr=%s", ptr);
				
				
			//}

		}
	}
		}
	return ret;
}

int para_zoomposinq ( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option  = 0;
	unsigned int var, info_value, info_extend;
	short int pos;
 //	parsing the instruction to option and arguement.
	option = CAM_ZOOM_POS_INQ;
	var = 0;
	
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address   = 1; 
	
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;

	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);

	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	
	//fprintf(stderr,"0x%08X\n",info_value);
	//fprintf(stderr," %08X\n",info_extend);
	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) ){
		pos = (interface.ibuf[2]<<12)+(interface.ibuf[3]<<8)+(interface.ibuf[4]<<4)+interface.ibuf[5];
		if((pos >= VISCA_30X_OPTICAL_1X) && (pos < VISCA_30X_OPTICAL_2X) )
			return sprintf(data, "Optical 1X");
		else if(((pos) >= VISCA_30X_OPTICAL_2X) && ((pos) < VISCA_30X_OPTICAL_3X) )
			return sprintf(data, "Optical 2X");
		else if((pos >= VISCA_30X_OPTICAL_3X) && (pos < VISCA_30X_OPTICAL_4X) )
			return sprintf(data, "Optical 3X");
		else if((pos >= VISCA_30X_OPTICAL_4X) && (pos < VISCA_30X_OPTICAL_5X) )
			return sprintf(data, "Optical 4X");
		else if((pos >= VISCA_30X_OPTICAL_5X) && (pos < VISCA_30X_OPTICAL_6X) )
			return sprintf(data, "Optical 5X");
		else if((pos >= VISCA_30X_OPTICAL_6X) && (pos < VISCA_30X_OPTICAL_7X) )
			return sprintf(data, "Optical 6X");
		else if((pos >= VISCA_30X_OPTICAL_7X) && (pos < VISCA_30X_OPTICAL_8X) )
			return sprintf(data, "Optical 7X");
		else if((pos >= VISCA_30X_OPTICAL_8X) && (pos < VISCA_30X_OPTICAL_9X) )
			return sprintf(data, "Optical 8X");
		else if((pos >= VISCA_30X_OPTICAL_9X) && (pos < VISCA_30X_OPTICAL_10X) )
			return sprintf(data, "Optical 9X");
		else if((pos >= VISCA_30X_OPTICAL_10X) && (pos < VISCA_30X_OPTICAL_11X) )
			return sprintf(data, "Optical 10X");
		else if((pos >= VISCA_30X_OPTICAL_11X) && (pos < VISCA_30X_OPTICAL_12X) )
			return sprintf(data, "Optical 11X");
		else if((pos >= VISCA_30X_OPTICAL_12X) && (pos < VISCA_30X_OPTICAL_13X) )
			return sprintf(data, "Optical 12X");
		else if((pos >= VISCA_30X_OPTICAL_13X) && (pos < VISCA_30X_OPTICAL_14X) )
			return sprintf(data, "Optical 13X");
		else if((pos >= VISCA_30X_OPTICAL_14X) && (pos < VISCA_30X_OPTICAL_15X) )
			return sprintf(data, "Optical 14X");
		else if((pos >= VISCA_30X_OPTICAL_15X) && (pos < VISCA_30X_OPTICAL_16X) )
			return sprintf(data, "Optical 15X");
		else if((pos >= VISCA_30X_OPTICAL_16X) && (pos < VISCA_30X_OPTICAL_17X) )
			return sprintf(data, "Optical 16X");
		else if((pos >= VISCA_30X_OPTICAL_17X) && (pos < VISCA_30X_OPTICAL_18X) )
			return sprintf(data, "Optical 17X");
		else if((pos >= VISCA_30X_OPTICAL_18X) && (pos < VISCA_30X_OPTICAL_19X) )
			return sprintf(data, "Optical 18X");
		else if((pos >= VISCA_30X_OPTICAL_19X) && (pos < VISCA_30X_OPTICAL_20X) )
			return sprintf(data, "Optical 19X");
		else if((pos >= VISCA_30X_OPTICAL_20X) && (pos < VISCA_30X_OPTICAL_21X) )
			return sprintf(data, "Optical 20X");
		else if((pos >= VISCA_30X_OPTICAL_21X) && (pos < VISCA_30X_OPTICAL_22X) )
			return sprintf(data, "Optical 21X");
		else if((pos >= VISCA_30X_OPTICAL_22X) && (pos < VISCA_30X_OPTICAL_23X) )
			return sprintf(data, "Optical 22X");
		else if((pos >= VISCA_30X_OPTICAL_23X) && (pos < VISCA_30X_OPTICAL_24X) )
			return sprintf(data, "Optical 23X");
		else if((pos >= VISCA_30X_OPTICAL_24X) && (pos < VISCA_30X_OPTICAL_25X) )
			return sprintf(data, "Optical 24X");
		else if((pos >= VISCA_30X_OPTICAL_25X) && (pos < VISCA_30X_OPTICAL_26X) )
			return sprintf(data, "Optical 25X");
		else if((pos >= VISCA_30X_OPTICAL_26X) && (pos < VISCA_30X_OPTICAL_27X) )
			return sprintf(data, "Optical 26X");
		else if((pos >= VISCA_30X_OPTICAL_27X) && (pos < VISCA_30X_OPTICAL_28X) )
			return sprintf(data, "Optical 27X");
		else if((pos >= VISCA_30X_OPTICAL_28X) && (pos < VISCA_30X_OPTICAL_29X) )
			return sprintf(data, "Optical 28X");
		else if((pos >= VISCA_30X_OPTICAL_29X) && (pos < VISCA_30X_OPTICAL_30X) )
			return sprintf(data, "Optical 29X");
		else if((pos == VISCA_30X_OPTICAL_30X) )
			return sprintf(data, "Optical 30X");
		else if((pos > VISCA_30X_OPTICAL_30X) && (pos < VISCA_30X_DIGITAL_2X) )
			return sprintf(data, "Digital 1X");
		else if((pos >= VISCA_30X_DIGITAL_2X) && (pos < VISCA_30X_DIGITAL_3X) )
			return sprintf(data, "Digital 2X");
		else if((pos >= VISCA_30X_DIGITAL_3X) && (pos < VISCA_30X_DIGITAL_4X) )
			return sprintf(data, "Digital 3X");
		else if((pos >= VISCA_30X_DIGITAL_4X) && (pos < VISCA_30X_DIGITAL_5X) )
			return sprintf(data, "Digital 4X");
		else if((pos >= VISCA_30X_DIGITAL_5X) && (pos < VISCA_30X_DIGITAL_6X) )
			return sprintf(data, "Digital 5X");
		else if((pos >= VISCA_30X_DIGITAL_6X) && (pos < VISCA_30X_DIGITAL_7X) )
			return sprintf(data, "Digital 6X");
		else if((pos >= VISCA_30X_DIGITAL_7X) && (pos < VISCA_30X_DIGITAL_8X) )
			return sprintf(data, "Digital 7X");
		else if((pos >= VISCA_30X_DIGITAL_8X) && (pos < VISCA_30X_DIGITAL_9X) )
			return sprintf(data, "Digital 8X");
		else if((pos >= VISCA_30X_DIGITAL_9X) && (pos < VISCA_30X_DIGITAL_10X) )
			return sprintf(data, "Digital 9X");
		else if((pos >= VISCA_30X_DIGITAL_10X) && (pos < VISCA_30X_DIGITAL_11X) )
			return sprintf(data, "Digital 10X");
		else if((pos >= VISCA_30X_DIGITAL_11X) && (pos < VISCA_30X_DIGITAL_12X) )
			return sprintf(data, "Digital 11X");
		else if((pos == VISCA_30X_DIGITAL_12X) )
			return sprintf(data, "Digital 12X");
	}
	else
		goto VISCA_ERROR;
	

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_dzoommodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.dzoommode);
#else
	return -1;
#endif
}

int para_dzoomcsmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_DZOOM_CS_MODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
		
	//fprintf(stderr,"0x%08X\n",info_value);
	//fprintf(stderr," %08X\n",info_extend);
	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) ){
		if(interface.ibuf[2] == 0x00)
			mode = interface.ibuf[2];
		else
			goto VISCA_ERROR;
	}
	return sprintf(data, "%d", mode);	

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_dzoomposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t pos;
// parsing the instruction to option and arguement.
	option = CAM_DZOOM_POS_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		pos = (interface.ibuf[4]<<4)+(interface.ibuf[5]);
	else
		goto VISCA_ERROR;
	
	return sprintf(data, "%d", pos);	

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_focusmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_FOCUS_MODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;
	
	if(mode == 0x02){
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){
		mode=0;
		return sprintf(data, "%d", mode);
	}

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_focusposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt16_t pos;
// parsing the instruction to option and arguement.
	option = CAM_FOCUS_POS_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) ){
		pos = (interface.ibuf[2]<<12)+(interface.ibuf[3]<<8)+(interface.ibuf[4]<<4)+interface.ibuf[5];
		return sprintf(data, "%d", pos);
	}
	else
		goto VISCA_ERROR;

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_focusnearlimitinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt16_t pos;
// parsing the instruction to option and arguement.
	option = CAM_FOCUS_NEAR_LIMIT_POS_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) ){
		pos = (interface.ibuf[2]<<12)+(interface.ibuf[3]<<8)+(interface.ibuf[4]<<4)+interface.ibuf[5];
		return sprintf(data, "%d", pos);
	}
	else
		goto VISCA_ERROR;

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_afmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_AFMODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x00){	//Normal
		mode=0;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x01){	//Interval
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x02){	//Zoom Trigger
		mode=2;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//P.T. Trigger
		mode=3;
		return sprintf(data, "%d", mode);
	}

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_wbmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%s", sys_info->vsptz.wbmode);
#else
	return -1;
#endif
}

int para_rgaininq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t pos;
// parsing the instruction to option and arguement.
	option = CAM_RGAIN_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		pos = (interface.ibuf[4]<<4) + interface.ibuf[5];
	else
		goto VISCA_ERROR;

	return sprintf(data, "%d", pos);

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_bgaininq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t pos;
// parsing the instruction to option and arguement.
	option = CAM_BGAIN_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		pos = (interface.ibuf[4]<<4) + interface.ibuf[5];
	else
		goto VISCA_ERROR;

	return sprintf(data, "%d", pos);

VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_aemodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%s", sys_info->vsptz.aemode);
#else
	return -1;
#endif
}

int para_slowshuttermodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_SLOWSHUTTER_MODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//Auto
		mode=0;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//Off
		mode=1;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_shutterposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%s", sys_info->vsptz.shutterpos);
#else
	return -1;
#endif
}

int para_irisposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t pos;
// parsing the instruction to option and arguement.
	option = CAM_IRIS_POS_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		pos = (interface.ibuf[4]<<4)+interface.ibuf[5];
	else
		goto VISCA_ERROR;

	return sprintf(data, "%d", pos);
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_gainposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.gainpos);
#else
	return -1;
#endif
}

int para_brightposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t pos;
// parsing the instruction to option and arguement.
	option = CAM_BRIGHT_POS_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		pos = (interface.ibuf[4]<<4)+interface.ibuf[5];
	else
		goto VISCA_ERROR;

	return sprintf(data, "%d", pos);
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_expcompmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_EXP_COMP_MODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//On
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//Off
		mode=0;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_expcompposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.expcomp);
#else
	return -1;
#endif
}

int para_backlightmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.backlight);
#else
	return -1;
#endif
}

int para_wdinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.wdrinfo);
#else
	return -1;
#endif
}

int para_apertureinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.aperture);
#else
	return -1;
#endif
}

int para_freezemodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.freezemode);
#else
	return -1;
#endif
}

int para_pictureflipmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.pictureflipmode);
#else
	return -1;
#endif
}

#if 0	//Hitachi Only
int para_stabilizer( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_STABILIZER_MODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	else
		fprintf( stderr,"VISCA_output: Open serial device success\n" );
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	else
		fprintf( stderr,"VISCA_output: Close serial success\n\n" );

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//On
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//Off
		mode=0;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}
#endif

int para_icrmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_ICR_MODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//On
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//Off
		mode=0;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_autoicrmodeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_AUTO_ICR_MODE_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//On
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//Off
		mode=0;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_irthreshold( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t pos;
// parsing the instruction to option and arguement.
	option = CAM_IR_THRESHOLD_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		pos = (interface.ibuf[4]<<4)+(interface.ibuf[5]);
	else
		goto VISCA_ERROR;

	return sprintf(data, "%d", pos);
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_privacydisplayinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.privacydisplay);
#else
	return -1;
#endif
}

int para_privacytransparency( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.privacytransparency);
#else
	return -1;
#endif
}

int para_privacycolor( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%s", sys_info->vsptz.privacycolor);
#else
	return -1;
#endif
}

int para_pantiltposinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt16_t pan_pos, tilt_pos;
// parsing the instruction to option and arguement.
	option = CAM_PAN_TILT_POS_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) ){
		pan_pos = (interface.ibuf[2]<<12)+(interface.ibuf[3]<<8)+(interface.ibuf[4]<<4)+interface.ibuf[5];
		tilt_pos = (interface.ibuf[6]<<12)+(interface.ibuf[7]<<8)+(interface.ibuf[8]<<4)+interface.ibuf[9];		
	}
	else
		goto VISCA_ERROR;

	return sprintf(data, "%d;%d", pan_pos, tilt_pos);
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_versioninq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	char optzoom[4]="";
	char *optzoom_str;
	unsigned int var, info_value, info_extend;
	UInt8_t sequence_point, cruise_line, optical_zoom, type, icr, wdr;
#if 0
	UInt8_t dome_type, motor_type;
	UInt16_t firmware_version;
#endif
	optzoom_str=optzoom;
// parsing the instruction to option and arguement.
	option = CAM_VERSION_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	//interface.ibuf[5]=0x0D;	//test
	//interface.ibuf[6]=0x66; //test

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) ){
		sequence_point = (interface.ibuf[2] & 0x01);
		cruise_line = (interface.ibuf[2] & 0x02);
		//dome_type = interface.ibuf[3];
		//motor_type = interface.ibuf[4];
		if(interface.ibuf[5] == 0x0D){
			optical_zoom = (interface.ibuf[6]>>4) & 0x0F;
			type = (interface.ibuf[6] & 0x01); //NTSC OR PAL
			icr = (interface.ibuf[6] & 0x02); // no ICR/ICR
			wdr = (interface.ibuf[6] & 0x04); // no WDR/WDR
		}
		else
			goto VISCA_ERROR;
		//firmware_version = (interface.ibuf[7]<<8)+interface.ibuf[8];
	}
	else
		goto VISCA_ERROR;

	if(sequence_point == 0x00)
		sequence_point = 32;	
	else if(sequence_point == 0x01)
		sequence_point = 64;

	if(cruise_line == 0x00)
		cruise_line = 4;	
	else if(cruise_line == 0x02)
		cruise_line = 8;

	if(optical_zoom == 0x00)
		optzoom_str = "30x";
	else if(optical_zoom == 0x05)
		optzoom_str = "35x";
	else if(optical_zoom == 0x06)
		optzoom_str = "36x";

	if(type == 0x00)
		type = 0;	//NTSC
	else if(type == 0x01)
		type = 1;	//PAL
		
	if(icr == 0x00)
		icr = 0;	//No ICR
	else if(icr == 0x02)
		icr = 1;	//ICR

	if(wdr == 0x00)
		wdr = 0;	//No WDR
	else if(wdr == 0x04)
		wdr = 1;	//WDR
	
	return sprintf(data, "%d;%d;%s;%d;%d;%d", sequence_point, cruise_line, optzoom_str, type, icr, wdr);
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}


int para_alarmstatus( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t status;
// parsing the instruction to option and arguement.
	option = CAM_ALARM_STATUS_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		status = (interface.ibuf[4]<<4)+interface.ibuf[5];
	else
		goto VISCA_ERROR;

	return sprintf(data, "0x%02X", status);
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_alarmoutput( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_ALARM_OUTPUT_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//On
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//Off
		mode=0;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_homefunction( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.homefunctionmode);
#else
	return -1;
#endif
}

int para_fliptypeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%s", sys_info->vsptz.fliptype);
#else
	return -1;
#endif
}

int para_angleadjust( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d;%d", sys_info->vsptz.minangle, sys_info->vsptz.maxangle);
#else
	return -1;
#endif
}

int para_speedbyzoom( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode;
// parsing the instruction to option and arguement.
	option = CAM_SPEED_BY_ZOOM_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) )
		mode = interface.ibuf[2];
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//On
		mode=1;
		return sprintf(data, "%d", mode);
	}
	else if(mode == 0x03){	//Off
		mode=0;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}

int para_autocalibration( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.autocalibration);
#else
	return -1;
#endif
}

int para_2dnoisereduction( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.twodnoise);
#else
	return -1;
#endif
}

int para_3dnoisereduction( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.threednoise);
#else
	return -1;
#endif
}

#if 0	//Hitachi P Only
int para_noisereduction( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	VISCACamera_t camera;
	VISCAInterface_t  interface;
	int option	= 0;
	unsigned int var, info_value, info_extend;
	UInt8_t mode, level;
// parsing the instruction to option and arguement.
	option = CAM_NOISE_REDUCTION_INQ;
	var = 0;
		
	interface.broadcast = 0;
	camera.socket_num	= 2;	
	camera.address	 = 1; 
		
	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;
	else
		fprintf( stderr,"VISCA_output: Open serial device success\n" );
	
	VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend);
	
	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;
	else
		fprintf( stderr,"VISCA_output: Close serial success\n\n" );

	if( (interface.ibuf[0] == 0x90) && (interface.ibuf[1] == 0x50) ){
		mode = interface.ibuf[2];
		level = interface.ibuf[3];
	}
	else
		goto VISCA_ERROR;

	if(mode == 0x02){	//On
		mode=1;
		if(level ==0x00)
			level=0;
		else if(level == 0x01)
			level=1;
		else if(level == 0x02)
			level=2;
		else if(level == 0x03)
			level=3;
		return sprintf(data, "%d:%d", mode, level);
	}
	else if(mode == 0x03){	//Off
		mode=0;
		return sprintf(data, "%d", mode);
	}
	
VISCA_ERROR:
	VISCA_close_serial(&interface);
	return sprintf( data, "error");
#else
	return -1;
#endif
}
#endif

int para_sequence64_1( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	//fprintf(stderr,">>>>>>>> %d\n", __LINE__);
	//VISCAInterface_t  interface;
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i=0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;

	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
     	fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
	
	//fprintf(stderr,">>>>>>>> %d\n", __LINE__);
	if ( *arg == 'a' ||  *arg == '\0'){
		//fprintf(stderr,">>>>>>>> %d\n", __LINE__);
		for(i=0;i<sys_info->vsptz.sequencepoint;i++)
		{
			//fprintf(stderr,"i=%d %d\n",i,__LINE__);
			lseek(Fd, i*4, SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;

			if(pp == 0)
				break;

			num++;
			if(num == 2)
			{
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
				//fprintf(stderr,">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %d\n",__LINE__);
			}		
		}	
	}

	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret;
	
#else
	return -1;
#endif
}

int para_sequence64_2( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i = 0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;
	
	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
		
	if ( *arg == 'a' ||  *arg == '\0'){
		for(i=0;i<sys_info->vsptz.sequencepoint;i++){
			lseek(Fd, (sys_info->vsptz.sequencepoint*4+i*4), SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;
			if(pp == 0)
				break;
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
			}		
		}	
	}
	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret;		
#else
	return -1;
#endif
}

int para_sequence64_3( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i = 0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;
		
	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
			
	if ( *arg == 'a' ||  *arg == '\0'){
		for(i=0;i<sys_info->vsptz.sequencepoint;i++){
			lseek(Fd, (sys_info->vsptz.sequencepoint*4*2+i*4), SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;
			if(pp == 0)
				break;
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
			}		
		}	
	}
	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret; 	
#else
	return -1;
#endif
}

int para_sequence64_4( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i = 0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;
		
	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
			
	if ( *arg == 'a' ||  *arg == '\0'){
		for(i=0;i<sys_info->vsptz.sequencepoint;i++){
			lseek(Fd, (sys_info->vsptz.sequencepoint*4*3+i*4), SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;
			if(pp == 0)
				break;
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
			}		
		}	
	}
	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret; 	
#else
	return -1;
#endif
}

int para_sequence64_5( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i = 0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;
		
	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
			
	if ( *arg == 'a' ||  *arg == '\0'){
		for(i=0;i<sys_info->vsptz.sequencepoint;i++){
			lseek(Fd, (sys_info->vsptz.sequencepoint*4*4+i*4), SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;
			if(pp == 0)
				break;
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
			}		
		}	
	}
	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret; 	
#else
	return -1;
#endif
}

int para_sequence64_6( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i = 0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;
		
	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
			
	if ( *arg == 'a' ||  *arg == '\0'){
		for(i=0;i<sys_info->vsptz.sequencepoint;i++){
			lseek(Fd, (sys_info->vsptz.sequencepoint*4*5+i*4), SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;
			if(pp == 0)
				break;
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
			}		
		}	
	}
	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret; 	
#else
	return -1;
#endif
}

int para_sequence64_7( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i = 0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;
		
	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
			
	if ( *arg == 'a' ||  *arg == '\0'){
		for(i=0;i<sys_info->vsptz.sequencepoint;i++){
			lseek(Fd, (sys_info->vsptz.sequencepoint*4*6+i*4), SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;
			if(pp == 0)
				break;
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
			}		
		}	
	}
	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret; 	
#else
	return -1;
#endif
}

int para_sequence64_8( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	char *ptr = data;
	UInt8_t preset_point[64], dwell_time[64], speed[64];
	int i = 0, j, num = 0, ret = 0, cnt = 0, tmp, Fd;
	char string_cmp[32];
	unsigned short int pp, qq;
		
	Fd = open(SEQUENCE_SAVE, O_RDONLY | O_NONBLOCK);
	if(Fd < 0)
		fprintf(stderr,"open sequence.vs failed! %d\n",__LINE__); 
			
	if ( *arg == 'a' ||  *arg == '\0'){
		for(i=0;i<sys_info->vsptz.sequencepoint;i++){
			lseek(Fd, (sys_info->vsptz.sequencepoint*4*7+i*4), SEEK_SET);
			read(Fd, &pp, sizeof(pp)); 
			read(Fd, &qq, sizeof(qq));
			preset_point[i] = pp;
			dwell_time[i] = (qq >> 8) & 0x00FF;
			speed[i] = qq & 0x00FF;
			if(pp == 0)
				break;
			num++;
			if(num == 2){
				num -= 1;
				*ptr++ = ';';
				ret++;
			}
			sscanf(conf_string_read(VISCA_PRESET_ID01+preset_point[i]-1), "%d-%s", &tmp, string_cmp);				
			if(strcmp (string_cmp, "") != 0){
				cnt = sprintf(ptr, "%d-%s:%d:%d", preset_point[i], string_cmp, dwell_time[i], speed[i]-1);
				ptr += cnt;
				ret += cnt;
			}		
		}	
	}
	for(j=i;i<64;i++){
		num++;
		if(num == 2){
			num -= 1;
			*ptr++ = ';';
			ret++;
		}
		cnt = sprintf(ptr, " : : ");
		ptr += cnt;
		ret += cnt;
	}
	if(close(Fd) != 0)
		fprintf(stderr,"close sequence.vs failed! %d\n",__LINE__);
	return ret; 	
#else
	return -1;
#endif
}

int para_autopan( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	UInt8_t pan_direction[4], pan_speed[4];
	char str1[11]="B900000000";
	char str2[11]="B901000000";
	char str3[11]="B902000000";
	char str4[11]="B903000000";

	if ( *arg == '0' ){
		if(VISCA_autopan_inquiry(str1, &pan_direction[0], &pan_speed[0]) != 0)
		 	goto VISCA_ERROR;
		return sprintf(data, "%d;%d", pan_direction[0], pan_speed[0]);
	}
	else if ( *arg == '1' ){
		if(VISCA_autopan_inquiry(str2, &pan_direction[1], &pan_speed[1]) != 0)
		 	goto VISCA_ERROR;
		return sprintf(data, "%d;%d", pan_direction[1], pan_speed[1]);
	}		
	else if ( *arg == '2' ){
		if(VISCA_autopan_inquiry(str3, &pan_direction[2], &pan_speed[2]) != 0)
		 	goto VISCA_ERROR;
		return sprintf(data, "%d;%d", pan_direction[2], pan_speed[2]);
	}
	else if ( *arg == '3' ){
		if(VISCA_autopan_inquiry(str1, &pan_direction[3], &pan_speed[3]) != 0)
		 	goto VISCA_ERROR;
		return sprintf(data, "%d;%d", pan_direction[3], pan_speed[3]);
	}
	else if ( *arg == 'a' || *arg == '\0' ){
		if(VISCA_autopan_inquiry(str1, &pan_direction[0], &pan_speed[0]) != 0)
		 	goto VISCA_ERROR;
		if(VISCA_autopan_inquiry(str2, &pan_direction[1], &pan_speed[1]) != 0)
		 	goto VISCA_ERROR;
		if(VISCA_autopan_inquiry(str3, &pan_direction[2], &pan_speed[2]) != 0)
		 	goto VISCA_ERROR;
		if(VISCA_autopan_inquiry(str4, &pan_direction[3], &pan_speed[3]) != 0)
		 	goto VISCA_ERROR;
		return sprintf(data, "%d;%d;%d;%d;%d;%d;%d;%d", pan_direction[0], pan_speed[0], pan_direction[1], pan_speed[1], 
			pan_direction[2], pan_speed[2], pan_direction[3], pan_speed[3]);
	}	

VISCA_ERROR:
	return sprintf( data, "error");
	
#else
	return -1;
#endif
}

int para_autopan_dir(request *req, char *data, char *arg)
{
#ifdef SUPPORT_VISCA
	if ( *arg == '0' )
		return sprintf(data, "%s", sys_info->vsptz.autopan.pan0_dir);
	else if( *arg == '1' )
		return sprintf(data, "%s", sys_info->vsptz.autopan.pan1_dir);
	else if( *arg == '2' )
		return sprintf(data, "%s", sys_info->vsptz.autopan.pan2_dir);
	else if( *arg == '3' )
		return sprintf(data, "%s", sys_info->vsptz.autopan.pan3_dir);
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%s;%s;%s;%s", sys_info->vsptz.autopan.pan0_dir, sys_info->vsptz.autopan.pan1_dir, 
		sys_info->vsptz.autopan.pan2_dir, sys_info->vsptz.autopan.pan3_dir);
#else
	return -1;
#endif
}

int para_autopan_speed(request *req, char *data, char *arg)
{
#ifdef SUPPORT_VISCA
		if ( *arg == '0' )
			return sprintf(data, "%d", sys_info->vsptz.autopan.pan0_speed);
		else if( *arg == '1' )
			return sprintf(data, "%d", sys_info->vsptz.autopan.pan1_speed);
		else if( *arg == '2' )
			return sprintf(data, "%d", sys_info->vsptz.autopan.pan2_speed);
		else if( *arg == '3' )
			return sprintf(data, "%d", sys_info->vsptz.autopan.pan3_speed);
		else if ( *arg == 'a' || *arg == '\0' )
			return sprintf(data, "%d;%d;%d;%d", sys_info->vsptz.autopan.pan0_speed, sys_info->vsptz.autopan.pan1_speed, 
			sys_info->vsptz.autopan.pan2_speed, sys_info->vsptz.autopan.pan3_speed);
#else
		return -1;
#endif

}

int para_homefunctionsetting( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%s", sys_info->vsptz.homefunctionsetting);
#else
	return -1;
#endif
}

int para_homefunctiontime( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.homedelaytime);
#else
	return -1;
#endif
}

int para_homefunctionline( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.homeline);
#else
	return -1;
#endif
}

int para_homeposition( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	__u16 num;
	num = sys_info->ipcam[VISCA_HOME_NUMBER].config.u16 - 1;
	if(sys_info->ipcam[VISCA_HOME_POSITION].config.u8)
		return sprintf(data, "%s", conf_string_read(VISCA_PRESET_ID01 + num));
	else
		return sprintf(data, "%s", "--Preset List--");
#else
	return -1;
#endif
}

int para_privacysizeinq( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	if ( *arg == 'a' || *arg == '\0' ){
		return sprintf(data, "%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d;%d:%d", 
		sys_info->vsptz.privacy_h_size[0], sys_info->vsptz.privacy_v_size[0], sys_info->vsptz.privacy_h_size[1], sys_info->vsptz.privacy_v_size[1], 
		sys_info->vsptz.privacy_h_size[2], sys_info->vsptz.privacy_v_size[2], sys_info->vsptz.privacy_h_size[3], sys_info->vsptz.privacy_v_size[3], 
		sys_info->vsptz.privacy_h_size[4], sys_info->vsptz.privacy_v_size[4], sys_info->vsptz.privacy_h_size[5], sys_info->vsptz.privacy_v_size[5],
		sys_info->vsptz.privacy_h_size[6], sys_info->vsptz.privacy_v_size[6], sys_info->vsptz.privacy_h_size[7], sys_info->vsptz.privacy_v_size[7], 
		sys_info->vsptz.privacy_h_size[8], sys_info->vsptz.privacy_v_size[8], sys_info->vsptz.privacy_h_size[9], sys_info->vsptz.privacy_v_size[9],
		sys_info->vsptz.privacy_h_size[10], sys_info->vsptz.privacy_v_size[10], sys_info->vsptz.privacy_h_size[11], sys_info->vsptz.privacy_v_size[11], 
		sys_info->vsptz.privacy_h_size[12], sys_info->vsptz.privacy_v_size[12], sys_info->vsptz.privacy_h_size[13], sys_info->vsptz.privacy_v_size[13], 
		sys_info->vsptz.privacy_h_size[14], sys_info->vsptz.privacy_v_size[14], sys_info->vsptz.privacy_h_size[15], sys_info->vsptz.privacy_v_size[15]);
	}
	else
		return -1;
#else
	return -1;
#endif
}

int para_visca_dometype( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	if(sys_info->vsptz.dome_type == 51)
		return sprintf(data, "DH510");
	else if(sys_info->vsptz.dome_type == 61)
		return sprintf(data, "DH610");
	else if(sys_info->vsptz.dome_type == 70)
		return sprintf(data, "DH701");
	else if(sys_info->vsptz.dome_type == 81)
		return sprintf(data, "DH811");
	else
		return sprintf(data, "Unknow");
#else
	return -1;
#endif
}

int para_visca_motortype( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	if(sys_info->vsptz.motor_type == 2)
		return sprintf(data, "D");
	else if(sys_info->vsptz.motor_type == 3)
		return sprintf(data, "E");
	else if(sys_info->vsptz.motor_type == 4)
		return sprintf(data, "F");
	else
		return sprintf(data, "Unknow");
#else
	return -1;
#endif
}

int para_visca_version( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%s", sys_info->vsptz.version);
#else
	return -1;
#endif
}

int para_visca_model( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	if(sys_info->vsptz.zoom == 12)
		return sprintf(data, "0");
	else if(sys_info->vsptz.zoom == 18)
		return sprintf(data, "1");
	else if(sys_info->vsptz.zoom == 30)
		return sprintf(data, "2");
	else if(sys_info->vsptz.zoom == 36)
		return sprintf(data, "3");
	else
		return -1;
#else
	return -1;
#endif
}

int para_visca_wintimezone(request *req, char *data, char *arg)
{
	// FIX ME: 
	//return sprintf(data, "%d", sys_info->config.lan_config.net.ntp_timezone);
	return sprintf(data, "%d", sys_info->ipcam[NTP_TIMEZONE].config.u8);
} 

int para_visca_netmac(request *req, char *data, char *arg)
{
#ifdef SUPPORT_VISCA
	__u8 mac[MAC_LENGTH];
	net_get_hwaddr(sys_info->ifether, mac);
	return sprintf(data, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#else
	return -1;
#endif
}

int para_visca_serial_number(request *req, char *data, char *arg)
{
#ifdef SUPPORT_VISCA
	BIOS_DATA bios;
	if (bios_data_read(&bios) < 0)
    	bios_data_set_default(&bios);
	return sprintf(data, "%s", bios.sn);
#else
	return -1;
#endif
}

int para_sequencepointnum( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.sequencepoint);
#else
	return -1;
#endif
}

int para_cruiselinenum( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.cruiseline);
#else
	return -1;
#endif
}

int para_visca_dome_ntsc( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.ntsc);
#else
	return -1;
#endif
}

int para_visca_dome_zoom( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	if(sys_info->vsptz.zoom == 12)
		return sprintf(data, "12");
	else if(sys_info->vsptz.zoom == 18)
		return sprintf(data, "18");
	else if(sys_info->vsptz.zoom == 30)
		return sprintf(data, "30");
	else if(sys_info->vsptz.zoom == 36)
		return sprintf(data, "36");
	else
		return sprintf(data, "Unknow");
#else
	return -1;
#endif
}

int para_visca_dome_icr( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.icr);
#else
	return -1;
#endif
}

int para_visca_dome_wdr( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf(data, "%d", sys_info->vsptz.wdr);
#else
	return -1;
#endif
}

int para_viscapresetlistname( request *req, char *data, char *arg )
{
	char *ptr = data;
	
	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;
	int page_num = 0;

	page_num = sys_info->visca_preset_page;
	//fprintf(stderr, "page_num = %d\n", page_num);

	if ( *arg == 'a' ||  *arg == '\0') {
		cnt += sprintf(ptr, "--Preset List--");
		ptr += cnt;
		ret += cnt;

		for(i = page_num*10; i< (page_num+1)*10; i++)
		{
			if(i<MAX_VISCAPRESET){
				*temp = '\0';
				cnt = 0;

				sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &tmp, temp);			
				if(num == 0){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%d-%s", tmp, temp);
				ptr += cnt;
				ret += cnt;
			}
		}
	}

	if(ret != 0)
		return ret;
	else
		return sprintf(data, " ");
}

int para_viscahomepresetlistname( request *req, char *data, char *arg )
{
	char *ptr = data;
	
	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;

	if ( *arg == 'a' ||  *arg == '\0') {
		cnt += sprintf(ptr, "--Preset List--");
		ptr += cnt;
		ret += cnt;

		for(i = 0; i< MAX_VISCAPRESET; i++)
		{
			*temp = '\0';
			cnt = 0;

			sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &tmp, temp);
			if(strcmp(temp, "nonsetting") != 0){
				if(num == 0){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%d-%s", tmp, temp);
				ptr += cnt;
				ret += cnt;
			}
		}
	}

	if(ret != 0)
		return ret;
	else
		return sprintf(data, " ");
}

int para_viscasequencelistname( request *req, char *data, char *arg )
{
	char *ptr = data;
	
	int tmp;
	char temp[32];
	int i, num=0;
	__u32 ret = 0, cnt = 0;

	if ( *arg == 'a' ||  *arg == '\0') {
		cnt += sprintf(ptr, "--Preset List--");
		ptr += cnt;
		ret += cnt;

		for(i = 0; i< MAX_VISCAPRESET-1; i++)
		{
			*temp = '\0';
			cnt = 0;

			sscanf(conf_string_read(VISCA_PRESET_ID01+i), "%d-%s", &tmp, temp);
			if(strcmp(temp, "nonsetting") != 0){
				if(num == 0){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				num++;
				if(num == 2){
					num -= 1;
					*ptr++ = ';';
					ret++;
				}
				cnt += sprintf(ptr, "%d-%s", tmp, temp);
				ptr += cnt;
				ret += cnt;
			}
		}
	}

	if(ret != 0)
		return ret;
	else
		return sprintf(data, " ");
}

int para_wbmodename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Auto");
	else if ( *arg == '1' )
		return sprintf(data, "Indoor");
	else if ( *arg == '2' )
		return sprintf(data, "Outdoor");
	else if ( *arg == '3' )
		return sprintf(data, "ATW");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Auto;Indoor;Outdoor;ATW");
	return -1;
}

int para_fliptypename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Off");
	else if ( *arg == '1' )
		return sprintf(data, "M.E. Flip");
	else if ( *arg == '2' )
		return sprintf(data, "Image Flip");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Off;M.E. Flip;Image Flip");
	return -1;
}

int para_aemodename(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Auto");
	else if ( *arg == '1' )
		return sprintf(data, "Manual");
	else if ( *arg == '2' )
		return sprintf(data, "Shutter Priority");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Auto;Manual;Shutter Priority");
	return -1;
}

int para_aeshuttername(request *req, char *data, char *arg)
{
	if ( (*arg == '0') && (*(arg+1) == '\0') )
		return sprintf(data, "1/60");
	else if ( (*arg == '1') && (*(arg+1) == '\0') )
		return sprintf(data, "1/90");
	else if ( (*arg == '2') && (*(arg+1) == '\0') )
		return sprintf(data, "1/100");
	else if ( (*arg == '3') && (*(arg+1) == '\0') )
		return sprintf(data, "1/125");
	else if ( (*arg == '4') && (*(arg+1) == '\0') )
		return sprintf(data, "1/180");
	else if ( (*arg == '5') && (*(arg+1) == '\0') )
		return sprintf(data, "1/250");
	else if ( (*arg == '6') && (*(arg+1) == '\0') )
		return sprintf(data, "1/350");
	else if ( (*arg == '7') && (*(arg+1) == '\0') )
		return sprintf(data, "1/500");
	else if ( (*arg == '8') && (*(arg+1) == '\0') )
		return sprintf(data, "1/725");
	else if ( (*arg == '9') && (*(arg+1) == '\0') )
		return sprintf(data, "1/1000");
	else if ( (*arg == '1') && (*(arg+1) == '0') )
		return sprintf(data, "1/1500");
	else if ( (*arg == '1') && (*(arg+1) == '1') )
		return sprintf(data, "1/2000");
	else if ( (*arg == '1') && (*(arg+1) == '2') )
		return sprintf(data, "1/3000");
	else if ( (*arg == '1') && (*(arg+1) == '3') )
		return sprintf(data, "1/4000");
	else if ( (*arg == '1') && (*(arg+1) == '4') )
		return sprintf(data, "1/6000");
	else if ( (*arg == '1') && (*(arg+1) == '5') )
		return sprintf(data, "1/10000");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1/60;1/90;1/100;1/125;1/180;1/250;1/350;1/500;1/725;1/1000;1/1500;1/2000;1/3000;1/4000;1/6000;1/10000");
	return -1;
}

int para_aemanualname(request *req, char *data, char *arg)
{
	if ( (*arg == '0') && (*(arg+1) == '\0') )
		return sprintf(data, "1");
	else if ( (*arg == '1') && (*(arg+1) == '\0') )
		return sprintf(data, "1/2");
	else if ( (*arg == '2') && (*(arg+1) == '\0') )
		return sprintf(data, "1/4");
	else if ( (*arg == '3') && (*(arg+1) == '\0') )
		return sprintf(data, "1/8");
	else if ( (*arg == '4') && (*(arg+1) == '\0') )
		return sprintf(data, "1/15");
	else if ( (*arg == '5') && (*(arg+1) == '\0') )
		return sprintf(data, "1/30");
	else if ( (*arg == '6') && (*(arg+1) == '\0') )
		return sprintf(data, "1/60");
	else if ( (*arg == '7') && (*(arg+1) == '\0') )
		return sprintf(data, "1/90");
	else if ( (*arg == '8') && (*(arg+1) == '\0') )
		return sprintf(data, "1/100");
	else if ( (*arg == '9') && (*(arg+1) == '\0') )
		return sprintf(data, "1/125");
	else if ( (*arg == '1') && (*(arg+1) == '0') )
		return sprintf(data, "1/180");
	else if ( (*arg == '1') && (*(arg+1) == '1') )
		return sprintf(data, "1/250");
	else if ( (*arg == '1') && (*(arg+1) == '2') )
		return sprintf(data, "1/350");
	else if ( (*arg == '1') && (*(arg+1) == '3') )
		return sprintf(data, "1/500");
	else if ( (*arg == '1') && (*(arg+1) == '4') )
		return sprintf(data, "1/725");
	else if ( (*arg == '1') && (*(arg+1) == '5') )
		return sprintf(data, "1/1000");
	else if ( (*arg == '1') && (*(arg+1) == '6') )
		return sprintf(data, "1/1500");
	else if ( (*arg == '1') && (*(arg+1) == '7') )
		return sprintf(data, "1/2000");
	else if ( (*arg == '1') && (*(arg+1) == '8') )
		return sprintf(data, "1/3000");
	else if ( (*arg == '1') && (*(arg+1) == '9') )
		return sprintf(data, "1/4000");
	else if ( (*arg == '2') && (*(arg+1) == '0') )
		return sprintf(data, "1/6000");
	else if ( (*arg == '2') && (*(arg+1) == '1') )
		return sprintf(data, "1/10000");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "1;1/2;1/4;1/8;1/15;1/30;1/60;1/90;1/100;1/125;1/180;1/250;1/350;1/500;1/725;1/1000;1/1500;1/2000;1/3000;1/4000;1/6000;1/10000");
	return -1;
}

int para_privacycolorname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "black");
	else if ( *arg == '1' )
		return sprintf(data, "white");
	else if ( *arg == '2' )
		return sprintf(data, "red");
	else if ( *arg == '3' )
		return sprintf(data, "green");
	else if ( *arg == '4' )
		return sprintf(data, "blue");
	else if ( *arg == '5' )
		return sprintf(data, "cyan");
	else if ( *arg == '6' )
		return sprintf(data, "yellow");
	else if ( *arg == '7' )
		return sprintf(data, "magenta");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "black;white;red;green;blue;cyan;yellow;magenta");
	return -1;
}

int para_homefunctionname(request *req, char *data, char *arg)
{
	if ( *arg == '0' )
		return sprintf(data, "Preset");
	else if ( *arg == '1' )
		return sprintf(data, "Sequence");
	else if ( *arg == '2' )
		return sprintf(data, "Autopan");
	else if ( *arg == '3' )
		return sprintf(data, "Cruise");
	else if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "Preset;Sequence;Autopan;Cruise");
	return -1;
}

int para_presetpageinq(request *req, char *data, char *arg)
{
	if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%d", sys_info->visca_preset_page+1);
	return -1;
}

int para_sequencepageinq(request *req, char *data, char *arg)
{
	if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%d", sys_info->visca_sequence_page+1);
	return -1;
}

int para_panspeed(request *req, char *data, char *arg)
{
	if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%d", sys_info->ipcam[VISCA_PAN_SPEED].config.value);
	return -1;
}

int para_tiltspeed(request *req, char *data, char *arg)
{
	if ( *arg == 'a' || *arg == '\0' )
		return sprintf(data, "%d", sys_info->ipcam[VISCA_TILT_SPEED].config.value);
	return -1;
}

int para_aperturename(request *req, char *data, char *arg)
{
	return sprintf(data, "1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16");
}

int para_angleminname(request *req, char *data, char *arg)
{
	return sprintf(data, "-10;-9;-8;-7;-6;-5;-4;-3;-2;-1;0;1;2;3;4;5;6;7;8;9;10");
}

int para_anglemaxname(request *req, char *data, char *arg)
{
	return sprintf(data, "80;81;82;83;84;85;86;87;88;89;90;91;92;93;94;95;96;97;98;99;100");
}

int para_anglemaxflipname(request *req, char *data, char *arg)
{
	return sprintf(data, "170;171;172;173;174;175;176;177;178;179;180;181;182;183;184;185;186;187;188;189;190");
}

int para_autopanname(request *req, char *data, char *arg)
{
	return sprintf(data, "1;2;3;4");
}

int para_autopandirecname(request *req, char *data, char *arg)
{
	return sprintf(data, "Right;Left");
}

int para_autopanspeedname(request *req, char *data, char *arg)
{
	return sprintf(data, "1;2;3;4");
}

int para_panspeedname(request *req, char *data, char *arg)
{
	return sprintf(data, "1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16");
}

int para_tiltspeedname(request *req, char *data, char *arg)
{
	return sprintf(data, "1;2;3;4;5;6;7;8;9;10;11;12;13;14;15;16");
}

int para_cruisename(request *req, char *data, char *arg)
{
	if(sys_info->vsptz.cruiseline == 4)
		return sprintf(data, "1;2;3;4");
	else if(sys_info->vsptz.cruiseline == 8)
		return sprintf(data, "1;2;3;4;5;6;7;8");
	else
		return -1;
}

int para_visca_burn_status(request *req, char *data, char *arg)
{
	if ( *arg == 'a' || *arg == '\0' ){
		if(sys_info->visca_burn_status == 0)
			return sprintf(data, "Stop");
		else if(sys_info->visca_burn_status == 1)
			return sprintf(data, "Running");
		else if(sys_info->visca_burn_status == 2)
			return sprintf(data, "Fail");
		else
			return sprintf(data, "Error");
	}
	else
		return -1;
}

int para_supportvisca ( request *req, char *data, char *arg )
{
#ifdef SUPPORT_VISCA
	return sprintf( data, "1");
#else
	return sprintf( data, "0");
#endif
}  


int para_supportfactorymode ( request *req, char *data, char *arg )
{
#ifdef SUPPORT_FACTORYMODE
	return sprintf( data, "1");
#else
	return sprintf( data, "0");
#endif
}  

int para_supportosdswitch(request *req, char *data, char *arg)
{
	return sprintf( data, "1");
}

int para_osdshow(request *req, char *data, char *arg)
{
	return sprintf( data, "%d", sys_info->ipcam[OSD_SHOW].config.value);
}

int para_support_QoS( request *req, char *data, char *arg )
{
#if defined( SUPPORT_QOS_SCHEDULE )
	return sprintf( data, "1" );
#else
	return sprintf( data, "0" );
#endif
}

int para_support_CoS( request *req, char *data, char *arg )
{
#if defined( SUPPORT_COS_SCHEDULE )
	return sprintf( data, "1" );
#else
	return sprintf( data, "0" );
#endif
}

int para_support_BWC( request *req, char *data, char *arg )
{
#if defined( SUPPORT_QOS_SCHEDULE ) || defined( SUPPORT_COS_SCHEDULE )
	return sprintf( data, "0" );
#else
	return sprintf( data, "1" );
#endif
}

#if defined( SUPPORT_COS_SCHEDULE )
int para_CoS_enable( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[COS_ENABLE].config.u32 );
}

int para_VLan_ID( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[COS_VLANID].config.u32 );
}

int para_CoS_prio_video( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[COS_PRIO_VIDEO].config.u32 );
}

int para_CoS_prio_audio( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[COS_PRIO_AUDIO].config.u32 );
}

int para_CoS_prio_event( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[COS_PRIO_EVENT].config.u32 );
}

int para_CoS_prio_manage( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[COS_PRIO_MANAGE].config.u32 );
}
#endif /* defined( SUPPORT_COS_SCHEDULE ) */
#if defined( SUPPORT_QOS_SCHEDULE )
int para_QoS_enable( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[QOS_ENABLE].config.u32 );
}

int para_QoS_DSCP_video( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[QOS_DSCP_VIDEO].config.u32 );
}

int para_QoS_DSCP_audio( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[QOS_DSCP_AUDIO].config.u32 );
}

int para_QoS_DSCP_event( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[QOS_DSCP_EVENT].config.u32 );
}

int para_QoS_DSCP_manage( request *req, char *data, char *arg )
{
	return sprintf( data, "%d", sys_info->ipcam[QOS_DSCP_MANAGE].config.u32 );
}
#endif /* defined( SUPPORT_QOS_SCHEDULE ) */

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#define HASH_TABLE_SIZE	(sizeof(HttpArgument)/sizeof(HTML_ARGUMENT))
HTML_ARGUMENT HttpArgument [] =
{
#if defined SENSOR_USE_TVP5150 || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined SENSOR_USE_TVP5150_MDIN270
	{ "videolossstatus" , get_video_loss_status		, AUTHORITY_VIEWER	 , NULL },
#endif
	{ "machinetype"     , para_machinetype        , AUTHORITY_VIEWER   , NULL },
	{ "supporttvout"     , para_supporttvout          , AUTHORITY_VIEWER   , NULL },
	{ "supportosdswitch", para_supportosdswitch     , AUTHORITY_VIEWER , NULL },
	{ "osdshow"         ,para_osdshow               , AUTHORITY_VIEWER , NULL },
	{ "supportled"     , para_supportled          , AUTHORITY_VIEWER   , NULL },
	{ "date"            , para_date                 , AUTHORITY_VIEWER   , NULL },
	{ "time"            , para_time                 , AUTHORITY_VIEWER   , NULL },
	{ "weekday"         , para_week                 , AUTHORITY_VIEWER   , NULL },
	{ "sntpip"          , para_sntpip               , AUTHORITY_OPERATOR , NULL },
	{ "sntpipname"      , para_sntpipname           , AUTHORITY_OPERATOR , NULL }, 
	{ "sntptimezone"    , para_sntptimezone         , AUTHORITY_OPERATOR , NULL },
	{ "sntpfrequency"   , para_sntpfrequency        , AUTHORITY_OPERATOR , NULL },
	{ "dhcpenable"      , para_dhcpenable           , AUTHORITY_OPERATOR , NULL },
	{ "dnsip"           , para_dnsip                , AUTHORITY_OPERATOR , NULL },
	{ "gateway"         , para_gateway              , AUTHORITY_OPERATOR , NULL },
	{ "httpport"        , para_httpport             , AUTHORITY_VIEWER , NULL },
	{ "defaulthttpport" , para_defaulthttpport      , AUTHORITY_OPERATOR , NULL },
	{ "netip"           , para_netip                , AUTHORITY_OPERATOR , NULL },
	{ "netmask"         , para_netmask              , AUTHORITY_OPERATOR , NULL },
	{ "macaddress"      , para_netmac               , AUTHORITY_OPERATOR , NULL },
	{ "title"           , para_title                , AUTHORITY_VIEWER   , NULL },
	{ "maxtitlelen"     , para_maxtitlelen          , AUTHORITY_OPERATOR , NULL },
	{ "liveresolution"  , para_liveresolution       , AUTHORITY_VIEWER   , NULL },
	//{ "mpeg4quality"    , para_mpeg4quality         , AUTHORITY_OPERATOR , NULL },
	{ "livequality"     , para_livequality          , AUTHORITY_VIEWER   , NULL },
	{ "livequality2"    , para_livequality2         , AUTHORITY_VIEWER   , NULL },
	{ "maxfqdnlen"      , para_maxfqdnlen           , AUTHORITY_OPERATOR , NULL },
	{ "httpstreamname1"  , para_http_streamname1   , AUTHORITY_VIEWER , NULL },
	{ "httpstreamname2"  , para_http_streamname2   , AUTHORITY_VIEWER , NULL },
	{ "httpstreamname3"  , para_http_streamname3   , AUTHORITY_VIEWER , NULL },
	{ "httpstreamname4"  , para_http_streamname4   , AUTHORITY_VIEWER , NULL },
	{ "rtspaccessname1"  , para_rtsp_streamname1   , AUTHORITY_VIEWER , NULL },
	{ "rtspaccessname2"  , para_rtsp_streamname2   , AUTHORITY_VIEWER , NULL },
	{ "rtspaccessname3"  , para_rtsp_streamname3   , AUTHORITY_VIEWER , NULL },
	{ "rtspaccessname4"  , para_rtsp_streamname4   , AUTHORITY_VIEWER , NULL },

	{ "minnetport"      , para_minnetport           , AUTHORITY_OPERATOR , NULL },
	{ "maxnetport"      , para_maxnetport           , AUTHORITY_OPERATOR , NULL },

	{ "OEMModel"      , para_OEMModel           , AUTHORITY_VIEWER , NULL },
	{ "OEMMaker"      , para_OEMMaker           , AUTHORITY_VIEWER , NULL },

	//{ "ftpip"           , para_ftpip                , AUTHORITY_OPERATOR , NULL },
	//{ "ftppiport"       , para_ftppiport            , AUTHORITY_OPERATOR , NULL },
	//{ "ftpuser"         , para_ftpuser              , AUTHORITY_OPERATOR , NULL },
	//{ "ftppassword"     , para_ftppassword          , AUTHORITY_OPERATOR , NULL },
	//{ "ftppath"         , para_ftppath              , AUTHORITY_OPERATOR , NULL },
	{ "maxftpuserlen"   , para_maxftpuserlen        , AUTHORITY_OPERATOR , NULL },
	{ "maxftppwdlen"    , para_maxftppwdlen         , AUTHORITY_OPERATOR , NULL },
	{ "maxftppathlen"   , para_maxftppathlen        , AUTHORITY_OPERATOR , NULL },

	//{ "smtpport"        , para_smtpport             , AUTHORITY_OPERATOR , NULL },
	//{ "smtpip"          , para_smtpip               , AUTHORITY_OPERATOR , NULL },
	//{ "smtpauth"        , para_smtpauth             , AUTHORITY_OPERATOR , NULL },
	//{ "smtpuser"        , para_smtpuser             , AUTHORITY_OPERATOR , NULL },
	{ "maxsmtpuser"     , para_maxsmtpuser          , AUTHORITY_OPERATOR , NULL },
	//{ "smtppwd"         , para_smtppwd              , AUTHORITY_OPERATOR , NULL },
	{ "maxsmtppwd"      , para_maxsmtppwd           , AUTHORITY_OPERATOR , NULL },
	//{ "smtpsender"      , para_smtpsender           , AUTHORITY_OPERATOR , NULL },
	{ "maxsmtpsender"   , para_maxsmtpsender        , AUTHORITY_OPERATOR , NULL },
	//{ "emailuser"       , para_emailuser            , AUTHORITY_OPERATOR , NULL },
	{ "maxemailuserlen" , para_maxemailuserlen      , AUTHORITY_OPERATOR , NULL },

	{ "supportmpeg4"    , para_supportmpeg4         , AUTHORITY_VIEWER   , NULL },
	{ "format"          , para_format               , AUTHORITY_VIEWER   , NULL },
	{ "imagesource"     , para_imagesource          , AUTHORITY_VIEWER   , NULL },
	{ "devicename"      , para_devicename           , AUTHORITY_VIEWER   , NULL },
	//{ "defaultstorage"  , para_defaultstorage       , AUTHORITY_VIEWER   , NULL },
	{ "cfinsert"        , para_cfinsert             , AUTHORITY_VIEWER   , NULL },
	{ "defaultcardgethtm", para_defaultcardgethtm   , AUTHORITY_VIEWER   , NULL },
	{ "brandurl"         , para_brandurl            , AUTHORITY_VIEWER   , NULL },
	{ "brandname"        , para_brandname           , AUTHORITY_VIEWER   , NULL },
	{ "brandpath"		 , para_brandpath			, AUTHORITY_VIEWER	 , NULL },
	{ "supporttstamp"    , para_supporttstamp       , AUTHORITY_VIEWER   , NULL },
	{ "supporttstampcolor"    , para_supporttstampcolor  , AUTHORITY_VIEWER   , NULL },
	
	//{ "mpeg4xsize"       , para_mpeg4xsize          , AUTHORITY_VIEWER   , NULL },
	//{ "mpeg4ysize"       , para_mpeg4ysize          , AUTHORITY_VIEWER   , NULL },
	{ "jpegxsize"        , para_profile1xsize           , AUTHORITY_VIEWER   , NULL },
	{ "jpegysize"        , para_profile1ysize           , AUTHORITY_VIEWER   , NULL },
	{ "socketauthority"  , para_socketauthority     , AUTHORITY_VIEWER   , NULL },
	{ "authoritychange"  , para_authoritychange     , AUTHORITY_VIEWER   , NULL },
	{ "supportmotion"    , para_supportmotion       , AUTHORITY_VIEWER   , NULL },
	{ "supportmotionmode", para_supportmotionmode   , AUTHORITY_VIEWER   , NULL },
	{ "supportwireless"  , para_supportwireless     , AUTHORITY_VIEWER   , NULL },
	{ "serviceftpclient" , para_serviceftpclient    , AUTHORITY_VIEWER   , NULL },
	{ "servicesmtpclient", para_servicesmtpclient   , AUTHORITY_VIEWER   , NULL },
	{ "servicepppoe"     , para_servicepppoe        , AUTHORITY_VIEWER   , NULL },
	{ "servicesntpclient", para_servicesntpclient   , AUTHORITY_VIEWER   , NULL },
	{ "serviceddnsclient", para_serviceddnsclient   , AUTHORITY_VIEWER   , NULL },
	{ "supportmaskarea"  , para_supportmaskarea     , AUTHORITY_VIEWER   , NULL },
	{ "supportlinein"  , para_supportlinein     , AUTHORITY_VIEWER   , NULL },
	{ "supportmic"  , para_supportmic     , AUTHORITY_VIEWER   , NULL },
	{ "servicevirtualclient"  , para_servicevirtualclient     , AUTHORITY_VIEWER   , NULL },
	{ "privacymaskenable"  , para_privacymaskenable     , AUTHORITY_VIEWER   , NULL },
	{ "maxmaskarea"  , para_maxmaskarea     , AUTHORITY_VIEWER   , NULL },
	{ "maskareaxlimit"  , para_maskareaxlimit     , AUTHORITY_VIEWER   , NULL },
	{ "maskareaylimit"  , para_maskareaylimit     , AUTHORITY_VIEWER   , NULL },
	{ "maskarea"  , para_maskarea     , AUTHORITY_VIEWER   , NULL },
	{ "machinecode"      , para_machinecode         , AUTHORITY_VIEWER   , NULL },
	{ "maxchannel"       , para_maxchannel          , AUTHORITY_VIEWER   , NULL },
	{ "supportrs232"     , para_supportrs232        , AUTHORITY_VIEWER   , NULL },
	{ "layoutnum"        , para_layoutnum           , AUTHORITY_VIEWER   , NULL },
	{ "supportmui"       , para_supportmui          , AUTHORITY_VIEWER   , NULL },
	{ "mui"              , para_mui                 , AUTHORITY_VIEWER   , NULL },
	{ "supporttstampmui" , para_support_tstamp_mui  , AUTHORITY_VIEWER   , NULL },
	{ "supportsequence"  , para_supportsequence     , AUTHORITY_VIEWER   , NULL },
	{ "quadmodeselect"   , para_quadmodeselect      , AUTHORITY_VIEWER   , NULL },
	{ "serviceipfilter"  , para_serviceipfilter     , AUTHORITY_VIEWER   , NULL },
	{ "oemflag0"         , para_oemflag0            , AUTHORITY_VIEWER   , NULL },
	{ "supportdncontrol" , para_supportdncontrol    , AUTHORITY_VIEWER   , NULL },
	{ "supportavc"       , para_supportavc          , AUTHORITY_VIEWER   , NULL },
	{ "supportaudio"     , para_supportaudio        , AUTHORITY_OPERATOR   , NULL },
	
	{ "supportptzpage"   , para_supportptzpage      , AUTHORITY_VIEWER   , NULL },
	{ "ptziconvisible"	 , para_ptziconvisible		, AUTHORITY_VIEWER	 , NULL },
	{ "supporteptz"   , para_supporteptz      , AUTHORITY_OPERATOR   , NULL },
	
	//{ "aviprealarm" 	 , para_aviprealarm			 , AUTHORITY_OPERATOR , NULL },
	//{ "avipostalarm"	 , para_avipostalarm			 , AUTHORITY_OPERATOR , NULL },
	
	
	{ "supportdhcpdipswitch"	, para_supportdhcpdipswitch		, AUTHORITY_VIEWER , NULL },
	{ "dhcpdipswitch"		, para_dhcpdipswitch 		, AUTHORITY_OPERATOR , NULL },
	{ "dhcpconfig"			, para_dhcpconfig			, AUTHORITY_OPERATOR , NULL },
	{ "timeformat"			, para_timeformat			, AUTHORITY_VIEWER   , NULL },
	{ "maxprealarmjpeg"		, para_maxprealarmjpeg			, AUTHORITY_OPERATOR , NULL },
	{ "maxpostalarmjpeg"		, para_maxpostalarmjpeg			, AUTHORITY_OPERATOR , NULL },
	//{ "prealarm"		, para_prealarm			, AUTHORITY_OPERATOR , NULL },
	//{ "postalarm"		, para_postalarm			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4cenable1"		, para_mpeg4cenable1			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4cenable2"		, para_mpeg4cenable2			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4desired"		, para_mpeg4desired			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4cenable"		, para_mpeg4cenable			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4cvalue"			, para_mpeg4cvalue			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg42cvalue"		, para_mpeg42cvalue			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4framerate"		, para_mpeg41framerate		, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4frameratename"	, para_mpeg41frameratename	, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg42framerate"		, para_mpeg42framerate		, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg42frameratename"	, para_mpeg42frameratename	, AUTHORITY_OPERATOR , NULL },
	//{ "jpegframerate"		, para_jpegframerate		, AUTHORITY_OPERATOR , NULL },
	//{ "jpegframerate2"		, para_jpegframerate2		, AUTHORITY_OPERATOR , NULL },
	//{ "jpegframeratename"	, para_jpegframeratename	, AUTHORITY_OPERATOR , NULL },
	//{ "jpegframeratename2"	, para_jpegframeratename2	, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4resname"		, para_mpeg4resname			, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg4resolution"		, para_mpeg4resolution      , AUTHORITY_OPERATOR , NULL },
	//{ "mpeg42resname"		, para_mpeg42resname		, AUTHORITY_OPERATOR , NULL },
	//{ "mpeg42resolution"	, para_mpeg42resolution		, AUTHORITY_OPERATOR , NULL },
	{ "resolutionname"		, para_resolutionname		, AUTHORITY_OPERATOR , NULL },
	{ "mpeg4quality1name"	, para_mpeg4quality1name		, AUTHORITY_OPERATOR , NULL },
	{ "mpeg4quality2name"	, para_mpeg4quality2name		, AUTHORITY_OPERATOR , NULL },
	{ "qualityname"			, para_qualityname			, AUTHORITY_VIEWER   , NULL },
	{ "qualityname2"			, para_qualityname2			, AUTHORITY_VIEWER   , NULL },
	{ "videocodecresname"	, para_videocodecresname	, AUTHORITY_OPERATOR , NULL },
	{ "videocodecres"		, para_videocodecres		, AUTHORITY_OPERATOR , NULL },
	{ "videocodecmodename"	, para_videocodecmodename	, AUTHORITY_OPERATOR , NULL },
	{ "videocodecmode"		, para_videocodecmode		, AUTHORITY_OPERATOR , NULL },
	{ "videoalc"		, para_videoalce		, AUTHORITY_OPERATOR , NULL },
	{ "waitserver"			, para_waitserver			, AUTHORITY_OPERATOR , NULL },
	{ "supportcolorkiller"	, para_supportcolorkiller	, AUTHORITY_VIEWER   , NULL },
	{ "supportAWB"			, para_supportAWB			, AUTHORITY_VIEWER   , NULL },
	{ "supportbrightness"	, para_supportbrightness	, AUTHORITY_VIEWER   , NULL },
	{ "supportdenoise"		, para_supportdenoise		, AUTHORITY_VIEWER	 , NULL },
	{ "supportevcomp"			, para_supportevcomp		, AUTHORITY_VIEWER	 , NULL },
	{ "supportdre"				, para_supportdre				, AUTHORITY_VIEWER	 , NULL },
	{ "supportmeteringmethod"	, para_supportmeteringmethod	, AUTHORITY_VIEWER	 , NULL },
	{ "supportcapturepriority"	, para_supportcapturepriority		, AUTHORITY_VIEWER	 , NULL },		
	{ "supportcontrast"		, para_supportcontrast		, AUTHORITY_VIEWER   , NULL },
	{ "supportsaturation"	, para_supportsaturation	, AUTHORITY_VIEWER   , NULL },
	{ "supportbacklight"	, para_supportbacklight		, AUTHORITY_VIEWER   , NULL },
	{ "supportsharpness"	, para_supportsharpness		, AUTHORITY_VIEWER   , NULL },
	{ "supportaudioinswitch"	, para_supportaudioinswitch	, AUTHORITY_VIEWER   , NULL },
	{ "supportaudioencselect"	, para_supportaudioencselect	, AUTHORITY_VIEWER   , NULL },
	{ "audioinsource"		, para_audioinsource		, AUTHORITY_VIEWER   , NULL },
	{ "audiotype"		, para_audiotype		, AUTHORITY_VIEWER   , NULL },
	{ "audiotypename"		, para_audiotypename		, AUTHORITY_VIEWER   , NULL },
	
	//two way audio
	{ "supporttwowayaudio"    , para_supporttwowayaudio       , AUTHORITY_OPERATOR   , NULL },
	{ "speaktoken"    , para_speaktoken       , AUTHORITY_VIEWER   , NULL },

	// RS-485  /* add by Alex, 2008.12.24  */
	{ "supportrs485"         , para_supportrs485         , AUTHORITY_VIEWER   , NULL },
	{ "uartspeedname"        , para_uartspeedname        , AUTHORITY_OPERATOR , NULL },
	{ "rs485enable"          , para_485enable            , AUTHORITY_OPERATOR , NULL },
	{ "rs485protocol"        , para_485protocol          , AUTHORITY_OPERATOR , NULL },
	{ "rs485protocolname"    , para_485protocolname      , AUTHORITY_OPERATOR , NULL },
	{ "rs485data"            , para_485data              , AUTHORITY_OPERATOR , NULL },
	{ "rs485stop"            , para_485stop              , AUTHORITY_OPERATOR , NULL },
	{ "rs485parity"          , para_485parity            , AUTHORITY_OPERATOR , NULL },
	{ "485type"              , para_485type              , AUTHORITY_OPERATOR , NULL },
	{ "uarttypename"         , para_uarttypename         , AUTHORITY_OPERATOR , NULL },
	{ "485ID"                , para_485id                , AUTHORITY_OPERATOR , NULL },	
	{ "485speed"             , para_485speed             , AUTHORITY_OPERATOR , NULL },
#if defined (SUPPORT_RS485) && defined(CONFIG_BRAND_DLINK)
	{ "ptzpresetname"        , para_ptzpresetname        , AUTHORITY_OPERATOR , NULL },
	{ "ptzpresetindex"       , para_ptzpresetindex       , AUTHORITY_OPERATOR , NULL },
	{ "maxptzpresetnum"      , para_maxptzpresetnum      , AUTHORITY_OPERATOR , NULL },
	{ "ptzpresetnamelen"     , para_ptzpresetnamelen     , AUTHORITY_OPERATOR , NULL },
#endif	
	// PPPoE
	{ "pppoeenable"          , para_pppoeenable          , AUTHORITY_OPERATOR , NULL },
	{ "pppoemodename"        , para_pppoemodename        , AUTHORITY_OPERATOR , NULL },
	{ "pppoeactive"          , para_pppoeactive          , AUTHORITY_OPERATOR , NULL },
	{ "pppoeaccount"         , para_pppoeaccount         , AUTHORITY_OPERATOR , NULL },
	{ "pppoepwd"             , para_pppoepwd             , AUTHORITY_OPERATOR , NULL },
	{ "maxpppoeusrlen"       , para_maxpppoeusrlen       , AUTHORITY_OPERATOR , NULL },
	{ "maxpppoepwdlen"       , para_maxpppoepwdlen       , AUTHORITY_OPERATOR , NULL },
	
	// DDNS
	{ "ddnsenable"           , para_ddnsenable           , AUTHORITY_OPERATOR , NULL },
	{ "ddnstype"             , para_ddnstype             , AUTHORITY_OPERATOR , NULL },
	{ "ddnshostname"         , para_ddnshostname         , AUTHORITY_OPERATOR , NULL },
	{ "ddnsaccount"          , para_ddnsaccount          , AUTHORITY_OPERATOR , NULL },
	{ "ddnspassword"         , para_ddnspassword         , AUTHORITY_OPERATOR , NULL },
	{ "ddnstypename"         , para_ddnstypename         , AUTHORITY_OPERATOR , NULL },
	{ "ddnsinterval"         , para_ddnsinterval         , AUTHORITY_OPERATOR , NULL },
	{ "maxddnsusrlen"        , para_maxddnsusrlen        , AUTHORITY_OPERATOR , NULL },
	{ "maxddnspwdlen"        , para_maxddnspwdlen        , AUTHORITY_OPERATOR , NULL },
	
	//{ "ddnswebname"          , para_ddnswebname             , AUTHORITY_OPERATOR , NULL },
	
	// Day Light Saving (DST)
	{ "daylight"			, para_enabledst			, AUTHORITY_VIEWER   , NULL },
	{ "daylightmode"		, para_dstmode				, AUTHORITY_VIEWER   , NULL },	
	{ "daylightshift"		, para_dstshift 			, AUTHORITY_VIEWER	 , NULL },
	{ "daylightshiftname"	, para_dstshiftname			, AUTHORITY_VIEWER	 , NULL },
	{ "daylightstart"		, para_dststart 			, AUTHORITY_VIEWER	 , NULL },
	{ "daylightend" 		, para_dstend				, AUTHORITY_VIEWER	 , NULL },	

#if 0 // NO MORE USED. disable by Alex. 2009.12.15 
	// Samba Client
	{ "ensmbrecord"          , para_smbenrecord          , AUTHORITY_OPERATOR , NULL },
	{ "smbauth"              , para_smbauthority         , AUTHORITY_OPERATOR , NULL },
	{ "smbuser"              , para_smbusername          , AUTHORITY_OPERATOR , NULL },
	{ "smbpassword"          , para_smbpassword          , AUTHORITY_OPERATOR , NULL },
	{ "smbserver"            , para_smbserver            , AUTHORITY_OPERATOR , NULL },
	{ "smbpath"              , para_smbpath              , AUTHORITY_OPERATOR , NULL },
	{ "smbreprofile"         , para_smbprofile           , AUTHORITY_OPERATOR , NULL },
	{ "smbrecrewrite"        , para_smbrewrite           , AUTHORITY_OPERATOR , NULL },
	{ "smbrectype"           , para_smbtype              , AUTHORITY_OPERATOR , NULL },
	{ "smbreceventtype"      , para_smbeventtype         , AUTHORITY_OPERATOR , NULL },
#endif 

	// For test event test server
	{ "testtimeout"          , para_testtimeout          , AUTHORITY_OPERATOR , NULL },
	{ "replytestserver"      , para_replytest            , AUTHORITY_OPERATOR , NULL },
	{ "recresolutionname"	 , para_recresolutionname    , AUTHORITY_OPERATOR , NULL },
	
/*
#ifdef SUPPORT_EVENT_2G
	// Digital Output
	{ "enmotionout" 		 , para_trgoutmotion		 , AUTHORITY_OPERATOR , NULL },
	{ "ensignal1out"		 , para_trgoutdin1			 , AUTHORITY_OPERATOR , NULL },
	{ "ensignal2out"		 , para_trgoutdin2			 , AUTHORITY_OPERATOR , NULL },
#endif


#ifdef MODEL_LC7315
	{ "ensignal1snapshot"    , para_trgjpgdin1           , AUTHORITY_OPERATOR , NULL },
	{ "ensignal2snapshot"    , para_trgjpgdin2           , AUTHORITY_OPERATOR , NULL },
	{ "entriggerout"		 , para_trgoutenable		 , AUTHORITY_OPERATOR , NULL },

	// Recording
	{ "enmotionrec"          , para_trgavimotion         , AUTHORITY_OPERATOR , NULL },
	{ "ensignal1rec"         , para_trgavidin1           , AUTHORITY_OPERATOR , NULL },
	{ "ensignal2rec"         , para_trgavidin2           , AUTHORITY_OPERATOR , NULL },	
	{ "enmotionsnapshot"	 , para_trgjpgmotion		 , AUTHORITY_OPERATOR , NULL },

	// SnapShot
	{ "ensnapshot"		 , para_ensnapshot	     , AUTHORITY_OPERATOR , NULL },
	{ "ftpdeffilename"	 , para_ftpjpegname	     , AUTHORITY_OPERATOR , NULL },
	{ "recresolution"		 , para_recresolution		 , AUTHORITY_OPERATOR , NULL },
	{ "recrewrite"		 , para_recrewrite	     , AUTHORITY_OPERATOR , NULL },
	{ "enrecord"			 , para_record				 , AUTHORITY_OPERATOR , NULL },
	{ "enkeeprecord"	 , para_keeprecord	     , AUTHORITY_OPERATOR , NULL },
	{ "eneventrecord"	 , para_eventrecord	     , AUTHORITY_OPERATOR , NULL },
	{ "enschedulerecord"	 , para_schedulerec	     , AUTHORITY_OPERATOR , NULL },
	{ "minfreespace"	 , para_freespace	     , AUTHORITY_OPERATOR , NULL },

#endif
*/	// NO MORE USED. disable by Alex. 2009.12.15 

	// Setup Network
	{ "secondarydnsip"       , para_seconddnsip          , AUTHORITY_OPERATOR , NULL },
	{ "rtspport"             , para_rtspport             , AUTHORITY_VIEWER , NULL },
	{ "defaultrtspport"      , para_defaultrtspport      , AUTHORITY_OPERATOR , NULL },
	{ "setupnpcexport"       , para_setupnpcexport       , AUTHORITY_OPERATOR , NULL },
	{ "upnpforwardingactive" , papa_upnpforwardingactive , AUTHORITY_OPERATOR , NULL },
	
	{ "allframeratename"     , para_allframeratename     , AUTHORITY_OPERATOR , NULL },
	// Profile 1
	{ "supportmultiprofile"	 , para_supportprofile       , AUTHORITY_OPERATOR , NULL },
	{ "profile1format"		 , para_prf1format           , AUTHORITY_OPERATOR , NULL },
	{ "profile1formatname"   , para_prf1formatname       , AUTHORITY_OPERATOR , NULL },
	{ "profile1resolution"	 , para_prf1res	             , AUTHORITY_VIEWER , NULL },
	{ "profile1resolutionname" , para_prf1resname        , AUTHORITY_OPERATOR , NULL },
	{ "profile1rate"		 , para_prf1rate	         , AUTHORITY_VIEWER   , NULL },
	{ "profile1ratename"     , para_prf1ratename         , AUTHORITY_OPERATOR , NULL },
	{ "profile1bps" 		 , para_prf1bps	             , AUTHORITY_OPERATOR , NULL },
	{ "profile1bpsname"      , para_prf1bpsname          , AUTHORITY_OPERATOR , NULL },
	{ "profile1quality"      , para_prf1quality          , AUTHORITY_OPERATOR , NULL },
	{ "profile1qualityjpeg"      , para_profile1qualityjpeg          , AUTHORITY_OPERATOR , NULL },
	{ "profile1qualitympeg4"      , para_profile1qualitympeg4          , AUTHORITY_OPERATOR , NULL },
	//{ "profile1rtspurl" 	 , para_prf1rtsp	         , AUTHORITY_OPERATOR , NULL },
	{ "profile1qmode" 	 	 , para_prf1qmode	         , AUTHORITY_OPERATOR , NULL },
	{ "profile1keyframeinterval" 	 	 , para_profile1keyframeinterval	         , AUTHORITY_OPERATOR , NULL },
	{ "profile1keyframeintervalname" 	 	 , para_profile1keyframeintervalname	         , AUTHORITY_OPERATOR , NULL },
	//{ "profile1customize" 	 	 , para_profile1customize	         , AUTHORITY_OPERATOR , NULL },
	{ "profile1viewres" 		 , para_prf1view 	     , AUTHORITY_OPERATOR , NULL },

	// Profile 2
	{ "profile2format"		 , para_prf2format           , AUTHORITY_OPERATOR , NULL },
	{ "profile2formatname"   , para_prf2formatname       , AUTHORITY_OPERATOR , NULL },
	{ "profile2resolution"	 , para_prf2res	             , AUTHORITY_VIEWER , NULL },
	{ "profile2resolutionname" , para_prf1resname        , AUTHORITY_OPERATOR , NULL },
	{ "profile2rate"		 , para_prf2rate	         , AUTHORITY_VIEWER   , NULL },
	{ "profile2ratename"     , para_prf1ratename         , AUTHORITY_OPERATOR , NULL },
	{ "profile2bps" 		 , para_prf2bps	             , AUTHORITY_OPERATOR , NULL },
	{ "profile2bpsname"      , para_prf2bpsname          , AUTHORITY_OPERATOR , NULL },
	{ "profile2quality"      , para_prf2quality          , AUTHORITY_OPERATOR , NULL },
	{ "profile2qualityjpeg"      , para_profile2qualityjpeg          , AUTHORITY_OPERATOR , NULL },
	{ "profile2qualitympeg4"      , para_profile2qualitympeg4          , AUTHORITY_OPERATOR , NULL },
	//{ "profile2rtspurl" 	 , para_prf2rtsp	         , AUTHORITY_OPERATOR , NULL },
	{ "profile2qmode" 	 	 , para_prf2qmode	         , AUTHORITY_OPERATOR , NULL },
	{ "profile2keyframeinterval" 	 	 , para_profile2keyframeinterval	         , AUTHORITY_OPERATOR , NULL },
	{ "profile2keyframeintervalname" 	 	 , para_profile1keyframeintervalname	         , AUTHORITY_OPERATOR , NULL },
	//{ "profile2customize" 	 	 , para_profile2customize	         , AUTHORITY_OPERATOR , NULL },
	{ "profile2viewres"		 , para_prf2view	     , AUTHORITY_OPERATOR , NULL },

	// Profile 3
	{ "profile3format"		 , para_prf3format           , AUTHORITY_OPERATOR , NULL },
	{ "profile3formatname"	 , para_prf3formatname       , AUTHORITY_OPERATOR , NULL },
	{ "profile3resolution"	 , para_prf3res	             , AUTHORITY_VIEWER , NULL },
	{ "profile3resolutionname" , para_prf1resname        , AUTHORITY_OPERATOR , NULL },
	{ "profile3rate"		 , para_prf3rate	         , AUTHORITY_VIEWER   , NULL },
	{ "profile3ratename"     , para_prf1ratename         , AUTHORITY_OPERATOR , NULL },
	{ "profile3bps" 		 , para_prf3bps	             , AUTHORITY_OPERATOR , NULL },
	{ "profile3bpsname"      , para_prf3bpsname          , AUTHORITY_OPERATOR , NULL },
	{ "profile3quality"      , para_prf3quality          , AUTHORITY_OPERATOR , NULL },
	{ "profile3qualityjpeg"      , para_profile3qualityjpeg          , AUTHORITY_OPERATOR , NULL },
	{ "profile3qualitympeg4"      , para_profile3qualitympeg4          , AUTHORITY_OPERATOR , NULL },
	//{ "profile3rtspurl" 	 , para_prf3rtsp	         , AUTHORITY_OPERATOR , NULL },
	{ "profile3qmode" 	 	 , para_prf3qmode	         , AUTHORITY_OPERATOR , NULL },
	{ "profile3keyframeinterval" 	 	 , para_profile3keyframeinterval	         , AUTHORITY_OPERATOR , NULL },
	{ "profile3keyframeintervalname" 	 	 , para_profile1keyframeintervalname	         , AUTHORITY_OPERATOR , NULL },
	//{ "profile3customize" 	 	 , para_profile3customize	         , AUTHORITY_OPERATOR , NULL },
	{ "profile3viewres"		 , para_prf3view	     , AUTHORITY_OPERATOR , NULL },

	// Profile 4
	{ "profile4format"		 , para_prf4format					 , AUTHORITY_OPERATOR , NULL },
#if defined( PLAT_DM365 )
	{ "profile4formatname"	 , para_prf1formatname			 , AUTHORITY_OPERATOR , NULL },
	{ "profile4resolutionname" , para_prf1resname 			 , AUTHORITY_OPERATOR , NULL },
	{ "profile4ratename"		 , para_prf1ratename				 , AUTHORITY_OPERATOR , NULL },
	{ "profile4keyframeinterval" 	 	 , para_profile4keyframeinterval	         , AUTHORITY_OPERATOR , NULL },
	{ "profile4keyframeintervalname" 	 	 , para_profile1keyframeintervalname	         , AUTHORITY_OPERATOR , NULL },
#elif defined( PLAT_DM355 )
	{ "profile4formatname"	 , para_prf4formatname			 , AUTHORITY_OPERATOR , NULL },
	{ "profile4resolutionname" , para_prf4resname 			 , AUTHORITY_OPERATOR , NULL },
	{ "profile4ratename"		 , para_prf4ratename				 , AUTHORITY_OPERATOR , NULL },
#endif
	{ "profile4resolution"	 , para_prf4res 						 , AUTHORITY_VIEWER , NULL },
	
	{ "profile4rate"		 , para_prf4rate					 , AUTHORITY_VIEWER , NULL },
	{ "profile4bps" 		 , para_prf4bps 						 , AUTHORITY_OPERATOR , NULL },
	{ "profile4bpsname" 		 , para_prf4bpsname 				 , AUTHORITY_OPERATOR , NULL },
	{ "profile4quality" 		 , para_prf4quality 				 , AUTHORITY_OPERATOR , NULL },
	{ "profile4qualityjpeg"      , para_profile4qualityjpeg          , AUTHORITY_OPERATOR , NULL },
	{ "profile4qualitympeg4"      , para_profile4qualitympeg4          , AUTHORITY_OPERATOR , NULL },
	{ "profile4qualityavc" 		 , profile4qualityavc 		 , AUTHORITY_OPERATOR , NULL },
	//{ "profile4rtspurl" 	 , para_prf4rtsp					 , AUTHORITY_OPERATOR , NULL },
	{ "profile4qmode" 		 , para_prf4qmode 				 , AUTHORITY_OPERATOR , NULL },
	
	{ "supportprofilenumber" 		 , para_supportprofilenumber 				 , AUTHORITY_OPERATOR , NULL },
	{ "supportaspectratio" 		 , para_supportaspectratio 				 , AUTHORITY_OPERATOR , NULL },
	{ "profilenumber" 		 , para_profilenumber 				 , AUTHORITY_OPERATOR , NULL },
	{ "profilenumbername" 		 , para_profilenumbername 				 , AUTHORITY_OPERATOR , NULL },
	{ "aspectratio" 		 , para_aspectratio 				 , AUTHORITY_OPERATOR , NULL },
	{ "aspectrationame" 		 , para_aspectrationame 				 , AUTHORITY_OPERATOR , NULL },
	
	//sensor crop
	{ "sensorcropresolution" 		 , para_sensorcropresolution 				 , AUTHORITY_OPERATOR , NULL },
	{ "sensorcropresolutionname" 		 , para_sensorcropresolutionname 				 , AUTHORITY_OPERATOR , NULL },
	{ "sensorstartx" 		 , para_sensorstartx 				 , AUTHORITY_OPERATOR , NULL },
	{ "sensorstarty" 		 , para_sensorstarty 				 , AUTHORITY_OPERATOR , NULL },

	{ "minoutvolume" 	 	 , para_minoutvolume         , AUTHORITY_OPERATOR , NULL },
	//{ "audiooutenable" 	 	 , para_audiooutenable       , AUTHORITY_OPERATOR , NULL },
	//{ "audiooutvolume" 	 	 , para_audiooutvolume       , AUTHORITY_OPERATOR , NULL },
	{ "audioinvolumename" 	 , para_audioinname          , AUTHORITY_OPERATOR , NULL },
	{ "audiooutname" 	 	 , para_audiooutname         , AUTHORITY_OPERATOR , NULL },
	{ "wintimezone"			 , para_wintimezone		     , AUTHORITY_OPERATOR , NULL },
//#if defined( SENSOR_USE_IMX035 )
	//{ "muteenable"			 	, para_muteenable		     , AUTHORITY_OPERATOR , NULL },
	{ "lineingain"			 , para_lineingain		     , AUTHORITY_OPERATOR , NULL },
	{ "lineingainname"			 , para_lineingainname		     , AUTHORITY_OPERATOR , NULL },
	{ "externalmicgain"			 , para_externalmicgain		     , AUTHORITY_OPERATOR , NULL },
	{ "externalmicgainname"			 , para_externalmicgainname		     , AUTHORITY_OPERATOR , NULL },
	{ "micgainname"			 , para_externalmicgainname		     , AUTHORITY_OPERATOR , NULL },
#if defined PLAT_DM355
	{ "micgain"			 	 , para_micgain		     , AUTHORITY_OPERATOR , NULL },
#elif defined PLAT_DM365
	{ "micgain"			 	 , para_externalmicgain		     , AUTHORITY_OPERATOR , NULL },
#else
#error Unknown product model
#endif

	{ "wizardinit"			 , para_wizardinit			 , AUTHORITY_OPERATOR , NULL },
	
	{ "audioamplifyratio"		, para_audioamplifyratio		, AUTHORITY_OPERATOR , NULL },
	{ "audiooutenable"			 , para_audiooutenable		     , AUTHORITY_OPERATOR , NULL },
	{ "audiooutvolume"			 , para_audiooutvolume		     , AUTHORITY_OPERATOR , NULL },
	{ "audiooutvolumename"			 , para_audiooutvolumename		     , AUTHORITY_OPERATOR , NULL },
//#endif
	//{ "stesdrenable"         , para_sdrenable            , AUTHORITY_OPERATOR , NULL },

	{ "ledmode"            , para_ledstatus            , AUTHORITY_OPERATOR , NULL },
	{ "guest"                , para_guest                , AUTHORITY_OPERATOR , NULL },
	{ "motionwziardconfig"   , para_mtwzconfig           , AUTHORITY_OPERATOR , NULL },

	{ "audioinvolume"       , para_audioinvolume    , AUTHORITY_OPERATOR , NULL },
	{ "audioinenable" 	 	 , para_audioinenable          , AUTHORITY_OPERATOR , NULL },
	/*Old*/
	//{ "poweroutstatus"       , para_powerout             , AUTHORITY_OPERATOR , NULL },
	//{ "supportpower"         , para_supportpower                     , AUTHORITY_OPERATOR, NULL },
	//{ "powerschedule"       , para_powerschedule         , AUTHORITY_OPERATOR , NULL },

	{ "dcpowermode"       	, para_powerout             , AUTHORITY_OPERATOR , NULL },
	{ "supportdcpower"         , para_supportdcpower                     , AUTHORITY_OPERATOR, NULL },
	{ "dcpowerschedule"       , para_powerschedule         , AUTHORITY_OPERATOR , NULL },

	{ "irledmode"       	, para_powerout             , AUTHORITY_OPERATOR , NULL },
	{ "supportirled"         , para_supportirled                     , AUTHORITY_OPERATOR, NULL },
	{ "irledschedule"       , para_powerschedule         , AUTHORITY_OPERATOR , NULL },

	//Day & Night control
#if  SUPPORT_DNC_EX 
	{ "dncontrolscheduleEx"	, para_dncontrolscheduleex	, AUTHORITY_OPERATOR , NULL },
#else
	{ "dncontrolschedule"		, para_dncontrolschedule		, AUTHORITY_OPERATOR , NULL },
#endif	
	{ "dncontrolsen"		, para_dncontrolsen			, AUTHORITY_OPERATOR , NULL },
	{ "dncontrold2n"		, para_dncontrold2n			, AUTHORITY_OPERATOR , NULL },
	{ "dncontroln2d"		, para_dncontroln2d			, AUTHORITY_OPERATOR , NULL },
	{ "dncontrold2nmin"		, para_dncontrold2nmin		, AUTHORITY_OPERATOR , NULL },
	{ "dncontrold2nmax"		, para_dncontrold2nmax		, AUTHORITY_OPERATOR , NULL },
	{ "dncontroln2dmin"		, para_dncontroln2dmin		, AUTHORITY_OPERATOR , NULL },
	{ "dncontroln2dmax"		, para_dncontroln2dmax		, AUTHORITY_OPERATOR , NULL },
	{ "dncontrolsenname"	, para_dncontrolsenname		, AUTHORITY_OPERATOR , NULL },
	{ "dncontrolmodename"	, para_dncontrolmodename	, AUTHORITY_OPERATOR , NULL },
	{ "dncontrolmode"		, para_dncontrolmode		, AUTHORITY_OPERATOR , NULL },
	
	//IP filter
	{ "ipfilterenable"		, para_ipfilterenable		, AUTHORITY_OPERATOR , NULL },
	{ "ipfilterdefaultpolicy"	, para_ipfilterdefaultpolicy	, AUTHORITY_OPERATOR , NULL },
	{ "maxipfilterpolicy"		, para_maxipfilterpolicy	, AUTHORITY_OPERATOR , NULL },
	{ "ipfilterpolicy"		, para_ipfilterpolicy		, AUTHORITY_OPERATOR , NULL },
	{ "ipfilterallow"		, para_ipfilterallow		, AUTHORITY_OPERATOR , NULL },
	{ "ipfilterdeny"		, para_ipfilterdeny		, AUTHORITY_OPERATOR , NULL },
	
	//Traffic control
	{ "maxquotainbound"			    , para_maxquotainbound				, AUTHORITY_OPERATOR , NULL },
	{ "maxquotaoutbound"			    , para_maxquotaoutbound				, AUTHORITY_OPERATOR , NULL },
	{ "quotainbound"			    , para_quotainbound				, AUTHORITY_OPERATOR , NULL },
	{ "quotaoutbound"			    , para_quotaoutbound				, AUTHORITY_OPERATOR , NULL },
#if defined( SENSOR_USE_IMX035 )
	//Virtual Server
	{ "virtaulen"			    , para_virtaulenable				, AUTHORITY_OPERATOR , NULL },
	{ "virtaulprotocol"			    , para_virtaulprotocol				, AUTHORITY_OPERATOR , NULL },
	{ "virtauldata"			    , para_virtauldata				, AUTHORITY_OPERATOR , NULL },	
	{ "aeshutter"			    , para_aeshutter				, AUTHORITY_OPERATOR , NULL },
	//{ "aes"			    , para_aes				, AUTHORITY_OPERATOR , NULL },
#endif

#if 1//defined( SENSOR_USE_IMX035 ) || defined( SENSOR_USE_OV2715 )
	{ "aes"			    , para_aes				, AUTHORITY_OPERATOR , NULL },
#endif

	{ "supportgamma"                , para_supportgamma                     , AUTHORITY_OPERATOR , NULL },
	{ "supportflip"                 , para_supportflip                , AUTHORITY_OPERATOR , NULL },
#if defined( SENSOR_USE_IMX035 )
	{ "gammaname"			    , para_gammaname				, AUTHORITY_OPERATOR , NULL },
	{ "gamma"			    , para_gamma				, AUTHORITY_OPERATOR , NULL },
	{ "flipname"					, para_flipmirrorname 			, AUTHORITY_OPERATOR , NULL },
#endif
	{ "flip"					, para_flip 			, AUTHORITY_OPERATOR , NULL },
	{ "antiflicker"			, para_antiflicker 			, AUTHORITY_OPERATOR , NULL },
	{ "quadmodeselectname"	, para_quadmodeselectname	, AUTHORITY_OPERATOR , NULL },
	{ "videocodecname"		, para_videocodecname		, AUTHORITY_OPERATOR , NULL },
	{ "videocodec"			, para_videocodec			, AUTHORITY_OPERATOR , NULL },
	{ "videomodename"		, para_videomodename		, AUTHORITY_VIEWER , NULL },
	{ "videomode"		    , para_videomode		    , AUTHORITY_VIEWER , NULL },
	{ "flickless"		    , para_flickless		    , AUTHORITY_VIEWER , NULL },
	{ "powerline"		    , para_powerline		    , AUTHORITY_VIEWER , NULL },
	{ "supportflickless"	, para_supportflickless		    , AUTHORITY_VIEWER , NULL },
	{ "supportpowerline"	, para_supportpowerline		    , AUTHORITY_VIEWER , NULL },
	{ "supportagc"			, para_supportagc			, AUTHORITY_VIEWER , NULL },
	{ "supportagcctrl"		, para_supportagcctrl			, AUTHORITY_VIEWER , NULL },
	{ "supportvideoalc"		, para_supportvideoalc			, AUTHORITY_VIEWER , NULL },
	{ "supportaes"			, para_supportaes			, AUTHORITY_VIEWER , NULL },
	{ "agc"					, para_agc					, AUTHORITY_OPERATOR , NULL },
	{ "agcname"					, para_agcname					, AUTHORITY_OPERATOR , NULL },
	{ "colorkiller"			, para_daynight			, AUTHORITY_OPERATOR , NULL },
	{ "fluorescent"		, para_fluorescent			, AUTHORITY_VIEWER , NULL },
	{ "fluorescentname"		, para_fluorescentname			, AUTHORITY_VIEWER , NULL },
	{ "mirror"			    , para_mirror				, AUTHORITY_OPERATOR , NULL },
	{ "upnpenable"			    , para_upnpenable				, AUTHORITY_OPERATOR , NULL },
	{ "supportonvif"		, para_supportonvif		    , AUTHORITY_VIEWER , NULL },
	
	//UPnP Client
	{ "upnpcenable"			    , para_upnpcenabl				, AUTHORITY_OPERATOR , NULL },
//	{ "upnpcextport"			, para_upnpcextport				, AUTHORITY_OPERATOR , NULL },
	{ "upnpforwardingport"		, para_upnpforwardingport		, AUTHORITY_OPERATOR , NULL },
#if 0
	{ "upnpport"			    , para_upnpport				, AUTHORITY_OPERATOR , NULL },	
	{ "upnpssdpage"			    , para_upnpssdpage				, AUTHORITY_OPERATOR , NULL },
	{ "upnpssdpport"			    , para_upnpssdpport				, AUTHORITY_OPERATOR , NULL },
#endif
	{ "blc"				, para_blc					, AUTHORITY_VIEWER , NULL },
	{ "kelvin"			, para_kelvin				, AUTHORITY_VIEWER , NULL },
	{ "supporthue"		, para_supporthue			, AUTHORITY_VIEWER , NULL },
	{ "supportfluorescent"	, para_supportfluorescent	, AUTHORITY_VIEWER , NULL },
	{ "supportmirror"		, para_supportmirror		, AUTHORITY_VIEWER , NULL },
	{ "supportkelvin"		, para_supportkelvin		, AUTHORITY_VIEWER , NULL },
	{ "senseup"			, para_senseup				, AUTHORITY_VIEWER , NULL },
	{ "supportsenseup"		, para_supportsenseup		, AUTHORITY_VIEWER , NULL },
	{ "supportmaxagcgain"	, para_supportmaxagcgain	, AUTHORITY_VIEWER , NULL },
	{ "supportblcmode"		, para_supportblcmode		, AUTHORITY_VIEWER , NULL },
	{ "supportbonjour"		, para_support_bonjour		, AUTHORITY_VIEWER , NULL },
	{ "supportaf"			, para_supportaf			, AUTHORITY_VIEWER , NULL },
	{ "supportantiflicker"	, para_supportantiflicker	, AUTHORITY_VIEWER , NULL },
	
	{ "blcmode"			, para_blcmode				, AUTHORITY_VIEWER , NULL },
	{ "supporthspeedshutter"	, para_supporthspeedshutter	, AUTHORITY_VIEWER , NULL },
	{ "hspeedshutter"		, para_hspeedshutter		, AUTHORITY_VIEWER , NULL },
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_OV2715 )
	{ "awbname" 		, para_awbname				, AUTHORITY_VIEWER , NULL },
#endif
	{ "awb" 				, para_awb					, AUTHORITY_OPERATOR , NULL },
	{ "awbname" 		, para_awbname				, AUTHORITY_VIEWER , NULL },
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 ) || defined( SENSOR_USE_OV2715 )
	{ "rgain"			, para_rgain				, AUTHORITY_VIEWER , NULL },
	{ "bgain"			, para_bgain				, AUTHORITY_VIEWER , NULL },
#endif
	{ "binningname"		, para_binningname				, AUTHORITY_VIEWER , NULL },
	{ "binning"			, para_binning					, AUTHORITY_VIEWER , NULL },
	{ "maxagcgainname"		, para_maxagcgainname		, AUTHORITY_VIEWER , NULL },
	{ "maxagcgain"		, para_maxagcgain			, AUTHORITY_VIEWER , NULL },
	{ "agcctrl"		, para_agcctrl			, AUTHORITY_VIEWER , NULL },
	{ "daynightname"		, para_daynightname			, AUTHORITY_VIEWER , NULL },  // disable by Alex. 2009.04.01
	{ "tvcable"			, para_tvcable				, AUTHORITY_VIEWER , NULL },
	{ "blcmodename"		, para_blcmodename			, AUTHORITY_VIEWER , NULL },
	{ "backlight"			, para_backlight			, AUTHORITY_VIEWER , NULL },
	{ "brightness"			, para_brightness			, AUTHORITY_VIEWER , NULL },
	{ "contrast"			, para_contrast				, AUTHORITY_VIEWER , NULL },
	{ "hue"				, para_hue					, AUTHORITY_VIEWER , NULL },
	{ "saturation"			, para_saturation			, AUTHORITY_VIEWER , NULL },
	//{ "exposure"			, para_exposure				, AUTHORITY_VIEWER , NULL },
	{ "sharpness"			, para_sharpness			, AUTHORITY_OPERATOR , NULL },
	{ "timezonename"		, para_timezonename			, AUTHORITY_OPERATOR , NULL },
	{ "timeformatname"		, para_timeformatname		, AUTHORITY_OPERATOR , NULL },
	//{ "schedule"			, para_schedule				, AUTHORITY_OPERATOR , NULL },
	{ "minbrightness"			, para_minbrightness			, AUTHORITY_OPERATOR , NULL },
	{ "maxbrightness"			, para_maxbrightness			, AUTHORITY_OPERATOR , NULL },
	{ "minsharpness"			, para_minsharpness			, AUTHORITY_OPERATOR , NULL },
	{ "maxsharpness"			, para_maxsharpness			, AUTHORITY_OPERATOR , NULL },
	{ "exposuretimename"			, para_exposuretimename			, AUTHORITY_OPERATOR , NULL },
	{ "maxshuttername"		, para_maxshuttername	, AUTHORITY_OPERATOR , NULL },
	{ "minshuttername"		, para_minshuttername	, AUTHORITY_OPERATOR , NULL },
	//{ "shuttername"		, para_shuttername	, AUTHORITY_OPERATOR , NULL },
	{ "exposureschedulemodename"	, para_exposureschedulemodename	, AUTHORITY_OPERATOR , NULL },
	{ "supportexposuretime"			, para_supportexposuretime		, AUTHORITY_OPERATOR , NULL },
	{ "supportexposuretimemode"		, para_supportexposuretimemode	, AUTHORITY_OPERATOR , NULL },
	{ "exposuretime"			, para_exposuretime 			, AUTHORITY_OPERATOR , NULL },
	{ "exposureschedule"		, para_exposureschedule 		, AUTHORITY_OPERATOR , NULL },
	{ "maxshutter"			, para_maxshutter 			, AUTHORITY_OPERATOR , NULL },
	{ "minshutter"			, para_minshutter 			, AUTHORITY_OPERATOR , NULL },
	{ "meteringmethod"			, para_meteringmethod 		, AUTHORITY_OPERATOR , NULL },
	{ "meteringmethodname"		, para_meteringmethodname 	, AUTHORITY_OPERATOR , NULL },
    { "conflicexposure"         , para_conflicexposure          , AUTHORITY_OPERATOR , NULL },

	{ "wdrenable"			, para_wdrenable 			, AUTHORITY_OPERATOR , NULL },
	{ "wdrlevel"			, para_wdrlevel 			, AUTHORITY_OPERATOR , NULL },
	{ "minwdrlevel"			, para_minwdrlevel 			, AUTHORITY_OPERATOR , NULL },
	{ "maxwdrlevel"			, para_maxwdrlevel 			, AUTHORITY_OPERATOR , NULL },
	{ "supportwdrlevel"			, para_supportwdrlevel 			, AUTHORITY_OPERATOR , NULL },

	{ "denoise"  		, para_denoise  , AUTHORITY_OPERATOR   , NULL },
	{ "mindenoise"  	, para_mindenoise  , AUTHORITY_OPERATOR   , NULL },
	{ "maxdenoise"  	, para_maxdenoise  , AUTHORITY_OPERATOR   , NULL },
	{ "evcomp" 			, para_evcomp	, AUTHORITY_OPERATOR   , NULL },
	{ "minevcomp"		, para_minevcomp, AUTHORITY_OPERATOR   , NULL },
	{ "maxevcomp"		, para_maxevcomp, AUTHORITY_OPERATOR   , NULL },
	{ "dremode"			, para_dremode, AUTHORITY_OPERATOR   , NULL },
	{ "dremodename"		, para_dremodename, AUTHORITY_OPERATOR   , NULL },

	{ "mincontrast"			, para_mincontrast			, AUTHORITY_VIEWER , NULL },
	{ "maxcontrast"			, para_maxcontrast			, AUTHORITY_VIEWER , NULL },
	{ "minsaturation"			, para_minsaturation			, AUTHORITY_VIEWER , NULL },
	{ "maxsaturation"			, para_maxsaturation			, AUTHORITY_VIEWER , NULL },

	{ "motionblock"		    , para_motionblock			, AUTHORITY_OPERATOR , NULL },
	{ "motionarea"		    , para_motionarea			, AUTHORITY_OPERATOR , NULL },
	{ "motionenable"		    , para_motionenable			, AUTHORITY_OPERATOR , NULL },
	{ "motiondrawenable"		    , para_motiondrawenable			, AUTHORITY_OPERATOR , NULL },
	{ "motionsensitivity"		, para_motionsensitivity	, AUTHORITY_OPERATOR , NULL },
	{ "motionname"		    , para_motionname			, AUTHORITY_OPERATOR , NULL },
	{ "motioncenable"		    , para_motioncenable		, AUTHORITY_OPERATOR , NULL },
	{ "motioncvalue"      	, para_motioncvalue			, AUTHORITY_OPERATOR , NULL },
	{ "motionlevel"		    , para_motionlevel			, AUTHORITY_OPERATOR , NULL },
	{ "motionpercentage"    , para_motionpercentage     , AUTHORITY_OPERATOR , NULL },
	{ "minmotionthreshold"	, para_minmotionthreshold	, AUTHORITY_OPERATOR , NULL },
	{ "maxmotionthreshold"	, para_maxmotionthreshold	, AUTHORITY_OPERATOR , NULL },
	//{ "lostalarm"			, para_lostalarm			, AUTHORITY_OPERATOR , NULL },
	//{ "sdaenable"			, para_sdaenable			, AUTHORITY_OPERATOR , NULL },
	//{ "aftpenable"			, para_aftpenable			, AUTHORITY_OPERATOR , NULL },
	//{ "asmtpenable"			, para_asmtpenable			, AUTHORITY_OPERATOR , NULL },
	//{ "recordduration"		, para_recordduration		, AUTHORITY_OPERATOR , NULL },
	//{ "alarmduration"		, para_alarmduration		, AUTHORITY_OPERATOR , NULL },
	{ "motionxlimit"		, para_motionxlimit	    	, AUTHORITY_OPERATOR , NULL },
	{ "motionylimit"	    , para_motionylimit			, AUTHORITY_OPERATOR , NULL },
	{ "motionxblock"		, para_motionxblock		    , AUTHORITY_OPERATOR , NULL },
	{ "motionyblock"		, para_motionyblock		    , AUTHORITY_OPERATOR , NULL },
	{ "ipncptz"			    , para_ipncptz		        , AUTHORITY_OPERATOR , NULL },

	{ "maxnamelen"	    	, para_maxnamelen			, AUTHORITY_VIEWER , NULL },
	{ "maxpwdlen"		    , para_maxpwdlen			, AUTHORITY_VIEWER , NULL },
	{ "authorityadmin"		, para_authorityadmin		, AUTHORITY_VIEWER , NULL },
	{ "authorityoperator"	, para_authorityoperator	, AUTHORITY_VIEWER , NULL },
	{ "authorityviewer"		, para_authorityviewer		, AUTHORITY_VIEWER , NULL },

	{ "minnamelen"		    , para_minnamelen			, AUTHORITY_VIEWER , NULL },
	{ "minpwdlen"		    , para_minpwdlen			, AUTHORITY_VIEWER , NULL },
	{ "user"			    , para_user				    , AUTHORITY_OPERATOR , NULL },
	{ "authority"		    , para_authority	   	    , AUTHORITY_OPERATOR , NULL },

	//{ "recordtype"			, para_recordtype			, AUTHORITY_OPERATOR , NULL },

	// SD Card
	{ "sddisplay"          , para_sddisplay          , AUTHORITY_VIEWER   , NULL },
	{ "sdinsert"           , para_sdinsert           , AUTHORITY_VIEWER   , NULL },
	{ "sdlock"             , para_sdlock             , AUTHORITY_VIEWER   , NULL },
	{ "sdleft"		       , para_sdleft             , AUTHORITY_OPERATOR , NULL },
	{ "sdused"		       , para_sdused             , AUTHORITY_OPERATOR , NULL },
	//{ "cardrewrite"		   , para_cardrewrite        , AUTHORITY_OPERATOR , NULL },
	//{ "sdduration"         , para_sdduration         , AUTHORITY_OPERATOR , NULL },
	//{ "sdrecordtype"       , para_sdrecordtype       , AUTHORITY_OPERATOR , NULL },
	//{ "sdrate"             , para_sdrate             , AUTHORITY_OPERATOR , NULL },
	//{ "sdcount"            , para_sdcount            , AUTHORITY_OPERATOR , NULL },
	//{ "maxsdrecord"        , para_maxsdrecord        , AUTHORITY_OPERATOR , NULL },
	//{ "sddurationname"     , para_sddurationname     , AUTHORITY_OPERATOR , NULL },
	{ "sdformat"           , para_sdformat           , AUTHORITY_OPERATOR , NULL },
	{ "sdprotect"          , para_sdprotect          , AUTHORITY_OPERATOR , NULL },

	{ "sdfolderlist"      , para_sdfolderlist        , AUTHORITY_OPERATOR   , NULL },
	{ "sdfilelist"        , para_sdfilelist          , AUTHORITY_OPERATOR   , NULL },
	{ "sdthispath"        , para_sdthispath          , AUTHORITY_OPERATOR   , NULL },
	{ "sdfpp"             , para_sdfpp               , AUTHORITY_OPERATOR   , NULL },
	{ "sdfppname"      	  , para_sdfppname        	 , AUTHORITY_OPERATOR   , NULL },
	{ "sdthispage"        , para_sdthispage          , AUTHORITY_OPERATOR   , NULL },
	{ "sdtotalpage"     , para_sdtotalpage       , AUTHORITY_OPERATOR   , NULL },
	//{ "sdthisfilenum"     , para_sdthisfilenum       , AUTHORITY_OPERATOR   , NULL },
	//{ "sdthisfoldernum"   , para_sdthisfoldernum     , AUTHORITY_OPERATOR   , NULL },
	//{ "sdsizeunit"        , para_sdsizeunit          , AUTHORITY_OPERATOR   , NULL },
	
	{ "privacywindowsize"	    , para_privacywindowsize		, AUTHORITY_OPERATOR , NULL },
	{ "motionwindowsize"	    , para_motionwindowsize		, AUTHORITY_OPERATOR , NULL },
	
	//ePtz
	{ "profile1eptzcoordinate"	    , para_profile1coordinate		, AUTHORITY_OPERATOR , NULL },
	{ "profile2eptzcoordinate"	    , para_profile2coordinate		, AUTHORITY_OPERATOR , NULL },
	{ "profile3eptzcoordinate"	    , para_profile3coordinate		, AUTHORITY_OPERATOR , NULL },
	
	//{ "sdfileformat"	    , para_sdfileformat		, AUTHORITY_OPERATOR , NULL },
	//{ "sdfileformatname"    , para_sdformatName		, AUTHORITY_OPERATOR , NULL },
	//{ "jpegcontfreq"	    , para_contfreq		    , AUTHORITY_OPERATOR , NULL },
	//{ "jpegcontfreqname"    , para_contfreqname		, AUTHORITY_OPERATOR , NULL },	
	//{ "aviduration"	        , para_aviduration		, AUTHORITY_OPERATOR , NULL },
	//{ "avidurationname"	    , para_avidurationname	, AUTHORITY_OPERATOR , NULL },
	//{ "aviformat"	        , para_aviformat		, AUTHORITY_OPERATOR , NULL },
	//{ "aviformatname"	    , para_aviformatname	, AUTHORITY_OPERATOR , NULL },
	//{ "ftpfileformat"	    , para_ftpfileformat	, AUTHORITY_OPERATOR , NULL },
	//{ "ftpmode"	    		, para_ftpmode			, AUTHORITY_OPERATOR , NULL },
	//{ "ftpjpegname"	    	, para_ftpjpegname		, AUTHORITY_OPERATOR , NULL },
	//{ "ftpfileformatname"   , para_formatName		, AUTHORITY_OPERATOR , NULL },
	//{ "attfileformat"	    , para_attfileformat	, AUTHORITY_OPERATOR , NULL },
	//{ "attfileformatname"   , para_formatName		, AUTHORITY_OPERATOR , NULL },
	{ "profile1xsize"		, para_stream1xsize	  	, AUTHORITY_VIEWER , NULL },
	{ "profile1ysize"		, para_stream1ysize	  	, AUTHORITY_VIEWER , NULL },
	{ "profile2xsize"		, para_stream2xsize	  	, AUTHORITY_VIEWER , NULL },
	{ "profile2ysize"		, para_stream2ysize	  	, AUTHORITY_VIEWER , NULL },
	{ "profile3xsize"		, para_stream3xsize	  	, AUTHORITY_VIEWER , NULL },
	{ "profile3ysize"		, para_stream3ysize	  	, AUTHORITY_VIEWER , NULL },
	{ "profile4xsize"		, para_stream4xsize	  	, AUTHORITY_VIEWER , NULL },
	{ "profile4ysize"		, para_stream4ysize	  	, AUTHORITY_VIEWER , NULL },
	{ "stream1name"			, para_stream1name	  	, AUTHORITY_VIEWER , NULL },
	{ "stream2name"			, para_stream2name	  	, AUTHORITY_VIEWER , NULL },
	{ "stream3name"			, para_stream3name	  	, AUTHORITY_VIEWER , NULL },
	//{ "stream4name"			, para_stream4name	  	, AUTHORITY_VIEWER , NULL },
	//{ "quality1mpeg4"		, para_quality1mpeg4	, AUTHORITY_VIEWER , NULL },
	//{ "quality2mpeg4"		, para_quality2mpeg4	, AUTHORITY_VIEWER , NULL },
	
	{ "dmsxsize"			, para_dmsxsize			, AUTHORITY_VIEWER , NULL },
	{ "dmsysize"			, para_dmsysize			, AUTHORITY_VIEWER , NULL },
	
	{ "numprofile"			, para_numprofile		, AUTHORITY_VIEWER , NULL },
	{ "webprofilenum"		, para_webprofilenum	, AUTHORITY_VIEWER , NULL },
	{ "supportprofile1"		, para_supportstream1	, AUTHORITY_VIEWER , NULL },
	{ "supportprofile2"		, para_supportstream2	, AUTHORITY_VIEWER , NULL },
	{ "supportprofile3"		, para_supportstream3	, AUTHORITY_VIEWER , NULL },
	{ "supportprofile4"		, para_supportstream4	, AUTHORITY_VIEWER , NULL },
	{ "img2a"				, para_image2a			, AUTHORITY_VIEWER , NULL },
	{ "img2aname"			, para_img2aname		, AUTHORITY_VIEWER , NULL },
	{ "ratecontrol"			, para_ratecontrol		, AUTHORITY_VIEWER , NULL },
	{ "ratecontrolname"		, para_ratecontrolname	, AUTHORITY_VIEWER , NULL },

	{ "kernelversion"		, para_kernelversion	, AUTHORITY_VIEWER , NULL },
	{ "biosversion"			, para_biosversion		, AUTHORITY_VIEWER , NULL },
	{ "CompileTime"			, para_compiletime		, AUTHORITY_VIEWER , NULL },

	//{ "smtpminattach"       , para_smtpminattach	, AUTHORITY_OPERATOR , NULL },
	//{ "smtpmaxattach"       , para_smtpmaxattach	, AUTHORITY_OPERATOR , NULL },
	//{ "asmtpattach"		    , para_asmtpattach		, AUTHORITY_OPERATOR , NULL },

	// HTTPS
	{ "httpsenable"       	, para_httpsenable		, AUTHORITY_OPERATOR , NULL },
	{ "httpscountry"       	, para_httpscountry		, AUTHORITY_OPERATOR , NULL },
	{ "maxhttpscountry"    	, para_maxhttpscountry	, AUTHORITY_OPERATOR , NULL },
	{ "httpsstate"       	, para_httpsstate		, AUTHORITY_OPERATOR , NULL },
	{ "maxhttpsstate"       , para_maxhttpsstate	, AUTHORITY_OPERATOR , NULL },
	{ "httpslocality"       , para_httpslocality	, AUTHORITY_OPERATOR , NULL },
	{ "maxhttpslocality"    , para_maxhttpslocality	, AUTHORITY_OPERATOR , NULL },
	{ "httpsorganization"   , para_httpsorganization	, AUTHORITY_OPERATOR , NULL },
	{ "maxhttpsorganization", para_maxhttpsorganization	, AUTHORITY_OPERATOR , NULL },
	{ "httpsorgunit"       	, para_httpsorgunit		, AUTHORITY_OPERATOR , NULL },
	{ "maxhttpsorgunit"     , para_maxhttpsorgunit	, AUTHORITY_OPERATOR , NULL },
	{ "httpscommon"       	, para_httpscommon		, AUTHORITY_OPERATOR , NULL },
	{ "maxhttpscommon"      , para_maxhttpscommon	, AUTHORITY_OPERATOR , NULL },
	{ "httpsvaliddays"      , para_httpsvaliddays	, AUTHORITY_OPERATOR , NULL },
	{ "httpscheckkey"       , para_httpscheckkey	, AUTHORITY_OPERATOR , NULL },
	{ "httpscheckcsr"       , para_httpscheckcsr	, AUTHORITY_OPERATOR , NULL },
	{ "httpscheckcert"      , para_httpscheckcert	, AUTHORITY_OPERATOR , NULL },
	{ "httpsport"           , para_httpsport       	, AUTHORITY_OPERATOR , NULL },
	{ "defaulthttpsport"    , para_defaulthttpsport , AUTHORITY_OPERATOR , NULL },

	// Wi-Fi
	{ "supportwifi"       	, para_wifi_support		, AUTHORITY_OPERATOR , NULL },
#ifdef SUPPORT_WIFI
	{ "wifienable"       	, para_wifi_enable		, AUTHORITY_OPERATOR , NULL },
	{ "wifissid"     	  	, para_wifi_ssid		, AUTHORITY_OPERATOR , NULL },
	{ "wifimode"       		, para_wifi_mode		, AUTHORITY_OPERATOR , NULL },
	{ "wifichannel"       	, para_wifi_channel		, AUTHORITY_OPERATOR , NULL },
	{ "wifiauth"      	 	, para_wifi_auth		, AUTHORITY_OPERATOR , NULL },
	{ "wifiencrypt"       	, para_wifi_encrypt		, AUTHORITY_OPERATOR , NULL },
	{ "wpa_key"       		, para_wpa_key			, AUTHORITY_OPERATOR , NULL },
	{ "wep_index"       	, para_wep_index		, AUTHORITY_OPERATOR , NULL },
	{ "wep_key1"       		, para_wep_key1			, AUTHORITY_OPERATOR , NULL },
	{ "wep_key2"       		, para_wep_key2			, AUTHORITY_OPERATOR , NULL },
	{ "wep_key3"       		, para_wep_key3			, AUTHORITY_OPERATOR , NULL },
	{ "wep_key4"       		, para_wep_key4			, AUTHORITY_OPERATOR , NULL },
	{ "wifiminchannel"      , para_wifi_minchannel	, AUTHORITY_OPERATOR , NULL },
	{ "wifimaxchannel"      , para_wifi_maxchannel	, AUTHORITY_OPERATOR , NULL },
	{ "wifisitesurveyname"	, para_wifi_sitelist	, AUTHORITY_VIEWER , NULL },
	{ "wifimodename"		, para_wifi_modename	, AUTHORITY_VIEWER , NULL },
	{ "wifichannelname"		, para_wifi_channelname	, AUTHORITY_VIEWER , NULL },
	{ "wifiauthname"		, para_wifi_authname	, AUTHORITY_VIEWER , NULL },
	{ "wifiencryptname"		, para_wifi_encryptname	, AUTHORITY_VIEWER , NULL },
#endif
	//{ "rftpenable"          , para_rftpenable		, AUTHORITY_OPERATOR , NULL },
	//{ "sdrenable"		    , para_sdrenable		, AUTHORITY_OPERATOR , NULL },
	//{ "cfrenable"		    , para_cfrenable  		, AUTHORITY_OPERATOR , NULL },

	// GIO in and GIO out
	{ "supportgiooutalwayson" , para_supportgiooutalwayson 	, AUTHORITY_OPERATOR , NULL },
	{ "supportgioin" 		, para_supportgioin		, AUTHORITY_VIEWER , NULL },
	{ "supportgioout" 		, para_supportgioout	, AUTHORITY_VIEWER , NULL },
	//{ "allgioinenable" 		, para_allgioinenable	, AUTHORITY_OPERATOR , NULL },
	//{ "allgiooutenable" 	, para_allgiooutenable	, AUTHORITY_OPERATOR , NULL },
	{ "gioinenable"		    , para_gioinenable 		, AUTHORITY_OPERATOR , NULL },
	{ "giointype"		    , para_giointype  		, AUTHORITY_OPERATOR , NULL },
	{ "gioinname"		    , para_gioinname  		, AUTHORITY_OPERATOR , NULL },
	{ "gioinstatus"		    , para_gioinstatus 		, AUTHORITY_OPERATOR , NULL },
	{ "giooutstatus"	    , para_giooutstatus		, AUTHORITY_OPERATOR , NULL },
	{ "giooutenable"	    , para_giooutenable 	, AUTHORITY_OPERATOR , NULL },
	{ "gioouttype"		    , para_gioouttype  		, AUTHORITY_OPERATOR , NULL },
	{ "giooutname"		    , para_giooutname  		, AUTHORITY_OPERATOR , NULL },
	{ "softwareversion"	    , para_softwareversion  , AUTHORITY_OPERATOR , NULL },
	{ "fullversion"	        , para_fullversion      , AUTHORITY_OPERATOR , NULL },

	// Time Stamp Setting
	{ "tstampcolor"			, para_tstampcolor		, AUTHORITY_OPERATOR , NULL },
	{ "tstamploc"			, para_tstamploc		, AUTHORITY_OPERATOR , NULL },
	{ "tstampenable"	    , para_tstampenable	    , AUTHORITY_OPERATOR , NULL },
	{ "tstampcolorname"	    , para_tstampcolorname	, AUTHORITY_OPERATOR , NULL },
	{ "tstamplocname"	    , para_tstamplocname	, AUTHORITY_OPERATOR , NULL },
	{ "tstampformatname"    , para_tstampformatname , AUTHORITY_OPERATOR , NULL },
	{ "tstampformat"        , para_tstampformat     , AUTHORITY_OPERATOR , NULL },
	{ "tstamplabel"         , para_tstamplabel      , AUTHORITY_OPERATOR , NULL },

	{ "audioinmax"          , para_audioinmax       , AUTHORITY_OPERATOR , NULL },
	{ "eventlocation"	    , para_event_location  	, AUTHORITY_OPERATOR , NULL },
	{ "event"				, para_get_event		, AUTHORITY_OPERATOR , NULL },
	{ "rtc_cal_status"    	, para_rtc_status       , AUTHORITY_OPERATOR, NULL },
	{ "rtc_cal_value"     	, para_rtc_value        , AUTHORITY_OPERATOR, NULL },

	// event 2nd generation
	{ "eventserverlist"	    , para_event_server     , AUTHORITY_OPERATOR, NULL },
	{ "eventmedialist"	    , para_event_media      , AUTHORITY_OPERATOR, NULL },
	{ "eventlist"	        , para_event_type       , AUTHORITY_OPERATOR, NULL },
	{ "eventrecordinglist"  , para_event_record     , AUTHORITY_OPERATOR, NULL },
	{ "isemptysamba"        , para_empty_smb        , AUTHORITY_OPERATOR, NULL },
	{ "isemptysd"           , para_empty_sd         , AUTHORITY_OPERATOR, NULL },
	{ "supportnetlost"      , para_support_netlost  , AUTHORITY_OPERATOR, NULL },
	{ "supportvideolost"    , para_support_videolost, AUTHORITY_OPERATOR, NULL },
	{ "supportpir"      	, para_support_pir  , AUTHORITY_OPERATOR, NULL },
    { "media_pic_pre_min"   , para_m_pic_pre_min    , AUTHORITY_OPERATOR, NULL },
    { "media_pic_pre_max"   , para_m_pic_pre_max    , AUTHORITY_OPERATOR, NULL },
    { "media_pic_post_min"  , para_m_pic_post_min   , AUTHORITY_OPERATOR, NULL },
    { "media_pic_post_max"  , para_m_pic_post_max   , AUTHORITY_OPERATOR, NULL },
    { "media_vid_pre_min"   , para_m_vid_pre_min    , AUTHORITY_OPERATOR, NULL },
    { "media_vid_pre_max"   , para_m_vid_pre_max    , AUTHORITY_OPERATOR, NULL },
    { "media_vid_post_min"  , para_m_vid_post_min   , AUTHORITY_OPERATOR, NULL },
    { "media_vid_post_max"  , para_m_vid_post_max   , AUTHORITY_OPERATOR, NULL },
    { "media_vid_maxsize"   , para_m_vid_maxsize    , AUTHORITY_OPERATOR, NULL },
    { "media_vid_minsize"   , para_m_vid_minsize    , AUTHORITY_OPERATOR, NULL },
    { "record_vid_maxsize"  , para_r_vid_maxsize    , AUTHORITY_OPERATOR, NULL },
    { "record_vid_minsize"  , para_r_vid_minsize    , AUTHORITY_OPERATOR, NULL },
    { "rewrite_minsize"     , para_rewrite_minsize  , AUTHORITY_OPERATOR, NULL },
    { "rewrite_maxsize"     , para_rewrite_maxsize  , AUTHORITY_OPERATOR, NULL },
	{ "maxactionmedia" 		, para_maxactionmedia	, AUTHORITY_OPERATOR, NULL },
	{ "maxeventserver"		, para_maxeventserver	, AUTHORITY_OPERATOR, NULL },
	{ "maxeventmedia"		, para_maxeventmedia	, AUTHORITY_OPERATOR, NULL },
	{ "maxeventnumber"		, para_maxeventnumber	, AUTHORITY_OPERATOR, NULL },
	{ "maxrecording"		, para_maxrecording		, AUTHORITY_OPERATOR, NULL },
	{ "eventactionchoice"   , para_event_actionchoice, AUTHORITY_OPERATOR, NULL },

	// CF Card
	{ "cfrate"              , para_cf_rate          , AUTHORITY_OPERATOR, NULL },
	{ "cfduration"          , para_cf_duration      , AUTHORITY_OPERATOR, NULL },
	{ "cfcount"             , para_cf_count         , AUTHORITY_OPERATOR, NULL },
	{ "maxcfrecord"         , para_cf_maxrecord     , AUTHORITY_OPERATOR, NULL },
	{ "cfleft"              , para_cf_left          , AUTHORITY_OPERATOR, NULL },
	{ "cfused"              , para_cf_used          , AUTHORITY_OPERATOR, NULL },
	{ "cfdurationname"      , para_cf_durationname  , AUTHORITY_OPERATOR, NULL },
	{ "cfrecordtype"        , para_cf_recordtype    , AUTHORITY_OPERATOR, NULL },
	{ "savingmode"        	, para_savingmode    	, AUTHORITY_OPERATOR, NULL },
	{ "analogoutput"      	, para_analog_output    , AUTHORITY_OPERATOR, NULL },

	//ajax
	{ "supportajax1"		, para_supportajax1		, AUTHORITY_VIEWER , NULL },
	{ "ajax1name"			, para_ajax1name		, AUTHORITY_VIEWER , NULL },
	{ "supportajax2"		, para_supportajax2		, AUTHORITY_VIEWER , NULL },
	{ "ajax2name"			, para_ajax2name		, AUTHORITY_VIEWER , NULL },
	{ "supportajax3"		, para_supportajax3		, AUTHORITY_VIEWER , NULL },
	{ "ajax3name"			, para_ajax3name		, AUTHORITY_VIEWER , NULL },
	
	// For web pages
	{ "httpstreamnum"		, para_httpstreamnum		, AUTHORITY_OPERATOR , NULL },
	{ "rtspstreamnum"		, para_rtspstreamnum		, AUTHORITY_OPERATOR , NULL },
	{ "profilecheckmode"	, para_profilecheckmode		, AUTHORITY_OPERATOR , NULL },

	{ "supportptzzoom"     , para_supportptzzoom        , AUTHORITY_VIEWER   , NULL },
	{ "supportfancontrol"     , para_supportfan        , AUTHORITY_VIEWER   , NULL },

	//AF
	{ "afmaxzoomstep"     	, para_afmaxzoomstep         , AUTHORITY_VIEWER   , NULL },
	{ "afmaxfocusstep"      , para_afmaxfocusstep        , AUTHORITY_VIEWER   , NULL },
	{ "autofocusmode"     	, para_autofocusmode         , AUTHORITY_VIEWER   , NULL },
	{ "autofocuslocation"   , para_autofocuslocation     , AUTHORITY_VIEWER   , NULL },
	{ "afzoomposition"     	, para_afzoomposition        , AUTHORITY_VIEWER   , NULL },
	{ "affocusposition"     , para_affocusposition       , AUTHORITY_VIEWER   , NULL },
	{ "lensspeed"     		, para_lensspeed			 , AUTHORITY_VIEWER   , NULL },
	{ "eptzspeed"     		, para_eptzspeed      		 , AUTHORITY_VIEWER   , NULL },
	{ "eptzstreamid"     	, para_eptzstreamid      		 , AUTHORITY_VIEWER   , NULL },
	//{ "eptzpresetlistname"  , para_eptzpresetlist      	 , AUTHORITY_VIEWER   , NULL },
	{ "eptzpresetlistname1"  , para_eptzpresetlist1      	 , AUTHORITY_VIEWER   , NULL },
	{ "eptzpresetlistname2"  , para_eptzpresetlist2      	 , AUTHORITY_VIEWER   , NULL },
	{ "eptzpresetlistname3"  , para_eptzpresetlist3      	 , AUTHORITY_VIEWER   , NULL },
	{ "sequencename1" 		, para_sequencename1      	 , AUTHORITY_VIEWER   , NULL },
	{ "sequencename2"  		, para_sequencename2      	 , AUTHORITY_VIEWER   , NULL },
	{ "sequencename3"  		, para_sequencename3      	 , AUTHORITY_VIEWER   , NULL },
	{ "sequencetime1" 		, para_sequencetime1      	 , AUTHORITY_VIEWER   , NULL },
	{ "sequencetime2"  		, para_sequencetime2      	 , AUTHORITY_VIEWER   , NULL },
	{ "sequencetime3"  		, para_sequencetime3      	 , AUTHORITY_VIEWER   , NULL },

#if defined (SUPPORT_VISCA) && defined(CONFIG_BRAND_DLINK)
	//VISCA inquiry
	{ "supportvisca"  		, para_supportvisca      , AUTHORITY_VIEWER   , NULL },
	{ "zoomposinq"  		, para_zoomposinq      	 , AUTHORITY_VIEWER   , NULL },
	{ "dzoommodeinq"  		, para_dzoommodeinq      , AUTHORITY_VIEWER   , NULL },
	{ "dzoomcsmodeinq"  	, para_dzoomcsmodeinq    , AUTHORITY_VIEWER   , NULL },
	{ "dzoomposinq"  		, para_dzoomposinq    	 , AUTHORITY_VIEWER   , NULL },
	{ "focusmodeinq"  		, para_focusmodeinq    	 , AUTHORITY_VIEWER   , NULL },
	{ "focusposinq"  		, para_focusposinq    	 , AUTHORITY_VIEWER   , NULL },
	{ "focusnearlimitinq"  	, para_focusnearlimitinq , AUTHORITY_VIEWER   , NULL },
	{ "afmodeinq"  			, para_afmodeinq 		 , AUTHORITY_VIEWER   , NULL },
	{ "wbmodeinq"  			, para_wbmodeinq 		 , AUTHORITY_VIEWER   , NULL },
	{ "rgaininq"  			, para_rgaininq 		 , AUTHORITY_VIEWER   , NULL },
	{ "bgaininq"  			, para_bgaininq 		 , AUTHORITY_VIEWER   , NULL },
	{ "aemodeinq"  			, para_aemodeinq 		 , AUTHORITY_VIEWER   , NULL },
	{ "slowshuttermodeinq"  , para_slowshuttermodeinq, AUTHORITY_VIEWER   , NULL },
	{ "shutterposinq"  		, para_shutterposinq 	 , AUTHORITY_VIEWER   , NULL },
	{ "irisposinq"  		, para_irisposinq 	 	 , AUTHORITY_VIEWER   , NULL },
	{ "gainposinq"  		, para_gainposinq 	 	 , AUTHORITY_VIEWER   , NULL },
	{ "brightposinq"  		, para_brightposinq 	 , AUTHORITY_VIEWER   , NULL },
	{ "expcompmodeinq"  	, para_expcompmodeinq 	 , AUTHORITY_VIEWER   , NULL },
	{ "expcompposinq"  		, para_expcompposinq 	 , AUTHORITY_VIEWER   , NULL },
	{ "backlightmodeinq"  	, para_backlightmodeinq  , AUTHORITY_VIEWER   , NULL },
	{ "wd"  				, para_wdinq  			 , AUTHORITY_VIEWER   , NULL },
	{ "apertureinq"  		, para_apertureinq  	 , AUTHORITY_VIEWER   , NULL },
	{ "freezemodeinq"  		, para_freezemodeinq  	 , AUTHORITY_VIEWER   , NULL },
	{ "pictureflipmodeinq"  , para_pictureflipmodeinq  	 , AUTHORITY_VIEWER   , NULL },
	//{ "stabilizer"  		, para_stabilizer  	 	 , AUTHORITY_VIEWER   , NULL },
	{ "icrmodeinq"  		, para_icrmodeinq  	 	 , AUTHORITY_VIEWER   , NULL },
	{ "autoicrmodeinq"  	, para_autoicrmodeinq  	 , AUTHORITY_VIEWER   , NULL },
	{ "irthreshold"  		, para_irthreshold  	 , AUTHORITY_VIEWER   , NULL },
	{ "privacydisplayinq"  	, para_privacydisplayinq , AUTHORITY_VIEWER   , NULL },
	{ "privacytransparency" , para_privacytransparency 	 , AUTHORITY_VIEWER   , NULL },
	{ "privacycolor"  		, para_privacycolor 	 , AUTHORITY_VIEWER   , NULL },
	{ "privacysizeinq" 		, para_privacysizeinq 	 , AUTHORITY_VIEWER   , NULL },
	{ "pantiltposinq"  		, para_pantiltposinq 	 , AUTHORITY_VIEWER   , NULL },
	{ "versioninq"  		, para_versioninq 	 	 , AUTHORITY_VIEWER   , NULL },
	{ "alarmstatus"  		, para_alarmstatus 	 	 , AUTHORITY_VIEWER   , NULL },
	{ "alarmoutput"  		, para_alarmoutput 	 	 , AUTHORITY_VIEWER   , NULL },
	{ "homefunction"  		, para_homefunction 	 , AUTHORITY_VIEWER   , NULL },
	{ "fliptypeinq"  		, para_fliptypeinq 	 	 , AUTHORITY_VIEWER   , NULL },	
	{ "angleadjust"  		, para_angleadjust 	 	 , AUTHORITY_VIEWER   , NULL },
	{ "speedbyzoom"  		, para_speedbyzoom 	 	 , AUTHORITY_VIEWER   , NULL },
	{ "autocalibration"  	, para_autocalibration 	 , AUTHORITY_VIEWER   , NULL },
	{ "2dnoisereduction"  	, para_2dnoisereduction  , AUTHORITY_VIEWER   , NULL },
	{ "3dnoisereduction"  	, para_3dnoisereduction  , AUTHORITY_VIEWER   , NULL },
	//{ "noisereduction"  	, para_noisereduction  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_1"  		, para_sequence64_1  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_2"  		, para_sequence64_2  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_3"  		, para_sequence64_3  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_4"  		, para_sequence64_4  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_5"  		, para_sequence64_5  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_6"  		, para_sequence64_6  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_7"  		, para_sequence64_7  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequence64_8"  		, para_sequence64_8  	 , AUTHORITY_VIEWER   , NULL },
	//{ "autopan"  			, para_autopan  	 	 , AUTHORITY_VIEWER   , NULL },
	{ "autopan_dir"  		, para_autopan_dir  	 , AUTHORITY_VIEWER   , NULL },
	{ "autopan_speed"  		, para_autopan_speed  	 , AUTHORITY_VIEWER   , NULL },
	{ "homefunctionsetting" , para_homefunctionsetting , AUTHORITY_VIEWER , NULL },
	{ "homefunctiontime"    , para_homefunctiontime  , AUTHORITY_VIEWER   , NULL },
	{ "homefunctionline"    , para_homefunctionline  , AUTHORITY_VIEWER   , NULL },
	{ "homeposition"        , para_homeposition      , AUTHORITY_VIEWER   , NULL },

	{ "sequencelinepointnum", para_sequencepointnum  , AUTHORITY_VIEWER   , NULL },
	{ "cruiselinenum" 		, para_cruiselinenum 	 , AUTHORITY_VIEWER   , NULL },
	{ "viscadomentsc" 		, para_visca_dome_ntsc 	 , AUTHORITY_VIEWER   , NULL },
	{ "viscadomezoom" 		, para_visca_dome_zoom 	 , AUTHORITY_VIEWER   , NULL },
	{ "viscadomeicr" 		, para_visca_dome_icr 	 , AUTHORITY_VIEWER   , NULL },
	{ "viscadomewdr" 		, para_visca_dome_wdr 	 , AUTHORITY_VIEWER   , NULL },
	{ "viscapresetlistname" , para_viscapresetlistname   , AUTHORITY_VIEWER   , NULL },
	{ "viscahomepresetlistname" , para_viscahomepresetlistname   , AUTHORITY_VIEWER   , NULL },
	{ "viscasequencelistname" , para_viscasequencelistname   , AUTHORITY_VIEWER   , NULL },
	{ "wbmodename"     		, para_wbmodename        , AUTHORITY_VIEWER   , NULL },
	{ "fliptypename"     	, para_fliptypename      , AUTHORITY_VIEWER   , NULL },
	{ "aemodename"     	    , para_aemodename        , AUTHORITY_VIEWER   , NULL },
	{ "aeshuttername"     	, para_aeshuttername     , AUTHORITY_VIEWER   , NULL },
	{ "aemanualname"     	, para_aemanualname      , AUTHORITY_VIEWER   , NULL },
	{ "privacycolorname"    , para_privacycolorname  , AUTHORITY_VIEWER   , NULL },
	{ "homefunctionname"    , para_homefunctionname  , AUTHORITY_VIEWER   , NULL },
	{ "panspeedname"    	, para_panspeedname  	 , AUTHORITY_VIEWER   , NULL },
	{ "tiltspeedname"    	, para_tiltspeedname  	 , AUTHORITY_VIEWER   , NULL },
	{ "presetpageinq"       , para_presetpageinq  	 , AUTHORITY_VIEWER   , NULL },
	{ "sequencepageinq"     , para_sequencepageinq   , AUTHORITY_VIEWER   , NULL },
	{ "panspeed"     		, para_panspeed   		 , AUTHORITY_VIEWER   , NULL },
	{ "tiltspeed"     		, para_tiltspeed   		 , AUTHORITY_VIEWER   , NULL },

	{ "aperturename"     	, para_aperturename   	 , AUTHORITY_VIEWER   , NULL },
	{ "angleminname"     	, para_angleminname   	 , AUTHORITY_VIEWER   , NULL },
	{ "anglemaxname"     	, para_anglemaxname   	 , AUTHORITY_VIEWER   , NULL },
	{ "anglemaxflipname"    , para_anglemaxflipname  , AUTHORITY_VIEWER   , NULL },
	{ "autopanname"    		, para_autopanname  	 , AUTHORITY_VIEWER   , NULL },
	{ "autopandirecname"    , para_autopandirecname  , AUTHORITY_VIEWER   , NULL },
	{ "autopanspeedname"    , para_autopanspeedname  , AUTHORITY_VIEWER   , NULL },
	{ "cruisename"    		, para_cruisename  	 	 , AUTHORITY_VIEWER   , NULL },
	
	{ "viscaburnstatus"     , para_visca_burn_status , AUTHORITY_NONE     , NULL },
	{ "viscadometype"		, para_visca_dometype  	 , AUTHORITY_NONE     , NULL },
	{ "viscamotortype"		, para_visca_motortype   , AUTHORITY_NONE     , NULL },
	{ "viscaversion"		, para_visca_version   	 , AUTHORITY_NONE  	  , NULL },
	{ "viscamodel"			, para_visca_model   	 , AUTHORITY_NONE     , NULL },
	{ "viscawintimezone"	, para_visca_wintimezone , AUTHORITY_NONE 	  , NULL },
	{ "viscamacaddress"     , para_visca_netmac      , AUTHORITY_NONE 	  , NULL },
	{ "viscaserialnumber"   , para_visca_serial_number, AUTHORITY_NONE 	  , NULL },
	
#endif
	{ "supportqos"          , para_support_QoS             , AUTHORITY_NONE           , NULL },
	{ "supportcos"          , para_support_CoS             , AUTHORITY_NONE           , NULL },
	{ "supportbwc"          , para_support_BWC             , AUTHORITY_NONE           , NULL },
#if defined( SUPPORT_COS_SCHEDULE )
	{ "cosenable"           , para_CoS_enable              , AUTHORITY_VIEWER         , NULL },
	{ "cosvlanid"           , para_VLan_ID                 , AUTHORITY_VIEWER         , NULL },
	{ "cospriovideo"        , para_CoS_prio_video          , AUTHORITY_VIEWER         , NULL },
	{ "cosprioaudio"        , para_CoS_prio_audio          , AUTHORITY_VIEWER         , NULL },
	{ "cosprioevent"        , para_CoS_prio_event          , AUTHORITY_VIEWER         , NULL },
	{ "cospriomanage"       , para_CoS_prio_manage         , AUTHORITY_VIEWER         , NULL },
#endif
#if defined( SUPPORT_QOS_SCHEDULE )
	{ "qosenable"           , para_QoS_enable              , AUTHORITY_VIEWER         , NULL },
	{ "qosdscpvideo"        , para_QoS_DSCP_video          , AUTHORITY_VIEWER         , NULL },
	{ "qosdscpaudio"        , para_QoS_DSCP_audio          , AUTHORITY_VIEWER         , NULL },
	{ "qosdscpevent"        , para_QoS_DSCP_event          , AUTHORITY_VIEWER         , NULL },
	{ "qosdscpmanage"       , para_QoS_DSCP_manage         , AUTHORITY_VIEWER         , NULL },
#endif

	//factory
	{ "supportfactorymode" , para_supportfactorymode   , AUTHORITY_OPERATOR   , NULL },

};
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
unsigned int arg_hash_cal_value(char *name)
{
	unsigned int value = 0;

	while (*name)
		value = value * 37 + (unsigned int)(*name++);
	return value;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void arg_hash_insert_entry(ARG_HASH_TABLE *table, HTML_ARGUMENT *arg)
{
	if (table->harg) {
		arg->next = table->harg;
	}
	table->harg = arg;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int arg_hash_table_init(void)
{
	int i;

	if ( (arg_hash = (ARG_HASH_TABLE *)calloc(sizeof(ARG_HASH_TABLE), MAX_ARG_HASH_SIZE)) == NULL) {
		return -1;
	}
	for (i=0; i<HASH_TABLE_SIZE; i++) {
		arg_hash_insert_entry(arg_hash+(arg_hash_cal_value(HttpArgument[i].name)%MAX_ARG_HASH_SIZE), HttpArgument+i);
	}
	return 0;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int TranslateWebPara(request *req, char *target, char *para, char *subpara)
{
	HTML_ARGUMENT *htmp;
	htmp = arg_hash[arg_hash_cal_value(para)%MAX_ARG_HASH_SIZE].harg;

	while (htmp) {
		if ( strcasecmp(htmp->name, para) == 0 ) {
			if (req->authority > htmp->authority) {
				dbg("[%s.%s] permission denied!!!\n", para, subpara);
				return -1;
			}
//			dbg("translate [%s.%s]\n", para, subpara);
			return (*htmp->handler) (req, target, subpara);
		}
		htmp = htmp->next;
	}
//	dbg("[%s.%s] not found\n", para, subpara);
	return -1;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/

void arg_hash_table_cleanup()
{
	free(arg_hash);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int ShowAllWebValue(request *req, char *data, int max_size)
{
	HTML_ARGUMENT *htmp;
	int i, total_size = 0, size;
	char buf[1024];
	for(i = 0;i < HASH_TABLE_SIZE; i++){
		htmp = &HttpArgument[i];
		if (req->authority > htmp->authority)
			continue;
		if(htmp ->handler == NULL)
			continue;
		size = sprintf(buf, "%s=", htmp->name);
		if(total_size + size + 1 > max_size){
			total_size = sprintf(data, "Not enogh size to show");
			break;
		}
		total_size += sprintf(data+total_size, "%s", buf);
		size = (*htmp->handler) (req, buf, "");
		if(size < 0){
			size = sprintf(buf, "Error return");
		}
		if(total_size + size + 1 > max_size){
			total_size = sprintf(data, "Not enogh size to show");
			break;
		}
		total_size += sprintf(data+total_size, "%s\n", buf);
	}
	return total_size;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/

static int ShowPara(char* buf, int index, char* name, AUTHORITY authority)
{
	char strAuthority[5][20] = {"ADMINISTRATOR","OPERATOR","VIEWER","NONE","ERROR"};
	int a_index;
	switch(authority){
		case AUTHORITY_ADMIN:
			a_index = 0;
			break;
		case AUTHORITY_OPERATOR:
			a_index = 1;
			break;
		case AUTHORITY_VIEWER:
			a_index = 2;
			break;
		case AUTHORITY_NONE:
			a_index = 3;
			break;
		default:
			a_index = 4;
			break;
	}
	return sprintf(buf, "%3d.%-25s%s\n", index, name, strAuthority[a_index]);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int ShowAllPara(request *req, char *data, int max_size)
{
	HTML_ARGUMENT *htmp;
	int i, total_size = 0, size,count = 0;
	char buf[80];
	for(i = 0;i < HASH_TABLE_SIZE; i++){
		htmp = &HttpArgument[i];
		if(htmp ->handler == NULL)
			continue;
		size = ShowPara(buf, ++count, htmp->name, htmp->authority);
		if(total_size + size + 1 > max_size){
			total_size = sprintf(data, "Not enogh size (%d bytes) to show", total_size + size + 1);
			break;
		}
		total_size += sprintf(data+total_size, "%s", buf);
	}
	return total_size;
}



