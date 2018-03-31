#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <asm/types.h>
#include <time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <stddef.h>
#include <linux/rtc.h> 
#include <sys/mount.h>  // for umount2

//#define LOCAL_DEBUG

#include <debug.h>
#include <utility.h>
#include <sysctrl.h>
#include <syslogno.h>
#include <net_config.h>
#include <file_msg_drv.h>
#include <config_range.h>
#include <sysctrl_struct.h>
#include <system_default.h>
#include <bios_data.h>
#include <sysinfo.h>
#include <signal.h>
#include <ipnc_define.h>
#include <storage.h>
#include <parser_name.h>
#include <pathname.h>
#include <gio_util.h>
#include <syslogapi.h>
#include <sys_log.h>
#include <ApproDrvMsg.h>
#include <libvisca.h>


#include "boa.h"
#include "appro_api.h"
#include "file_list.h"
#include "ioport.h"
#include "rs485.h"
#include "verify.h"	// nedwu
#include "sensor_lib.h"

#include <unistd.h>
#include <fcntl.h>
#include <asm/arch/lenctrl_ioctl.h>
//#include <config_save.h>

#define MANUAL_HEATER_ON        0x02
#define MANUAL_HEATER_HIGH      0x03
#define MANUAL_FAN              0x04

unsigned int event_location = 0;
#ifdef CO_PROCESSOR
static int fan_status = 0;
static int heater_status = 0;
#endif
unsigned char lower_ascii[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

typedef struct __API_BOTTOM_HALF
{
	unsigned int ntpd_restart : 1;
	
} API_BOTTOM_HALF;

union {
	API_BOTTOM_HALF bottom;
	__u32 bottom_test;
} api;

extern int TranslateWebPara(request *req, char *target, char *para, char *subpara);

void strtolower(unsigned char *str)
{
	while (*str) {
		*str = lower_ascii[*str];
		str++;
	}
}

typedef union __NET_IPV4 {
	__u32	int32;
	__u8	str[4];
} NET_IPV4;

static inline char *skip_token(const char *p)  //add by mikewang
{
	while (isspace(*p)) p++;
	while (*p && !isspace(*p)) p++;
	return (char *)p;
}

char *ip_insert_dot(char *ip){  //add by mikewang
	char *dotAddress , tmp_addr[4];
	unsigned int tmp[4];
	
	dotAddress = malloc(40);
	sprintf(tmp_addr,"%c%c%c",ip[0],ip[1],ip[2]);
	tmp[0] = atoi(tmp_addr);
	
	sprintf(tmp_addr,"%c%c%c",ip[3],ip[4],ip[5]);
	tmp[1] = atoi(tmp_addr);
	
	sprintf(tmp_addr,"%c%c%c",ip[6],ip[7],ip[8]);
	tmp[2] = atoi(tmp_addr);
	
	sprintf(tmp_addr,"%c%c%c",ip[9],ip[10],ip[11]);
	tmp[3] = atoi(tmp_addr);
	
	sprintf(dotAddress ,"%d.%d.%d.%d",tmp[0],tmp[1],tmp[2],tmp[3]);
	return dotAddress;
}

void ipfilter_parser(char src[64],char result_ip[2][4][10]){  //result_ip[0]:start ip , result_ip[1]:stop ip
	int x , y = 0 , ipsection_is_range = 0 , is_range_1 = 1;
	int index0 = 0 , index1 = 0;
dbg("src = %s\n",src);
	for(x = 0 ; x < 4 ; x++){
//dbg("x = %d\n",x);
		while(src[y]){
//dbg("in while loop y = %d,",y);
			if(src[y] > 47 && src[y] < 58){   //is digit
//dbg("is digit src[%d] = %c , ipsection_is_range = %d , is_range_1 = %d\n",y,src[y],ipsection_is_range,is_range_1);
				if(ipsection_is_range){
					if(is_range_1){
						result_ip[0][x][index0++] = src[y++];
					}else{  //is range 2
						result_ip[1][x][index1++] = src[y++];
					}
				}else{
					result_ip[0][x][index0++] = result_ip[1][x][index1++] = src[y++];
				}
			}else if(src[y] == '*'){
//dbg("is '*' src[%d] = %c\n",y,src[y]);
				strcpy(result_ip[0][x],"0");
				strcpy(result_ip[1][x],"255");
				index0 = index1 = 0;
				ipsection_is_range = 0;
				is_range_1 = 1;
				y += 2;
				break;
			}else if(src[y] == '(' || src[y] == ')'){
//dbg("is '(' ')' src[%d] = %c\n",y,src[y]);
				if(!src[y+1]){
					result_ip[0][3][index0] = '\0';
					result_ip[1][3][index1] = '\0';
				}
				ipsection_is_range = 1;
				y++;
			}else if(src[y] == '-'){
//dbg("is '-' src[%d] = %c\n",y,src[y]);
				result_ip[0][x][index0] = '\0';
				is_range_1 = 0;
				y++;
			}
			else if(src[y] == '.'){
//dbg("is '.' src[%d] = %c\n",y,src[y]);
//dbg("next char = %c\n",src[y+1]);
				if(ipsection_is_range){
					if(is_range_1){
						result_ip[0][x][index0] = '\0';
//dbg("ip1 = %s\n",result_ip[0][x]);
					}
					else{  //is range 2
						result_ip[1][x][index1] = '\0';
//dbg("ip2 = %s\n",result_ip[1][x]);
					}
				}else
					result_ip[0][x][index0] = result_ip[1][x][index1] = '\0';
				index0 = index1 = 0;
				y++;
				ipsection_is_range = 0;
				is_range_1 = 1;
				break;
			}
		}
	}
}

void cmd_array_parser( const char *src , char cmd[8][32] ){  //add by mikewang
  int index_src = 0 ,index_cmd = 0 , count = 0;

  while(index_src < strlen(src) ){
    if( isspace(src[index_src]) ){
      index_src++;
      continue;
    }
    if( src[index_src] == ':' ){
      cmd[index_cmd][count] = '\0';
      index_src++;  index_cmd++;
      count = 0;
      continue;
    }
    cmd[index_cmd][count++] = src[index_src++];
  }
/*  fprintf(stderr,"string 1 = %s\n",cmd[0]);
  fprintf(stderr,"string 1 = %s\n",cmd[1]);
  fprintf(stderr,"string 1 = %s\n",cmd[2]);
  fprintf(stderr,"string 1 = %s\n",cmd[3]);
  fprintf(stderr,"string 1 = %s\n",cmd[4]);
*/
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int numeric_check(const char * data)
{
    char * ptr = (char*)data;

    if(!ptr)
        return -1;

    for( ; *ptr != '\0' ; ptr++) {
        if( !isdigit(*ptr) )
            return -1;
    }

    return 0;
}
int profile_check(int profileid)
{
	/* 
	Range : 1 ~ MAX_WEB_PROFILE_NUM
	
	return -1 : Profile is no exist.
	return 0 : Special profile. (ONLY READ)
	return 1 : Profile is exist.
	*/

	if(profileid < 0){
		DBG("profileid error");
		return -1;
	}
	
#ifdef SUPPORT_PROFILE_NUMBER
	if(profileid == sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value)
		return SPECIAL_PROFILE;
	
	if(profileid > sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value){
		//DBG("profileid(%d) over range(max:%d)", profileid, sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value);
		return -1;
	}
#else
	if(profileid == MAX_PROFILE_NUM)
		return SPECIAL_PROFILE;

	if(profileid > MAX_PROFILE_NUM){
		DBG("profileid(%d) over range(max:%d)", profileid, MAX_PROFILE_NUM);
		return -1;
	}
#endif

	return EXIST_PROFILE;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
static int netsplit( char *pAddress, void *ip )
{
	unsigned int ret;
	NET_IPV4 *ipaddr = (NET_IPV4 *)ip;

	if ((ret = atoi(pAddress + 9)) > 255)
		return FALSE;
	ipaddr->str[3] = ret;

	*( pAddress + 9 ) = '\x0';
	if ((ret = atoi(pAddress + 6)) > 255)
		return FALSE;
	ipaddr->str[2] = ret;

	*( pAddress + 6 ) = '\x0';
	if ((ret = atoi(pAddress + 3)) > 255)
		return FALSE;
	ipaddr->str[1] = ret;

	*( pAddress + 3 ) = '\x0';
	if ((ret = atoi(pAddress + 0)) > 255)
		return FALSE;
	ipaddr->str[0] = ret;

	return TRUE;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int ipv4_str_to_num(char *data, struct in_addr *ipaddr)
{
	if ( strchr(data, '.') == NULL )
		return netsplit(data, ipaddr);
	return inet_aton(data, ipaddr);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#define HEX_TO_DECIMAL(char1, char2)	\
    (((char1 >= 'A') ? (((char1 & 0xdf) - 'A') + 10) : (char1 - '0')) * 16) + \
    (((char2 >= 'A') ? (((char2 & 0xdf) - 'A') + 10) : (char2 - '0')))
int hex_to_mac(char *mac, char *hex)
{
	int i;

	for (i=0; i<6; i++) {
		if (!isxdigit(hex[0]) || !isxdigit(hex[1]))
			return -1;
		mac[i] = HEX_TO_DECIMAL(hex[0], hex[1]);
		hex += 2;
	}
	return 0;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
char *req_bufptr(request * req)
{
	return (req->buffer + req->buffer_end);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_dhcpclient(request *req, COMMAND_ARGUMENT *argm)
{
	int value = 0;

	do {
		if (ControlSystemData(SFIELD_GET_DHCPC_ENABLE, (void *)&value, sizeof(value), NULL) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_dhcpclient(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NET_DHCP_ENABLE].config.value);
        return;
    }

	do {
//		if ( value > 1 )
//			break;

		#ifndef DHCP_CONFIG
		if(sys_info->ipcam[DHCPDIPSWITCH].config.value == 1)
			break;
		#endif

        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);

#if 0
		if(ControlSystemData(SFIELD_GET_DHCP_CONFIG, (void *)&sys_dhcp, sizeof(sys_dhcp)) < 0)
			break;
		if(sys_dhcp == 0)
			break;
		if (ControlSystemData(SFIELD_GET_DHCPC_ENABLE, (void *)&sys_dhcp, sizeof(sys_dhcp)) < 0)
			break;

		if (sys_dhcp != value) {
			if (value == TRUE)
				net_enable_dhcpcd(sys_info->ifname);  //kevinlee eth0 or ppp0
			else
				net_disable_dhcpcd(sys_info->ifname); //kevinlee eth0 or ppp0

			ControlSystemData(SFIELD_SET_DHCPC_ENABLE, (void *)&value, sizeof(value));
		}
#else
        if ( value < sys_info->ipcam[NET_DHCP_ENABLE].min || value > sys_info->ipcam[NET_DHCP_ENABLE].max )
            break;

		//nelson
		#if 0	//ryan 20110616
		if (sys_info->ipcam[PPPOEENABLE].config.u16 == 1) {
			dbg("config.lan_config.net.pppoeenable == 1\n");
			break;
		}
		#endif
		//if(sys_info->config.lan_config.net.dhcp_config == 0)
		//	break;

		//if(sys_info->config.lan_config.net.dhcp_enable != value)
		if(sys_info->ipcam[NET_DHCP_ENABLE].config.value != value)
			if(ControlSystemData(SFIELD_SET_DHCPC_ENABLE, (void *)&value, sizeof(value), &req->curr_user) < 0)
				break;
#endif
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
void set_dhcpdipswitch(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DHCPDIPSWITCH].config.value);
		return;
	}
	
	do{
		 if(numeric_check(argm->value) < 0){
		 	dbg("error");
            break;
		 	}
        int value = atoi(argm->value);
		if ( value < sys_info->ipcam[DHCPDIPSWITCH].min || value > sys_info->ipcam[DHCPDIPSWITCH].max ){
			dbg("range error");
            break;
		}
		if(sys_info->ipcam[DHCPDIPSWITCH].config.value != value)
			if(ControlSystemData(SFIELD_SET_DHCPDIPSWITCH, (void *)&value, sizeof(value), &req->curr_user) < 0){
				dbg("SFIELD_SET_DHCPDIPSWITCH error");
				break;
			}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_netip(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ip;

	do {
		if ( (ip.s_addr = net_get_ifaddr(sys_info->ifname)) == -1)  //kevinlee eth0 or ppp0
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(ip));
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_netip(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ipaddr, sys_ip, old_mask;

    if (!argm->value[0]) {
        sys_ip.s_addr = net_get_ifaddr(sys_info->ifname);
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(sys_ip));
        return;
    }

	do {
		// add by Alex, 2009.03.26
		#if 0	//ryan 20110616
		if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
		//if(sys_info->config.lan_config.net.pppoeactive == 1 && sys_info->config.lan_config.net.pppoeenable == 1)
			break;
		#endif
		
		if (ipv4_str_to_num(argm->value, &ipaddr) == 0)
			break;

		// user can change eth0 ip only
		// modified by Alex, 2009.03.25
		if ( (sys_ip.s_addr = net_get_ifaddr(sys_info->ifether)) == -1)
			break;

		// keep the old netmask
		old_mask.s_addr = net_get_netmask(sys_info->ifether);

		//if ((sys_ip.s_addr != ipaddr.s_addr) || (ipaddr.s_addr != sys_info->config.lan_config.net.ip.s_addr)) {
		if ((sys_ip.s_addr != ipaddr.s_addr) || (ipaddr.s_addr != sys_info->ipcam[IP].config.ip.s_addr)) {
			if (net_set_ifaddr(sys_info->ifether, ipaddr.s_addr) < 0)
				break;

			net_set_netmask(sys_info->ifether, old_mask.s_addr);

			ControlSystemData(SFIELD_SET_IP, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_netmask(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ip;

	do {
		if ( (ip.s_addr = net_get_netmask(sys_info->ifname)) == -1)  //kevinlee eth0 or ppp0
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(ip));
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_netmask(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(sys_info->ipcam[NETMASK].config.ip));
        return;
    }

	struct in_addr ipaddr, sys_ip;

	do {
		// add by Alex, 2009.03.26
		#if 0	//ryan 20110616
		if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
		//if(sys_info->config.lan_config.net.pppoeactive == 1 && sys_info->config.lan_config.net.pppoeenable == 1)
			break;
		#endif
		if (ipv4_str_to_num(argm->value, &ipaddr) == 0)
			break;

		if ( (sys_ip.s_addr = net_get_netmask(sys_info->ifether)) == -1)  //kevinlee eth0 or ppp0
			break;

		if ( (sys_ip.s_addr != ipaddr.s_addr) || (ipaddr.s_addr != sys_info->ipcam[NETMASK].config.ip.s_addr)) {
		//if ( (sys_ip.s_addr != ipaddr.s_addr) || (ipaddr.s_addr != sys_info->config.lan_config.net.netmask.s_addr)) {
			if (net_set_netmask(sys_info->ifether, ipaddr.s_addr) < 0) //kevinlee eth0 or ppp0
				break;

			ControlSystemData(SFIELD_SET_NETMASK, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

#if 0
void set_aviduration(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = atoi(argm->value);

	/*
	SysInfo* pSysInfo = GetSysInfo();
	if (pSysInfo->config.sdcard_config.sdinsert==1)
		return;
	*/ // Disable by Alex, 2008.09.24
	
	do {
		if(sys_info->ipcam[AVIDURATION].config.u8 != value)
		//if(sys_info->config.lan_config.aviduration != value)
			ControlSystemData(SFIELD_SET_AVIDURATION, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_aviprealarm(request *req, COMMAND_ARGUMENT *argm)
{
    __u32 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[NAVIPREALARM].config.u32 != value)
		//if(sys_info->config.lan_config.naviprealarm != value)
			ControlSystemData(SFIELD_SET_AVIPREALARM, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_avipostalarm(request *req, COMMAND_ARGUMENT *argm)
{
    __u32 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[NAVIPOSTALARM].config.u32 != value)
		//if(sys_info->config.lan_config.navipostalarm != value)
			info_write(&value, NAVIPOSTALARM, 0, 0);
			ControlSystemData(SFIELD_SET_AVIPREALARM, (void *)&value, sizeof(value), &req->curr_user);
			//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.navipostalarm), sizeof(value),0);
			//ControlSystemData(SFIELD_SET_AVIPREALARM, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_aviformat(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[AVIFORMAT].config.u8 != value)
		//if(sys_info->config.lan_config.aviformat != value)
			ControlSystemData(SFIELD_SET_AVIFORMAT, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
    return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_ftpfileformat(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = atoi(argm->value);

	do {
		if(sys_info->config.ftp_config.ftpfileformat != value)
			ControlSystemData(SFIELD_SET_FTPFILEFORMAT, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
    return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/***************************************************************************
 *									   *
 ***************************************************************************/
void set_sdcount(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *									   *
 ***************************************************************************/
void get_sdcount(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

#if 0
/***************************************************************************
 *									   *
 ***************************************************************************/
void set_sdduration(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	__u16 value = atoi(argm->value);
	do {
		
		if(sys_info->ipcam[SDCARD_DURATION].config.u16 != value)
		//if(sys_info->config.sdcard.duration != value)
			ControlSystemData(SFIELD_SET_SD_DURATION, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *									   *
 ***************************************************************************/
void get_sdduration(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[SDCARD_DURATION].config.u16);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.sdcard.duration);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_sdrecordtype(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	__u8 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[SDCARD_FILEFORMAT].config.u8 != value)
		//if(sys_info->config.sdcard.fileformat != value)
			ControlSystemData(SFIELD_SET_SDFILEFORMAT, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_sdrecordtype(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[SDCARD_FILEFORMAT].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.sdcard.fileformat);
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_sdaenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	__u8 bEnable = atoi(argm->value);
	do {
		if(sys_info->ipcam[BSDAENABLE].config.u8 != bEnable)
		//if(sys_info->config.lan_config.bSdaEnable != bEnable)
			if(ControlSystemData(SFIELD_SET_SDAENABLE, &bEnable, sizeof(bEnable), &req->curr_user) < 0)
				break;
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_sdaenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[BSDAENABLE].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.bSdaEnable);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_sdrenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	__u8 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[SDCARD_ENABLE_REC].config.u8 != value)
		//if(sys_info->config.sdcard.enable_rec != value)
			ControlSystemData(SFIELD_SET_SD_SDRENABLE, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_sdrenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[SDCARD_ENABLE_REC].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.sdcard.enable_rec);
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void sdformat(request *req, COMMAND_ARGUMENT *argm)
{
#if 0  // Disabled by Alex. 2009.12.16, please use '/cgi-bin/format.cgi?sd'
	int value = atoi(argm->value);
	do {
		if (ControlSystemData(SFIELD_SD_FORMAT, &value, sizeof(value), NULL) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_cardrewrite(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	__u8 value = atoi(argm->value);
	do {

		if(sys_info->ipcam[SDCARD_REWRITE].config.u8 != value)
		//if(sys_info->config.sdcard.rewrite!= value)
			if (ControlSystemData(SFIELD_SD_REWRITE, &value, sizeof(value), NULL) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_jpegcontfreq(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	__u8 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[SDCARD_JPG_RECRATE].config.u8!= value)
		//if(sys_info->config.sdcard.jpg_recrate!= value)
			if (ControlSystemData(SFIELD_SET_JPEGCONTFREQ, &value, sizeof(value), &req->curr_user) < 0)
				break;
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
#endif

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_sdinsert(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , (sys_info->status.mmc & SD_INSERTED)? 1 : 0 );
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_sdfpp(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[SDCARD_SDFPP].config.u8);
        return;
    }
    
    __u8 value = atoi(argm->value);
    do {
		
        info_write(&value, SDCARD_SDFPP, 0, 0);     
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
void set_sddel(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD

	//dbg("req->cmd_arg[1].name=%s\n", req->cmd_arg[1].name);
	//dbg("req->cmd_arg[1].value=%s\n", req->cmd_arg[1].value);
	//dbg("argm->value=%s\n", argm->value);
  
	char indir[256] = "";
	strncat(indir, SD_MOUNT_PATH, sizeof(indir) - 1);
	strncat(indir, argm->value, sizeof(indir) - 1);
	//fprintf(stderr, "rootdir=%s\n", rootdir);
	if(CheckDlinkFileExist(indir) == 0){
		//dbg("delfile is exit\n");
		DeleteDlinkFile(indir);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	}else{
		dbg("delfile is not exit\n");
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
	}
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
#if 0
void set_attfileformat(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = atoi(argm->value);

	do {
		if(sys_info->config.smtp_config.attfileformat != value)
			ControlSystemData(SFIELD_SET_ATTFILEFORMAT, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
    return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void get_audioenable(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AUDIOINENABLE].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.audioinenable);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_audioenable(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		__u8 value = atoi(argm->value);

		//sysconf_lock();
        conf_lock();
#ifdef CONFIG_BRAND_DLINK
		sys_info->ipcam[AUDIOINENABLE].config.u8 = !value;
		//sys_info->config.lan_config.audioinenable = !value;
#else
		sys_info->ipcam[AUDIOINENABLE].config.u8 = value;
		//sys_info->config.lan_config.audioinenable = value;
#endif
		sys_info->osd_setup = 1;
        conf_unlock();
		info_set_flag(CONF_FLAG_CHANGE);
		//sysconf_unlock();
		//sysinfo_set_flag(SYSCONF_FLAG_CHANGE);

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0
void set_asmtpattach(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = atoi(argm->value);

	do {
		if(sys_info->config.smtp_config.attachments != value)
			ControlSystemData(SFIELD_SET_SMTP_ATTACHMENTS, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
    return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_rftpenable(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = atoi(argm->value);

	do {
		if(sys_info->config.ftp_config.rftpenable != value)
			ControlSystemData(SFIELD_SET_FTP_RFTPENABLE, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
    return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
void get_dnsip(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ip;

	ip.s_addr = net_get_dns();
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(ip));
}

void set_dnsip(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ipaddr, sys_ip;

	do {
		// add by Alex, 2009.03.26
		if(sys_info->config.lan_config.net.pppoeactive == 1 && sys_info->config.lan_config.net.pppoeenable == 1)
			break;
		
		if (ipv4_str_to_num(argm->value, &ipaddr) == 0)
			break;
		
		sys_ip.s_addr = net_get_dns();

		if (sys_ip.s_addr != ipaddr.s_addr) {
			if (net_set_dns(inet_ntoa(ipaddr)) < 0)
				break;

			ControlSystemData(SFIELD_SET_DNS, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
#endif
void get_dnsip(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ipaddr[MAX_DNS_NUM];

	memset(ipaddr, 0, sizeof(ipaddr));
	net_get_multi_dns(ipaddr);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(ipaddr[0]));
}

void set_dnsip(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(sys_info->ipcam[DNS].config.ip));
        return;
    }

	struct in_addr ipaddr[MAX_DNS_NUM], ip;

	do {
		// add by Alex, 2009.03.26
		#if 0	//ryan 20110616
		if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
		//if(sys_info->config.lan_config.net.pppoeactive == 1 && sys_info->config.lan_config.net.pppoeenable == 1)
			break;
		#endif
		if (ipv4_str_to_num(argm->value, &ip) == 0)
			break;
		
		ipaddr[0].s_addr = sys_info->ipcam[DNS].config.ip.s_addr;
		ipaddr[1].s_addr = sys_info->ipcam[DNS2].config.ip.s_addr;
		//ipaddr[0].s_addr = sys_info->config.lan_config.net.dns.s_addr;
		//ipaddr[1].s_addr = sys_info->config.lan_config.net.dns2.s_addr;

		if (ip.s_addr != ipaddr[0].s_addr) {
			ipaddr[0].s_addr = ip.s_addr;
			if (net_set_multi_dns(ipaddr) < 0)
				break;

			//sysconf_lock();
                         conf_lock();
			sys_info->ipcam[DNS].config.ip.s_addr = ipaddr[0].s_addr;
			sys_info->ipcam[DNS2].config.ip.s_addr = ipaddr[1].s_addr;
			//sys_info->config.lan_config.net.dns.s_addr = ipaddr[0].s_addr;
			//sys_info->config.lan_config.net.dns2.s_addr = ipaddr[1].s_addr;
			sys_info->osd_setup = 1;
                        conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			//sysconf_unlock();
			//sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void dns2(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ipaddr[MAX_DNS_NUM], ip;

	if (!argm->value[0]) {
		memset(ipaddr, 0, sizeof(ipaddr));
		net_get_multi_dns(ipaddr);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(ipaddr[1]));
		return;
	}

	do {
		// add by Alex, 2009.03.26
		#if 0	//ryan 20110616
		if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
		//if(sys_info->config.lan_config.net.pppoeactive == 1 && sys_info->config.lan_config.net.pppoeenable == 1)
			break;
		#endif
		if (ipv4_str_to_num(argm->value, &ip) == 0)
			break;
		
//		sys_ip.s_addr = net_get_dns();
//		net_get_multi_dns(ipaddr);
		ipaddr[0].s_addr = sys_info->ipcam[DNS].config.ip.s_addr;
		ipaddr[1].s_addr = sys_info->ipcam[DNS2].config.ip.s_addr;

		//ipaddr[0].s_addr = sys_info->config.lan_config.net.dns.s_addr;
		//ipaddr[1].s_addr = sys_info->config.lan_config.net.dns2.s_addr;

		if (ip.s_addr != ipaddr[1].s_addr) {
			ipaddr[1].s_addr = ip.s_addr;
			if (net_set_multi_dns(ipaddr) < 0)
				break;

                        conf_lock();
			//sysconf_lock();
                        sys_info->ipcam[DNS].config.ip.s_addr = ipaddr[0].s_addr;
			sys_info->ipcam[DNS2].config.ip.s_addr = ipaddr[1].s_addr;
			//sys_info->config.lan_config.net.dns.s_addr = ipaddr[0].s_addr;
			//sys_info->config.lan_config.net.dns2.s_addr = ipaddr[1].s_addr;
			sys_info->osd_setup = 1;
	                conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			//sysconf_unlock();
			//sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*void get_dnsip_ext(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ip;

	ip.s_addr = net_get_dns();
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(ip));
}

void set_dnsip_ext(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ipaddr[2], sys_ip;

	do {
		char *second_dns;
		// add by Alex, 2009.03.26
		if(sys_info->config.lan_config.net.pppoeactive == 1 && sys_info->config.lan_config.net.pppoeenable == 1)
			break;
		
		if ( (next_dns = strchr(argm->value, ':')) == NULL ) {
			if (ipv4_str_to_num(argm->value, &ipaddr[0]) == 0)
				break;
			ipaddr[1].s_addr = 0;
		}
		else {
			char *temp_dns;
			*second_dns++ = 0;
			if ( (temp_dns = strchr(second_dns, ':')) != NULL )
		}
		



		if (ipv4_str_to_num(argm->value, &ipaddr) == 0)
			break;
		
		sys_ip.s_addr = net_get_dns();

		if (sys_ip.s_addr != ipaddr.s_addr) {
			if (net_set_dns(inet_ntoa(ipaddr)) < 0)
				break;

			ControlSystemData(SFIELD_SET_DNS, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_gateway(request *req, COMMAND_ARGUMENT *argm)
{
	struct in_addr ip;

	ip.s_addr = net_get_gateway(sys_info->ifname);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(ip));
}

void set_gateway(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, inet_ntoa(sys_info->ipcam[GATEWAY].config.ip));
        return;
    }

	struct in_addr ipaddr, sys_ip;

	do {
		// add by Alex, 2009.03.26
		#if 0	//ryan 20110616
		if(sys_info->ipcam[PPPOEENABLE].config.u8 == 1)
			break;
		#endif
		if (ipv4_str_to_num(argm->value, &ipaddr) == 0)
			break;

		sys_ip.s_addr = net_get_gateway(sys_info->ifether);

		if ((sys_ip.s_addr != ipaddr.s_addr) || (ipaddr.s_addr != sys_info->ipcam[GATEWAY].config.ip.s_addr)) {
			net_set_gateway(ipaddr.s_addr);
			ControlSystemData(SFIELD_SET_GATEWAY, (void *)&ipaddr.s_addr, sizeof(ipaddr.s_addr), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_maxmtu(request *req, COMMAND_ARGUMENT *argm)
{
	int value = atoi(argm->value);
	
	do {
		if(value < 68 || value >1500)
			break;
		if(ControlSystemData(SFIELD_SET_MAXMTU, (void *)&value, sizeof(value), &req->curr_user) == -1)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_maxmtu(request *req, COMMAND_ARGUMENT *argm)
{
	int ret;
	int value;
	do {
		if((ret = ControlSystemData(SFIELD_GET_MAXMTU, (void *)&value, sizeof(value), &req->curr_user)) == -1)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_httpport(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned short value;

	do {
		if (ControlSystemData(SFIELD_GET_HTTPPORT, (void *)&value, sizeof(value), NULL) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_etherstatus(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, 1);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_nicerror(request *req, COMMAND_ARGUMENT *argm)
{
	FILE *fh;
	char *buf;
	char unit[8] = "bytes";
	unsigned long int bytes;
	do {
		buf = malloc(512);
		fh = fopen(_PATH_PROCNET_DEV, "r");
		if (!fh) {
			dbg("%s open error!\n",_PATH_PROCNET_DEV);
		}
		fgets(buf, 512, fh);	/* eat line */
    fgets(buf, 512, fh);
    fgets(buf, 512, fh);
    fgets(buf, 512, fh);
    fclose(fh);
    buf = skip_token(buf);
    buf = skip_token(buf);
    buf = skip_token(buf);
    bytes = strtoul(buf, &buf, 10 );
    if(bytes > 1048576){
    	bytes /= 1048576;	strcpy( unit , "Mbytes");
    }else if(bytes > 1024){
    	bytes /= 1024;	strcpy( unit , "Kbytes");
    }
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%lu %s\n", argm->name, bytes , unit);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_nicin(request *req, COMMAND_ARGUMENT *argm)
{
	FILE *fh;
	char *buf;
	char unit[8] = "bytes";
	unsigned long int bytes;
	do {
		buf = malloc(512);
		fh = fopen(_PATH_PROCNET_DEV, "r");
		if (!fh) {
			dbg("%s open error!\n",_PATH_PROCNET_DEV);
		}
		fgets(buf, 512, fh);	/* eat line */
    fgets(buf, 512, fh);
    fgets(buf, 512, fh);
    fgets(buf, 512, fh);
    fclose(fh);
    buf = skip_token(buf);
    bytes = strtoul(buf, &buf, 10 );
    if(bytes > 1048576){
    	bytes /= 1048576;	strcpy( unit , "Mbytes");
    }else if(bytes > 1024){
    	bytes /= 1024;	strcpy( unit , "Kbytes");
    }
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%lu %s\n", argm->name, bytes , unit);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_nicout(request *req, COMMAND_ARGUMENT *argm)
{
	FILE *fh;
	int x;
	char *buf;
	char unit[8] = "bytes";
	unsigned long int bytes;
	do {
		buf = malloc(512);
		fh = fopen(_PATH_PROCNET_DEV, "r");
		if (!fh) {
			dbg("%s open error!\n",_PATH_PROCNET_DEV);
		}
		fgets(buf, 512, fh);	/* eat line */
    fgets(buf, 512, fh);
    fgets(buf, 512, fh);
    fgets(buf, 512, fh);
    fclose(fh);
    for(x = 0;x < 9;x++)
    	buf = skip_token(buf);
    bytes = strtoul(buf, &buf, 10 );
    if(bytes > 1048576){
    	bytes /= 1048576;	strcpy( unit , "Mbytes");
    }else if(bytes > 1024){
    	bytes /= 1024;	strcpy( unit , "Kbytes");
    }
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%lu %s\n", argm->name, bytes , unit);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void speakstreamosddebug(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		sys_info->audioDebug = value;
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
/***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void rtspframeratedebug(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		sys_info->osd_debug.rtsp_framerate = value;
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void speakstreamtest(request *req, COMMAND_ARGUMENT *argm)
  {
  	do {
  		system("killall -9 audiotest");
			system("/opt/ipnc/audiotest speakstreamtest &");
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_speak(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_TWOWAY_AUDIO
	static int speakid = 1;

	do {
		if( sys_info->speaktokenid == 0 ){
			sys_info->speaktokenid = speakid++;
			sys_info->speaktokentime = 300;
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->speaktokenid > 0 ? sys_info->speaktokenid : -1 );
		return;
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s=\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_httpport(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[HTTP_PORT].config.u16);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        unsigned short value = atoi(argm->value);

        if(value < sys_info->ipcam[HTTP_PORT].min && value != HTTP_PORT_DEFAULT)
            break;

		if(value == sys_info->ipcam[RTSP_PORT].config.u16) 
            break;
		
#ifdef SERVER_SSL
		if(value == sys_info->ipcam[HTTPS_PORT].config.u16) 
            break;
#endif        
        if (sys_info->ipcam[HTTP_PORT].config.u16 != value) {
            ControlSystemData(SFIELD_SET_HTTPPORT, (void *)&value, sizeof(value), &req->curr_user);
            req->req_flag.restart = 1;
        }
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void http_stream_name_1(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(STREAM0_HTTPSTREAMNAME));
        return;
    }

	do {
		if (strlen(argm->value) > sys_info->ipcam[STREAM0_HTTPSTREAMNAME].max)
			break;
		
		if (strcmp(conf_string_read(STREAM0_HTTPSTREAMNAME), argm->value)) {
			conf_lock();
			strcpy(conf_string_read( STREAM0_HTTPSTREAMNAME), argm->value);
			sys_info->osd_setup = 1;
			conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			sprintf(http_stream[0], "/%s", conf_string_read( STREAM0_HTTPSTREAMNAME));
			uri_hash_table_release();
			uri_hash_table_init();
		}
/*
		if (strcmp(sys_info->config.lan_config.stream[0].httpstreamname, argm->value)) {
			sysconf_lock();
			strcpy(sys_info->config.lan_config.stream[0].httpstreamname, argm->value);
			sys_info->osd_setup = 1;
			sysconf_unlock();
			sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
			sprintf(http_stream[0], "/%s", sys_info->config.lan_config.stream[0].httpstreamname);
			uri_hash_table_release();
			uri_hash_table_init();
		}
*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void http_stream_name_2(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(STREAM1_HTTPSTREAMNAME));
        return;
    }

	do {
		if (strlen(argm->value) > sys_info->ipcam[STREAM1_HTTPSTREAMNAME].max)
			break;
		
		if (strcmp(conf_string_read( STREAM1_HTTPSTREAMNAME), argm->value)) {
					conf_lock();
					strcpy(conf_string_read( STREAM1_HTTPSTREAMNAME), argm->value);
					sys_info->osd_setup = 1;
					conf_unlock();
					info_set_flag(CONF_FLAG_CHANGE);
					sprintf(http_stream[1], "/%s", conf_string_read( STREAM1_HTTPSTREAMNAME));
					uri_hash_table_release();
					uri_hash_table_init();
		}
/*
		if (strcmp(sys_info->config.lan_config.stream[1].httpstreamname, argm->value)) {
			sysconf_lock();
			strcpy(sys_info->config.lan_config.stream[1].httpstreamname, argm->value);
			sys_info->osd_setup = 1;
			sysconf_unlock();
			sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
			sprintf(http_stream[1], "/%s", sys_info->config.lan_config.stream[1].httpstreamname);
			uri_hash_table_release();
			uri_hash_table_init();
		}
		*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void http_stream_name_3(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(STREAM2_HTTPSTREAMNAME));
        return;
    }

	do {
		if (strlen(argm->value) > sys_info->ipcam[STREAM2_HTTPSTREAMNAME].max)
			break;
		
		if (strcmp(conf_string_read( STREAM2_HTTPSTREAMNAME), argm->value)) {
					conf_lock();
					strcpy(conf_string_read( STREAM2_HTTPSTREAMNAME), argm->value);
					sys_info->osd_setup = 1;
					conf_unlock();
					info_set_flag(CONF_FLAG_CHANGE);
					sprintf(http_stream[2], "/%s", conf_string_read( STREAM2_HTTPSTREAMNAME));
					uri_hash_table_release();
					uri_hash_table_init();
		}
/*
		if (strcmp(sys_info->config.lan_config.stream[2].httpstreamname, argm->value)) {
			sysconf_lock();
			strcpy(sys_info->config.lan_config.stream[2].httpstreamname, argm->value);
			sys_info->osd_setup = 1;
			sysconf_unlock();
			sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
			sprintf(http_stream[2], "/%s", sys_info->config.lan_config.stream[2].httpstreamname);
			uri_hash_table_release();
			uri_hash_table_init();
		}
		*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void rtsp_stream_name_1(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(STREAM0_RTSPSTREAMNAME));
        return;
    }

	do {
		if (strlen(argm->value) > sys_info->ipcam[STREAM0_RTSPSTREAMNAME].max)
			break;
		if (strcmp(conf_string_read( STREAM0_RTSPSTREAMNAME), argm->value)) {
			conf_lock();
			strcpy(conf_string_read( STREAM0_RTSPSTREAMNAME), argm->value);
			sys_info->osd_setup = 1;
			conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			sprintf(rtsp_stream[0], "/%s", conf_string_read( STREAM0_RTSPSTREAMNAME));
			uri_hash_table_release();
			uri_hash_table_init();
			system("killall -SIGTERM rtsp-streamer\n");
			system("/opt/ipnc/rtsp-streamer &\n");
		}
/*
		if (strcmp(sys_info->config.lan_config.stream[0].rtspstreamname, argm->value)) {
			sysconf_lock();
			strcpy(sys_info->config.lan_config.stream[0].rtspstreamname, argm->value);
			sys_info->osd_setup = 1;
			sysconf_unlock();
			sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
			system("killall -SIGTERM rtsp-streamer\n");
			system("/opt/ipnc/rtsp-streamer &\n");
		}
		*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void rtsp_stream_name_2(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(STREAM1_RTSPSTREAMNAME));
        return;
    }

	do {
		if (strlen(argm->value) > sys_info->ipcam[STREAM0_RTSPSTREAMNAME].max)
			break;
		if (strcmp(conf_string_read( STREAM1_RTSPSTREAMNAME), argm->value)) {
			conf_lock();
			strcpy(conf_string_read( STREAM1_RTSPSTREAMNAME), argm->value);
			sys_info->osd_setup = 1;
			conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			sprintf(rtsp_stream[1], "/%s", conf_string_read( STREAM1_RTSPSTREAMNAME));
			uri_hash_table_release();
			uri_hash_table_init();
			system("killall -SIGTERM rtsp-streamer\n");
			system("/opt/ipnc/rtsp-streamer &\n");
		}
		/*
		if (strcmp(sys_info->config.lan_config.stream[1].rtspstreamname, argm->value)) {
			sysconf_lock();
			strcpy(sys_info->config.lan_config.stream[1].rtspstreamname, argm->value);
			sys_info->osd_setup = 1;
			sysconf_unlock();
			sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
			system("killall -SIGTERM rtsp-streamer\n");
			system("/opt/ipnc/rtsp-streamer &\n");
		}
*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void rtsp_stream_name_3(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(STREAM2_RTSPSTREAMNAME));
        return;
    }

	do {
		if (strlen(argm->value) > sys_info->ipcam[STREAM0_RTSPSTREAMNAME].max)
			break;
		if (strcmp(conf_string_read( STREAM2_RTSPSTREAMNAME), argm->value)) {
			conf_lock();
			strcpy(conf_string_read( STREAM2_RTSPSTREAMNAME), argm->value);
			sys_info->osd_setup = 1;
			conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			sprintf(rtsp_stream[2], "/%s", conf_string_read( STREAM2_RTSPSTREAMNAME));
			uri_hash_table_release();
			uri_hash_table_init();
			system("killall -SIGTERM rtsp-streamer\n");
			system("/opt/ipnc/rtsp-streamer &\n");
		}
		/*
		if (strcmp(sys_info->config.lan_config.stream[2].rtspstreamname, argm->value)) {
			sysconf_lock();
			strcpy(sys_info->config.lan_config.stream[2].rtspstreamname, argm->value);
			sys_info->osd_setup = 1;
			sysconf_unlock();
			sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
			system("killall -SIGTERM rtsp-streamer\n");
			system("/opt/ipnc/rtsp-streamer &\n");
		}
		*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_sntpip(request *req, COMMAND_ARGUMENT *argm)
{
	char value[256];
	int len;

	do {
		if ((len = ControlSystemData(SFIELD_GET_SNTP_FQDN, (void *)value, sizeof(value), NULL)) < 0)
			break;
		value[len] = '\0';
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_sntptimezone(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		/*
		if( pSysInfo == NULL )
			break;
		*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NTP_TIMEZONE].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.net.ntp_timezone);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_sntpfqdn(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( NTP_SERVER));
        return;
    }

    do {
        if(strlen(argm->value) > sys_info->ipcam[NTP_SERVER].max)
            break;
        
        if (strcmp(conf_string_read( NTP_SERVER), argm->value)) {
            if (sys_info->ipcam[NTP_FREQUENCY].config.s8 == 1)
                api.bottom.ntpd_restart = 1;  //exec_ntpd(argm->value);

            ControlSystemData(SFIELD_SET_SNTP_SERVER, (void *)argm->value, strlen(argm->value), &req->curr_user);
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_date(request *req, COMMAND_ARGUMENT *argm)
{
	static char wday_name[7][3] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	time_t tnow;
	struct tm *tmnow;
	int string_len;

	time(&tnow);
	tmnow = localtime(&tnow);

	string_len = sprintf(req_bufptr(req), OPTION_OK "%s=%04d-%02d-%02d %.3s\n", argm->name,
			tmnow->tm_year+1900, tmnow->tm_mon+1, tmnow->tm_mday, wday_name[tmnow->tm_wday]);
	req->buffer_end += string_len;
}

void get_time(request *req, COMMAND_ARGUMENT *argm)
{
	time_t tnow;
	struct tm *tmnow;
	int string_len;

	time(&tnow);
	tmnow = localtime(&tnow);

	string_len = sprintf(req_bufptr(req), OPTION_OK "%s=%02d:%02d:%02d\n", argm->name,
			 		tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);
	req->buffer_end += string_len;
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int sys_set_date(int year, int month, int day)
{
	struct tm *tm;
	time_t now;

	now = time(NULL);
	tm = localtime(&now);

	year = (year>1900) ? year-1900 : 0;
	tm->tm_year = year;
	month = (month>0) ? month-1 : 0;
	tm->tm_mon = month;
	tm->tm_mday = day;

	if ((now = mktime(tm)) < 0)
		return -1;
	stime(&now);

	if(systime_to_rtc() < 0)
		return -1;

	/*
	if(sys_info->config.lan_config.net.dst_enable && 
		sys_info->config.lan_config.net.dst_mode == 0) {
		tm->tm_hour++;
		now = mktime(tm);
		return stime(&now);
	}
	*/ // disble by Alex. 2009.08.28

	return 0;

	// return systime_to_rtc();
	// modified by Alex, 2009.05.05
}

int sys_set_time(int hour, int min, int sec)
{
	struct tm *tm;
	time_t now;

	now = time(NULL);
	tm = localtime(&now);

	tm->tm_hour = hour;
	tm->tm_min = min;
	tm->tm_sec = sec;

	if ((now = mktime(tm)) < 0)
		return -1;
	stime(&now);

	if(systime_to_rtc() < 0)
		return -1;

	/*
	if(sys_info->config.lan_config.net.dst_enable && 
		sys_info->config.lan_config.net.dst_mode == 0) {
		tm->tm_hour++;
		now = mktime(tm);
		return stime(&now);
	}
	*/  // disble by Alex. 2009.08.28

	return 0;

	// return systime_to_rtc();
	// modified by Alex, 2009.05.05
}

void set_system_date(request *req, COMMAND_ARGUMENT *argm)
{
	int year, month, day;
	int ret;
    
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04d_%02d_%02d\n", argm->name, 
            sys_info->tm_now.tm_year + 1900, sys_info->tm_now.tm_mon + 1, sys_info->tm_now.tm_mday );
        return;
    }

	do {
		// modified by Alex, 2009.04.14. for more compatible
		if (sscanf(argm->value, "%d%*c%d%*c%d", &year, &month, &day) != 3)
			break;
		ret = getmaxmday(year, month);		//add by jiahung, 2009.05.08

		if (ret < 0 || ret < day)
			break;	
		if (sys_set_date(year, month, day) < 0)
			break;
#ifdef MSGLOGNEW
		char loginip[20];
		char logstr[MSG_MAX_LEN];
		char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
		strcpy(loginip, ipstr(req->curr_user.ip));
		sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
		snprintf(logstr, MSG_USE_LEN, "%s SET DATE Year %04d Month %02d Day %02d", vistor, year, month, day);
		sprintf(logstr, "%s\r\n", logstr);
		sysdbglog(logstr);
#endif
		// no more used. 2010.07.12 Alex.
		//ControlSystemData(SFIELD_SET_DATE, argm->value, strlen(argm->value), &req->curr_user);

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

#if 0
void get_asmtpenable(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[BASMTPENABLE].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.bASmtpEnable);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_asmtpattach(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value;

	do {
		ControlSystemData(SFIELD_GET_SMTP_ATTACHMENTS, (void *)&value, sizeof(value), NULL);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , value);
    return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_imagesource(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.net.imagesource);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void set_system_time(request *req, COMMAND_ARGUMENT *argm)
{
	int hour, min, sec;

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%02d:%02d:%02d\n", argm->name, 
            sys_info->tm_now.tm_hour, sys_info->tm_now.tm_min, sys_info->tm_now.tm_sec );
        return;
    }

	do {
		if (sscanf(argm->value, "%d:%d:%d", &hour, &min, &sec) != 3)
			break;

		if (sys_set_time(hour, min, sec) < 0)
			break;

		ControlSystemData(SFIELD_SET_TIME, argm->value, strlen(argm->value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_time_frequency(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NTP_FREQUENCY].config.s8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.net.ntp_frequency);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

		int value = atoi(argm->value);
		if(sys_info->ipcam[NTP_FREQUENCY].config.s8 != value) {
		//if(sys_info->config.lan_config.net.ntp_frequency != value) {
			if (value == 0)
				kill_ntpd();
//			else if (value < 0) {
			else if (value == 1) {
				api.bottom.ntpd_restart = 1;
//				if (exec_ntpd(sys_info->config.lan_config.net.ntp_server) < 0)
//					break;
			}
			else if (value <= MAX_NTP_FREQUENCY) {
				if (exec_ntpdate(conf_string_read( NTP_SERVER)) < 0)
				//if (exec_ntpdate(sys_info->config.lan_config.net.ntp_server) < 0)
					break;
			}
			else
				break;

			//sysconf_lock();
                        conf_lock();
                        sys_info->ipcam[NTP_FREQUENCY].config.s8 = value;
			//sys_info->config.lan_config.net.ntp_frequency = value;
			sys_info->osd_setup = 1;
                        conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
			//sysconf_unlock();
			//sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
			ControlSystemData(SFIELD_SET_SNTP_FREQUENCY, (void *)&value, sizeof(value), &req->curr_user);
		}			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void ntp_enable(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NTP_SERVER].config.s8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.net.ntp_frequency);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

		int value = atoi(argm->value);
		if(sys_info->ipcam[NTP_SERVER].config.s8 != value) {
		//if(sys_info->config.lan_config.net.ntp_frequency != value) {
			if (value == 0)
				kill_ntpd();
			else if (value == 1) {
				api.bottom.ntpd_restart = 1;
//				if (exec_ntpd(sys_info->config.lan_config.net.ntp_server) < 0)
//					break;
			}
			else
				break;
			
			conf_lock();
			sys_info->ipcam[NTP_SERVER].config.s8 = value;
			sys_info->osd_setup = 1;
			conf_unlock();
			info_set_flag(CONF_FLAG_CHANGE);
/*
			sysconf_lock();
			sys_info->config.lan_config.net.ntp_frequency = value;
			sys_info->osd_setup = 1;
			sysconf_unlock();
			sysinfo_set_flag(SYSCONF_FLAG_CHANGE);
*/
			ControlSystemData(SFIELD_SET_SNTP_FREQUENCY, (void *)&value, sizeof(value), &req->curr_user);
		}			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}


void set_day_night(request *req, COMMAND_ARGUMENT *argm)
{
    /*
	__u8 value = atoi(argm->value);

	do {
		if ( value > 1 )
			break;

		if(sys_info->config.lan_config.net.daylight_time != value)
			ControlSystemData(SFIELD_SET_DAY_NIGHT, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	*/
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
} 
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ddnsenable(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080827
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DDNS_ENABLE].config.u8);
        return;
    }
    
    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[DDNS_ENABLE].max)
            break;

        if(sys_info->ipcam[DDNS_ENABLE].config.u8 != value)
            ControlSystemData(SFIELD_SET_DDNS_ENABLE, (void *)&value, sizeof(value), &req->curr_user);

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnsenable(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[DDNS_ENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ddnstype(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080827
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DDNS_TYPE].config.s8);
        return;
    }

//#ifdef CONFIG_BRAND_DLINK
//        if(value > 0)   
//            value--;
//#endif
    
    do {
        if(numeric_check(argm->value) < 0)
            break;

        __s8 value = atoi(argm->value);

        if(value < sys_info->ipcam[DDNS_TYPE].min || value > sys_info->ipcam[DDNS_TYPE].max)
            break;

        if(sys_info->ipcam[DDNS_TYPE].config.s8 != value)
            ControlSystemData(SFIELD_SET_DDNS_TYPE, (void *)&value, sizeof(value), &req->curr_user);

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnstype(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[DDNS_TYPE].config.s8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ddnshostname(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080827
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(DDNS_HOSTNAME));
        return;
    }

    do {
        if(strlen(argm->value) > sys_info->ipcam[DDNS_HOSTNAME].max)
            break;

        if (strcmp(conf_string_read(DDNS_HOSTNAME), argm->value)) {
            ControlSystemData(SFIELD_SET_DDNS_HOST, (void *)argm->value, strlen(argm->value), &req->curr_user);
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnshostname(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read( DDNS_HOSTNAME));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ddnsaccount(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080827
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(DDNS_ACCOUNT));
        return;
    }

    do {
        if(strlen(argm->value) > sys_info->ipcam[DDNS_ACCOUNT].max)
            break;

        if ( strcmp(conf_string_read(DDNS_ACCOUNT), argm->value)) {
            ControlSystemData(SFIELD_SET_DDNS_ACCOUNT, (void *)argm->value, strlen(argm->value), &req->curr_user);
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;

    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnsaccount(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080827
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read( DDNS_ACCOUNT));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ddnspassword(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080827
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(DDNS_PASSWORD));
        return;
    }

    do {
        if(strlen(argm->value) > sys_info->ipcam[DDNS_PASSWORD].max)
            break;

        if (strcmp(conf_string_read(DDNS_PASSWORD), argm->value)) {
            ControlSystemData(SFIELD_SET_DDNS_PASSWORD, (void *)argm->value, strlen(argm->value), &req->curr_user);
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;

    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnspassword(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080827
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read( DDNS_PASSWORD));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ddnsinterval(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u\n", argm->name, sys_info->ipcam[A_DDNS_INTERVAL].config.u32);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u32 value = atoi(argm->value);

        if(value < sys_info->ipcam[A_DDNS_INTERVAL].min || value > sys_info->ipcam[A_DDNS_INTERVAL].max)
            break;
        
        if(sys_info->ipcam[A_DDNS_INTERVAL].config.u32 != value)
            ControlSystemData(SFIELD_SET_DDNS_INTERVAL, (void *)&value, sizeof(value), &req->curr_user);

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnsstatus(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->status.ddns);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ipfilterenable(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080902
{
#ifndef SUPPORT_NEW_IPFILTER
	pid_t child;
	int x;
	char ip_paserd[2][4][10];
	char ip_addr[64];
	__u8 enable = atoi(argm->value);
	
	do {
		//if( sys_info->config.lan_config.net.ipfilterenable != enable ) {
			//sys_info->config.lan_config.net.ipfilterenable = enable;
		if( sys_info->ipcam[IPFILTERENABLE].config.u8 != enable ) {
			sys_info->ipcam[IPFILTERENABLE].config.u8 = enable;
			
			child  = fork();
			switch(child) {
			case -1:
		  		dbg("fork() error!\n");
		  		break;
				
		  	case  0:  //in child process
		  		if(enable == 1) {
		  			char cmd[30];
					for(x = 0; x < sys_info->ipcam[MAXIPFILTERPOLICY].config.u8; x++) {
						if(sys_info->ipcam[IP_FILTER0_POLICY+x*2].config.value != -1) {
							if(strchr(conf_string_read(IP_FILTER0_IP+x*2), '(') != NULL || strchr(conf_string_read(IP_FILTER0_IP+x*2), '*') != NULL){
								ipfilter_parser(conf_string_read(IP_FILTER0_IP+x*2), ip_paserd);
								sprintf(ip_addr,"%s.%s.%s.%s-%s.%s.%s.%s",ip_paserd[0][0],ip_paserd[0][1],ip_paserd[0][2],ip_paserd[0][3],ip_paserd[1][0],ip_paserd[1][1],ip_paserd[1][2],ip_paserd[1][3]);
								execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-A", "INPUT", "-m", "iprange", "--src-range",
								ip_addr,"-j", sys_info->ipcam[IP_FILTER0_POLICY+x*2].config.value ? "ACCEPT" : "DROP", 0);
							}
							else {
								dbg(IPTABLES_EXEC" -A INPUT -s %s -j %s\n",conf_string_read(IP_FILTER0_IP+x*2), sys_info->ipcam[IP_FILTER0_POLICY+x*2].config.value ? "ACCEPT" : "DROP");
								sprintf(cmd, IPTABLES_EXEC" -A INPUT -s %s -j %s",conf_string_read(IP_FILTER0_IP+x*2), sys_info->ipcam[IP_FILTER0_POLICY+x*2].config.value ? "ACCEPT" : "DROP");
								system(cmd);
							}
						}else
							break;
					}
					/*for(x = 0; x < sys_info->config.lan_config.net.maxipfilterpolicy; x++) {
						if(sys_info->config.lan_config.net.ip_filter_table[x].policy != -1) {
							if(strchr(sys_info->config.lan_config.net.ip_filter_table[x].ip,'(') != NULL || strchr(sys_info->config.lan_config.net.ip_filter_table[x].ip,'*') != NULL){
								ipfilter_parser(sys_info->config.lan_config.net.ip_filter_table[x].ip,ip_paserd);
								sprintf(ip_addr,"%s.%s.%s.%s-%s.%s.%s.%s",ip_paserd[0][0],ip_paserd[0][1],ip_paserd[0][2],ip_paserd[0][3],ip_paserd[1][0],ip_paserd[1][1],ip_paserd[1][2],ip_paserd[1][3]);
								execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-A", "INPUT", "-m", "iprange", "--src-range",
								ip_addr,"-j",sys_info->config.lan_config.net.ip_filter_table[x].policy ? "ACCEPT" : "DROP",0);
							}
							else {
								dbg(IPTABLES_EXEC" -A INPUT -s %s -j %s\n",sys_info->config.lan_config.net.ip_filter_table[x].ip,sys_info->config.lan_config.net.ip_filter_table[x].policy ? "ACCEPT" : "DROP");
								sprintf(cmd, IPTABLES_EXEC" -A INPUT -s %s -j %s",sys_info->config.lan_config.net.ip_filter_table[x].ip,sys_info->config.lan_config.net.ip_filter_table[x].policy ? "ACCEPT" : "DROP");
								system(cmd);
							}
						}else
							break;
					}
*/
				}
				else {
					for(x = 0; x < sys_info->ipcam[MAXIPFILTERPOLICY].config.u8; x++) {
						if(sys_info->ipcam[IP_FILTER0_POLICY+x*2].config.value != -1)
							system(IPTABLES_EXEC" -D INPUT 1");
					/*for(x = 0; x < sys_info->config.lan_config.net.maxipfilterpolicy; x++) {
						if(sys_info->config.lan_config.net.ip_filter_table[x].policy != -1)
							system(IPTABLES_EXEC" -D INPUT 1");
*/
					}
				}
		  		_exit(0);
		  		break;
			}
			
			info_write(&enable, IPFILTERENABLE, 0, 0);
			//sysinfo_write(&enable, offsetof(SysInfo, config.lan_config.net.ipfilterenable), sizeof(enable),0);
			ControlSystemData(SFIELD_SET_IPFILTER_ENABLE, (void *)&enable, sizeof(enable), &req->curr_user);	/* add by Alex, 2008.12.23	*/
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ipfilterdefaultpolicy(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080902
{
#ifndef SUPPORT_NEW_IPFILTER
	/*SysInfo* pSysInfo = GetSysInfo();*/
	pid_t child;
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[IPFILTERDEFAULTPOLICY].config.u8 != atoi(argm->value)){
			if((atoi(argm->value) == 0) && (sys_info->ipcam[IPFILTERDEFAULTPOLICY].config.u8 == -1)){
				dbg("the first rule can't be empty!.\n");
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
			sys_info->ipcam[IPFILTERDEFAULTPOLICY].config.u8 = atoi(argm->value);
dbg("Default Policy = %d\n",sys_info->ipcam[IPFILTERDEFAULTPOLICY].config.u8);
			child  = fork();
			switch(child){
				case -1:
			 		dbg("fork() error!\n");
			 		break;
			 	case  0:  //in child process
			 		//execl("/bin/ls","ls",0);
			 		execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-P", "INPUT", sys_info->ipcam[IPFILTERDEFAULTPOLICY].config.u8 ? "ACCEPT" : "DROP",0);
			 		dbg("child process error! (set_ipfilterdefaultpolicy)\n");
			 		_exit(-1);
			 		break;
			}
#if 0
		if(sys_info->config.lan_config.net.ipfilterdefaultpolicy != atoi(argm->value)){
			if((atoi(argm->value) == 0) && (sys_info->config.lan_config.net.ip_filter_table[0].policy == -1)){
				dbg("the first rule can't be empty!.\n");
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
			sys_info->config.lan_config.net.ipfilterdefaultpolicy = atoi(argm->value);
dbg("Default Policy = %d\n",sys_info->config.lan_config.net.ipfilterdefaultpolicy);
			child  = fork();
			switch(child){
				case -1:
			 		dbg("fork() error!\n");
			 		break;
			 	case  0:  //in child process
			 		//execl("/bin/ls","ls",0);
			 		execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-P", "INPUT", sys_info->config.lan_config.net.ipfilterdefaultpolicy ? "ACCEPT" : "DROP",0);
			 		dbg("child process error! (set_ipfilterdefaultpolicy)\n");
			 		_exit(-1);
			 		break;
			}
#endif
			info_write(&value, IPFILTERDEFAULTPOLICY, 0, 0);
			//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.net.ipfilterdefaultpolicy), sizeof(__u8),0);
			ControlSystemData(SFIELD_SET_IPFILTER_DEFAULT, (void *)&value, sizeof(value), &req->curr_user);	/* add by Alex, 2008.12.23	*/			
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ipfilterpolicyexch(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080903
{
#ifndef SUPPORT_NEW_IPFILTER

	pid_t child;
	int table_entry[2];
	int tmp_policy;
	int ip_is_range = 0;
	char tmp_ip[64];
	char remove_entry[30];
	char moved_entry[3];
	char ip_paserd[2][4][10];
	__u16 value = 0;


	table_entry[0] = argm->value[0] - 48;
	table_entry[1] = argm->value[2] - 48;
	value = (table_entry[0] << 8) & table_entry[1];	/* add by Alex, 2008.12.24	*/
	
	do {
		if(table_entry[0] > table_entry[1]){		//MOVE UP
			if(table_entry[0] == 1 && (strchr(conf_string_read(IP_FILTER0_IP+table_entry[0]*2),'*') != NULL || sys_info->ipcam[IP_FILTER0_POLICY+table_entry[0]*2].config.value == 0)){
				dbg("the first rule can't be '*'!.\n");
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
			tmp_policy = sys_info->ipcam[IP_FILTER0_POLICY+table_entry[1]*2].config.value;
			strcpy(tmp_ip , conf_string_read(IP_FILTER0_IP+table_entry[1]*2));
			sys_info->ipcam[IP_FILTER0_POLICY+table_entry[1]*2].config.value = sys_info->ipcam[IP_FILTER0_POLICY+table_entry[0]*2].config.value;
			strcpy(conf_string_read(IP_FILTER0_IP+table_entry[1]*2) , conf_string_read(IP_FILTER0_IP+table_entry[1]*2));
			sys_info->ipcam[IP_FILTER0_POLICY+table_entry[0]*2].config.value = tmp_policy;
			strcpy( conf_string_read(IP_FILTER0_IP+table_entry[0]*2) , tmp_ip );
			sprintf(remove_entry, IPTABLES_EXEC" -D INPUT %d",table_entry[1] + 1);
			sprintf(moved_entry,"%d",table_entry[0] + 1);
		}else{		//MOVE DOWN
			if(table_entry[1] == 1 && (strchr(conf_string_read(IP_FILTER0_IP+table_entry[1]*2),'*') != NULL || sys_info->ipcam[IP_FILTER0_POLICY+table_entry[0]*2].config.value == 0)){
				dbg("the first rule can't be '*'!.\n");
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
			tmp_policy = sys_info->ipcam[IP_FILTER0_POLICY+table_entry[0]*2].config.value;
			strcpy(tmp_ip , conf_string_read(IP_FILTER0_IP+table_entry[0]*2) );
			sys_info->ipcam[IP_FILTER0_POLICY+table_entry[0]*2].config.value = sys_info->ipcam[IP_FILTER1_POLICY+table_entry[0]*2].config.value;
			strcpy(conf_string_read(IP_FILTER0_IP+table_entry[0]*2) , conf_string_read(IP_FILTER0_IP+table_entry[1]*2) );
			sys_info->ipcam[IP_FILTER0_POLICY+table_entry[1]*2].config.value = tmp_policy;
			strcpy( conf_string_read(IP_FILTER0_IP+table_entry[1]*2) , tmp_ip );
			sprintf(remove_entry, IPTABLES_EXEC" -D INPUT %d",table_entry[0] + 1);
			sprintf(moved_entry,"%d",table_entry[1] + 1);
		}
		if(strchr(tmp_ip,'(') != NULL || strchr(tmp_ip,'*') != NULL){
			ipfilter_parser(tmp_ip,ip_paserd);
			ip_is_range = 1;
			sprintf(tmp_ip,"%s.%s.%s.%s-%s.%s.%s.%s",ip_paserd[0][0],ip_paserd[0][1],ip_paserd[0][2],ip_paserd[0][3],ip_paserd[1][0],ip_paserd[1][1],ip_paserd[1][2],ip_paserd[1][3]);
		}
#if 0
			if(table_entry[0] == 1 && (strchr(sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].ip,'*') != NULL || sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].policy == 0)){
				dbg("the first rule can't be '*'!.\n");
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
			tmp_policy = sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].policy;
			strcpy(tmp_ip , sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].ip);
			sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].policy = sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].policy;
			strcpy(sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].ip , sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].ip);
			sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].policy = tmp_policy;
			strcpy( sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].ip , tmp_ip );
			sprintf(remove_entry, IPTABLES_EXEC" -D INPUT %d",table_entry[1] + 1);
			sprintf(moved_entry,"%d",table_entry[0] + 1);
		}else{		//MOVE DOWN
			if(table_entry[1] == 1 && (strchr(sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].ip,'*') != NULL || sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].policy == 0)){
				dbg("the first rule can't be '*'!.\n");
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
			tmp_policy = sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].policy;
			strcpy(tmp_ip , sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].ip );
			sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].policy = sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].policy;
			strcpy(sys_info->config.lan_config.net.ip_filter_table[table_entry[0]].ip , sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].ip );
			sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].policy = tmp_policy;
			strcpy( sys_info->config.lan_config.net.ip_filter_table[table_entry[1]].ip , tmp_ip );
			sprintf(remove_entry, IPTABLES_EXEC" -D INPUT %d",table_entry[0] + 1);
			sprintf(moved_entry,"%d",table_entry[1] + 1);
		}
		if(strchr(tmp_ip,'(') != NULL || strchr(tmp_ip,'*') != NULL){
			ipfilter_parser(tmp_ip,ip_paserd);
			ip_is_range = 1;
			sprintf(tmp_ip,"%s.%s.%s.%s-%s.%s.%s.%s",ip_paserd[0][0],ip_paserd[0][1],ip_paserd[0][2],ip_paserd[0][3],ip_paserd[1][0],ip_paserd[1][1],ip_paserd[1][2],ip_paserd[1][3]);
		}
#endif
		//sprintf(cmd,"iptables -D INPUT %s",remove_entry);
		dbg("remove_entry = %s\n",remove_entry);
		dbg("moved_entry = %s, tmp_ip = %s,tmp_policy = %d\n",moved_entry,tmp_ip,tmp_policy);

		child  = fork();
		switch(child){
		case -1:
	  		dbg("fork() error!\n");
	  		break;

	  	case  0://in child process
	  		system(remove_entry);
	  		if(ip_is_range)
	 			execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-I", "INPUT", moved_entry, "-m", "iprange", "--src-range", tmp_ip, "-j", tmp_policy ? "ACCEPT" : "DROP", 0);
	 		else
	 			execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-I", "INPUT", moved_entry, "-s", tmp_ip, "-j", tmp_policy ? "ACCEPT" : "DROP", 0);
	  		dbg("child process error! (set_ipfilterpolicyexch)\n");
	  		_exit(-1);
	  		break;
	  }

		info_set_flag(CONF_FLAG_CHANGE);
		//sysinfo_write(&(sys_info->config.lan_config.net.ip_filter_table[0]), offsetof(SysInfo, config.lan_config.net.ip_filter_table), sizeof(IP_FILTER_TABLE) * MAX_IPFILTER_NUM, 0);
		ControlSystemData(SFIELD_SET_IPFILTER_EXCHT, (void *)&value, sizeof(value), &req->curr_user);	/* add by Alex, 2008.12.23	*/		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ipfilterpolicydel(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080903
{
#ifndef SUPPORT_NEW_IPFILTER
	/*SysInfo* pSysInfo = GetSysInfo();*/
	pid_t child;
	int active_table_num;
	int x;
	char tmp[2];
	
	active_table_num = argm->value[0] - 48;

	do {
		if((active_table_num == 0 && sys_info->ipcam[IP_FILTER0_POLICY+(active_table_num+1)*2].config.value == 0) ||
			(sys_info->ipcam[IP_FILTER1_POLICY].config.value == -1 && sys_info->ipcam[IPFILTERDEFAULTPOLICY].config.u8 == 0)	){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		}
		sys_info->ipcam[IP_FILTER1_POLICY+active_table_num*2].config.value = -1;
		//strcpy(pSysInfo->lan_config.net.ip_filter_table[active_table_num].ip , "");
		for(x = active_table_num ; x < sys_info->ipcam[MAXIPFILTERPOLICY].config.u8 - 1; x++){
			if(sys_info->ipcam[IP_FILTER0_POLICY+(x+1)*2].config.value != -1){
				sys_info->ipcam[IP_FILTER0_POLICY+x*2].config.value = sys_info->ipcam[IP_FILTER0_POLICY+(x+1)*2].config.value;
				strcpy(conf_string_read(IP_FILTER0_IP+x*2), conf_string_read(IP_FILTER0_IP+(x+1)*2);
				sys_info->ipcam[IP_FILTER0_POLICY+(x+1)*2].config.value = -1;
			}else	break;
		}
		#if 0
		if((active_table_num == 0 && sys_info->config.lan_config.net.ip_filter_table[active_table_num + 1].policy == 0) ||
			(sys_info->config.lan_config.net.ip_filter_table[1].policy == -1 && sys_info->config.lan_config.net.ipfilterdefaultpolicy == 0)	){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		}
		sys_info->config.lan_config.net.ip_filter_table[active_table_num].policy = -1;
		//strcpy(pSysInfo->lan_config.net.ip_filter_table[active_table_num].ip , "");
		for(x = active_table_num ; x < sys_info->config.lan_config.net.maxipfilterpolicy - 1; x++){
			if(sys_info->config.lan_config.net.ip_filter_table[x + 1].policy != -1){
				sys_info->config.lan_config.net.ip_filter_table[x].policy = sys_info->config.lan_config.net.ip_filter_table[x + 1].policy;
				strcpy(sys_info->config.lan_config.net.ip_filter_table[x].ip,sys_info->config.lan_config.net.ip_filter_table[x + 1].ip);
				sys_info->config.lan_config.net.ip_filter_table[x + 1].policy = -1;
			}else	break;
		}
		#endif
		sprintf(tmp,"%d",active_table_num + 1);

		child  = fork();
		switch(child){
			case -1:
	  		dbg("fork() error!\n");
	  		break;
	  	case  0://in child process
	  		execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-D", "INPUT", tmp, 0);
	  		dbg("child process error! (set_ipfilterpolicydel)\n");
	  		_exit(-1);
	  		break;
	  }
		ControlSystemData(SFIELD_SET_IPFILTER_DEL, (void *)&active_table_num, sizeof(active_table_num), &req->curr_user);	/* add by Alex, 2008.12.23	*/		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ipfilterpolicyedit(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080903
{
#ifndef SUPPORT_NEW_IPFILTER
	/*SysInfo* pSysInfo = GetSysInfo();*/
	pid_t child;
	int policy , old_policy;
	int active_table_num = 0;
	int ip_is_range = 0;
	char *location;
	char ip_paserd[2][4][10];
	char ip_addr[64];


dbg("ipfilterpolicyedit cmd string = %s\n",argm->value);
	active_table_num = argm->value[0] - 48;
dbg("active_table_num = %d\n",active_table_num);
	if(active_table_num < 0 ){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	}
	location = strchr(argm->value , ':'); location++;
dbg("location string = %s\n",location);
	if(location[0] == 'D')
		policy = 0;
	else
		policy = 1;
	location = strchr(location , ':'); location++;
	if(strchr(location,'(') != NULL || strchr(location,'*') != NULL ){
		if(active_table_num == 0 && strchr(location,'*') != NULL  && policy == 0){
			dbg("the first rule can't be '*'!.\n");
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		}
		ipfilter_parser(location,ip_paserd);
		sprintf(ip_addr,"%s.%s.%s.%s-%s.%s.%s.%s",ip_paserd[0][0],ip_paserd[0][1],ip_paserd[0][2],ip_paserd[0][3],ip_paserd[1][0],ip_paserd[1][1],ip_paserd[1][2],ip_paserd[1][3]);
dbg("ip_range = %s\n",ip_addr);
		ip_is_range = 1;
	}else if(strchr(location,'-') != NULL || 
	( (strcmp(conf_string_read(IP_FILTER0_IP+active_table_num*2),location) == 0 ||
		sys_info->ipcam[IP_FILTER0_POLICY].config.value == -1 ) &&
		/*( (strcmp(sys_info->config.lan_config.net.ip_filter_table[active_table_num].ip,location) == 0 || 
		sys_info->config.lan_config.net.ip_filter_table[0].policy == -1 ) && */
		(policy == 0 && active_table_num == 0))){
dbg("the same ip.\n");
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	}else
		strcpy(ip_addr , location);

	do {
		old_policy = sys_info->ipcam[IP_FILTER0_POLICY+active_table_num*2].config.value;
		sys_info->ipcam[IP_FILTER0_POLICY+active_table_num*2].config.value = policy;
		strcpy( conf_string_read(IP_FILTER0_IP+active_table_num*2) , location);
	/*	
                old_policy = sys_info->config.lan_config.net.ip_filter_table[active_table_num].policy;
		sys_info->config.lan_config.net.ip_filter_table[active_table_num].policy = policy;
		strcpy( sys_info->config.lan_config.net.ip_filter_table[active_table_num].ip , location);
		*/
                sprintf(location,"%d",toascii(active_table_num + 1));
		child  = fork();
		switch(child){
			case -1:
	  		dbg("fork() error!\n");
	  		break;
	  	case  0://in child process
	  		if(ip_is_range){
	  			if(old_policy == -1)
		  			execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-A", "INPUT", "-m", "iprange", "--src-range", ip_addr, "-j", policy ? "ACCEPT" : "DROP",0);
		  		else
		  			execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-R", "INPUT", location, "-m", "iprange", "--src-range", ip_addr, "-j", policy ? "ACCEPT" : "DROP",0);
	  		}else{
		  		if(old_policy == -1)
		  			execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-A", "INPUT", "-s", ip_addr, "-j", policy ? "ACCEPT" : "DROP",0);
		  		else
		  			execl(IPTABLES_EXEC_PATH, IPTABLES_EXEC, "-R", "INPUT", location, "-s", ip_addr, "-j", policy ? "ACCEPT" : "DROP",0);
	  		}
	  		dbg("child process error! (set_ipfilterpolicyedit)\n");
	  		_exit(-1);
	  		break;
		}
		
		info_set_flag(CONF_FLAG_CHANGE);
			/*
		sysinfo_write(&sys_info->config.lan_config.net.ip_filter_table[active_table_num], 
			offsetof(SysInfo, config.lan_config.net.ip_filter_table[active_table_num]), 
			sizeof(IP_FILTER_TABLE), 0);
		*/
		ControlSystemData(SFIELD_SET_IPFILTER_EDIT, (void *)&active_table_num, sizeof(active_table_num), &req->curr_user);	/* add by Alex, 2008.12.23	*/		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_quotainbound(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080902
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u\n", argm->name, sys_info->ipcam[QUOTAINBOUND].config.u32);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u32 value = atoi(argm->value);

        if(value > sys_info->ipcam[QUOTAINBOUND].max)
            break;
        
        if(sys_info->ipcam[QUOTAINBOUND].config.u32 != value)
            if (ControlSystemData(SFIELD_SET_QUOTA_INBOUND, (void *)&value, sizeof(value), &req->curr_user) < 0)
				break;
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_quotaoutbound(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080902
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u\n", argm->name, sys_info->ipcam[QUOTAOUTBOUND].config.u32);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u32 value = atoi(argm->value);

        if(value > sys_info->ipcam[QUOTAOUTBOUND].max)
            break;

        if(sys_info->ipcam[QUOTAOUTBOUND].config.u32 != value)
            if (ControlSystemData(SFIELD_SET_QUOTA_OUTBOUND, (void *)&value, sizeof(value), &req->curr_user) < 0)
				break;
#if 0       
        sprintf(cmd[0],"%u",sys_info->config.lan_config.net.quotainbound*8);
        sprintf(cmd[1],"%u",sys_info->config.lan_config.net.quotaoutbound*8);
      
        child  = fork();
        switch(child){
            case -1:
                dbg("fork() error!\n");
                break;
            
            case  0: //in child process
            //dbg("in child!, outbound = %d\n",pSysInfo->lan_config.net.quotaoutbound*8);
            if((sys_info->config.lan_config.net.quotainbound == 0) && (sys_info->config.lan_config.net.quotaoutbound == 0)){
                execl(QoS_SCRIPT_PATH,WSHAPER_NAME,"stop",0);
            }else{
                if((sys_info->config.lan_config.net.pppoeactive == 0))
                    execl(QoS_SCRIPT_PATH,WSHAPER_NAME,cmd[0],cmd[1],0);
                else
                    execl(PPPoE_QoS_SCRIPT_PATH,PPPoE_WSHAPER_NAME,cmd[0],cmd[1],0);
                }
                dbg("child process error!\n");
                _exit(-1);
                break;
        }
#endif      
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_pppoeenable(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080825
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PPPOEENABLE].config.u8);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[PPPOEENABLE].max)
            break;

        if(sys_info->ipcam[PPPOEENABLE].config.u8 != value){
            ControlSystemData(SFIELD_SET_PPPOE_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
            
            // WHY? why if value equal to zero needs to send message queue again??
            if ( value == 0 )
                ControlSystemData(SFIELD_SET_PPPOE_ENABLE, (void *)&value, sizeof(value), &req->curr_user);    
        }
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_pppoeenable(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080825
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PPPOEENABLE].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_pppoeaccount(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(PPPOEACCOUNT));
}
#if 0 /* Disable by Alex, 2009.2.19 */
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnsfqdn(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->config.ddns.webname);
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ddnsport(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		/*if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 80);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_isppassword(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,conf_string_read(PPPOEPASSWORD));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ispaccount(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,conf_string_read(PPPOEACCOUNT));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_pppoepwd(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(PPPOEPASSWORD));
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_pppoeaccount(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(PPPOEACCOUNT));
        return;
    }

    do {
        if(strlen(argm->value) > sys_info->ipcam[PPPOEACCOUNT].max)
            break;
        
        if ( (strcmp( conf_string_read( PPPOEACCOUNT), argm->value)) || (sys_info->pppoe_active == 0)) {
            ControlSystemData(SFIELD_SET_PPPOE_ACCOUNT, (void *)argm->value, strlen(argm->value), &req->curr_user);
        }
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_pppoepwd(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read(PPPOEPASSWORD));
        return;
    }

    do {
        if(strlen(argm->value) > sys_info->ipcam[PPPOEACCOUNT].max)
            break;

        if ( (strcmp(conf_string_read( PPPOEPASSWORD), argm->value)) || (sys_info->pppoe_active == 0) ){
            ControlSystemData(SFIELD_SET_PPPOE_PASSWORD, (void *)argm->value, strlen(argm->value), &req->curr_user);
        }
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_upnpenable(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080821
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[UPNPENABLE].config.u8);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[UPNPENABLE].max)
            break;

        if(sys_info->ipcam[UPNPENABLE].config.u8 != value)
            ControlSystemData(SFIELD_SET_UPNP_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*void set_upnpforwardingport(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080821
{
	__u16 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.net.upnpforwardport != value) {
			//ControlSystemData(SFIELD_SET_UPNP_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
			
			sysinfo_write(&value, offsetof(SysInfo, config.lan_config.net.upnpforwardport), sizeof(value), 0);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_upnprefreshtime(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080821
{
	__u8 value = atoi(argm->value);

	do {
		/*pSysInfo->config.lan_config.net.upnpenable = value;*/
		if(sys_info->ipcam[UPNPAVREFRESHTIME].config.u8 != value)
		//if(sys_info->config.lan_config.net.upnpavrefreshtime != value)
			ControlSystemData(SFIELD_SET_UPNP_REFRESHTIME, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_upnprefresh(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080821
{
//	FILE *fd;
//	char tmp[64];
	do {
/*		if((fd = fopen(UPNP_AV_PID,"r")) == NULL){
			dbg("%s file open error!\n",UPNP_AV_PID);
			break;
		}
		fgets( tmp , 16 , fd );
		fclose( fd );
		kill((pid_t)atoi( tmp ), SIGUSR1);*/
		if (!sys_info->ipcam[UPNPENABLE].config.u8)
		//if (!sys_info->config.lan_config.net.upnpenable)
			break;

		if (refresh_upnp(SIGUSR1) < 0)
			break;
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_upnpenable(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080821
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[UPNPENABLE].config.u8);
}

void set_upnpssdpage(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080822
{
	__u16 value = atoi(argm->value);

	do {
		/*pSysInfo->config.lan_config.net.upnpssdpage = value;*/
		if(sys_info->ipcam[UPNPSSDPAGE].config.u16 != value)
		//if(sys_info->config.lan_config.net.upnpssdpage != value)
			ControlSystemData(SFIELD_SET_UPNP_SSDP_AGE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_upnpssdpage(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080822
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[UPNPSSDPAGE].config.u16);
}

void set_upnpport(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080822
{
	__u16 value = atoi(argm->value);

	do {
		/*pSysInfo->config.lan_config.net.upnpport = value;*/
		if(sys_info->ipcam[UPNPPORT].config.u16!= value)
		//if(sys_info->config.lan_config.net.upnpport != value)
			ControlSystemData(SFIELD_SET_UPNP_PORT, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_upnpport(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080822
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[UPNPSSDPPORT].config.u16);
}

/*
void set_fluorescent(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080807
{
	if(sys_info->ipcam[SUPPORTFLUORESCENT].config.u8 == 0){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
			return;
	}

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NFLUORESCENT].config.u8);
		return;
	}

	__u8 value = atoi(argm->value);

	do {
        if(value > sys_info->ipcam[NFLUORESCENT].max)
            break;
        
		if(sys_info->ipcam[NFLUORESCENT].config.u8 != value)
			ControlSystemData(SFIELD_SET_FLUORESCENT, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/

void set_mirror(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080807
{
#ifdef SUPPORT_MIRROR

	if (!argm->value[0]) {
#ifdef MIRROR_FLIP_INV
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, !sys_info->ipcam[NMIRROR].config.u8);
#else
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NMIRROR].config.u8);
#endif
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

#ifdef MIRROR_FLIP_INV
        __u8 value = !atoi(argm->value);
#else
        __u8 value = atoi(argm->value);
#endif

        if(value > sys_info->ipcam[NMIRROR].max)
            break;
        
		if(sys_info->ipcam[NMIRROR].config.u8 != value)
		//if(sys_info->config.lan_config.nMirror != value)
			ControlSystemData(SFIELD_SET_MIRROR, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_flip(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20090205
{
#ifdef SUPPORT_FLIP
	
	if (!argm->value[0]) {
#ifdef MIRROR_FLIP_INV
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, !sys_info->ipcam[NFLIP].config.u8);
#else
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NFLIP].config.u8);
#endif
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

#ifdef MIRROR_FLIP_INV
			__u8 value = !atoi(argm->value);
#else
			__u8 value = atoi(argm->value);
#endif

        if(value > sys_info->ipcam[NFLIP].max)
            break;

		if(sys_info->ipcam[NFLIP].config.u8 != value)
		//if(sys_info->config.lan_config.nFlip != value)
			ControlSystemData(SFIELD_SET_FLIP, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void wm8978test(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		ControlSystemData(SFIELD_SET_WM8978, (void *)argm->value, strlen(argm->value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , argm->value );
		return;
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/

void set_muteenable(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20090618
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_IMX035 )|| defined( SENSOR_USE_TVP5150 ) || defined ( SENSOR_USE_OV2715 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 ) || defined ( SENSOR_USE_OV9715 )
	__u8 value = atoi(argm->value) ? 0 : 1;

	do {
		if(sys_info->ipcam[AUDIOINENABLE].config.u8 != value)
		//if(sys_info->config.lan_config.audioinenable != value)
			ControlSystemData(SFIELD_SET_AUDIOENABLE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}

void set_externalmicgain(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( EXTERNALMICGAIN));
		return;
	}
	
	do {
		if( strcmp( argm->value , AUDIOINGAIN_20_DB ) != 0 &&
			strcmp( argm->value , AUDIOINGAIN_26_DB ) != 0){
			break;
		}

		if( strcmp( conf_string_read( EXTERNALMICGAIN) , argm->value ) != 0 ){
			ControlSystemData(SFIELD_SET_EXTERNAL_MICINGAIN, (void *)argm->value, strlen(argm->value) + 1, &req->curr_user);
		}
		
		//info_write(argm->value, EXTERNALMICGAIN , strlen(argm->value), 0);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_lineingain(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20090618
{
//fprintf(stderr, "\n\n\n\n###API: set_lineingain = %s\n\n\n\n", argm->value);
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( LINEININPUTGAIN));
		return;
	}

#if 1 //defined( SENSOR_USE_TVP5150 ) || defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_IMX035 ) || defined ( SENSOR_USE_OV2715 ) || defined IMGS_TS2713 || defined IMGS_ADV7180
	do {
		if( strcmp( argm->value , AUDIOINGAIN_1 ) != 0 &&
			strcmp( argm->value , AUDIOINGAIN_2 ) != 0 &&
			strcmp( argm->value , AUDIOINGAIN_3 ) != 0 &&
			strcmp( argm->value , AUDIOINGAIN_4 ) != 0){
			break;
		}
		
		if( strcmp( conf_string_read( LINEININPUTGAIN) , argm->value ) != 0 ){
			ControlSystemData(SFIELD_SET_LINEINGAIN, (void *)argm->value, strlen(argm->value) + 1, &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
#error Unknown product model
#endif
}

void set_micgain(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20090618
{
#if defined( SENSOR_USE_TVP5150 ) || defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_IMX035 ) || defined ( SENSOR_USE_OV2715 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 ) || defined ( SENSOR_USE_OV9715 )
	__u8 value = 0;
//fprintf(stderr, "\n\n\n\n###API: set_micgain = %s\n\n\n\n", argm->value);
	if ((strcmp(argm->value, MICGAIN_1) == 0))		value = 0;
	else if((strcmp(argm->value, MICGAIN_2) == 0))	value = 1;
	
	do {
		if(sys_info->ipcam[MICININPUTGAIN].config.u8 != value)
		//if(sys_info->config.lan_config.micininputgain != value)
			ControlSystemData(SFIELD_SET_MICINGAIN, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}


void set_gamma(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20090204
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 ) 
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[NGAMMA].config.u8 != value)
		//if(sys_info->config.lan_config.nGamma != value)
			ControlSystemData(SFIELD_SET_GAMMA, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

/*
void set_aeshutter(request *req, COMMAND_ARGUMENT *argm)
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 )
	__u8 value = atoi(argm->value);

	do {
		if( value >= 11 ) break;
		if(sys_info->ipcam[NHSPEEDSHUTTER].config.u8 != value)
		//if(sys_info->config.lan_config.nHspeedshutter != value)
			ControlSystemData(SFIELD_SET_AESHUTTER, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
void set_aes(request *req, COMMAND_ARGUMENT *argm)
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 )
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[NAESSWITCH].config.u8 != value){
		//if(sys_info->config.lan_config.nAESswitch != value){
			ControlSystemData(SFIELD_SET_AES_SWITCH, (void *)&value, sizeof(value), &req->curr_user);
			ControlSystemData(SFIELD_SET_VIDEO_CODEC, (void *)&value, sizeof(value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_aewswitch(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do
	{
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 )
		if ( ( sys_info->ipcam[NAEWSWITCH].config.u8 != value ) || ( value == 1 ) )
		//if ( ( sys_info->config.lan_config.nAEWswitch != value ) || ( value == 1 ) )
#else
		if ( sys_info->ipcam[NAEWSWITCH].config.u8 != value )
		//if ( sys_info->config.lan_config.nAEWswitch != value )
#endif
			ControlSystemData( SFIELD_SET_AEWSWITCH, (void *) &value, sizeof( value ), &req->curr_user );
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while (0);
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}
*/
void set_isupdatecal(request *req, COMMAND_ARGUMENT *argm)
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 )  || defined( SENSOR_USE_IMX036 )  || defined( SENSOR_USE_IMX076 )  || defined( SENSOR_USE_OV2715 )  || defined( SENSOR_USE_OV9715 ) 
	char value = atoi(argm->value);
	do {
		if(sys_info->ipcam[AWBISUPDATECAL].config.u8 != value)
		//if(sys_info->config.lan_config.isupdatecal != value)
			ControlSystemData(SFIELD_SET_ISUPDATECAL, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}
void set_rgain(request *req, COMMAND_ARGUMENT *argm)
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 )  || defined( SENSOR_USE_IMX036 )  || defined( SENSOR_USE_IMX076 )  || defined( SENSOR_USE_OV2715 )  || defined( SENSOR_USE_OV9715 ) 
	int value = atoi(argm->value);
	do {
		if(sys_info->ipcam[RGAIN].config.value != value)
		//if(sys_info->config.lan_config.rgain != value)
			ControlSystemData(SFIELD_SET_RGAIN, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}
void set_bgain(request *req, COMMAND_ARGUMENT *argm)
{
#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 )  || defined( SENSOR_USE_IMX036 )  || defined( SENSOR_USE_IMX076 )  || defined( SENSOR_USE_OV2715 )  || defined( SENSOR_USE_OV9715 ) 
	int value = atoi(argm->value);
	do {
		if(sys_info->ipcam[BGAIN].config.value != value)
		//if(sys_info->config.lan_config.bgain != value)
			ControlSystemData(SFIELD_SET_BGAIN, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}

void set_aegain(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_AGC
	int i;
	if (!argm->value[0]) {
		if(EXPOSURETYPE == EXPOSUREMODE_SELECT){
			if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_INDOOR))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_INDOOR_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_OUTDOOR))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_OUTDOOR_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_NIGHT))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_NIGHT_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_MOVING))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_MOVING_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_LOW_NOISE))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_LOWNOISE_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE1))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_CUSTOMIZE1_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE2))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_CUSTOMIZE2_GAIN));
			else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_CUSTOMIZE3))
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_CUSTOMIZE3_GAIN));
			//else if(!strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_SCHEDULE))
			//	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_MODE7_GAIN));
			else 
				DBG("UNKNOWN EXPOSURE_MODE");
		}else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(NMAXAGCGAIN));
		return;
	}
	

	do {
#if defined (SUPPORT_EXPOSURETIME)
        if( sys_info->ipcam[AGCCTRL].config.u8 != 0 && (EXPOSURETYPE == EXPOSURETIME_SELECT)) {
           break;
        }
#endif

		for( i = 0 ; i < MAX_AGC; i++ ){
			if( strcmp( argm->value , sys_info->api_string.agc[i] ) == 0 ){
				ControlSystemData(SFIELD_SET_AEGAIN, (void *)argm->value, strlen(argm->value)+1, &req->curr_user);
			}
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_agcctrl(request *req, COMMAND_ARGUMENT *argm)
{
#if defined(SUPPORT_AGC) || defined(SUPPORT_EXPOSURETIME)

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AGCCTRL].config.u8);
		return;
	}

	do {
		if(EXPOSURETYPE == EXPOSUREMODE_SELECT)
			break;
		
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[AGCCTRL].max)
            break;
        
		if(sys_info->ipcam[AGCCTRL].config.u8 != value)
			info_write(&value, AGCCTRL, 0, 0);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_videoalc(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080807
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[NVIDEOALC].config.u8 != value)
		//if(sys_info->config.lan_config.nVideoalc != value)
			ControlSystemData(SFIELD_SET_VIDEOALC, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_contrast(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CONTRAST
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NCONTRAST].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_hue(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_HUE
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NHUE].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_saturation(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SATURATION
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NSATURATION].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_maxagcgain(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080807
{
//no used
#ifdef SUPPORT_MAXAGCGAIN

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(NMAXAGCGAIN));
		return;
	}

	do {
		if( strcmp( conf_string_read( NMAXAGCGAIN) , argm->value ) != 0 )
			ControlSystemData(SFIELD_SET_AGC, (void *)argm->value, HTTP_OPTION_LEN, &req->curr_user);
		
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_hspeedshutter(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080807
{
#ifdef SUPPORT_HSPEEDSHUTTER
	
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NHSPEEDSHUTTER].config.u8);
		return;
	}
    
	__u8 value = atoi(argm->value);
		
	do {
        if(value > sys_info->ipcam[NHSPEEDSHUTTER].max)
            break;
        
		if(sys_info->ipcam[NHSPEEDSHUTTER].config.u8!= value)
			ControlSystemData(SFIELD_SET_HSPEEDSHUTTER, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_white_balance(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_AWB
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( NWHITEBALANCE));
		return;
	}
	int i = 0;
	
	do {

		//modify by jiahung, 20101124
		for(i=0;i<MAX_AWB;i++){
			if(strcmp(sys_info->api_string.awb[i], argm->value) == 0){
				if( strcmp( conf_string_read( NWHITEBALANCE), argm->value ) != 0 || strcmp( argm->value, "Push Hold" ) == 0 ){					
					ControlSystemData(SFIELD_SET_WHITE_BALANCE, (void *)argm->value, strlen(argm->value) + 1, &req->curr_user);
					req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
					return;
				}
			}
		}
		break;
		
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_backlight(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BACKLIGHT
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NBACKLIGHT].config.u8);
		return;
	}

	__u8 value = atoi(argm->value);
	do {
        if(value > sys_info->ipcam[NBACKLIGHT].max)
            break;
        
		if(sys_info->ipcam[NBACKLIGHT].config.u8 != value)
			ControlSystemData(SFIELD_SET_BACKLIGHT, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_brightness(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BRIGHTNESS
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NBRIGHTNESS].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

#ifdef IMAGE_8_ORDER
		value *=  29;
#endif		
        if(value > sys_info->ipcam[NBRIGHTNESS].max)
            break;
        
		if(sys_info->ipcam[NBRIGHTNESS].config.u8 != value)
			ControlSystemData(SFIELD_SET_BRIGHTNESS, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_denoise(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DENOISE
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NDENOISE].config.u8);
		return;
	}
	
	do {
		if(numeric_check(argm->value) < 0)
			break;
	
			__u8 value = atoi(argm->value);

			if(value > sys_info->ipcam[NDENOISE].max)
				break;

			if(sys_info->ipcam[NDENOISE].config.u8 != value)
			{
				ControlSystemData(SFIELD_SET_DENOISE, (void *)&value, sizeof(value), &req->curr_user);
			}
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		} while (0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
	
}

void set_evcomp(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_EVCOMP

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NEVCOMP].config.u8);
		return;
	}
	
	do {
		if(numeric_check(argm->value) < 0)
			break;
	
		__u8 value = atoi(argm->value);

		if(value > sys_info->ipcam[NEVCOMP].max)
			break;

		if(sys_info->ipcam[NEVCOMP].config.u8 != value)
			{
				ControlSystemData(SFIELD_SET_EVCOMP, (void *)&value, sizeof(value), &req->curr_user);
			}
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		} while (0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
	
}

void set_dremode(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DREMODE

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NDREMODE].config.u8);
		return;
	}

	do {	
		if(numeric_check(argm->value) < 0)
			break;
		
		__u8 value = atoi(argm->value);

		if(value > sys_info->ipcam[NDREMODE].max)
			break;

		if(sys_info->ipcam[NDREMODE].config.u8 != value)		
			ControlSystemData(SFIELD_SET_DREMODE, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_contrast(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CONTRAST
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NCONTRAST].config.u8);
		return;
	}
	
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);
		
#ifdef IMAGE_8_ORDER
		value *=  29;
#endif

        if(value > sys_info->ipcam[NCONTRAST].max)
            break;
            
		if(sys_info->ipcam[NCONTRAST].config.u8 != value)
			ControlSystemData(SFIELD_SET_CONTRAST, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_saturation(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SATURATION
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NSATURATION].config.u8);
		return;
	}
	
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[NSATURATION].max)
            break;
        
		if(sys_info->ipcam[NSATURATION].config.u8 != value)
			ControlSystemData(SFIELD_SET_SATURATION, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_hue(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_HUE
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NHUE].config.u8);
		return;
	}

	__u8 value = atoi(argm->value);
	
	do {
        if(value > sys_info->ipcam[NHUE].max)
            break;

		if(sys_info->ipcam[NHUE].config.u8 != value)
			ControlSystemData(SFIELD_SET_HUE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_sharpness(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SHARPNESS
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NSHARPNESS].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);
		
#ifdef IMAGE_8_ORDER
		value *=  29;
#endif

        if(value > sys_info->ipcam[NSHARPNESS].max)
            break;
        
		if(sys_info->ipcam[NSHARPNESS].config.u8 != value)
		//if(sys_info->config.lan_config.nSharpness != value)
			ControlSystemData(SFIELD_SET_SHARPNESS, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}
void set_wdrenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WDRLEVEL
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[WDR_EN].config.u8);
		return;
	}    
	
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if (value > sys_info->ipcam[WDR_EN].max)
            break;
        
		if(sys_info->ipcam[WDR_EN].config.u8 != value)
			ControlSystemData(SFIELD_SET_WDR_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_wdrlevel(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WDRLEVEL
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[WDR_LEVEL].config.u8);
		return;
	}    
	
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if (value > sys_info->ipcam[WDR_LEVEL].max)
            break;
        
		if(sys_info->ipcam[WDR_LEVEL].config.u8 != value)
			ControlSystemData(SFIELD_SET_WDR_LEVEL, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_exposuretime(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_EXPOSURETIME
	int i;

	if (!argm->value[0]) {
		if(EXPOSURETYPE == EXPOSUREMODE_SELECT)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(EXPOSURE_MODE));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(A_EXPOSURE_TIME));
		return;
	}

	do {

		
#if (defined SUPPORT_AGC)
        if( (sys_info->ipcam[AGCCTRL].config.u8 != 1) && (EXPOSURETYPE == EXPOSURETIME_SELECT) ) {
           break;
        }
#endif
#if (defined SUPPORT_ANTIFLICKER)
				if((EXPOSURETYPE == EXPOSUREMODE_SELECT) && (sys_info->ipcam[ANTIFLICKER].config.u8 == 1))
					break;
#endif

		for( i = 0 ; i < MAX_EXPOSURETIME ; i++ ){
			if( strcmp( argm->value , sys_info->api_string.exposure_time[i] ) == 0 ){
				ControlSystemData(SFIELD_SET_EXPOSURE_TIME, (void *)argm->value, strlen(argm->value)+1, &req->curr_user);
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
		}

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
#if 0
int check_exposure_schedule_time(int id, EXPOSURE_TIME_EX setting)
{
	int setting_strart = (setting.start.nHour*60)+setting.start.nMin;
	int setting_end = (setting.end.nHour*60)+setting.end.nMin;
	int i, idx, tmp_start, tmp_end;
	
	if((setting.enable == 0) || (id == EXPOSURE_EX_MAX_NUM-1)) //EXPOSURE_EX_MAX_NUM-1 is "Other"
		return 0;
	
	if(setting_strart == setting_end)
		return -1;
	
	for(i=0;i<(EXPOSURE_EX_MAX_NUM-1);i++){
		
		idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*i;
		
		if(i == id)	continue;
		if(sys_info->ipcam[EXPOSURE_SCHEDULE0_EN+idx].config.value == 0)	continue;
		
		tmp_start = sys_info->ipcam[EXPOSURE_SCHEDULE0_START_HOUR+idx].config.value*60 + sys_info->ipcam[EXPOSURE_SCHEDULE0_START_MIN+idx].config.value;
		tmp_end = sys_info->ipcam[EXPOSURE_SCHEDULE0_END_HOUR+idx].config.value*60 + sys_info->ipcam[EXPOSURE_SCHEDULE0_END_MIN+idx].config.value;
		dbg("i = %d, id = %d", i, id);
		dbg("tmp_start = %d , tmp_end = %d", tmp_start , tmp_end);
		dbg("setting_strart = %d , setting_end = %d", setting_strart , setting_end);
#if 1
		if( (tmp_start < tmp_end) ){
			if((setting_strart >= tmp_start) && (setting_strart <= tmp_end) ){
				dbg("strart_time over");
				return -1;
			}
			if((setting_end >= tmp_start) && (setting_end <= tmp_end) ){
				dbg("end_time over");
				return -1;
			}
		}else if((tmp_start > tmp_end) && (setting_strart < setting_end)){
			if( (setting_strart >= tmp_start) || (setting_strart <= tmp_end)){
				dbg("===strart_time over===");
				return -1;
			}
			
			if( (setting_end >= tmp_start) || (setting_end <= tmp_end)){
				dbg("===end_time over===");
				return -1;
			}
		}else
			return -1;
		
		
			
#else		
		if((setting_strart > tmp_start) && (setting_strart < tmp_end) ){
			dbg("strart_time over");
			return -1;
		}
		if((setting_end > tmp_start) && (setting_end < tmp_end) ){
			dbg("end_time over");
			return -1;
		}
#endif
			
	}

	return 0;
}

void set_exposureschedule(request *req, COMMAND_ARGUMENT *argm)
{
#if (defined SUPPORT_EXPOSURETIME)

	int i, id;
	int idx = 0;
	EXPOSURE_TIME_EX setting;
	char *ptr = NULL;
	
	if (!argm->value[0]) {
		if(EXPOSURETYPE == EXPOSUREMODE_SELECT){
			for(i = 0; i < EXPOSURE_EX_MAX_NUM; i++) {
				idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*i;
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d:%s:%02d%02d:%02d%02d\n", 
					argm->name, i, 
					sys_info->ipcam[EXPOSURE_SCHEDULE0_EN+idx].config.value,
					conf_string_read(EXPOSURE_SCHEDULE0_MODE+idx),
					sys_info->ipcam[EXPOSURE_SCHEDULE0_START_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_START_MIN+idx].config.value,
					sys_info->ipcam[EXPOSURE_SCHEDULE0_END_HOUR+idx].config.value, sys_info->ipcam[EXPOSURE_SCHEDULE0_END_MIN+idx].config.value);
			}
		}else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
		return;
	}

	do {
		if(strcmp(conf_string_read(EXPOSURE_MODE), EXPOSURETIME_SCHEDULE) != 0)
			break;
		
		ptr = argm->value;
		for(i = 0; i < 3 ; i++) {
			if( (ptr = strchr(ptr+1, ':')) == NULL ) {
				req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
				return;
			}
		}
		*ptr = ' ';
		dbg("argm->value = %s", argm->value);
		
		if(sscanf(argm->value, "%d:%d:%s %02d%02d:%02d%02d", &id,
				&setting.enable, setting.value,
				&setting.start.nHour, &setting.start.nMin,
				&setting.end.nHour, &setting.end.nMin) != 7) {
			dbg("%s setting %s format not correct!! ", argm->name, argm->value);
			break;
		}

		if(id < 0 || id > EXPOSURE_EX_MAX_NUM) {
			dbg("%s setting id %d is over range !! ", argm->name, id);
			break;
		}

		if(check_exposure_schedule_time(id, setting) < 0){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s=1\n", argm->name);
			return;
		}
		
		idx = EXPOSURE_SCHEDULE_STRUCT_SIZE*id;
#if 0
		dbg("id = %d", id);
		dbg("setting.enable = %d", setting.enable);
		dbg("setting.value = %s", setting.value);
		dbg("setting.start.nHour = %d", setting.start.nHour);
		dbg("setting.start.nMin = %d", setting.start.nMin);
		dbg("setting.end.nHour = %d", setting.end.nHour);
		dbg("setting.end.nMin = %d", setting.end.nMin);
#endif
		info_write(&setting.enable, EXPOSURE_SCHEDULE0_EN+idx, 0, 0);
		info_write(setting.value, EXPOSURE_SCHEDULE0_MODE+idx, HTTP_OPTION_LEN, 0);
		info_write(&setting.start.nHour, EXPOSURE_SCHEDULE0_START_HOUR+idx, 0, 0);
		info_write(&setting.start.nMin, EXPOSURE_SCHEDULE0_START_MIN+idx, 0, 0);
		info_write(&setting.end.nHour, EXPOSURE_SCHEDULE0_END_HOUR+idx, 0, 0);
		info_write(&setting.end.nMin, EXPOSURE_SCHEDULE0_END_MIN+idx, 0, 0);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
		
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
#endif
void set_meteringmethod(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_METERINGMETHOD
	__u8 value=0;

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(METERINGMETHOD));
		return;
	}

	do {

			if( !strcmp( argm->value , "Average" ) ) value = METERING_AVERAGE;
			else if( !strcmp( argm->value , "Center-weighted" ) ) value = METERING_CENTER_WEIGHTED;
			else if( !strcmp( argm->value , "Spot" ) ) value = METERING_SPOT;
			else	break;
			
			ControlSystemData(SFIELD_SET_METERINGMETHOD, (void *)&value, sizeof(value), &req->curr_user);
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
			
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}


void get_sharpness(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SHARPNESS
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NSHARPNESS].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_vidcodec(request *req, COMMAND_ARGUMENT *argm)
{
    do {
      req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->procs_status.avserver);
      return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_videocodecdebug(request *req, COMMAND_ARGUMENT *argm)
{
    do {
    	if( atoi(argm->value) == 1 ){
    		sys_info->osd_debug.avserver_restart = OSD_DEBUG_ENABLE;
    		osd_init();
    	}else if( atoi(argm->value) == 0 ){
    		sys_info->osd_debug.avserver_restart = OSD_DEBUG_DISABLE;
    		osd_exit();
    	}else
    		break;
      req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
      return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_vidcodec(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = 0;
    char debug[64];
	//int spec_profile_idx = (sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value-1)*PROFILE_STRUCT_SIZE;

    if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
        sprintf(debug,"set_vidcodec sys_info->procs_status.reset_status = %d",sys_info->procs_status.reset_status);
        osd_print( 3, 12 , debug );
    }

    do {
		
      if( sys_info->procs_status.reset_status == NEED_RESET ){
        sys_info->procs_status.avserver = AV_SERVER_RESTARTING;
        ControlSystemData(SFIELD_SET_VIDEO_CODEC, (void *)&value, sizeof(value), &req->curr_user);  
      }
      req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
      return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_motionsensitivity(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].config.u16);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_motionmax(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , MOTION_LEVEL_NUM);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/*
void get_motionsentable(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		if(value < 0 || value > 4)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[MOTION_CONFIG_MOTIONSENTABLE0+value].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.motion_config.motionsentable[value]);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_motionsentable(request *req, COMMAND_ARGUMENT *argm)
{
	int value = atoi(argm->value);
	int value2 = 0;
	
	char *pp = argm->value;
	
	do {
		if(strlen(argm->value) == 0)
			break;
		if(value < 0 || value > 4)
			break;		
		pp = strchr(pp, ':' );
		pp++;
		value2 = atoi(pp);
		sys_info->ipcam[MOTION_CONFIG_MOTIONSENTABLE0+value].config.u8 = value2;
		//sys_info->config.motion_config.motionsentable[value] = value2;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
void get_motionenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8);
#else
	 req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_motion(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[MOTION_CONFIG_MOTIONCENABLE].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_motionx1(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 0);
}

void get_motiony1(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 0);
}

void get_motionx2(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 0);
}

void get_motiony2(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 0);
}

void get_motionylimit(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 480);
}

void get_motionxlimit(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 480);
}

void set_cameratitle(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(TITLE));
        return;
    }

	do {
		//if(strchr(argm->value, 34)) // character "
		//	break
		
		if(check_specical_symbols(argm->value, FILENAME_SYMBOL) < 0){
			DBG("argm->value[%s] Error", argm->value);
			break;
		}

        if(strlen(argm->value) > sys_info->ipcam[TITLE].max)
            break;
        
		if (strcmp(conf_string_read(TITLE), argm->value)) {
			ControlSystemData(SFIELD_SET_TITLE, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void cfformat(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	
	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_cfcount(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_cfcount(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_cfduration(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_cfduration(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif	
}

void get_cferror(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_cfleft(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_cfrate(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_cfrate(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_cfrecordtype(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_cfrecordtype(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_cfrenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_cfrenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_cfsize(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_cfstatus(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_cfused(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_cfaenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_cfaenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

/*
void smtptest(request *req, COMMAND_ARGUMENT *argm)
{
	int script,field = 0,ichar = 0,index = 0;
	char smtp_info[6][32],buffer[64];
	
	while( index <= strlen(argm->value) ){
		if( argm->value[index] == ':' ){
			smtp_info[field][ichar] = '\0';
			index++; field++;
			if( field >= 6 ) goto SMTP_FAIL;
			ichar = 0;
		}else{
			smtp_info[field][ichar] = argm->value[index];
			index++;  ichar++;
			if( ichar >= 32 ) goto SMTP_FAIL;
		}
	}
	smtp_info[field][ichar] = '\0';
	
	do {
		if((script = open("/etc/esmtprc_test",O_WRONLY | O_CREAT)) != -1){
			sprintf(buffer,"hostname = %s:%d\n",smtp_info[2],atoi(smtp_info[5]));
			write(script, buffer ,strlen(buffer));
			sprintf(buffer,"username = %c%s%c\n",'"',smtp_info[0],'"');
			write(script, buffer ,strlen(buffer));
			sprintf(buffer,"password = %c%s%c\n",'"',smtp_info[1],'"');
			write(script, buffer ,strlen(buffer));
			sprintf(buffer,"starttls disabled\n");
			write(script, buffer ,strlen(buffer));
			fchmod(script,S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH | S_IXOTH);
	
			close( script );
		}else	{
			DBGERR("/etc/esmtprc file open error!\n");
			// TODO:
			// dlink log example
			// ControlSystemData(SFIELD_ADD_DLINKLOG, "/etc/esmtprc file open error!\n", strlen("/etc/esmtprc file open error!\n"), NULL);
			exit(1);
		}
		sprintf(buffer,"cat /etc/testmail.txt | /opt/ipnc/esmtp -C /etc/esmtprc_test -P %d %s -f %s",atoi(smtp_info[5]),smtp_info[4],smtp_info[3] );
		if( system(buffer) == -1 ) break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
SMTP_FAIL:
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void manualmail(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	pid_t child;
	
	do {
		if( value == 1 ){
			child  = fork();
			switch(child){
				case -1:
		  		dbg("fork() error!\n");
		  		break;
		  	case  0:  //in child process
		  		execl("/opt/ipnc/appro-mail", "appro-mail" , 0);
		  		break;
		  	default:
		  		break;
		  }
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
void get_title(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , conf_string_read( TITLE));
}

void get_helpurl(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=http://www.approtech.com\n", argm->name);
}

void videomode(request *req, COMMAND_ARGUMENT *argm)
{
    BIOS_DATA bios;

    if (!argm->value[0]) {       
        if (bios_data_read(&bios) < 0)
            bios_data_set_default(&bios);
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, bios.video_system);
        return;
    }

	do {
		if (get_submcode_from_kernel() & 0xf000) {
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
			return;
		}

        if(numeric_check(argm->value) < 0)
            break;

		__u8 value = atoi(argm->value);

		if (value > VIDEO_SYSTEM_VGA)
			break;

		if (bios_data_read(&bios) < 0)
			bios_data_set_default(&bios);

		bios.video_system = value;

		if (bios_data_write(&bios, BIOS_AUTO_CORRECT) < 0)
			break;
		dbg("bios.video_system = %d\n", bios.video_system);
		ControlSystemData(SFIELD_SET_VIDEO_MODE, (void *)&value, sizeof(value), &req->curr_user);		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

#if 0
	if (argm->value[0]) {
	__u8 value = atoi(argm->value);

	do {
		if (value >= 5)
			break;
		
		if(sys_info->config.lan_config.nVideomode != value)
			ControlSystemData(SFIELD_SET_VIDEO_MODE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
	}
	else {
		/*SysInfo* pSysInfo = GetSysInfo();*/
		do {
			/*
			if(pSysInfo == NULL)
				break;
			*/
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.nVideomode);
			return;
		} while (0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
	}
#endif
}
void antiflicker(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_ANTIFLICKER
	
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[ANTIFLICKER].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);
		
		if(ControlSystemData(SFIELD_SET_ANTIFLICKER, (void *)&value, sizeof(value), &req->curr_user) < 0)
			break;
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void flickless(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FLICKLESS
	
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NVIDEOMODE].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

		if (value > VIDEO_SYSTEM_PAL)
			break;

		if( sys_info->video_type != value ){
			if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
				osd_print( 3, 4 , "powerline set reset_status");
			}
			sys_info->procs_status.reset_status = NEED_RESET;
			ControlSystemData(SFIELD_SET_FLICKLESS, (void *)&value, sizeof(value), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
void powerline(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FLICKLESS
	
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, sys_info->ipcam[NVIDEOMODE].config.u8?"50":"60");
		return;
	}
    
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 powerline = atoi(argm->value);
        __u8 value;

		if(powerline == 50)	value = 1;
		else if(powerline == 60)	value = 0;
    	else	break;
		
		if (value > VIDEO_SYSTEM_PAL)
			break;

		if( sys_info->video_type != value ){
			if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
				osd_print( 3, 4 , "powerline set reset_status");
			}

			sys_info->procs_status.reset_status = NEED_RESET;
			ControlSystemData(SFIELD_SET_FLICKLESS, (void *)&value, sizeof(value), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void getflickless(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FLICKLESS
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NVIDEOMODE].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_log_tvout_debug(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->log_tvout_debug);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
        	break;

        __u8 value = atoi(argm->value);
		if(value > 1)
			break;
        sys_info->log_tvout_debug = value;
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

#ifdef APIDEBUG
void getapihistory(request *req, COMMAND_ARGUMENT *argm)
{
	int	 ApiFd	= -1;
	struct stat api_stat;
	char api_command[600]="";
	int count = 0;
	int i;
	ApiFd = open(APILOG_FILE, O_RDONLY | O_NONBLOCK);
	if(ApiFd == -1)
		fprintf(stderr,"%s does not exist %d\n",APILOG_FILE,__LINE__);
	if(fstat(ApiFd, &api_stat) == -1)
    	fprintf(stderr,"get api.log file stat failed! %d\n",__LINE__);
	count = api_stat.st_size / sizeof(api_command);
	for(i = 0;i < count; i++)
	{
		read(ApiFd, api_command, sizeof(api_command));
		
		req->buffer_end += sprintf(req_bufptr(req), "%s<br>", api_command);
	}
	close(ApiFd);
}
#endif

#ifdef SUPPORT_VISCA
void set_vspresetpage(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
  	
  	do {
  		sys_info->visca_preset_page = value-1;
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_vspresetpage(request *req, COMMAND_ARGUMENT *argm)
{
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->visca_preset_page+1);
  	return;
}

void set_vssequencepage(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
  	
  	do {
  		sys_info->visca_sequence_page = value-1;
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_vssequencepage(request *req, COMMAND_ARGUMENT *argm)
{
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->visca_sequence_page+1);
  	return;
}

void set_vspanspeed(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_VISCA
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[VISCA_PAN_SPEED].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);
		
        if( (value < 1) || (value > sys_info->ipcam[VISCA_PAN_SPEED].max) )
            break;
        
		if(sys_info->ipcam[VISCA_PAN_SPEED].config.u8 != value)
			info_write(&value, (VISCA_PAN_SPEED), sizeof(value), 0);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_vstiltspeed(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_VISCA
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[VISCA_TILT_SPEED].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);
		
        if( (value < 1) || (value > sys_info->ipcam[VISCA_TILT_SPEED].max) )
            break;
        
		if(sys_info->ipcam[VISCA_TILT_SPEED].config.u8 != value)
			info_write(&value, (VISCA_TILT_SPEED), sizeof(value), 0);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}


void get_vsburnstatus(request *req, COMMAND_ARGUMENT *argm)
{
	if(sys_info->visca_burn_status == 0)
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, "Stop");
	else if(sys_info->visca_burn_status == 1)
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, "Running");
	else if(sys_info->visca_burn_status == 2)
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, "Fail");
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  	return;
}

void vsburntest_getdate(request *req, COMMAND_ARGUMENT *argm)
{
	static char wday_name[7][3] = {
            "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	time_t tnow;
	struct tm *tmnow;
	int string_len;

	time(&tnow);
	tmnow = localtime(&tnow);

	string_len = sprintf(req_bufptr(req), OPTION_OK "%s=%04d-%02d-%02d %.3s\n", argm->name,
			tmnow->tm_year+1900, tmnow->tm_mon+1, tmnow->tm_mday, wday_name[tmnow->tm_wday]);
	req->buffer_end += string_len;
}

void vsburntest_gettime(request *req, COMMAND_ARGUMENT *argm)
{
	time_t tnow;
	struct tm *tmnow;
	int string_len;

	time(&tnow);
	tmnow = localtime(&tnow);

	string_len = sprintf(req_bufptr(req), OPTION_OK "%s=%02d:%02d:%02d\n", argm->name,
			 		tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);
	req->buffer_end += string_len;
}

void set_vswintimezone(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIN_TIMEZONE
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NTP_TIMEZONE].config.u8);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);

        if(value < sys_info->ipcam[NTP_TIMEZONE].min || value > sys_info->ipcam[NTP_TIMEZONE].max)
            break;
        
        if(sys_info->ipcam[NTP_TIMEZONE].config.u8 != value) {
            ControlSystemData(SFIELD_SET_TIMEZONE, (void *)&value, sizeof(value), &req->curr_user);

            setenv("TZ", sys_info->tzenv, 1);
            tzset();
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_vssystem_date(request *req, COMMAND_ARGUMENT *argm)
{
	int year, month, day;
	int ret;
    
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04d_%02d_%02d\n", argm->name, 
            sys_info->tm_now.tm_year + 1900, sys_info->tm_now.tm_mon + 1, sys_info->tm_now.tm_mday );
        return;
    }

	do {
		// modified by Alex, 2009.04.14. for more compatible
		if (sscanf(argm->value, "%d%*c%d%*c%d", &year, &month, &day) != 3)
			break;
		ret = getmaxmday(year, month);		//add by jiahung, 2009.05.08

		if (ret < 0 || ret < day)
			break;	
		if (sys_set_date(year, month, day) < 0)
			break;
#ifdef MSGLOGNEW
		char loginip[20];
		char logstr[MSG_MAX_LEN];
		char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
		strcpy(loginip, ipstr(req->curr_user.ip));
		sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
		snprintf(logstr, MSG_USE_LEN, "%s SET DATE Year %04d Month %02d Day %02d", vistor, year, month, day);
		sprintf(logstr, "%s\r\n", logstr);
		sysdbglog(logstr);
#endif
		// no more used. 2010.07.12 Alex.
		//ControlSystemData(SFIELD_SET_DATE, argm->value, strlen(argm->value), &req->curr_user);

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_vssystem_time(request *req, COMMAND_ARGUMENT *argm)
{
	int hour, min, sec;

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%02d:%02d:%02d\n", argm->name, 
            sys_info->tm_now.tm_hour, sys_info->tm_now.tm_min, sys_info->tm_now.tm_sec );
        return;
    }

	do {
		if (sscanf(argm->value, "%d:%d:%d", &hour, &min, &sec) != 3)
			break;

		if (sys_set_time(hour, min, sec) < 0)
			break;

		ControlSystemData(SFIELD_SET_TIME, argm->value, strlen(argm->value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void restart_visca_ipcam(request *req, COMMAND_ARGUMENT *argm)
{
    if(atoi(argm->value) != 1) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
#ifndef MSGLOGNEW
	SYSLOG tlog;
#endif
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);

#ifdef SUPPORT_AF
	SetMCUWDT(0);
#endif
	req->req_flag.reboot = 1;
#ifdef MSGLOGNEW
	char loginip[20];
	char logstr[MSG_MAX_LEN];
	char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
	strcpy(loginip,ipstr(req->curr_user.ip));
	sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
	snprintf(logstr, MSG_USE_LEN, "%s Reboot Device", vistor);
	sprintf(logstr, "%s\r\n", logstr);
	sysdbglog(logstr);
#else
	tlog.opcode = SYSLOG_SYSSET_NETREBOOT;
	tlog.ip = req->curr_user.ip;
	strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
	SetSysLog(&tlog);
#endif
}

void set_visca_factory_default(request *req, COMMAND_ARGUMENT *argm)
{   
	do{
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

#ifndef SUPPORT_FACTORYMODE
		if(value != 1)
            break;
#ifdef SUPPORT_VISCA
		if( remove(SEQUENCE_SAVE) != 0)
          	dbg("remove %s failed %d\n", SEQUENCE_SAVE,__LINE__);				 			
#endif
		ControlSystemData(SFIELD_FACTORY_DEFAULT, &value, sizeof(value), &req->curr_user);
		
#else
        if(!(value & FLAG_LOAD_ALLDEF))
			break;

		dbg("value = %d\n", value);
		if(ControlSystemData(SFIELD_FACTORY_DEFAULT, &value, sizeof(value), &req->curr_user) < 0){
			DBG("SFIELD_FACTORY_DEFAULT ERROR\n");
			break;
		}
#endif

#ifdef SUPPORT_AF
		SetMCUWDT(0);
#endif
 		req->req_flag.reboot = 1;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
        
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_vs_giooutalwayson(request *req, COMMAND_ARGUMENT *argm)
{
    int id, enable;
    __u32 value = 0;

    if(GIOOUT_NUM == 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }

    if (!argm->value[0]) {
        int i;
        for ( i = 1; i <= GIOOUT_NUM; i++ ) {
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", 
                    argm->name, i, sys_info->ipcam[GIOOUT_ALWAYSON0+i].config.u16);
        }
        return;
    }

    do {
        if(strchr(argm->value, ':')) {
            if(sscanf(argm->value, "%d:%d", &id, &enable) != 2)
                break;
        }
        else {
            id = 1;
            enable = atoi(argm->value);
        }
        
        if(id < 1 || id > GIOOUT_NUM)
            break;

        if(enable < sys_info->ipcam[GIOOUT_ALWAYSON0+id].min || enable > sys_info->ipcam[GIOOUT_ALWAYSON0+id].max)
            break;
        
        if(sys_info->ipcam[GIOOUT_ALWAYSON0+id].config.u16 != enable) {

            value = (id << 16) | enable;
            if(ControlSystemData(SFIELD_SET_GIOOUT_ALWAYSON, &value, sizeof(value), &req->curr_user) < 0)
                break;
        }
            
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_vs_dlink_alarm_status(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned int status = 0;
	
	do{
		if (sys_info->status.alarm8)
			status |= 0x4000;
		if (sys_info->status.alarm7)
			status |= 0x2000;
		if(sys_info->status.fan_control)
			status |= 0x1000;
		if(sys_info->status.fan_temperature)
			status |= 0x800;
		if(sys_info->status.fan_healthy)
			status |= 0x400;
		if (sys_info->status.alarm6)
			status |= 0x200;
		if (sys_info->status.alarm5)
			status |= 0x100;		
		if (sys_info->status.alarm4)
			status |= 0x80;
		if (sys_info->status.alarm3)
			status |= 0x40;		
		if (sys_info->gioout_time)
			status |= 0x20;
		if (sys_info->ipcam[GIOOUT_ALWAYSON1].config.u16)
		//if (sys_info->config.lan_config.gioout_alwayson[1])
			status |= 0x10;
		if (sys_info->status.on_record)
			status |= 0x08;
		if (sys_info->status.motion)
			status |= 0x04;
		if (sys_info->status.alarm2)
			status |= 0x02;
		if (sys_info->status.alarm1)
			status |= 0x01;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04x\n", argm->name, status);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_vs_mac(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;
//	char buffer[80];
	
	do {
		if (bios_data_read(&bios) < 0)
			bios_data_set_default(&bios);
		if (hex_to_mac(bios.mac0, argm->value) < 0)
			break;

		if (!is_valid_ether_addr(bios.mac0)) {
			dbg("Invalid ethernet MAC address.\n");
			break;
		}
		
		if (bios_data_write(&bios, BIOS_AUTO_CORRECT) < 0)
			break;
#if 0
		sprintf(buffer, "ifconfig eth0 down hw ether %02X:%02X:%02X:%02X:%02X:%02X", bios.mac0[0],bios.mac0[1],bios.mac0[2],bios.mac0[3],bios.mac0[4],bios.mac0[5]);
		system(buffer);
		system("ifconfig eth0 up");
#else
		req->req_flag.set_mac = 1;
#endif
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

#endif

void power_saving_mode(request *req, COMMAND_ARGUMENT *argm)
{
	__u32 power_save = ((sys_info->ipcam[POWER_MODE].config.value & POWER_MODE_SAVING_ALL) == POWER_MODE_SAVING_ALL);
	//__u32 power_save = ((sys_info->config.power_mode & POWER_MODE_SAVING_ALL) == POWER_MODE_SAVING_ALL);

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, power_save);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

		__u32 value = atoi(argm->value);

		if (value > sys_info->ipcam[POWER_MODE].max)
			break;

		if(power_save != value) {
			int power_mode = sys_info->ipcam[POWER_MODE].config.value;
			//int power_mode = sys_info->config.power_mode;
			if (value)
				power_mode |= POWER_MODE_SAVING_ALL;
			else
				power_mode &= ~POWER_MODE_SAVING_ALL;
			ControlSystemData(SFIELD_SET_SAVING_MODE, (void *)&power_mode, sizeof(power_mode), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void analog_output(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_TVOUT_OFF
	__u32 analog_out = ((sys_info->ipcam[POWER_MODE].config.value & POWER_MODE_ANALOG_OUT) == POWER_MODE_ANALOG_OUT);
	//__u32 analog_out = ((sys_info->config.power_mode & POWER_MODE_ANALOG_OUT) == POWER_MODE_ANALOG_OUT);

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, analog_out);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

		__u32 value = atoi(argm->value);
		
		if (value > sys_info->ipcam[POWER_MODE].max)
			break;

		if(analog_out != value) {
			int power_mode = sys_info->ipcam[POWER_MODE].config.value;
			//int power_mode = sys_info->config.power_mode;
			if (value)
				power_mode |= POWER_MODE_ANALOG_OUT;
			else
				power_mode &= ~POWER_MODE_ANALOG_OUT;
			ControlSystemData(SFIELD_SET_SAVING_MODE, (void *)&power_mode, sizeof(power_mode), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_videomode(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;

	if (bios_data_read(&bios) < 0)
		bios_data_set_default(&bios);

	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, bios.video_system);
}

//SDK 0.7 old API
void set_imageformat(request *req, COMMAND_ARGUMENT *argm)
{
	char tmp[HTTP_OPTION_LEN] = "";
	int id = 0;
	int value = 0;
	__u8 restart = 1;
	
	do {	
		if(strchr(argm->value, ':')) {
			if(sscanf(argm->value, "%d:%d", &id, &value) != 2){
				req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
				return;
			}
			id -= 1;
		}
		else {
			id = 0;
			value = atoi(argm->value);
		}

		dbg("%d:%d\n", id, value);

		if(profile_check(id+1) < EXIST_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}

		switch(value)
		{
				case 0:
					strcpy(tmp, JPEG_PARSER_NAME);
					break;
				case 1:
					strcpy(tmp, MPEG4_PARSER_NAME);
					break;
				case 2:
					strcpy(tmp, H264_PARSER_NAME);
					break;
				default:
					req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
					return;
		}
		//sysinfo_write(tmp, offsetof(SysInfo, config.profile_config[0].codecname), HTTP_OPTION_LEN, 0);
		info_write(tmp, PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*id, HTTP_OPTION_LEN, 0);
		ControlSystemData(SFIELD_SET_VIDEO_CODEC, (void *)&restart, sizeof(restart), &req->curr_user);
		
		//if(sys_info->config.lan_config.net.imageformat != value)
			//ControlSystemData(SFIELD_SET_IMAGEFORMAT, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
//SDK 0.7 old API
void set_resolution(request *req, COMMAND_ARGUMENT *argm)
{
	int x, id = 0;
	char value[HTTP_OPTION_LEN] = "";
	__u8 restart = 1;

	do {
		if(strchr(argm->value, ':')) {
			sscanf(argm->value, "%d:%s", &id, value);
			if(strcmp(value, "") == 0){
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(PROFILE_CONFIG0_RESOLUTION+PROFILE_STRUCT_SIZE*(id-1)));
				return;
			}
		}else
			break;
		
		id -= 1;
		if(profile_check(id+1) < EXIST_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		dbg("%d:%s\n", id, value);
		
		for( x = 0 ; x < MAX_RESOLUTIONS ; x++){
			if( strcmp( value , sys_info->api_string.resolution_list[x] ) == 0 ){

				sys_info->ipcam[OLD_PROFILE_SIZE0_XSIZE+2*id].config.value = sys_info->ipcam[PROFILESIZE0_XSIZE+2*id].config.value;
				sys_info->ipcam[OLD_PROFILE_SIZE0_YSIZE+2*id].config.value = sys_info->ipcam[PROFILESIZE0_YSIZE+2*id].config.value;
				if( strcmp( conf_string_read( PROFILE_CONFIG0_RESOLUTION+PROFILE_STRUCT_SIZE*id) , value ) != 0 ){
					info_write(value, PROFILE_CONFIG0_RESOLUTION+PROFILE_STRUCT_SIZE*id, HTTP_OPTION_LEN, 0);
					ControlSystemData(SFIELD_SET_VIDEO_CODEC, (void *)&restart, sizeof(restart), &req->curr_user);
				}
	
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				return;
			}
		}

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_liveresolution(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 3);
}
//SDK 0.7 old API
void get_livequality(request *req, COMMAND_ARGUMENT *argm)
{
	int id = atoi(argm->value) - 1;

	if(profile_check(id+1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	do {
		if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*id) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*id) , H264_PARSER_NAME ) == 0)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY+PROFILE_STRUCT_SIZE*id].config.u8);
		else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*id) , JPEG_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY+PROFILE_STRUCT_SIZE*id].config.u8);
		else
			break;

		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
//SDK 0.7 old API
void set_quality(request *req, COMMAND_ARGUMENT *argm)
{
	int id , value;
	__u8 video_reset = 1;
	
	do {
		if(strchr(argm->value, ':')) {
			if(sscanf(argm->value, "%d:%d", &id, &value) != 2){			
				req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
				return;
			}
		}else
			break;
		
		id -= 1;

		dbg("%d:%d\n", id, value);

		if(profile_check(id+1) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}

		if( strcmp( conf_string_read( PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*id) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read( PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*id) , H264_PARSER_NAME ) == 0){
			if( sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY+PROFILE_STRUCT_SIZE*id].config.u8 != value ){
				info_write(&value, PROFILE_CONFIG0_FIXED_QUALITY+PROFILE_STRUCT_SIZE*id, sizeof(value), 0);
				ControlSystemData(SFIELD_SET_VIDEO_CODEC, (void *)&video_reset, sizeof(video_reset), &req->curr_user);
			}
		}else{
			if( sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY+PROFILE_STRUCT_SIZE*id].config.u8 != value ){
				info_write(&value, PROFILE_CONFIG0_JPEG_QUALITY+PROFILE_STRUCT_SIZE*id, sizeof(value), 0);
				ControlSystemData(SFIELD_SET_VIDEO_CODEC, (void *)&video_reset, sizeof(video_reset), &req->curr_user);
			}
				
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_localaic(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,"WM8978G");
}

void get_localdecoder(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,"NONE");
}

void get_localflash(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,"Samsung NAND 32MiB 3,3V 8-bit");
}

void get_localnic(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,"DM9000AE");
}

void get_localsdram(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,"K4T1G164QD-ZCE6");
}

void get_localsram(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name ,"NONE");
}
#if 0
void set_mpeg4cenable1(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.net.mpeg4cenable1 != value)
			sys_info->config.lan_config.net.mpeg4cenable1 = value;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_mpeg4cenable2(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.net.mpeg4cenable2 != value)
			sys_info->config.lan_config.net.mpeg4cenable2 = value;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_quality1mpeg4(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	int value1;

	do {
		if(sys_info->config.lan_config.net.quality1mpeg4 != value){
			sys_info->config.lan_config.net.quality1mpeg4 = value;
			if(value == 0)
				value1 = 7626;
			else if(value == 1)
				value1 = 6152;
			else if(value == 2)
				value1 = 4678;
			else if(value == 3)
				value1 = 3204;
			else
				value1 = 1730;
			value1 *= 1000;
		  fprintf(stderr,"MP41bitrate : %d \n", value1);
			if (ControlSystemData(SFIELD_SET_MPEG41_BITRATE, (void *)&value1, sizeof(value1), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_quality2mpeg4(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	int value1;

	do {
		if(sys_info->config.lan_config.net.quality2mpeg4 != value){
			sys_info->config.lan_config.net.quality2mpeg4 = value;
			if(value == 0)
				value1 = 7626;
			else if(value == 1)
				value1 = 6152;
			else if(value == 2)
				value1 = 4678;
			else if(value == 3)
				value1 = 3204;
			else
				value1 = 1730;
			value1 *= 1000;
		  fprintf(stderr,"MP42bitrate : %d \n", value1);
			if (ControlSystemData(SFIELD_SET_MPEG42_BITRATE, (void *)&value1, sizeof(value1), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_mpeg4resolution(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
	
	__u8 value = atoi(argm->value);

	do {
		
		if(sys_info->config.lan_config.net.mpeg4resolution != value)
			ControlSystemData(SFIELD_SET_MPEG4_RES, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

void get_mpeg4xsize(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.Mpeg41.xsize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_mpeg4ysize(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.Mpeg41.ysize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_schedule(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		if(ControlSystemData(SFIELD_SET_SCHEDULE, argm->value, strlen(argm->value), &req->curr_user) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void del_schedule(request *req, COMMAND_ARGUMENT *argm)
{
	int bEnable = atoi(argm->value);
	do {
		if(ControlSystemData(SFIELD_DEL_SCHEDULE, &bEnable, sizeof(bEnable), &req->curr_user) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_mpeg42resolution(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	do {
		if(sys_info->config.lan_config.net.mpeg42resolution != value)
			ControlSystemData(SFIELD_SET_MPEG42_RES, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_lostalarm(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned char bEnable = atoi(argm->value);
	do {
		if(sys_info->ipcam[LOSTALARM].config.u8!= bEnable)
		//if(sys_info->config.lan_config.lostalarm != bEnable)
			if(ControlSystemData(SFIELD_SET_LOSTALARM, &bEnable, sizeof(bEnable), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_prealarm(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned char value = atoi(argm->value);
	do {
		if(sys_info->config.ftp_config.prealarm != value)
			if(ControlSystemData(SFIELD_SET_PREALARM, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_postalarm(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned char value = atoi(argm->value);
	do {
		if(sys_info->config.ftp_config.postalarm != value)
			if(ControlSystemData(SFIELD_SET_POSTALARM, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif

void undefined(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
}
void af_cnt(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%d\n", sys_info->af_status.cnt);
}
void af_test(request *req, COMMAND_ARGUMENT *argm)
{
	if(!argm->value[0]){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->af_status.test_time);
		return;
	}
	__u8 value = atoi(argm->value);
	sys_info->af_status.test_time = value;
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
}
void get_machinetype(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef MACHINE_TYPE_LANCAM
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#elif	defined (MACHINE_TYPE_VIDEOSERVER)
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
#error Must define MACHINE_TYPE into Lancam or Video Server
#endif

}

void get_supportmaskarea(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MASKAREA
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void set_maskarea(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MASKAREA	
	char value[16];
	strcpy(value , argm->value);

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( MASKAREA0_MASKAREA_CMD ));
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( MASKAREA1_MASKAREA_CMD ));
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( MASKAREA2_MASKAREA_CMD ));
		return;
	}

	do {

		if(strcmp(conf_string_read( MASKAREA0_MASKAREA_CMD+MASKAREA_STRUCT_SIZE*(value[2]-'0')), value ) != 0)
		//if(strcmp(sys_info->config.lan_config.net.maskarea[value[2]-'0'].maskarea_cmd , value ) != 0)
			if(ControlSystemData(SFIELD_SET_MASKAREA, value, sizeof(value), &req->curr_user) < 0)
				break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_eptzcoordinate(request *req, COMMAND_ARGUMENT *argm)
{
	int profileID = argm->value[0] - 48;
	
	if(profile_check(profileID) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	do {
		//if(strcmp(sys_info->config.profile_config[0].eptzCoordinate , argm->value ) != 0)
		if( profileID == 1 ){
			if(strcmp(conf_string_read(PROFILE_CONFIG0_EPTZCOORDINATE) , argm->value ) != 0)
				if(ControlSystemData(SFIELD_SET_EPTZ_COORDINATE, argm->value, strlen(argm->value)+1, &req->curr_user) < 0)
					break;
		}else if( profileID == 2 ){
			if(strcmp(conf_string_read(PROFILE_CONFIG1_EPTZCOORDINATE) , argm->value ) != 0)
				if(ControlSystemData(SFIELD_SET_EPTZ_COORDINATE, argm->value, strlen(argm->value)+1, &req->curr_user) < 0)
					break;
		}else if( profileID == 3 ){
			if(strcmp(conf_string_read(PROFILE_CONFIG2_EPTZCOORDINATE) , argm->value ) != 0)
				if(ControlSystemData(SFIELD_SET_EPTZ_COORDINATE, argm->value, strlen(argm->value)+1, &req->curr_user) < 0)
					break;
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_eptzwindowsize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		//if(strcmp(sys_info->config.profile_config[0].eptzWindowSize , argm->value ) != 0)
		if(strcmp(conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE) , argm->value ) != 0)
			if(ControlSystemData(SFIELD_SET_EPTZ_WINDOWSIZE, argm->value, strlen(argm->value)+1, &req->curr_user) < 0)
				break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}


void set_maskareaenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MASKAREA

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[MASKAREAENABLE].config.u8 );
		return;
	}

	
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);
        
        if(value > sys_info->ipcam[MASKAREAENABLE].max)
            break;
        
		if(sys_info->ipcam[MASKAREAENABLE].config.u8 != value)
			ControlSystemData(SFIELD_SET_MASKAREA_ENABLE, (void *)&value, sizeof(value), &req->curr_user);
		//if(sys_info->config.lan_config.net.maskareaenable != value)
			//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.net.maskareaenable), sizeof(value), 0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_supportonvif(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_ONVIF
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportflickless(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FLICKLESS
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportpowerline(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FLICKLESS
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportagc(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_AGC
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportawb(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_AWB
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportbacklight(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BACKLIGHT
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportblcmode(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BLCMODE
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportbrightness(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BRIGHTNESS
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportdenoise(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DENOISE
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportevcomp(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_EVCOMP
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportdre(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DREMODE
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportcapturepriority(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CAPTUREPRIORITY
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportcfcard(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CFCARD
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportcolorkiller(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_COLORKILLER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_maxchannel(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 1);
}

void get_maxregister(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , ACOUNT_NUM);
}

void get_minnamelen(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 3);
}

void get_supportcontrast(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_CONTRAST
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportds1302(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DS1302
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportexposure(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_EXPOSURETIME
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportexposuretimemode(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_EXPOSURETIME
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, EXPOSURETYPE);
#endif
}

void get_supportfluorescent(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FLUORESCENT
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportfs(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FS
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supporth3a(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORTH3A
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif

}

void get_armhz(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , get_arm_clock());
}

void get_sdramhz(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , get_ddr_clock());
}

void get_serialnumber(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , "00-00-00-00-00-00");
}

void get_setupversion(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , "1.0");
}

void get_softwareversion(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d.%02d\n", argm->name , APP_VERSION_MAJOR, APP_VERSION_MINOR);
}

void get_firmwareversion(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d.%02d.%02d\n", argm->name , APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_SUB_MINOR);
}

void get_uartspeed(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 7);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0
void set_biosmode(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		/*
		if( pSysInfo == NULL )
			break;
		*/
		sys_info->config.lan_config.net.imagesource = value;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void get_biosversion(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , get_bios_version());
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_supporthspeedshutter(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_HSPEEDSHUTTER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supporthue(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_HUE
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportkelvin(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_KELVIN
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportluminance(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_LUMINANCE
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportmaxagcgain(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MAXAGCGAIN
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportmirror(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MIRROR
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportflip(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_FLIP
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportprotect(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_PROTECT
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_supportrs485(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void rs485_delay(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_485enable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RS485_ENABLE].config.u8);
        return;
    }
    
	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[RS485_ENABLE].max)
            break;
		
		if(sys_info->ipcam[RS485_ENABLE].config.u8 != value) {

			if(value == 1) {
				if(rs485_init(sys_info) < 0)
					break;
			}
			else {
				rs485_free();
			}
			
			if(ControlSystemData(SFIELD_SET_485_ENABLE, &value, sizeof(value), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_485protocol(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RS485_PROTOCOL].config.u8);
        return;
    }
    
	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[RS485_PROTOCOL].max)
            break;
        
		if(sys_info->ipcam[RS485_PROTOCOL].config.u8 != value) {
			if(ControlSystemData(SFIELD_SET_485_PROTOCOL, &value, sizeof(value), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_485data(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RS485_DATA].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[RS485_DATA].max)
            break;

		if(sys_info->ipcam[RS485_DATA].config.u8 != value) {

			if(rs485_set_data(value) < 0)
				break;
			
			if(ControlSystemData(SFIELD_SET_485_DATA, &value, sizeof(value), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_485stop(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RS485_STOP].config.u8);
        return;
    } 
	
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[RS485_STOP].max)
            break;

		if(sys_info->ipcam[RS485_STOP].config.u8 != value) {

			if(rs485_set_stop(value) < 0)
				break;
			
			if(ControlSystemData(SFIELD_SET_485_STOP, &value, sizeof(value), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_485parity(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RS485_PARITY].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[RS485_PARITY].max)
            break;
        
		if(sys_info->ipcam[RS485_PARITY].config.u8 != value) {

			if(rs485_set_parity(value) < 0)
				break;
			
			if(ControlSystemData(SFIELD_SET_485_PARITY, &value, sizeof(value), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
void set_485type(request *req, COMMAND_ARGUMENT *argm)
{
  __u8 value = atoi(argm->value);
  
  if(sys_info->ipcam[SUPPORTRS485].config.u8 == 0){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
		return;
	}
  
  do {

	  switch(value) {
	  	default:
	  	case 0 : rs485_set_data(1); rs485_set_parity(0); rs485_set_stop(1); break;  // 8-N-1
	  	case 1 : rs485_set_data(1); rs485_set_parity(1); rs485_set_stop(1); break;  // 8-E-1
	  	case 2 : rs485_set_data(1); rs485_set_parity(2); rs485_set_stop(1); break;  // 8-O-1
	  	case 3 : rs485_set_data(0); rs485_set_parity(0); rs485_set_stop(1); break;  // 7-N-1
	  	case 4 : rs485_set_data(0); rs485_set_parity(1); rs485_set_stop(1); break;  // 7-E-1
	  	case 5 : rs485_set_data(0); rs485_set_parity(2); rs485_set_stop(1); break;  // 7-O-1
	  }

	  req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	  return;
  } while (0);
  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_485id(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RS485_ID].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[RS485_ID].max)
            break;

		if(sys_info->ipcam[RS485_ID].config.u8 != value )
			if(ControlSystemData(SFIELD_SET_485_ID, &value, sizeof(value), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_485speed(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RS485_SPEED].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[RS485_SPEED].max)
            break;

		if(sys_info->ipcam[RS485_SPEED].config.u8 != value) {

			if(rs485_set_speed(value) < 0)
				break;
			
			if(ControlSystemData(SFIELD_SET_485_SPEED, &value, sizeof(value), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/   
void rs485_write(request *req, COMMAND_ARGUMENT *argm)
{	
#ifdef SUPPORT_RS485

	do {
		if(sys_info->ipcam[RS485_ENABLE].config.u8 == 0)
			break;

		sys_info->osd_uart = 1;
		if(rs485_write_data(argm->value) == -1)
			break;

//		sigio_flag = 0;
//		system("cat "RS485_DEVICE);
	
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void rs485_output(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485

	do {
		if( sys_info->ipcam[RS485_ENABLE].config.u8 == 0)
			break;

		sys_info->osd_uart = 1;
		if(rs485_write_data(argm->value) == -1)
			break;

//		sigio_flag = 0;
//		system("cat "RS485_DEVICE);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0 /* 'DISABLE' by Alex, 2008.12.25  */ 

void rs485_read(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		if( /*pSysInfo == NULL ||*/ sys_info->config.lan_config.net.supportrs485 == 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}


void get_485connect(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	
	do {
		if( /*pSysInfo == NULL ||*/ sys_info->config.lan_config.net.supportrs485 == 0)
			break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_485id(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.rs485.id);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_485speed(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.rs485.speed);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_485type(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.rs485.type);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif  /* 'DISABLE' by Alex, 2008.12.25  */
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_createpreset(request *req, COMMAND_ARGUMENT *argm)
{
#if defined (SUPPORT_RS485) && defined(CONFIG_BRAND_DLINK)
	char buf[64];
	int preset_point = 0;
	char name_tmp[MAX_PTZPRESETNAME_LEN];
	char* ptr;
	int addr = 0;
	int i;
	//input fomat is preset_point:preset_name
	ptr = argm->value;
	strcpy(buf, argm->value);
	
	//dbg("buf = %s\n", buf);
	preset_point = atoi(buf);
	//dbg("index_tmp = %d\n", index_tmp);
	
	ptr = strtok(buf, ":");
	bzero(name_tmp, sizeof(name_tmp));
	if(preset_point > 9 ) {
		//dbg("ptr = %s\n", ptr+3);
		strcpy(name_tmp, ptr+3);
	}
	else {
		//dbg("ptr = %s\n", ptr+2);
		strcpy(name_tmp, ptr+2);
	}
	//dbg("name_tmp = %s\n", name_tmp);	
		
	do {
		for(i = 0 ; i < MAX_PTZPRESET_NUM ; i++){
			if(strlen(conf_string_read( PRESETPOSITION0_NAME+i)) <= 0 ){
    		//if(strlen(sys_info->config.ptz_config.presetposition[i].name) <= 0 ){
    			//dbg("i=%d", i);
    			addr = i;
    			break;
    		}
    	}
		if((preset_point <= MAX_PTZPRESET_NUM) && (preset_point != 0) && strlen(name_tmp) < MAX_PTZPRESETNAME_LEN){
			//dbg("preset_point = %d\n", preset_point);	
			//dbg("preset_name = %s\n", name_tmp);	
			info_write(&preset_point, PRESETPOSITION0_INDEX+addr, 0, 0);
			info_write(name_tmp, PRESETPOSITION0_NAME+addr, strlen(name_tmp), 0);
			//sysinfo_write(&preset_point, offsetof(SysInfo, config.ptz_config.presetposition[addr].index), sizeof(preset_point), 0);
			//sysinfo_write(&name_tmp, offsetof(SysInfo, config.ptz_config.presetposition[addr].name), sizeof(name_tmp), 0);
		}else
			break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
void set_deletepreset(request *req, COMMAND_ARGUMENT *argm)
{
#if defined (SUPPORT_RS485) && defined(CONFIG_BRAND_DLINK)

	int del_index = 0, index_clr = 0;
	int i, addr=-1, preset_num = 0;
	char name_clr[MAX_PTZPRESETNAME_LEN]="";
	
	del_index = atoi(argm->value);
	//dbg("input_index = %d\n", del_index);
	if(del_index > MAX_PTZPRESET_NUM )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		
	do {
		for(i = 0 ; i < MAX_PTZPRESET_NUM ; i++){

			if(strlen(conf_string_read( PRESETPOSITION0_NAME+i)) != 0 ){		
    		//if(strlen(sys_info->config.ptz_config.presetposition[i].name) != 0 ){		
    			if(preset_num == del_index){
    				//dbg("i=%d", i);
    				addr = i;
    				break;
    			}else
    				preset_num++;
    		}
    	}
		
		if(addr == -1)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		
    	//dbg("addr = %d\n", addr);
    	//dbg("del_index = %d\n", del_index);
		if( addr < MAX_PTZPRESET_NUM ){	
			info_write(&index_clr, PRESETPOSITION0_INDEX+addr, 0, 0);
			info_write(name_clr, PRESETPOSITION0_NAME+addr, sizeof(name_clr), 0);
			//sysinfo_write(&index_clr, offsetof(SysInfo, config.ptz_config.presetposition[addr].index), sizeof(index_clr), 0);
			//sysinfo_write(&name_clr, offsetof(SysInfo, config.ptz_config.presetposition[addr].name), sizeof(name_clr), 0);
		}else
			break;
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void ledmode(request *req, COMMAND_ARGUMENT *argm)
{
#if (SUPPORT_LED == 1)

  if (!argm->value[0]) {
      req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[LED_STATUS].config.u8);
      return;
  }

  __u8 value = atoi(argm->value);
  
  do {
        if(value > sys_info->ipcam[LED_STATUS].max)
            break;
        
        if(sys_info->ipcam[LED_STATUS].config.u8 != value){
            info_write(&value, LED_STATUS, 0, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char *logpara;
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip, ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			logpara = (value == TRUE ? "TURN ON" : "TURN OFF");
			snprintf(logstr, MSG_USE_LEN, "%s %s LED", vistor, logpara);
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
        }
        
      req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[LED_STATUS].config.u8);
      return;
  } while (0);
  
  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
  req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_rtspport(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[RTSP_PORT].config.u16);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u16 value = atoi(argm->value);

        if(value > sys_info->ipcam[RTSP_PORT].max)
            break;

		if(value == sys_info->ipcam[HTTP_PORT].config.u16) 
            break;
		
#ifdef SERVER_SSL
		if(value == sys_info->ipcam[HTTPS_PORT].config.u16) 
            break;
#endif
		
        if(sys_info->ipcam[RTSP_PORT].config.u16 != value) {
            info_write(&value, RTSP_PORT, 0, 0);
            system("killall -SIGTERM rtsp-streamer\n");
            system("/opt/ipnc/rtsp-streamer &\n");
        }

      req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
      return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
#if 0 // NO MORE USED, Alex. 2009.12.15
void smb_enrecord(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	__u8 enable = atoi(argm->value);
	
	do {
		if(sys_info->config.samba.enable_rec != enable)
			if(ControlSystemData(SFIELD_SET_SMB_ENRECORD, &enable, sizeof(enable), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_authority(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	__u8 value = atoi(argm->value);
	
	do {
		if(sys_info->config.samba.authority != value)
			if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_username(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	do {

		if (strcmp(sys_info->config.samba.username, argm->value)) {
			ControlSystemData(SFIELD_SET_SMB_USERNAME, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_password(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	do {

		if (strcmp(sys_info->config.samba.password, argm->value)) {
			ControlSystemData(SFIELD_SET_SMB_PASSWORD, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_server(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	do {

		if (strcmp(sys_info->config.samba.server, argm->value)) {
			ControlSystemData(SFIELD_SET_SMB_SERVER, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_path(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	do {

		if (strcmp(sys_info->config.samba.path, argm->value)) {
			ControlSystemData(SFIELD_SET_SMB_PATH, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_status(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	do {

		//if (strcmp(sys_info->config.samba.path, argm->value)) {
		//	ControlSystemData(SFIELD_SET_SMB_PATH, (void *)argm->value, strlen(argm->value), &req->curr_user);
		//}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_profile(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	do {

		//if (strcmp(sys_info->config.samba.path, argm->value)) {
		//	ControlSystemData(SFIELD_SET_SMB_PATH, (void *)argm->value, strlen(argm->value), &req->curr_user);
		//}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/ 
void smb_rewrite(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	do {

		//if (strcmp(sys_info->config.samba.path, argm->value)) {
		//	ControlSystemData(SFIELD_SET_SMB_PATH, (void *)argm->value, strlen(argm->value), &req->curr_user);
		//}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *																		   *
 ***************************************************************************/
void smb_rectype(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_SAMBA
	//__u8 value = atoi(argm->value);
	
	do {
		//if(sys_info->config.trigger[TRIGGER_AVI].en_motion != value)
		  //sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_AVI].en_motion), sizeof(value), 0);
			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
			//	break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void trg_outmotion(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	
	do {
		if(sys_info->config.trigger[TRIGGER_ALARM].en_motion != value)
		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_ALARM].en_motion), sizeof(value), 0);
			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
			//	break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void trg_outgioin1(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	
	do {
		if(sys_info->config.trigger[TRIGGER_ALARM].en_alarm1 != value)
		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_ALARM].en_alarm1), sizeof(value), 0);
			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
			//	break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void trg_outgioin2(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	
	do {
		if(sys_info->config.trigger[TRIGGER_ALARM].en_alarm2 != value)
		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_ALARM].en_alarm2), sizeof(value), 0);
			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
			//	break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}


#ifdef MODEL_LC7315
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void trg_jpgmotion(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.trigger[TRIGGER_JPG].en_motion != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_JPG].en_motion), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void trg_jpggioin1(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.trigger[TRIGGER_JPG].en_alarm1 != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_JPG].en_alarm1), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void trg_jpggioin2(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.trigger[TRIGGER_JPG].en_alarm2 != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_JPG].en_alarm2), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void trg_avimotion(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.trigger[TRIGGER_AVI].en_motion != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_AVI].en_motion), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void trg_avigioin1(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.trigger[TRIGGER_AVI].en_alarm1 != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_AVI].en_alarm1), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void trg_avigioin2(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.trigger[TRIGGER_AVI].en_alarm2 != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.trigger[TRIGGER_AVI].en_alarm2), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void set_recresolution(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u16 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.sdcard.source != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.sdcard.source), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void ser_recrewrite(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.sdcard.rewrite != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.sdcard.rewrite), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void set_ensavejpg(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.lan_config.jpg_rec_main_en != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.lan_config.jpg_rec_main_en), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }  
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void set_enrecord(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u8 value = atoi(argm->value);
  	
  	do {
  		if(sys_info->config.lan_config.avi_rec_main_en != value)
  		  sysinfo_write(&value, offsetof(SysInfo, config.lan_config.avi_rec_main_en), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void set_avikeeprec(request *req, COMMAND_ARGUMENT *argm)
  {
  	__u32 value = atoi(argm->value);
  	
  	do {
  		if( (sys_info->config.lan_config.avi_rec_mode & AVI_RECMODE_NOSTOP) != value ) {
			sys_info->config.lan_config.avi_rec_mode &= ~AVI_RECMODE_NOSTOP;
			sys_info->config.lan_config.avi_rec_mode |= value;
			sysinfo_write(&(sys_info->config.lan_config.avi_rec_mode), offsetof(SysInfo, config.lan_config.avi_rec_mode), sizeof(__u32), 0);
  		}
  		  //sysinfo_write(&value, offsetof(SysInfo, config.lan_config.avirec_enable), sizeof(value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void set_avieventrec(request *req, COMMAND_ARGUMENT *argm)
  {
	  __u32 value = atoi(argm->value);
	  
	  do {
		  if( (sys_info->config.lan_config.avi_rec_mode & AVI_RECMODE_EVENT) >> 4 != value ) {
			  sys_info->config.lan_config.avi_rec_mode &= ~AVI_RECMODE_EVENT;
			  sys_info->config.lan_config.avi_rec_mode |= value << 4;
			  sysinfo_write(&(sys_info->config.lan_config.avi_rec_mode), offsetof(SysInfo, config.lan_config.avi_rec_mode), sizeof(__u32), 0);
		  }
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void set_avischedulerec(request *req, COMMAND_ARGUMENT *argm)
  {
	  __u32 value = atoi(argm->value);
	  
	  do {
		  if( (sys_info->config.lan_config.avi_rec_mode & AVI_RECMODE_SCHEDULE) >> 8 != value ) {
			  sys_info->config.lan_config.avi_rec_mode &= ~AVI_RECMODE_SCHEDULE;
			  sys_info->config.lan_config.avi_rec_mode |= value << 8;
			  sysinfo_write(&(sys_info->config.lan_config.avi_rec_mode), offsetof(SysInfo, config.lan_config.avi_rec_mode), sizeof(__u32), 0);
		  }
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;

  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
  /***************************************************************************
   *                                                                         *
   ***************************************************************************/
  void set_freespace(request *req, COMMAND_ARGUMENT *argm)
  {
	__u32 value = atoi(argm->value);

	do {
		if(value < REWRITE_MINIMUM)
			break;
		
		if( sys_info->config.sdcard.disklimit != value ) {
			sysinfo_write(&value, offsetof(SysInfo, config.sdcard.disklimit), sizeof(value), 0);
#if SUPPORT_SAMBA
			sysinfo_write(&value, offsetof(SysInfo, config.samba.disklimit), sizeof(value), 0);
#endif
		  //if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
		  //  break;
		}

	  req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	  return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  } 
#endif  //MODEL_LC7315 7311
#endif // NO MORE USED, Alex. 2009.12.15

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void replytest(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_EVENT_2G
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->testserver);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_dlink_alarm_status(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned int status = 0;
	
	do{
		if (sys_info->status.alarm8)
			status |= 0x4000;
		if (sys_info->status.alarm7)
			status |= 0x2000;
		if(sys_info->status.fan_control)
			status |= 0x1000;
		if(sys_info->status.fan_temperature)
			status |= 0x800;
		if(sys_info->status.fan_healthy)
			status |= 0x400;
		if (sys_info->status.alarm6)
			status |= 0x200;
		if (sys_info->status.alarm5)
			status |= 0x100;		
		if (sys_info->status.alarm4)
			status |= 0x80;
		if (sys_info->status.alarm3)
			status |= 0x40;		
		if (sys_info->gioout_time)
			status |= 0x20;
		if (sys_info->ipcam[GIOOUT_ALWAYSON1].config.u16)
		//if (sys_info->config.lan_config.gioout_alwayson[1])
			status |= 0x10;
		if (sys_info->status.on_record)
			status |= 0x08;
		if (sys_info->status.motion)
			status |= 0x04;
		if (sys_info->status.alarm2)
			status |= 0x02;
		if (sys_info->status.alarm1)
			status |= 0x01;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04x\n", argm->name, status);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_dlink_sd_status(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD

	int status = 0;
	
	do{
		if (sys_info->status.mmc & SD_FORMAT_FAIL)
			status |= 0x020;
		if (sys_info->status.mmc & SD_ON_FORMAT)
			status |= 0x010;
		if (sys_info->status.mmc & SD_ON_DETECT)
			status |= 0x008;
		if (sys_info->status.mmc & SD_MOUNTED)
			status |= 0x004;
		if (sys_info->status.mmc & SD_LOCK)
			status |= 0x002;
		if (sys_info->status.mmc & SD_INSERTED)
			status |= 0x001;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%03x\n", argm->name, status);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}
void get_eptz_status(request *req, COMMAND_ARGUMENT *argm)
{
	
	do{
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%01x%01x\n", argm->name, sys_info->status.auto_pan, sys_info->status.eptzpreset_seq);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_wintimezone(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIN_TIMEZONE
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NTP_TIMEZONE].config.u8);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);

        if(value < sys_info->ipcam[NTP_TIMEZONE].min || value > sys_info->ipcam[NTP_TIMEZONE].max)
            break;
        
        if(sys_info->ipcam[NTP_TIMEZONE].config.u8 != value) {
            ControlSystemData(SFIELD_SET_TIMEZONE, (void *)&value, sizeof(value), &req->curr_user);

            setenv("TZ", sys_info->tzenv, 1);
            tzset();
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_maxstreamcount(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_PROFILE_NUMBER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value );
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , MAX_WEB_PROFILE_NUM );
#endif
}
void get_supportstream1(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
#ifdef SUPPORT_PROFILE_NUMBER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , (sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >=1 )?"1":"0");
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name );
#endif
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name );
#endif
}

void get_supportstream2(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
#ifdef SUPPORT_PROFILE_NUMBER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , (sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >=2 )?"1":"0");
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name );
#endif

#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name );
#endif
}

void get_supportstream3(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
#ifdef SUPPORT_PROFILE_NUMBER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , (sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >=3 )?"1":"0");
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name );
#endif

#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name );
#endif
}

void get_supportstream4(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 4 )
 #ifdef SUPPORT_PROFILE_NUMBER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , (sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value >=4 )?"1":"0");
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name );
#endif
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name );
#endif
}

void get_profile1format(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_CODECNAME));
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile2format(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_CODECNAME));
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile3format(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_CODECNAME));
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile4format(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 4 )
	if(profile_check(4) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG3_CODECNAME));
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile1xsize(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILESIZE0_XSIZE].config.value);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile2xsize(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILESIZE1_XSIZE].config.value);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile3xsize(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILESIZE2_XSIZE].config.value);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile1ysize(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILESIZE0_YSIZE].config.value);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile2ysize(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILESIZE1_YSIZE].config.value);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile3ysize(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILESIZE2_YSIZE].config.value);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile1fps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->buf_info.fps[0]);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile2fps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->buf_info.fps[1]);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile3fps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->buf_info.fps[2]);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
  void get_format(request *req, COMMAND_ARGUMENT *argm)
  {
	int id = atoi(argm->value) - 1 ;
	char stream_fmt[HTTP_OPTION_LEN];

	if(profile_check(id+1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	do {
		if( id >= 0 && id <= MAX_PROFILE_NUM )
			strncpy(stream_fmt, conf_string_read( PROFILE_CONFIG0_CODECNAME+PROFILE_STRUCT_SIZE*id), HTTP_OPTION_LEN);
			//sysinfo_read(stream_fmt, offsetof(SysInfo, config.profile_config[value-1].codecname), sizeof(stream_fmt));
		else break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s profile %d is %s\n", argm->name,id+1,stream_fmt);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
  void get_profilexsize(request *req, COMMAND_ARGUMENT *argm)
  {
	__u8 value = atoi(argm->value);
	int xsize = 0;

	if(profile_check(value) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	do {
		if( value == 1 )
			xsize = sys_info->ipcam[PROFILESIZE0_XSIZE].config.value;
			//sysinfo_read(&xsize, offsetof(SysInfo, config.lan_config.profilesize[0].xsize), sizeof(xsize));
		else if( value == 2)
			xsize = sys_info->ipcam[PROFILESIZE1_XSIZE].config.value;
			//sysinfo_read(&xsize, offsetof(SysInfo, config.lan_config.profilesize[1].xsize), sizeof(xsize));
		else if( value == 3)
			xsize = sys_info->ipcam[PROFILESIZE2_XSIZE].config.value;
			//sysinfo_read(&xsize, offsetof(SysInfo, config.lan_config.profilesize[2].xsize), sizeof(xsize));
		else if( value == 4)
			xsize = sys_info->ipcam[PROFILESIZE3_XSIZE].config.value;
			//sysinfo_read(&xsize, offsetof(SysInfo, config.lan_config.profilesize[3].xsize), sizeof(xsize));
		else
			break;
			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
			//	break;
  
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s profile %d xsize = %d\n", argm->name,value,xsize);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
  void get_profileysize(request *req, COMMAND_ARGUMENT *argm)
  {
	__u8 value = atoi(argm->value);
	int ysize = 0;

	if(profile_check(value) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	do {
		if( value == 1 )
			ysize = sys_info->ipcam[PROFILESIZE2_YSIZE].config.value;
			//sysinfo_read(&ysize, offsetof(SysInfo, config.lan_config.profilesize[0].ysize), sizeof(ysize));
		else if( value == 2)
			ysize = sys_info->ipcam[PROFILESIZE2_YSIZE].config.value;
			//sysinfo_read(&ysize, offsetof(SysInfo, config.lan_config.profilesize[1].ysize), sizeof(ysize));
		else if( value == 3)
			ysize = sys_info->ipcam[PROFILESIZE2_YSIZE].config.value;
			//sysinfo_read(&ysize, offsetof(SysInfo, config.lan_config.profilesize[2].ysize), sizeof(ysize));
		else if( value == 4)
			ysize = sys_info->ipcam[PROFILESIZE2_YSIZE].config.value;
			//sysinfo_read(&ysize, offsetof(SysInfo, config.lan_config.profilesize[2].ysize), sizeof(ysize));
		else
			break;
			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
			//	break;
  
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s profile %d xsize = %d\n", argm->name,value,ysize);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf1format(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else	
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_CODECNAME));
        return;
    }
	
	if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if( strcmp( argm->value , JPEG_PARSER_NAME ) != 0 &&
            strcmp( argm->value , MPEG4_PARSER_NAME ) != 0 &&
            strcmp( argm->value , H264_PARSER_NAME ) != 0 ){
            break;
        }
#if defined RESOLUTION_TYCO_720P || defined RESOLUTION_TYCO_D2 || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
				if( strcmp( argm->value , MPEG4_PARSER_NAME ) == 0 )
					break;
#endif
        if( strcmp( conf_string_read( PROFILE_CONFIG0_CODECNAME) , argm->value) != 0 ){
					if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
						osd_print( 3, 5 , "set_prf1format set reset_status");
					}
        	sys_info->procs_status.reset_status = NEED_RESET;
            info_write(argm->value, PROFILE_CONFIG0_CODECNAME, HTTP_OPTION_LEN, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor[sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip, ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			if(strcmp( argm->value , JPEG_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Mode %s", vistor, "JPEG");
				#else
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Mode %s %s", vistor, "JPEG", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			else if(strcmp( argm->value , MPEG4_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Mode %s", vistor, "MPEG4");
				#else
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Mode %s %s", vistor, "MPEG4", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			else if(strcmp( argm->value , H264_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Mode %s", vistor, "H264");
				#else
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Mode %s %s", vistor, "H264", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
int modify_eptz_position(int streamid, char* new_res, char* old_res)//, char* view_size)
{
#ifndef SUPPORT_EPTZ
	return 0;
#endif
	int new_res_x, new_res_y, old_res_x, old_res_y;
	int coordinate_x, coordinate_y;
	char coordinate[HTTP_OPTION_LEN];
	int i;
	char string_cmp[32];
	int tmp, x_position, y_position;
	int id = (streamid-1)*PROFILE_STRUCT_SIZE;
	EPTZPRESET preset;
	char buf[64];

	
	if( sscanf(old_res,"%dx%d",&old_res_x,&old_res_y) != 2 ){
		dbg("old_res error");
		return -1;
	}
	
	if( sscanf(new_res,"%dx%d",&new_res_x,&new_res_y) != 2 ){
		dbg("new_res error");
		return -1;
	}
	
	if( sscanf(conf_string_read(PROFILE_CONFIG0_EPTZCOORDINATE+id),"%04d%04d",&coordinate_x,&coordinate_y) != 2 ){
		dbg("PROFILE_CONFIG0_EPTZCOORDINATE error");
		return -1;
	}
//fprintf(stderr, "old=<%d, %d>\n", coordinate_x, coordinate_y);
	
//	dbg("new_res_x=%d, old_res_x=%d, new_view_x=%d, coordinate_x=%d", new_res_x, old_res_x, new_view_x, coordinate_x);
//	dbg("new_res_y=%d, old_res_y=%d, new_view_y=%d, coordinate_y=%d", new_res_y, old_res_y, new_view_y, coordinate_y);
	
	//coordinate_x = (coordinate_x * new_res_x/old_res_x) - (new_view_x/2);
	
	coordinate_x = (coordinate_x*new_res_x)/old_res_x;
	coordinate_y = (coordinate_y*new_res_y)/old_res_y;
//fprintf(stderr, "new=<%d, %d>\n", coordinate_x, coordinate_y);

	sprintf(coordinate, "%04d%04d", coordinate_x, coordinate_y);
	strcpy(conf_string_read(PROFILE_CONFIG0_EPTZCOORDINATE+id), coordinate);
	dbg("===================================================\n\n\n\ncoordinate_x=%d, coordinate_y=%d", coordinate_x, coordinate_y);

	for(i=0;i<MAX_EPTZPRESET;i++)
	{
		*string_cmp = '\0';

		sscanf(conf_string_read((EPTZPRESET_STREAM1_ID01+(streamid-1)*20)+i), "%d:%d:%d:%d:%d:%04d%04d:%s", 
				&preset.streamid, &preset.index, &preset.seq_time, &preset.zoom, &preset.focus, &x_position, &y_position, string_cmp);
		if(preset.index != 0){
			x_position = (x_position*new_res_x)/old_res_x;
			y_position = (y_position*new_res_y)/old_res_y;
			sprintf(buf, "%d:%d:%d:%d:%d:%04d%04d:%s", preset.streamid, preset.index, preset.seq_time, preset.zoom, preset.focus, x_position, y_position, string_cmp);
			//fprintf(stderr, "buf=%s\n", buf);
			strcpy(conf_string_read((EPTZPRESET_STREAM1_ID01+(streamid-1)*20)+i), buf);
		}
	}

	return 0;
}
#endif
void set_prf1res(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

	if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		}else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_RESOLUTION));
		return;
	}
		
	if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	int profileWidth,profileHeight;
	if( sscanf(argm->value,"%dx%d",&profileWidth,&profileHeight) != 2 ){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
	#if USE_SENSOR_CROP
		int cropWidth,cropHeight;

		if( sscanf(conf_string_read(SENSOR_CROP_RES),"%dx%d",&cropWidth,&cropHeight) != 2 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( profileWidth > cropWidth && profileHeight > cropHeight ){
			DBG("USE_SENSOR_CROP ERROR(%d, %d), (%d, %d)", profileWidth, profileHeight, cropWidth, cropHeight);
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
	#endif
	#ifndef SUPPORT_EPTZ
	if( strcmp( conf_string_read( PROFILE_CONFIG0_CODECNAME ) , MPEG4_PARSER_NAME ) == 0 ){
		if( profileWidth > 1920 && profileHeight > 1080 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
	}
	#endif

    int x;
    do {
        for( x = 0 ; x < MAX_RESOLUTIONS ; x++){

            if( strcmp( argm->value , sys_info->api_string.resolution_list[x] ) == 0 ){
                
                /*if((strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME), MPEG4_PARSER_NAME) == 0)
                    && ((strcmp(argm->value, IMG_SIZE_160x120)==0) || (strcmp(argm->value, IMG_SIZE_176x120)==0) || (strcmp(argm->value, IMG_SIZE_176x144)==0))){
                    break;
                }*/
                /*
                sys_info->config.lan_config.net.old_profile_size[0].xsize = sys_info->config.lan_config.profilesize[0].xsize;
                sys_info->config.lan_config.net.old_profile_size[0].ysize = sys_info->config.lan_config.profilesize[0].ysize;
                if( strcmp( sys_info->config.profile_config[0].resolution , argm->value ) != 0 ){
                    sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[0].resolution), HTTP_OPTION_LEN, 0);
                }
                */
                if(sscanf(conf_string_read(PROFILE_CONFIG0_RESOLUTION), "%dx%d", &sys_info->eptz_status[0].old_res_xsize, &sys_info->eptz_status[0].old_res_ysize)!=2)
					break;
				
                sys_info->ipcam[OLD_PROFILE_SIZE0_XSIZE].config.value = sys_info->ipcam[PROFILESIZE0_XSIZE].config.value;
                sys_info->ipcam[OLD_PROFILE_SIZE0_YSIZE].config.value = sys_info->ipcam[PROFILESIZE0_YSIZE].config.value;
                if( strcmp( conf_string_read( PROFILE_CONFIG0_RESOLUTION) , argm->value ) != 0 ){
									if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
										osd_print( 3, 6 , "set_prf1res set reset_status");
									}
					
				
					//if(modify_eptz_position(1, argm->value, conf_string_read( PROFILE_CONFIG0_RESOLUTION)) < 0 )			
					//	DBG("modify_eptz_position error");
					sys_info->eptz_status[0].res_change = 1;
					sys_info->procs_status.reset_status = NEED_RESET;
                    info_write(argm->value, PROFILE_CONFIG0_RESOLUTION, HTTP_OPTION_LEN, 0);
#ifdef MSGLOGNEW
					char loginip[20];
					char logstr[MSG_MAX_LEN];
					char vistor[sizeof( loginip ) + MAX_USERNAME_LEN + 6];
					strcpy(loginip,ipstr(req->curr_user.ip));
					sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
					#if 1
					snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Frame Size %s", vistor, argm->value);
					#else
					snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 Frame Size %s %s", vistor, argm->value, sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
					#endif
					sprintf(logstr, "%s\r\n", logstr);
					sysdbglog(logstr);
#endif
                }

                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf1resrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

	int i;
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s", argm->name, sys_info->api_string.resolution_list[0]);
	for (i=0; i<MAX_RESOLUTIONS; i++){
		if(strlen(sys_info->api_string.resolution_list[i]) == 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), ";%s", sys_info->api_string.resolution_list[i]);
	}
	req->buffer_end += sprintf(req_bufptr(req), "\n");
	
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf1rate(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_FRAMERATE));
        return;
    }
	
	if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if(numeric_check(argm->value) < 0)
            break;

        if( atoi(argm->value) < 1 || atoi(argm->value) > 30 )
            break;
        
        if( strcmp( conf_string_read( PROFILE_CONFIG0_FRAMERATE), argm->value ) != 0 ){
            if(ControlSystemData(SFIELD_SET_PROFILE1_FRAMERATE,argm->value , strlen(argm->value)+1, &req->curr_user) < 0)
                break;
        }

        //sys_info->procs_status.reset_status = NEED_RESET;
        //info_write(argm->value, PROFILE_CONFIG0_FRAMERATE, HTTP_OPTION_LEN, 0);
        //if( strcmp( sys_info->config.profile_config[0].framerate , argm->value ) != 0 ){      
        //sysinfo_write( argm->value , offsetof(SysInfo, config.profile_config[0].framerate), HTTP_OPTION_LEN , 0);
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
        
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf1raterange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    extern int para_allframeratename(request *req, char *data, char *arg);
    extern int para_prf1ratename(request *req, char *data, char *arg);
    char buff[128] = "";
    int ret;

    if( strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME), JPEG_PARSER_NAME) == 0 )
        ret = para_allframeratename(req, buff, "");
    else
        ret = para_prf1ratename(req, buff, "");

    if(ret < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf1bps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_CBR_QUALITY));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
	
    if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
    int x;
    do {
        if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;
        
        for( x = 0 ; x < MAX_MP4_BITRATE ; x++){
            if( strcmp( argm->value , sys_info->api_string.mp4_bitrate[x] ) == 0 ){
            	if(ControlSystemData(SFIELD_SET_PROFILE1_CBR,argm->value , strlen(argm->value)+1, &req->curr_user) < 0)
                	break;

                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf1bpsrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    extern int para_prf1bpsname(request *req, char *data, char *arg);
    char buff[128] = "";
       
    if(para_prf1bpsname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
void get_profile1bps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_CBR_QUALITY));
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif      
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf1quality(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE){
		   	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY].config.u8);
		else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY].config.u8);
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
    }
	
    if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
    do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);

        if(value > MAX_PROFILE_QUALITY)
            break;
		
		if(ControlSystemData(SFIELD_SET_PROFILE1_QUALITY, &value, sizeof(value), &req->curr_user) < 0)
        	break;
        /*
        if( !strcmp( sys_info->config.profile_config[0].codecname , MPEG4_PARSER_NAME ) || !strcmp( sys_info->config.profile_config[0].codecname , H264_PARSER_NAME )){
            if( sys_info->config.profile_config[0].Fixed_quality != value ){
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[0].Fixed_quality), sizeof(value), 0);
            }
        }else{
            if( sys_info->config.profile_config[0].jpeg_quality != value )
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[0].jpeg_quality), sizeof(value), 0);
        }
        */
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
void set_prf1rtsp(request *req, COMMAND_ARGUMENT *argm)
{
    if(sys_info->ipcam[SUPPORTSTREAM0].config.u8 == 0){
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_RTSPURL));
        return;
    }

    do {
        if (strcmp(conf_string_read( PROFILE_CONFIG0_RTSPURL), argm->value))
            info_write(argm->value, PROFILE_CONFIG0_RTSPURL, strlen(argm->value), 0);
        //if (strcmp(sys_info->config.profile_config[0].rtspurl, argm->value))
            //sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[0].rtspurl), strlen(argm->value), 0);
            //if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
            //  break;

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf1qmod(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG0_QMODE].config.u8);
		else 
	   		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;

    }
	
	if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if(numeric_check(argm->value) < 0)
            break;

        if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[PROFILE_CONFIG0_QMODE].max)
            break;
        
        if ( sys_info->ipcam[PROFILE_CONFIG0_QMODE].config.u8 != value){
        	sys_info->procs_status.reset_status = NEED_RESET;
            info_write(&value, PROFILE_CONFIG0_QMODE, 0, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor[sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			if(value == 0)
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 %s", vistor, "Constant Bit Rate");
			else if(value == 1)
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE1 %s", vistor, "Fixed Quality");
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
        }
        //if ( sys_info->config.profile_config[0].qmode != value)
            //sysinfo_write(&value, offsetof(SysInfo, config.profile_config[0].qmode), sizeof(value), 0);

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
void set_profile1customize(request *req, COMMAND_ARGUMENT *argm)
{ 
__u8 value = atoi(argm->value);
do {
    //if ( sys_info->config.profile_config[0].mpeg4qualityRange != value)
        //sysinfo_write(&value, offsetof(SysInfo, config.profile_config[0].mpeg4qualityRange), sizeof(value), 0);

    if ( sys_info->ipcam[PROFILE_CONFIG0_MPEG4QUALITYRANGE].config.u8 != value)
        info_write(&value, PROFILE_CONFIG0_MPEG4QUALITYRANGE, 0, 0);

    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
    return;
} while (0);

req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_profile1keyframeinterval(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_MPEG4GOV));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
	
	if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    unsigned char value = atoi(argm->value);

    do {
        if( strcmp(conf_string_read(PROFILE_CONFIG0_CODECNAME), JPEG_PARSER_NAME) == 0 )
            break;
            
        if( value > 30 )	break;
            
        if( sys_info->ipcam[PROFILE_CONFIG0_MPEG4KEYFRAMEINTERVAL].config.u8 == value )	break;
        	
        if(ControlSystemData(SFIELD_SET_PROFILE1_GOV,&value , sizeof(value), &req->curr_user) < 0)
        	break;
        	
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_profile1govrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    extern int para_profile1keyframeintervalname(request *req, char *data, char *arg);
    char buff[128] = "";


    if(para_profile1keyframeintervalname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_profile1view(request *req, COMMAND_ARGUMENT *argm)
{
#if defined(SUPPORT_EPTZ) && ( MAX_WEB_PROFILE_NUM >= 1 )
    if (!argm->value[0]) {
		if(profile_check(1) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else	
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_EPTZWINDOWSIZE));
        return;
    }

    if(profile_check(1) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	int profileWidth,profileHeight;
	
	if( sscanf(argm->value,"%dx%d",&profileWidth,&profileHeight) != 2 ){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}	
		
	if( strcmp( conf_string_read( PROFILE_CONFIG0_CODECNAME ) , MPEG4_PARSER_NAME ) == 0 ){
		if( profileWidth > 1920 && profileHeight > 1080 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
	}

    int x;
    do {
        for( x = 0 ; x < MAX_RESOLUTIONS ; x++){
            if( strcmp( argm->value , sys_info->api_string.resolution_list[x] ) == 0 ){
				if(sscanf(conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE), "%dx%d", &sys_info->eptz_status[0].old_win_xsize, &sys_info->eptz_status[0].old_win_ysize)!=2)
					break;
				
                //if( strcmp( sys_info->config.profile_config[0].eptzWindowSize , argm->value ) != 0 ){
                if( strcmp( conf_string_read(PROFILE_CONFIG0_EPTZWINDOWSIZE) , argm->value ) != 0 ){
					
                    //sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[0].eptzWindowSize), HTTP_OPTION_LEN, 0);
                  	sys_info->eptz_status[0].res_change = 1;
                    sys_info->procs_status.reset_status = NEED_RESET;
                    info_write(argm->value, PROFILE_CONFIG0_EPTZWINDOWSIZE, HTTP_OPTION_LEN, 0);
                }
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf2format(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    if (!argm->value[0]) {
		if(profile_check(2) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_CODECNAME));
        return;
    }
	
	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if( strcmp( argm->value , JPEG_PARSER_NAME ) != 0 &&
            strcmp( argm->value , MPEG4_PARSER_NAME ) != 0 &&
            strcmp( argm->value , H264_PARSER_NAME ) != 0 ){
            break;
        }
#if defined RESOLUTION_TYCO_720P || defined RESOLUTION_TYCO_D2 || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_TYCO_3M) || defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
				if( strcmp( argm->value , MPEG4_PARSER_NAME ) == 0 )
					break;
#endif
        if( strcmp( conf_string_read( PROFILE_CONFIG1_CODECNAME) , argm->value) != 0 ){
					if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
						osd_print( 3, 7 , "set_prf2format set reset_status");
					}
        	sys_info->procs_status.reset_status = NEED_RESET;
          info_write(argm->value, PROFILE_CONFIG1_CODECNAME, HTTP_OPTION_LEN, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			if(strcmp( argm->value , JPEG_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Mode %s", vistor, "JPEG");
				#else	//For Debug use
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Mode %s %s", vistor, "JPEG", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			else if(strcmp( argm->value , MPEG4_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Mode %s", vistor, "MPEG4");
				#else	//For Debug use
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Mode %s %s", vistor, "MPEG4", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			else if(strcmp( argm->value , H264_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Mode %s",vistor, "H264");
				#else	//For Debug use
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Mode %s %s",vistor, "H264", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf2res(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	
    if (!argm->value[0]) {
		if(profile_check(2) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_RESOLUTION));
		return;
	}
	
	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
	int profileWidth,profileHeight;
	
	if( sscanf(argm->value,"%dx%d",&profileWidth,&profileHeight) != 2 ){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
	#if USE_SENSOR_CROP
		int cropWidth,cropHeight;
		
		if( sscanf(conf_string_read(SENSOR_CROP_RES),"%dx%d",&cropWidth,&cropHeight) != 2 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( profileWidth > cropWidth && profileHeight > cropHeight ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		
	#endif
	#ifndef SUPPORT_EPTZ
	if( strcmp( conf_string_read( PROFILE_CONFIG1_CODECNAME ) , MPEG4_PARSER_NAME ) == 0 ){
		if( profileWidth > 1920 && profileHeight > 1080 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
	}
	#endif
	
    int x;
    do {
        for( x = 0 ; x < MAX_RESOLUTIONS ; x++){
            if( strcmp( argm->value , sys_info->api_string.resolution_list[x] ) == 0 ){
                /*if((strcmp(conf_string_read(PROFILE_CONFIG1_CODECNAME), MPEG4_PARSER_NAME) == 0)
                    && ((strcmp(argm->value, IMG_SIZE_160x120)==0) || (strcmp(argm->value, IMG_SIZE_176x120)==0) || (strcmp(argm->value, IMG_SIZE_176x144)==0))){
                    break;
                }*/
                /*
                sys_info->config.lan_config.net.old_profile_size[1].xsize = sys_info->config.lan_config.profilesize[1].xsize;
                sys_info->config.lan_config.net.old_profile_size[1].ysize = sys_info->config.lan_config.profilesize[1].ysize;
                if( strcmp( sys_info->config.profile_config[1].resolution , argm->value ) != 0 ){
                    sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[1].resolution), HTTP_OPTION_LEN, 0);
                }
                */
                if(sscanf(conf_string_read(PROFILE_CONFIG1_RESOLUTION), "%dx%d", &sys_info->eptz_status[1].old_res_xsize, &sys_info->eptz_status[1].old_res_ysize)!=2)
					break;
				
                sys_info->ipcam[OLD_PROFILE_SIZE1_XSIZE].config.value = sys_info->ipcam[PROFILESIZE1_XSIZE].config.value;
                sys_info->ipcam[OLD_PROFILE_SIZE1_YSIZE].config.value = sys_info->ipcam[PROFILESIZE1_YSIZE].config.value;
                if( strcmp( conf_string_read( PROFILE_CONFIG1_RESOLUTION) , argm->value ) != 0 ){
									if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
										osd_print( 3, 8 , "set_prf2res set reset_status");
									}
					
					//if(modify_eptz_position(2, argm->value, conf_string_read( PROFILE_CONFIG0_RESOLUTION)) < 0 )			
					//	DBG("modify_eptz_position error");
					sys_info->eptz_status[1].res_change = 1;

                	sys_info->procs_status.reset_status = NEED_RESET;
                    info_write(argm->value, PROFILE_CONFIG1_RESOLUTION, HTTP_OPTION_LEN, 0);
#ifdef MSGLOGNEW
					char loginip[20];
					char logstr[MSG_MAX_LEN];
					char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
					strcpy(loginip,ipstr(req->curr_user.ip));
					sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
					#if 1
					snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Frame Size %s", vistor, argm->value);
					#else
					snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 Frame Size %s %s", vistor, argm->value, sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
					#endif
					sprintf(logstr, "%s\r\n", logstr);
					sysdbglog(logstr);
#endif
                }

                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf2resrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	int i;
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s", argm->name, sys_info->api_string.resolution_list[0]);
	for (i=0; i<MAX_RESOLUTIONS; i++){
		if(strlen(sys_info->api_string.resolution_list[i]) == 0)
				break;
		req->buffer_end += sprintf(req_bufptr(req), ";%s", sys_info->api_string.resolution_list[i]);
	}
	req->buffer_end += sprintf(req_bufptr(req), "\n");

#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf2rate(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    if (!argm->value[0]) {
		if(profile_check(2) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_FRAMERATE));
        return;
    }
	
	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if(numeric_check(argm->value) < 0)
            break;

        if( atoi(argm->value) < 1 || atoi(argm->value) > 30 )
            break;
        
        if( strcmp( conf_string_read( PROFILE_CONFIG1_FRAMERATE), argm->value ) != 0 ){
            if(ControlSystemData(SFIELD_SET_PROFILE2_FRAMERATE,argm->value , strlen(argm->value)+1, &req->curr_user) < 0)
                break;
            //info_write(argm->value, PROFILE_CONFIG1_FRAMERATE, HTTP_OPTION_LEN, 0);
            //if( strcmp( sys_info->config.profile_config[1].framerate , argm->value ) != 0 ){      
            //sysinfo_write( argm->value , offsetof(SysInfo, config.profile_config[1].framerate), HTTP_OPTION_LEN , 0);
        }
		//sys_info->procs_status.reset_status = NEED_RESET;

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;

    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf2raterange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    extern int para_allframeratename(request *req, char *data, char *arg);
    extern int para_prf2ratename(request *req, char *data, char *arg);
    char buff[128] = "";
    int ret;

    if( strcmp(conf_string_read(PROFILE_CONFIG1_CODECNAME), JPEG_PARSER_NAME) == 0 )
        ret = para_allframeratename(req, buff, "");
    else
        ret = para_prf2ratename(req, buff, "");

    if(ret < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void get_profile2bps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_CBR_QUALITY));
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif  
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void set_prf2qmod(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    if (!argm->value[0]) {
		if(profile_check(2) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
        if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG1_QMODE].config.u8);
		else 
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
    }
	
	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if(numeric_check(argm->value) < 0)
            break;

        if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[PROFILE_CONFIG1_QMODE].max)
            break;

        if ( sys_info->ipcam[PROFILE_CONFIG1_QMODE].config.u8 != value){
        	sys_info->procs_status.reset_status = NEED_RESET;
            info_write(&value, PROFILE_CONFIG1_QMODE, 0, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			if(value == 0)
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 %s", vistor, "Constant Bit Rate");
			else if(value == 1)
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE2 %s", vistor, "Fixed Quality");
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
        }
        //if ( sys_info->config.profile_config[1].qmode != value)
            //sysinfo_write(&value, offsetof(SysInfo, config.profile_config[1].qmode), sizeof(value), 0);

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif    
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_profile2keyframeinterval(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    if (!argm->value[0]) {
		if(profile_check(2) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_MPEG4GOV));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

        return;
    }
	
	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    unsigned char value = atoi(argm->value);

    do {
        if( strcmp(conf_string_read(PROFILE_CONFIG1_CODECNAME), JPEG_PARSER_NAME) == 0 )
            break;
            
        if( value > 30 )	break;
            
        if( sys_info->ipcam[PROFILE_CONFIG1_MPEG4KEYFRAMEINTERVAL].config.u8 == value )	break;
        	
        if(ControlSystemData(SFIELD_SET_PROFILE2_GOV,&value , sizeof(value), &req->curr_user) < 0)
        	break;
        	
       	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_profile2govrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    extern int para_profile1keyframeintervalname(request *req, char *data, char *arg);
    char buff[128] = "";

    if(para_profile1keyframeintervalname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_profile2view(request *req, COMMAND_ARGUMENT *argm)
{
#if defined(SUPPORT_EPTZ) && ( MAX_WEB_PROFILE_NUM >= 2 )
	if (!argm->value[0]) {
		if(profile_check(2) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_EPTZWINDOWSIZE));
		return;
  	}

	if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	int profileWidth,profileHeight;
	
	if( sscanf(argm->value,"%dx%d",&profileWidth,&profileHeight) != 2 ){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}	
		
	if( strcmp( conf_string_read( PROFILE_CONFIG1_CODECNAME ) , MPEG4_PARSER_NAME ) == 0 ){
		if( profileWidth > 1920 && profileHeight > 1080 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
	}

    int x;
    do {
        for( x = 0 ; x < MAX_RESOLUTIONS ; x++){
            if( strcmp( argm->value , sys_info->api_string.resolution_list[x] ) == 0 ){
				if(sscanf(conf_string_read(PROFILE_CONFIG1_EPTZWINDOWSIZE), "%dx%d", &sys_info->eptz_status[1].old_win_xsize, &sys_info->eptz_status[1].old_win_ysize)!=2)
					break;
                //if( strcmp( sys_info->config.profile_config[0].eptzWindowSize , argm->value ) != 0 ){
                if( strcmp( conf_string_read(PROFILE_CONFIG1_EPTZWINDOWSIZE) , argm->value ) != 0 ){
					
                    //sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[0].eptzWindowSize), HTTP_OPTION_LEN, 0);
                    sys_info->eptz_status[1].res_change = 1;
                    sys_info->procs_status.reset_status = NEED_RESET;
                    info_write(argm->value, PROFILE_CONFIG1_EPTZWINDOWSIZE, HTTP_OPTION_LEN, 0);
                }
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_profile3keyframeinterval(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
    	}
		if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_MPEG4GOV));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

        return;
    }
	
    if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
    unsigned char value = atoi(argm->value);

    do {
        if( strcmp(conf_string_read(PROFILE_CONFIG2_CODECNAME), JPEG_PARSER_NAME) == 0 )
            break;
            
        if( value > 30 )	break;
            
        if( sys_info->ipcam[PROFILE_CONFIG2_MPEG4KEYFRAMEINTERVAL].config.u8 == value )	break;
        	
        if(ControlSystemData(SFIELD_SET_PROFILE3_GOV,&value , sizeof(value), &req->curr_user) < 0)
        	break;
        	
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_profile3govrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    extern int para_profile1keyframeintervalname(request *req, char *data, char *arg);
    char buff[128] = "";

    if(para_profile1keyframeintervalname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_profile3view(request *req, COMMAND_ARGUMENT *argm)
{
#if defined(SUPPORT_EPTZ) && ( MAX_WEB_PROFILE_NUM >= 3 )
    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_EPTZWINDOWSIZE));
        return;
    }

	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
	int profileWidth,profileHeight;
	
	if( sscanf(argm->value,"%dx%d",&profileWidth,&profileHeight) != 2 ){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
		
	if( strcmp( conf_string_read( PROFILE_CONFIG2_CODECNAME ) , MPEG4_PARSER_NAME ) == 0 ){
		if( profileWidth > 1920 && profileHeight > 1080 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
	}
	
    int x;
    do {
            for( x = 0 ; x < MAX_RESOLUTIONS ; x++){
                if( strcmp( argm->value , sys_info->api_string.resolution_list[x] ) == 0 ){
					if(sscanf(conf_string_read(PROFILE_CONFIG2_EPTZWINDOWSIZE), "%dx%d", &sys_info->eptz_status[2].old_win_xsize, &sys_info->eptz_status[2].old_win_ysize)!=2)
						break;
                    //if( strcmp( sys_info->config.profile_config[0].eptzWindowSize , argm->value ) != 0 ){
                    if( strcmp( conf_string_read(PROFILE_CONFIG2_EPTZWINDOWSIZE) , argm->value ) != 0 ){
						
                        //sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[0].eptzWindowSize), HTTP_OPTION_LEN, 0);
						sys_info->eptz_status[2].res_change = 1;
						sys_info->procs_status.reset_status = NEED_RESET;
                        info_write(argm->value, PROFILE_CONFIG2_EPTZWINDOWSIZE, HTTP_OPTION_LEN, 0);
                    }
                    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                    return;
                }
            }
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
void set_prf3format(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_CODECNAME));
		return;
	}

	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

    do{
        if( strcmp( argm->value , JPEG_PARSER_NAME ) != 0 &&
            strcmp( argm->value , MPEG4_PARSER_NAME ) != 0 &&
            strcmp( argm->value , H264_PARSER_NAME ) != 0 ){
            break;
        }
#if defined RESOLUTION_TYCO_720P || defined RESOLUTION_TYCO_D2
				if( strcmp( argm->value , MPEG4_PARSER_NAME ) == 0 )
					break;
#endif
        if( strcmp( conf_string_read( PROFILE_CONFIG2_CODECNAME) , argm->value) != 0 ){
					if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
						osd_print( 3, 9 , "set_prf3format set reset_status");
					}
        	sys_info->procs_status.reset_status = NEED_RESET;
        	info_write(argm->value, PROFILE_CONFIG2_CODECNAME, HTTP_OPTION_LEN, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			if(strcmp( argm->value , JPEG_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Mode %s", vistor, "JPEG");
				#else
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Mode %s %s", vistor, "JPEG", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			else if(strcmp( argm->value , MPEG4_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Mode %s", vistor, "MPEG4");
				#else
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Mode %s %s", vistor, "MPEG4", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			else if(strcmp( argm->value , H264_PARSER_NAME ) == 0){
				#if 1
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Mode %s", vistor, "H264");
				#else
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Mode %s %s", vistor, "H264", sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
				#endif
			}
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf3res(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_RESOLUTION));
        return;
    }

	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	int profileWidth,profileHeight;
	
	if( sscanf(argm->value,"%dx%d",&profileWidth,&profileHeight) != 2 ){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
		#if USE_SENSOR_CROP
		int cropWidth,cropHeight;
		
		if( sscanf(conf_string_read(SENSOR_CROP_RES),"%dx%d",&cropWidth,&cropHeight) != 2 )
			return req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		
		if( profileWidth > cropWidth && profileHeight > cropHeight )
			return req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
	#endif
	#ifndef SUPPORT_EPTZ
	if( strcmp( conf_string_read( PROFILE_CONFIG2_CODECNAME ) , MPEG4_PARSER_NAME ) == 0 ){
		if( profileWidth > 1920 && profileHeight > 1080 ){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
	}
	#endif
	
    int x;
    do {
        for( x = 0 ; x < MAX_RESOLUTIONS ; x++){
            if( strcmp( argm->value , sys_info->api_string.resolution_list[x] ) == 0 ){
                /*if((strcmp(conf_string_read(PROFILE_CONFIG2_CODECNAME), MPEG4_PARSER_NAME) == 0)
                    && ((strcmp(argm->value, IMG_SIZE_160x120)==0) || (strcmp(argm->value, IMG_SIZE_176x120)==0) || (strcmp(argm->value, IMG_SIZE_176x144)==0))){
                    break;
                }*/
                /*
                sys_info->config.lan_config.net.old_profile_size[2].xsize = sys_info->config.lan_config.profilesize[2].xsize;
                sys_info->config.lan_config.net.old_profile_size[2].ysize = sys_info->config.lan_config.profilesize[2].ysize;
                if( strcmp( sys_info->config.profile_config[2].resolution , argm->value ) != 0 ){
                    sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[2].resolution), HTTP_OPTION_LEN, 0);
                }
                */
                if(sscanf(conf_string_read(PROFILE_CONFIG2_RESOLUTION), "%dx%d", &sys_info->eptz_status[2].old_res_xsize, &sys_info->eptz_status[2].old_res_ysize)!=2)
					break;
				
                sys_info->ipcam[OLD_PROFILE_SIZE2_XSIZE].config.value = sys_info->ipcam[PROFILESIZE2_XSIZE].config.value;
                sys_info->ipcam[OLD_PROFILE_SIZE2_YSIZE].config.value = sys_info->ipcam[PROFILESIZE2_YSIZE].config.value;
                if( strcmp( conf_string_read( PROFILE_CONFIG2_RESOLUTION) , argm->value ) != 0 ){
									if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
										osd_print( 3, 10 , "set_prf3res set reset_status");
									}
					
					//if(modify_eptz_position(3, argm->value, conf_string_read( PROFILE_CONFIG0_RESOLUTION)) < 0 )			
					//	DBG("modify_eptz_position error");
					sys_info->eptz_status[2].res_change = 1;
                	sys_info->procs_status.reset_status = NEED_RESET;
                  info_write(argm->value, PROFILE_CONFIG2_RESOLUTION, HTTP_OPTION_LEN, 0);
#ifdef MSGLOGNEW
					char loginip[20];
					char logstr[MSG_MAX_LEN];
					char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
					strcpy(loginip,ipstr(req->curr_user.ip));
					sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
					#if 1
					snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Frame Size %s", vistor, argm->value);
					#else
					snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 Frame Size %s %s", vistor, argm->value, sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
					#endif
					sprintf(logstr, "%s\r\n", logstr);
					sysdbglog(logstr);
#endif
                }

                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf3resrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	int i;
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s", argm->name, sys_info->api_string.resolution_list[0]);
	for (i=0; i<MAX_RESOLUTIONS; i++){
		if(strlen(sys_info->api_string.resolution_list[i]) == 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), ";%s", sys_info->api_string.resolution_list[i]);
	}
	req->buffer_end += sprintf(req_bufptr(req), "\n");
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf3rate(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE)
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		else
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_FRAMERATE));
		return;
	}

	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        if( atoi(argm->value) < 1 || atoi(argm->value) > 30 )
            break;
        
        if( strcmp( conf_string_read( PROFILE_CONFIG2_FRAMERATE), argm->value ) != 0 ){
            if(ControlSystemData(SFIELD_SET_PROFILE3_FRAMERATE,argm->value , strlen(argm->value)+1, &req->curr_user) < 0)
                break;
                //info_write(argm->value, PROFILE_CONFIG2_FRAMERATE, HTTP_OPTION_LEN, 0);
        }
        //sys_info->procs_status.reset_status = NEED_RESET;
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;

    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf3raterange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    extern int para_allframeratename(request *req, char *data, char *arg);
    extern int para_prf3ratename(request *req, char *data, char *arg);
    char buff[128] = "";
    int ret;

    if( strcmp(conf_string_read(PROFILE_CONFIG2_CODECNAME), JPEG_PARSER_NAME) == 0 )
        ret = para_allframeratename(req, buff, "");
    else
        ret = para_prf3ratename(req, buff, "");

    if(ret < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf3bps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE){
			return;
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		}
        if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_CBR_QUALITY));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
    }
	
	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

    int x;
    do {
        if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;
        
        for( x = 0 ; x < MAX_MP4_BITRATE ; x++){
            if( strcmp( argm->value , sys_info->api_string.mp4_bitrate[x] ) != 0 ){
            	if(ControlSystemData(SFIELD_SET_PROFILE3_CBR,argm->value , strlen(argm->value)+1, &req->curr_user) < 0)
                	break;

                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif	
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf3bpsrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
    extern int para_prf3bpsname(request *req, char *data, char *arg);
    char buff[128] = "";

    if(para_prf3bpsname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf3quality(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
        if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG2_FIXED_QUALITY].config.u8);
		else if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG2_JPEG_QUALITY].config.u8);
       	else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
    }
	
    if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > MAX_PROFILE_QUALITY)
            break;
		
		if(ControlSystemData(SFIELD_SET_PROFILE3_QUALITY, &value, sizeof(value), &req->curr_user) < 0)
                break;
        /*
        if( !strcmp( sys_info->config.profile_config[2].codecname , MPEG4_PARSER_NAME ) || !strcmp( sys_info->config.profile_config[2].codecname , H264_PARSER_NAME )){
            if( sys_info->config.profile_config[2].Fixed_quality != value ){
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[2].Fixed_quality), sizeof(value), 0);
            }
        }else{
            if( sys_info->config.profile_config[2].jpeg_quality != value )
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[2].jpeg_quality), sizeof(value), 0);
        }*/
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
  void set_prf3rtsp(request *req, COMMAND_ARGUMENT *argm)
  {
  	do {
		if (strcmp(conf_string_read( PROFILE_CONFIG2_RTSPURL), argm->value))
			info_write(argm->value, PROFILE_CONFIG2_RTSPURL, strlen(argm->value), 0);
		//if (strcmp(sys_info->config.profile_config[2].rtspurl, argm->value))
			//sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[2].rtspurl), strlen(argm->value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  } 
*/

/***************************************************************************
*                                                                         *
***************************************************************************/
void set_profile4keyframeinterval(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 4 )

    if (!argm->value[0]) {
		if(profile_check(4) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}

        if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG3_MPEG4GOV));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
	
	if(profile_check(4) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    int x;
    do {
        if( strcmp(conf_string_read(PROFILE_CONFIG3_CODECNAME), JPEG_PARSER_NAME) == 0 )
            break;

        for( x = 0; x < MAX_MP4_GOV ; x++ ){
            if(strcmp( sys_info->api_string.mp4_gov[x] , argm->value ) == 0 ){
                if ( strcmp( conf_string_read(PROFILE_CONFIG3_MPEG4GOV) , argm->value ) != 0 )
                    info_write(argm->value, PROFILE_CONFIG3_MPEG4GOV, HTTP_OPTION_LEN, 0);
                
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_profile4govrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 4 )

    extern int para_profile1keyframeintervalname(request *req, char *data, char *arg);
    char buff[128] = "";

    if(para_profile1keyframeintervalname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
*                                                                         *
***************************************************************************/
/*
  void set_profile2customize(request *req, COMMAND_ARGUMENT *argm)
  {	
	__u8 value = atoi(argm->value);
  	do {
		if ( sys_info->ipcam[PROFILE_CONFIG1_MPEG4QUALITYRANGE].config.u8 != value)
			info_write(&value, PROFILE_CONFIG1_MPEG4QUALITYRANGE, 0, 0);
		//if ( sys_info->config.profile_config[1].mpeg4qualityRange != value)
			//sysinfo_write(&value, offsetof(SysInfo, config.profile_config[1].mpeg4qualityRange), sizeof(value), 0);
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
*/
  /***************************************************************************
   *																		 *
   ***************************************************************************/
/*
  void set_profile3customize(request *req, COMMAND_ARGUMENT *argm)
  {	
	__u8 value = atoi(argm->value);
  	do {
		if ( sys_info->ipcam[PROFILE_CONFIG2_MPEG4QUALITYRANGE].config.u8 != value)
			info_write(&value, PROFILE_CONFIG2_MPEG4QUALITYRANGE, 0, 0);
		//if ( sys_info->config.profile_config[2].mpeg4qualityRange != value)
			//sysinfo_write(&value, offsetof(SysInfo, config.profile_config[2].mpeg4qualityRange), sizeof(value), 0);
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf3qmod(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
        if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG2_QMODE].config.u8);
		else 
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
    }
	
	if(profile_check(3) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if(numeric_check(argm->value) < 0)
            break;

        if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[PROFILE_CONFIG2_QMODE].max)
            break;

        if ( sys_info->ipcam[PROFILE_CONFIG2_QMODE].config.u8 != value){
        	sys_info->procs_status.reset_status = NEED_RESET;
            info_write(&value, PROFILE_CONFIG2_QMODE, 0, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor[sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			if(value == 0)
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 %s", vistor, "Constant Bit Rate");
			else if(value == 1)
				snprintf(logstr, MSG_USE_LEN, "%s SET PROFILE3 %s", vistor, "Fixed Quality");
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif
        }
        //if ( sys_info->config.profile_config[2].qmode != value)
            //sysinfo_write(&value, offsetof(SysInfo, config.profile_config[2].qmode), sizeof(value), 0);

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf4qmod(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 4 )

    if (!argm->value[0]) {
		if(profile_check(3) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
        if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG3_QMODE].config.u8);
		else 
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
    }
	
	if(profile_check(4) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    do {
        if(numeric_check(argm->value) < 0)
            break;

        if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[PROFILE_CONFIG3_QMODE].max)
            break;
        
        if ( sys_info->ipcam[PROFILE_CONFIG3_QMODE].config.u8 != value)
            info_write(&value, PROFILE_CONFIG3_QMODE, 0, 0);

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_supportprofilenumber(request *req, COMMAND_ARGUMENT *argm)
{
    do {
#if defined SUPPORT_PROFILE_NUMBER
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,1);
#else
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,0);
#endif
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_supportaspectratio(request *req, COMMAND_ARGUMENT *argm)
{
    do {
#if defined SUPPORT_ASPECT_RATIO
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,1);
#else
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,0);
#endif
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
*	return "argm->name NG=0xabcd"
*	'a' is no used, 'b' is Profile id, Recording has been used.
*	'x' is no used, 'y' is Profile id, Event has been used.
*
*/
void set_profilenumber(request *req, COMMAND_ARGUMENT *argm)
{
#ifndef SUPPORT_PROFILE_NUMBER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value);
		return;
	}

	int i;

    do {
        if(numeric_check(argm->value) < 0)
        	break;

        int value = atoi(argm->value)+1;
        
		if( value > sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].max ||  value < sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].min)
        	break;

        if( value == sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value ){
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        	return;
        }

		for(i=0;i<E2G_MEDIA_MAX_NUM;i++){
			
			if(!sys_info->ipcam[E2G_MEDIA0_INFO_VALID+i*MEDIA_STRUCT_SIZE].config.u8)
				continue;

			if(sys_info->ipcam[E2G_MEDIA0_INFO_TYPE+i*MEDIA_STRUCT_SIZE].config.u16 == EMEDIA_TYPE_PIC){
				if( (value-1) < (sys_info->ipcam[E2G_MEDIA0_PIC_SOURCE+i*MEDIA_STRUCT_SIZE].config.u8+1) ) {
					DBG("\"Event Media%d\" has been used the \"Profile%d\" .", i+1, sys_info->ipcam[E2G_MEDIA0_PIC_SOURCE+i*MEDIA_STRUCT_SIZE].config.u8+1);
					req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s=0x000%01X\n", argm->name, sys_info->ipcam[E2G_MEDIA0_PIC_SOURCE+i*MEDIA_STRUCT_SIZE].config.u8+1);
		        	return;
				}
			}else if(sys_info->ipcam[E2G_MEDIA0_INFO_TYPE+i*MEDIA_STRUCT_SIZE].config.u16 == EMEDIA_TYPE_VIDEO){
				if( (value-1) < (sys_info->ipcam[E2G_MEDIA0_VIDEO_SOURCE+i*MEDIA_STRUCT_SIZE].config.u8+1) ) {
					DBG("\"Event Media%d\" has been used the \"Profile%d\" .", i+1, sys_info->ipcam[E2G_MEDIA0_VIDEO_SOURCE+i*MEDIA_STRUCT_SIZE].config.u8+1);
					req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s=0x000%01X\n", argm->name, sys_info->ipcam[E2G_MEDIA0_VIDEO_SOURCE+i*MEDIA_STRUCT_SIZE].config.u8+1);
		        	return;
				}
			}else
				DBG("E2G_MEDIA%d_INFO_TYPE = %x", i, sys_info->ipcam[E2G_MEDIA0_INFO_TYPE+i*MEDIA_STRUCT_SIZE].config.u16);

		}
		
		for(i=0;i<E2G_RECORD_MAX_NUM;i++){
			if(!sys_info->ipcam[E2G_RECORD0_INFO_VALID+i*MEDIA_STRUCT_SIZE].config.u8)
				continue;
			
			if(1){//(sys_info->ipcam[E2G_RECORD0_INFO_ENABLE+i*MEDIA_STRUCT_SIZE].config.u8 == 1){
				if( (value-1) < (sys_info->ipcam[E2G_RECORD0_NORMAL_SOURCE+i*RECORD_STRUCT_SIZE].config.u8+1) ) {
					DBG("\"Record%d\" has been used the \"Profile%d\" .", i+1, sys_info->ipcam[E2G_RECORD0_NORMAL_SOURCE+i*RECORD_STRUCT_SIZE].config.u8+1);
					req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s=0x0%01X00\n", argm->name, sys_info->ipcam[E2G_RECORD0_NORMAL_SOURCE+i*RECORD_STRUCT_SIZE].config.u8+1);
		        	return;
				}
			}
		}
		
		if(value > sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value)
			if(ControlSystemData(SFIELD_SET_ADJ_PROFILE_DEF, &value, sizeof(value), &req->curr_user) < 0)
				break;
		
		info_write(&value, CONFIG_MAX_PROFILE_NUM, 0, 0);
		//dbg("CONFIG_MAX_PROFILE_NUM value = %d", sys_info->ipcam[CONFIG_MAX_PROFILE_NUM].config.value);
		
		value -= 1;
		info_write(&value, CONFIG_MAX_WEB_PROFILE_NUM, 0, 0);
		//dbg("CONFIG_MAX_WEB_PROFILE_NUM value = %d", sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value);
        
        sys_info->procs_status.reset_status = NEED_RESET;

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_aspectratio(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_ASPECT_RATIO
    do {
		if (!argm->value[0]) {
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( IMG_ASPECT_RATIO ));
        	return;
    	}

#if 0//defined SENSOR_USE_IMX036
		if( strcmp( argm->value, IMG_ASPECT_RATIO_4_3 ) != 0 && strcmp( argm->value, IMG_ASPECT_RATIO_16_9 ) != 0 ){
			dbg("IMX036 not support %s aspectratio!\n",argm->value);
			break;
		}
#endif

        if( strcmp( conf_string_read( IMG_ASPECT_RATIO ), argm->value ) != 0 ) {
			if(ControlSystemData(SFIELD_SET_ADJ_ASPECT, argm->value, strlen(argm->value)+1, &req->curr_user) < 0)
				break;
			
			sys_info->procs_status.reset_status = NEED_RESET;
        	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        	return;
        }
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/

void set_videodefault(request *req, COMMAND_ARGUMENT *argm)
{
#ifndef SUPPORT_PROFILE_NUMBER
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

	do {
		
		if(numeric_check(argm->value) < 0)
        	break;

        int value = MAX_PROFILE_NUM;
		char aspect[HTTP_OPTION_LEN];
		strcpy(aspect, ASPECT_RATIO_DEF);
		
		if(ControlSystemData(SFIELD_SET_ADJ_PROFILE_DEF, &value, sizeof(value), &req->curr_user) < 0)
			break;
		
		if(ControlSystemData(SFIELD_SET_ADJ_ASPECT, aspect, strlen(aspect)+1, &req->curr_user) < 0)
			break;
		
		sys_info->procs_status.reset_status = NEED_RESET;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/


void get_profile3bps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_CBR_QUALITY));
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif  
}

void get_profile4bps(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_MG1264
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->ipcam[CONFIG_AVC_CVALUE].config.u32);
#elif ( MAX_WEB_PROFILE_NUM >= 4 )
	if(profile_check(4) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
    if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG3_CBR_QUALITY));
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile1quality(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )

    do {
		if(profile_check(1) < SPECIAL_PROFILE)
			break;

        if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0)
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG0_FIXED_QUALITY].config.u8);
        else if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG0_JPEG_QUALITY].config.u8);
        else
            break;
        
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void get_profile2quality(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    do {
		if(profile_check(2) < SPECIAL_PROFILE)
			break;
		
        if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0)
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG1_FIXED_QUALITY].config.u8);
        else if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG1_JPEG_QUALITY].config.u8);
        else
            break;
        
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void get_profile3quality(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )

    do {
		if(profile_check(3) < SPECIAL_PROFILE)
			break;
		
        if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0)
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG2_FIXED_QUALITY].config.u8);
        else if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG2_JPEG_QUALITY].config.u8);
        else
            break;
        
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
*                                                                         *
***************************************************************************/

void get_profile4quality(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_MG1264
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.avc_quality );
#elif ( MAX_WEB_PROFILE_NUM >= 4 )

    do {
		if(profile_check(4) < SPECIAL_PROFILE)
			break;
		
        if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0)
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG3_FIXED_QUALITY].config.u8);
        else if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG3_JPEG_QUALITY].config.u8);
        else
            break;
        
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_profile1qmode(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0)
    	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG0_QMODE].config.u8);
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile2qmode(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
    if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0)
    	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG1_QMODE].config.u8);
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile3qmode(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
    if(profile_check(3) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
    if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0)
    	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG2_QMODE].config.u8);
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile4qmode(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 4 )
	if(profile_check(4) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0)
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[PROFILE_CONFIG3_QMODE].config.u8);
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile1gov(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 1 )
	if(profile_check(1) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if( strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG0_CODECNAME) , H264_PARSER_NAME ) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG0_MPEG4GOV));
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_profile2gov(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	if(profile_check(2) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_MPEG4GOV));
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_profile3gov(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 3 )
	if(profile_check(3) < SPECIAL_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if( strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG2_CODECNAME) , H264_PARSER_NAME ) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG2_MPEG4GOV));
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_numprofile(request *req, COMMAND_ARGUMENT *argm)
{

do {
#if SUPPORT_MG1264      
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , MAX_PROFILE_NUM+1 );
#else
#ifdef SUPPORT_PROFILE_NUMBER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value );
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , MAX_WEB_PROFILE_NUM );
#endif
#endif
    return;
} while (0);

    //req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf2bps(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    if (!argm->value[0]) {
		if(profile_check(2) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
		if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG1_CBR_QUALITY));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
	
    if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
    int x;
    do {
        if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;
        
        for( x = 0 ; x < MAX_MP4_BITRATE ; x++){
            if( strcmp( argm->value , sys_info->api_string.mp4_bitrate[x] ) != 0 ){
            	if(ControlSystemData(SFIELD_SET_PROFILE2_CBR, argm->value , strlen(argm->value)+1, &req->curr_user) < 0)
                	break;

               req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
               return;
           }
        }
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf2bpsrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )

    extern int para_prf2bpsname(request *req, char *data, char *arg);
    char buff[128] = "";

    if(para_prf2bpsname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf2quality(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 2 )
	
    if (!argm->value[0]) {
		 if(profile_check(2) < SPECIAL_PROFILE){
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return;
		}
        if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG1_FIXED_QUALITY].config.u8);
		else if( strcmp( conf_string_read(PROFILE_CONFIG1_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG1_JPEG_QUALITY].config.u8);
       	else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
	
    if(profile_check(2) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > MAX_PROFILE_QUALITY)
            break;

		if(ControlSystemData(SFIELD_SET_PROFILE2_QUALITY, &value, sizeof(value), &req->curr_user) < 0)
            break;
        /*
        if( !strcmp( sys_info->config.profile_config[1].codecname , MPEG4_PARSER_NAME ) || !strcmp( sys_info->config.profile_config[1].codecname , H264_PARSER_NAME )){
            if( sys_info->config.profile_config[1].Fixed_quality != value ){
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[1].Fixed_quality), sizeof(value), 0);
            }
        }else{
            if( sys_info->config.profile_config[1].jpeg_quality != value )
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[1].jpeg_quality), sizeof(value), 0);
        }*/
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
  void set_prf2rtsp(request *req, COMMAND_ARGUMENT *argm)
  {
  	do {
		if (strcmp(conf_string_read( PROFILE_CONFIG1_RTSPURL), argm->value))
			info_write(argm->value, PROFILE_CONFIG1_RTSPURL, strlen(argm->value), 0);
		//if (strcmp(sys_info->config.profile_config[1].rtspurl, argm->value))
			//sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[1].rtspurl), strlen(argm->value), 0);
  			//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
  			//	break;
  
  		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
  		return;
  	} while (0);
  	
  	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
  }
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_prf4format( request *req, COMMAND_ARGUMENT *argm )
{
#if 0
#if ( MAX_WEB_PROFILE_NUM >= 4 )
	if(profile_check(4) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG3_CODECNAME));
        return;
    }

    do
    {
        if( strcmp( argm->value , JPEG_PARSER_NAME ) != 0 &&
            strcmp( argm->value , MPEG4_PARSER_NAME ) != 0 &&
            strcmp( argm->value , H264_PARSER_NAME ) != 0 ){
            break;
        }
        
        if( strcmp( conf_string_read( PROFILE_CONFIG3_CODECNAME) , argm->value) != 0 )
            info_write(argm->value, PROFILE_CONFIG3_CODECNAME, HTTP_OPTION_LEN, 0);
        
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
        return;
    }
    while ( 0 );
    req->buffer_end += sprintf( req_bufptr(req), OPTION_NG "%s\n", argm->name );
#endif
#else
    req->buffer_end += sprintf( req_bufptr(req), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                                                                                               *
 ***************************************************************************/
void set_prf4res( request *req, COMMAND_ARGUMENT *argm )
{
#if 0	//tmp jiahung
#if defined( PLAT_DM365 ) && ( MAX_WEB_PROFILE_NUM >= 4 )
	if(profile_check(4) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	#if USE_SENSOR_CROP
		int cropWidth,cropHeight;
		int profileWidth,profileHeight;
		
		if( sscanf(conf_string_read(SENSOR_CROP_RES),"%dx%d",&cropWidth,&cropHeight) != 2 )
			return req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		if( sscanf(argm->value,"%dx%d",&profileWidth,&profileHeight) != 2 )
			return req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		if( profileWidth > cropWidth && profileHeight > cropHeight )
			return req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
	#endif
	
	if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG3_RESOLUTION));
        return;
    }
	   
    int x;
    do {
        for( x = 0 ; x < sys_info->profile_config.max_resolution_num ; x++){
            if( strcmp( argm->value , sys_info->api_string.resolution_list[x] ) == 0 ){
                /*
                sys_info->config.lan_config.net.old_profile_size[3].xsize = sys_info->config.lan_config.profilesize[3].xsize;
                sys_info->config.lan_config.net.old_profile_size[3].ysize = sys_info->config.lan_config.profilesize[3].ysize;
                if( strcmp( sys_info->config.profile_config[3].resolution , argm->value ) != 0 ){
                    dbg("resolution = %s", argm->value);
                    sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[3].resolution), HTTP_OPTION_LEN, 0);
                }
                */
                sys_info->ipcam[OLD_PROFILE_SIZE3_XSIZE].config.value = sys_info->ipcam[PROFILESIZE3_XSIZE].config.value;
                sys_info->ipcam[OLD_PROFILE_SIZE3_YSIZE].config.value = sys_info->ipcam[PROFILESIZE3_YSIZE].config.value;
                if( strcmp( conf_string_read( PROFILE_CONFIG0_RESOLUTION) , argm->value ) != 0 ){
                    info_write(argm->value, PROFILE_CONFIG3_RESOLUTION, HTTP_OPTION_LEN, 0);
					//if(modify_eptz_position(1, argm->value, conf_string_read( PROFILE_CONFIG0_RESOLUTION)) < 0 )			
						//dbg("===================================================\n\n\n\ncoordinate_x");
					//sys_info->eptz_status.res_change = 1;
                }
                
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
                return;
            }
        }
    } while (0);
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#elif defined( PLAT_DM355 )
        __u8 value = 0;// atoi( argm->value );
        
        do
        {
#if defined( SENSOR_USE_MT9V136 )
            if( strcmp( argm->value , RESOLUTION_4 ) != 0 &&
                strcmp( argm->value , RESOLUTION_5 ) != 0 &&
                strcmp( argm->value , RESOLUTION_6 ) != 0 &&
                strcmp( argm->value , RESOLUTION_7 ) != 0 ){
                break;
            }
#elif defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )
            if( strcmp( argm->value , RESOLUTION_1 ) != 0 &&
                strcmp( argm->value , RESOLUTION_2 ) != 0 &&
                strcmp( argm->value , RESOLUTION_4 ) != 0 &&
                strcmp( argm->value , RESOLUTION_5 ) != 0 ){
                break;
            }
#endif
            sys_info->config.lan_config.net.old_profile_size[3].xsize = sys_info->config.lan_config.profilesize[3].xsize;
            sys_info->config.lan_config.net.old_profile_size[3].ysize = sys_info->config.lan_config.profilesize[3].ysize;
            if ( strcmp( sys_info->config.profile_config[3].resolution , argm->value ) != 0 )
            {
                sysinfo_write( argm->value, offsetof( SysInfo, config.profile_config[3].resolution ), HTTP_OPTION_LEN, 0 );
#if defined( SENSOR_USE_MT9V136 ) || defined ( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined ( SENSOR_USE_TVP5150_MDIN270 )
    #ifdef SENSOR_USE_MT9V136
                if ( strcmp( argm->value , RESOLUTION_4 ) == 0 || strcmp( argm->value , RESOLUTION_5 ) == 0)    value = 0;
                else if ( strcmp( argm->value , RESOLUTION_6 ) == 0 || strcmp( argm->value , RESOLUTION_7 ) == 0)   value = 1;
    #elif defined SENSOR_USE_TVP5150  || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined SENSOR_USE_TVP5150_MDIN270
                if ( strcmp( argm->value , RESOLUTION_1 ) == 0 || strcmp( argm->value , RESOLUTION_4 ) == 0)    value = 0;
                else if ( strcmp( argm->value , RESOLUTION_2 ) == 0 || strcmp( argm->value , RESOLUTION_5 ) == 0)   value = 1;
    #endif
                sysinfo_write( &value, offsetof( SysInfo, config.lan_config.avc_resolution ), sizeof( value ), 0 );
                switch( value ){
                    default:
                    case 0:
                        sys_info->config.lan_config.profilesize[3].xsize = 720;
                        sys_info->config.lan_config.profilesize[3].ysize = ( ! sys_info->video_type ) ? 480 : 576;
                        break;
                    case 1:
                        sys_info->config.lan_config.profilesize[3].xsize = 352;
                        sys_info->config.lan_config.profilesize[3].ysize = ( ! sys_info->video_type ) ? 240 : 288;
                        break;
                }
//fprintf(stderr, "set_prf4res: xsize = %d, ysize = %d\n\n", sys_info->config.lan_config.profilesize[3].xsize, sys_info->config.lan_config.profilesize[3].ysize);               
                avc_config_update();
#else
                DBGERR("set_prf4res not defined.\n");
#endif
            }
            req->buffer_end += sprintf( req_bufptr(req), OPTION_OK "%s\n", argm->name );
            return;
        }
    while ( 0 );
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#endif
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf4resrange(request *req, COMMAND_ARGUMENT *argm)
{
#if ( MAX_WEB_PROFILE_NUM >= 4 )
#if 0	//tmp jiahung
#if defined(RESOLUTION_APPRO_SXGA) || defined(RESOLUTION_APPRO_1080P) || defined(RESOLUTION_TYCO_2M) || defined(RESOLUTION_IMX036_2M) || defined(RESOLUTION_OV9715_1M)
        if(0)//(strcmp(conf_string_read( PROFILE_CONFIG3_CODECNAME), MPEG4_PARSER_NAME) == 0)
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s;%s\n", argm->name, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4);
        else 
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s;%s;%s;%s;%s;%s\n", argm->name, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6, RESOLUTION_7, RESOLUTION_8);
            
#elif defined (SENSOR_USE_MT9V136)
        if(0)//(strcmp(conf_string_read( PROFILE_CONFIG3_CODECNAME), MPEG4_PARSER_NAME) == 0)
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s\n", argm->name, RESOLUTION_1, RESOLUTION_2);
        else    
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s\n", argm->name, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
        
#elif defined (SENSOR_USE_TVP5150) || defined IMGS_TS2713 || defined IMGS_ADV7180 || (SENSOR_USE_TVP5150_MDIN270)
        if (sys_info->video_type == 0) {  
            if(0)//(strcmp(conf_string_read( PROFILE_CONFIG3_CODECNAME), MPEG4_PARSER_NAME) == 0)
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s\n", argm->name, RESOLUTION_1, RESOLUTION_2);
            else    
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s\n", argm->name, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
        }
        else {
            if(0)//(strcmp(conf_string_read( PROFILE_CONFIG3_CODECNAME), MPEG4_PARSER_NAME) == 0)
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s\n", argm->name, RESOLUTION_4, RESOLUTION_5);
            else    
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s\n", argm->name, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
        }
            
#elif defined(RESOLUTION_TYCO_D2) || defined(RESOLUTION_TYCO_720P)
        if (sys_info->video_type == 0) {  
            if(0)//(strcmp(conf_string_read( PROFILE_CONFIG3_CODECNAME), MPEG4_PARSER_NAME) == 0)
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s\n", argm->name, RESOLUTION_7, RESOLUTION_1, RESOLUTION_2);
            else
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s;%s\n", argm->name, RESOLUTION_7, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3);
        }
        else {
            if(0)//(strcmp(conf_string_read( PROFILE_CONFIG3_CODECNAME), MPEG4_PARSER_NAME) == 0)
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s\n", argm->name, RESOLUTION_7, RESOLUTION_4, RESOLUTION_5);
            else
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s;%s\n", argm->name, RESOLUTION_7, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
        }
#elif defined(RESOLUTION_IMX036_3M)
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s;%s;%s%s\n", argm->name, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
#elif defined(RESOLUTION_TYCO_3M)
    if (sys_info->video_type == 0)
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s;%s;%s%s\n", argm->name, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_4, RESOLUTION_5, RESOLUTION_6);
	else
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s;%s;%s;%s;%s%s\n", argm->name, RESOLUTION_1, RESOLUTION_2, RESOLUTION_3, RESOLUTION_7, RESOLUTION_8, RESOLUTION_9);
#elif defined(RESOLUTION_TYCO_OV2715_D1) || defined(RESOLUTION_TYCO_IMX076_D1)
	int i, res_num = sys_info->profile_config.max_resolution_num;
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s", sys_info->api_string.resolution_list[0]);
	for (i=1; i<res_num; i++)
		req->buffer_end += sprintf(req_bufptr(req), ";%s", sys_info->api_string.resolution_list[i]);
	req->buffer_end += sprintf(req_bufptr(req), "\n");
#else
#error Unknown product model
#endif
#endif
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                                                                                               *
 ***************************************************************************/
void set_prf4rate( request *req, COMMAND_ARGUMENT *argm )
{
#if 0
#if defined( PLAT_DM365 ) && ( MAX_WEB_PROFILE_NUM >= 4 )
	if(profile_check(4) < EXIST_PROFILE){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG3_FRAMERATE));
        return;
    }

    do {

        if(numeric_check(argm->value) < 0)
            break;

        if( atoi(argm->value) < 1 || atoi(argm->value) > 30 )
            break;
        
        if( strcmp( conf_string_read( PROFILE_CONFIG3_FRAMERATE), argm->value ) != 0 ){
            info_write(argm->value, PROFILE_CONFIG3_FRAMERATE, HTTP_OPTION_LEN, 0);
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
            return;
        }

    } while (0);
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#elif defined( PLAT_DM355 )
        __u8 value = 0;
        do
        {
            if ( strcmp( sys_info->config.profile_config[3].framerate , argm->value ) != 0 )
            {
                sysinfo_write( argm->value , offsetof( SysInfo, config.profile_config[3].framerate ), HTTP_OPTION_LEN, 0 );
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )
                     if ( strcmp( argm->value , AVC_FRAMERATE_1 ) == 0 )    value = 0;
                else if ( strcmp( argm->value , AVC_FRAMERATE_2 ) == 0 )    value = 1;
                else if ( strcmp( argm->value , AVC_FRAMERATE_3 ) == 0 )    value = 2;
                else if ( strcmp( argm->value , AVC_FRAMERATE_4 ) == 0 )    value = 3;
                sysinfo_write( &value , offsetof( SysInfo, config.lan_config.avc_framerate ), sizeof(value), 0 );
                avc_config_update();
#endif
            }
            req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
            return;
        }
        while ( 0 );
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#endif
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf4raterange(request *req, COMMAND_ARGUMENT *argm)
{
#if 0
#if ( MAX_WEB_PROFILE_NUM >= 4 )

    extern int para_allframeratename(request *req, char *data, char *arg);
    extern int para_prf4ratename(request *req, char *data, char *arg);
    char buff[128] = "";
    int ret;

    if( strcmp(conf_string_read(PROFILE_CONFIG3_CODECNAME), JPEG_PARSER_NAME) == 0 )
        ret = para_allframeratename(req, buff, "");
    else
        ret = para_prf4ratename(req, buff, "");

    if(ret < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#endif
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif	
}
/***************************************************************************
 *                                                                                                                                               *
 ***************************************************************************/
void set_prf4bps( request *req, COMMAND_ARGUMENT *argm )
{
#if 0
#if defined( PLAT_DM365 ) && ( MAX_WEB_PROFILE_NUM >= 4 )
	 if(profile_check(4) < EXIST_PROFILE){
		 req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		 return;
	 }

	 if (!argm->value[0]) {
        if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read( PROFILE_CONFIG3_CBR_QUALITY));
		else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
	  
    int x;
    do {
        if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME), JPEG_PARSER_NAME ) == 0 )
            break;

        for( x = 0 ; x < MAX_MP4_BITRATE ; x++){
            if( strcmp( argm->value , sys_info->api_string.mp4_bitrate[x] ) == 0 ){
                if( strcmp( conf_string_read( PROFILE_CONFIG3_CBR_QUALITY) , argm->value ) != 0 )
                    info_write(argm->value, PROFILE_CONFIG3_CBR_QUALITY, HTTP_OPTION_LEN, 0);
            }
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
            return;
        }
    } while (0);
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
    return;
    
#elif defined( PLAT_DM355 )
        int value = 0;

        do
        {
            if( strcmp( sys_info->config.profile_config[3].codecname , H264_PARSER_NAME ) == 0 && sys_info->config.profile_config[3].qmode == 0 ){
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )
                     if ( strcmp( argm->value , MP4_BITRATE_1 ) == 0 )  value = 4 * 1024;
                else if ( strcmp( argm->value , MP4_BITRATE_2 ) == 0 )  value = 2 * 1024;
                else if ( strcmp( argm->value , MP4_BITRATE_3 ) == 0 )  value = 1 * 1024;
                else if ( strcmp( argm->value , MP4_BITRATE_4 ) == 0 )  value =      512;
                else if ( strcmp( argm->value , MP4_BITRATE_5 ) == 0 )  value =      256;
                else if ( strcmp( argm->value , MP4_BITRATE_6 ) == 0 )  value =      200;
                else if ( strcmp( argm->value , MP4_BITRATE_7 ) == 0 )  value =      128;
                else if ( strcmp( argm->value , MP4_BITRATE_8 ) == 0 )  value =       64;
                else    break;
                if ( sys_info->config.lan_config.avc_cvalue != value )
                {
                    sysinfo_write( &value, offsetof( SysInfo, config.lan_config.avc_cvalue ), sizeof(int), 1 );
                    avc_config_update();
                }
#endif              
            }
#if 0   //If necessary, you can open here let profile 4 support MPEG4 and M-JPEG        
            else if( strcmp( sys_info->config.profile_config[3].codecname , MPEG4_PARSER_NAME ) == 0  && sys_info->config.profile_config[3].qmode == 1 ){
                if( sys_info->config.profile_config[3].Fixed_quality != value ){
                    sysinfo_write(&value, offsetof(SysInfo, config.profile_config[3].Fixed_quality), sizeof(value), 0);
                }
            }
            else{
                if(sys_info->config.profile_config[3].jpeg_quality != value )
                    sysinfo_write( &value , offsetof(SysInfo, config.profile_config[3].jpeg_quality), 1 , 1);
            }
#endif
            req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
            return;
        }
        while ( 0 );
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#endif
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_prf4bpsrange(request *req, COMMAND_ARGUMENT *argm)
{
#if 0
#if ( MAX_WEB_PROFILE_NUM >= 4 )

    extern int para_prf4bpsname(request *req, char *data, char *arg);
    char buff[128] = "";
       
    if(para_prf4bpsname(req, buff, "") < 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, buff);
#endif
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                                                                                               *
 ***************************************************************************/
void set_prf4quality( request *req, COMMAND_ARGUMENT *argm )
{
#if 0
#if defined( PLAT_DM365 ) && ( MAX_WEB_PROFILE_NUM >= 4 )
	 if(profile_check(4) < EXIST_PROFILE){
		 req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		 return;
	 }

	 if (!argm->value[0]) {
        if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0 )
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG3_FIXED_QUALITY].config.u8);
		else if( strcmp( conf_string_read(PROFILE_CONFIG3_CODECNAME) , JPEG_PARSER_NAME ) == 0 )
       		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[PROFILE_CONFIG3_JPEG_QUALITY].config.u8);
       	else
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }
	 
    do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);
        
        if( strcmp( conf_string_read( PROFILE_CONFIG3_CODECNAME) , MPEG4_PARSER_NAME ) == 0 || strcmp( conf_string_read( PROFILE_CONFIG3_CODECNAME) , H264_PARSER_NAME ) == 0){
            if( sys_info->ipcam[PROFILE_CONFIG3_FIXED_QUALITY].config.u8 != value ){
                info_write(&value, PROFILE_CONFIG3_FIXED_QUALITY, sizeof(value), 0);
            }
        }else{
            if( sys_info->ipcam[PROFILE_CONFIG3_JPEG_QUALITY].config.u8 != value )
                info_write(&value, PROFILE_CONFIG3_JPEG_QUALITY, sizeof(value), 0);
        }
        /*
        if( !strcmp( sys_info->config.profile_config[3].codecname , MPEG4_PARSER_NAME ) || !strcmp( sys_info->config.profile_config[3].codecname , H264_PARSER_NAME )){
            if( sys_info->config.profile_config[3].Fixed_quality != value ){
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[3].Fixed_quality), sizeof(value), 0);
            }
        }else{
            if( sys_info->config.profile_config[3].jpeg_quality != value )
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[3].jpeg_quality), sizeof(value), 0);
        }*/
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#elif defined( PLAT_DM355 ) 
        __u8 value = atoi(argm->value);
        
        do {
        if( strcmp( sys_info->config.profile_config[3].codecname , H264_PARSER_NAME ) == 0 && sys_info->config.profile_config[3].qmode == 1 ){
            if( sys_info->config.lan_config.avc_quality != value ){
                sysinfo_write( &value, offsetof( SysInfo, config.lan_config.avc_quality ), sizeof(value), 1 );
                avc_config_update();
            }
        }
#if 0   //If necessary, you can open here let profile 4 support MPEG4 and M-JPEG    
        else if( strcmp( sys_info->config.profile_config[3].codecname , MPEG4_PARSER_NAME ) == 0 && sys_info->config.profile_config[3].qmode == 1 ){
            if( sys_info->config.profile_config[3].Fixed_quality != value ){
                sysinfo_write(&value, offsetof(SysInfo, config.profile_config[3].Fixed_quality), sizeof(value), 0);
            }
        }else{
            if(sys_info->config.profile_config[3].jpeg_quality != value)
                sysinfo_write( &value , offsetof(SysInfo, config.profile_config[3].jpeg_quality), 1 , 1);
        }
#endif      
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
            return;
        } while (0);
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
	/***************************************************************************
	 *																																				 *
	 ***************************************************************************/
/*
	void set_prf4rtsp(request *req, COMMAND_ARGUMENT *argm)
	{
	#if ( MAX_WEB_PROFILE_NUM == 4 )
		do {
			if (strcmp(conf_string_read( PROFILE_CONFIG3_RTSPURL), argm->value))
				info_write(argm->value, PROFILE_CONFIG3_RTSPURL, strlen(argm->value), 0);
		//if (strcmp(sys_info->config.profile_config[3].rtspurl, argm->value))
			//sysinfo_write(argm->value, offsetof(SysInfo, config.profile_config[3].rtspurl), strlen(argm->value), 0);
				//if(ControlSystemData(SFIELD_SET_SMB_AUTHORITY, &value, sizeof(value), &req->curr_user) < 0)
				//	break;

			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		} while (0);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
	#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
	#endif
	} 
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_supportsaturation(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SATURATION
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportsd(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SD
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportsenseup(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SENSEUP
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportsharpness(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_SHARPNESS
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportst16550(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_ST16550
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supporttwowayaudio(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_TWOWAY_AUDIO
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportusb(request *req, COMMAND_ARGUMENT *argm)
{
#if defined (CONFIG_BRAND_DLINK) || defined (MACHINE_TYPE_VIDEOSERVER) || (SUPPORT_DOME == 1)
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#endif
}

void get_supportwatchdog(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
}

void get_supportwireless(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void set_wladhoc(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_wladhoc(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS    
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_wlchannel(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_wlchannel(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS    
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_wlencryption(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_wlencryption(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS    
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_wlessid(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS
	
		do {
			
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		} while (0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_wlessid(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS    
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_wlstatus(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS    
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_wlwepkey(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS
	
		do {
			
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		} while (0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_wlwepkey(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS    
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_wlwepwhich(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS
	
		do {
			
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		} while (0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_wlwepwhich(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIRELESS    
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_servicedhcpclient(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_DHCPCLIENT
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportdhcpdipswitch(request *req, COMMAND_ARGUMENT *argm)
{
#ifndef DHCP_CONFIG
	#ifdef CONFIG_BRAND_TYCO
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
	#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
	#endif
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_servicehello(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_HELLO
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_servicemcenter(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_mcenterenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_mcenterfqdn(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_mcenterfqdn(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_mcenterpassword(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_mcenterpassword(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_mcenterport(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_mcenterport(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {

		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_mcenterenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_mcenteraccount(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_MCENTER

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_servicenetbiosns(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_NETBIOSNS
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_servicennews(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_NNEWS
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_servicescanport(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_SCANPORT
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportaudioinswitch(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_AUDIO_SWITCH
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportaudioencselect(request *req, COMMAND_ARGUMENT *argm)
{
#if defined(MODEL_VS2415)
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportwdrlevel(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WDRLEVEL
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportantiflicker(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_ANTIFLICKER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_servicesmtpclient(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_SMTPCLIENT
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_localrtc(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , "FM4005-G");
}

void get_rtcstatus(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , 1);
}

void get_serviceftpserver(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_FTPSERVER
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_serviceftpclient(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_FTPCLIENT
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_servicesntpclient(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_SNTPCLIENT
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_servicepppoe(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_PPPOE
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_serviceddnsclient(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_DDNSCLIENT
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_serviceupnpdevice(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SERVICE_UPNPDEVICE
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_serviceipfilter(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_NEW_IPFILTER
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
    //req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[SERVICEIPFILTER].config.u8);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void add_ipfilterpolicy(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_NEW_IPFILTER
    struct in_addr ip_start_t, ip_end_t;
    char * pstart;
    char * pend;
    
    char ip_rule[8] = "";
    char ip_start[16] = "";
    char ip_end[16] = "";

    __u32 policy, idx;

    //IP_FILTER_TABLE * ip_table;
    IP_FILTER_TABLE  ip_table;
    
    do {
        pstart = strchr(argm->value, ':');
        pend = strchr(argm->value, '-');

        if( pend == NULL || pstart == NULL )
            break;

        *pstart++ = '\0';
        *pend++ = '\0';
        
        // sperate ip range from strings
        strncpy(ip_start, pstart, sizeof(ip_start)-1);
        strncpy(ip_end, pend, sizeof(ip_end)-1);
        strncpy(ip_rule, argm->value, sizeof(ip_rule)-1);
        dbg("ip_start = %s, ip_end = %s, ip_rule = %s", ip_start, ip_end, ip_rule);

        // if inet_aton return value == 0, the address is not valid
        if(inet_aton(ip_start, &ip_start_t) == 0) {
            dbg("ip start address [%s] error ", ip_start);
            break;
        }
        
        if(inet_aton(ip_end, &ip_end_t) == 0) {
            dbg("ip end address [%s] error ", ip_end);
            break;
        }

        // check the rule string
        if(strcasecmp(ip_rule, "allow") == 0) {
            policy = IP_FILTER_ALLOW;
        }
        else if(strcasecmp(ip_rule, "deny") == 0) {
            policy = IP_FILTER_DENY;
        }
        else {
            dbg("UNKNOWN IP FILTER RULE [%s] ", ip_rule);
            break;
        }

        // find the empty space in ip filter table
        for(idx = 0; idx < MAX_IPFILTER_NUM; idx++) {
            
            if(policy == IP_FILTER_ALLOW){
                ip_table.start.s_addr = sys_info->ipcam[ALLOW_TABLE0_START+idx*2].config.ip.s_addr;
                ip_table.end.s_addr = sys_info->ipcam[ALLOW_TABLE0_END+idx*2].config.ip.s_addr;
                //ip_table = &sys_info->config.lan_config.net.allow_table[idx];
            }
            else {
                ip_table.start.s_addr = sys_info->ipcam[DENY_TABLE0_START+idx*2].config.ip.s_addr;
                ip_table.end.s_addr = sys_info->ipcam[DENY_TABLE0_END+idx*2].config.ip.s_addr;
                //ip_table = &sys_info->config.lan_config.net.deny_table[idx];
            }

            //if(ip_table->start.s_addr || ip_table->end.s_addr) {
            if(ip_table.start.s_addr || ip_table.end.s_addr) {
                continue;
            }
            else {
                if(ip_start_t.s_addr > ip_end_t.s_addr) {
                    if(policy == IP_FILTER_ALLOW){
                        sys_info->ipcam[ALLOW_TABLE0_START+idx*2].config.ip.s_addr = ip_end_t.s_addr;
                        sys_info->ipcam[ALLOW_TABLE0_END+idx*2].config.ip.s_addr = ip_start_t.s_addr;
                    }else{
                        sys_info->ipcam[DENY_TABLE0_START+idx*2].config.ip.s_addr = ip_end_t.s_addr;
                        sys_info->ipcam[DENY_TABLE0_END+idx*2].config.ip.s_addr = ip_start_t.s_addr;
                    }
                    //ip_table->start.s_addr = ip_end_t.s_addr;
                    //ip_table->end.s_addr = ip_start_t.s_addr;
                }
                else {
                    if(policy == IP_FILTER_ALLOW){
                        sys_info->ipcam[ALLOW_TABLE0_START+idx*2].config.ip.s_addr = ip_start_t.s_addr;
                        sys_info->ipcam[ALLOW_TABLE0_END+idx*2].config.ip.s_addr = ip_end_t.s_addr;
                    }else{
                        sys_info->ipcam[DENY_TABLE0_START+idx*2].config.ip.s_addr = ip_start_t.s_addr;
                        sys_info->ipcam[DENY_TABLE0_END+idx*2].config.ip.s_addr = ip_end_t.s_addr;
                    }
                    
                    //ip_table->start.s_addr = ip_start_t.s_addr;
                    //ip_table->end.s_addr = ip_end_t.s_addr;
                }
                break;
            }
        }

        if(idx == MAX_IPFILTER_NUM) {
            dbg("THE %s RULETABLE IS FULL ", policy ? "ALLOW" : "DENY");
            break;
        }

        // combine rule and index value. send to server
        idx = ( policy << 16 ) | idx;
        if(ControlSystemData(SFIELD_SET_IPFILTER_ADD, &idx, sizeof(idx), &req->curr_user) < 0)
            break;

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        //req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.net.serviceipfilter);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void del_ipfilterpolicy(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_NEW_IPFILTER
	char * pnum;
	char ip_rule[8] = "";
	__u32 policy, idx;

	//IP_FILTER_TABLE * ip_table;
	IP_FILTER_TABLE  ip_table;
	
	do {
		if( (pnum = strchr(argm->value, ':')) == NULL)
			break;

		idx = atoi(pnum + 1);
		if(idx + 1 > MAX_IPFILTER_NUM)
			break;

		// get rule string 
		strncpy(ip_rule, argm->value, pnum - argm->value);

		// check the rule string
		if(strcasecmp(ip_rule, "allow") == 0) {
			policy = IP_FILTER_ALLOW;
		}
		else if(strcasecmp(ip_rule, "deny") == 0) {
			policy = IP_FILTER_DENY;
		}
		else {
			dbg("UNKNOWN IP FILTER RULE [%s] \n", ip_rule);
			break;
		}

		// disabled by Alex. 2009.07.15 
		// CANNOT delete the allow first rule
		//if(idx == 0 && policy == IP_FILTER_ALLOW)
		//	break;

		if(policy == IP_FILTER_ALLOW){
			ip_table.start.s_addr = sys_info->ipcam[ALLOW_TABLE0_START+idx*2].config.ip.s_addr;
			ip_table.end.s_addr = sys_info->ipcam[ALLOW_TABLE0_END+idx*2].config.ip.s_addr;
			//ip_table = &sys_info->config.lan_config.net.allow_table[idx];
		}else{
			ip_table.start.s_addr = sys_info->ipcam[DENY_TABLE0_START+idx*2].config.ip.s_addr;
			ip_table.end.s_addr = sys_info->ipcam[DENY_TABLE0_END+idx*2].config.ip.s_addr;
			//ip_table = &sys_info->config.lan_config.net.deny_table[idx];
		}
			
		//if(!ip_table->start.s_addr && !ip_table->end.s_addr) {
		if(!ip_table.start.s_addr && !ip_table.end.s_addr) {
			dbg("%s RULE INDEX %d IS EMPTY \n", policy ? "ALLOW" : "DENY", idx);
			break;
		}
		
		// combine rule and index value. send to server
		idx = ( policy << 16 ) | idx;
		if(ControlSystemData(SFIELD_SET_IPFILTER_DEL, &idx, sizeof(idx), &req->curr_user) < 0)
			break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.net.serviceipfilter);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0	//disable by jiahung, 2010.03.22
void set_upnpcenable(request *req, COMMAND_ARGUMENT *argm)
{
	
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.net.upnpcenable != value)
			if (ControlSystemData(SFIELD_SET_UPNP_CLIENT_ENABLE, (void *)&value, sizeof(value), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_upnpcextport(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned int value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.net.upnpcextport != value)
			if (ControlSystemData(SFIELD_SET_UPNP_CLIENT_EXTERNAL_PORT, (void *)&value, sizeof(value), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void get_upnpportforwardingstatus(request *req, COMMAND_ARGUMENT *argm)
{
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->status.upnpportforewarding);
}

#if 0
void set_virtauldeldata(request *req, COMMAND_ARGUMENT *argm)
{
	VIRTUAL_SERVER_TABLE *Pstruct = sys_info->config.lan_config.net.lvs_table;
  int value = atoi(argm->value);
  int x = 0;
  pid_t child;
  FILE *LVS_CONFIG;
  char command[128];
  NET_IPV4 ip;

  do {
  	if( Pstruct[x].publicport != 0 ){
	  	for( x = value ; x < 10 ; x++){
	  		Pstruct[x].publicport = 0;
	  		if( x < 9 ){
	  			if( Pstruct[x + 1].publicport == 0 ) break;
	  			Pstruct[x].publicport = Pstruct[x + 1].publicport;
		    	Pstruct[x].privateport = Pstruct[x + 1].privateport;
		    	Pstruct[x].LVS_protocol = Pstruct[x + 1].LVS_protocol;
		    	strcpy( Pstruct[x].name , Pstruct[x + 1].name );
		    	strcpy( Pstruct[x].LVS , Pstruct[x + 1].LVS );
	  		}
	  	}
	  	child  = fork();
	    switch(child){
	      case -1:
	        dbg("fork() error!\n");
	        break;
	      case  0:  //in child process
	        LVS_CONFIG = fopen(LVS_CONFIG_PATH,"wb");
	        if(LVS_CONFIG == NULL){
	          dbg("Can't open file '%s'",LVS_CONFIG_PATH);
	          _exit(-1);
	        }
	        
	        system("ipvsadm -C");
	        for( x = 0 ; x < 10 ; x++ ){
	        	if(Pstruct[x].publicport == 0) break;
		        sprintf(command,"-A -t %d.%d.%d.%d:%d -s rr\n",ip.str[0],ip.str[1],ip.str[2],ip.str[3] , Pstruct[x].publicport);
		        fputs(command,LVS_CONFIG);
		        sprintf(command,"-a -t %d.%d.%d.%d:%d -r %s:%d -m -w 1\n",ip.str[0],ip.str[1],ip.str[2],ip.str[3] , Pstruct[x].publicport, Pstruct[x].LVS ,Pstruct[x].privateport);
		        fputs(command,LVS_CONFIG);
		      }
		      fclose(LVS_CONFIG);
	        execl("/opt/ipnc/ipvsadm.sh","ipvsadm.sh","restart",0);
	        
	        dbg("child process error!)\n");
	        _exit(-1);
	        break;
	    }
	  }
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
    return;
  } while (0);
  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_virtaulenable(request *req, COMMAND_ARGUMENT *argm)
{
  int enable = atoi(argm->value);
  int DIR;

  do {
    if(enable != sys_info->config.lan_config.net.LVS_enable){
      sys_info->config.lan_config.net.LVS_enable = enable;

      if( sys_info->config.lan_config.net.LVS_enable == 1 ){
      	DIR = open("/proc/sys/net/ipv4/ip_forward",O_WRONLY);
      	write( DIR , "1" , sizeof(char*));
				close(DIR);
				DIR = open("/proc/sys/net/ipv4/conf/all/send_redirects",O_WRONLY);
      	write( DIR , "0" , sizeof(char*));
				close(DIR);
				DIR = open("/proc/sys/net/ipv4/conf/default/send_redirects",O_WRONLY);
      	write( DIR , "0" , sizeof(char*));
				close(DIR);
				DIR = open("/proc/sys/net/ipv4/conf/eth0/send_redirects",O_WRONLY);
      	write( DIR , "0" , sizeof(char*));
				close(DIR);
      } else {
      	DIR = open("/proc/sys/net/ipv4/ip_forward",O_WRONLY);
      	write( DIR , "0" , sizeof(char*));
				close(DIR);
				DIR = open("/proc/sys/net/ipv4/conf/all/send_redirects",O_WRONLY);
      	write( DIR , "1" , sizeof(char*));
				close(DIR);
				DIR = open("/proc/sys/net/ipv4/conf/default/send_redirects",O_WRONLY);
      	write( DIR , "1" , sizeof(char*));
				close(DIR);
				DIR = open("/proc/sys/net/ipv4/conf/eth0/send_redirects",O_WRONLY);
      	write( DIR , "1" , sizeof(char*));
				close(DIR);
      }
    }
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
    return;
  } while (0);
  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_virtaulserver(request *req, COMMAND_ARGUMENT *argm)
{
	VIRTUAL_SERVER_TABLE *Pstruct = sys_info->config.lan_config.net.lvs_table;
  pid_t child;
  FILE *LVS_CONFIG;
  char *location;
  NET_IPV4 ip;
  char cmd[8][32] ,command[128];
  ip.int32 = net_get_ifaddr(sys_info->ifname);  //kevinlee eth0 or ppp0
  int table_index = argm->value[0] - 48 , x;
  
  location = strchr(argm->value , ':'); location++;
  cmd_array_parser(location , cmd);
  do {
  	if( sys_info->config.lan_config.net.LVS_enable == 0 )
  		break;
  	strcpy(Pstruct[table_index].name , cmd[0]);
  	strcpy(Pstruct[table_index].LVS , ip_insert_dot(cmd[1]));
  	Pstruct[table_index].LVS_protocol = atoi(cmd[2]);
  	Pstruct[table_index].privateport = atoi(cmd[3]);
  	Pstruct[table_index].publicport = atoi(cmd[4]);

    child  = fork();
    switch(child){
      case -1:
        dbg("fork() error!\n");
        break;
      case  0:  //in child process
        LVS_CONFIG = fopen(LVS_CONFIG_PATH,"wb");
        if(LVS_CONFIG == NULL){
          dbg("Can't open file '%s'",LVS_CONFIG_PATH);
          _exit(-1);
        }
        
        system("ipvsadm -C");
        for( x = 0 ; x < 10 ; x++ ){
        	if(Pstruct[x].publicport == 0) break;
	        sprintf(command,"-A -t %d.%d.%d.%d:%d -s rr\n",ip.str[0],ip.str[1],ip.str[2],ip.str[3] , Pstruct[x].publicport);
	        fputs(command,LVS_CONFIG);
	        sprintf(command,"-a -t %d.%d.%d.%d:%d -r %s:%d -m -w 1\n",ip.str[0],ip.str[1],ip.str[2],ip.str[3] , Pstruct[x].publicport, Pstruct[x].LVS ,Pstruct[x].privateport);
	        fputs(command,LVS_CONFIG);
	      }
	      fclose(LVS_CONFIG);
        execl("/opt/ipnc/ipvsadm.sh","ipvsadm.sh","restart",0);
        
        dbg("child process error!)\n");
        _exit(-1);
        break;
    }
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
    return;
  } while (0);
  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void get_supporttstamp(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_TSTAMP
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportmotion(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}
void get_supportmotionmode(request *req, COMMAND_ARGUMENT *argm)
{
	/* 0:old motion, block type */
	/* 1:new motion, window type */

	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, SUPPORT_MOTIONMODE);
}

void get_supportdncontrol(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DNCONTROL
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportaudio(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_AUDIO
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportavc(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_AVC
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportmpeg4(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MPEG4
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportmui(request *req, COMMAND_ARGUMENT *argm)
{

    //req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[SUPPORTMUI].config.u8);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, SUPPORT_MUI);
}

void whoareonline(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, SUPPORT_MUI);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[SUPPORTMUI].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.net.supportmui);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0
void set_aftpenable(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned char bEnable = atoi(argm->value);
	do {
		if(sys_info->ipcam[BAFTPENABLE].config.u8 != bEnable)
		//if(sys_info->config.lan_config.bAFtpEnable != bEnable)
			if(ControlSystemData(SFIELD_SET_AFTPENABLE, &bEnable, sizeof(bEnable), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}


void get_aftpenable(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		/*if( pSysInfo == NULL )
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[BAFTPENABLE].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.bAFtpEnable);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_rftpenable(request *req, COMMAND_ARGUMENT *argm)
{
  /*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.ftp_config.rftpenable);
    return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void get_fluorescent(request *req, COMMAND_ARGUMENT *argm)  //add by mikewang 20080807
{
#ifdef SUPPORT_FLUORESCENT 
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NFLUORESCENT].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void get_brightness(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BRIGHTNESS 
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NBRIGHTNESS].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_denoise(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DENOISE 
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NDENOISE].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_evcomp(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_EVCOMP 
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NEVCOMP].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_backlight(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BACKLIGHT  
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NBACKLIGHT].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_blc(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BLCMODE 
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NBACKLIGHTCONTROL].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_mirror(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MIRROR 

#if SUPPORT_DOME && (!defined(SENSOR_USE_MT9V136))
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, !sys_info->ipcam[NMIRROR].config.u8);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NMIRROR].config.u8);
#endif

#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

/*
void set_asmtpenable(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned char bEnable = atoi(argm->value);
	do {
		if(sys_info->ipcam[BASMTPENABLE].config.u8 != bEnable)
		//if(sys_info->config.lan_config.bASmtpEnable != bEnable)
			if(ControlSystemData(SFIELD_SET_ASMTPENABLE, &bEnable, sizeof(bEnable), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_alarmduration(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[NALARMDURATION].config.u8 != value)
		//if(sys_info->config.lan_config.nAlarmDuration != value)
			if(ControlSystemData(SFIELD_SET_ALARMDURATION, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
void set_motionenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].max)
            break;
        #ifdef SUPPORT_VISCA
		if(value){
			VISCACamera_t camera;
			VISCAInterface_t  interface;
			char temp1[3], temp2[9];
			unsigned int option, var, info_value, info_extend;

			interface.broadcast = 0;
			camera.socket_num	= 2;	
			camera.address	 = 1; 

			if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
				dbg("VISCA_open_serial failed!\n");
			/**********************Stop Tour Command*******************/
			strcpy( temp1, "7B");
			option = strtol(temp1, NULL, 16);
			strcpy(temp2, "00000000");
			var = strtoul(temp2, NULL, 16);			
			if (VISCA_command_dispatch( &interface, &camera, option, var, &info_value, &info_extend) != VISCA_SUCCESS)
				dbg("VISCA_command_dispatch failed!\n");
			if( (interface.ibuf[0] != 0x90) || (interface.ibuf[1] != 0x51) )
				dbg("Receive packet error!\n");
			if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
				dbg("VISCA_close_serial failed!\n");
			/**********************Stop Tour Command*******************/
		}	
		#endif
		if(sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8 != value)
			if(ControlSystemData(SFIELD_SET_MOTIONENABLE, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        dbg("\n motionenable= %d\n",value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void motiondrawenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[MOTION_CONFIG_MOTION_DRAW_ENABLE].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[MOTION_CONFIG_MOTION_DRAW_ENABLE].max)
            break;
        
		if(sys_info->ipcam[MOTION_CONFIG_MOTION_DRAW_ENABLE].config.u8 != value)
			if(ControlSystemData(SFIELD_SET_MOTION_DRAW_ENABLE, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        dbg("\n motiondrawenable= %d\n",value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}


void motionregion(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION
	char startX[5];
	char startY[5];
	char endX[5];
	char endY[5];
	int idx = 1;
	int regionId;
	
  if( strlen(argm->value) == 0 ) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}
	
	if( strlen(argm->value) != 17 ) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		return;
	}

	do {
		if(numeric_check(argm->value) < 0)
			break;

		if( strlen(argm->value) > (sys_info->ipcam[MOTION_CONFIG_MOTIONREGION0].max) )
			break;
			
		regionId = argm->value[0] - 48;

		if( regionId > 9 )
			break;
		
		memcpy( startX , argm->value + idx , 4 );
		startX[4] = '\0';
		idx += 4;
		
		memcpy( startY , argm->value + idx , 4 );
		startY[4] = '\0';
		idx += 4;
		
		memcpy( endX , argm->value + idx , 4 );
		endX[4] = '\0';
		idx += 4;
		
		memcpy( endY , argm->value + idx , 4 );
		endY[4] = '\0';
		
		if( atoi(startX) > atoi(endX) || atoi(startY) > atoi(endY) )	break;
    
		if(sys_info->ipcam[MOTION_CONFIG_MOTIONENABLE].config.u8){
			if(ControlSystemData(SFIELD_SET_MOTIONREGION, argm->value, strlen(argm->value)+1, &req->curr_user) < 0)
				break;
		}else	break;
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
//        dbg("\n motionenable= %d\n",value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_motioncenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION

	__u8 value = atoi(argm->value);
	do {
		
#ifdef CONFIG_BRAND_DLINK
		break;
#endif
		if(sys_info->ipcam[MOTION_CONFIG_MOTIONCENABLE].config.u8 != value)
		//if(sys_info->config.motion_config.motioncenable != value)
			if(ControlSystemData(SFIELD_SET_MOTIONCENABLE, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		dbg("\n motioncenable= %d\n",value);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}

void set_motionlevel(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION

	__u8 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[MOTION_CONFIG_MOTIONLEVEL].config.u8 != value)
		//if(sys_info->config.motion_config.motionlevel != value)
			if(ControlSystemData(SFIELD_SET_MOTIONLEVEL, &value, sizeof(value), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}


void set_motioncvalue(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].config.u16);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u16 value = atoi(argm->value);

        if(value > sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].max)
            break;
        
		if(sys_info->ipcam[MOTION_CONFIG_MOTIONCVALUE].config.u16 != value)
			if(ControlSystemData(SFIELD_SET_MOTIONCVALUE, &value, sizeof(value), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void set_motionblock(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(MOTION_CONFIG_MOTIONBLOCK));
        return;
    }

    do {
        if(strlen(argm->value) > sys_info->ipcam[MOTION_CONFIG_MOTIONBLOCK].max)
            break;
        if( !strcmp(argm->value, conf_string_read(MOTION_CONFIG_MOTIONBLOCK)) )
			break;
        if(ControlSystemData(SFIELD_SET_MOTIONBLOCK, argm->value, strlen(argm->value), &req->curr_user) < 0)
            break;

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}


void set_motionpercentage(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_MOTION

    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[MOTION_CONFIG_MOTIONPERCENTAGE].config.u8);
        return;
    }
    
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[MOTION_CONFIG_MOTIONPERCENTAGE].max)
            break;
        
		if(sys_info->ipcam[MOTION_CONFIG_MOTIONPERCENTAGE].config.u8 != value) {
			if(ControlSystemData(SFIELD_SET_MOTION_PERCENTAGE, &value, sizeof(value), &req->curr_user) < 0)
				break;
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void sensorcropenable(request *req, COMMAND_ARGUMENT *argm)
{
#if USE_SENSOR_CROP == 0
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
	return;
#endif

	if(strlen(argm->value) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->ipcam[SENSOR_CROP_ENABLE].config.u8);

	do {

		int value = atoi(argm->value);

		if( value > 1 ) break;

		if(sys_info->ipcam[SENSOR_CROP_ENABLE].config.u32 != value) {
			sys_info->procs_status.reset_status = NEED_RESET;
			info_write(&value, SENSOR_CROP_ENABLE, 0, 0);
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void sensorcropresolution(request *req, COMMAND_ARGUMENT *argm)
{
#if USE_SENSOR_CROP == 0
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

	if(strlen(argm->value) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(SENSOR_CROP_RES));
    
	do {
#if USE_SENSOR_CROP
		int cropEnable;

		if( strcmp( argm->value , IMG_SIZE_1280x1024 ) != 0 &&
				strcmp( argm->value , IMG_SIZE_1280x720 ) != 0 &&
				strcmp( argm->value , IMG_SIZE_640x480 ) != 0 )
			break;

		if( strcmp( argm->value , IMG_SIZE_1280x1024 ) == 0 )
			cropEnable = 0;
		else
			cropEnable = 1;

		info_write(&cropEnable, SENSOR_CROP_ENABLE, 0, 0);
#else
		break;
#endif

		if( strcmp( conf_string_read(SENSOR_CROP_RES) , argm->value ) != 0 ) {
			sys_info->procs_status.reset_status = NEED_RESET;
			info_write( argm->value, SENSOR_CROP_RES, strlen(argm->value), 0 );
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void sensorstartx(request *req, COMMAND_ARGUMENT *argm)
{
#if USE_SENSOR_CROP == 0
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

	if( strlen(argm->value) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->ipcam[SENSOR_CROP_STARTX].config.u32);
    
	do {
		if(numeric_check(argm->value) < 0 || sys_info->ipcam[SENSOR_CROP_ENABLE].config.u8 == 0 )
			break;

		int value = atoi(argm->value);
		
		if( value < 0 || value > 520 )
			break;

		if(sys_info->ipcam[SENSOR_CROP_STARTX].config.u32 != value) {
			if(ControlSystemData(SFIELD_SET_SENSOR_STARTX, &value, sizeof(value), &req->curr_user) < 0)
				break;
			//info_write(&value, SENSOR_CROP_STARTX, 0, 0);
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void sensorstarty(request *req, COMMAND_ARGUMENT *argm)
{
#if USE_SENSOR_CROP == 0
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

	if( strlen(argm->value) == 0 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->ipcam[SENSOR_CROP_STARTY].config.u32);
    	return;
	do {
		if(numeric_check(argm->value) < 0 || sys_info->ipcam[SENSOR_CROP_ENABLE].config.u8 == 0 )
			break;

		int value = atoi(argm->value);
		
		if( value < 0 || value > 300 )
			break;
        
		if(sys_info->ipcam[SENSOR_CROP_STARTY].config.u32 != value) {
			if(ControlSystemData(SFIELD_SET_SENSOR_STARTY, &value, sizeof(value), &req->curr_user) < 0)
				break;
			//info_write(&value, SENSOR_CROP_STARTY, 0, 0);
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/*
void get_motionxblock(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, 12);
	return;
	__u8 value = atoi(argm->value);
	do {
		if(ControlSystemData(SFIELD_SET_ALARMDURATION, &value, sizeof(value), &req->curr_user) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_motionyblock(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, 8);
	return;
	__u8 value = atoi(argm->value);
	do {
		if(ControlSystemData(SFIELD_SET_ALARMDURATION, &value, sizeof(value), &req->curr_user) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/ // Disable by Alex, 2008.10.17

void paratest(request *req, COMMAND_ARGUMENT *argm)
{
	int ret;
	char buf[1024];
	char *arg1, zero_str[1]={0x00};
	
	arg1 = strchr(argm->value, '.');
	if (arg1) 
		*arg1++ = '\0';
	else
		arg1 = zero_str;
			
	do {
		if((ret=TranslateWebPara(req, buf, argm->value, arg1)) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->value, buf);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->value);
}
/*-----------------------------------------------------------------*/
void set_ratecontrol(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		ControlSystemData(SFIELD_SET_RATE_CONTROL, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0
void set_jpegframerate(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.nJpegfrmrate != value)
			ControlSystemData(SFIELD_SET_JPEG_FRAMERATE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_jpegframerate2(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.nJpeg2frmrate != value)
			ControlSystemData(SFIELD_SET_JPEG_2_FRAMERATE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_mpeg41framerate(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.nMpeg41frmrate != value)
			ControlSystemData(SFIELD_SET_MPEG41_FRAMERATE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_mpeg42framerate(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.nMpeg42frmrate != value)
			ControlSystemData(SFIELD_SET_MPEG42_FRAMERATE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/*-----------------------------------------------------------------*/
void set_videocodecmode(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[NVIDEOCODECMODE].config.u8 != value)
		//if(sys_info->config.lan_config.nVideocodecmode != value)
			ControlSystemData(SFIELD_SET_VIDEOCODECMODE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_videocodecres(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[NVIDEOCODECRES].config.u8!= value)
		//if(sys_info->config.lan_config.nVideocodecres != value)
			ControlSystemData(SFIELD_SET_VIDEOCODECRES, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0
void set_imagesource(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->config.lan_config.net.imagesource != value)
			ControlSystemData(SFIELD_SET_IMAGESOURCE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
unsigned char gl_TIflag = 0;
void set_img2a(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	if (value == 3 && gl_TIflag == 0) {
		gl_TIflag = 1;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	}
	if (gl_TIflag == 1)
		value += 4;
	do {
		ControlSystemData(SFIELD_SET_IMAGE2A, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/*-----------------------------*/
void set_tvcable(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[NTVCABLE].config.u8!= value)
		//if(sys_info->config.lan_config.nTVcable != value)
			ControlSystemData(SFIELD_SET_TVCABLE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_binning(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		if(sys_info->ipcam[NBINNING].config.u8!= value)
		//if(sys_info->config.lan_config.nBinning != value)
			ControlSystemData(SFIELD_SET_BINNING, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/*
void set_blc(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);

	do {
		ControlSystemData(SFIELD_SET_BLC, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
void set_blcmode(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_BLCMODE
	
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[NBACKLIGHTCONTROL].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[NBACKLIGHTCONTROL].max)
            break;
        
		if(sys_info->ipcam[NBACKLIGHTCONTROL].config.u8 != value)
			ControlSystemData(SFIELD_SET_BLCMODE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
#if 0
void set_mpeg41bitrate(request *req, COMMAND_ARGUMENT *argm)
{
   int value = atoi(argm->value);
   value *= 1000;
   fprintf(stderr,"MP41bitrate : %d \n", value);

	do {
		if( (value > 8192*1000) || (value < 64*1000) )
			break;
		if(sys_info->config.lan_config.nMpeg41bitrate != value)
			if (ControlSystemData(SFIELD_SET_MPEG41_BITRATE, (void *)&value, sizeof(value), &req->curr_user) < 0)
				break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_mpeg42bitrate(request *req, COMMAND_ARGUMENT *argm)
{
   int value = atoi(argm->value);
   value *= 1000;
   fprintf(stderr,"MP42bitrate : %d \n", value);
	do {
		if( (value > 8192*1000) || (value < 64*1000) )
			break;
		if(sys_info->config.lan_config.nMpeg42bitrate != value)
			if (ControlSystemData(SFIELD_SET_MPEG42_BITRATE, (void *)&value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void get_machinecode(request *req, COMMAND_ARGUMENT *argm)
{
	int sub_code = get_submcode_from_kernel();
	if (sub_code > 0)
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04x%04x\n", argm->name , MACHINE_CODE, sub_code);
	else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04x\n", argm->name, MACHINE_CODE);
}

void sub_machinecode(request *req, COMMAND_ARGUMENT *argm)
{
	int sub_code = get_submcode_from_kernel();
/*	if (sub_code <= 0) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
		return;
	}
*/
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04x\n", argm->name, sub_code);
		return;
	}

	do {
		int value = strtol(argm->value, NULL, 16);
		if (ControlSystemData(SFIELD_SET_SUB_MACHINECODE, (void *)&value, sizeof(value), &req->curr_user) < 0) {
			renew_submcode();
			break;
		}
		renew_submcode();

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

void bios_modelname(request *req, COMMAND_ARGUMENT *argm)
{
	int sub_code = get_submcode_from_kernel();
/*	if (sub_code <= 0) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
		return;
	}
*/
	BIOS_DATA bios;
	if (bios_data_read(&bios) < 0)
		bios_data_set_default(&bios);

	do {
		int len = strlen(argm->value);
		if (len > MAX_LANCAM_TITLE_LEN)
			break;
		strcpy(bios.model_name, argm->value);
		bios_data_write(&bios, BIOS_AUTO_CORRECT);
		if (ControlSystemData(SFIELD_SET_SUB_MACHINECODE, (void *)&bios.sub_machine_code, sizeof(bios.sub_machine_code), &req->curr_user) < 0) {
			//renew_submcode();
			break;
		}
		//renew_submcode();

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

void channel_plan(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;
	if (bios_data_read(&bios) < 0)
		bios_data_set_default(&bios);

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%02x\n", argm->name, bios.reg_domain);
		return;
	}

	do {
		__u32 value = strtol(argm->value, NULL, 16);
		if (value > 0x0a)
			break;
		bios.reg_domain = value;
		bios_data_write(&bios, BIOS_AUTO_CORRECT);

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

void set_machsubcode(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;
	
	do {
		__u32 subcode = strtol(argm->value, NULL, 16);

		if (bios_data_read(&bios) < 0)
			bios_data_set_default(&bios);

//		bios.brand = (subcode >> 12) & 0xff;
		bios.video_system = (subcode & 0x00004) ? VIDEO_SYSTEM_PAL : VIDEO_SYSTEM_NTSC;
//		bios.dn_ctrl = (subcode & 0x00100) ? 1 : 0;
		if (bios_data_write(&bios, BIOS_AUTO_CORRECT) < 0)
			break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_machsubcode(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;
	__u32 subcode;
	
	if (bios_data_read(&bios) < 0)
		bios_data_set_default(&bios);

//	subcode = get_brand_id() << 12;
	if (bios.video_system == VIDEO_SYSTEM_PAL)
		subcode |= 0x00004;
//	if (bios.dn_ctrl)
//		subcode |= 0x00100;
		
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%05x\n", argm->name, subcode);
}
void get_brandid(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;
	__u32 brandid;
	
	if (bios_data_read(&bios) < 0)
		bios_data_set_default(&bios);

	brandid = get_brand_id();
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, brandid);
}

void get_mac0(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	__u8 mac[MAC_LENGTH];
//	memcpy( mac , sys_info->config.lan_config.net.MAC , sizeof(sys_info->config.lan_config.net.MAC));
	net_get_hwaddr(sys_info->ifether, mac);  //kevinlee eth0 or ppp0

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%02X:%02X:%02X:%02X:%02X:%02X\n", argm->name , mac[0] , mac[1] , mac[2] , mac[3] , mac[4] , mac[5] );
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_mac(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;
	
	do {
		if (sys_info->wifi_active) // cannot set wifi mac
			break;

		if (bios_data_read(&bios) < 0)
			bios_data_set_default(&bios);
		if (hex_to_mac(bios.mac0, argm->value) < 0)
			break;

		if (!is_valid_ether_addr(bios.mac0)) {
			dbg("Invalid ethernet MAC address.\n");
			break;
		}
		
		if (bios_data_write(&bios, BIOS_AUTO_CORRECT) < 0)
			break;

		req->req_flag.set_mac = 1;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void boot_message(request *req, COMMAND_ARGUMENT *argm)
{
	BIOS_DATA bios;
	
	if (bios_data_read(&bios) < 0)
		bios_data_set_default(&bios);
		
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, bios.boot_message);
		return;
	}

	do {
		__u32 ret = atoi(argm->value);

		if (ret > 1)
			break;
		bios.boot_message = ret;
		if (bios_data_write(&bios, BIOS_AUTO_CORRECT) < 0)
			break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void tv_output(request *req, COMMAND_ARGUMENT *argm)
{
	if (bios_data_read(&sys_info->bios) < 0)
		bios_data_set_default(&sys_info->bios);
		
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->bios.tv_output);
		return;
	}

	do {
		__u32 ret = atoi(argm->value);

		if (ret == 0)
			osd_encoder_disable();
		else if (ret == 1)
			osd_encoder_enable();
		else
			break;

		sys_info->bios.tv_output = ret;
		if (bios_data_write(&sys_info->bios, BIOS_AUTO_CORRECT) < 0)
			break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_maxaccount(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , ACOUNT_NUM );
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0 //disenable by jiahung 2010.01.06
void get_schedule(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	Schedule_t *schedule = sys_info->config.lan_config.aSchedules;
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d %02d:%02d:%02d %02d:%02d:%02d\n", argm->name , schedule->bStatus ? schedule->nDay : 0 , schedule->tStart.nHour , schedule->tStart.nMin , schedule->tStart.nSec , schedule->tDuration.nHour , schedule->tDuration.nMin , schedule->tDuration.nSec);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
void get_memoryleft(request *req, COMMAND_ARGUMENT *argm)
{
	char *pp;
	int mem_fd,len_mem;
	char buffer_mem[ 4096 + 1 ];
	unsigned long int memory_stats;
	mem_fd = open( "/proc/meminfo" , O_RDONLY);
	len_mem = read(mem_fd, buffer_mem, sizeof (buffer_mem) - 1 );
	close(mem_fd);
	buffer_mem[len_mem] = '\0';

  pp = buffer_mem;
	pp = skip_token(pp);
	memory_stats = strtoul(pp, & pp, 10 );
	pp = strchr(pp, '\n' );
	pp = skip_token(pp);
	memory_stats = strtoul(pp, & pp, 10 );
	memory_stats = memory_stats * 1024;
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%lu\n", argm->name , memory_stats );
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_maxpwdlen(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , MAX_PASS_SIZE );
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_maxnamelen(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , MAX_USER_SIZE );
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_maxfqdnlen(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , MAX_FQDN_LENGTH );
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void set_imagedefault(request *req, COMMAND_ARGUMENT *argm)
{
	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);

        if(value != 1)
            break;
        
		ControlSystemData(SFIELD_SET_IMAGEDEFAULT, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_chipcheck(request *req, COMMAND_ARGUMENT *argm)
{
#if	defined( SENSOR_USE_IMX035 )

	__u8 value = 0;
	do {

		ControlSystemData(SFIELD_CHIPCHECK, (void *)&value, sizeof(value), &req->curr_user);
		req->buffer_end += sprintf(req_bufptr(req), "chipcheck = %s\n", (sys_info->status.chipcheck == 0)?"OK":"FAIL");
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void get_cpuversion(request *req, COMMAND_ARGUMENT *argm)
{
	char *pp , *p;
	int cpu_fd,len_mem;
	char buffer_mem[ 4096 + 1 ];
	cpu_fd = open( "/proc/cpuinfo" , O_RDONLY);
	len_mem = read(cpu_fd, buffer_mem, sizeof (buffer_mem) - 1 );
	close(cpu_fd);
	buffer_mem[len_mem] = '\0';

  pp = buffer_mem;
	pp = skip_token(pp);
	pp += 2;
	p = strchr(pp, '\n' );
	*p = '\0';

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , pp);
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void get_localcpu(request *req, COMMAND_ARGUMENT *argm)
{
	char *pp , *p;
	int mem_fd,len_mem;
	char buffer_mem[ 4096 + 1 ];
	mem_fd = open( "/proc/cpuinfo" , O_RDONLY);
	len_mem = read(mem_fd, buffer_mem, sizeof (buffer_mem) - 1 );
	close(mem_fd);
	buffer_mem[len_mem] = '\0';

  pp = buffer_mem;
	while(strncmp(pp,"Hardware",strlen("Hardware"))){
		pp = strchr(pp, '\n' );
		pp++;
	}
	pp = skip_token(pp);
	pp += 2;
	p = strchr(pp, '\n' );
	*p = '\0';

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , pp);
		return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0 //tmp jiahung
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ftppassword(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[33];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_FTP_PASSWORD, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.ftp_config.password, argm->value)) {
			ControlSystemData(SFIELD_SET_FTP_PASSWORD, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ftpuser(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[33];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_FTP_USERNAME, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.ftp_config.username, argm->value)) {
			ControlSystemData(SFIELD_SET_FTP_USERNAME, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ftpjpegname(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[33];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_FTP_USERNAME, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.ftp_config.jpegname, argm->value)) {
			ControlSystemData(SFIELD_SET_FTP_JPEGNAME, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name,sys_info->config.ftp_config.jpegname);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ftppath(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[256];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_FTP_FOLDNAME, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.ftp_config.foldername, argm->value)) {
			ControlSystemData(SFIELD_SET_FTP_FOLDNAME, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_smtppwd(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[256];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_SMTP_PASSWORD, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.smtp_config.password, argm->value)) {
			ControlSystemData(SFIELD_SET_SMTP_PASSWORD, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_smtpport(request *req, COMMAND_ARGUMENT *argm)
{
	__u16 value = atoi(argm->value);

	do {
		if (value < MIN_SMTP_PORT && value != SMTP_PORT_DEFAULT)
			break;
		
		if (sys_info->config.smtp_config.port != value )
			ControlSystemData(SFIELD_SET_SMTP_PORT, (void *)&value, sizeof(value), &req->curr_user);

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,sys_info->config.smtp_config.port);
		return;
	} while (0);
	
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ftpip(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->config.ftp_config.servier_ip);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ftppiport(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.ftp_config.port);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ftppath(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->config.ftp_config.foldername);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ftppassword(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->config.ftp_config.password);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_smtpuser(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[256];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_SMTP_USERNAME, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.smtp_config.username, argm->value)) {
			ControlSystemData(SFIELD_SET_SMTP_USERNAME, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_ftpuser(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->config.ftp_config.username);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_smtpto(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[256];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_SMTP_RECEIVER_EMAIL_ADDRESS, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.smtp_config.receiver_email, argm->value)) {
			ControlSystemData(SFIELD_SET_SMTP_RECEIVER_EMAIL_ADDRESS, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_smtpsender(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[256];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_SMTP_SENDER_EMAIL_ADDRESS, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.smtp_config.sender_email, argm->value)) {
			ControlSystemData(SFIELD_SET_SMTP_SENDER_EMAIL_ADDRESS, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_emailuser(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->config.smtp_config.receiver_email);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_smtpfqdn(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[256];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_SMTP_SERVER_IP, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.smtp_config.servier_ip, argm->value)) {
			ControlSystemData(SFIELD_SET_SMTP_SERVER_IP, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_smtpip(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/

	do {
		/*if( pSysInfo == NULL )
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name , sys_info->config.smtp_config.servier_ip);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
void get_jpegxsize(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		/*if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Jpeg1.xsize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#endif
void get_audioinvolume(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.audioinvolume);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[AUDIOINVOLUME].config.u8);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#if 0
#if 0 // disable by Alex, 2009.04.07
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_gioinenable(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.gioin1enable);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_gioouttype(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.gioouttype);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_giooutenable(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.giooutenable);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_giointype(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.gioin1type);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif 

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
void get_jpegysize(request *req, COMMAND_ARGUMENT *argm)
{
	/*SysInfo* pSysInfo = GetSysInfo();*/
	do {
		/*if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Jpeg1.ysize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ftpfqdn(request *req, COMMAND_ARGUMENT *argm)
{
	/*char sys_value[256];*/

	do {
		/*if (ControlSystemData(SFIELD_GET_FTP_SERVER_IP, (void *)sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (strcmp(sys_info->config.ftp_config.servier_ip, argm->value)) {
			ControlSystemData(SFIELD_SET_FTP_SERVER_IP, (void *)argm->value, strlen(argm->value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_smtpauth(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned char /*sys_value,*/ value = atoi(argm->value);

	do {
		/*if (ControlSystemData(SFIELD_GET_SMTP_AUTHENTICATION, (void *)&sys_value, sizeof(sys_value), NULL) < 0)
			break;*/

		if (sys_info->config.smtp_config.authentication != value) {
			ControlSystemData(SFIELD_SET_SMTP_AUTHENTICATION, (void *)&value, sizeof(value), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ftpport(request *req, COMMAND_ARGUMENT *argm)
{
	__u16 port = atoi(argm->value);

	do {
		if(port < MIN_FTP_PORT && port != FTP_PORT_DEFAULT)
			break;
		
		if (sys_info->config.ftp_config.port != port)
			ControlSystemData(SFIELD_SET_FTP_PORT, (void *)&port, sizeof(port), &req->curr_user);

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name,port);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_ftpmode(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 mode = atoi(argm->value);

	do {
		/*if (ControlSystemData(SFIELD_GET_FTP_PORT, (void *)&sys_port, sizeof(sys_port), NULL) < 0)
			break;*/
		if (sys_info->config.ftp_config.mode != mode) {
			ControlSystemData(SFIELD_SET_FTP_MODE, (void *)&mode, sizeof(mode), &req->curr_user);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, mode ? "Passive Mode" : "Active Mode ");
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
#if 0
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream1xsize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Jpeg1.xsize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream1ysize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Jpeg1.ysize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream2xsize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Jpeg2.xsize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream2ysize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Jpeg2.ysize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream3xsize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Mpeg41.xsize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream3ysize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Mpeg41.ysize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream4xsize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Mpeg42.xsize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void stream4ysize(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		/*SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;*/
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.Mpeg42.ysize);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
 #endif //tmp
void set_ipncptz(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_RS485
//	struct timeval tv;
//	int ret;
/*	char tmp[16];


	osd_init();
	ret = gettimeofday(&tv, NULL);
	if(ret)
		dbg("error");
	else{
		
		dbg("sec = %ld, usec = %ld", (long)tv.tv_sec, (long)tv.tv_usec);
		sprintf(tmp, "%ld_%ld", (long)tv.tv_sec, (long)tv.tv_usec);
		osd_print(5, 5, tmp);
	}

	ret = gettimeofday(&tv, NULL);
	if(ret)
		dbg("error");
	else{
		
		dbg("sec = %ld, usec = %ld", (long)tv.tv_sec, (long)tv.tv_usec);
		sprintf(tmp, "%ld_%ld", (long)tv.tv_sec, (long)tv.tv_usec);
		osd_print(5, 7, tmp);
	}
	
*/
	char buf[17] = "";
	if(sys_info->ipcam[RS485_PROTOCOL].config.u8 == 0)
	//if(sys_info->config.rs485.protocol == 0)
		strncpy(buf, argm->value, 14);		//protocol D
	else
		strncpy(buf, argm->value, 16);
	
	//dbg("buf=%s\n", buf);

	do {
		if(sys_info->ipcam[RS485_ENABLE].config.u8 == 0)
			break;

		sys_info->osd_uart = 1;
		
		if(rs485_write_data(buf) == -1)
			break;
		//if(ControlSystemData(SFIELD_IPNCPTZ, argm->value, strlen(argm->value), &req->curr_user) < 0)
		//	break;

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void init_ipncptz(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		if(ControlSystemData(SFIELD_INIT_IPNCPTZ, argm->value, strlen(argm->value), &req->curr_user) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_timeformat(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[TIME_FORMAT].config.u8);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[TIME_FORMAT].max)
            break;
        
        if(sys_info->ipcam[TIME_FORMAT].config.u8 != value)
            ControlSystemData(SFIELD_SET_TIMEFORMAT, (void *)&value, sizeof(value), &req->curr_user);
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_daylight(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DST_ENABLE].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);

        if(value > sys_info->ipcam[DST_ENABLE].max)
            break;
        
		if(sys_info->ipcam[DST_ENABLE].config.u8 != value) {
			ControlSystemData(SFIELD_SET_DAYLIGHT, (void *)&value, sizeof(value), &req->curr_user);

            setenv("TZ", sys_info->tzenv, 1);
            tzset();
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_dstshift(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DST_SHIFT].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);

        if(value > sys_info->ipcam[DST_SHIFT].max)
            break;
        
		if(sys_info->ipcam[DST_SHIFT].config.u8!= value) {
			ControlSystemData(SFIELD_SET_DST_SHIFT, (void *)&value, sizeof(value), &req->curr_user);
            
            setenv("TZ", sys_info->tzenv, 1);
            tzset();
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_dstmode(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DST_MODE].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        int value = atoi(argm->value);

        if(value > sys_info->ipcam[DST_MODE].max)
            break;
        
		if(sys_info->ipcam[DST_MODE].config.u8 != value) {
			ControlSystemData(SFIELD_SET_DST_MODE, (void *)&value, sizeof(value), &req->curr_user);

            setenv("TZ", sys_info->tzenv, 1);
            tzset();
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_dststart(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d:%d:%d:%d\n", argm->name, 
            sys_info->ipcam[DSTSTART_MONTH].config.u8, sys_info->ipcam[DSTSTART_WEEK].config.u8,
            sys_info->ipcam[DSTSTART_DAY].config.u8, sys_info->ipcam[DSTSTART_HOUR].config.u8,
            sys_info->ipcam[DSTSTART_MINUTE].config.u8);
        return;
    }

    dbg("%s", argm->value);
    
    do {
        if(ControlSystemData(SFIELD_SET_DST_START, (void *)argm->value, strlen(argm->value) + 1, &req->curr_user) < 0)
            break;

        setenv("TZ", sys_info->tzenv, 1);
        tzset();
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_dstend(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d:%d:%d:%d\n", argm->name, 
            sys_info->ipcam[DSTEND_MONTH].config.u8, sys_info->ipcam[DSTEND_WEEK].config.u8,
            sys_info->ipcam[DSTEND_DAY].config.u8, sys_info->ipcam[DSTEND_HOUR].config.u8,
            sys_info->ipcam[DSTEND_MINUTE].config.u8);
        return;
    }
    
	dbg("%s", argm->value);

	do {
		if(ControlSystemData(SFIELD_SET_DST_END, (void *)argm->value, strlen(argm->value) + 1, &req->curr_user) < 0)
			break;

        setenv("TZ", sys_info->tzenv, 1);
        tzset();

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_timezone(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_WIN_TIMEZONE
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#else
	do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);

		if (value > 24)
			break;
		
		//if(sys_info->config.lan_config.net.ntp_timezone != value)
		if(sys_info->ipcam[NTP_TIMEZONE].config.u8 != value)
			ControlSystemData(SFIELD_SET_TIMEZONE, (void *)&value, sizeof(value), &req->curr_user);
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if 0
void set_allgioinenable(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	
	do {
		if(sys_info->ipcam[GIOIN_MAIN_EN].config.u8 != value)
			info_write(&value, GIOIN_MAIN_EN, 0, 0);
		
		//if(sys_info->config.lan_config.gioin_main_en != value)
			//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.gioin_main_en), sizeof(value), 0);
			//if(ControlSystemData(SFIELD_SET_GIOINENABLE, &value, sizeof(value), &req->curr_user) < 0)
			//	break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_allgiooutenable(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	
	do {
		if(sys_info->ipcam[GIOOUT_MAIN_EN].config.u8 != value)
			info_write(&value, GIOOUT_MAIN_EN, 0, 0);
		
		//if(sys_info->config.lan_config.gioout_main_en != value)
			//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.gioout_main_en), sizeof(value), 0);
			//if(ControlSystemData(SFIELD_SET_GIOINENABLE, &value, sizeof(value), &req->curr_user) < 0)
			//	break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_gioinenable(request *req, COMMAND_ARGUMENT *argm)
{
    int id, enable;
    __u32 value = 0;

    if(GIOIN_NUM == 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }

    if (!argm->value[0]) {
        int i;
        for ( i = 1; i <= GIOIN_NUM; i++ ) {
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", 
                    argm->name, i, sys_info->ipcam[GIOIN0_ENABLE+i*4].config.u16);
        }
        return;
    }

    do {
        if(strchr(argm->value, ':')) {
            if(sscanf(argm->value, "%d:%d", &id, &enable) != 2)
                break;
        }
        else {
            id = 1;
            enable = atoi(argm->value);
        }

        if(id < 1 || id > GIOIN_NUM)
            break;

        if(enable < sys_info->ipcam[GIOIN0_ENABLE+4*id].min || enable > sys_info->ipcam[GIOIN0_ENABLE+4*id].max)
            break;
        
        if(sys_info->ipcam[GIOIN0_ENABLE+4*id].config.u16 != enable) {      
            value = (id << 16) | enable;
            if(ControlSystemData(SFIELD_SET_GIOINENABLE, &value, sizeof(value), &req->curr_user) < 0)
                break;
        }

        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_giointype(request *req, COMMAND_ARGUMENT *argm)
{
    int i;

    if(GIOIN_NUM == 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }
    
    do {
        if (!argm->value[0]) {
            for ( i = 1; i <= GIOIN_NUM; i++ ) {
                req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", argm->name, i, sys_info->ipcam[GIOIN0_TYPE+i*4].config.u16);
                //req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", argm->name, i, sys_info->config.lan_config.gioin[i].type);
            }
            return;
        }
        
        i = atoi(argm->value);
        if( i < 1 || i > GIOIN_NUM) {
            req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
            return;
        }

    } while (0);

    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", argm->name, i, sys_info->ipcam[GIOIN0_TYPE+i*4].config.u16);
    return;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_giointype(request *req, COMMAND_ARGUMENT *argm)
{
    int id, type;
    __u32 value = 0;

    if(GIOIN_NUM == 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }

    if (!argm->value[0]) {
        int i;
        for ( i = 1; i <= GIOIN_NUM; i++ ) {
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", 
                    argm->name, i, sys_info->ipcam[GIOIN0_TYPE+i*4].config.u16);
        }
        return;
    }


    do {
        if(strchr(argm->value, ':')) {
            if(sscanf(argm->value, "%d:%d", &id, &type) != 2)
                break;
        }
        else {
            id = 1;
            type = atoi(argm->value);
        }

        if(id < 1 || id > GIOIN_NUM)
            break;

        if(value < sys_info->ipcam[GIOIN0_TYPE+4*id].min || value > sys_info->ipcam[GIOIN0_TYPE+4*id].max)
            break;
        
        if(sys_info->ipcam[GIOIN0_TYPE+id*4].config.u16 != type) {
            value = (id << 16) | type;
            if(ControlSystemData(SFIELD_SET_GIOINTYPE, &value, sizeof(value), &req->curr_user) < 0)
                break;
        }
            
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_giooutenable(request *req, COMMAND_ARGUMENT *argm)
{
    int id, enable;
    __u32 value = 0;

    if(GIOOUT_NUM == 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }

    if (!argm->value[0]) {
        int i;
        for ( i = 1; i <= GIOOUT_NUM; i++ ) {
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", 
                    argm->name, i, sys_info->ipcam[GIOOUT0_ENABLE+i*4].config.u16);
        }
        return;
    }

    do {
        if(strchr(argm->value, ':')) {
            if(sscanf(argm->value, "%d:%d", &id, &enable) != 2)
                break;
        }
        else {
            id = 1;
            enable = atoi(argm->value);
        }

        if(id < 1 || id > GIOOUT_NUM)
            break;

        if(enable < sys_info->ipcam[GIOOUT0_ENABLE+4*id].min || enable > sys_info->ipcam[GIOOUT0_ENABLE+4*id].max)
            break;

        if(sys_info->ipcam[GIOOUT0_ENABLE+id*4].config.u16 != enable) {
            value = (id << 16) | enable;
            if(ControlSystemData(SFIELD_SET_GIOOUTENABLE, &value, sizeof(value), &req->curr_user) < 0)
                break;
        }
            
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_gioouttype(request *req, COMMAND_ARGUMENT *argm)
{
    int id, type;
    __u32 value = 0;

    if(GIOOUT_NUM == 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }

    if (!argm->value[0]) {
        int i;
        for ( i = 1; i <= GIOOUT_NUM; i++ ) {
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", 
                    argm->name, i, sys_info->ipcam[GIOOUT0_TYPE+i*4].config.u16);
        }
        return;
    }

    do {        
        if(strchr(argm->value, ':')) {
            if(sscanf(argm->value, "%d:%d", &id, &type) != 2)
                break;
        }
        else {
            id = 1;
            type = atoi(argm->value);
        }

        if(id < 1 || id > GIOOUT_NUM)
            break;

        if(value < sys_info->ipcam[GIOOUT0_TYPE+4*id].min || value > sys_info->ipcam[GIOOUT0_TYPE+4*id].max)
            break;
        
        if(sys_info->ipcam[GIOOUT0_TYPE+4*id].config.u16 != type) {
            value = (id << 16) | type;
            if(ControlSystemData(SFIELD_SET_GIOOUTTYPE, &value, sizeof(value), &req->curr_user) < 0)
                break;
        }
            
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_giooutalwayson(request *req, COMMAND_ARGUMENT *argm)
{
    int id, enable;
    __u32 value = 0;

    if(GIOOUT_NUM == 0) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
        return;
    }

    if (!argm->value[0]) {
        int i;
        for ( i = 1; i <= GIOOUT_NUM; i++ ) {
            req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d:%d\n", 
                    argm->name, i, sys_info->ipcam[GIOOUT_ALWAYSON0+i].config.u16);
        }
        return;
    }

    do {
        if(strchr(argm->value, ':')) {
            if(sscanf(argm->value, "%d:%d", &id, &enable) != 2)
                break;
        }
        else {
            id = 1;
            enable = atoi(argm->value);
        }
        
        if(id < 1 || id > GIOOUT_NUM)
            break;

        if(enable < sys_info->ipcam[GIOOUT_ALWAYSON0+id].min || enable > sys_info->ipcam[GIOOUT_ALWAYSON0+id].max)
            break;
        
        if(sys_info->ipcam[GIOOUT_ALWAYSON0+id].config.u16 != enable) {
			
            value = (id << 16) | enable;
            if(ControlSystemData(SFIELD_SET_GIOOUT_ALWAYSON, &value, sizeof(value), &req->curr_user) < 0)
                break;
        }
            
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_giooutreset(request *req, COMMAND_ARGUMENT *argm)
{
	//__u8 value = atoi(argm->value);
	int id, enable;
//	__u32 value = 0;
	
	do {
		if(!GIOOUT_NUM)	
			break;
		
		if(strchr(argm->value, ':')) {
			if(sscanf(argm->value, "%d:%d", &id, &enable) != 2)
				break;
		}
		else {
			id = 1;
			enable = atoi(argm->value);
		}

		if(id < 1 || id > GIOOUT_NUM)
			break;

		sys_info->gioout_time = 0;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_tstampenable(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_TSTAMP

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[TSTAMPENABLE].max)
            break;
        
		if(sys_info->ipcam[TSTAMPENABLE].config.u8 != value)
		//if(sys_info->config.lan_config.tstampenable != value)
			if(ControlSystemData(SFIELD_SET_TSTAMPENABLE, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_tstampformat(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_TSTAMP
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[TIME_FORMAT].config.u8);
		return;
	}

//this is used show time function for D-Link

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

		//if(sys_info->config.lan_config.tstampformat != value)	//mark by mikewang
			if(ControlSystemData(SFIELD_SET_TSTAMPFORMAT, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_tstampcolor(request *req, COMMAND_ARGUMENT *argm)//add by mikewang
{
#ifdef SUPPORT_TSTAMP
		
	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

		if(sys_info->ipcam[TSTAMPCOLOR].config.u8 != value)
		//if(sys_info->config.lan_config.tstampcolor != value)
			if(ControlSystemData(SFIELD_SET_TSTAMPCOLOR, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_tstamploc(request *req, COMMAND_ARGUMENT *argm)//add by mikewang
{
#ifdef SUPPORT_TSTAMP

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[TSTAMPLOCATION].max)
            break;
        
		//if(sys_info->config.lan_config.tstamplocation != value)	//mark by mikewang
			if(ControlSystemData(SFIELD_SET_TSTAMPLOCATION, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_tstamplabel(request *req, COMMAND_ARGUMENT *argm)//add by mikewang
{
#ifdef SUPPORT_TSTAMP

	do {
        if(strlen(argm->value) > sys_info->ipcam[TSTAMPLABEL].max)
            break;
		
        if(strchr(argm->value, '"'))
			break;
		
		//if (strcmp(sys_info->config.lan_config.tstamplabel, argm->value)) {	//mark by mikewang
			ControlSystemData(SFIELD_SET_TSTAMP_LABEL, (void *)argm->value, strlen(argm->value), &req->curr_user);
		//}
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif

}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_audiotype(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s\n", argm->name, conf_string_read(AUDIO_INFO_CODEC_NAME));
        return;
    }

	do {
        if( strcmp( argm->value , AUDIO_CODEC_G711 ) != 0 &&
			strcmp( argm->value , AUDIO_CODEC_G726 ) != 0 &&
			strcmp( argm->value , AUDIO_CODEC_AAC ) != 0 ){
			break;
		}
		if( strcmp( conf_string_read(AUDIO_INFO_CODEC_NAME) , argm->value) != 0 ){
			if( sys_info->osd_debug.avserver_restart == OSD_DEBUG_ENABLE ){
				osd_print( 3, 11 , "set_audiotype set reset_status");
			}
			sys_info->procs_status.reset_status = NEED_RESET;
			info_write(argm->value, AUDIO_INFO_CODEC_NAME, HTTP_OPTION_LEN, 0);
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			#if 1
			snprintf(logstr, MSG_USE_LEN, "%s SET AUDIO ENCODING TYPE %s", vistor, argm->value);
			#else
			snprintf(logstr, MSG_USE_LEN, "%s SET AUDIO ENCODING TYPE %s %s", vistor, argm->value, sys_info->procs_status.reset_status == 0 ? "Dont Reset" : "Need Reset");
			#endif
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#else
		//sysinfo_write(argm->value, offsetof(SysInfo, config.audio_info.codec_name), HTTP_OPTION_LEN, 0);
#endif
		}

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_audioinenable(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AUDIOINENABLE].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

#ifdef CONFIG_BRAND_DLINK
        value = !value;
#endif

		if(value > sys_info->ipcam[AUDIOINENABLE].max)
			break;
		
		if(sys_info->ipcam[AUDIOINENABLE].config.u8 != value)
			ControlSystemData(SFIELD_SET_AUDIOENABLE, (void *)&value, sizeof(value), &req->curr_user);
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	return;
	} while (0);

  req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_audioinvolume(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	do {
		if(sys_info->ipcam[AUDIOINVOLUME].config.u8 != value)
		//if(sys_info->config.lan_config.audioinvolume != value)
			if(ControlSystemData(SFIELD_SET_AUDIOINVOLUME, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_audiooutenable(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AUDIOOUTENABLE].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);
#ifdef CONFIG_BRAND_DLINK
        value = !value;
#endif

        if(value > sys_info->ipcam[AUDIOOUTENABLE].max)
            break;
        
		dbg("value = %d", value);
		if(sys_info->ipcam[AUDIOOUTENABLE].config.u8 != value)
			if(ControlSystemData(SFIELD_SET_AUDIOOUTENABLE, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_audiooutvolume(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AUDIOOUTVOLUME].config.u8);
        return;
    }

	__u8 value = atoi(argm->value);
	do {
        if(value < sys_info->ipcam[AUDIOOUTVOLUME].min || value > sys_info->ipcam[AUDIOOUTVOLUME].max)
            break;
        
		if(sys_info->ipcam[AUDIOOUTVOLUME].config.u8 != value)
		//if(sys_info->config.lan_config.audiooutvolume != value)
			if(ControlSystemData(SFIELD_SET_AUDIOOUTVOLUME, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void set_audioinsourceswitch(request *req, COMMAND_ARGUMENT *argm)
{
#if defined ( SUPPORT_AUDIO_SWITCH )
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AUDIOINSOURCESWITCH].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[AUDIOINSOURCESWITCH].max)
            break;
        
		if(sys_info->ipcam[AUDIOINSOURCESWITCH].config.u8 != value)
		//if(sys_info->config.lan_config.audioinsourceswitch != value)
			if(ControlSystemData(SFIELD_SET_AUDIOINSOURCESWITCH, &value, sizeof(value), &req->curr_user) < 0)
				break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#endif
}

void audioamplifyratio(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0])
	{
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%f\n", argm->name, sys_info->audioamplifyratio);
		return;
	}

	do
	{
	/*
		if(numeric_check(argm->value) < 0)
			break;
	*/
		sys_info->audioamplifyratio = atof(argm->value); 
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	}
	while (0);

	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

void audiotest(request *req, COMMAND_ARGUMENT *argm)
{
	__u8 value = atoi(argm->value);
	if(value == 1){
		system("killall -9 audiotest");
		system("/opt/ipnc/audiotest &");
	}else if(value == 2){
		system("killall -9 audiotest");
		system("/opt/ipnc/audiotest speakstreamtest &");
	}else
		system("killall -9 audiotest");
}
/*
void rs485test(request *req, COMMAND_ARGUMENT *argm)
{
	struct timeval tv;
	int ret;
	char tmp[16];
	osd_init();
	ret = gettimeofday(&tv, NULL);
	if(ret)
		dbg("error");
	else{
		
		dbg("sec = %ld, usec = %ld", (long)tv.tv_sec, (long)tv.tv_usec);
		sprintf(tmp, "%ld_%ld", (long)tv.tv_sec, (long)tv.tv_usec);
		osd_print(5, 5, tmp);
	}
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_daylight(request *req, COMMAND_ARGUMENT *argm)
{

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->ipcam[NDAYNIGHT].config.u8 );
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->config.lan_config.nDayNight);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_regdomain(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
return value
NG adduser=1	, UI response "System administrator privileges can not be changed!".
UA adduser	, UI response "Permission denied!".
*/
void add_user(request *req, COMMAND_ARGUMENT *argm)
{
	int i, blank_idx = -1;
	int name_len, pass_len;
	char *user_name, *user_pass, *auth;
	AUTHORITY authority;
	USER_INFO user;//, *account = sys_info->config.acounts;
	BIOS_DATA bios;
	int brand=0, min_pwd_len=0;
	
#ifdef MSGLOGNEW
	char loginip[20];
	char logstr[MSG_MAX_LEN];
	char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
#else
	SYSLOG tlog;
#endif

	do {
		user_name = argm->value;
		
		if ( (user_pass = strchr(user_name, ':')) == NULL )
			break;
		*user_pass++ = 0;

		name_len = strlen(user_name);
		if ((name_len > MAX_USER_SIZE) || (name_len < MIN_USER_SIZE))
			break;

		if ( (auth = strchr(user_pass, ':')) == NULL )
			break;
		*auth++ = 0;

		pass_len = strlen(user_pass);
		if (pass_len > MAX_PASS_SIZE)
			break;
		
		bios_data_read(&bios);
		brand = get_brand_id();

		if(brand == BRAND_ID_FMM)
			min_pwd_len = 8;

		if (pass_len < min_pwd_len)
			break;

		authority = atoi(auth);
		if ((authority > AUTHORITY_VIEWER) || (authority < AUTHORITY_ADMIN))
			break;

		if (strcasecmp(user_name, req->connect->user_name) == 0) {
			if(authority > sys_info->ipcam[ACOUNTS0_AUTHORITY+req->connect->user_idx*3].config.u8){		// Permissions fall
				if(strcasecmp(conf_string_read(ACOUNTS0_NAME), req->connect->user_name) == 0){
					req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
					DBG("System administrator privileges can not be changed!\n");
					return;
				}	
			}else if(authority < sys_info->ipcam[ACOUNTS0_AUTHORITY+req->connect->user_idx*3].config.u8){	// Permissions Increased
				if(strcasecmp(conf_string_read(ACOUNTS0_NAME), req->connect->user_name) != 0){
					req->buffer_end += sprintf(req_bufptr(req), OPTION_UA "%s\n", argm->name);
					DBG("Only an administrator can change the permissions!\n");
					return;
				}	
			}//else
			//	dbg("Does not change the permissions");
			
			info_write(user_pass, ACOUNTS0_PASSWD+req->connect->user_idx*3, pass_len, 0);
            info_write(&authority, ACOUNTS0_AUTHORITY+req->connect->user_idx*3, 0, 0);  // Alex. 2010-09-30
			//sysinfo_write(user_pass, offsetof(SysInfo, config.acounts[req->connect->user_idx].passwd), pass_len+1, 0);
			sys_info->user_auth_mode = 1; //jiahung
			//sys_info->user_auth_mode = get_user_auth_mode(sys_info->config.acounts);
			// FIX_ME : SFIELD_ADD_USER
			// ControlSystemData(SFIELD_ADD_USER, (void *)&user, sizeof(user), &req->curr_user);
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		}
		else if (req->authority != AUTHORITY_ADMIN) {
			req->buffer_end += sprintf(req_bufptr(req), OPTION_UA "%s\n", argm->name);
			DBG("You must have administrator authority!\n");
			return;
		}

		for (i=0; i<ACOUNT_NUM; i++) {
			if (blank_idx<0 && (strcmp(conf_string_read( ACOUNTS0_NAME+i*3), "") == 0))
			//if (blank_idx<0 && !account[i].name[0])
				blank_idx = i;
			if (strcasecmp(user_name, conf_string_read( ACOUNTS0_NAME+i*3)) == 0) {
			//if (strcasecmp(user_name, account[i].name) == 0) {
				if(req->authority == AUTHORITY_ADMIN) {
					blank_idx = i;
					break;	// Alex, 2010-08-05
				}
				else {
					DBG("The specific user was existed!\n");
					goto add_user_fail;
				}
			}
		}

		if (blank_idx > 0) {
			memset(&user, 0, sizeof(USER_INFO));
			strcpy(user.name, user_name);
			strcpy(user.passwd, user_pass);
			user.authority = authority;
			dbg("blank_idx = %d\n", blank_idx);
			info_write(user_name, ACOUNTS0_NAME+blank_idx*3, strlen(user_name), 0);
			info_write(user_pass, ACOUNTS0_PASSWD+blank_idx*3, strlen(user_pass), 0);
			info_write(&authority, ACOUNTS0_AUTHORITY+blank_idx*3, 0, 0);
			//sysinfo_write(&user, offsetof(SysInfo, config.acounts[blank_idx]), sizeof(USER_INFO), 0);
//			ControlSystemData(SFIELD_ADD_USER, (void *)&user, sizeof(user), &req->curr_user)
		}
		else {
			DBG("The user list was full!\n");
			break;
		}

		sys_info->user_auth_mode = 1;
		//sys_info->user_auth_mode = get_user_auth_mode(sys_info->config.acounts);
		
#ifdef MSGLOGNEW
		strcpy(loginip,ipstr(req->curr_user.ip));
		sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
		snprintf(logstr, MSG_USE_LEN, "%s ADD %s %s", vistor, AuthorityName( authority), user_name);
		sprintf(logstr, "%s\r\n", logstr);
		sysdbglog(logstr);
#else
		tlog.opcode = SYSLOG_SET_ADDUSER_OK;
		tlog.ip = req->curr_user.ip;
		tlog.arg_0 = authority;
		strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
		strncpy(tlog.para, user_name, MAX_SYSLOG_PARA_SIZE-1);
		SetSysLog(&tlog);
#endif
		//ControlSystemData(SFIELD_ADD_USER, (void *)&user, sizeof(user), &req->curr_user);

		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
add_user_fail:

#ifdef MSGLOGNEW
	strcpy(loginip,ipstr(req->curr_user.ip));
	sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
	snprintf(logstr, MSG_USE_LEN, "%s ADD USER FAIL", vistor);
	sprintf(logstr, "%s\r\n", logstr);
	sysdbglog(logstr);
#else
	tlog.opcode = SYSLOG_SET_ADDUSER_FAIL;
	tlog.ip = req->curr_user.ip;
	strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
	SetSysLog(&tlog);
#endif
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_user(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned char value = atoi(argm->value);

	do {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s:*:%d\n", argm->name , conf_string_read( ACOUNTS0_NAME+3*value) , sys_info->ipcam[ACOUNTS0_AUTHORITY+3*value].config.u8);
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%s:*:%d\n", argm->name , sys_info->config.acounts[value].name , sys_info->config.acounts[value].authority);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_userleft(request *req, COMMAND_ARGUMENT *argm)
{
	int i, num = 0;
	//USER_INFO *account = sys_info->config.acounts;

	do {
		for(i=0; i<ACOUNT_NUM; i++) {
			if (!(*conf_string_read( ACOUNTS0_NAME)))
			//if (!account[i].name[0])
				num++;
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , num);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void del_user(request *req, COMMAND_ARGUMENT *argm)
{
	int i;
	//USER_INFO *account = sys_info->config.acounts;
	SYSLOG tlog;
#ifdef MSGLOGNEW
	char loginip[20];
	char logstr[MSG_MAX_LEN];
	char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
#endif

	tlog.opcode = SYSLOG_SET_DELUSER_FAIL;
	tlog.ip = req->curr_user.ip;
	strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);

	if (req->authority == AUTHORITY_ADMIN) {
		for (i=0; i<ACOUNT_NUM; i++) {
			if (strcasecmp(argm->value, conf_string_read( ACOUNTS0_NAME+3*i)) == 0) {
			//if (strcasecmp(argm->value, account[i].name) == 0) {
				USER_INFO user;
				if (i == 0) {
					DBG("You can not delete the administrator account!\n");
					break;;
				}
#ifdef CONFIG_BRAND_DLINK
				else if(i == 1){
					DBG("You can not delete the guest accunt!\n");
					break;
				}
#endif
				memset(&user, 0, sizeof(USER_INFO));
				user.authority = AUTHORITY_NONE;
				info_write(user.name, ACOUNTS0_NAME+i*3, strlen(user.name), 0);
				info_write(user.passwd, ACOUNTS0_PASSWD+i*3, strlen(user.passwd), 0);
				info_write(&user.authority, ACOUNTS0_AUTHORITY+i*3, 0, 0);
				//sysinfo_write(&user, offsetof(SysInfo, config.acounts[i]), sizeof(USER_INFO), 0);
				revoke_connection(argm->value);
				//ControlSystemData(SFIELD_DEL_USER, (void *)argm->value, strlen(argm->value)+1, &req->curr_user);
				req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
				sys_info->user_auth_mode = 1;
				//sys_info->user_auth_mode = get_user_auth_mode(sys_info->config.acounts);

				// Write system log
#ifdef MSGLOGNEW
				strcpy(loginip,ipstr(req->curr_user.ip));
				sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
				snprintf(logstr, MSG_USE_LEN, "%s DELETE USER %s", vistor, argm->value);
				sprintf(logstr, "%s\r\n", logstr);
				sysdbglog(logstr);
#else
				tlog.opcode = SYSLOG_SET_DELUSER_OK;
				strncpy(tlog.para, argm->value, MAX_SYSLOG_PARA_SIZE-1);
				SetSysLog(&tlog);
#endif
				
				return;
			}
		}
	}
	else
		DBG("You must have administrator authority!\n");

#ifdef MSGLOGNEW
	strcpy(loginip,ipstr(req->curr_user.ip));
	sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
	snprintf(logstr, MSG_USE_LEN, "%s DELETE USER FAIL", vistor);
	sprintf(logstr, "%s\r\n", logstr);
	sysdbglog(logstr);
#else
	SetSysLog(&tlog);
#endif
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void event_start(request *req, COMMAND_ARGUMENT *argm)
{
	int value = atoi(argm->value);
	do {
		if (value < 0)
			break;
		event_location = value;
	
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void language(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	return;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void restart_ipcam(request *req, COMMAND_ARGUMENT *argm)
{
    if(atoi(argm->value) != 1) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
        return;
    }

	//char cmd[256];
#ifndef MSGLOGNEW
	SYSLOG tlog;
#endif
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	//sprintf(cmd, "shutdown -r now\n");   //modified by jiahung, 2009.06.18
	//kill_video_processor();
	//sprintf(cmd, "reboot -f");
#ifdef SUPPORT_AF
	SetMCUWDT(0);
#endif
	req->req_flag.reboot = 1;
	//softboot();
#ifdef MSGLOGNEW
	char loginip[20];
	char logstr[MSG_MAX_LEN];
	char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
	strcpy(loginip,ipstr(req->curr_user.ip));
	sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
	snprintf(logstr, MSG_USE_LEN, "%s Reboot Device", vistor);
	sprintf(logstr, "%s\r\n", logstr);
	sysdbglog(logstr);
#else
	tlog.opcode = SYSLOG_SYSSET_NETREBOOT;
	tlog.ip = req->curr_user.ip;
	strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
	SetSysLog(&tlog);
#endif
	//system(cmd);
	//return;
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
void getalarmstatus(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		SysInfo* pSysInfo = GetSysInfo();
		if(pSysInfo == NULL)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04x\n",
		argm->name, pSysInfo->alarm_status);
		return;
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/ // disable by Alex, 2008.10.30
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
void sd_unmount(request *req, COMMAND_ARGUMENT *argm)
{
    __u8 value = atoi(argm->value);

    ControlSystemData(SFIELD_SD_UNMOUNT, (void *)&value, sizeof(value), NULL);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
    return;
}
*/ // disable  by Alex, 2008.10.28
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_factory_default(request *req, COMMAND_ARGUMENT *argm)
{   
	do{
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

#ifndef SUPPORT_FACTORYMODE
		if(value != 1)
            break;
#ifdef SUPPORT_VISCA
		if( remove(SEQUENCE_SAVE) != 0)
			dbg("remove %s failed %d\n", SEQUENCE_SAVE,__LINE__);							
#endif

		ControlSystemData(SFIELD_FACTORY_DEFAULT, &value, sizeof(value), &req->curr_user);
		
#else
        if(!(value & FLAG_LOAD_ALLDEF))
			break;

		dbg("value = %d\n", value);
		if(ControlSystemData(SFIELD_FACTORY_DEFAULT, &value, sizeof(value), &req->curr_user) < 0){
			DBG("SFIELD_FACTORY_DEFAULT ERROR\n");
			break;
		}
#endif

#ifdef SUPPORT_AF
		SetMCUWDT(0);
#endif
 		req->req_flag.reboot = 1;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
        
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
void get_supportfactorymode(request *req, COMMAND_ARGUMENT *argm)
{
 
#ifdef SUPPORT_FACTORYMODE
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
 		
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_mui(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[MUI].config.u8);
        return;
    }

    do {
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

        if(value > sys_info->ipcam[MUI].max)
            break;
        
        if(sys_info->ipcam[MUI].config.u8!= value)
            if(ControlSystemData(SFIELD_SET_MUI, &value, sizeof(value), &req->curr_user) < 0)
                break;
            
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
    } while (0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);  
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_gpio_dir(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		int dir;
		unsigned gpio = atoi(argm->value);

		if((dir = gio_get_direction(gpio)) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u:%d\n", argm->name, gpio, dir);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_gpio_dir(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		int dir;
		unsigned gpio;

		if (sscanf(argm->value, "%u:%d", &gpio, &dir) != 2)
			break;

		if (dir < 0 || dir > 1)
			break;

		if(gio_set_direction(gpio, dir) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u:%d\n", argm->name, gpio, dir);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_gpio_value(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		int state;
		unsigned gpio = atoi(argm->value);

		if((state = gio_get_value(gpio)) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u:%d\n", argm->name, gpio, state);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_gpio_value(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		int state;
		unsigned gpio;

		if (sscanf(argm->value, "%u:%d", &gpio, &state) != 2)
			break;

		if (state < 0 || state > 1)
			break;

		if(gio_set_value(gpio, state) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u:%d\n", argm->name, gpio, state);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_gpio_state(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		int state;
		unsigned gpio = atoi(argm->value);

		if((state = gio_get_state(gpio)) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u:%d\n", argm->name, gpio, state);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_gpio_state(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		int state;
		unsigned gpio;

		if (sscanf(argm->value, "%u:%d", &gpio, &state) != 2)
			break;

		if (state < 0 || state > 1)
			break;

		if(gio_set_state(gpio, state) < 0)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%u:%d\n", argm->name, gpio, state);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_arm_ioport(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		unsigned long addr;
		unsigned int data;

		if ( ioport_init() < 0 )
			break;
		if (sscanf(argm->value, "%lx", &addr) != 1)
			break;
		if(ioport_read_data(addr, &data) < 0)
			break;
		ioport_exit();
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0x%lX:0x%08X\n", argm->name, addr, data);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_arm_ioport(request *req, COMMAND_ARGUMENT *argm)
{
	do{
		unsigned long addr;
		unsigned int data;

		if ( ioport_init() < 0 )
			break;
		if (sscanf(argm->value, "%lx:%x", &addr, &data) != 2)
			break;

		if(ioport_write_data(addr, &data) < 0)
			break;
		ioport_exit();
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0x%lX:0x%08X\n", argm->name, addr, data);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}


/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void ccddatawrite(request *req, COMMAND_ARGUMENT *argm)  //
{
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )
	unsigned addr, data;
	char *strptr;

	strptr = argm->value;
	if ( strlen( strptr ) )
	{
		addr = 0;
		while ( *strptr && ( addr <= 0xFFFF ) )
		{
			switch ( *strptr )
			{
				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
					addr = ( addr << 4 ) + ( *strptr - '0' );
					break;
				case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
					addr = ( addr << 4 ) + ( *strptr - 'A' + 10 );
					break;
				case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
					addr = ( addr << 4 ) + ( *strptr - 'a' + 10 );
					break;
				case ':':
					break;
				default:
					goto ccddatawrite_error;
			}
			if ( *strptr == ':' )
				break;
			strptr++;
		}
		if ( *strptr != ':' )
			goto ccddatawrite_error;
		strptr++;
		data = 0;
		while ( *strptr && ( data <= 0xFFFF ) )
		{
			switch ( *strptr )
			{
				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
					data = ( data << 4 ) + ( *strptr - '0' );
					break;
				case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
					data = ( data << 4 ) + ( *strptr - 'A' + 10 );
					break;
				case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
					data = ( data << 4 ) + ( *strptr - 'a' + 10 );
					break;
				default:
					goto ccddatawrite_error;
			}
			if ( ! *strptr )
				break;
			strptr++;
		}
		if ( *strptr )
			goto ccddatawrite_error;
#ifdef PLAT_DM355	//mikewang , need to be fix
		if ( sensor_write( addr, data ) < 0 )
			goto ccddatawrite_error;
#endif
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
ccddatawrite_error:
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}


/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void ccddataread(request *req, COMMAND_ARGUMENT *argm)  //
{
#if defined( SENSOR_USE_MT9V136 ) || defined( SENSOR_USE_TVP5150 ) || defined IMGS_TS2713 || defined IMGS_ADV7180 || defined( SENSOR_USE_TVP5150_MDIN270 )
	unsigned addr, data;
	char *strptr;

	strptr = argm->value;
	if ( strlen( strptr ) )
	{
		addr = 0;
		while ( *strptr && ( addr <= 0xFFFF ) )
		{
			switch ( *strptr )
			{
				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
					addr = ( addr << 4 ) + ( *strptr - '0' );
					break;
				case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
					addr = ( addr << 4 ) + ( *strptr - 'A' + 10 );
					break;
				case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
					addr = ( addr << 4 ) + ( *strptr - 'a' + 10 );
					break;
				default:
					goto ccddataread_error;
			}
			if ( ! *strptr )
				break;
			strptr++;
		}
		if ( *strptr )
			goto ccddataread_error;
#ifdef PLAT_DM355	//mikewang , need to be fix
		if ( ( data = sensor_read( addr, 0 ) ) < 0 )
			goto ccddataread_error;
#endif
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s %X\n", argm->name, data );
		return;
	}
ccddataread_error:
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void https_clearkey(request *req, COMMAND_ARGUMENT *argm)
{
	do {
		if((unlink(SSL_KEY_PATH) < 0) && (errno != ENOENT)) {
			DBGERR("unlink " SSL_KEY_PATH "\n");
			break;
		}

		if((unlink(SSL_CSR_PATH) < 0) && (errno != ENOENT)) {
			DBGERR("unlink " SSL_KEY_PATH "\n");
			break;
		}

		if((unlink(SSL_CERT_PATH) < 0) && (errno != ENOENT)) {
			DBGERR("unlink " SSL_KEY_PATH "\n");
			break;
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
		
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#ifdef SERVER_SSL
inline int https_check_restart(int enable)
{
	extern int do_sock;
	struct stat st_pem;

	if (enable && (do_sock == 2)) {
		if ((stat(SSL_CERT_PATH, &st_pem) == 0) && S_ISREG(st_pem.st_mode))
			return 0;
	}
	return -1;
}
void https_enable(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[HTTPS_ENABLE].config.u8);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;
        
        __u8 value = atoi(argm->value);

		if (value > sys_info->ipcam[HTTPS_ENABLE].max)
			break;

		if ((sys_info->ipcam[HTTPS_ENABLE].config.u8 != value) || (https_check_restart(value) == 0)) {
		//if ((sys_info->config.https.https_enable != value) || (https_check_restart(value) == 0)) {
			ControlSystemData(SFIELD_SET_HTTPSENABLE, (void *)&value, sizeof(value), &req->curr_user);
			req->req_flag.restart = 1;
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#ifdef SERVER_SSL
void https_port(request *req, COMMAND_ARGUMENT *argm)
{
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[HTTPS_PORT].config.u16);
        return;
    }

	do {
        if(numeric_check(argm->value) < 0)
            break;

        unsigned short value = atoi(argm->value);

		if (value < sys_info->ipcam[HTTPS_PORT].min && value != HTTPS_PORT_DEFAULT)
			break;

		if( (value == sys_info->ipcam[HTTP_PORT].config.u16) || 
					(value == sys_info->ipcam[RTSP_PORT].config.u16) )
				break;

		if (sys_info->ipcam[HTTPS_PORT].config.u16 != value) {
		//if (sys_info->config.https.port != value) {
			ControlSystemData(SFIELD_SET_HTTPSPORT, (void *)&value, sizeof(value), &req->curr_user);
//			sysinfo_write(&value, offsetof(SysInfo, config.https.port), sizeof(value), 0);
//			raise(SIGHUP);
			req->req_flag.restart = 1;
		}
		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void giveupspeaktoken(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_TWOWAY_AUDIO
	int speakid = atoi(argm->value);

	do {
		if( sys_info->speaktokenid == speakid )
			sys_info->speaktokenid = 0;
		else break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name );
		return;
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s=\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void speaktokentime(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_TWOWAY_AUDIO

	int speakid = atoi(argm->value);

	do {
		if( sys_info->speaktokenid != speakid ) break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , sys_info->speaktokentime );
		return;
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s=\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void dncontrol_mode(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DNCONTROL

	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DNC_MODE].config.u8);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

		__u8 value = atoi(argm->value);
#ifndef MSGLOGNEW
		SYSLOG tlog;
#endif
		if (value >= sys_info->ipcam[DNC_MODE].max)
			break;
		
		if (sys_info->ipcam[DNC_MODE].config.u8 != value){
			info_write(&value, DNC_MODE, 0, 0);
			sys_info->cmd.dn_control |= DNC_CMD_MODE;

#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			snprintf(logstr, MSG_USE_LEN, "%s SET DN CONTROL %s MODE", vistor, dnmode_name(value));
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#else
			tlog.opcode = SYSLOG_SYSSET_DN_MODE;
			tlog.ip = req->curr_user.ip;
			strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
			tlog.arg_0 = value;
			SetSysLog(&tlog);
#endif
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
		
	} while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s=\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void dncontrol_sensitivity(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#if 0 //dm270
	if (!argm->value[0]) {
		//req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->config.lan_config.dnc_sensitivity);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[DNC_SENSITIVITY].config.u8);
		return;
	}

	do{
		__u8 value = atoi(argm->value);

		if (value >= 5)
			break;
			
		info_write(&value, DNC_SENSITIVITY, 0, 0);		
		//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.dnc_sensitivity), sizeof(value), 0);
		sys_info->cmd.dn_control |= DNC_CMD_SENSITIVITY;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#if	SUPPORT_DNC_EX			//add by jiahung, 2009.05.05
void dncontrol_scheduleex(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DNCONTROL

	int start_tmp = 0, end_tmp = 0;
#ifndef MSGLOGNEW
	SYSLOG tlog;
#endif
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%06u%06u\n", 
                argm->name, sys_info->ipcam[DNC_SCHEDULE_START].config.u32, sys_info->ipcam[DNC_SCHEDULE_END].config.u32);
		return;
	}
	
	do{
        if(sscanf(argm->value, "%06u%06u", &start_tmp, &end_tmp) != 2)
            break;

		//if(start_tmp > end_tmp)
		//	break;
		info_write(&start_tmp, DNC_SCHEDULE_START, 0, 0);	
		info_write(&end_tmp, DNC_SCHEDULE_END, 0, 0);	

		sys_info->cmd.dn_control |= DNC_CMD_SCHEDULE;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
#ifdef MSGLOGNEW
		char loginip[20];
		char logstr[MSG_MAX_LEN];
		char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
		strcpy(loginip,ipstr(req->curr_user.ip));
		sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
		snprintf(logstr, MSG_USE_LEN, "%s SET DN CONTROL SCHEDULE RANGE %04d to %04d", vistor, start_tmp/100, end_tmp/100);
		sprintf(logstr, "%s\r\n", logstr);
		sysdbglog(logstr);
#else	
		tlog.opcode = SYSLOG_SYSSET_DN_SCHEDULE;
		tlog.ip = req->curr_user.ip;
		strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
		SetSysLog(&tlog);
#endif
		return;

	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s=\n", argm->name );
#endif
}
#else
void dncontrol_schedule(request *req, COMMAND_ARGUMENT *argm)
{
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s\n", argm->name);
#if 0 //dm270
	SYSLOG tlog;
	
	do{
		__u32 value;
	
		if (sscanf(argm->value, "%x", &value) != 1)
			break;
		
		info_write(&value, DNC_SCHEDULE, 0, 0);	
		//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.dnc_schedule), sizeof(value), 0);
		sys_info->cmd.dn_control |= DNC_CMD_SCHEDULE;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);

		tlog.ip = req->curr_user.ip;
		#ifndef SENSOR_USE_TVP5150 || defined IMGS_TS2713 || defined IMGS_ADV7180
		strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
		#endif
		SetSysLog(&tlog);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#endif
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void dncontrol_d2n(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DNCONTROL

	__u16 value = atoi(argm->value);

	do{
		if (value < DNC_D2N_MIN || value > DNC_D2N_MAX)
			break;
			
		info_write(&value, DNC_D2N, 0, 0);		
		//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.dnc_d2n), sizeof(value), 0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s=\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void dncontrol_n2d(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef SUPPORT_DNCONTROL

	__u16 value = atoi(argm->value);

	do{
		if (value < DNC_N2D_MIN || value > DNC_N2D_MAX)
			break;
			
		info_write(&value, DNC_N2D, 0, 0);	
		//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.dnc_n2d), sizeof(value), 0);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NS "%s=\n", argm->name );
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_supportirled(request *req, COMMAND_ARGUMENT *argm)
{
#if (SUPPORT_DCPOWER == 1)
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=1\n", argm->name);
#else
    req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
#endif
}

void get_supportdcpower(request *req, COMMAND_ARGUMENT *argm)
{
#ifdef PLAT_DM365	//by jiahung
	#if defined( MODEL_LC7415 ) || defined( MODEL_LC7415B ) || defined( MODEL_LC7515 )
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, SUPPORT_DCPOWER);
	#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);
	#endif
#else
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, SUPPORT_DCPOWER);
#endif

    //req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=0\n", argm->name);//only dm365
}

void set_powerout(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_DCPOWER 
	
	do{
        if(numeric_check(argm->value) < 0)
            break;

        __u8 value = atoi(argm->value);

		if (value > 3)
			break;
		
		SYSLOG tlog;	
		
		if(sys_info->ipcam[POWEROUT].config.u8 != value){
			info_write(&value, POWEROUT, 0 ,0);
			//if(sys_info->config.lan_config.powerout != value){
			//sysinfo_write(&value, offsetof(SysInfo, config.lan_config.powerout), sizeof(value), 0);
		
		#ifdef SUPPORT_IR 	
			tlog.opcode = SYSLOG_SYSSET_IR_MODE;
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			snprintf(logstr, MSG_USE_LEN, "%s SET IR LIGHT %s MODE", vistor, dcpower_name(value));
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif	
		#else
			tlog.opcode = SYSLOG_SYSSET_DCPOWER_MODE;
#ifdef MSGLOGNEW
			char loginip[20];
			char logstr[MSG_MAX_LEN];
			char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
			strcpy(loginip,ipstr(req->curr_user.ip));
			sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
			snprintf(logstr, MSG_USE_LEN, "%s SET DCPOWER %s MODE", vistor, dcpower_name(value));
			sprintf(logstr, "%s\r\n", logstr);
			sysdbglog(logstr);
#endif	
		#endif
			tlog.ip = req->curr_user.ip;
			strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
			tlog.arg_0 = value;
			SetSysLog(&tlog);
		}
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif

}

void powerschedule(request *req, COMMAND_ARGUMENT *argm)
{
#if SUPPORT_DCPOWER 
	int start_tmp = 0, end_tmp = 0;
	sscanf(argm->value, "%06d%06d", &start_tmp, &end_tmp);
	do{
		//if(start_tmp > end_tmp)
		//	break;

		info_write(&start_tmp, POWERSCHEDULE_START, 0, 0);
		info_write(&end_tmp, POWERSCHEDULE_END, 0, 0);
		//sysinfo_write(&start_tmp, offsetof(SysInfo, config.lan_config.powerschedule_start), sizeof(start_tmp), 0);
		//sysinfo_write(&end_tmp, offsetof(SysInfo, config.lan_config.powerschedule_end), sizeof(end_tmp), 0);
#ifdef MSGLOGNEW
		char loginip[20];
		char logstr[MSG_MAX_LEN];
		char vistor [sizeof( loginip ) + MAX_USERNAME_LEN + 6];
		strcpy(loginip,ipstr(req->curr_user.ip));
		sprintf(vistor,"%s FROM %s",req->curr_user.id,loginip);
		snprintf(logstr, MSG_USE_LEN, "%s SET IR LIGHT RANGE %04d to %04d", vistor, start_tmp/100, end_tmp/100);
		sprintf(logstr, "%s\r\n", logstr);
		sysdbglog(logstr);
#endif		
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;

	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}

/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void osd_show(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[OSD_SHOW].config.value);
		return;
	}

    do{
        if(numeric_check(argm->value) < 0)
            break;
        
        int value = atoi(argm->value);

        if(value < 0 || value > 1)
            break;
        
        if(ControlSystemData(SFIELD_SET_OSD_SHOW, &value, sizeof(value), &req->curr_user) < 0)
            break;
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
        
    }while(0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
/*
void get_alarm_status(request *req, COMMAND_ARGUMENT *argm)
{
	int event_status = 0;

	do{
		if (sys_info->osd_alarm_ext)
			event_status |= 0x05;
		if (sys_info->osd_alarm_motion)
			event_status |= 0x0A;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%04X\n", argm->name, event_status);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void user_login(request *req, COMMAND_ARGUMENT *argm)
{
	char *user_name = argm->value, *user_passwd;

	do {
		if (req->connect) {
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
			return;
		}
//		dbg("user_name = %s\n", user_name);
		if ( (user_passwd = strchr(user_name, ':')) == NULL )
			break;
		*user_passwd++ = 0;

//		dbg("*user_passwd = 0x%02X\n", *user_passwd);
		if ((strlen(user_name) > MAX_USER_SIZE) || (strlen(user_passwd) > MAX_PASS_SIZE))
			break;

		if (check_user(req, user_name, user_passwd) < 0)
			break;
			
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
		
	}while(0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void user_logout(request *req, COMMAND_ARGUMENT *argm)
{
#ifndef MSGLOGNEW
	SYSLOG tlog;
#endif
	free_connection(req->connect);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);

#ifdef MSGLOGNEW
	char loginip[20];
	char logstr[MSG_MAX_LEN];
	strcpy(loginip,ipstr(req->curr_user.ip));
	snprintf(logstr, MSG_USE_LEN,"%s LOGOUT FROM %s",req->curr_user.id,loginip);
	sprintf(logstr, "%s\r\n", logstr);
	sysdbglog(logstr);
#else
	tlog.opcode = SYSLOG_SET_LOGOUT;
	tlog.ip = req->curr_user.ip;
	strncpy(tlog.name, req->curr_user.id, MAX_USERNAME_LEN);
	SetSysLog(&tlog);
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void clearalllog(request *req, COMMAND_ARGUMENT *argm)
{   
    do{
        if(numeric_check(argm->value) < 0)
            break;
        
        int value = atoi(argm->value);

        if(value != 1)
            break;
        
        if(ControlSystemData(SFIELD_DEL_LOGALL, &value, sizeof(value), &req->curr_user) < 0)
            break;
        
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
        return;
        
    }while(0);
    req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/ 
void call_verify_func(request *req, COMMAND_ARGUMENT *argm)
{
	unsigned verifycode = (unsigned) atoi( argm->value );
	unsigned verifystatus;

	if ( ( *( argm->value ) ) && ( System_Verify( verifycode , &verifystatus ) == TRUE ) )
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d,%d\n", argm->name, verifycode, verifystatus );
	else
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void osd_show_framerate(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0])
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->show_framerate);
	else {
		sys_info->show_framerate = atoi(argm->value);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	}
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void wizardinit(request *req, COMMAND_ARGUMENT *argm)
{
	if (!argm->value[0])
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[WIZARD_INIT].config.value);
	else {
		if(atoi(argm->value) < sys_info->ipcam[WIZARD_INIT].min || atoi(argm->value) > sys_info->ipcam[WIZARD_INIT].max){

			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
			return ;
		}
		sys_info->ipcam[WIZARD_INIT].config.value = atoi(argm->value);
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	}
}

/*
void test_config(request *req, COMMAND_ARGUMENT *argm)
{
	int value = atoi(argm->value);
	if(sys_info->ipcam[value].type == TYPE_STRING){
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", conf_string_read(value));
	}else{
		req->buffer_end += sprintf(req_bufptr(req), OPTION_NG);
	}
}
*/
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
#define	FM4005_SUPPORT		1
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_rtc_cal_value( request *req, COMMAND_ARGUMENT *argm )
{
#if FM4005_SUPPORT == 1
	struct rtc_pll_info rtcpllinfo;
	int fd;

	if ( *( argm->value ) == 0 )
	{
		if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
		{
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
			return;
		}
		rtcpllinfo.pll_clock = 0;
		if ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		}
		else
		{
			close ( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s = %d\n", argm->name, rtcpllinfo.pll_value );
		}
	}
	else
	{
		if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
		{
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
			return;
		}
		rtcpllinfo.pll_clock = 0;
		rtcpllinfo.pll_value = atoi( argm->value );
		if ( ioctl( fd, RTC_PLL_SET, &rtcpllinfo ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		}
		else
		{
			close ( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		}
	}
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_rtc_cal_status( request *req, COMMAND_ARGUMENT *argm )
{
#if FM4005_SUPPORT == 1
	struct rtc_pll_info rtcpllinfo;
	int fd;

	if ( *( argm->value ) == 0 )
	{
		if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
		{
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
			return;
		}
		rtcpllinfo.pll_clock = 1;
		if ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		}
		else
		{
			close ( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s = %d\n", argm->name, rtcpllinfo.pll_ctrl );
		}
	}
	else
	{
		if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
		{
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
			return;
		}
		rtcpllinfo.pll_clock = 1;
		rtcpllinfo.pll_ctrl = atoi( argm->value );
		if ( ioctl( fd, RTC_PLL_SET, &rtcpllinfo ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		}
		else
		{
			close ( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		}
	}
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_rtc_cal_faster( request *req, COMMAND_ARGUMENT *argm )
{
#if FM4005_SUPPORT == 1
	struct rtc_pll_info rtcpllinfo;
	int fd;

	if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
	{
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		return;
	}
	rtcpllinfo.pll_clock = 1;
	if ( ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) ) || ( rtcpllinfo.pll_ctrl == 0 ) )
	{
		close( fd );
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
	}
	else
	{
		rtcpllinfo.pll_clock = 0;
		if ( ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) ) || ( rtcpllinfo.pll_value > 30 ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
			return;
		}
		rtcpllinfo.pll_value += 1;
		if ( ioctl( fd, RTC_PLL_SET, &rtcpllinfo ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		}
		else
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s = %d\n", argm->name, rtcpllinfo.pll_value );
		}
	}
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_rtc_cal_slower( request *req, COMMAND_ARGUMENT *argm )
{
#if FM4005_SUPPORT == 1
	struct rtc_pll_info rtcpllinfo;
	int fd;

	if ( ( fd = open( "/dev/misc/rtc", O_RDONLY ) ) < 0 )
	{
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		return;
	}
	rtcpllinfo.pll_clock = 1;
	if ( ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) ) || ( rtcpllinfo.pll_ctrl == 0 ) )
	{
		close( fd );
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
	}
	else
	{
		rtcpllinfo.pll_clock = 0;
		if ( ( ioctl( fd, RTC_PLL_GET, &rtcpllinfo ) ) || ( rtcpllinfo.pll_value < -30 ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
			return;
		}
		rtcpllinfo.pll_value -= 1;
		if ( ioctl( fd, RTC_PLL_SET, &rtcpllinfo ) )
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
		}
		else
		{
			close( fd );
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s = %d\n", argm->name, rtcpllinfo.pll_value );
		}
	}
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_event_server( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    int i = 0, max = 0, src = 0, para_cnt = 0;
    //EVENT_2G_BASIC  basic = E2G_BASIC_SERVER_DEFAULT;
    EVENT_2G_SERVER server = {E2G_BASIC_SERVER_DEFAULT, {ESERVER_FTP_DEFAULT}};
    char *ptr[E2G_SERVER_PARA_NUM];
    char *str = argm->value;

    memset(ptr, 0, sizeof(char*) * E2G_SERVER_PARA_NUM);
    memset(&server, 0, sizeof(EVENT_2G_SERVER));
    //memcpy(&server.info, &basic, sizeof(EVENT_2G_BASIC));

    dbg("argm->value = %s ", argm->value);

    for(i = 0; i < E2G_SERVER_PARA_NUM + 1; i++) {
        str = strchr(str, ':');

        if(str == NULL)
            break;
        
        ptr[i] = str;
        str++;
        para_cnt++;
    }

    if(para_cnt != E2G_SERVER_PARA_NUM) {
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s=2\n", argm->name );
        dbg("event server parameter count error ");
        return;
    }

    
    do {
        server.info.id = atoi(argm->value);
        if(server.info.id > E2G_SERVER_MAX_NUM - 1) {
            dbg("server id error, id = %d ", server.info.id);
            break;
        }

        max = sizeof(server.info.name) - 1;
        src = ptr[ES_USER] - (ptr[ES_NAME]+1);
        memset(server.info.name, 0, sizeof(server.info.name));
        strncpy(server.info.name, ptr[ES_NAME]+1, (max > src) ? src : max);
        if(strlen(server.info.name) < 1) {
            dbg("server name is empty!! ");
            break;
        }
		if(check_specical_symbols(server.info.name, FILENAME_SYMBOL) < 0){
			DBG("ES_NAME[%s] Error", server.info.name);
			break;
		}

        server.info.type = atoi(ptr[ES_TYPE]+1);
        if(server.info.type == 0) {
            /*  SMTP    */
            //dbg("SERVER TYPE = SMTP");
            server.info.type = ESERVER_TYPE_SMTP;

            max = sizeof(server.smtp.username) - 1;
            src = ptr[ES_PASS] - (ptr[ES_USER]+1);
            strncpy(server.smtp.username, ptr[ES_USER]+1, (max > src) ? src : max);
            //if(strlen(server.smtp.username) < 1) {
            //  dbg("smtp user name is empty!!\n");
            //  break;
            //}

            max = sizeof(server.smtp.password) - 1;
            src = ptr[ES_FQDN] - (ptr[ES_PASS]+1);
            strncpy(server.smtp.password, ptr[ES_PASS]+1, (max > src) ? src : max);

            max = sizeof(server.smtp.fqdn);
            src = ptr[ES_PORT] - (ptr[ES_FQDN]+1); 
            strncpy(server.smtp.fqdn, ptr[ES_FQDN]+1, (max > src) ? src : max);
            //if(strlen(server.smtp.fqdn) < 1) {
            //  dbg("smtp server fqdn is empty!!\n");
            //  break;
            //}

            server.smtp.port = atoi(ptr[ES_PORT]+1);
            if(server.smtp.port == 0) {
                dbg("smtp server port error, port = %d ", server.smtp.port);
                break;
            }

            max = sizeof(server.smtp.from_mail) - 1;
            src = ptr[ES_TO] - (ptr[ES_FROM]+1);
            strncpy(server.smtp.from_mail, ptr[ES_FROM]+1, (max > src) ? src : max);
            //if(strlen(server.smtp.from_mail) < 1) {
            //  dbg("smtp server from_mail is empty!!\n");
            //  break;
            //}

            max = sizeof(server.smtp.to_mail) - 1;
            src = ptr[ES_PATH] - (ptr[ES_TO]+1);
            strncpy(server.smtp.to_mail, ptr[ES_TO]+1, (max > src) ? src : max);
            //if(strlen(server.smtp.to_mail) < 1) {
            //  dbg("smtp server to_mail is empty!!\n");
            //  break;
            //}
            
            // check the mail address include '@' and a '.' 
            /*
            char * ptr = strchr(server.smtp.to_mail, '@');
            if(ptr == NULL) {
                dbg("mail address not include '@' symbol");
                break;
            }
            
            if( strchr(ptr, '.') == NULL) {
                dbg("mail address not include '.' symbol after '@'");
                break;
            }
            */
            server.smtp.encryption = atoi(ptr[ES_ENCRYPT] + 1);
        }
        else if(server.info.type == 1) {
            /*  FTP     */
            //dbg("SERVER TYPE = FTP");
            server.info.type = ESERVER_TYPE_FTP;
            
            max = sizeof(server.ftp.username) - 1;
            src = ptr[ES_PASS] - (ptr[ES_USER]+1);
            strncpy(server.ftp.username, ptr[ES_USER]+1, (max > src) ? src : max);
            //if(strlen(server.ftp.username) < 1) {
            //  dbg("ftp user name is empty!!\n");
            //  break;
            //}

            max = sizeof(server.ftp.password) - 1;
            src = ptr[ES_FQDN] - (ptr[ES_PASS]+1);
            strncpy(server.ftp.password, ptr[ES_PASS]+1, (max > src) ? src : max);
            //if(strlen(server.ftp.password) < 1)
            //  break;

            max = sizeof(server.ftp.fqdn) - 1;
            src = ptr[ES_PORT] - (ptr[ES_FQDN]+1);
            strncpy(server.ftp.fqdn, ptr[ES_FQDN]+1, (max > src) ? src : max);
            //if(strlen(server.ftp.fqdn) < 1) {
            //  dbg("ftp server fqdn is empty!!\n");
            //  break;
            //}

            server.ftp.port = atoi(ptr[ES_PORT]+1);
            if(server.ftp.port == 0) {
                dbg("ftp server port error, port = %d ", server.ftp.port);
                break;
            }

            max = sizeof(server.ftp.path) - 1;
            src = ptr[ES_PASSIVE] - (ptr[ES_PATH]+1);
            strncpy(server.ftp.path, ptr[ES_PATH]+1, (max > src) ? src : max);
			if(check_specical_symbols(server.ftp.path, FILENAME_SYMBOL) < 0){
				DBG("ES_PATH [%s] Error", server.ftp.path);
				break;
			}
			
            //if(strlen(server.ftp.path) < 1) {
            //  dbg("ftp server path is empty!!\n");
            //  break;
            //}
            
            server.ftp.passive_mode = atoi(ptr[ES_PASSIVE]+1);
            if(server.ftp.passive_mode > 1) {
                dbg("ftp server passive_mode error, passive_mode = %d!!\n", server.ftp.passive_mode);
                break;
            }
        }
        else if(server.info.type == 3) {
            /*  SAMBA   */

            for(i = 0; i < E2G_SERVER_MAX_NUM + 1; i++) {

                if(sys_info->ipcam[E2G_SERVER0_INFO_TYPE+SERVER_STRUCT_SIZE*i].config.u16 == ESERVER_TYPE_SAMBA){
                //if(sys_info->config.e2g_server[i].info.type == ESERVER_TYPE_SAMBA) {
                    if(server.info.id == i) {
                        break;
                    }
                    else {
                        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=3\n", argm->name );
                        return ;
                    }
                }
            }

            //dbg("SERVER TYPE = SAMBA");
            //const char * mount[] = { SAMBA_E2G_MNT_PATH1, SAMBA_E2G_MNT_PATH2,
            //  SAMBA_E2G_MNT_PATH3, SAMBA_E2G_MNT_PATH4, SAMBA_E2G_MNT_PATH5 };
            server.info.type = ESERVER_TYPE_SAMBA;

            max = sizeof(server.samba.username) - 1;
            src = ptr[ES_PASS] - (ptr[ES_USER]+1);
            strncpy(server.samba.username, ptr[ES_USER]+1, (max > src) ? src : max);
            //if(strlen(server.samba.username) < 1) {
            //  dbg("samba user name is empty!!\n");
            //  break;
            //}

            max = sizeof(server.samba.password) - 1;
            src = ptr[ES_FQDN] - (ptr[ES_PASS]+1);
            strncpy(server.samba.password, ptr[ES_PASS]+1, (max > src) ? src : max);

            max = sizeof(server.samba.fqdn) - 1;
            src = ptr[ES_PORT] - (ptr[ES_FQDN]+1);
            strncpy(server.samba.fqdn, ptr[ES_FQDN]+1, (max > src) ? src : max);
            //if(strlen(server.samba.fqdn) < 1) {
            //  dbg("samba server fqdn is empty!!\n");
            //  break;
            //}

            max = sizeof(server.samba.workgroup) - 1;
            src = ptr[ES_WINS] - (ptr[ES_WORKGROUP]+1);
            strncpy(server.samba.workgroup, ptr[ES_WORKGROUP]+1, (max > src) ? src : max);          
            //if(inet_aton(ptr[ES_WINS]+1, &server.samba.wins) < 0 ) {
            //  dbg("samba server wins ip %s is invalid !!\n", ptr[ES_WINS]+1);
            //  break;
            //}

            /*
            pid_t pid;
            char option[256] = "";
            char path[256] = "";
            char * ptr = NULL;
            char * pmount = NULL;

            ptr = option;
            if(strlen(server.samba.workgroup))
                ptr += sprintf(ptr, "workgroup=%s,", server.samba.workgroup);
            
            if(strlen(server.samba.username))
                ptr += sprintf(ptr, "username=%s,", server.samba.username);
            else
                ptr += sprintf(ptr, "username=%s,", "guest");

            ptr += sprintf(ptr, "password=%s", server.samba.password);
            
            // convert character '\' to '/' for linux
            strncpy(path, server.samba.fqdn, sizeof(path));
            ptr = path;
            while( (ptr = strchr(ptr, '\\')) ){
                *ptr = '/';
            }

            switch(server.info.id) {
                case 0:     pmount = SAMBA_E2G_MNT_PATH1;   break;
                case 1:     pmount = SAMBA_E2G_MNT_PATH2;   break;
                case 2:     pmount = SAMBA_E2G_MNT_PATH3;   break;
                case 3:     pmount = SAMBA_E2G_MNT_PATH4;   break;
                case 4:     pmount = SAMBA_E2G_MNT_PATH5;   break;
            }

            // if the mount source changed. need to remount
            if(strncmp(server.samba.fqdn, sys_info->config.e2g_server[server.info.id].samba.fqdn, sizeof(server.samba.fqdn) != 0)) {
                dbg("fork command = %s %s %s -o %s \n", SAMBA_EXEC_PATH, path, pmount, option);

                // umount anyway. Alex.
                umount2(SAMBA_E2G_MNT_PATH1, MNT_DETACH);
                if( (pid = fork()) == 0 ) {
                    execl(SAMBA_EXEC_PATH, SAMBA_EXEC, path, pmount, "-o", option, NULL);
                    DBGERR("exec " SAMBA_EXEC_PATH " failed\n");
                    _exit(0);
                    break;
                }
                else if(pid < 0) {
                    DBGERR("fork " SAMBA_EXEC_PATH " failed\n");
                    break;
                }
            }
            */
            // the smbmount will execute by server's pollingthread
            // Alex. 2009.09.16

            //dbg("server.info.id = %d", server.info.id);

            // re-mount 
            //if(sys_info->smbmount_pid[server.info.id])
            //  kill(sys_info->smbmount_pid[server.info.id], SIGTERM);

            // umount anyway. 
            //umount2(mount[server.info.id], MNT_DETACH);
            //sys_info->smbmount_pid[server.info.id] = 0;
        }
        else if(server.info.type == 4) {
            /*  SD Card */

            for(i = 0; i < E2G_SERVER_MAX_NUM + 1; i++) {
                if(sys_info->ipcam[E2G_SERVER0_INFO_TYPE+SERVER_STRUCT_SIZE*i].config.u16 == ESERVER_TYPE_SD){
                //if(sys_info->config.e2g_server[i].info.type == ESERVER_TYPE_SD) {
                    if(server.info.id == i) {
                        break;
                    }
                    else {
                        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=3\n", argm->name );
                        return ;
                    }
                }
            }

            //dbg("SERVER TYPE = SD CARD");
            server.info.type = ESERVER_TYPE_SD;

            //max = sizeof(server.sd.path) - 1;
            //src = ptr[ES_PASSIVE] - (ptr[ES_PATH]+1);
            //strncpy(server.sd.path, ptr[ES_PATH]+1, (max > src) ? src : max);
        }
        else {
            DBG("Unknown event server type %04X\n", server.info.type);
            req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
            return;
        }

        //dbg("EVENT SERVER SETTING DONE ");
        server.info.valid = 1;
        //dbg("--------------server.samba.fqdn = %s\n", server.samba.fqdn);
        //dbg("server.ftp.fqdn = %s\n", server.ftp.fqdn);
        //if(1) {
        //if(memcmp(&server, &sys_info->config.e2g_server[server.info.id], sizeof(EVENT_2G_SERVER)) != 0) {
        dbg("EVENT SERVER SETTING DONE, write to config ");
        //int event_2g_server_size = E2G_SERVER0_SD_PATH - E2G_SERVER0_INFO_VALID+1;
        int sel = server.info.id * SERVER_STRUCT_SIZE;
        
        info_write(&server.info.valid, E2G_SERVER0_INFO_VALID+sel, 0, 0);
        info_write(&server.info.enable, E2G_SERVER0_INFO_ENABLE+sel, 0, 0);
        info_write(&server.info.id, E2G_SERVER0_INFO_ID+sel, 0, 0);
        info_write(&server.info.type, E2G_SERVER0_INFO_TYPE+sel, 0, 0);
        info_write(server.info.name, E2G_SERVER0_INFO_NAME+sel, strlen(server.info.name), 0);
                    
        switch(server.info.type){
            case ESERVER_TYPE_FTP:
                info_write(&server.ftp.passive_mode, E2G_SERVER0_FTP_PASSIVE_MODE+sel, 0, 0);
                info_write(&server.ftp.upload_rate, E2G_SERVER0_FTP_UPLOAD_RATE+sel, 0, 0);
                info_write(&server.ftp.port, E2G_SERVER0_FTP_PORT+sel, 0, 0);
                info_write(server.ftp.fqdn, E2G_SERVER0_FTP_FQDN+sel, strlen(server.ftp.fqdn), 0);
                info_write(server.ftp.username, E2G_SERVER0_FTP_USERNAME+sel, strlen(server.ftp.username), 0);
                info_write(server.ftp.password, E2G_SERVER0_FTP_PASSWORD+sel, strlen(server.ftp.password), 0);
                info_write(server.ftp.path, E2G_SERVER0_FTP_PATH+sel, strlen(server.ftp.path), 0);
                break;
                
            case ESERVER_TYPE_SMTP:
                info_write(&server.smtp.attach_num, E2G_SERVER0_SMTP_ATTACH_NUM+sel, 0, 0);
                info_write(&server.smtp.encryption, E2G_SERVER0_SMTP_ENCRYPTION+sel, 0, 0);
                info_write(&server.smtp.port, E2G_SERVER0_SMTP_PORT+sel, 0, 0);
                info_write(server.smtp.fqdn, E2G_SERVER0_SMTP_FQDN+sel, strlen(server.smtp.fqdn), 0);
                info_write(server.smtp.username, E2G_SERVER0_SMTP_USERNAME+sel, strlen(server.smtp.username), 0);
                info_write(server.smtp.password, E2G_SERVER0_SMTP_PASSWORD+sel, strlen(server.smtp.password), 0);
                info_write(server.smtp.from_mail, E2G_SERVER0_SMTP_FROM_MAIL+sel, strlen(server.smtp.from_mail), 0);
                info_write(server.smtp.to_mail, E2G_SERVER0_SMTP_TO_MAIL+sel, strlen(server.smtp.to_mail), 0);
                break;
                
            case ESERVER_TYPE_SAMBA:    
                info_write(&server.samba.authority, E2G_SERVER0_SAMBA_AUTHORITY+sel, 0, 0);
                info_write(&server.samba.wins, E2G_SERVER0_SAMBA_WINS+sel, 0, 0);
                info_write(server.samba.fqdn, E2G_SERVER0_SAMBA_FQDN+sel, strlen(server.samba.fqdn), 0);
                info_write(server.samba.username, E2G_SERVER0_SAMBA_USERNAME+sel, strlen(server.samba.username), 0);
                info_write(server.samba.password, E2G_SERVER0_SAMBA_PASSWORD+sel, strlen(server.samba.password), 0);
                info_write(server.samba.path, E2G_SERVER0_SAMBA_PATH+sel, strlen(server.samba.path), 0);
                info_write(server.samba.workgroup, E2G_SERVER0_SAMBA_WORKGROUP+sel, strlen(server.samba.workgroup), 0);
                
                // Clear mount status , let server process remout it.
                sys_info->smb_mount_stat[server.info.id] = 0;
                break;
                
            case ESERVER_TYPE_SD:
                info_write(server.sd.path, E2G_SERVER0_SD_PATH+sel, strlen(server.sd.path), 0);
                break;
                
            default:
                dbg("UNKNOWN SERVER TYPE");
                break;
        }   
            //sysinfo_write(&server, offsetof(SysInfo, config.e2g_server[server.info.id]), sizeof(EVENT_2G_SERVER), 0);
        //}

#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
		char log_servertype[20] = "";
		if(server.info.type == ESERVER_TYPE_SMTP)
			strcpy(log_servertype,"Email");
		else if(server.info.type == ESERVER_TYPE_FTP)
			strcpy(log_servertype,"FTP");
		else if(server.info.type == ESERVER_TYPE_SAMBA)
			strcpy(log_servertype,"Network storage");
		else if(server.info.type == ESERVER_TYPE_SD)
			strcpy(log_servertype,"SD Card");
        snprintf(logstr, MSG_USE_LEN, "%s SET EVENT SERVER %d ; Name : %s, Type : %s", req->curr_user.id, server.info.id + 1, server.info.name, log_servertype);
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif

        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name );
        return;

    } while(0);

    req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=3\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void del_event_server( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    __u32 id = atoi(argm->value);

    int sel = id * SERVER_STRUCT_SIZE;
    do {
        if(id >= E2G_SERVER_MAX_NUM)
            break;
        
        /*EVENT_2G_BASIC info = E2G_BASIC_SERVER_DEFAULT;
        EVENT_SERVER_FTP ftp = ESERVER_FTP_DEFAULT;
        EVENT_2G_SERVER server;*/
        char * pmount = NULL;

        /*memset(&server, 0, sizeof(EVENT_2G_SERVER));
        memcpy(&server.info, &info, sizeof(EVENT_2G_BASIC));
        memcpy(&server.ftp, &ftp, sizeof(EVENT_SERVER_FTP));*/
        
        //EVENT_2G_SERVER * pdel_server = &sys_info->config.e2g_server[id];

        //switch(pdel_server->info.id) {
switch(sys_info->ipcam[E2G_SERVER0_INFO_ID+sel].config.u8) {
            case 0:     pmount = SAMBA_E2G_MNT_PATH1;   break;
            case 1:     pmount = SAMBA_E2G_MNT_PATH2;   break;
            case 2:     pmount = SAMBA_E2G_MNT_PATH3;   break;
            case 3:     pmount = SAMBA_E2G_MNT_PATH4;   break;
            case 4:     pmount = SAMBA_E2G_MNT_PATH5;   break;
            
            default:
                DBG("DEL SERVER ID %d ERROR \n", sys_info->ipcam[E2G_SERVER0_INFO_ID+sel].config.u8);
                //DBG("DEL SERVER ID %d ERROR \n", pdel_server->info.id);
                req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
                return;
                
        }

        if(sys_info->ipcam[E2G_SERVER0_INFO_TYPE+sel].config.u16 == ESERVER_TYPE_SAMBA) {
        //if(pdel_server->info.type == ESERVER_TYPE_SAMBA) {
            if(umount2(pmount, MNT_DETACH) == -1)
                DBGERR("%s umount error\n", pmount);
        }
        
        
        //memcpy(pdel_server, &server, sizeof(EVENT_2G_SERVER));
        info_write(&sys_info->ipcam[E2G_SERVER0_INFO_VALID+sel].def_value.value, E2G_SERVER0_INFO_VALID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_INFO_ENABLE+sel].def_value.value, E2G_SERVER0_INFO_ENABLE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_INFO_ID+sel].def_value.value, E2G_SERVER0_INFO_ID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_INFO_TYPE+sel].def_value.value, E2G_SERVER0_INFO_TYPE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_INFO_NAME+sel].def_value.value, E2G_SERVER0_INFO_NAME+sel, 0, 0);
        
        info_write(&sys_info->ipcam[E2G_SERVER0_FTP_PASSIVE_MODE+sel].def_value.value, E2G_SERVER0_FTP_PASSIVE_MODE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_FTP_UPLOAD_RATE+sel].def_value.value, E2G_SERVER0_FTP_UPLOAD_RATE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_FTP_PORT+sel].def_value.value, E2G_SERVER0_FTP_PORT+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_FTP_FQDN+sel].def_value.value, E2G_SERVER0_FTP_FQDN+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_FTP_USERNAME+sel].def_value.value, E2G_SERVER0_FTP_USERNAME+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_FTP_PASSWORD+sel].def_value.value, E2G_SERVER0_FTP_PASSWORD+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_FTP_PATH+sel].def_value.value, E2G_SERVER0_FTP_PATH+sel, 0, 0);
        
        info_write(&sys_info->ipcam[E2G_SERVER0_SMTP_ATTACH_NUM+sel].def_value.value, E2G_SERVER0_SMTP_ATTACH_NUM+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SMTP_PORT+sel].def_value.value, E2G_SERVER0_SMTP_PORT+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SMTP_FQDN+sel].def_value.value, E2G_SERVER0_SMTP_FQDN+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SMTP_USERNAME+sel].def_value.value, E2G_SERVER0_SMTP_USERNAME+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SMTP_PASSWORD+sel].def_value.value, E2G_SERVER0_SMTP_PASSWORD+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SMTP_FROM_MAIL+sel].def_value.value, E2G_SERVER0_SMTP_FROM_MAIL+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SMTP_TO_MAIL+sel].def_value.value, E2G_SERVER0_SMTP_TO_MAIL+sel, 0, 0);
        
        info_write(&sys_info->ipcam[E2G_SERVER0_SAMBA_AUTHORITY+sel].def_value.value, E2G_SERVER0_SAMBA_AUTHORITY+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SAMBA_WINS+sel].def_value.value, E2G_SERVER0_SAMBA_WINS+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SAMBA_FQDN+sel].def_value.value, E2G_SERVER0_SAMBA_FQDN+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SAMBA_USERNAME+sel].def_value.value, E2G_SERVER0_SAMBA_USERNAME+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SAMBA_PASSWORD+sel].def_value.value, E2G_SERVER0_SAMBA_PASSWORD+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SAMBA_PATH+sel].def_value.value, E2G_SERVER0_SAMBA_PATH+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_SERVER0_SAMBA_WORKGROUP+sel].def_value.value, E2G_SERVER0_SAMBA_WORKGROUP+sel, 0, 0);

        info_write(&sys_info->ipcam[E2G_SERVER0_SD_PATH+sel].def_value.value, E2G_SERVER0_SD_PATH+sel, 0, 0);
        
        //memcpy(pdel_server, &server, sizeof(EVENT_2G_SERVER));

        //sysinfo_write(pdel_server, offsetof(SysInfo, config.e2g_server[id]), sizeof(EVENT_2G_SERVER), 0);

#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
        snprintf(logstr, MSG_USE_LEN, "%s DELETE EVENT SERVER %d", req->curr_user.id, id + 1 );
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif

        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
        break;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_event_media( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    int i = 0, para_cnt = 0;
    EVENT_2G_MEDIA media = {E2G_BASIC_EVENT_DEFAULT, {EMEDIA_PIC_DEFAULT}};
    char *ptr[E2G_MEDIA_PARA_NUM];
    char *str = argm->value;

    memset(ptr, 0, sizeof(char*) * E2G_MEDIA_PARA_NUM);
    //bzero(&media, sizeof(EVENT_2G_MEDIA));

    dbg("argm->value = %s ", argm->value);

    for(i = 0; i < E2G_MEDIA_PARA_NUM + 1; i++) {
        str = strchr(str, ':');
        ptr[i] = str;

        if(str == NULL) 
            break;
        
        str++;
        para_cnt++;
    }

    if(para_cnt != E2G_MEDIA_PARA_NUM) {
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s=2\n", argm->name );
        dbg("event media parameter count error ");
        return;
    }


    do {

        media.info.id = atoi(argm->value);
        if(media.info.id > E2G_MEDIA_MAX_NUM - 1) {
            dbg("media id error, id = %d ", media.info.id);
            break;
        }

        int max = sizeof(media.info.name) - 1;
        int src = ptr[EM_SOURCE] - (ptr[EM_NAME]+1);
        memset(media.info.name, 0, sizeof(media.info.name));
        strncpy(media.info.name, ptr[EM_NAME]+1, (max > src) ? src : max);
        if(strlen(media.info.name) < 1) {
            dbg("media name is empty!!");
            break;
        }
		if(check_specical_symbols(media.info.name, FILENAME_SYMBOL) < 0){
			DBG("EM_NAME[%s] Error", media.info.name);
			break;
		}

        media.info.type = atoi(ptr[EM_TYPE]+1);
        if(media.info.type == 0) {
            /*  PICTURE */
            media.info.type = EMEDIA_TYPE_PIC;

            media.pic.source = atoi(ptr[EM_SOURCE]+1);
            if(media.pic.source > MAX_E2G_SOURCE) {
				#ifdef SUPPORT_PROFILE_NUMBER
				if(media.pic.source > (sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value-1))
					break;
				#endif
                dbg("media source value error, value = %d ", media.pic.source);
                break;
            }
            
            media.pic.pre_event = atoi(ptr[EM_PRE]+1);
            if(media.pic.pre_event > MAX_E2G_PIC_PRE) {
                dbg("media pre-event value error, value = %d ", media.pic.pre_event);
                break;
            }

            media.pic.post_event = atoi(ptr[EM_POST]+1);
            if(media.pic.post_event > MAX_E2G_PIC_POST) {
                dbg("media post-event value error, value = %d ", media.pic.post_event);
                break;
            }

            media.pic.filename_suffix = atoi(ptr[EM_SUFFIX]+1);
            if(media.pic.filename_suffix > MAX_E2G_SUFFIX) {
                dbg("media filename_suffix value error, value = %d ", media.pic.filename_suffix);
                break;
            }

			if(check_specical_symbols(ptr[EM_FNAME]+1, FILENAME_SYMBOL) < 0){
				DBG("EM_FNAME[%s] Error", ptr[EM_FNAME]+1);
				break;
			}else
            	strncpy(media.pic.filename, ptr[EM_FNAME]+1, E2G_FILE_NAME_LEN);
			
            //if(strlen(media.pic.filename) < 1) {
            //  dbg("media filename is empty!!\n");
            //  break;
            //}
            
        }
        else if(media.info.type == 1) {
            /*  VIDEO   */
            media.info.type = EMEDIA_TYPE_VIDEO;
            
            media.video.source = atoi(ptr[EM_SOURCE]+1);
            if(media.pic.source > MAX_E2G_SOURCE) {
				#ifdef SUPPORT_PROFILE_NUMBER
				if(media.pic.source > (sys_info->ipcam[CONFIG_MAX_WEB_PROFILE_NUM].config.value-1))
					break;
				#endif
                dbg("media source value error, value = %d ", media.pic.source);
                break;
            }
            
            media.video.pre_event = atoi(ptr[EM_PRE]+1);
            if(media.video.pre_event > MAX_E2G_VID_PRE) {
                dbg("media pre-event value error, value = %d ", media.video.pre_event);
                break;
            }

            media.video.duration = atoi(ptr[EM_POST]+1);
            if(media.video.duration > MAX_E2G_VID_DUR) {
                dbg("media duration value error, value = %d \n", media.video.duration);
                break;
            }

            media.video.maxsize = atoi(ptr[EM_SIZE]+1);
            if(media.video.maxsize > MAX_E2G_VID_FILESIZE) {
                dbg("media maxsize value error, value = %d \n", media.video.maxsize);
                break;
            }
			
            if(check_specical_symbols(ptr[EM_FNAME]+1, FILENAME_SYMBOL) < 0){
				DBG("EM_FNAME[%s] Error", ptr[EM_FNAME]+1);
				break;
			}else
           		strncpy(media.video.filename, ptr[EM_FNAME]+1, E2G_FILE_NAME_LEN);
			
            //if(strlen(media.video.filename) < 1) {
            //  dbg("media filename is empty!!\n");
            //  break;
            //}

        }
        else if(media.info.type == 2) {
            /*  LOG */
            media.info.type = EMEDIA_TYPE_LOG;
        }
        else {
            DBG("Unknown event server type %04X", media.info.type);
            req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
            return;
        }

        dbg("EVENT MEDIA SETTING DONE");
        media.info.valid = 1;
        //if(1) {
        //if(memcmp(&media, &sys_info->config.e2g_media[media.info.id], sizeof(EVENT_2G_MEDIA)) != 0) {
        dbg("write to config ");
        //int event_2g_media_size = E2G_MEDIA0_LOG_FILENAME - E2G_MEDIA0_INFO_VALID+1;
        int sel = media.info.id * MEDIA_STRUCT_SIZE;
        
        info_write(&media.info.valid, E2G_MEDIA0_INFO_VALID+sel, 0, 0);
        info_write(&media.info.enable, E2G_MEDIA0_INFO_ENABLE+sel, 0, 0);
        info_write(&media.info.id, E2G_MEDIA0_INFO_ID+sel, 0, 0);
        info_write(&media.info.type, E2G_MEDIA0_INFO_TYPE+sel, 0, 0);
        info_write(media.info.name, E2G_MEDIA0_INFO_NAME+sel, strlen(media.info.name), 0);
        
        info_write(&media.pic.source, E2G_MEDIA0_PIC_SOURCE+sel, 0, 0);
        info_write(&media.pic.pre_event, E2G_MEDIA0_PIC_PRE_EVENT+sel, 0, 0);
        info_write(&media.pic.post_event, E2G_MEDIA0_PIC_POST_EVENT+sel, 0, 0);
        info_write(&media.pic.filename_suffix, E2G_MEDIA0_PIC_FILENAME_SUFFIX+sel, 0, 0);
        info_write(media.pic.filename, E2G_MEDIA0_PIC_FILENAME+sel, strlen(media.pic.filename), 0);

        info_write(&media.video.source, E2G_MEDIA0_VIDEO_SOURCE+sel, 0, 0);
        info_write(&media.video.maxsize, E2G_MEDIA0_VIDEO_MAXSIZE+sel, 0, 0);
        info_write(&media.video.pre_event, E2G_MEDIA0_VIDEO_PRE_EVENT+sel, 0, 0);
        info_write(&media.video.duration, E2G_MEDIA0_VIDEO_DURATION+sel, 0, 0);
        //info_write(&media.video.post_event, E2G_MEDIA0_VIDEO_DURATION+sel, 0, 0);
        info_write(&media.video.filename_suffix, E2G_MEDIA0_VIDEO_FILENAME_SUFFIX+sel, 0, 0);
        info_write(media.video.filename, E2G_MEDIA0_VIDEO_FILENAME+sel, strlen(media.video.filename), 0);

        info_write(media.log.filename, E2G_MEDIA0_LOG_FILENAME+sel, strlen(media.log.filename), 0);

#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
		char log_mediatype[20] = "";
		if(media.info.type == EMEDIA_TYPE_PIC)
			strcpy(log_mediatype,"Snapshot");
		else if(media.info.type == EMEDIA_TYPE_VIDEO)
			strcpy(log_mediatype,"Video Clip");
		else if(media.info.type == EMEDIA_TYPE_LOG)
			strcpy(log_mediatype,"System log");
        snprintf(logstr, MSG_USE_LEN, "%s SET EVENT MEDIA %d ; Name : %s, Type : %s", req->curr_user.id, media.info.id + 1, media.info.name, log_mediatype);
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif
            //sysinfo_write(&media, offsetof(SysInfo, config.e2g_media[media.info.id]), sizeof(EVENT_2G_MEDIA), 0);
        //}
            
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name );
        return;

    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=3\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void del_event_media( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    __u32 id = atoi(argm->value);

    int sel = id * MEDIA_STRUCT_SIZE;
    do {
        if(id >= E2G_MEDIA_MAX_NUM)
            break;

        /*EVENT_2G_BASIC info = E2G_BASIC_MEDIA_DEFAULT;
        EVENT_MEDIA_PIC pic = EMEDIA_PIC_DEFAULT;
        EVENT_2G_MEDIA media;

        memset(&media, 0, sizeof(EVENT_2G_MEDIA));
        memcpy(&media.info, &info, sizeof(EVENT_2G_BASIC));
        memcpy(&media.pic, &pic, sizeof(EVENT_MEDIA_PIC));
        
        memcpy(&sys_info->config.e2g_media[id], &media, sizeof(EVENT_2G_MEDIA));*/

        info_write(&sys_info->ipcam[E2G_MEDIA0_INFO_VALID+sel].def_value.value, E2G_MEDIA0_INFO_VALID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_INFO_ENABLE+sel].def_value.value, E2G_MEDIA0_INFO_ENABLE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_INFO_ID+sel].def_value.value, E2G_MEDIA0_INFO_ID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_INFO_TYPE+sel].def_value.value, E2G_MEDIA0_INFO_TYPE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_INFO_NAME+sel].def_value.value, E2G_MEDIA0_INFO_NAME+sel, 0, 0);

        info_write(&sys_info->ipcam[E2G_MEDIA0_PIC_SOURCE+sel].def_value.value, E2G_MEDIA0_PIC_SOURCE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_PIC_PRE_EVENT+sel].def_value.value, E2G_MEDIA0_PIC_PRE_EVENT+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_PIC_POST_EVENT+sel].def_value.value, E2G_MEDIA0_PIC_POST_EVENT+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_PIC_FILENAME_SUFFIX+sel].def_value.value, E2G_MEDIA0_PIC_FILENAME_SUFFIX+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_PIC_FILENAME+sel].def_value.value, E2G_MEDIA0_PIC_FILENAME+sel, 0, 0);

        info_write(&sys_info->ipcam[E2G_MEDIA0_VIDEO_SOURCE+sel].def_value.value, E2G_MEDIA0_VIDEO_SOURCE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_VIDEO_MAXSIZE+sel].def_value.value, E2G_MEDIA0_VIDEO_MAXSIZE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_VIDEO_PRE_EVENT+sel].def_value.value, E2G_MEDIA0_VIDEO_PRE_EVENT+sel, 0, 0);
        //info_write(&sys_info->ipcam[E2G_MEDIA0_VIDEO_POST_EVENT+sel].def_value.value, E2G_MEDIA0_VIDEO_POST_EVENT+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_VIDEO_DURATION+sel].def_value.value, E2G_MEDIA0_VIDEO_DURATION+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_VIDEO_FILENAME_SUFFIX+sel].def_value.value, E2G_MEDIA0_VIDEO_FILENAME_SUFFIX+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_MEDIA0_VIDEO_FILENAME+sel].def_value.value, E2G_MEDIA0_VIDEO_FILENAME+sel, 0, 0);

        info_write(&sys_info->ipcam[E2G_MEDIA0_LOG_FILENAME+sel].def_value.value, E2G_MEDIA0_LOG_FILENAME+sel, 0, 0);
        
        //sysinfo_write(&media, offsetof(SysInfo, config.e2g_media[id]), sizeof(EVENT_2G_MEDIA), 0);

#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
        snprintf(logstr, MSG_MAX_LEN, "%s DELETE EVENT MEDIA %d", req->curr_user.id, id + 1 );
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/

int strcmp_config_env(int server, int media)
{
	int i, j, tmp=0;
	int evt_id=0;
	int env_server, env_media;
	int action_server_mask = 0x01;
	int action_media_mask = 0x0000F;
	
	for(i=0;i<E2G_EVENT_MAX_NUM;i++){
		evt_id = i * EVENT_STRUCT_SIZE;
		
		if(sys_info->ipcam[E2G_EVENT0_INFO_VALID+evt_id].config.u8 != 1)
			continue;

		env_server = (int)sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_SERVER+evt_id].config.u16;
		env_media = (int)sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA+evt_id].config.u32;
		
		for(j=0;j<E2G_SERVER_MAX_NUM;j++){
				
			if(env_server & (action_server_mask<<j)){
				dbg("select env_server(%d)", j);
				tmp = (int)( env_media & (action_media_mask<<(j*4)) );
				dbg("env_media(%d) = %d", j, tmp);
				if((tmp == media) && (server != j))
					return -1;
				else if((tmp == media) && (server == j))
					dbg("skip");
				else
					dbg("space");
			}
		}
		
			
	}
	
	return 0;
}
int check_act_media_status(__u8 event_id, __u16 action_server, __u32 action_media)
{
	int i=0, j=0;
	int action_server_mask = 0x01;
	int action_media_mask = 0x0000F;
	//dbg("event_id = %d\n", event_id);
	
	for(i=0;i<E2G_EVENT_MAX_NUM;i++){
		
		if(sys_info->ipcam[E2G_EVENT0_INFO_VALID+i*EVENT_STRUCT_SIZE].config.u8 != 1)
			continue;
		
		for(j=0;j<E2G_SERVER_MAX_NUM;j++){

			if(i == event_id){
				sys_info->e2g_action[i].media_flag[j] = 0xF;	//0xF is no used
			}else{
				////dbg("%d ACTION_SERVER = %d", i, sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_SERVER+i*EVENT_STRUCT_SIZE].config.u16);
				if(sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_SERVER+i*EVENT_STRUCT_SIZE].config.u16 & (action_server_mask<<j)){
					////dbg("j=%d", j);
					////if(sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA+i*EVENT_STRUCT_SIZE].config.u32 & (action_media_mask<<j*4))
						sys_info->e2g_action[i].media_flag[j] = ( (sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA+i*EVENT_STRUCT_SIZE].config.u32 & (action_media_mask<<j*4)) >> (j*4) );
						//dbg(">>>>>>>sys_info->e2g_action[%d].media_flag[%d] = %01X", i, j, sys_info->e2g_action[i].media_flag[j]);
				}else
					sys_info->e2g_action[i].media_flag[j] = 0xF;
			}		
			//dbg("buf:sys_info->e2g_action[%d].media_flag[%d] = %01X", i, j, sys_info->e2g_action[i].media_flag[j]);
		}
	}

	for(i=0;i<E2G_SERVER_MAX_NUM;i++){
		if(action_server & (action_server_mask<<i)){
			////dbg("action_media = %05X", action_media);
			////dbg("action_media_mask<<(i*4) = %05X", action_media_mask<<(i*4));
			////dbg("value = %01X", ((action_media & (action_media_mask<<(i*4)))>>(i*4)) );
			sys_info->e2g_action[event_id].media_flag[i] = (int)((action_media & (action_media_mask<<(i*4)))>>(i*4));
		}else
			sys_info->e2g_action[event_id].media_flag[i] = 0xF;
			//dbg("buf:sys_info->e2g_action[%d].media_flag[%d] = %01X", event_id, i, sys_info->e2g_action[event_id].media_flag[i]);
		}
	
	int same_tmp = 0;
	int same_cnt = 0;
	for(same_tmp=0;same_tmp<E2G_MEDIA_MAX_NUM;same_tmp++){
		same_cnt = 0;
		//dbg("same_tmp = %d", same_tmp);
		for(i=0;i<E2G_EVENT_MAX_NUM;i++){
			
			if(sys_info->ipcam[E2G_EVENT0_INFO_VALID+i*EVENT_STRUCT_SIZE].config.u8 != 1 && (event_id != i))
				continue;
			
			for(j=0;j<E2G_SERVER_MAX_NUM;j++){
				//dbg("sys_info->e2g_action[%d].media_flag[%d] = %01X", i, j, sys_info->e2g_action[i].media_flag[j]);
				if(sys_info->e2g_action[i].media_flag[j] == same_tmp){
					//dbg("same_cnt = %d", same_cnt);
					if(++same_cnt > 1)
						return -1;
				}
			}
		}
	}
	
	return 0;
}
void set_event_type( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    int i = 0, para_cnt = 0;
    char *ptr[E2G_EVENT_PARA_NUM];
    char *str = argm->value;;
	
    memset(ptr, 0, sizeof(char*) * E2G_EVENT_PARA_NUM);
    EVENT_2G_TYPE event = {E2G_BASIC_EVENT_DEFAULT, E2G_SCHEDULE_DEFAULT, {E2G_TYPE_DEFAULT}};
    //bzero(&event, sizeof(EVENT_2G_TYPE));

    dbg("argm->value = %s ", argm->value);

    for(i = 0; i < E2G_EVENT_PARA_NUM + 1; i++) {
        str = strchr(str, ':');
        ptr[i] = str;

        if(str == NULL)
            break;
        
        str++;
        para_cnt++;
    }

    if(para_cnt != E2G_EVENT_PARA_NUM) {
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s=2\n", argm->name );
        dbg("event parameter count error \n");
        return;
    }


    do {

        event.info.id = atoi(argm->value);
        if(event.info.id > E2G_EVENT_MAX_NUM - 1) {
            dbg("event id error, id = %d ", event.info.id);
            break;
        }

        event.info.enable = atoi(ptr[ET_ENABLE]+1);
        if(event.info.enable > 1) {
            dbg("event enable value error, value = %d!!", event.info.enable);
            break;
        }

        int max = sizeof(event.info.name) - 1;
        int src = ptr[ET_SC_DAY] - (ptr[ET_NAME]+1);
        memset(event.info.name, 0, sizeof(event.info.name));
        strncpy(event.info.name, ptr[ET_NAME]+1, (max > src) ? src : max);
        if(strlen(event.info.name) < 1) {
            dbg("event name is empty!!");
            break;
        }
    	if(check_specical_symbols(event.info.name, FILENAME_SYMBOL) < 0){
			DBG("ET_NAME[%s] Error", event.info.name);
			break;
		}
        event.info.type = EVENT_TYPE_NORMAL;

        event.normal.priority = atoi(ptr[ET_PRIORITY]+1);
        if(event.normal.priority > 2) {
            dbg("event priority value error, value = %d ", event.normal.priority);
            break;
        }

        event.normal.delay = atoi(ptr[ET_DELAY]+1);
        if(event.normal.delay > MAX_E2G_DELAY) {
            dbg("event delay value error, value = %d ", event.normal.delay);
            break;
        }

        event.normal.trigger_type = atoi(ptr[ET_TRIGGER]+1);
        if(event.normal.trigger_type >= TRIG_BY_MAXNUM) {
            dbg("event trigger type value error, value = %d ", event.normal.trigger_type);
            break;
        }

        event.normal.periodic_time = atoi(ptr[ET_RESERVE]+1);
        if(event.normal.periodic_time > MAX_E2G_PERIODIC || event.normal.periodic_time < MIN_E2G_PERIODIC) {
            dbg("event periodic time value error, value = %d ", event.normal.periodic_time);
            break;
        }


        event.schedule.day = strtol(ptr[ET_SC_DAY]+1, NULL, 16);
        if(event.schedule.day > 0x007F) {
            dbg("event schedule day value error, value = %04X ", event.schedule.day);
            break;
        }

        event.schedule.mode = atoi(ptr[ET_SC_MODE]+1);
        if(event.schedule.mode > 1) {
            dbg("event schedule mode value error, value = %d", event.schedule.mode);
            break;
        }

        if( (sscanf(ptr[ET_SC_RANGE]+1, "%2d%2d%2d%2d%2d%2d", &event.schedule.start.nHour, 
            &event.schedule.start.nMin, &event.schedule.start.nSec, &event.schedule.end.nHour,
            &event.schedule.end.nMin, &event.schedule.end.nSec) != 6) ) {

            dbg("event schedule value error!! \n");
            break;
        }

		if( (event.schedule.start.nHour*60*60 + event.schedule.start.nMin*60) >= (event.schedule.end.nHour*60*60 + event.schedule.end.nMin*60) ) {
            dbg("event schedule value error!! \n");
            break;
        }
		
        event.normal.gioout_trig = atoi(ptr[ET_DO_EN]+1);
        if(event.normal.gioout_trig > 1) {
            dbg("event enable gio out error, value = %d \n", event.normal.gioout_trig);
            break;
        }

        event.normal.gioout_duration[0] = atoi(ptr[ET_DO_TIME]+1);
        if((event.normal.gioout_duration[0] > MAX_E2G_GIOOUT) || (event.normal.gioout_duration[0] < MIN_E2G_GIOOUT)) {
            dbg("event gio out duration error, value = %d \n", event.normal.gioout_duration[0]);
            break;
        }

        // each bit present server 1, max 5 action servers
        event.normal.action_server = strtol(ptr[ET_AC_SERVER]+1, NULL, 16);
		
#ifdef EVENT_ACTION_CHOICE	
		int server_count = 0;
		for(i=0;i<E2G_SERVER_MAX_NUM;i++){
			if((event.normal.action_server & (0x01 << i)) > 0)
				server_count++;
		}
		if(server_count>1)
			break;
#endif
		
        if(event.normal.action_server > 0x1F) {
            dbg("event action server error, value = %04X ", event.normal.action_server);
            break;
        }

        // action_media value start from 0, 0xf means no action media.
        event.normal.action_media = strtol(ptr[ET_AC_MEDIA]+1, NULL, 16);
        //if(event.normal.action_media > 55555 && event.normal.action_media != 0xFFFFF) {
        //  dbg("event action media error, value = %05X ", event.normal.action_media);
        //  break;
        //}

#ifdef SUPPORT_VISCA
		int ret = 0;

		if(check_act_media_status(event.info.id, event.normal.action_server, event.normal.action_media) < 0){
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s=3\n", argm->name );
			dbg("media already used");
			return;
		}

        event.normal.gioin_trig = strtol(ptr[ET_AC_DI_TRIG]+1, NULL, 16);
		event.normal.action_ptz_en = atoi(ptr[ET_AC_PTZ_EN]+1);
		event.normal.action_ptz_mode = atoi(ptr[ET_AC_PTZ_MODE]+1);
		
		if(event.normal.action_ptz_en == 1){
			if((atoi(ptr[ET_AC_PTZ_M_ARG]+1) < 1) || (atoi(ptr[ET_AC_PTZ_M_ARG]+1) > (MAX_PTZ_M_ARG+1)))
				break;
			
			event.normal.action_ptz_m_arg = (atoi(ptr[ET_AC_PTZ_M_ARG]+1))-1;

			switch(event.normal.action_ptz_mode){
				case VISCA_PRESET_MD:
					if(event.normal.action_ptz_m_arg > 255)
					break;
				case VISCA_SEQUENCE_MD:
					if(event.normal.action_ptz_m_arg > 7)
						ret = -1;
					break;
				case VISCA_AUTOPAN_MD:
					if(event.normal.action_ptz_m_arg > 3)
						ret = -1;
					break;
				case VISCA_CRUISE_MD:
					if(event.normal.action_ptz_m_arg > (sys_info->vsptz.cruiseline-1))
						ret = -1;
					break;
				default:
					ret= -1;
					break;
			}

			if(ret != 0){
				req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=3\n", argm->name );
				dbg("event action_ptz error, value = [%d, %d]", event.normal.action_ptz_mode, event.normal.action_ptz_m_arg);
			}
		}
#endif
		//dbg("EN=%d, MODE=%d, ARG=%d\n", event.normal.action_ptz_en, event.normal.action_ptz_mode, event.normal.action_ptz_m_arg);
        dbg("EVENT TYPE SETTING DONE \n");
        event.info.valid = 1;

        //if(1) {
        //if(memcmp(&event, &sys_info->config.e2g_event[event.info.id], sizeof(EVENT_2G_TYPE)) != 0) {
        dbg("write to config \n");

	   	//int event_2g_type_size = E2G_EVENT0_NORMAL_ACTION_MEDIA - E2G_EVENT0_INFO_VALID+1;
		int sel = event.info.id * EVENT_STRUCT_SIZE;
        
        info_write(&event.info.valid, E2G_EVENT0_INFO_VALID+sel, 0, 0);
        info_write(&event.info.enable, E2G_EVENT0_INFO_ENABLE+sel, 0, 0);
        info_write(&event.info.id, E2G_EVENT0_INFO_ID+sel, 0, 0);
        info_write(&event.info.type, E2G_EVENT0_INFO_TYPE+sel, 0, 0);
        info_write(event.info.name, E2G_EVENT0_INFO_NAME+sel, strlen(event.info.name), 0);
        
        info_write(&event.schedule.mode, E2G_EVENT0_SCHEDULE_MODE+sel, 0, 0);
        info_write(&event.schedule.day, E2G_EVENT0_SCHEDULE_DAY+sel, 0, 0);
        info_write(&event.schedule.start.nHour, E2G_EVENT0_SCHEDULE_START_NHOUR+sel, 0, 0);
        info_write(&event.schedule.start.nMin, E2G_EVENT0_SCHEDULE_START_NMIN+sel, 0, 0);
        info_write(&event.schedule.start.nSec, E2G_EVENT0_SCHEDULE_START_NSEC+sel, 0, 0);
        info_write(&event.schedule.end.nHour, E2G_EVENT0_SCHEDULE_END_NHOUR+sel, 0, 0);
        info_write(&event.schedule.end.nMin, E2G_EVENT0_SCHEDULE_END_NMIN+sel, 0, 0);
        info_write(&event.schedule.end.nSec, E2G_EVENT0_SCHEDULE_END_NSEC+sel, 0, 0);

        info_write(&event.normal.priority, E2G_EVENT0_NORMAL_PRIORITY+sel, 0, 0);
        info_write(&event.normal.delay, E2G_EVENT0_NORMAL_DELAY+sel, 0, 0);
        info_write(&event.normal.trigger_type, E2G_EVENT0_NORMAL_TRIGGER_TYPE+sel, 0, 0);
        info_write(&event.normal.motion_window, E2G_EVENT0_NORMAL_MOTION_WINDOW+sel, 0, 0);
        info_write(&event.normal.gioin_trig, E2G_EVENT0_NORMAL_GIOIN_TRIG+sel, 0, 0);
        info_write(&event.normal.gioin_type, E2G_EVENT0_NORMAL_GIOOUT_TYPE+sel, 0, 0);
        info_write(&event.normal.periodic_time, E2G_EVENT0_NORMAL_PERIODIC_TIME+sel, 0, 0);
        info_write(&event.normal.gioout_trig, E2G_EVENT0_NORMAL_GIOOUT_TRIG+sel, 0, 0);
        info_write(&event.normal.gioout_type, E2G_EVENT0_NORMAL_GIOOUT_TYPE+sel, 0, 0); 
        info_write(&event.normal.gioout_duration, E2G_EVENT0_NORMAL_GIOOUT_DURATION+sel, 0, 0);
        info_write(&event.normal.action_server, E2G_EVENT0_NORMAL_ACTION_SERVER+sel, 0, 0);
        info_write(&event.normal.action_media, E2G_EVENT0_NORMAL_ACTION_MEDIA+sel, 0, 0);
#ifdef SUPPORT_VISCA
		info_write(&event.normal.action_ptz_en, E2G_EVENT0_NORMAL_ACTION_PTZ_EN+sel, 0, 0);
		if(event.normal.action_ptz_en == 1){
			info_write(&event.normal.action_ptz_mode, E2G_EVENT0_NORMAL_ACTION_PTZ_MODE+sel, 0, 0);
			info_write(&event.normal.action_ptz_m_arg, E2G_EVENT0_NORMAL_ACTION_PTZ_M_ARG+sel, 0, 0);
		}
#endif		
            //sysinfo_write(&event, offsetof(SysInfo, config.e2g_event[event.info.id]), sizeof(EVENT_2G_TYPE), 0);
        //}

#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
		char log_triggertype[30] = "";
		switch(event.normal.trigger_type)
		{
			case TRIG_BY_MOTION:
				strcpy(log_triggertype, "Trigger : Motion Detection");
				break;
			case TRIG_BY_PERIODIC:
				strcpy(log_triggertype, "Trigger : Periodic");
				break;
			case TRIG_BY_GIOIN:
				strcpy(log_triggertype, "Trigger : Digital Input");
				break;
			case TRIG_BY_BOOT:
				strcpy(log_triggertype, "Trigger : System Boot");
				break;
			#ifdef SUPPORT_NETWOK_LOST
			case TRIG_BY_NETLOST:
				strcpy(log_triggertype, "Trigger : Network Lost");
				break;
			#endif
			#ifdef SUPPORT_VIDEO_LOST
			case TRIG_BY_VIDLOST:
				strcpy(log_triggertype, "Trigger : Video Lost");
				break;
			#endif
			#ifdef SUPPORT_PIR
			case TRIG_BY_PIR:
				strcpy(log_triggertype, "Trigger : Passive Infrared sensor");
				break;
			#endif
			default:
				strcpy(log_triggertype, "Trigger : Unknown");
				break;
		}
        snprintf(logstr, MSG_USE_LEN, "%s SET EVENT TYPE %d ; %s", req->curr_user.id, event.info.id + 1, log_triggertype);
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif

        // convert minute to second
        sys_info->e2g_period[event.info.id] = event.normal.periodic_time * 60;
            
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name );
        return;

    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=3\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void del_event_type( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    __u32 id = atoi(argm->value);

    int sel = id * EVENT_STRUCT_SIZE;

    do {
        if(id >= E2G_EVENT_MAX_NUM)
            break;

        /*EVENT_2G_BASIC info = E2G_BASIC_EVENT_DEFAULT;
        EVENT_2G_SCHEDULE schedule = E2G_SCHEDULE_DEFAULT;
        EVENT_TYPE normal = E2G_TYPE_DEFAULT;
        EVENT_2G_TYPE event;

        memset(&event, 0, sizeof(EVENT_2G_TYPE));
        memcpy(&event.info, &info, sizeof(EVENT_2G_BASIC));
        memcpy(&event.schedule, &schedule, sizeof(EVENT_2G_SCHEDULE));
        memcpy(&event.normal, &normal, sizeof(EVENT_TYPE));
        
        memcpy(&sys_info->config.e2g_event[id], &event, sizeof(EVENT_2G_TYPE));*/

        info_write(&sys_info->ipcam[E2G_EVENT0_INFO_VALID+sel].def_value.value, E2G_EVENT0_INFO_VALID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_INFO_ENABLE+sel].def_value.value, E2G_EVENT0_INFO_ENABLE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_INFO_ID+sel].def_value.value, E2G_EVENT0_INFO_ID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_INFO_TYPE+sel].def_value.value, E2G_EVENT0_INFO_TYPE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_INFO_NAME+sel].def_value.value, E2G_EVENT0_INFO_NAME+sel, 0, 0);
        
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_MODE+sel].def_value.value, E2G_EVENT0_SCHEDULE_MODE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_DAY+sel].def_value.value, E2G_EVENT0_SCHEDULE_DAY+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NHOUR+sel].def_value.value, E2G_EVENT0_SCHEDULE_START_NHOUR+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NMIN+sel].def_value.value, E2G_EVENT0_SCHEDULE_START_NMIN+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_START_NSEC+sel].def_value.value, E2G_EVENT0_SCHEDULE_START_NSEC+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NHOUR+sel].def_value.value, E2G_EVENT0_SCHEDULE_END_NHOUR+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NMIN+sel].def_value.value, E2G_EVENT0_SCHEDULE_END_NMIN+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_SCHEDULE_END_NSEC+sel].def_value.value, E2G_EVENT0_SCHEDULE_END_NSEC+sel, 0, 0);

        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_PRIORITY+sel].def_value.value, E2G_EVENT0_NORMAL_PRIORITY+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_DELAY+sel].def_value.value, E2G_EVENT0_NORMAL_DELAY+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_TRIGGER_TYPE+sel].def_value.value, E2G_EVENT0_NORMAL_TRIGGER_TYPE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_MOTION_WINDOW+sel].def_value.value, E2G_EVENT0_NORMAL_MOTION_WINDOW+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_GIOIN_TRIG+sel].def_value.value, E2G_EVENT0_NORMAL_GIOIN_TRIG+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_GIOIN_TYPE+sel].def_value.value, E2G_EVENT0_NORMAL_GIOIN_TYPE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_PERIODIC_TIME+sel].def_value.value, E2G_EVENT0_NORMAL_PERIODIC_TIME+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_GIOOUT_TRIG+sel].def_value.value, E2G_EVENT0_NORMAL_GIOOUT_TRIG+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_GIOOUT_TYPE+sel].def_value.value, E2G_EVENT0_NORMAL_GIOOUT_TYPE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_GIOOUT_DURATION+sel].def_value.value, E2G_EVENT0_NORMAL_GIOOUT_DURATION+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_SERVER+sel].def_value.value, E2G_EVENT0_NORMAL_ACTION_SERVER+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_MEDIA+sel].def_value.value, E2G_EVENT0_NORMAL_ACTION_MEDIA+sel, 0, 0);
		info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_PTZ_EN+sel].def_value.value, E2G_EVENT0_NORMAL_ACTION_PTZ_EN+sel, 0, 0);
		info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_PTZ_MODE+sel].def_value.value, E2G_EVENT0_NORMAL_ACTION_PTZ_MODE+sel, 0, 0);
		info_write(&sys_info->ipcam[E2G_EVENT0_NORMAL_ACTION_PTZ_M_ARG+sel].def_value.value, E2G_EVENT0_NORMAL_ACTION_PTZ_M_ARG+sel, 0, 0);

		

        //sysinfo_write(&event, offsetof(SysInfo, config.e2g_event[id]), sizeof(EVENT_2G_TYPE), 0);

#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
        snprintf(logstr, MSG_USE_LEN, "%s DELETE EVENT TYPE %d", req->curr_user.id, id + 1 );
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void set_event_record( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    int i = 0, para_cnt = 0;
    EVENT_2G_RECORD record;
    char *ptr[E2G_RECORD_PARA_NUM];
    char *str = argm->value;;

    memset(ptr, 0, sizeof(char*) * E2G_RECORD_PARA_NUM);
    memset(&record, 0, sizeof(EVENT_2G_RECORD));

    dbg("argm->value = %s ", argm->value);

    for(i = 0; i < E2G_RECORD_PARA_NUM + 1; i++) {
        str = strchr(str, ':');
        ptr[i] = str;

        if(str == NULL) 
            break;
        
        str++;
        para_cnt++;
    }

    if(para_cnt != E2G_RECORD_PARA_NUM) {
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s=2\n", argm->name );
        dbg("event server parameter count error \n");
        return;
    }
    

    do {

        record.info.id = atoi(argm->value);
        if(record.info.id > E2G_RECORD_MAX_NUM - 1) {
            dbg("record id error, id = %d ", record.info.id);
            break;
        }

        record.info.enable = atoi(ptr[ER_ENABLE]+1);
        if(record.info.enable > 1) {
            dbg("record enable value error, value = %d!!", record.info.enable);
            break;
        }

        int max = sizeof(record.info.name) - 1;
        int src = ptr[ER_SC_DAY] - (ptr[ER_NAME]+1);
        memset(record.info.name, 0, sizeof(record.info.name));
        strncpy(record.info.name, ptr[ER_NAME]+1, (max > src) ? src : max);
        if(strlen(record.info.name) < 1) {
            dbg("record name is empty!!");
            break;
        }
		if(check_specical_symbols(record.info.name, FILENAME_SYMBOL) < 0){
			DBG("ER_NAME[%s] Error", record.info.name);
			break;
		}
        record.info.type = ERECORD_TYPE_NORMAL;

        record.normal.priority = atoi(ptr[ER_PRIORITY]+1);
        if(record.normal.priority > 2) {
            dbg("record priority value error, value = %d ", record.normal.priority);
            break;
        }

        record.normal.source = atoi(ptr[ER_SOURCE]+1);
        if(record.normal.source > E2G_SERVER_MAX_NUM) {
            dbg("record source value error, value = %d ", record.normal.source);
            break;
        }

        record.schedule.day = strtol(ptr[ER_SC_DAY]+1, NULL, 16);
        if(record.schedule.day > 0x007F) {
            dbg("record schedule day value error, value = %04X ", record.schedule.day);
            break;
        }

        record.schedule.mode = atoi(ptr[ER_SC_MODE]+1);
        if(record.schedule.mode > 1) {
            dbg("record schedule mode value error, value = %d", record.schedule.mode);
            break;
        }

        if( (sscanf(ptr[ER_SC_RANGE]+1, "%2d%2d%2d%2d%2d%2d", &record.schedule.start.nHour, 
            &record.schedule.start.nMin, &record.schedule.start.nSec, &record.schedule.end.nHour,
            &record.schedule.end.nMin, &record.schedule.end.nSec) != 6) ) {

            dbg("record schedule value error!! ");
            break;
        }

        //record.normal.dest_server = strtol(ptr[ER_DEST]+1, NULL, 16);
        //if(record.normal.dest_server > E2G_SERVER_MAX_NUM - 1 && record.normal.dest_server != 0xF) {
        //  dbg("record destination server error, value = %d ", record.normal.dest_server);
        //  break;
        //}

        // 0 : NONE, 1 : SAMBA, 2 : SD
        record.normal.dest_server = atoi(ptr[ER_DEST]+1);
        if(/*record.normal.dest_server < 0 ||*/ record.normal.dest_server > 2 ) {
            dbg("ERROR destination server number %d", record.normal.dest_server);
            break;
        }
        dbg("record.normal.dest_server = %d", record.normal.dest_server);

        record.normal.rewrite_limit = strtoul(ptr[ER_MAX_SIZE]+1, NULL, 10);
        if( record.normal.rewrite_limit < MIN_E2G_REWRITE || record.normal.rewrite_limit > MAX_E2G_REWRITE ) {
            dbg("record rewrite limit value error, value = %u ", record.normal.rewrite_limit);
            break;
        }

        record.normal.file_max_size = strtoul(ptr[ER_FILE_SIZE]+1, NULL, 10);
        if( record.normal.file_max_size < MIN_E2G_REC_FILESIZE || record.normal.file_max_size > MAX_E2G_REC_FILESIZE ) {
            dbg("record file size value error, value = %u ", record.normal.file_max_size);
            break;
        }
		
		if(check_specical_symbols(ptr[ER_PREFIX]+1, FILENAME_SYMBOL) < 0){
			DBG("ER_PREFIX [%s] Error", ptr[ER_PREFIX]+1);
			break;
		}
		else
			strncpy(record.normal.filename, ptr[ER_PREFIX]+1, E2G_FILE_NAME_LEN);
		
        /*
        if( strlen(record.normal.filename) < 1 ) {
            dbg("record filename pre-fix error, value = %s \n", record.normal.filename);
            break;
        }
        */
        
        dbg("EVENT RECORD SETTING DONE ");
        record.info.valid = 1;

        //if(1) {
        //if(memcmp(&record, &sys_info->config.e2g_record[record.info.id], sizeof(EVENT_2G_RECORD)) != 0) {
        dbg("write to config ");
        //int record_2g_type_size = E2G_RECORD0_NORMAL_FILENAME - E2G_RECORD0_INFO_VALID+1;
        int sel = record.info.id * RECORD_STRUCT_SIZE;
        
        info_write(&record.info.valid, E2G_RECORD0_INFO_VALID+sel, 0, 0);
        info_write(&record.info.enable, E2G_RECORD0_INFO_ENABLE+sel, 0, 0);
        info_write(&record.info.id, E2G_RECORD0_INFO_ID+sel, 0, 0);
        info_write(&record.info.type, E2G_RECORD0_INFO_TYPE+sel, 0, 0);
        info_write(record.info.name, E2G_RECORD0_INFO_NAME+sel, strlen(record.info.name), 0);
        
        info_write(&record.schedule.mode, E2G_RECORD0_SCHEDULE_MODE+sel, 0, 0);
        info_write(&record.schedule.day, E2G_RECORD0_SCHEDULE_DAY+sel, 0, 0);
        info_write(&record.schedule.start.nHour, E2G_RECORD0_SCHEDULE_START_NHOUR+sel, 0, 0);
        
        info_write(&record.schedule.start.nMin, E2G_RECORD0_SCHEDULE_START_NMIN+sel, 0, 0);
        info_write(&record.schedule.start.nSec, E2G_RECORD0_SCHEDULE_START_NSEC+sel, 0, 0);
        info_write(&record.schedule.end.nHour, E2G_RECORD0_SCHEDULE_END_NHOUR+sel, 0, 0);
        info_write(&record.schedule.end.nMin, E2G_RECORD0_SCHEDULE_END_NMIN+sel, 0, 0);
        info_write(&record.schedule.end.nSec, E2G_RECORD0_SCHEDULE_END_NSEC+sel, 0, 0);

        info_write(&record.normal.priority, E2G_RECORD0_NORMAL_PRIORITY+sel, 0, 0);
        info_write(&record.normal.source, E2G_RECORD0_NORMAL_SOURCE+sel, 0, 0);
        info_write(&record.normal.dest_server, E2G_RECORD0_NORMAL_DEST_SERVER+sel, 0, 0);
        info_write(&record.normal.rewrite_limit, E2G_RECORD0_NORMAL_REWRITE_LIMIT+sel, 0, 0);
        info_write(&record.normal.file_max_size, E2G_RECORD0_NORMAL_FILE_MAX_SIZE+sel, 0, 0);
        info_write(&record.normal.filename_suffix, E2G_RECORD0_NORMAL_FILENAME_SUFFIX+sel, 0, 0);
        info_write(record.normal.filename, E2G_RECORD0_NORMAL_FILENAME+sel, strlen(record.normal.filename), 0);

#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
        snprintf(logstr, MSG_USE_LEN, "%s SET EVENT RECORD %d ; Name : %s", req->curr_user.id, record.info.id + 1, record.info.name);
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif   
            //sysinfo_write(&record, offsetof(SysInfo, config.e2g_record[record.info.id]), sizeof(EVENT_2G_RECORD), 0);
        //}
            
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name );
        return;

    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=3\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void del_event_record( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EVENT_2G

    __u32 id = atoi(argm->value);

    int sel = id * RECORD_STRUCT_SIZE;
    do {
        if(id >= E2G_RECORD_MAX_NUM)
            break;

        /*EVENT_2G_BASIC info = E2G_BASIC_RECORD_DEFAULT;
        EVENT_2G_SCHEDULE schedule = E2G_SCHEDULE_DEFAULT;
        EVENT_RECORD normal = E2G_RECORD_DEFAULT;
        EVENT_2G_RECORD record;

        memset(&record, 0, sizeof(EVENT_2G_RECORD));
        memcpy(&record.info, &info, sizeof(EVENT_2G_BASIC));
        memcpy(&record.schedule, &schedule, sizeof(EVENT_2G_SCHEDULE));
        memcpy(&record.normal, &normal, sizeof(EVENT_RECORD));
        
        memcpy(&sys_info->config.e2g_record[id], &record, sizeof(EVENT_RECORD));*/

        info_write(&sys_info->ipcam[E2G_RECORD0_INFO_VALID+sel].def_value.value, E2G_RECORD0_INFO_VALID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_INFO_ENABLE+sel].def_value.value, E2G_RECORD0_INFO_ENABLE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_INFO_ID+sel].def_value.value, E2G_RECORD0_INFO_ID+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_INFO_TYPE+sel].def_value.value, E2G_RECORD0_INFO_TYPE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_INFO_NAME+sel].def_value.value, E2G_RECORD0_INFO_NAME+sel, 0, 0);
                
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_MODE+sel].def_value.value, E2G_RECORD0_SCHEDULE_MODE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_DAY+sel].def_value.value, E2G_RECORD0_SCHEDULE_DAY+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_START_NHOUR+sel].def_value.value, E2G_RECORD0_SCHEDULE_START_NHOUR+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_START_NMIN+sel].def_value.value, E2G_RECORD0_SCHEDULE_START_NMIN+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_START_NSEC+sel].def_value.value, E2G_RECORD0_SCHEDULE_START_NSEC+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_END_NHOUR+sel].def_value.value, E2G_RECORD0_SCHEDULE_END_NHOUR+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_END_NMIN+sel].def_value.value, E2G_RECORD0_SCHEDULE_END_NMIN+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_SCHEDULE_END_NSEC+sel].def_value.value, E2G_RECORD0_SCHEDULE_END_NSEC+sel, 0, 0);
        
        info_write(&sys_info->ipcam[E2G_RECORD0_NORMAL_PRIORITY+sel].def_value.value, E2G_RECORD0_NORMAL_PRIORITY+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_NORMAL_SOURCE+sel].def_value.value, E2G_RECORD0_NORMAL_SOURCE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_NORMAL_DEST_SERVER+sel].def_value.value, E2G_RECORD0_NORMAL_DEST_SERVER+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_NORMAL_REWRITE_LIMIT+sel].def_value.value, E2G_RECORD0_NORMAL_REWRITE_LIMIT+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_NORMAL_FILE_MAX_SIZE+sel].def_value.value, E2G_RECORD0_NORMAL_FILE_MAX_SIZE+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_NORMAL_FILENAME_SUFFIX+sel].def_value.value, E2G_RECORD0_NORMAL_FILENAME_SUFFIX+sel, 0, 0);
        info_write(&sys_info->ipcam[E2G_RECORD0_NORMAL_FILENAME+sel].def_value.value, E2G_RECORD0_NORMAL_FILENAME+sel, 0, 0);

        //sysinfo_write(&record, offsetof(SysInfo, config.e2g_record[id]), sizeof(EVENT_RECORD), 0);
#ifdef MSGLOGNEW
        char logstr[MSG_MAX_LEN] = "";
        snprintf(logstr, MSG_USE_LEN, "%s DELETE EVENT RECORD %d", req->curr_user.id, id + 1 );
		sprintf(logstr, "%s\r\n", logstr);
        sysdbglog(logstr);
#endif        
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
*                                                                         *
***************************************************************************/
void dv840_output(request *req, COMMAND_ARGUMENT *argm)
{
	char cmd_v[ 12 ];
	int trg_v[12];
	char trg[24];
	char temp[3];
	int i = 0,j = 0;
	
	for( i = 0, j = 0; i < 10; i+=2, j++ ){
		strncpy(temp, argm->value + i , 2);
		temp[ 2 ] = '\0';
		cmd_v[ j ] = strtol(temp, NULL, 16);
	}
	if ( sys_info->ipcam[RS485_PROTOCOL].config.u8 == 0 )// 0 is D, 1 is P, 2 is VISCA
	{	trg_v[0] = 0xFF;
		trg_v[1] = sys_info->ipcam[RS485_ID].config.u8;
	}
	else if( sys_info->ipcam[RS485_PROTOCOL].config.u8 == 1 )
	{	trg_v[0] = 0xA0;
		trg_v[1] = sys_info->ipcam[RS485_ID].config.u8 -1 ;
	}else if( sys_info->ipcam[RS485_PROTOCOL].config.u8 == 2 )
/*
	if ( sys_info->config.rs485.protocol == 0 )// 0 is D, 1 is P, 2 is VISCA
	{	trg_v[0] = 0xFF;
		trg_v[1] = sys_info->config.rs485.id;
	}
	else if( sys_info->config.rs485.protocol == 1 )
	{	trg_v[0] = 0xA0;
		trg_v[1] = sys_info->config.rs485.id -1 ;
	}else if( sys_info->config.rs485.protocol == 2 )
*/
	switch(cmd_v[ 1 ]) {
		case 0x00:	
			if (cmd_v[2] == 0x00) {// PTZ stop
				trg_v[2] = 0x0;	trg_v[3] = 0x0;	trg_v[4] = 0x0;	trg_v[5] = 0x0;
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x02) { //SETP_Right
				trg_v[2] = 0x0;	trg_v[3] = 0x02;trg_v[4] = cmd_v[3];trg_v[5] = 0x0;
				dbg("DV840:Set_Right %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x03) { //Set Preset
				trg_v[2] = 0x0;trg_v[3] = 0x03;	trg_v[4] = 0x0;trg_v[5] = cmd_v[3];
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x04) { //SETP_LEFT
				trg_v[2] = 0x0;	trg_v[3] = 0x04;trg_v[4] = cmd_v[3];trg_v[5] = 0x0;
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x05) { //Clear Preset
				trg_v[2] = 0x0;	trg_v[3] = 0x05;trg_v[4] = 0x0;	trg_v[5] = cmd_v[3];
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x07) { //ZERO PAN   //appro is other cmd
				trg_v[2] = 0x0;trg_v[3] = 0x07;trg_v[4] = 0x0;
				if (trg_v[5] == 0x5F) 	trg_v[5] = 0x5F;
				else 					trg_v[5] = 0x22;
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x08) { //CMD_STEP_UP
				trg_v[2] = 0x0;trg_v[3] = 0x08;trg_v[4] = 0x0;trg_v[5] = cmd_v[3];
			}
			else if (cmd_v[2] == 0x10) { //TILT_DOWN
				trg_v[2] = 0x0;trg_v[3] = 0x10;trg_v[4] = 0x0;trg_v[5] = cmd_v[3];
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x20){ //Zoom in
				trg_v[2] = 0x0;	trg_v[3] = 0x20; trg_v[4] = 0x0; trg_v[5] = 0x0;
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x40){ //Zoom out
				trg_v[2] = 0x0; trg_v[3] = 0x40; trg_v[4] = 0x0; trg_v[5] = 0x0;
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x0A) { //SETP_RIGHT_UP
				trg_v[2] = 0x0;trg_v[3] = 0x0A;trg_v[4] = cmd_v[3];trg_v[5] = cmd_v[3];
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x0C) { //SETP_LEFT_UP
				trg_v[2] = 0x0;trg_v[3] = 0x0C;trg_v[4] = cmd_v[3];trg_v[5] = cmd_v[3];
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x12) { //SETP_RIGHT_DOWN
				trg_v[2] = 0x0;trg_v[3] = 0x12;trg_v[4] = cmd_v[3];trg_v[5] = cmd_v[3];
				dbg("DV840: %d\n", __LINE__);
			}
			else if (cmd_v[2] == 0x14) { //SETP_LEFT_DOWN
				trg_v[2] = 0x0;trg_v[3] = 0x14;trg_v[4] = cmd_v[3];trg_v[5] = cmd_v[3];
				dbg("DV840: %d\n", __LINE__);
			}
		break;
		
		case 0x01: //Focus far;
			trg_v[2] = 0x01; trg_v[3] = 0x0; trg_v[4] = 0x0; trg_v[5] = 0x0;
			dbg("DV840: %d\n", __LINE__);
		break;
			
		case 0x02: //Focus near;
			trg_v[2] = 0x02; trg_v[3] = 0x0; trg_v[4] = 0x0; trg_v[5] = 0x0;
			dbg("DV840: %d\n", __LINE__);
		break;
		
		case 0x04: //iris open;
			trg_v[2] = 0x04; trg_v[3] = 0x0; trg_v[4] = 0x0; trg_v[5] = 0x0;
			dbg("DV840: %d\n", __LINE__);
		break;
		
		case 0x08: //iris close;
			trg_v[2] = 0x08; trg_v[3] = 0x0; trg_v[4] = 0x0; trg_v[5] = 0x0;
			dbg("DV840: %d\n", __LINE__);
		break;
		
	}

	//if ( sys_info->config.rs485.protocol == 0 )// 0 is D, 1 is P
	if ( sys_info->ipcam[RS485_PROTOCOL].config.u8 == 0 )// 0 is D, 1 is P
	{
		trg_v[6] = 0x0 ;

		for ( i = 1 ; i < 6 ; i++ ) {
			if (trg_v[6] <= 256) {
				trg_v[6] = trg_v[6] % 256 + trg_v[i] ;//D - protocol
			}
			else {
				trg_v[6] = trg_v[6] + trg_v[i] ;	
			}
		}
//dbg ("trg_v[0]: %x	\ntrg_v[1]: %x \ntrg_v[2]: %x \ntrg_v[3]: %x \ntrg_v[4]: %x \ntrg_v[5]: %x \ntrg_v[6]: %x  \n",trg_v[0],trg_v[1],trg_v[2],trg_v[3],trg_v[4],trg_v[5],trg_v[6]);
		int val;
		for( i = 0; i < 7; i++ ) {
			val = ( trg_v[ i ] & 0xf0 ) >> 4;
			if ( val >=0 && val <= 9 )				trg[ i * 2 ] = val + '0';
			else if ( val >=0xa && val <= 0xf ) 	trg[ i * 2 ] = val + 'A' -10;
			
			val = ( trg_v[ i ] & 0x0f );
			if ( val >=0 && val <= 9 )				trg[ i * 2 + 1 ] = val + '0';
			else if ( val >=0xa && val <= 0xf ) 	trg[ i * 2 + 1 ] = val + 'A' - 10;
		}
	}
	else
	{
		trg_v[6] = 0xAF;
		trg_v[7] = 0x00;
		
		for ( i = 0 ; i <= 6 ; i++ ) {
			trg_v[7] = trg_v[7] ^ trg_v[i];//P - protocol
		}
//dbg ("trg_v[0]: %2x	\ntrg_v[1]: %2x \ntrg_v[2]: %2x \ntrg_v[3]: %2x \ntrg_v[4]: %2x \ntrg_v[5]: %2x \ntrg_v[6]: %2x  \ntrg_v[7]: %2x\n",trg_v[0],trg_v[1],trg_v[2],trg_v[3],trg_v[4],trg_v[5],trg_v[6],trg_v[7]);
		int val;
		for( i = 0; i < 8; i++ ) {
			val = ( trg_v[ i ] & 0xf0 ) >> 4;
			if ( val >=0 && val <= 9 )				trg[ i * 2 ] = val + '0';
			else if ( val >=0xa && val <= 0xf ) 	trg[ i * 2 ] = val + 'A' -10;
			
			val = ( trg_v[ i ] & 0x0f );
			if ( val >=0 && val <= 9 )				trg[ i * 2 + 1 ] = val + '0';
			else if ( val >=0xa && val <= 0xf ) 	trg[ i * 2 + 1 ] = val + 'A' - 10;
		}
	}

	do {
		if(rs485_write_data(trg) == -1)
			break;
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		return;
	} while (0);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);

}
/***************************************************************************
*                                                                         *
***************************************************************************/
/*	
	PTZ protocol: Video System Control Architecture (VISCA)
	Baud rate: 9600, 	Start bit: 1, Data bit: 8, Parity: None, 	Stop bit: 1, MSB format
	If you wanna more detial about VISCA, please visit the following web sites.
	1. http://damien.douxchamps.net/libvisca/
	2. http://www.chuktech.net/video/ViscaProtocol.pdf

	Input arguement:
	first 2 bytes is functionality.
	Rest 8 bytes is arguement 0~3 and each of them is 2 bytes.
*/
void visca_protocol(request *req, COMMAND_ARGUMENT *argm)
{
	VISCACamera_t    camera;
	VISCAInterface_t interface;
	unsigned int  arg = 0, option = 0;
	char          temp1[3],temp2[9];
	unsigned int info_value, info_extend;

//	parsing the instruction to option and arguement.
	strncpy( temp1, argm->value, 2 );
	temp1[2]='\0';
	option = strtol(temp1, NULL, 16);
	
	strncpy( temp2, argm->value + sizeof(UInt16_t), sizeof(UInt16_t)*4 );
	temp2[8]='\0';
	arg = strtoul(temp2, NULL, 16);	
	
	interface.broadcast = 0;
//	camera.socket_num	= ( option < 65 || option > 78 ) ? 1 : 2;
	camera.socket_num	= 2;
	//camera.address      = sys_info->config.rs485.id;
	camera.address   = 1;//sys_info->ipcam[RS485_ID].config.u8;

	if (VISCA_open_serial( &interface, VISCA_DEV ) != VISCA_SUCCESS)
		goto VISCA_ERROR;

	VISCA_command_dispatch( &interface, &camera, option, arg, &info_value, &info_extend);

	if ( VISCA_close_serial(&interface) != VISCA_SUCCESS )
		goto VISCA_ERROR;

	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
	req->buffer_end += sprintf(req_bufptr(req), "0x%08X\n", info_value);
	req->buffer_end += sprintf(req_bufptr(req), " %08X\n", info_extend);
	return;

VISCA_ERROR:
	VISCA_close_serial(&interface);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/ 
void set_afzoom( request *req, COMMAND_ARGUMENT *argm )
{
#if 0//def SUPPORT_AF
	int fd;
	int flag, i;
	
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AUTOFOCUS_ZOOM_POSTITION].config.value);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);
        dbg("value = %d\n", value);

	//	if(value > sys_info->max_affocus){
		//		dbg("sys_info->max_affocus = %d\n", sys_info->max_affocus);
		//		break;
		//	}

		if ( ( fd = open( "/dev/lencontrol", O_RDWR ) ) < 0 )
			break;

		ioctl( fd, LENCTRL_SET_ZOOM_POSITION, value );
		ioctl( fd, LENCTRL_ZOOM_START, 0 );
		for(i=0;i<10;i++)
		{
		
			ioctl( fd, LENCTRL_STATUS, &flag );
			dbg("===LENCTRL_STATUS = %x", flag);
			//dbg("=====LENCTRL_ZOOMDONE = %x", LENCTRL_ZOOMDONE);
			if(flag & LENCTRL_ZOOMDONE){
				dbg("=====LENCTRL_ZOOMDONE====== \n\n\n");
				ioctl( fd, LENCTRL_CLRSTATUS, LENCTRL_ZOOMDONE );
				break;
			}
		}

		info_write(&value, AUTOFOCUS_ZOOM_POSTITION, sizeof(value), 0);
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		close( fd );
		return;
		
	} while(0);
	
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void set_affocus( request *req, COMMAND_ARGUMENT *argm )
{
#if 0//def SUPPORT_AF
	int fd;
	int flag, i;
	
	if (!argm->value[0]) {
		req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[AUTOFOCUS_FOCUS_POSTITION].config.value);
		return;
	}

	do {
        if(numeric_check(argm->value) < 0)
            break;

        int value = atoi(argm->value);
        dbg("value = %d\n", value);
		
	//	if(value > sys_info->max_affocus){
	//		dbg("sys_info->max_affocus = %d\n", sys_info->max_affocus);
	//		break;
	//	}
		
		if ( ( fd = open( "/dev/lencontrol", O_RDWR ) ) < 0 )
			break;
		




		ioctl( fd, LENCTRL_SET_FOCUS_POSITION, value );		
		ioctl( fd, LENCTRL_FOCUS_START, 0 );
		for(i=0;i<10;i++)
		{
			uninterrupted_sleep(1, 0);
			ioctl( fd, LENCTRL_STATUS, &flag );
			dbg("=====LENCTRL_STATUS = %x", flag);
			//dbg("=====LENCTRL_FOCUSDONE = %x", LENCTRL_FOCUSDONE);
			if(flag & LENCTRL_FOCUSDONE){
				dbg("=====LENCTRL_FOCUSDONE====== \n\n\n");
				ioctl( fd, LENCTRL_CLRSTATUS, LENCTRL_FOCUSDONE );
				break;
			}
		}

		info_write(&value, AUTOFOCUS_FOCUS_POSTITION, sizeof(value), 0);
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		close( fd );
		return;
	} while(0);
	
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void set_auto_iris( request *req, COMMAND_ARGUMENT *argm )
{
#if 1

  if (!argm->value[0]) {
	  req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, GetAutoIris());
	  return;
  }

  int value = atoi(argm->value);

    do {
		if (value > 1)	break;
		SetAutoIris(value);
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, GetAutoIris() );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}

void get_mcu_wdt( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_AF//CO_PROCESSOR, modify jiahung
    int value;

    do {
        if( (value = GetMCUWDT()) < 0)
            break;

		if(value != sys_info->status.mcu_wdt)
			DBGERR("MCU value != sys_info->status.mcu_wdt\n");
		
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, value );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void set_mcu_wdt( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_AF//CO_PROCESSOR, modify jiahung
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, sys_info->status.mcu_wdt);
        return;
    }
	
	int ret;
	int value = atoi(argm->value);

    do {
 		if(value > 1)
			break;
		
        ret = SetMCUWDT(value);
        if(ret < 0)
            break;
		
		sys_info->status.mcu_wdt = value;
		
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}

void disablemcuwdt( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_AF//CO_PROCESSOR, modify jiahung

    do {
       	unlink(MCU_WDT_PATH);
		
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}

void get_af_temp( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef CO_PROCESSOR
    int value;

    do {
        if( (value = GetAfTemperature()) < 0)
            break;
        
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, value );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/ 
void get_af_adc( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef CO_PROCESSOR
    int value;

    do {
        if( (value = GetAfADC()) < 0)
            break;
        
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, value );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void get_autofocusbusy( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_AF
//should call GetAfZoomDone(server)
    do {
        
        if( sys_info->af_status.mode == AF_MANUAL)
        	req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=0\n", argm->name);
		else
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name);
		
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void eptz_speed( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EPTZ
	int speed = atoi(argm->value);

	if (!argm->value[0]) {
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[EPTZ_SPEED].config.value);
			return;
	}

    do {
		if((speed > sys_info->ipcam[EPTZ_SPEED].max) || (speed < sys_info->ipcam[EPTZ_SPEED].min))
			break;
		
        if( sys_info->ipcam[EPTZ_SPEED].config.value != speed)
        	info_write(&speed, EPTZ_SPEED, sizeof(speed), 0);
		
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void eptz_pspeed( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EPTZ
	int speed = atoi(argm->value);

	if (!argm->value[0]) {
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[EPTZ_PSPEED].config.value);
			return;
	}

    do {
        if((speed > sys_info->ipcam[EPTZ_PSPEED].max) || (speed < sys_info->ipcam[EPTZ_PSPEED].min))
			   break;
		
        if( sys_info->ipcam[EPTZ_PSPEED].config.value != speed)
        	info_write(&speed, EPTZ_PSPEED, sizeof(speed), 0);
		
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void eptz_tspeed( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EPTZ
	int speed = atoi(argm->value);

	if (!argm->value[0]) {
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[EPTZ_TSPEED].config.value);
			return;
	}

    do {
        if((speed > sys_info->ipcam[EPTZ_TSPEED].max) || (speed < sys_info->ipcam[EPTZ_TSPEED].min))
			break;
				
        if( sys_info->ipcam[EPTZ_TSPEED].config.value != speed)
        	info_write(&speed, EPTZ_TSPEED, sizeof(speed), 0);
		
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void eptz_zspeed( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EPTZ
	int speed = atoi(argm->value);

	if (!argm->value[0]) {
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[EPTZ_ZSPEED].config.value);
			return;
	}

    do {
        if((speed > sys_info->ipcam[EPTZ_ZSPEED].max) || (speed < sys_info->ipcam[EPTZ_ZSPEED].min))
			break;
				
        if( sys_info->ipcam[EPTZ_ZSPEED].config.value != speed)
        	info_write(&speed, EPTZ_ZSPEED, sizeof(speed), 0);
		
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
void eptz_advspeed( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_EPTZ
	int speed = atoi(argm->value);

	if (!argm->value[0]) {
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[EPTZ_ADVSPEED].config.value);
			return;
	}

    do {
        if((speed > sys_info->ipcam[EPTZ_ADVSPEED].max) || (speed < sys_info->ipcam[EPTZ_ADVSPEED].min))
			break;
				
        if( sys_info->ipcam[EPTZ_ADVSPEED].config.value != speed)
        	info_write(&speed, EPTZ_ADVSPEED, sizeof(speed), 0);
		
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}

void lens_speed( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef SUPPORT_AF
	int speed = atoi(argm->value);

	if (!argm->value[0]) {
			req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, sys_info->ipcam[LENS_SPEED].config.value);
			return;
	}

    do {
        if((speed > sys_info->ipcam[LENS_SPEED].max) || (speed < sys_info->ipcam[LENS_SPEED].min))
			break;
				
        if( sys_info->ipcam[LENS_SPEED].config.value != speed)
        	info_write(&speed, LENS_SPEED, sizeof(speed), 0);
		
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name);
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}



/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void manual_ctrl_fan( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef CO_PROCESSOR
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, fan_status ? 1 : 0);
        return;
    }

    do {
        // 0 : Disable Fan
        // 1 : Enable Fan
        if( (fan_status = atoi(argm->value)) > 1)
            break;

        if(fan_status)
            fan_status = heater_status | MANUAL_FAN;
        else
            fan_status = heater_status & ~MANUAL_FAN;

        if(ManualCtrlFan(fan_status) < 0)
            break;
        
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, fan_status ? 1 : 0 );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}
/***************************************************************************
 *                                                                         *
 ***************************************************************************/ 
void manual_ctrl_heater( request *req, COMMAND_ARGUMENT *argm )
{
#ifdef CO_PROCESSOR
    if (!argm->value[0]) {
        req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name, heater_status);
        return;
    }

    do {
        // 0 : Manual Heater OFF
        // 1 : Manual Heater ON (Low Heat)
        // 2 : Manual Heater High Heat
        heater_status = atoi(argm->value);
        switch(heater_status) {
            case 0:
                heater_status = 0;
                break;

            case 1:
                heater_status = MANUAL_HEATER_ON;
                break;

            case 2:
                heater_status = MANUAL_HEATER_HIGH;
                break;

            default:
                req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
                return;
        }
        
        heater_status |= fan_status;
        if(ManualCtrlHeater(heater_status) < 0)
            break;
        
        req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=%d\n", argm->name, heater_status );
        return;
    } while(0);
    
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
#else
    req->buffer_end += sprintf( req_bufptr( req ), OPTION_NS "%s\n", argm->name );
#endif
}

void get_support_QoS( request *req, COMMAND_ARGUMENT *argm )
{
#if defined( SUPPORT_QOS_SCHEDULE )
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=0\n", argm->name );
#endif
}

void get_support_CoS( request *req, COMMAND_ARGUMENT *argm )
{
#if defined( SUPPORT_COS_SCHEDULE )
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=0\n", argm->name );
#endif
}

void get_support_BWC( request *req, COMMAND_ARGUMENT *argm )
{
#if defined( SUPPORT_QOS_SCHEDULE ) || defined( SUPPORT_COS_SCHEDULE )
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=0\n", argm->name );
#else
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s=1\n", argm->name );
#endif
}

#if defined( SUPPORT_COS_SCHEDULE )
void set_CoS_enable( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[COS_ENABLE].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_COS_ENABLE,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_VLan_ID( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[COS_VLANID].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_COS_VLANID,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_CoS_prio_video( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[COS_PRIO_VIDEO].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_COS_PRIO_VIDEO,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_CoS_prio_audio( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[COS_PRIO_AUDIO].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_COS_PRIO_AUDIO,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_CoS_prio_event( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[COS_PRIO_EVENT].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_COS_PRIO_EVENT,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_CoS_prio_manage( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[COS_PRIO_MANAGE].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_COS_PRIO_MANAGE,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}
#endif /* defined( SUPPORT_COS_SCHEDULE ) */
#if defined( SUPPORT_QOS_SCHEDULE )
void set_QoS_enable( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[QOS_ENABLE].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_QOS_ENABLE,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_QoS_DSCP_video( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[QOS_DSCP_VIDEO].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_QOS_DSCP_VIDEO,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_QoS_DSCP_audio( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[QOS_DSCP_AUDIO].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_QOS_DSCP_AUDIO,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_QoS_DSCP_event( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[QOS_DSCP_EVENT].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_QOS_DSCP_EVENT,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}

void set_QoS_DSCP_manage( request *req, COMMAND_ARGUMENT *argm )
{
	int temp;
	char *last = NULL;

	do
	{
		if ( ! argm->value[0] )
			break;
		temp = strtol( argm->value, &last, 10 );
		if ( ( ! last ) || ( *last ) )
			break;
		if ( ( sys_info->ipcam[QOS_DSCP_MANAGE].config.u32 != temp ) &&
		     ( ControlSystemData( SFIELD_SET_QOS_DSCP_MANAGE,(void *) &temp, sizeof( temp ), &req->curr_user ) < 0 ) )
			break;
		req->buffer_end += sprintf( req_bufptr( req ), OPTION_OK "%s\n", argm->name );
		return;
	}
	while ( 0 );
	req->buffer_end += sprintf( req_bufptr( req ), OPTION_NG "%s\n", argm->name );
}
#endif /* defined( SUPPORT_QOS_SCHEDULE ) */

/***************************************************************************
 *                                                                         *
 ***************************************************************************/ 
#define HASH_TABLE_SIZE	(sizeof(HttpOptionTable)/sizeof(HTTP_OPTION))
HTTP_OPTION HttpOptionTable [] =
{
	//MISC
	{ "debug"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "delay"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "videocodec"			, set_vidcodec				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },  //restart program
	{ "videocodecdebug"			, set_videocodecdebug				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },  //restart program
	{ "videocodecready"			, get_vidcodec				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "cameratitle"			, set_cameratitle			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "gettitle"			, get_title				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ccddata"			, ccddatawrite			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ccderror"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ccdx"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ccdy"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "checkpartner"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "videomode"			, videomode				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "antiflicker"			, antiflicker			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "flickless"			, flickless				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "powerline"			, powerline				, AUTHORITY_VIEWER, FALSE,  API_VIDEO, NULL },
	{ "getflickless"			, getflickless				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	//{ "attfileformat"	     , set_attfileformat		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "engineermode"			, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "factorydefault"			, set_factory_default		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "supportfactorymode"			, get_supportfactorymode		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getarmhz"		, get_armhz			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaxlhz"		, get_armhz			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getarmioport"		, get_arm_ioport			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdsphz"		    , undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocalsdram"		    , get_localsdram	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocalsram"		    , get_localsram	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "paratest"            , paratest                  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ipncptz"          , set_ipncptz                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ptzpopup"          , init_ipncptz                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  /// {"/ptz.htm"                 ,NULL			,AUTHORITY_OPERATOR ,URI_FLAG_NEED_PARSE   ,NULL },
	{ "getdip1switch"	    	, undefined			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdip2switch"	    	, undefined			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "language"		    , language	            	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getblockdata"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getccddata"		    , ccddataread	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcompiletime"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "gethelpurl"		    , get_helpurl	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlayoutnum"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlayoutstr"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlayouturl"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocaldecoder"		    , get_localdecoder	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocalflash"		    , get_localflash	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmaxchannel"		    , get_maxchannel	            	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getmaxfqdnlen"		    , get_maxfqdnlen	            	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getmaxnamelen"		    , get_maxnamelen	            	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getmaxpwdlen"		    , get_maxpwdlen	            	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getmemoryleft"		    , get_memoryleft	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getminnamelen"		    , get_minnamelen	            	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getnetmedia"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setparameter"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getparameter"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getportmancount"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getregdomain"		    , get_regdomain	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getregister"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getregisterleft"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
//	{ "verify"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "verify"			, call_verify_func				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setwriteatonce"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setsdramclk"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setlayouturl"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setlayoutstr"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setlayoutnum"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "sethostname"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setgioout0data"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsdramhz"			, get_sdramhz				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setsdrate"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsdrate"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getserialnumber"			, get_serialnumber				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsramdepth"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getstack"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsystemdata"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ifrmonly"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "keepalive"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#ifdef CONFIG_BRAND_DLINK
	{ "mui"			, set_mui				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
#else
	{ "mui"			, set_mui				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
	{ "nnewsduration"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "selftest"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "senseup"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setallreboot"			, restart_ipcam				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setarmclk"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setarmioport"			, set_arm_ioport				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setdspclk"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "getloop"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "writesetupdata"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ipcamrestartcmd"		, restart_ipcam				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "savingmode"			, power_saving_mode	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "analogoutput"			, analog_output	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//version info
//	{ "setbiosmode"			, set_biosmode				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsetupsize"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsetupversion"			, get_setupversion				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsoftwareversion"			, get_softwareversion				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getfirmwareversion"			, get_firmwareversion				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmachinecode"		    , get_machinecode	            	, AUTHORITY_NONE, FALSE,  API_NONE, NULL },
	{ "submachinecode"		    , sub_machinecode	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "biosmodelname"		    , bios_modelname	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmachsubcode"		    , get_machsubcode	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "machsubcode"		    , set_machsubcode	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "getcpuversion"		    , get_cpuversion	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getbiosversion"		    , get_biosversion	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocalcpu"		    , get_localcpu	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmachsubcode"			, set_machsubcode				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "channelplan"		    , channel_plan	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getbrandid"			, get_brandid						, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//Wireless
	{ "setwladhoc"			, set_wladhoc				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getwladhoc"			, get_wladhoc				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setwlchannel"			, set_wlchannel				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getwlchannel"			, get_wlchannel				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setwlencryption"			, set_wlencryption				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getwlencryption"			, get_wlencryption				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setwlessid"			, set_wlessid				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getwlessid"			, get_wlessid				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getwlstatus"			, get_wlstatus				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setwlwepkey"			, set_wlwepkey				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getwlwepkey"			, get_wlwepkey				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setwlwepwhich"			, set_wlwepwhich				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getwlwepwhich"			, get_wlwepwhich				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "supportwireless"			, get_supportwireless				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	
	//Message center
	{ "getmcenterenable"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmcenterfqdn"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmcenterfqdn"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmcenterpassword"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmcenterpassword"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmcenterport"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmcenterport"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmcenterenable"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmcenteraccount"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "servicemcenter"			, undefined				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	
	//NIC
	{ "getetherstatus"		        , get_etherstatus			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getnicerror"		    , get_nicerror	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getnicflow"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getnicin"		    , get_nicin	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getnicout"		    , get_nicout	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocalnic"		    , get_localnic	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//eth
	{ "setmaxmtu"			, set_maxmtu				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },  //unmodified
	{ "getmaxmtu"		    , get_maxmtu	       	   	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmac"				, set_mac					, AUTHORITY_OPERATOR, FALSE,  API_SET_MAC, NULL },
	{ "getmac0"		    	, get_mac0	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmac1"		    	, undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "bootmessage"			, boot_message			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "tvoutput"			, tv_output				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//CF card
	{ "cfformat"			, cfformat			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfcount"		    	, get_cfcount			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setcfcount"		    	, set_cfcount			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setcfduration"		, set_cfduration		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfduration"		, get_cfduration		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcferror"			, get_cferror			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfleft"			, get_cfleft			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setcfrate"			, set_cfrate			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfrate"			, get_cfrate			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setcfrecordtype"		, set_cfrecordtype		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfrecordtype"		, get_cfrecordtype		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setcfrenable"		, set_cfrenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfrenable"		, get_cfrenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfsize"			, get_cfsize			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfstatus"			, get_cfstatus			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfused"			, get_cfused			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setcfaenable"		, set_cfaenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getcfaenable"		, get_cfaenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "supportcfcard"		, get_supportcfcard		, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	
	// Audio
	{ "audiotype"			, set_audiotype			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "audioinenable"			, set_audioinenable			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	/*UI NO OPTION*/{ "audioinvolume"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "audiooutenable"			, set_audiooutenable			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "audiooutvolume"			, set_audiooutvolume			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "audioinsourceswitch"		, set_audioinsourceswitch		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "audiotest"		        , audiotest		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "audioamplifyratio"		        , audioamplifyratio		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	//{ "rs485test"		        , rs485test		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	{ "getmaxaudiopointer"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudioinmax"		     , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "audioerror"		     , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "audiostream"		     , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudioenable"       , get_audioenable          , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "audioenable"       , set_audioinenable          , AUTHORITY_VIEWER, FALSE,  API_VIDEO, NULL },
	/*UI NO OPTION*/{ "getaudioinvolume"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudiomax"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudiomode"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudionetmax"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudiooutmax"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudiooutvolume"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudiopointer"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudiotype"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaudiobitrate"		, undefined	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocalaic"		    , get_localaic	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//syslog
	{ "geteventlist"		        , undefined			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "geteventpointer"		        , undefined			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "eventcount"				, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "clearalllog"				, clearalllog					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	
	//uart
	{ "setuartspeed"		, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getuartspeed"		, get_uartspeed				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setuarttype"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getuarttype"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getuniversal"		, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocaluart"		   , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//image
	//{ "aviprealarm"		    , set_aviprealarm			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "avipostalarm"        , set_avipostalarm         	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	//{ "aviduration"		    , set_aviduration			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "aviformat"		    , set_aviformat				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getliveresolution"	, get_liveresolution	    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlivequality"		, get_livequality	    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "quality"        		, set_quality          		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getformat"        	, get_format           		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setjpegdelay"		, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "jpegstatus"			, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getjpegcount"		, undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getjpegmax"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getjpegsn"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getmpeg4xsize"       , get_mpeg4xsize         	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getmpeg4ysize"       , get_mpeg4ysize          	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "mpeg4audiorate"      , undefined          		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "mpeg4cenable"        , undefined          		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "mpeg4cvalue"        	, undefined          		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "mpeg4ipratio"        , undefined          		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "mpeg4quality"        , undefined          		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "avccenable"       	, undefined          		, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
 	{ "avccvalue"       	, undefined          		, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  	{ "avcframerate"       	, undefined          		, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  	{ "avcquality"       	, undefined          		, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  	{ "avcresolution"       , undefined          		, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "exposure"			, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setfps"				, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getvideomode"		, get_videomode				, AUTHORITY_VIEWER	, FALSE,  API_NONE, NULL },
	{ "imageformat"			, set_imageformat			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "resolution"			, set_resolution			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "mpeg4cenable1"		, set_mpeg4cenable1			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "mpeg4cenable2"		, set_mpeg4cenable2			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "quality1mpeg4"		, set_quality1mpeg4			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "quality2mpeg4"		, set_quality2mpeg4			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "mpeg4resolution"		, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "mpeg42resolution"	, set_mpeg42resolution		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ratecontrol"			, set_ratecontrol			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "jpegframerate"		, set_jpegframerate			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "jpegframerate2"		, set_jpegframerate2		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "mpeg4framerate"		, set_mpeg41framerate		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "mpeg42framerate"		, set_mpeg42framerate		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "videocodecmode"		, set_videocodecmode		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "videocodecres"		, set_videocodecres			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
//	{ "setimagesource"		, set_imagesource				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
//	{ "getimagesource"		, get_imagesource				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "img2a"				, set_img2a					, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "mpeg4cvalue"			, set_mpeg41bitrate			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "mpeg42cvalue"		, set_mpeg42bitrate			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "imagedefault"		, set_imagedefault			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "colorkiller"				, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	{ "chipcheck"			, get_chipcheck				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	//{ "fluorescent"			, set_fluorescent			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getfluorescent"		, get_fluorescent			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "mirror"				, set_mirror				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "flip"				, set_flip			        , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
//#if defined( SENSOR_USE_TC922 ) || defined( SENSOR_USE_IMX035 )
	{ "gamma"				, set_gamma 				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
//#endif 
#if defined PLAT_DM355
	{ "micgain" 			, set_micgain				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#elif defined PLAT_DM365
	{ "micgain" 			, set_externalmicgain				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
#else
#error Unknown product model
#endif
	{ "muteenable"			, set_muteenable			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "lineingain"			, set_lineingain			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "externalmicgain"		, set_externalmicgain			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "aeshutter"			, set_aeshutter 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "aes" 				, set_aes					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "awb" 				, set_white_balance 		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "isupdatecal"		, set_isupdatecal 		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "rgain"				, set_rgain 				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "bgain"				, set_bgain 				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "agc" 				, set_aegain				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "agcctrl" 			, set_agcctrl				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },

	{ "setvideoalc" 		, set_videoalc				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getmirror"			, get_mirror				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "kelvin"				, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getkelvin"			, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "maxagcgain"			, set_maxagcgain			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "hspeedshutter"		, set_hspeedshutter 		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "tvcalbe" 			, set_tvcable				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "binning" 			, set_binning				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
//	{ "setblc"				, set_blc						, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getblc"				, get_blc					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "blcmode" 			, set_blcmode				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "setbacklight"		, undefined 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getbacklight"		, undefined 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "brightness"			, set_brightness			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getbrightness"		, get_brightness			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
//#if defined SENSOR_USE_IMX036 || defined SENSOR_USE_IMX076
	{ "denoise"				, set_denoise				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getdenoise"			, get_denoise				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "evcomp"				, set_evcomp				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getevcomp"			, get_evcomp				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "dremode"				, set_dremode				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
//#endif	
	{ "contrast"			, set_contrast				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getcontrast" 		, get_contrast				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "h3aaeexposure"		, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "h3aaefunc"			, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "h3aaffunc"			, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "h3aawbfunc"			, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "gethue"				, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "hue" 				, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "saturation"			, set_saturation			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getsaturation"		, get_saturation			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "sharpness"			, set_sharpness 			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getsharpness"		, get_sharpness 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "wdrenable"			, set_wdrenable				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "wdrlevel"			, set_wdrlevel				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "exposuretime"		, set_exposuretime			, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "maxshutter"			, set_maxshutter		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "minshutter"			, set_minshutter		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
//	{ "exposureschedule"	, set_exposureschedule		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "meteringmethod"		, set_meteringmethod		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "setcolorbar" 		, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "maskarea"			, set_maskarea				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "privacymaskenable"	, set_maskareaenable		, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "stream1xsize"		, stream1xsize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "stream1ysize"		, stream1ysize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "stream2xsize"		, stream2xsize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "stream2ysize"		, stream2ysize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "stream3xsize"		, stream3xsize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "stream3ysize"		, stream3ysize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "stream4xsize"		, stream4xsize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "stream4ysize"		, stream4ysize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getjpegxsize"		, get_jpegxsize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getjpegyield"		, undefined				  	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getjpegysize"		, get_jpegysize				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "luminance"			, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getluminance"		, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getluminancenow" 	, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getexposure" 		, undefined 				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//support
	{ "supportonvif"		, get_supportonvif				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportflickless"	, get_supportflickless				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportpowerline"	, get_supportpowerline				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportagc"			, get_supportagc				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportaudio"		, get_supportaudio				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportavc"			, get_supportavc				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
//#ifndef SENSOR_USE_TVP5150	
	{ "supportawb"			, get_supportawb				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportbacklight"	, get_supportbacklight				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportblcmode"		, get_supportblcmode				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
//#endif
	{ "supportbrightness"	, get_supportbrightness				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
//#if defined SENSOR_USE_IMX036 || defined SENSOR_USE_IMX076	
	{ "supportdenoise"		, get_supportdenoise				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportevcomp"			, get_supportevcomp					, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportdre"				, get_supportdre						, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportcapturepriority"	, get_supportcapturepriority		, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
//#endif	
	{ "supportcolorkiller"	, get_supportcolorkiller				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportcontrast"		, get_supportcontrast				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportdncontrol"	, get_supportdncontrol				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportds1302"		, get_supportds1302				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportexposure"		, get_supportexposure				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportexposuretimemode" , get_supportexposuretimemode				, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },
	{ "supportfluorescent"	, get_supportfluorescent				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportfs"			, get_supportfs				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supporth3a"			, get_supporth3a				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supporthspeedshutter", get_supporthspeedshutter				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supporthue"			, get_supporthue				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportkelvin"		, get_supportkelvin				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportluminance"	, get_supportluminance				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportmaskarea"		, get_supportmaskarea				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportmaxagcgain"	, get_supportmaxagcgain				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportmirror"		, get_supportmirror				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportflip"			, get_supportflip 			, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportmotion"			, get_supportmotion				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportmotionmode"		, get_supportmotionmode 			, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },
	{ "supportmpeg4"			, get_supportmpeg4				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportmui"			, get_supportmui				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportprotect"			, get_supportprotect				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportsaturation"			, get_supportsaturation				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportsd"			, get_supportsd				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportsenseup"			, get_supportsenseup				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportsharpness"			, get_supportsharpness				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportst16550"			, get_supportst16550				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supporttstamp"			, get_supporttstamp				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supporttwowayaudio"			, get_supporttwowayaudio				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportusb"			, get_supportusb				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportwatchdog"			, get_supportwatchdog				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "servicehello"			, get_servicehello				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "servicenetbiosns"			, get_servicenetbiosns				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "servicennews"			, get_servicennews				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "servicescanport"			, get_servicescanport				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportaudioinswitch"		, get_supportaudioinswitch				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportaudioencselect"		, get_supportaudioencselect				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportwdrlevel"		, get_supportwdrlevel				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "machinetype"		, get_machinetype				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportantiflicker"		, get_supportantiflicker				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	
	// OSD
	{ "osdshow"			, osd_show				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "osdx"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "osdy"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setosd"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getinfolocation"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "infolocation"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getosdstatus"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	// SD card
	//{ "sdfileformat"        , set_sdfileformat          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "setsdaenable"        , set_sdaenable             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	//{ "getsdaenable"        , get_sdaenable             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },      
	{ "setsdcount"          , undefined               , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },            
	{ "getsdcount"          , undefined               , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },            
	//{ "setsdduration"       , set_sdduration            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },          
	//{ "getsdduration"       , get_sdduration            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },          
	{ "getsderror"          , undefined                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },            
	{ "getsdinsert"         , get_sdinsert              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },        
	//{ "setsdrecordtype"     , set_sdrecordtype          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },  
	//{ "getsdrecordtype"     , get_sdrecordtype          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },  
	//{ "setsdrenable"        , set_sdrenable             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },        
	//{ "getsdrenable"        , get_sdrenable             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },        
	{ "getsdsize"           , undefined                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },                
	//{ "sdformat"            , sdformat                  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },    
	//{ "cardrewrite"         , set_cardrewrite           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },        
	//{ "setsdrate"           , set_jpegcontfreq          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
//  { "sdunmount"       , sd_unmount                  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "sddel"          			, set_sddel                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },            
	{ "sdfpp"          			, set_sdfpp                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },            

  // Day Light Saving (DST)
  { "setdaylight"         , set_daylight              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getdaylight"         , get_daylight              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "daylightshift"       , set_dstshift              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "daylightmode"        , set_dstmode               , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "daylightstart"       , set_dststart              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "daylightend"         , set_dstend                , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	// Alarm & Schedule
	//{ "delschedule"         , del_schedule              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "digitalalarmin"      , undefined                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "digitalalarmreset"   , undefined                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "schedule"            , set_schedule              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getschedule"			, get_schedule              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "postalarm"		    , set_postalarm	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getpostalarm"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "prealarm"		    , set_prealarm	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprealarm"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getrecordstate"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "lostalarm"           , set_lostalarm 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	//{ "alarmduration"		, set_alarmduration			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getalarmstatus"		, get_alarm_status				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },

	
	//UPnP
  { "setupnpenable"       , set_upnpenable          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
//  { "upnpforwardingport"  , set_upnpforwardingport  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getupnpenable"       , get_upnpenable          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
//  { "setupnprefreshtime"       , set_upnprefreshtime          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "upnprefresh"       , set_upnprefresh          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#if 0
  { "setupnpssdpage"       , set_upnpssdpage          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getupnpssdpage"       , get_upnpssdpage          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setupnpport"       , set_upnpport          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getupnpport"       , get_upnpport          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getupnpssdpport"       , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setupnpcardread"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setupnpssdpport"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
  { "serviceupnpdevice"       , get_serviceupnpdevice          , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  
  //DHCP client
  { "dhcp"				, set_dhcpclient			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
   { "dhcpdipswitch"			, set_dhcpdipswitch			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "servicedhcpclient"	    , get_servicedhcpclient			, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getdhcpenable"	    , get_dhcpclient			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpcport"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpfail"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpoffer"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcprebinding"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcprefresh"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcprelease"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcprenewal"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsack"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setdhcpsdesignate"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsdesignate"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setdhcpsenable"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsenable"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setdhcpsleasetime"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsleasetime"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpslist"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setdhcpsmaxdomain"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsmaxdomain"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsnak"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsoccupy"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsoffer"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getdhcpsport"	    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "supportdhcpdipswitch"	, get_supportdhcpdipswitch 		, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },

  //PPPoE
  { "setpppoeenable"      , set_pppoeenable       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getpppoeenable"      , get_pppoeenable       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setpppoeaccount"     , set_pppoeaccount      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getpppoeaccount"     , get_pppoeaccount      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setpppoepwd"         , set_pppoepwd          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getpppoepwd"         , get_pppoepwd          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getispaccount"       , get_ispaccount        , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getisppassword"      , get_isppassword       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getispphone"         , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setpppoeidle"        , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getpppoeidle"        , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "servicepppoe"        , get_servicepppoe      , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  { "serviceppp"          , undefined             , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },

  //DDNS
  { "setddnsenable"       , set_ddnsenable        , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnsenable"       , get_ddnsenable        , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },  
  { "setddnstype"         , set_ddnstype          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnstype"         , get_ddnstype          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setddnshostname"     , set_ddnshostname      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnshostname"     , get_ddnshostname      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setddnsaccount"      , set_ddnsaccount       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnsaccount"      , get_ddnsaccount       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setddnspassword"     , set_ddnspassword      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnspassword"     , get_ddnspassword      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "ddnsinterval"        , set_ddnsinterval      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "ddnsstatus"          , get_ddnsstatus        , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "serviceddnsclient"   , get_serviceddnsclient , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },

#if 0 /* Disable by Alex, 2009.2.19  */
  { "setddnsfqdn"         , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnsfqdn"         , get_ddnsfqdn          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnsconnect"      , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "setddnsport"         , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnsport"         , get_ddnsport          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getddnssuccess"      , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
    
  //DNS
  { "servicednsclient"    , undefined             , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  { "getdnsip"            , get_dnsip             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getdns"              , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getdnsport"          , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getdnsrecord"        , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getdnsstatus"        , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getdnstableleft"     , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "getdnstablesize"     , undefined             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "secondarydnsip"      , dns2                  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  
  
  //IP filter
  { "ipfilterenable"           , set_ipfilterenable             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "ipfilterdefaultpolicy"    , set_ipfilterdefaultpolicy      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "ipfilterpolicyedit"       , set_ipfilterpolicyedit         , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "ipfilterpolicydel"        , set_ipfilterpolicydel          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "ipfilterpolicyexch"       , set_ipfilterpolicyexch         , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "serviceipfilter"          , get_serviceipfilter            , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  { "addipfilterpolicy"	       , add_ipfilterpolicy             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "delipfilterpolicy"        , del_ipfilterpolicy             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

  
  //UPnP client
#if 0	//disable by jiahung, 2010.03.22  
  { "setupnpcenable"          , set_upnpcenable       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "upnpforwardingport"          , set_upnpcextport       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
  { "getupnpportforwardingstatus"          , get_upnpportforwardingstatus       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  
#if 0	//mikewang
  //Virtual Server
  { "setvirtaulserver"          , set_virtaulserver       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "delvirtauldata"         , set_virtauldeldata       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "virtaulen"         , set_virtaulenable       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
  //Traffic control
  { "quotainbound"       , set_quotainbound          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "quotaoutbound"       , set_quotaoutbound          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  
  //time stamp
	{ "tstampenable"		, set_tstampenable	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "tstampformat"		, set_tstampformat	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#ifdef PLAT_DM355	
	{ "tstampcolor"			, set_tstampcolor	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#else
	{ "tstampcolor"			, undefined	     			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif	
	{ "tstamploc"			, set_tstamploc				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "tstamplabel"			, set_tstamplabel			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//GIO
	//{ "allgioinenable"		, set_allgioinenable		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "allgiooutenable"		, set_allgiooutenable		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setgioinenable"		, set_gioinenable	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setgiointype"		, set_giointype	        	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setgiooutenable"		, set_giooutenable		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setgioouttype"		, set_gioouttype	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "giooutalwayson"		, set_giooutalwayson	   	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "giooutreset"			, set_giooutreset	     	, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getgpiodir"			, get_gpio_dir		     	, AUTHORITY_ADMIN, FALSE,  API_NONE, NULL },
	{ "setgpiodir"			, set_gpio_dir		     	, AUTHORITY_ADMIN, FALSE,  API_NONE, NULL },
	{ "getgpiovalue"		, get_gpio_value	     	, AUTHORITY_ADMIN, FALSE,  API_NONE, NULL },
	{ "setgpiovalue"		, set_gpio_value	     	, AUTHORITY_ADMIN, FALSE,  API_NONE, NULL },
	{ "getgpiostate"		, get_gpio_state	     	, AUTHORITY_ADMIN, FALSE,  API_NONE, NULL },
	{ "setgpiostate"		, set_gpio_state	     	, AUTHORITY_ADMIN, FALSE,  API_NONE, NULL },

	{ "getgiointype"		, get_giointype 		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	/*
	{ "getgioinenable"		, get_gioinenable	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getgiooutenable"		, get_giooutenable		  	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getgioouttype"		, get_gioouttype	     	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getgiooutdata"		, undefined	                , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getgio1data"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "gioinput"			, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "giooutput"			, undefined					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	*/ // disbale by Alex, 2009.04.07
	
	
	//network
	{ "gethostname"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmaxsocket"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsocketconnected"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsocketfree"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsocketloadtimeout"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsockettimeout"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmaxarprecord"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getarp"		, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "internetip"			, set_netip					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "subnetmask"			, set_netmask				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "dnsip"				, set_dnsip					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "gateway"				, set_gateway				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "httpport"			, set_httpport				, AUTHORITY_OPERATOR, FALSE,  API_RESTART, NULL },
	{ "getnetip"		    , get_netip					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getnetmask"		    , get_netmask				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getgateway"		    , get_gateway				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "gethttpport"		    , get_httpport				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ledmode"				, ledmode     			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "rtspport"			, set_rtspport				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "httpstreamname1"		, http_stream_name_1		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "httpstreamname2"		, http_stream_name_2		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "httpstreamname3"		, http_stream_name_3		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "rtspaccessname1"		, rtsp_stream_name_1		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "rtspaccessname2"		, rtsp_stream_name_2		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "rtspaccessname3"		, rtsp_stream_name_3		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	

	//account info
	{ "getmaxregister"		    , get_maxregister	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "adduser"		        , add_user		        	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getuser"			, get_user				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getuserleft"			, get_userleft				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "deluser"		        , del_user	    	    	, AUTHORITY_ADMIN, FALSE,  API_NONE, NULL },
	{ "eventstart"	    	, event_start			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "whoareonline"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlogin"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getloginfail"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getloginsuccess"		    , undefined	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmaxaccount"		    , get_maxaccount	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "login"			, user_login				, AUTHORITY_NONE, FALSE,  API_NONE, NULL },
	{ "logout"			, user_logout				, AUTHORITY_NONE, FALSE,  API_NONE, NULL },
	{ "getauthority"		    , get_authority	            	, AUTHORITY_NONE, FALSE,  API_NONE, NULL },
	
	//SNTP
	{ "getdate"			    , get_date					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "gettime"			    , get_time					, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ntpfqdn"			, set_sntpfqdn				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ntpenable"			, ntp_enable				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "sntpfqdn"			, set_sntpfqdn				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "newdate"				, set_system_date			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "newtime"				, set_system_time			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "timezone"			, set_timezone				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "wintimezone"			, set_wintimezone			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "timefrequency"		, set_time_frequency		, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "timeformat"			, set_timeformat			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsntpip"		    , get_sntpip				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsntpfail"		    , undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsntpfrequency"		    , undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsntpnext"		    , undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "sntpport"		    , undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsntpport"		    , undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsntpsuccess"		    , undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsntptimezone"		    , get_sntptimezone				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "servicesntpclient"		    , get_servicesntpclient				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "setsntpupdate"		    , undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//RTC
	{ "getrtcstatus"	    	, get_rtcstatus				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getuptime"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rtcread"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rtcwrite"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getlocalrtc"		    , get_localrtc	            	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	//SMTP Client
	//{ "smtptest"			, smtptest				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "manualmail"			, manualmail				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "smtpfqdn"	    	, set_smtpfqdn				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getsmtpip"	    	, get_smtpip				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "smtpport"	    	, set_smtpport				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getsmtpport"	    	, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "smtpauth"		    , set_smtpauth				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "smtpsender"		    , set_smtpsender			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "smtpto"		        , set_smtpto				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "smtpuser"		    , set_smtpuser		    	, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "smtppwd"		        , set_smtppwd			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getemailuser"		        , get_emailuser			    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "asmtpattach"			, set_asmtpattach			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getasmtpattach"			, get_asmtpattach			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rsmtpenable"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getrsmtpenable"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rsmtpattach"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getrsmtpattach"			, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "asmtpenable"			, set_asmtpenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getasmtpenable"			, get_asmtpenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "servicesmtpclient"			, get_servicesmtpclient			, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	

	//FTP Client
	//{ "setftpjpegname"		 , set_ftpjpegname			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ftpmode"              , set_ftpmode			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ftpfileformat"		 , set_ftpfileformat			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "manualftp"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "manualstopftp"			, undefined				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ftpfqdn"	    	    , set_ftpfqdn				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ftpport"		        , set_ftpport				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ftpuser"		        , set_ftpuser				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ftppath"		        , set_ftppath   	        , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "ftppassword"		    , set_ftppassword			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ftprate"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getftpdtpport"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getftpgap"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getftpindexname"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getftpip"		    , get_ftpip			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getftppassword"		    , get_ftppassword			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getftppath"		    , get_ftppath			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getftppiport"		    , get_ftppiport			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setftpsenable"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getftpsenable"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setftpsport"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getftpsport"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getftpstate"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getftpuser"		    , get_ftpuser			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getftpvdelay"		    , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "rftpenable"			, set_rftpenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getrftpenable"			, get_rftpenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "aftpenable"          , set_aftpenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "getaftpenable"          , get_aftpenable			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getaftprate"          , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getalarmduration"          , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "aftprate"          , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rftprate"          , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getrftprate"          , undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "serviceftpclient"          , get_serviceftpclient			, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "serviceftpserver"          , get_serviceftpserver			, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
#ifdef SERVER_SSL
	// HTTPS
	{ "https_enable"            , https_enable				, AUTHORITY_OPERATOR, FALSE,  API_RESTART, NULL },
	{ "httpsport"            , https_port				, AUTHORITY_OPERATOR, FALSE,  API_RESTART, NULL },
	{ "clearsslkey"          , https_clearkey			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
	//two way audio
	{ "getspeaktoken"      , get_speak 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "giveupspeaktoken"   , giveupspeaktoken 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "speaktokentime"     , speaktokentime 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "wm8978test"     , wm8978test 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//audio debug
	{ "speakstreamosddebug"	   , speakstreamosddebug   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "speakstreamtest"	   , speakstreamtest   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	{ "rtspframeratedebug"	   , rtspframeratedebug   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//RS-232
	{ "rs232delay"           , undefined 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rs232output"           , undefined 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rs232read"           , undefined 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "rs232write"           , undefined 			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	//RS-485
  { "supportrs485"           , get_supportrs485        , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
  { "rs485delay"             , rs485_delay             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "rs485output"            , rs485_output            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "rs485write"             , rs485_write             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "rs485enable"            , set_485enable           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "rs485protocol"          , set_485protocol         , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "rs485data"              , set_485data             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "rs485stop"              , set_485stop             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "rs485parity"            , set_485parity           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "set485type"             , undefined               , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "set485id"               , set_485id               , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  { "set485speed"            , set_485speed            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
  
	//ptz
	{ "createpresetname"            , set_createpreset            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "deletepresetname"            , set_deletepreset            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "dv840output"          , dv840_output                 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ipncvisca"             , visca_protocol         , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	// Day/Night Control
	{ "dncontrolmode"          , dncontrol_mode          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "dncontrolsensitivity"   , undefined   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#if	SUPPORT_DNC_EX
	{ "dncontrolscheduleex"    , dncontrol_scheduleex    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#else
	{ "dncontrolschedule"    	 , undefined   	 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
	{ "dncontrold2n"           , dncontrol_d2n           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "dncontroln2d"           , dncontrol_n2d           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "focusadjust"           , undefined           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	{ "supportdcpower"         , get_supportdcpower        , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	/*Old*///{ "poweroutstatus"         , set_powerout            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "dcpowermode"         	, set_powerout            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "dcpowerschedule"          , powerschedule            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "supportirled"         	, get_supportirled        , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "irledmode"         		, set_powerout            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "irledschedule"          , powerschedule            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

// NO MORE USED, Alex. 2009.12.15
#if 0
	// Samba Client
	{ "ensmbrecord"            , smb_enrecord            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbauth"                , smb_authority           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbuser"                , smb_username            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbpassword"            , smb_password            , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbserver"              , smb_server              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbpath"                , smb_path                , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbstatus"              , smb_status              , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbrecprofile"          , smb_profile             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbrecrewrite"          , smb_rewrite             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "smbrectype"             , smb_rectype             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	{ "enmotionout" 	   , trg_outmotion   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ensignal1out"	   , trg_outgioin1   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ensignal2out"	   , trg_outgioin2   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	// SnapShot
	{ "enmotionsnapshot"       , trg_jpgmotion   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ensignal1snapshot"      , trg_jpggioin1   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ensignal2snapshot"      , trg_jpggioin2   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	// Recording
	{ "enmotionrec"		       , trg_avimotion   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ensignal1rec"	       , trg_avigioin1   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "ensignal2rec"	       , trg_avigioin2   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	{ "recresolution"		   , set_recresolution	 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "recrewrite"		       , ser_recrewrite	 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },

	{ "ensnapshot"		       , set_ensavejpg	 , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "enrecord"			   , set_enrecord    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "enkeeprecord"		   , set_avikeeprec  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "eneventrecord"		   , set_avieventrec , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "enschedulerecord"	   , set_avischedulerec    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	
	{ "minfreespace"           , set_freespace   , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif

	// For test event test server
	{ "replytestserver"	 , replytest	     , AUTHORITY_OPERATOR, FALSE, API_NONE ,NULL },

	{ "geteptzstatus"       , get_eptz_status		, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },
#ifdef CONFIG_BRAND_DLINK
	{ "getdlinkalarmstatus"    , get_dlink_alarm_status		, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },
	{ "getdlinksdstatus"       , get_dlink_sd_status		, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },
#else
	{ "getalarmstatus"    , get_dlink_alarm_status		, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },
	{ "getsdstatus"       , get_dlink_sd_status		, AUTHORITY_VIEWER, FALSE,	API_NONE, NULL },
#endif

	//ePtz
	{ "eptzcoordinate"			, set_eptzcoordinate				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "eptzwindowsize"			, set_eptzwindowsize				, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },

	//multi-profile
	{ "supportprofile1"			, get_supportstream1				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportprofile2"			, get_supportstream2				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportprofile3"			, get_supportstream3				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "supportprofile4"			, get_supportstream4				, AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
	{ "getmaxstreamcount"		, get_maxstreamcount				, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },	
	{ "getprofile1format"        , get_profile1format             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2format"        , get_profile2format             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3format"        , get_profile3format             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile4format"        , get_profile4format             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile1xsize"         , get_profile1xsize             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2xsize"         , get_profile2xsize             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3xsize"         , get_profile3xsize             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile1ysize"         , get_profile1ysize             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2ysize"         , get_profile2ysize             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3ysize"         , get_profile3ysize             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile1fps"          , get_profile1fps             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2fps"          , get_profile2fps             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3fps"          , get_profile3fps             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile1bps"          , get_profile1bps             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2bps"          , get_profile2bps             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3bps"          , get_profile3bps             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile4bps"          , get_profile4bps             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile1quality"      , get_profile1quality             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2quality"      , get_profile2quality             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3quality"      , get_profile3quality             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile4quality"      , get_profile4quality             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile1qualitymode"      , get_profile1qmode             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2qualitymode"      , get_profile2qmode             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3qualitymode"      , get_profile3qmode             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile4qualitymode"      , get_profile4qmode             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile1gov"      , get_profile1gov             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile2gov"      , get_profile2gov             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getprofile3gov"      , get_profile3gov             , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getnumprofile"      , get_numprofile             , AUTHORITY_VIEWER, FALSE,  API_NONE, NULL },
        
	// Profile 1
	{ "profile1format"         , set_prf1format  , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile1resolution"     , set_prf1res     , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile1resolutionname" , get_prf1resrange, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile1rate"           , set_prf1rate    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile1ratename"       , get_prf1raterange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	{ "profile1bps"            , set_prf1bps     , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile1bpsname"        , get_prf1bpsrange, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile1quality"        , set_prf1quality , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "profile1rtspurl"        , set_prf1rtsp    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile1qmode"          , set_prf1qmod    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "profile1customize"      , set_profile1customize    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile1keyframeinterval"            , set_profile1keyframeinterval , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile1govname"        , get_profile1govrange , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile1view"           , set_profile1view, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
        
	// Profile 2
	{ "profile2format"         , set_prf2format  , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile2resolution"     , set_prf2res     , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile2resolutionname" , get_prf2resrange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	{ "profile2rate"           , set_prf2rate    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile2ratename"       , get_prf2raterange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	{ "profile2bps"            , set_prf2bps     , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile2bpsname"        , get_prf2bpsrange, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile2quality"        , set_prf2quality , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "profile2rtspurl"        , set_prf2rtsp    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile2qmode"          , set_prf2qmod    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "profile2customize"      , set_profile2customize    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile2keyframeinterval"            , set_profile2keyframeinterval , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile2govname"        , get_profile2govrange , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile2view"           , set_profile2view, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },

	// Profile 3
	#ifdef CONFIG_BRAND_TYCO
	{ "profile3format"         , set_prf2format  , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile3resolution"     , set_prf2res     , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile3resolutionname" , get_prf2resrange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	#else
	{ "profile3format"         , set_prf3format  , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile3resolution"     , set_prf3res     , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile3resolutionname" , get_prf3resrange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	#endif
	{ "profile3rate"           , set_prf3rate    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile3ratename"       , get_prf3raterange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	{ "profile3bps"            , set_prf3bps     , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile3bpsname"        , get_prf3bpsrange, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile3quality"        , set_prf3quality , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "profile3rtspurl"        , set_prf3rtsp    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile3qmode"          , set_prf3qmod    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	//{ "profile3customize"      , set_profile3customize    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile3keyframeinterval"            , set_profile3keyframeinterval , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile3govname"        , get_profile3govrange , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile3view"           , set_profile3view, AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },

#if defined( MODEL_LC7317 ) || defined( MODEL_LC7337 ) || defined( MODEL_LC7357 ) || defined( MODEL_VS2311T ) || defined( MODEL_VS2311B ) || defined( PLAT_DM365 )
	// Profile 4
	{ "profile4format"         , set_prf4format  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile4resolution"     , set_prf4res     , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile4resolutionname" , get_prf4resrange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	{ "profile4rate"           , set_prf4rate    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile4ratename"       , get_prf4raterange, AUTHORITY_OPERATOR,FALSE,  API_NONE, NULL },
	{ "profile4bps"            , set_prf4bps     , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile4bpsname"        , get_prf4bpsrange, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile4quality"        , set_prf4quality , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "profile4qualityavc"     , set_prf4quality , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	//{ "profile4rtspurl"        , set_prf4rtsp    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
#endif
	{ "profile4keyframeinterval"            , set_profile4keyframeinterval , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profile4govname"        , get_profile4govrange , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "profile4qmode"          , set_prf4qmod    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	
	{ "supportprofilenumber"          , get_supportprofilenumber    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "supportaspectratio"          , get_supportaspectratio    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "profilenumber"          , set_profilenumber    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "aspectratio"          , set_aspectratio    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "videoprofiledefault"  , set_videodefault    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	
	//sensor crop
	{ "sensorcropenable"      , sensorcropenable    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "sensorcropresolution"  , sensorcropresolution    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "sensorstartx"          , sensorstartx    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "sensorstarty"          , sensorstarty    , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	
	/*
	{ "rs485read"              , rs485_read      , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "get485connect"          , get_485connect  , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "get485id"               , get_485id       , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "get485speed"            , get_485speed    , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "get485type"             , get_485type     , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	*/ /* Disable by Alex, 2008.12.25  */
	
	//motion
	{ "motionenable"        , set_motionenable          , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "getmotionenable"        , get_motionenable          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotion"        , get_motion          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmotioncnttable"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotioncnttable"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotionmax"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotionsensitivity"        , get_motionsensitivity          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "setmotionsentable"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotionsentable"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotionx1"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotionx2"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotionxlimit"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotionylimit"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotiony1"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "getmotiony2"        , undefined          , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "motionxy"		, undefined			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "motioncenable"       , undefined         , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "motionlevel"         , undefined           , AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "motioncvalue"        , set_motioncvalue          , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "motionblock"         , set_motionblock           , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "motionpercentage"    , set_motionpercentage      , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "motiondrawenable"    , motiondrawenable      , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	{ "motionregion"    , motionregion      , AUTHORITY_OPERATOR, FALSE,  API_VIDEO, NULL },
	/*
	{ "motionxblock"		, get_motionxblock			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	{ "motionyblock"		, get_motionyblock			, AUTHORITY_OPERATOR, FALSE,  API_NONE, NULL },
	*/ // disable by Alex, 2008.10.17

	{ "rtc_cal_value"     , set_rtc_cal_value         , AUTHORITY_ADMIN   , FALSE,  API_NONE, NULL },
	{ "rtc_cal_status"    , set_rtc_cal_status        , AUTHORITY_ADMIN   , FALSE,  API_NONE, NULL },
	{ "rtc_cal_faster"    , set_rtc_cal_faster        , AUTHORITY_ADMIN   , FALSE,  API_NONE, NULL },
	{ "rtc_cal_slower"    , set_rtc_cal_slower        , AUTHORITY_ADMIN   , FALSE,  API_NONE, NULL },

	// osd debug api
	{ "show_framerate"    , osd_show_framerate        , AUTHORITY_ADMIN   , FALSE,  API_NONE, NULL },

	// wizardinit for LANDAP
	{ "wizardinit"    , wizardinit        , AUTHORITY_ADMIN   , FALSE,  API_NONE, NULL },

	//{ "readconfig"	  		, test_config		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },

	{ "createeventserver"	, set_event_server		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "deleteeventserver"	, del_event_server		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "createeventmedia"	, set_event_media		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "deleteeventmedia"	, del_event_media		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "createevent"			, set_event_type		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "deleteevent"			, del_event_type		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "createeventrecording", set_event_record		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "deleteeventrecording", del_event_record		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "autoiris"			, set_auto_iris		  	  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	
	
	//AF
	{ "setafzoom"			, set_afzoom		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "setaffocus"			, set_affocus		  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
    { "getaftemperature"    , get_af_temp         , AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
    { "getafadcvalue"       , get_af_adc          , AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
    { "getautofocusbusy"    , get_autofocusbusy   , AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
	{ "lensspeed"			, lens_speed		  , AUTHORITY_OPERATOR	 , FALSE,	API_NONE, NULL },
	{ "getmcuwdt"			, get_mcu_wdt		  , AUTHORITY_OPERATOR	 , FALSE,	API_NONE, NULL },
	{ "setmcuwdt"			, set_mcu_wdt		  , AUTHORITY_OPERATOR	 , FALSE,	API_NONE, NULL },
	{ "disablemcuwdt"		, disablemcuwdt		  , AUTHORITY_OPERATOR	 , FALSE,	API_NONE, NULL },
	{ "af_cnt"				, af_cnt			  , AUTHORITY_VIEWER	 , FALSE,   API_NONE, NULL },
	{ "af_test"				, af_test			  , AUTHORITY_VIEWER	 , FALSE,   API_NONE, NULL },

	//ePTZ
	{ "eptzspeed"    		, eptz_speed   		, AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
	{ "eptzpspeed"    		, eptz_pspeed   	, AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
	{ "eptztspeed"    		, eptz_tspeed   	, AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
	{ "eptzzspeed"    		, eptz_zspeed   	, AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
	{ "eptzadvspeed"    	, eptz_advspeed   	, AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
	
    // Manual Control Periphery
    { "manualctrlfan"       , manual_ctrl_fan     , AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },
    { "manualctrlheater"    , manual_ctrl_heater  , AUTHORITY_OPERATOR   , FALSE,   API_NONE, NULL },

	//VISCA
#ifdef SUPPORT_VISCA
	{ "setpresetpage"		, set_vspresetpage	  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "getpresetpage"		, get_vspresetpage	  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "setsequencepage"		, set_vssequencepage  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "getsequencepage"		, get_vssequencepage  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "setvspanspeed"		, set_vspanspeed  	  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
	{ "setvstiltspeed"		, set_vstiltspeed  	  , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },

	{ "getburnstatus"		, get_vsburnstatus    , AUTHORITY_NONE   	 , FALSE,	API_NONE, NULL },
	{ "viscagetdate"		, vsburntest_getdate  , AUTHORITY_NONE		 , FALSE,  	API_NONE, NULL },
	{ "viscagettime"		, vsburntest_gettime  , AUTHORITY_NONE		 , FALSE,  	API_NONE, NULL },
	{ "viscawintimezone"	, set_vswintimezone	  , AUTHORITY_NONE		 , FALSE,   API_NONE, NULL },
	{ "viscanewdate"		, set_vssystem_date	  , AUTHORITY_NONE		 , FALSE,   API_NONE, NULL },
	{ "viscanewtime"		, set_vssystem_time	  , AUTHORITY_NONE		 , FALSE,   API_NONE, NULL },
	{ "viscasetallreboot"	, restart_visca_ipcam , AUTHORITY_NONE		 , FALSE,   API_NONE, NULL },
	{ "viscafactorydefault"	, set_visca_factory_default , AUTHORITY_NONE , FALSE,   API_NONE, NULL },
	{ "viscagiooutalwayson"	, set_vs_giooutalwayson, AUTHORITY_NONE	 	 , FALSE,   API_NONE, NULL },
	{ "viscagetdlinkalarmstatus" , get_vs_dlink_alarm_status		, AUTHORITY_NONE, FALSE,	API_NONE, NULL },
	{ "viscasetmac"			, set_vs_mac		  , AUTHORITY_NONE	 	 , FALSE,   API_NONE, NULL },
#endif
	{ "supportqos"          , get_support_QoS             , AUTHORITY_NONE           , FALSE, API_NONE      , NULL },
	{ "supportcos"          , get_support_CoS             , AUTHORITY_NONE           , FALSE, API_NONE      , NULL },
	{ "supportbwc"          , get_support_BWC             , AUTHORITY_NONE           , FALSE, API_NONE      , NULL },
#if defined( SUPPORT_COS_SCHEDULE )
	{ "cosenable"           , set_CoS_enable              , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "cosvlanid"           , set_VLan_ID                 , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "cospriovideo"        , set_CoS_prio_video          , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "cosprioaudio"        , set_CoS_prio_audio          , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "cosprioevent"        , set_CoS_prio_event          , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "cospriomanage"       , set_CoS_prio_manage         , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
#endif
#if defined( SUPPORT_QOS_SCHEDULE )
	{ "qosenable"           , set_QoS_enable              , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "qosdscpvideo"        , set_QoS_DSCP_video          , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "qosdscpaudio"        , set_QoS_DSCP_audio          , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "qosdscpevent"        , set_QoS_DSCP_event          , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
	{ "qosdscpmanage"       , set_QoS_DSCP_manage         , AUTHORITY_OPERATOR       , FALSE, API_NONE      , NULL },
#endif
	{ "logtvoutdebug"		, set_log_tvout_debug , AUTHORITY_OPERATOR   , FALSE,	API_NONE, NULL },
#ifdef APIDEBUG
    { "getapihistory"       , getapihistory       , AUTHORITY_VIEWER     , FALSE,   API_NONE, NULL }
#endif

};



void process_bottom_half(request *req)
{
	if (api.bottom.ntpd_restart) {
		exec_ntpd(conf_string_read( NTP_SERVER));
		//exec_ntpd(sys_info->config.lan_config.net.ntp_server);
		api.bottom.ntpd_restart = 0;
	}
}

unsigned int hash_cal_value(char *name)
{
	unsigned int value = 0;

	while (*name)
		value = value * 37 + (unsigned int)(*name++);
	return value;
}


void hash_insert_entry(CMD_HASH_TABLE *table, HTTP_OPTION *op)
{
	if (table->entry) {
		op->next = table->entry;
	}
	table->entry = op;
}

CMD_HASH_TABLE *cmd_hash;
int hash_table_init(void)
{
	int i;

	if ( (cmd_hash = (CMD_HASH_TABLE *)calloc(sizeof(CMD_HASH_TABLE), MAX_CMD_HASH_SIZE)) == NULL) {
		return -1;
	}
	for (i=0; i<HASH_TABLE_SIZE; i++) {
		hash_insert_entry(cmd_hash+(hash_cal_value(HttpOptionTable[i].name)%MAX_CMD_HASH_SIZE), HttpOptionTable+i);
	}
	fprintf(stderr, "HASH_TABLE_SIZE = %d\n", HASH_TABLE_SIZE);
	return 0;
}

HTTP_OPTION *http_option_search(char *arg)
{
	HTTP_OPTION *opt;

	opt = cmd_hash[hash_cal_value(arg)%MAX_CMD_HASH_SIZE].entry;

	while (opt) {
		if ( strcmp(opt->name, arg) == 0 )
			return opt;
		opt = opt->next;
	}
	return NULL;
}

void http_run_command(request *req, COMMAND_ARGUMENT *arg, int num)
{
	HTTP_OPTION *option;
	char terminate[1] = {0x00};
	int  i;

#ifdef APIDEBUG
	char api_command[600] = "";
	char buffer_command[600*49] = "";
	char next_command[30] = "";
	int  count = 0;
	int	 ApiFd = -1,TestRFd = -1,TestWFd = -1;
	struct stat api_stat;
	time_t timep;
  	struct tm *p;
#endif
	send_request_ok_api(req);     /* All's well */
//	dbg("argument count = %d\n", num);
	for (i=0; i<num; i++) {
		strtolower(arg[i].name);  // convert the command argument to lowcase
#if 0//def LOCAL_DEBUG
		fprintf(stderr, "arg[%d].name=%s", i, arg[i].name);
		if (arg[i].value[0])
			fprintf(stderr, ", value=%s\n", arg[i].value);
		else
			fprintf(stderr, "\n");
#endif
#ifdef APIDEBUG
		if(num <= 1){
			if (arg[i].value[0])
				sprintf(api_command,"vb.htm?%s=%s", arg[i].name, arg[i].value);
			else
				sprintf(api_command,"vb.htm?%s", arg[i].name);
		}
		else{
			if(i <= 0){
				if (arg[i].value[0])
					sprintf(api_command,"vb.htm?%s=%s", arg[i].name, arg[i].value);
				else
					sprintf(api_command,"vb.htm?%s", arg[i].name);
			}	
			else{
				if (arg[i].value[0])
					sprintf(next_command,"&%s=%s", arg[i].name, arg[i].value);
				else
					sprintf(next_command,"&%s", arg[i].name);
				strcat(api_command,next_command);
			}
			//fprintf(stderr,"api_command = %s %d\n",api_command,__LINE__);				
		}
#endif
		option = http_option_search(arg[i].name);
//		dbg("Api authority = %d, user authority = %d\n", option->authority, req->authority);
		if (option) {
			if (req->authority <= option->authority) {
				if ((option->flag&API_VIDEO) && (sys_info->procs_status.avserver==AV_SERVER_RESTARTING) && arg[i].value[0]) {
					req_write(req, OPTION_NG);
					req_write(req, arg[i].name);
				}
				else {
					(*option->handler) (req, &arg[i]);
//				if (option->flag & API_RESTART)
//					req->server_restart = 1;
				}
			}
			else {
				req_write(req, OPTION_UA);
				req_write(req, arg[i].name);
				dbg("http_run_command(%s): Permission denied!!!\n", arg[i].name);
			}
		}
		else {
			req_write(req, OPTION_UW);
			req_write(req, arg[i].name);
			dbg("Unknown command : %s\n", arg[i].name);
		}
	}
#ifdef APIDEBUG
	if( (strcmp(api_command,"vb.htm?language=ie&getalarmstatus&getsdstatus") != 0) && (strcmp(api_command,"vb.htm?language=ie&videocodecready") != 0) )
	{
		ApiFd = open(APILOG_FILE, O_RDWR | O_APPEND | O_NONBLOCK);
		if(ApiFd == -1){
    		ApiFd = open(APILOG_FILE, O_APPEND | O_CREAT | O_NONBLOCK | O_RDWR, 0644);
    		if(ApiFd == -1){
      			fprintf(stderr,"open api.log file failed! %d\n",__LINE__);
    		}
  		}
		//fprintf(stderr,"api_command = %s %d\n",api_command,__LINE__);
		if(fstat(ApiFd, &api_stat) == -1)
    		fprintf(stderr,"get api log file stat failed! %d\n",__LINE__);
    	count = api_stat.st_size / sizeof(api_command);
		//fprintf(stderr,"count = %d %d\n",count,__LINE__);
		if(count == 50){
			TestRFd = open(APILOG_FILE, O_RDONLY | O_NONBLOCK);
			TestWFd = open(APILOG_N_FILE, O_RDWR | O_APPEND | O_NONBLOCK);
			if(TestWFd == -1){
    			TestWFd = open(APILOG_N_FILE, O_APPEND | O_CREAT | O_NONBLOCK | O_RDWR, 0644);
    			if(TestWFd == -1){
      				fprintf(stderr,"open api_n.log file failed! %d\n",__LINE__);
    			}
  			}
			lseek(TestRFd, sizeof(api_command), SEEK_SET);
			read(TestRFd, buffer_command, sizeof(api_command)*(50-1));
			write(TestWFd, buffer_command, sizeof(api_command)*(50-1));

			close(TestRFd);
			close(TestWFd);
			close(ApiFd);
			if( remove(APILOG_FILE) != 0)
        		fprintf(stderr,"remove %s failed\n", APILOG_FILE);
			if(rename(APILOG_N_FILE, APILOG_FILE) != 0)
				fprintf(stderr,"rename %s to %s failed\n", APILOG_N_FILE, APILOG_FILE);
			ApiFd = open(APILOG_FILE, O_RDWR | O_APPEND | O_NONBLOCK);
			lseek(ApiFd, sizeof(api_command)*(50-1), SEEK_SET);		
		}
		time(&timep);
      	p = localtime(&timep);
		sprintf( api_command, "%s&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp%4d-%02d-%02d %02d:%02d:%02d", api_command, p->tm_year+1900, p->tm_mon+1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
		//fprintf(stderr, "api_command = %s %d\n",api_command,__LINE__);
		write(ApiFd, api_command, sizeof(api_command));
		close(ApiFd);
	}
#endif
	if (api.bottom_test)
		process_bottom_half(req);

	// terminated with 0x00, for compatible with old camera
	req_copy(req, terminate, sizeof(terminate));
}

extern int ShowAllWebValue(request *req, char *data, int max_size);
extern int ShowAllPara(request *req, char *data, int max_size);
int html_ini_cmd(request *req, char *addr, int max_size)
{
	int ret;

	ret = ShowAllWebValue(req, addr, max_size);
	return ret;
}
void http_run_dlink_command(request *req, COMMAND_ARGUMENT *arg, int num)
{
	AUTHORITY authority = req->authority;
	HTTP_OPTION *option;
	int i;

	//send_request_ok_api(req);     /* All's well */
//	dbg("argument count = %d\n", num);
	for (i=0; i<num; i++) {
		strtolower(arg[i].name);  // convert the command argument to lowcase
#ifdef xxx //DEBUG
		dbg("arg[%d].name=%s", i, arg[i].name);
		if (arg[i].value[0])
			dbg(", value=%s\n", arg[i].value);
		else
			dbg("\n");
#endif
		option = http_option_search(arg[i].name);
		if (option) {
			if (authority <= option->authority)
				(*option->handler) (req, &arg[i]);
			else {
				req_write(req, OPTION_UA);
				req_write(req, arg[i].name);
				req_write(req, "\n");
				dbg("http_run_command: Permission denied!!!\n");
			}
		}
		else {
			req_write(req, OPTION_UW);
			req_write(req, arg[i].name);
			req_write(req, "\n");
			dbg("Unknown command : %s\n", arg[i].name);
	}
	}

}
int html_keyword_cmd(request *req, char *addr, int max_size)
{
	int ret;

	ret = ShowAllPara(req, addr, max_size);
	return ret;
}

int html_cmdlist_cmd(request *req, char *addr, int max_size)
{
	int ret = 0,i,count = 0,size;
	HTTP_OPTION *option;
	char buf[80];
	for (i=0; i<HASH_TABLE_SIZE; i++) {
		option = &HttpOptionTable[i];
		if (req->authority > option->authority)
			continue;
		size = sprintf(buf, "%03d.%-25s%d\n", ++count, option->name, option->authority);
		if(ret + size + 1 > max_size){
			ret = sprintf(addr, "Not enogh size to show");
			break;
		}
		ret += sprintf(addr + ret, "%s", buf);
	}
	return ret;
}

// Disable by Alex, 2010.04.12.  NO MORE USED.
#if 0
int http_sdget_cmd(request *req, COMMAND_ARGUMENT *argm, char *addr, int max_size, char *sdpath)
{
	int ret = 0;
	struct tm s_time;
	time_t    start_time = 0;

	//fprintf(stderr,"name : %s \n", argm->name);
	//fprintf(stderr,"value : %s \n", argm->value);

	if (argm->value != "\0")
	{
		if( CheckFileExist(SD_PATH_IPNC,argm->value) == 0 )
		{
			strcpy(sdpath, SD_PATH_IPNC);
			return 0;
		}
	}
/*
	if( MEM_List_files_To_html( SD_PATH_IPNC, MMC_NODE, addr, max_size) > 0 )
	{
		ret = strlen(addr);
	}
*/
	if(req->cmd_count) {

		if(strcmp(argm->name, "sdlisttime"))
			return 0;
		
		if(sscanf(argm->value, "%4d%2d%2d%2d%2d%2d", &s_time.tm_year, &s_time.tm_mon, &s_time.tm_mday,
													&s_time.tm_hour, &s_time.tm_min, &s_time.tm_sec) != 6) {
			return 0;
		}

		s_time.tm_year -= 1900;
		s_time.tm_mon--;

		if( (start_time = mktime(&s_time)) == -1)
			return 0;
	}

	MEM_List_files_To_html( SD_PATH_IPNC, SD_PATH_IPNC, addr, max_size, start_time);
	ret = strlen(addr);

	return ret;
}

void http_sddel_cmd(request *req, COMMAND_ARGUMENT *argm)
{
	char http_ip_addr[100];

	GetIP_Addr(http_ip_addr);

	send_request_ok_api(req);

//	dbg("http_sddel_cmd: parameter=%s, value=%s\n", argm->name, argm->value);

	if (strcmp(argm->name, "FILE") == 0)
	{
		if ( DeleteFile(SD_PATH_IPNC, argm->value) == 0 ) {
			req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s\n", argm->name);
		}
		else {
			req->buffer_end += sprintf(req_bufptr(req), OPTION_NG "%s\n", argm->name);
		}
		req->buffer_end += sprintf(req_bufptr(req), "<HR>HTTP Server at <A HREF=\"http://%s\">ipnc</A><BR></PRE></BODY></HTML>", http_ip_addr);
	}
	else {
		req_write(req, OPTION_UW);
		req_write(req, argm->name);
	}
}
#endif
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
void get_authority(request *req, COMMAND_ARGUMENT *argm)
{
	HTTP_OPTION *opt;
	opt = http_option_search(argm->value);
	req->buffer_end += sprintf(req_bufptr(req), OPTION_OK "%s=%d\n", argm->name , opt->authority);
	return;
}

