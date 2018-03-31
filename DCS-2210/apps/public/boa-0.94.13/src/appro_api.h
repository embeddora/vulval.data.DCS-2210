// command.h
//
// Last changed:   November 26, 2007
//

#ifndef __APPRO_API_H__
#define __APPRO_API_H__

#ifndef TRUE
#define TRUE		1
#define FALSE	0
#endif

#define HTTP_OPTION_CHANGED		0x0001
  
#define OPTION_OK		"OK "
#define OPTION_NG		"NG "
#define OPTION_NS		"NS "
#define OPTION_UW		"UW "
#define OPTION_UA		"UA "

#define MIN_HTTP_PORT		1024
#define MAX_HTTP_PORT		65535

#define MIN_HTTPS_PORT		1024
#define MAX_HTTPS_PORT		65535

// add by Alex, 2009.03.27
#define MIN_SMTP_PORT		1024
#define MAX_SMTP_PORT		65535

#define MIN_FTP_PORT		1024
#define MAX_FTP_PORT		65535


int hash_table_init(void);
void http_run_command(request *req, COMMAND_ARGUMENT *arg, int num);
int html_ini_cmd(request *req, char *addr, int max_size);
int html_keyword_cmd(request *req, char *addr, int max_size);
int html_cmdlist_cmd(request *req, char *addr, int max_size); 
int http_sdget_cmd(request *req, COMMAND_ARGUMENT *argm, char *addr, int max_size, char *sdpath);
void get_authority(request *req, COMMAND_ARGUMENT *argm);  //add by mikewang
void http_sddel_cmd(request *req, COMMAND_ARGUMENT *argm);
void http_run_dlink_command(request *req, COMMAND_ARGUMENT *arg, int num);
#endif /* __APPRO_API_H__ */

