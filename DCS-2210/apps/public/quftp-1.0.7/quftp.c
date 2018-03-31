/*
 *	Queued FTP Client
 *
 *  $Revision: 1.9 $
 *  $Date: 2003/03/11 01:26:39 $
 *  vim: sw=4 ts=4
 *
 */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <glob.h>
#include <sys/resource.h>	// for setpriority
#ifndef __GLIBC__
	#include <term.h>
#endif
#include "ftp.h"
#include "text.h"
#include "rc.h"
#include "quftp.h"

#include <arpa/inet.h>	// for inet_addr

#include <ApproDrvMsg.h>
#include <Appro_interface.h>
#include <cmem.h>
//#include <sysctrl.h>

#define LOCAL_DEBUG
#include <debug.h>
#include <sysconf.h>
#include <sysinfo.h>
#include <system_default.h>
#include <file_msg_drv.h>

/* DEBUG*/
#define DEV_TTYS0      "/dev/ttyS0"

/* Increase this for a steadier but less accurate ETA and Speed readout */
#define SPEEDS_AVERAGE		5

/* Maximum number of concurrent connections */
#define MAX_CONNECTIONS		3

/* Define a couple of very handy macros */
#define CURRENT_CONNECTION	connection[atoi(config(variables, "CURRENT_CONNECTION"))]
#define LAST_CONNECTION		connection[atoi(config(variables, "LAST_CONNECTION"))]

struct ftpconnection *connection[MAX_CONNECTIONS];
struct qitem *leftqueue = NULL;
struct variable_list *variables = NULL;
struct alias_list *aliases = NULL;

struct COMMAND command_list[] = {
	{"ls", cmd_ls, "List current directory", NULL},
	{"lls", cmd_lls, "List local directory", NULL},
	{"dir", cmd_ls, "Synonym for \"ls\"", NULL},
	{"cd", cmd_cd, "Change the current directory", NULL},
	{"lcd", cmd_lcd, "Change the current local directory", NULL},
	{"pwd", cmd_pwd, "Show current working directory", NULL},
	{"get", cmd_get, "Queue a file for download", NULL},
	{"put", cmd_put, "Queue a file for upload", NULL},
	{"fxp", cmd_fxp, "Queue a file to be transferred between connections", NULL},
	{"go", cmd_go, "Start transferring the queue", NULL},
	{"clear", cmd_clear, "Delete all items from the queue", NULL},
	{"queue", cmd_queue, "List all items on the queue", NULL},
	{"quit", cmd_quit, "Exit the program", NULL},
	{"close", cmd_close, "Disconnect from the server", NULL},
	{"open", cmd_open, "Connect to a server", NULL},
	{"status", cmd_status, "Show current status", NULL},
	{"stat", cmd_stat, "Display file statistics", NULL},
	{"nlst", cmd_nlst, "List filenames in current directory only", NULL},
	{"switch", NULL, "Switch to another connection", NULL},
	{"debug", NULL, "Set debugging level", "Specify the debug level from 1 to 5"},
	{"user", cmd_user, "Enter username and password for the server", "Syntax: user username [password]\nPassword is optional and should be separated from username by a space."},
	{"cat", cmd_cat, "Output a text file to stdout", NULL},
	{"more", cmd_more, "Page a text file", NULL},
	{"set", cmd_set, "Set a variable", NULL},
	{"unset", cmd_unset, "Remove a variable", NULL},
	{"rm", cmd_dele, "Delete a remote file", NULL},
	{"run", NULL, "Run a script file", NULL},
	{"alias", cmd_alias, "Define a command alias", NULL},
	{"getlist", cmd_getlist, "Get all files listed in param.txt", NULL},
	{"assert", cmd_assert, "Die if the last operation was not successful", NULL},
	{"quote", cmd_quote, "Send a command directly to the server", NULL},
	{"exit", cmd_quit, "Synonym for quit", NULL},
	{"mkdir", cmd_mkdir, "Make directory", NULL},
	{"rmdir", cmd_rmdir, "Remove directory", NULL},
	{"alarm", cmd_alarm, "Motion alarm", NULL},
	{"ujpeg", cmd_ujpeg, "Upload jpeg", NULL},
	{"test", cmd_test, "Test FTP server", NULL},
	{NULL, NULL, NULL, NULL}
};

//char user[64] = "anonymous",password[64] = "appro@approtech.com";
char user[MAX_FTP_ID_LEN] = "",password[MAX_FTP_PWD_LEN] = "";
char folderName[MAX_STRING_LENGTH] = "";
char ufileName[64] = "";
char ufilepath[64] = "/tmp";
char serverIP[MAX_DOMAIN_NAME+1];
char jpeg_name[MAX_STRING_LENGTH];
int imageTotal = -1;
int name_with_date = 0;
int pid;
int mode = 0;	//active or passive
int prealarm = 0;
int postalarm = 0;
int jpg_sel = 1;
int msgid = 0;
int m_id = 0;
int test_mode = 0;
static int qid;
char *pid_filename = NULL;
//int profileID;
SysInfo *pSysInfo;
/***************************************************************************
 *                                                                         *
 ***************************************************************************/
int getCurrentFrameRate(int img_src){
    //const int frame_rate[] = {30, 15, 7, 4, 1};
    int x;

    for( x = 0; x < 3; x++) {
        if( pSysInfo->profile_config.profile_codec_fmt[x] == SEND_FMT_JPEG ) {
            if( (pSysInfo->profile_config.profile_codec_idx[x] - 1) == img_src )
                //return atoi(pSysInfo->config.profile_config[x].framerate);
                return atoi(conf_string_read(PROFILE_CONFIG0_FRAMERATE+x*PROFILE_STRUCT_SIZE));
        }
    }
    return -1;
}

int main(int argc, char **argv) {
	int arg, ignoreerrors = 0;
	int i , in;
	//int nice_value;
	char *buffer;
	FILE *scriptfile = NULL;
	FILE *rcfilehandle = NULL;
	char *rcfile = NULL;
	//struct stat statbuf;
	//mode_t modes;
	//pid_t w_pid;
#if defined( SUPPORT_QOS_SCHEDULE )
	uid_t orig_euid;
#endif

#if defined( SUPPORT_QOS_SCHEDULE )
	orig_euid = geteuid();
#endif
	if(freopen(DEV_TTYS0, "w", stderr) == NULL) {
		system("echo 'freopen /dev/ttyS0 failed' >> /proc/appro/osd_puts\n");
	}

	log_set_level(5);
	log_show_function = false;
	log_show_pid = false;
	log_show_file = false;
	log_show_level = false;
	log_show_date = false;
	//int snapshot_jpg_index,x;

	setbuf(stdout, NULL);
	log(LOG_CRITICAL, "quftp version %s David Parrish (david@dparrish.com)\n\n", VERSION);
	leftqueue = NULL;

	rl_attempted_completion_function = (CPPFunction *)quftp_completion;
	setconfig(&variables, "PROGRAM_NAME", "quftp");
	setconfig(&variables, "VERSION", VERSION);
	setconfig(&variables, "CURRENT_CONNECTION", "0");
	setconfig(&variables, "LAST_CONNECTION", "1");
	
//	stat( FTP_CLIENT_PID , &statbuf );
// 	modes = statbuf.st_mode;

	if((buffer = malloc(512)) == NULL){
		DBGERR("Memory allocate error!\n");
		exit(-1);
	}
	memset(buffer,0,512);
/*  AICHI disabled
	if( S_ISREG(modes) ){
		DBG("ftp-client is executing.\n");
		return 0;
	}
	if((in = open( FTP_CLIENT_PID ,O_WRONLY | O_CREAT)) == -1 ){
		DBG("%s file open error!\n",FTP_CLIENT_PID);
		exit(1);
	}	
	sprintf(buffer,"%d\n",getpid());
	write(in, buffer ,strlen(buffer));
	close( in );
*/
	//pSysInfo = (SysInfo *)sysinfo_init(sizeof(SysInfo));
	pSysInfo = (SysInfo *)sysinfo_init(sizeof(SysInfo), 1);
	for (i = 0; i < MAX_CONNECTIONS; i++) {
		connection[i] = ftp_new();
		connection[i]->variables = variables;
		getcwd((char *)connection[i]->localdir, 256);
	}

	/* Process command line arguments kevinlee*/  
	while ((arg = getopt(argc, argv, "U:W:S:F:R:P:M:T:A:a:e:j:d::::r:s:f:g:N:ixhI:E:t")) != -1) {
		switch (arg) {
			case 'U':
				strcpy (user , optarg);
				printf ("user =%s \n",user);
				break;
			case 'W':
				strcpy (password, optarg);
				printf ("user =%s \n",password);
				break;
			case 'S':
				strcpy (serverIP, optarg);
				printf ("serverIP =%s \n",serverIP);
				break;
			case 'F':
				strcpy (folderName, optarg);
				printf ("folderName =%s \n",folderName);
				break;
			case 'R':
				strcpy (jpeg_name, optarg);
				printf ("jpeg_name=%s \n",jpeg_name);
				break;
			case 'x':
				name_with_date =1;
				printf ("file name with date\n");
				break;
			case 'P':
				ftp_port = atoi(optarg);
				printf ("ftp_port=%d \n",ftp_port);
				break;
			case 'M':
				mode = atoi(optarg);
				printf ("mode=%d \n",mode);
				break;
			case 'T':
				imageTotal= atoi(optarg);
				printf ("imageTotal=%d \n",imageTotal);
				break;
			case 'A':
				postalarm= atoi(optarg);
				printf ("postalarm=%d \n",postalarm);
				break;
			case 'a':
				prealarm = atoi(optarg);
				printf("prealarm time = %d\n",prealarm);
				if( prealarm < 0 ) return -1;
				  break;
			case 'e':
				DBG("pid_filename = %s\n",optarg);
				pid_filename = malloc(strlen(optarg)+1);
				strcpy( pid_filename , optarg );
				break;
			case 'j':
				  jpg_sel = atoi(optarg);
				  if( jpg_sel < 1 || jpg_sel > MAX_PROFILE_NUM ) return -1;
				  //jpg_sel--;
				  //printf("jpg_sel = %d\n",jpg_sel);
				  break;
				  
			case 'I':
				msgid = atoi(optarg);
                /*
				if(msgid < 11 || msgid > 31) {
				    DBG("msgid = %d over range", msgid);
				    exit(0);
				}
				*/
			break;
                
			case 'd': if (optarg)
					  log_set_level(atoi(optarg));
				  else
					  log_set_level(log_get_level() + 1);
				  log(log_get_level(), __FILE__, __LINE__, __FUNCTION__, "Debugging level is now %d", log_get_level());
				  break;
			case 'i': ignoreerrors = 1;
				  break;
			case 'r': rcfile = strdup(optarg);
				  log(LOG_WARNING, "Reading configuration from %s\n", rcfile);
				  break;
			case 's': if (!(scriptfile = fopen(optarg, "r"))) {
					  perror("fopen");
					  if (!ignoreerrors) exit(0);
				  }
				  break;
			case 'f': strcpy(ufileName,strdup(optarg));
					printf ("ufileName=%s \n",ufileName);
				  break;
			case 'g': strcpy(ufilepath,strdup(optarg));
					printf ("ufilepath=%s \n",ufilepath);
				  break;				  
			case 'N': //nice_value = atoi(optarg);
				  //printf ("nice_value=%d \n",nice_value);
				  //if(nice_value < 20 && nice_value > -21 && nice_value != 0) {
					//dbg("priority = %d ", nice_value);
					//setpriority(PRIO_PROCESS, 0, nice_value);
				  //}
				  break;

			case 'E':
				m_id = atoi(optarg);
				dbg("Media Video ID = %d", m_id);
				break;

			case 't':
				test_mode = 1;
				dbg("Enable TEST MODE");
				break;
				
			case 'h': 
				printf( "Usage: quftp [-Uusername][-Wpassword][-vhi] [-d[n]] [-rfilename] [-sfilename] [URL]\n\n" );
				printf( " -U\tUsername\n" );
				printf( " -W\tPassword\n" );
				printf( " -S\tserver IP\n" );
				printf( " -F\tFolderName\n" );
				printf( " -R\tFile Name \n" );
				printf( " -P\tFTP Server Port \n" );
				printf( " -M\tPassive Mode 1 open 0 clase \n" );
				printf( " -A\tprealarm file \n" );
				printf( " -h\tPrint this help, then exit\n" );
				printf( " -d\tIncrease debugging level (optional argument)\n" );
				printf( " -f\tFile path\n" );
				printf( " -i\tIgnore errors in a script\n" );
				printf( " -r\tRead configuration from filename instead of %s\n", DEFAULT_RC_FILENAME );
				printf( " -s\tProcess script filename\n" );
				printf( "URL\tConnect to any valid ftp url.. Read man 1 quftp for more information\n" );
				printf( "\n" );
				exit( 0 );
				break;
		}
	}

	if(ApproDrvInit(msgid) < 0){
		DBG("ApproDrvInit Fail\n");
		exit(1);
	}

	if(func_get_mem(&qid) != 0){
		ApproDrvExit();
		exit(1);
	}

	if(InitFileMsgDrv(FILE_MSG_KEY, msgid) < 0) {
		DBG("FileMsgDrv Initial Failed!! ");
		exit(1);
	}

	if( pid_filename ){
		sprintf(buffer,"/var/run/%s",pid_filename);
		if( access( buffer , F_OK ) == 0 ){
			printf("FTP pid file /var/run/%s exist.\n",pid_filename);
			free( pid_filename );
			return 0;
		}else{
			if((in = open( buffer ,O_WRONLY | O_CREAT)) == -1 ){
				DBGERR("/var/run/%s open error!\n",pid_filename);
				goto FTP_END;
			}
			sprintf(buffer,"%d\n",getpid());
			write(in, buffer ,strlen(buffer));
			close( in );
		}
	}

	//profileID = jpg_sel;
	/*
	if( pSysInfo->profile_codec_fmt[jpg_sel] == SEND_FMT_JPEG ){
		if( pSysInfo->profile_codec_idx[jpg_sel] == 1 ) jpg_sel = 0;
		else if( pSysInfo->profile_codec_idx[jpg_sel] == 2 ) jpg_sel = 1;
		else if( pSysInfo->profile_codec_idx[jpg_sel] == 3 ) jpg_sel = 2;
		else goto FTP_END;
	}
	else if( pSysInfo->profile_codec_fmt[jpg_sel] == SEND_FMT_MPEG4 ){
		snapshot_jpg_index = pSysInfo->buf_info.jpg_used - 1;
		for( x = 0 ; x <= jpg_sel ; x++ ){
			if( pSysInfo->profile_codec_fmt[x] == SEND_FMT_MPEG4 ){
				snapshot_jpg_index++;
			}
		}
		jpg_sel = snapshot_jpg_index;
		printf("snapshot_jpg_index = %d\n",jpg_sel);
	}
	else if(!strlen(ufileName)) {
		DBG("profile %d is MPEG4.\n",jpg_sel + 1);
		goto FTP_END;
	}
	*/  // Disable by Alex. 2010.01.09

	/*    open script file   */
	if (!(scriptfile = fopen("/opt/ipnc/upload", "r"))) {
		perror("fopen");
		if (!ignoreerrors) exit(0);
 	}

	if (!rcfile) {
		rcfile = strdup(DEFAULT_RC_FILENAME);
	}

	if (rcfile) {
		rcfile = expand_home(rcfile);
		rcfilehandle = fopen(rcfile, "r");
		if (!rcfilehandle) log(LOG_WARNING, "Can't find rcfile %s", rcfile);
	}

	if (optind < argc) {
		if (!parseurl(argv[optind], CURRENT_CONNECTION)) {
			log(LOG_ALERT, "There was an error parsing \"%s\"\n", argv[optind]);
		} else {
			if (strlen(CURRENT_CONNECTION->password) == 0) {
				if (config(variables, "email"))
					strncpy((char *)CURRENT_CONNECTION->password, config(variables, "email"), 32);
				else
					strncpy((char *)CURRENT_CONNECTION->password, "user@quftp.com", 32);
			}
			CURRENT_CONNECTION->status = STATUS_WAIT_CONNECT;
			if (CURRENT_CONNECTION->remotedir[0] != '\0')
				(void)getconnected(CURRENT_CONNECTION);
		}
	}
#if defined( SUPPORT_QOS_SCHEDULE )
	if ( seteuid( 15 ) )	/* set Effective UID to 'ftp' */
		perror( "seteuid" );
#endif

	while (1) {
		char *line = NULL, *command_word = NULL;
		struct COMMAND *command = NULL;
		register int i;
		int result;

		if (line) {
			free(line);
			line = NULL;
		}
		if (rcfilehandle) {
			char *temp;
			line = (char *)malloc(4096);
			fgets(line, 4096, rcfilehandle);
			if (!*line) { /* End of script? */
				fclose(rcfilehandle);
				rcfilehandle = NULL;
				continue;
			}
			if (*line == '#') continue;	// Comment
			temp = strchr(line, '\n');
			if (temp == line) continue;	// Zero length line?
			else *temp = '\0';		// Strip \n
			log(LOG_INFO, "%s> %s", rcfile, line);
		} else if (scriptfile) { /* Reading from a script or command line? */
			char *temp;
			line = (char *)malloc(4096);
			fgets(line, 4096, scriptfile);
			if (!*line) { /* End of script? */
				fclose(scriptfile);
				scriptfile = NULL;
				continue;
			}
			if (*line == '#') continue;
			temp = strchr(line, '\n');
			if (temp == line) continue;	// Zero length line?
			else *temp = '\0';		// Strip \n
			add_history(line);
			printf("quftp> %s\n", line);
		} else {
			line = readline("quftp> ");
			if (!line) {			// NULL returned from readline is CTRL-D
				line=strdup("quit");
			} else if(!*line) {		// "" returned from readline is an empty line
				continue;
			}
			add_history(line);
		}

		line = parseline(line);
		line = expand_alias(line);

		command_word = getnextword(line, GNW_START);
		if (!command_word || !*command_word) continue;

		/* Match core command set */
		if (strcasecmp(command_word, "switch") == 0) {
			char *temp;
			char *param = getnextword(line, GNW_NEXT);
			if (!param) {
				int curr = atoi(config(variables, "CURRENT_CONNECTION"));
				int last = atoi(config(variables, "LAST_CONNECTION"));
				temp = (char *)malloc(4096);
				sprintf(temp, "%d", last);
				setconfig(&variables, "CURRENT_CONNECTION", temp);
				sprintf(temp, "%d", curr);
				setconfig(&variables, "LAST_CONNECTION", temp);
				free(temp);
			} else {
				int new_connection = atoi(param);
				if (new_connection >= MAX_CONNECTIONS) {
					log(LOG_CRITICAL, "Only %d number of connections avaliable (0-%d)\n",
						MAX_CONNECTIONS, MAX_CONNECTIONS-1);
					continue;
				} else {
					temp = (char *)malloc(4096);
					if (new_connection == atoi(config(variables, "CURRENT_CONNECTION"))) continue;
					setconfig(&variables, "LAST_CONNECTION", config(variables, "CURRENT_CONNECTION"));
					sprintf(temp, "%d", new_connection);
					setconfig(&variables, "CURRENT_CONNECTION", temp);
					free(temp);
				}
			}
			log(LOG_WARNING, "Switched to connection %s\n", config(variables, "CURRENT_CONNECTION"));
			setconfig(&variables, "HOSTNAME", *CURRENT_CONNECTION->hostname ? CURRENT_CONNECTION->hostname : NULL);
			setconfig(&variables, "USERNAME", *CURRENT_CONNECTION->username ? CURRENT_CONNECTION->username : NULL);
			continue;
		} else if (strstr(command_word, "!") == command_word) {
			char *temp;
			int rval;
			temp = line + 1;
			rval = system(temp);
			if (rval) log(LOG_ALERT, "Return value from \"%s\": %d\n", temp, rval);
			continue;
		} else if (strcasecmp(command_word, "run") == 0) {
			char *filename;
			filename = getnextword(line, GNW_NEXT);
			if (!filename || !*filename) {
				log(LOG_CRITICAL, "You must specify a script file to run\n");
				continue;
			}
			if (!(scriptfile = fopen(filename, "r"))) {
				perror("fopen");
				if (!ignoreerrors) continue;
			}
			continue;
		} else if (strcasecmp(command_word, "debug") == 0) {
			char *param;
			if ((param = getnextword(line, GNW_NEXT)))
				log_set_level(atoi(param));
			else
				log_set_level(log_get_level());
			log(log_get_level(), __FILE__, __LINE__, __FUNCTION__, "Debugging level is now %d", log_get_level());
			continue;
		} else if (strcasecmp(command_word, "help") == 0 || strcmp(command_word, "?") == 0) {
			char *param;
			int foundit = 0;
			if ((param = getnextword(line, GNW_NEXT))) {
				for (i = 0; command_list[i].name && *command_list[i].name; i++) {
					if (strcmp(command_list[i].name, param) == 0) {
						if (command_list[i].longhelp) {
							printf("%s\n", command_list[i].longhelp);
						} else {
							printf("No extra help available for %s\n", param);
						}
						foundit = 1;
					}
				}
				if (!foundit) {
					printf("Command %s does not exist\n", param);
				}
			} else {
				for (i = 0; command_list[i].name && *command_list[i].name; i++)
					printf("%-10s%s\n", command_list[i].name,
							command_list[i].description);
			}
			continue;
		}

		/* Match dynamic command set */
		for (i = 0; command_list[i].name; i++) {
			if (strcasecmp (command_word, command_list[i].name) == 0) {
				command = &command_list[i];
				break;
			}
		}
		if (!command) {
			printf("Unknown command \"%s\"\n", command_word);
			if (scriptfile && !ignoreerrors) break;
			continue;
		}
		result = (command->function)(line);
		if (result < 0) {
			break;
		} else if (result > 0) {	// Successful command
			setconfig(&variables, "SUCCESSFUL_OPERATION", "true");
		} else {			// Unsuccessful command
			setconfig(&variables, "SUCCESSFUL_OPERATION", "false");
			if (scriptfile && !ignoreerrors) {
				log(LOG_CRITICAL, "Error processing script\n");
				break;
			}
		}
		continue;
	}
FTP_END:
#if defined( SUPPORT_QOS_SCHEDULE )
	if ( seteuid( orig_euid ) )	/* set Effective UID to origional euid */
		perror( "seteuid" );
#endif
	ApproInterfaceExit();
	//SysDrvExit();
	if( pid_filename ){
		sprintf(buffer,"/var/run/%s",pid_filename);
		unlink( buffer );
	}
#if defined( SUPPORT_QOS_SCHEDULE )
	if ( seteuid( 15 ) )	/* set Effective UID to 'ftp' */
		perror( "seteuid" );
#endif
	ftp_disconnect(CURRENT_CONNECTION);
#if defined( SUPPORT_QOS_SCHEDULE )
	if ( seteuid( orig_euid ) )	/* set Effective UID to origional euid */
		perror( "seteuid" );
#endif
	if( pid_filename ) free( pid_filename );

	// delete the specific file
	if(strlen(ufileName))
		remove(ufileName);
	return 0;
}

struct qitem *addtoqueue(struct qitem **queue, struct qitem *item) {
	struct qitem *newitem = 0;
	struct qitem *tempitem = 0, *lastgooditem = 0;
	int id = 0;
	if (!(newitem = (struct qitem *)malloc(sizeof(struct qitem)))) {
		perror("malloc");
		exit(-1);
	}
	memcpy(newitem, item, sizeof(struct qitem));
	tempitem = *queue; /* Find the last item on the queue */
	while (tempitem) {
		lastgooditem = tempitem;
		if (strcmp(newitem->remotefilename, tempitem->remotefilename) == 0) return NULL;
		if (tempitem->id > id) id = tempitem->id;
		tempitem = tempitem->next;
	}
	newitem->id = ++id;
	if (lastgooditem) lastgooditem->next = newitem;
	if (!*queue) *queue = newitem;
	newitem->next = 0;
	return newitem;
}

struct qitem *delfromqueue(struct qitem **queue, int id) {
	struct qitem *item, *last;
	int counter;
	item = last = *queue;
	while (item) {
		if (item->id == id) {
			last->next = item->next;
			if (item == *queue) {
				if (item->next) *queue = item->next;
				else *queue = NULL;
			}
			free(item);
			item = *queue;
			counter = 1; /* Straighten out counter */
			while (item) {
				item->id = counter;
				counter++;
				item = item->next;
			}
			return NULL;
		}
		last = item;
		item = item->next;
	}
	return *queue;
}

void clearqueue(struct qitem *queue) {
	struct qitem *item, *next = NULL;
	item = queue;
	while (item) {
		next = item->next;
		free(item);
		item = next;
	}
}

struct qitem *runqueue(struct qitem *queue) {
	struct qitem *item;
	int id, n;
	double starttime, timelength, endtime, speed;
	struct timeval tv;
	item = queue;
	while (item) {
		id = item->id;
		if (item->direction == DIRECTION_GET) {
			int result;
			result = chdir(item->localdir);
			if (result) switch(errno) {
				case ENOENT  : log(LOG_ERROR, "Creating directory %s\n", item->localdir);
							mkdir(item->localdir, 0755);
							break;
				case ENOTDIR : log(LOG_ERROR, "A component of path is not a directory.\n");
							item = item->next;
							delfromqueue(&queue, id);
							break;
				case EACCES  :
				default	     : log(LOG_ERROR, "Can't change to %s: ", item->localdir, strerror(errno));
							item = item->next;
							delfromqueue(&queue, id);
							continue;
							break;
			}
			result = chdir(item->localdir);
			ftp_cwd(item->remoteconnection, item->remotedir);
			gettimeofday(&tv, NULL);
			if ((item->permissions & 2048) == 2048) {
				struct qitem *newitem, *newitem2;
				char *param, *filenamelist[500];
				char path[4096];
				struct filedetails details;
				int count = 0;
				sprintf(path, "%s/%s", item->remotedir, item->remotefilename);
				ftp_cwd(item->remoteconnection, path);
				memset(filenamelist, 0, 500 * sizeof(char *));
				ftp_nlst(item->remoteconnection, NULL, filenamelist, 500);
				while (count < 500 && filenamelist[count]) {
					param = filenamelist[count];
					if (!param || !*param) {
						free(param);
						count++;
						continue;
					}
					if (!(newitem = malloc(sizeof(struct qitem)))) {
							perror("malloc");
							exit(1);
					}
					memset(newitem, 0, sizeof(struct qitem));
					strncpy(newitem->localfilename, param, 256);
					strncpy(newitem->remotefilename, param, 256);

					result = ftp_exists(CURRENT_CONNECTION, param);
					if (result == 0) {
						log(LOG_ALERT, "%s does not exist\n", param);
						free(newitem);
						free(param);
						continue;
					}
					if (result < 0) {
						log(LOG_ALERT, "Can't verify existance of %s.. Don't blame me if this fails!\n", param);
					}
					if (result > 0) {
						ftp_stat(CURRENT_CONNECTION, param, &details);
						newitem->permissions = details.permissions;
						newitem->size = details.size;
					}
					newitem->direction = DIRECTION_GET;
					sprintf(newitem->remotedir, "%s/%s", item->remotedir,
							item->remotefilename);
					sprintf(newitem->localdir, "%s/%s", item->localdir,
							item->localfilename);
					newitem->remoteconnection = item->remoteconnection;
					newitem->localconnection = item->localconnection;
					if ((newitem2 = addtoqueue(&queue, newitem))) {
						log(LOG_INFO, "Queued %s/%s for download, position %d\n", newitem2->remotedir, newitem2->localfilename, newitem2->id);
					}
					free(newitem);
					free(param);
					count++;
				}
				item = item->next;
				delfromqueue(&queue, id);
				continue;
			}

			starttime = ((tv.tv_sec * 1.0) + (tv.tv_usec / 1000000.0));
			n = ftp_get_file(item->remoteconnection, item);
			if (n > 0) {
				gettimeofday(&tv, NULL);
				endtime = ((double)tv.tv_sec + (tv.tv_usec / 1000000));
				timelength = abs(endtime - starttime);
				if (timelength == 0) timelength = 1;
				log(LOG_INFO, "Received %s (%d bytes)",
					item->remotefilename, n);
				speed = (n / 1000) / timelength;
				log(LOG_INFO, " in %lu seconds (%1.2fKb/s)\n",
					(unsigned long)timelength, speed);
			}
		} else if (item->direction == DIRECTION_PUT) {
			if (!ftp_cwd(item->remoteconnection, item->remotedir)) {
				if (!ftp_mkd(item->remoteconnection, item->remotedir)) {
					item = item->next;
					delfromqueue(&queue, id);
					continue;
				}
				if (!ftp_cwd(item->remoteconnection, item->remotedir)) {
					item = item->next;
					delfromqueue(&queue, id);
					continue;
				}
			}
			chdir(item->localdir);
			if (S_ISDIR(item->permissions)) {
				struct qitem *newitem, *newitem2;
				struct dirent *dirent;
				DIR *dir;
				char path[4096];
				int count = 0;
				sprintf(path, "%s/%s", item->localdir, item->localfilename);
				dir = opendir(path);
				if (!dir) {
					log(LOG_ALERT, "Can't recursively put %s\n", path);
					item = item->next;
					delfromqueue(&queue, id);
					continue;
				}
				while ((dirent = readdir(dir))) {
					char fullname[4096];
					struct stat statbuf;
					if (strcmp(dirent->d_name, ".") == 0) continue;
					if (strcmp(dirent->d_name, "..") == 0) continue;
					if (strlen(dirent->d_name) == 0) continue;
					if (!(newitem = malloc(sizeof(struct qitem)))) {
							perror("malloc");
							exit(1);
					}
					sprintf(fullname, "%s/%s", path, dirent->d_name);
					if (stat(fullname, &statbuf)) {
						log(LOG_ALERT, "%s does not exist\n", fullname);
						free(newitem);
						continue;
					}
					memset(newitem, 0, sizeof(struct qitem));
					strncpy(newitem->localfilename, dirent->d_name, 256);
					strncpy(newitem->remotefilename, dirent->d_name, 256);
					newitem->permissions = statbuf.st_mode;
					newitem->size = statbuf.st_size;
					newitem->direction = DIRECTION_PUT;
					sprintf(newitem->remotedir, "%s/%s", item->remotedir,
							item->remotefilename);
					sprintf(newitem->localdir, "%s/%s", item->localdir,
							item->localfilename);
					newitem->remoteconnection = item->remoteconnection;
					newitem->localconnection = item->localconnection;
					if ((newitem2 = addtoqueue(&queue, newitem))) {
						log(LOG_INFO, "Queued %s/%s for upload, position %d\n", newitem2->remotedir, newitem2->localfilename, newitem2->id);
					}
					free(newitem);
					count++;
				}
				item = item->next;
				delfromqueue(&queue, id);
				continue;
			}
			starttime = ((tv.tv_sec * 1.0) + (tv.tv_usec / 1000000.0));
			n = ftp_put_file(item->remoteconnection, item);
			if (n > 0) {
				gettimeofday(&tv, NULL);
				endtime = ((double)tv.tv_sec + (tv.tv_usec / 1000000));
				timelength = abs(endtime - starttime);
				if (timelength == 0) timelength = 1;
				log(LOG_INFO, "Sent %s (%d bytes)",
					item->remotefilename, n);
				speed = (n / 1000) / timelength;
				log(LOG_INFO, " in %lu seconds (%1.2fKb/s)\n",
					(unsigned long)timelength, speed);
			}
		} else if (item->direction == DIRECTION_FXP) {
			char sourcefile[4096], destfile[4096];
			if ((item->permissions & 2048) == 2048) {
				struct qitem *newitem, *newitem2;
				char *param, *filenamelist[500];
				char path[4096];
				struct filedetails details;
				int count = 0;
				sprintf(path, "%s/%s", item->remotedir, item->remotefilename);
				ftp_cwd(item->localconnection, path);
				memset(filenamelist, 0, 500 * sizeof(char *));
				ftp_nlst(item->localconnection, NULL, filenamelist, 500);
				while (count < 500 && filenamelist[count]) {
					int result;
					param = filenamelist[count];
					if (!param || !*param) {
						free(param);
						count++;
						continue;
					}
					if (!(newitem = malloc(sizeof(struct qitem)))) {
							perror("malloc");
							exit(1);
					}

					result = ftp_exists(CURRENT_CONNECTION, param);
					if (result == 0) {
						log(LOG_WARNING, "%s does not exist\n", param);
						free(newitem);
						free(param);
						continue;
					}
					if (result < 0) {
						log(LOG_WARNING, "Can't verify existance of %s.. Don't blame me if this fails!\n", param);
					}
					if (result > 0) {
						ftp_stat(CURRENT_CONNECTION, param, &details);
						newitem->permissions = details.permissions;
						newitem->size = details.size;
					}

					memset(newitem, 0, sizeof(struct qitem));
					strncpy(newitem->localfilename, param, 256);
					strncpy(newitem->remotefilename, param, 256);
					newitem->direction = DIRECTION_GET;
					sprintf(newitem->remotedir, "%s/%s", item->remotedir,
							item->remotefilename);
					sprintf(newitem->localdir, "%s/%s", item->localdir,
							item->localfilename);
					newitem->remoteconnection = item->remoteconnection;
					newitem->localconnection = item->localconnection;
					if ((newitem2 = addtoqueue(&queue, newitem))) {
						log(LOG_INFO, "Queued %s/%s for FxP, position %d\n", newitem2->remotedir, newitem2->localfilename, newitem2->id);
					}
					free(newitem);
					free(param);
					count++;
				}
				item = item->next;
				delfromqueue(&queue, id);
				continue;
			}
			sprintf(sourcefile, "%s/%s", item->localdir, item->localfilename);
			sprintf(destfile, "%s/%s", item->remotedir, item->remotefilename);
			starttime = ((tv.tv_sec * 1.0) + (tv.tv_usec / 1000000.0));
			log(LOG_WARNING, "Progress information not available for FxP transfers\n");
			n = ftp_fxp(item->localconnection, item->remoteconnection,
					sourcefile, destfile);
			if (n == 0) {
				gettimeofday(&tv, NULL);
				endtime = ((double)tv.tv_sec + (tv.tv_usec / 1000000));
				timelength = abs(endtime - starttime);
				if (timelength == 0) timelength = 1;
				log(LOG_INFO, "Sent %s (%lu bytes)",
					item->remotefilename, item->size);
				speed = (item->size / 1000) / timelength;
				log(LOG_INFO, " in %lu seconds (%1.2fKb/s)\n",
					(unsigned long)timelength, speed);
			}
		} else {
			log(LOG_CRITICAL, "Corrupted item %d\n", id);
		}
		item = item->next;
		delfromqueue(&queue, id);
	}
	return queue;
}

void printqueue(struct qitem *queue) {
	struct qitem *item;
	char fullname[200];
	long totalsize = 0;
	log(LOG_CALLS, "%p", queue);
	item = queue;
	printf("%-3s    %-30s %-10s%-30s\n", "ID", "Source",
		"Size", "Destination");
	while (item) {
		if (item->direction == DIRECTION_GET) {
			sprintf(fullname, "%s/%s", item->remotedir, item->remotefilename);
			printf("%-3d <- %-30s %-10lu%-30s\n", item->id, fullname, item->size,
					item->localdir);
		} else if (item->direction == DIRECTION_PUT) {
			sprintf(fullname, "%s/%s", item->localdir, item->localfilename);
			printf("%-3d -> %-30s %-10lu%-30s\n", item->id, fullname,
				item->size, item->remotedir);
		} else if (item->direction == DIRECTION_FXP) {
			char remotename[200];
			sprintf(fullname, "%s:%s%s%s%s", item->localconnection->hostname,
					item->localdir, (item->remotefilename[0] == '/')
					? "" : "/", item->localfilename,
					((item->permissions & 2048) == 2048) ? "/" : "");
			sprintf(remotename, "%s:%s%s%s%s", item->remoteconnection->hostname,
					item->remotedir, (item->remotefilename[0] == '/')
					? "" : "/", item->remotefilename,
					((item->permissions & 2048) == 2048) ? "/" : "");
			printf("%-3d -> %-30s %-10lu%-30s\n", item->id, fullname,
				item->size, remotename);
		} else {
			log(LOG_CRITICAL, "Bad entry %d\n", item->id);
		}
		totalsize += item->size;
		item = item->next;
	}
	printf("\nTotal queue size: %li bytes\n\n", totalsize);
}

int getconnected(struct ftpconnection *connection) {
	char *dir = NULL;
	log(LOG_CALLS, "%p", connection);
	if (!connection) return 0;
	if (!connection->hostname) return 0;
	if (connection->status > STATUS_WAIT_CONNECT) return 1;
	if (connection->remotedir[0] != '\0') {
		dir = strdup(connection->remotedir);
	}
	if (ftp_connect(connection)) {
		if (dir) free(dir);
		return 0;
	}
	if (ftp_login(connection)) {
		SYSLOG tlog;
		
		tlog.opcode = SYSLOG_NETFAIL_FTPC_LOGIN;
		tlog.ip = connection->hostip;
		SetSysLog(&tlog);

		if (dir) free(dir);
		ftp_disconnect(connection);
		return 0;
	}
	if (dir) {
		if (!ftp_cwd(connection, dir)) log(LOG_ALERT, "Can't change directory to %s\n", dir);
		free(dir);
	}
	connection->status = STATUS_IDLE;
	setconfig(&variables, "USERNAME", connection->username);
	return 1;
}

char *complete_remote_filename(const char *text, int state) {
	static char *filenamelist[500];
	static int index = 0;
	int response;
	char *retstring = (char *)NULL;

	if (state == 0) {
		char *path;
		memset(&filenamelist[0], 0, sizeof(char *) * 500);
		if (!text || !*text) return NULL;
		path = getpath(text);
		response = ftp_nlst(CURRENT_CONNECTION, path, filenamelist, 500);
		if (response < 0) return NULL;
		index = 0;
	}

	while (filenamelist[index]) {
		if (strstr(filenamelist[index], text) == filenamelist[index]) {
			retstring = strdup(filenamelist[index]);
			free(filenamelist[index]);
			index++;
			return retstring;
		}
		free(filenamelist[index]);
		index++;
	}
	index = 0;
	return NULL;
}

char *complete_command(const char *text, int state) {
	/* Don't know how I'm going to do this yet */
	return NULL;
}

char **quftp_completion(char *text, int start, int end) {
	char **matches;
	matches = (char **)NULL;
	if (start == 0) {
		/* It's a command */
		matches = rl_completion_matches(text, complete_command);
	} else if (strstr(text, "put") != text){
		/* It's a filename */
		matches = rl_completion_matches(text, complete_remote_filename);
	}
	return (matches);
}

int cmd_ls(char *command_line) {
	int result;
	char *params = NULL;
	params = getnextword(command_line, GNW_REST);
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "ls: Not connected\n");
		return 0;
	}
	result = ftp_list(CURRENT_CONNECTION, params, 1);
	printf("\n");
	if (result < 0) return 0;
	return 1;
}


int cmd_cd(char *command_line) {
	char *param;
	SYSLOG tlog;
	
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "cd: Not connected\n");
		return 0;
	}
	getnextword(command_line, GNW_START);
	param = getnextword(command_line, GNW_NEXT);
	if (!param || !*param) {
		log(LOG_ALERT, "Must specify a directory to change\n");
		return 0;
	} else {
		if (!ftp_cwd(CURRENT_CONNECTION, param)) {
			tlog.opcode = SYSLOG_NETFAIL_FTPC_CWD;
			tlog.ip = inet_addr(CURRENT_CONNECTION->hostname);
			SetSysLog(&tlog);
			return 0;
		}
	}
	return 1;
}

int cmd_cdup (char *command_line) {
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "cdup: Not connected\n");
		return 0;
	}
	if (!ftp_cwd(CURRENT_CONNECTION, "..")) return 0;
	return 1;
}

int cmd_pwd (char *command_line) {
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "pwd: Not connected\n");
		return 0;
	}
	ftp_pwd(CURRENT_CONNECTION);
	log(LOG_INFO, "The current remote directory is %s\n", CURRENT_CONNECTION->remotedir);
	log(LOG_INFO, "The current local directory is %s\n", CURRENT_CONNECTION->localdir);
	return 1;
}

int cmd_lcd(char *command_line) {
	char *param = NULL;
	int rval;
	getnextword(command_line, GNW_START);
	param = getnextword(command_line, GNW_NEXT);
	if (!param || !*param) {
		log(LOG_ALERT, "Specify a directory to change to\n");
		return 0;
	}
	rval = chdir(param);
	if (rval) {
		log(LOG_ALERT, "Can't change to \"%s\"\n", param);
		return 0;
	}
	getcwd(CURRENT_CONNECTION->localdir, 256);
	log(LOG_INFO, "Current local directory is now \"%s\"\n", CURRENT_CONNECTION->localdir);
	return 1;
}

int cmd_get(char *command_line) {
	struct qitem *item, *newitem;
	char *expanded_line;
	char *param;
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "get: Not connected\n");
		return 0;
	}
	expanded_line = expand_wildcard(command_line);
	getnextword(expanded_line, GNW_START);
	while ((param = getnextword(expanded_line, GNW_NEXT))) {
		char path[4096], filename[4096];
		int index;
		struct filedetails details;
		if (!param || !*param) continue;
		if (ftp_stat(CURRENT_CONNECTION, param, &details)) {
			log(LOG_ALERT, "%s does not exist\n", param);
			continue;
		}
		if (!(item = malloc(sizeof(struct qitem)))) {
				perror("malloc");
				exit(1);
		}
		memset(item, 0, sizeof(struct qitem));
		memset(path, 0, 4096);
		memset(filename, 0, 4096);
		item->permissions = details.permissions;
		item->size = details.size;
		/* Split the path and filename from the paramater */
		index = strlen(param);
		while (index) {
			if (param[index] == '/') {
				strncpy(path, param, index);
				strncpy(filename, &param[index + 1], 4096);
				break;
			}
			index--;
		}
		if (strlen(filename) == 0) strncpy(filename, param, 4096);
		strncpy(item->localfilename, filename, 256);
		strncpy(item->remotefilename, filename, 256);
		item->direction = DIRECTION_GET;
		if (path[0] == '/') {
			strncpy(item->remotedir, path, 256);
		} else {
			sprintf(item->remotedir, "%s%s%s", CURRENT_CONNECTION->remotedir,
				(CURRENT_CONNECTION->remotedir[strlen(CURRENT_CONNECTION->remotedir)-1] != '/') ? "/" : "", path);
		}
		strncpy(item->localdir, CURRENT_CONNECTION->localdir, 256);
		if (item->remotedir[strlen(item->remotedir) - 1] == '/')
			item->remotedir[strlen(item->remotedir) - 1] = '\0';
		if (item->localdir[strlen(item->localdir) - 1] == '/')
			item->localdir[strlen(item->localdir) - 1] = '\0';
		item->remoteconnection = CURRENT_CONNECTION;
		item->localconnection = NULL;
		newitem = addtoqueue(&leftqueue, item);
		if (newitem) log(LOG_INFO, "Queued %s/%s for download, position %d\n",
			item->remotedir, item->localfilename, newitem->id);
		free(item);
	}
	if (config(variables, "queuefiles")) {
		if (strcasecmp(config(variables, "queuefiles"), "false") == 0)
			cmd_go(expanded_line);
	}
	free(expanded_line);
	return 1;
}

int cmd_put(char *command_line) {
	struct qitem *item, *newitem;
	char *param, *expanded_line;
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "put: Not connected\n");
		return 0;
	}
	expanded_line = expand_wildcard(command_line);
	getnextword(expanded_line, GNW_START);
	while ((param = getnextword(expanded_line, GNW_NEXT))) {
		char path[4096], filename[4096];
		struct stat stat_buf;
		int index;
		if (!param || !*param) continue;
		if (strcmp(param, "End") == 0) continue;
		if (stat(param, &stat_buf) != 0)
			switch (errno) {
				case ENOENT	  : log(LOG_ALERT, "File %s does not exist\n", param);
						    continue;
						    break;
				case EACCES	  : log(LOG_ALERT, "Permission denied to stat\n.");
						    continue;
						    break;
				case ENOMEM	  : log(LOG_ALERT, "Out of memory\n");
						    exit(1);
						    break;
				case ENAMETOOLONG : log(LOG_ALERT, "File name \"%s\" too long\n", param);
						    continue;
						    break;
				default		  : log(LOG_ALERT, "Unknown error doing stat: %d\n", errno);
						    continue;
						    break;
			}
		if (!(item = malloc(sizeof(struct qitem)))) {
				perror("malloc");
				exit(1);
		}
		memset(item, 0, sizeof(struct qitem));
		memset(path, 0, 4096);
		memset(filename, 0, 4096);
		/* Split the path and filename from the paramater */
		index = strlen(param);
		while (index) {
			if (param[index] == '/') {
				strncpy(path, param, index);
				strncpy(filename, &param[index + 1], 4096);
				break;
			}
			index--;
		}
		if (strlen(filename) == 0) strncpy(filename, param, 4096);
		strncpy(item->localfilename, filename, 256);
		strncpy(item->remotefilename, filename, 256);
		item->direction = DIRECTION_PUT;
		if (path[0] == '/') {
			strncpy(item->localdir, path, 256);
		} else {
			sprintf(item->localdir, "%s%s%s", CURRENT_CONNECTION->localdir,
				(item->localdir[strlen(item->localdir)] != '/') ? "/" : "",
				path);
		}
		strncpy(item->remotedir, CURRENT_CONNECTION->remotedir, 256);
		if (strlen(item->remotedir) > 1)
			if (item->remotedir[strlen(item->remotedir) - 1] == '/')
				item->remotedir[strlen(item->remotedir) - 1] = '\0';
		if (strlen(item->localdir) > 1)
			if (item->localdir[strlen(item->localdir) - 1] == '/')
				item->localdir[strlen(item->localdir) - 1] = '\0';
		item->remoteconnection = CURRENT_CONNECTION;
		item->localconnection = NULL;
		item->permissions = stat_buf.st_mode;
		item->size = stat_buf.st_size;
		newitem = addtoqueue(&leftqueue, item);
		if (newitem) log(LOG_INFO, "Queued %s/%s for upload, position %d\n",
			item->remotedir, param, newitem->id);
		free(item);
	}
	if (config(variables, "queuefiles")) {
		if (strcasecmp(config(variables, "queuefiles"), "false") == 0)
			cmd_go(expanded_line);
	}
	free(expanded_line);
	return 1;
}

int cmd_fxp(char *command_line) {
	struct qitem *item, *newitem;
	char *param, *expanded_line;

	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "fxp: Not connected\n");
		return 0;
	}
	if (!getconnected(LAST_CONNECTION)) {
		log(LOG_ALERT, "fxp: Not connected\n");
		return 0;
	}
	expanded_line = expand_wildcard(command_line);
	getnextword(expanded_line, GNW_START);
	while ((param = getnextword(expanded_line, GNW_NEXT))) {
		char path[4096], filename[4096];
		int index;
		struct filedetails details;
		if (!param || !*param) continue;
		log(LOG_NOTICE, "FxP'ing \"%s\" from %s to %s", param,
				CURRENT_CONNECTION->hostname,
				LAST_CONNECTION->hostname);
		if (!(item = malloc(sizeof(struct qitem)))) {
				perror("malloc");
				exit(1);
		}
		if (ftp_stat(CURRENT_CONNECTION, param, &details)) {
			log(LOG_ALERT, "%s does not exist\n", param);
			free(item);
			continue;
		}
		memset(item, 0, sizeof(struct qitem));
		memset(path, 0, 4096);
		memset(filename, 0, 4096);
		/* Split the path and filename from the paramater */
		index = strlen(param);
		while (index) {
			if (param[index] == '/') {
				strncpy(path, param, index);
				strncpy(filename, &param[index + 1], 4096);
				break;
			}
			index--;
		}
		if (strlen(filename) == 0) strncpy(filename, param, 4096);
		strncpy(item->localfilename, param, 256);
		strncpy(item->remotefilename, param, 256);
		item->permissions = details.permissions;
		item->size = details.size;
		item->direction = DIRECTION_FXP;
		if (path[0] == '/') {
			strncpy(item->remotedir, path, 256);
		} else {
			sprintf(item->remotedir, "%s%s%s", LAST_CONNECTION->remotedir, (LAST_CONNECTION->remotedir[strlen(LAST_CONNECTION->remotedir)-1] != '/') ? "/" : "", path);
		}
		strncpy(item->localdir, CURRENT_CONNECTION->remotedir, 256);
		if (item->remotedir[strlen(item->remotedir) - 1] == '/')
			item->remotedir[strlen(item->remotedir) - 1] = '\0';
		if (item->localdir[strlen(item->localdir) - 1] == '/')
			item->localdir[strlen(item->localdir) - 1] = '\0';
		item->remoteconnection = LAST_CONNECTION;
		item->localconnection = CURRENT_CONNECTION;
		newitem = addtoqueue(&leftqueue, item);
		if (newitem) log(LOG_INFO, "Queued %s/%s for FxP, position %d\n",
			item->remotedir, item->localfilename, newitem->id);
		free(item);
	}
	if (config(variables, "queuefiles")) {
		if (strcasecmp(config(variables, "queuefiles"), "false") == 0)
			cmd_go(expanded_line);
	}
	free(expanded_line);
	return 1;
}

int cmd_go (char *command_line) {
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "go: Not connected\n");
		return 0;
	}
	leftqueue = runqueue(leftqueue);
	return 1;
}

int cmd_clear (char *command_line) {
	clearqueue(leftqueue);
	leftqueue = NULL;
	log(LOG_ALERT, "Queue cleared\n");
	return 1;
}

int cmd_queue (char *command_line) {
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "queue: Not connected\n");
		return 0;
	}
	printqueue(leftqueue);
	return 1;
}

int cmd_rem (char *command_line) {
	char *param;
	int id;
	getnextword(command_line, GNW_START);
	while ((param = getnextword(command_line, GNW_NEXT))) {
		id = atoi(param);
		if (id > 0) {
			if (delfromqueue(&leftqueue, id) == 0) {
				log(LOG_WARNING, "Deleted item %d\n", id);
				return 1;
			} else {
				log(LOG_ALERT, "Item %d doesn't exist!\n", id);
				return 0;
			}
		}
	}
	return 1;
}

int cmd_quit (char *command_line) {
	printf("\n");
	return -1;
}

int cmd_close (char *command_line) {
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "close: Not connected\n");
		return 0;
	}
	if (leftqueue) {
		log(LOG_ALERT, "You still have untransferred items\n");
		return 0;
	}
	ftp_disconnect(CURRENT_CONNECTION);
	CURRENT_CONNECTION->status = STATUS_WAIT_CONNECT;
	return 1;
}

int cmd_open(char *command_line) {
	char *param;
	getnextword(command_line, GNW_START);
	if (!(param = getnextword(command_line, GNW_NEXT))) {
		log(LOG_ALERT, "No address specified!\n");
		return 0;
	}
	if (!*param) return 0;
	if (!parseurl(param, CURRENT_CONNECTION)) {
		log(LOG_ALERT, "Can't open %s\n", param);
		return 0;
	}
	if (strlen(CURRENT_CONNECTION->password) == 0) {
		if (config(variables, "email"))
			strncpy(CURRENT_CONNECTION->password, config(variables, "email"), 32);
		else
			strncpy(CURRENT_CONNECTION->password, "user@quftp.com", 32);
	}
	if (CURRENT_CONNECTION->remotedir[0] != '\0') getconnected(CURRENT_CONNECTION);
	setconfig(&variables, "HOSTNAME", CURRENT_CONNECTION->hostname);
	return 1;
}

int cmd_user (char *command_line) {
	char *param;
	getnextword(command_line, GNW_START);
	if (!(param = getnextword(command_line, GNW_NEXT))) {
		log(LOG_ALERT, "No username specified!\n");
		return 0;
	}
	if (CURRENT_CONNECTION->status >= STATUS_IDLE) {
		ftp_disconnect(CURRENT_CONNECTION);
	}
	strncpy(CURRENT_CONNECTION->username, param, 32);
	if ((param = getnextword(command_line, GNW_NEXT))) {
		strncpy(CURRENT_CONNECTION->password, param, 32);
	} else {
		strncpy(CURRENT_CONNECTION->password, getpassword(CURRENT_CONNECTION->username), 32);
	}
	CURRENT_CONNECTION->status = STATUS_WAIT_CONNECT;
	return 1;
}

int cmd_stat(char *command_line) {
	struct filedetails details;
	char *param;
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "stat: Not connected\n");
		return 0;
	}
	getnextword(command_line, GNW_START);
	while ((param = getnextword(command_line, GNW_NEXT))) {
		if (!*param) continue;
		if (ftp_stat(CURRENT_CONNECTION, param, &details) == 0) {
			log(LOG_INFO, "Details for %s\n", details.filename);
			log(LOG_INFO, "File Size: %lu\n", details.size);
			log(LOG_INFO, "Number of Hard Links: %d\n", details.links);
			if (*details.owner) log(LOG_INFO, "Owner: %s\n", (char *)details.owner);
			if (*details.group) log(LOG_INFO, "Group: %s\n", (char *)details.group);
			if (details.permissions) log(LOG_INFO, "Permission: %o\n", (unsigned int)details.permissions);
		}
	}
	return 1;
}

int cmd_status (char *command_line) {
	int conn;
	log(LOG_INFO, "Current connection: %s\n", config(variables, "CURRENT_CONNECTION"));
	log(LOG_INFO, "\n");
	for (conn = 0; conn < MAX_CONNECTIONS; conn++) {
		if (connection[conn]->status == 0) continue;
		log(LOG_INFO, "Connection %d:\n", conn);
		log(LOG_INFO, "	Host: %s:%d\n", connection[conn]->hostname, connection[conn]->port);
		log(LOG_INFO, "	Username: %s\n", connection[conn]->username);
		log(LOG_INFO, "	Directory: %s\n", connection[conn]->remotedir);
		log(LOG_INFO, "	Status: ");
		switch(connection[conn]->status) {
			case 0 : log(LOG_INFO, "No connection\n"); break;
			case 1 : log(LOG_INFO, "Ready to connect\n"); break;
			case 2 : log(LOG_INFO, "Waiting for login\n"); break;
			case 3 : log(LOG_INFO, "Idle\n"); break;
			case 4 : log(LOG_INFO, "Waiting...\n"); break;
			case 5 : log(LOG_INFO, "Busy transferring\n"); break;
			case 99 : log(LOG_INFO, "Error!\n"); break;
			default : log(LOG_INFO, "Unknown!\n"); break;
		}
	}
	return 1;
}

int cmd_nlst(char *command_line) {
	char *list[200];
	char *params = NULL;
	register int index = 0;
	getnextword(command_line, GNW_START);
	params = getnextword(command_line, GNW_REST);
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "nlst: Not connected\n");
		return 0;
	}
	if (ftp_nlst(CURRENT_CONNECTION, params, list, 200) < 0) return 0;
	for (index = 0; list[index]; index++) {
		printf("%s\n", list[index]);
		free(list[index]);
	}
	return 1;
}

int cmd_lls(char *command_line) {
	char *param = command_line;
	param++;
	system(param);
	return 1;
}

int cmd_cat(char *command_line) {
	char *param = (char *)NULL;
	char *expanded_line;
	expanded_line = expand_wildcard(command_line);
	param = getnextword(expanded_line, GNW_START);
	while ((param = getnextword(expanded_line, GNW_NEXT))) {
		char *buffer;
		int n;
		buffer = (char *)malloc(4096);
		sprintf(buffer, "%s", param);
		if (ftp_read_file(CURRENT_CONNECTION, buffer, 4095, TRANS_START) < 0) {
			free(expanded_line);
			return 0;
		}
		while ((n = ftp_read_file(CURRENT_CONNECTION, buffer, 4095, TRANS_TRANS)) > 0) {
			buffer[4095] = '\0';
			printf("%s", buffer);
		}
		ftp_read_file(CURRENT_CONNECTION, buffer, 4095, TRANS_CLOSE);
		free(buffer);
	}
	free(expanded_line);
	return 1;
}

int cmd_more(char *command_line) {
	char *param = (char *)NULL;
	char *expanded_line;
	FILE *fh;
	expanded_line = expand_wildcard(command_line);
	param = getnextword(expanded_line, GNW_START);
	while ((param = getnextword(expanded_line, GNW_NEXT))) {
		char *pager;
		char *buffer;
		int n;
		if (ftp_read_file(CURRENT_CONNECTION, param, 4095, TRANS_START) < 0) {
			free(expanded_line);
			return 0;
		}
		pager = config(variables, "pager");
		if (!pager) pager = strdup("/bin/more");
		fh = popen(pager, "w");
		if (!fh) {
			perror("popen");
			ftp_read_file(CURRENT_CONNECTION, NULL, 0, TRANS_CLOSE);
			free(expanded_line);
			return 0;
		}
		buffer = (char *)malloc(4096);
		while ((n = ftp_read_file(CURRENT_CONNECTION, buffer, 4095, TRANS_TRANS)) > 0) {
			buffer[4095] = '\0';
			fputs(buffer, fh);
		}
		ftp_read_file(CURRENT_CONNECTION, buffer, 4095, TRANS_CLOSE);
		free(buffer);
		pclose(fh);
	}
	free(expanded_line);
	return 1;
}

int cmd_set(char *command_line) {
	char *key = NULL, *value;
	value = getnextword(command_line, GNW_START);
	value = getnextword(command_line, GNW_NEXT);
	if (value) {
		key = strdup(value);
		value = getnextword(command_line, GNW_NEXT);
		if (value) {
			setconfig(&variables, strlower(key), value);
		} else {
			char *data = config(variables, key);
			if (data) printf("%s: %s\n", key, data);
			else log(LOG_ALERT, "%s does not exist\n", key);
		}
	} else {
		struct variable_list *list = variables;
		while (list) {
			printf("%s: %s\n", list->key, list->value);
			list = list->next;
		}
	}
	if (key) free(key);
	return 1;
}

int cmd_unset(char *command_line) {
	char *key = NULL;
	getnextword(command_line, GNW_START);
	key = strlower(getnextword(command_line, GNW_NEXT));
	if (key) {
		setconfig(&variables, key, NULL);
		free(key);
		return 1;
	}
	return 0;
}

int cmd_assert(char *command_line) {
	char *result;
	result = config(variables, "SUCCESSFUL_OPERATION");
	if (result && strcmp(result, "false") == 0) {
		log(LOG_CRITICAL, "Assertion failed... quitting\n");
		return -1;
	}
	return 1;
}

char *parseline(char *line) { /* Expand variables */
	int index = 0;
	char *linestart = line;
	char *temp;
	temp = (char *)malloc(4096);
	memset(temp, 0, 4096);
	while (*line) {
		if (*line == '$') {		// Expand variables
			char *endbit;
			line++;
			endbit = line;
			while (endbit && *endbit && *endbit != ' ') endbit++;
			*endbit = '\0';
			if (config(variables, line)) strcat((temp + index), config(variables, line));
			index += (endbit - line);
			line = endbit + 1;
		} else {
			temp[index] = *line;
		}
		line++;
		if (line > (linestart + strlen(linestart))) break;
		index++;
	}
	return temp;
}

int cmd_dele(char *command_line) {
	char *filename, *expanded_line;
	expanded_line = expand_wildcard(command_line);
	getnextword(expanded_line, GNW_START);
	while ((filename = getnextword(expanded_line, GNW_NEXT))) {
		ftp_delete(CURRENT_CONNECTION, filename);
	}
	free(expanded_line);
	return 1;
}

int cmd_alias(char *command_line) {
	struct alias_list *item = aliases, *last = aliases;
	char *alias = NULL, *expanded = NULL;
	getnextword(command_line, GNW_START);
	expanded = getnextword(command_line, GNW_NEXT);
	if (expanded) {
		alias = strdup(expanded);
	}
	expanded = getnextword(command_line, GNW_REST);
	if (!alias || !*alias) {
		if (!item) {
			log(LOG_ALERT, "No aliases have been defined\n");
			return 1;
		}
		printf("%-20s%s\n", "Alias", "Expands to");
		while (item) {
			printf("%-20s%s\n", item->alias, item->expanded);
			item = item->next;
		}
		printf("\n");
		return 1;
	}
	if (!expanded || !*expanded) {
		while (item) {
			if (strcasecmp(item->alias, alias) == 0) {
				free(item->expanded);
				free(item->alias);
				last->next = item->next;
				if (item == aliases) aliases = NULL;
				free(item);
				return 1;
			}
			last = item;
			item = item->next;
		}
		log(LOG_ALERT, "Can't find \"%s\"\n", alias);
		return 0;
	}
	while (item) {
		if (strcasecmp(item->alias, alias) == 0) {
			free(item->expanded);
			item->expanded = strdup(expanded);
			item->next = NULL;
			return 1;
		}
		last = item;
		item = item->next;
	}
	item = (struct alias_list *)malloc(sizeof(struct alias_list));
	if (!aliases) aliases = item;
	if (last) last->next = item;
	item->alias = strdup(alias);
	item->expanded = strdup(expanded);
	item->next = NULL;
	return 1;
}

char *expand_wildcard(char *inputstring) {
	char *word, *tempstring, *retstring;

	if (!inputstring) return NULL;
	if ((strstr(inputstring, "*") == 0) && (strstr(inputstring, "?")) == 0)
		return strdup(inputstring);
	tempstring = (char *)malloc(65535);
	sprintf(tempstring, "%s ", getnextword(inputstring, GNW_START));
	if (strcasecmp(tempstring, "get ") == 0 || strcasecmp(tempstring, "fxp ") == 0) {
		while ((word = getnextword(inputstring, GNW_NEXT))) {
			int index = 0;
			char *temp;
			while ((temp = ftp_glob(CURRENT_CONNECTION, word, index++))) {
				sprintf(tempstring, "%s %s", tempstring, temp);
			}
		}
		retstring = strdup(tempstring);
		free(tempstring);
		return retstring;
	} else if (strcasecmp(tempstring, "put ") == 0) {
		glob_t globvec;
		sprintf(tempstring, "%s ", getnextword(inputstring, GNW_START));
		while ((word = getnextword(inputstring, GNW_NEXT))) {
			int result, index;
			memset(&globvec, 0, sizeof(glob_t));
			result = glob(word, GLOB_TILDE, NULL, &globvec);
			switch (result) {
				case 0: for (index = 0; index < globvec.gl_pathc; index++)
						sprintf(tempstring, "%s %s", tempstring,
							globvec.gl_pathv[index]);
					globfree(&globvec);
					break;
				case GLOB_NOSPACE: log(LOG_CRITICAL, "Ran out of memory for glob\n");
						   free(tempstring);
						   return strdup(inputstring);
				case GLOB_ABORTED: log(LOG_CRITICAL, "Glob Read error\n");
						   free(tempstring);
						   return strdup(inputstring);
				case GLOB_NOMATCH: log(LOG_ALERT, "Glob found no matches\n");
						   free(tempstring);
						   return strdup(inputstring);
				default : log(LOG_ALERT, "Unknown glob error %d\n", result);
					  break;
			}
		}
		retstring = strdup(tempstring);
		free(tempstring);
		return retstring;
	} else {
		free(tempstring);
		return strdup(inputstring);
	}
}

char *expand_alias(char *string) {
	struct alias_list *item;
	char *returnstring = (char *)NULL;
	char firstword[500], *rest;
	if (!string || !*string) return string;
	memset(firstword, 0, 500);
	strncpy(firstword, getnextword(string, GNW_START), 500);
	rest = getnextword(string, GNW_REST);
	item = aliases;
	while (item) {
		if (strcasecmp(item->alias, firstword) == 0) {
			returnstring = (char *)malloc(strlen(item->expanded) + strlen(rest) + 2);
			sprintf(returnstring, "%s %s", item->expanded, rest);
			return returnstring;
		}
		item = item->next;
	}
	return strdup(string);
}

char *print_permissions(int permissions) {
	char *returnstring;
	returnstring = (char *)malloc(11);
	memset(returnstring, '-', 10);
	if ((permissions & 512) == 512) returnstring[0] = 'd';
	if ((permissions & 256) == 256) returnstring[1] = 'r';
	if ((permissions & 128) == 128) returnstring[2] = 'w';
	if ((permissions & 64) == 64) returnstring[3] = 'x';
	if ((permissions & 32) == 32) returnstring[4] = 'r';
	if ((permissions & 16) == 16) returnstring[5] = 'w';
	if ((permissions & 8) == 8) returnstring[6] = 'x';
	if ((permissions & 4) == 4) returnstring[7] = 'r';
	if ((permissions & 2) == 2) returnstring[8] = 'w';
	if ((permissions & 1) == 1) returnstring[9] = 'x';
	return returnstring;
}

int progress_bar(unsigned long current, unsigned long max, double speed) {
	unsigned long etasec;
	int index, width, count = 0;
	double percent;
	static double oldpercent = -1;
	static double speeds[SPEEDS_AVERAGE];
	static int speedindex = 0;
	char *temp;

	/* Don't display a progress bar for a 0 byte file */
	if (max == 0) {
		return 0;
	}

	/* If a new progress bar, clear the speed history */
	if (current == 0) {
		for (index = 0; index < SPEEDS_AVERAGE; index++) speeds[index] = 0.0;
		speedindex = 0;
	}

	/* Record current speed */
	speeds[++speedindex % SPEEDS_AVERAGE] = speed;

	/* Get current average speed */
	for (index = 0; index < SPEEDS_AVERAGE; index++) {
		if (speeds[index] == 0.0) continue;
		speed += speeds[index];
		count++;
	}
	speed /= (count * 1.0);
	percent = (current * 1.0) / (max * 1.0);

	/* No movement? Return without displaying anything */
	if (percent == oldpercent) return 0;

	etasec = ((max - current) / 1024.00) / speed;
	if ((temp = config(variables, "screenwidth"))) width = atoi(temp);
	else if ((temp = getenv("COLUMNS"))) width = atoi(temp);
	else width = 80;
	width -= 20;
	for (index = 0; index <= (width * percent); index++) printf("#");
	for (index = (width * percent) + 1; index <= width; index++) printf("-");
	printf(" %-6.6s %3.1fKb/s", timestring(etasec), speed);
	printf("\r");
	if (current == max) {
		for (index = 0; index < (width + 20); index++) printf(" ");
		printf("\r");
	}
	oldpercent = percent;
	return 0;
}

char *timestring(unsigned long seconds) {
	char *returnstring;
	returnstring = (char *)malloc(15);
	sprintf(returnstring, "%02lu:%02lu", seconds / 60, seconds % 60);
	return returnstring;
}

int cmd_getlist(char *command_line) {
	char *infile = NULL;
	char line[MAX_LINE_SIZE];
	FILE *fh;
	getnextword(command_line, GNW_START);
	infile = getnextword(command_line, GNW_NEXT);
	fh = fopen(infile, "r");
	if (!fh) {
		log(LOG_CRITICAL, "Can't open file %s\n", infile);
		return 0;
	}
	while (fgets(line, MAX_LINE_SIZE, fh)) {
		char path[MAX_LINE_SIZE], filename[MAX_LINE_SIZE], *param;
		int index;
		int result;
		struct qitem *item = NULL, *newitem;
		struct filedetails details;
		param = strchr(line, '\n');
		*param = '\0';
		if (line[0] == 0) continue;
		param = getnextword(line, GNW_START);
		result = ftp_stat(CURRENT_CONNECTION, param, &details);
		switch (result) {
			case 211 :
			case 212 : log(LOG_ALERT, "%s does not exist\n", param);
				   continue;
				   break;
			case 500 :
			case 501 :
			case 502 : break;
			case 421 :
			default  : continue;
				   break;
		}
		item = (struct qitem *)malloc(sizeof(struct qitem));
		memset(item, 0, sizeof(struct qitem));
		memset(path, 0, MAX_LINE_SIZE);
		memset(filename, 0, MAX_LINE_SIZE);
		/* Split the path and filename from the paramater */
		index = strlen(param);
		while (index) {
			if (param[index] == '/') {
				strncpy(path, param, index);
				strncpy(filename, &param[index + 1], MAX_LINE_SIZE);
				break;
			}
			index--;
		}
		if (filename[0] == 0) strncpy(filename, param, MAX_LINE_SIZE);
		strncpy(item->localfilename, filename, 256);
		strncpy(item->remotefilename, filename, 256);
		item->permissions = details.permissions;
		item->size = details.size;
		item->direction = DIRECTION_GET;
		if (path[0] == '/') {
			strncpy(item->remotedir, path, 256);
		} else {
			sprintf(item->remotedir, "%s%s%s", CURRENT_CONNECTION->remotedir,
				(CURRENT_CONNECTION->remotedir[strlen(CURRENT_CONNECTION->remotedir)-1] != '/') ? "/" : "",
				path);
		}
		strncpy(item->localdir, CURRENT_CONNECTION->localdir, 256);
		if (item->remotedir[strlen(item->remotedir) - 1] == '/')
			item->remotedir[strlen(item->remotedir) - 1] = '\0';
		if (item->localdir[strlen(item->localdir) - 1] == '/')
			item->localdir[strlen(item->localdir) - 1] = '\0';
		item->remoteconnection = CURRENT_CONNECTION;
		item->localconnection = NULL;
		newitem = addtoqueue(&leftqueue, item);
		if (newitem) log(LOG_INFO, "Queued %s/%s for download, position %d\n",
			item->remotedir, item->localfilename, newitem->id);
		free(item);
	}
	fclose(fh);
	return 1;
}

int cmd_quote(char *command_line) {
	char command[4096];
	sprintf(command, "%s\r\n", getnextword(command_line, GNW_REST));
	ftp_sendline(CURRENT_CONNECTION, command);
	ftp_getrc(CURRENT_CONNECTION, NULL, 0, 1);
	return 1;
}

int cmd_mkdir(char *command_line) {
	char *param;
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "mkdir: Not connected\n");
		return 0;
	}
	getnextword(command_line, GNW_START);
	param = getnextword(command_line, GNW_NEXT);
	if (!param || !*param) {
		log(LOG_ALERT, "Must specify a directory to create\n");
		return 0;
	} else {
		if (!ftp_mkd(CURRENT_CONNECTION, param)) {
			SYSLOG tlog;
			tlog.opcode = SYSLOG_NETFAIL_FTPC_MKD;
			tlog.ip = CURRENT_CONNECTION->hostip;
			SetSysLog(&tlog);
			return 0;
		}
		else log(LOG_INFO, "Created directory %s on %s in %s\n", param, CURRENT_CONNECTION->hostname,
			CURRENT_CONNECTION->remotedir);
	}
	return 1;
}

int cmd_rmdir(char *command_line) {
	char *param;
	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "rmdir: Not connected\n");
		return 0;
	}
	getnextword(command_line, GNW_START);
	param = getnextword(command_line, GNW_NEXT);
	if (!param || !*param) {
		log(LOG_ALERT, "Must specify a directory to remove\n");
		return 0;
	} else {
		if (!ftp_rmd(CURRENT_CONNECTION, param)) return 0;
		else log(LOG_INFO, "Removed directory %s on %s in %s\n", param, CURRENT_CONNECTION->hostname,
			CURRENT_CONNECTION->remotedir);
	}
	return 1;
}

int cmd_ujpeg(char *command_line) {  //add by mikewang
	struct ftpconnection *remoteconnection;
	int response,ret;
	char *param1,*buffer,*filename,line[MAX_LINE_SIZE];
	unsigned long file_size = 0;
	//unsigned long file_size,index = 0,n = 0,temp;
	AV_DATA av_data;
	//static int last_serial = -1;
	//struct timeval curtime;
	int prealarm_t;
	int err;
	//int tmp_buf_size , req_preframe;
	//static int prealarm_frm = 0;
	//int diff_time;
	getnextword(command_line, GNW_START);
	param1 = getnextword(command_line, GNW_NEXT);
	if(!param1){
		DBG("argument 1 not assigned!\n");
		return 1;
	}	
	prealarm_t = atoi(param1);

	filename = getnextword(command_line, GNW_NEXT);
	if(!filename){
		DBG("argument 2 not assigned!\n");
		return 1;
	}
	//filename[strlen(filename) + 1 ] = '\0';

	if (!getconnected(CURRENT_CONNECTION)) {
		log(LOG_ALERT, "ujpeg: Not connected\n");
		return 0;
	}
	remoteconnection = CURRENT_CONNECTION;

	/*
	if( GetAVData(JPEG_FIELD[GET_MJPEG_SERIAL][jpg_sel], -1, &av_data ) != RET_SUCCESS) {
		printf("Error on GET_MJPEG_SERIAL\n");
		return -1;
	}
	dbg("last_serial:%d\n", last_serial);
	while(last_serial == av_data.serial){
		usleep(5);
		GetAVData(JPEG_FIELD[GET_MJPEG_SERIAL][jpg_sel], -1, &av_data );
	}
	last_serial = av_data.serial;
	*/ // Disable by Alex. 2010.01.09
	
//printf("debug in %s %d , jpg_sel = %d\n",__func__,__LINE__,jpg_sel);
#if 0	//prealarm
	if( pSysInfo->profile_config.profile_codec_fmt[jpg_sel] == SEND_FMT_JPEG ){
		req_preframe = prealarm_t * pSysInfo->buf_info.fps[jpg_sel];
		
		if( prealarm_frm == 0 ){
			tmp_buf_size = (av_data.size/(10*1024));	//10k is jpeg block size
			
			if( av_data.size % (10*1024) != 0)
				tmp_buf_size++;
			tmp_buf_size *= 10*1024;
			
			prealarm_frm = pSysInfo->buf_info.jpg_buf_size[jpg_sel] / tmp_buf_size - 5;
			if( req_preframe <= prealarm_frm )
				prealarm_frm = req_preframe;
			
			if( jpg_sel == 1 )
				prealarm_frm /= 2;
			
			if( (av_data.serial - prealarm_frm) >= 0 )
				av_data.serial -= prealarm_frm;	//prealarm_frm: max prealarm frame(s)
				
			if( av_data.serial < 0 ) return -1;
			prealarm_frm = av_data.serial;
		}
		else {
			prealarm_frm += pSysInfo->buf_info.fps[jpg_sel];
			if( prealarm_frm <= av_data.serial )
				av_data.serial = prealarm_frm;
		}
	}
	else {
		av_data.serial -= prealarm_t;
	}
#endif
//printf("debug in %s %d , av_data.serial = %d , prealarm_t = %d , jpg_sel = %d , getavdata = 0x%x\n",__func__,__LINE__,av_data.serial,prealarm_t,jpg_sel,JPEG_OPT[LOCK_MJPEG][jpg_sel]);
	//sleep(1);
	 if( (err = GetAVData(jpg_sel|AV_FMT_MJPEG|AV_OP_GET_SERIAL, -1, &av_data)) != RET_SUCCESS) {
        DBG("Error on GET_MJPEG_SERIAL , img_src = %d, ERRNO = %d", jpg_sel, err);
        return -1;
    }

	 int timeout=0;
	 while(1)
	{
		timeout++;
		if(timeout > 5) {
			timeout = 0;
			return -1;
		}
		
		if( GetAVData(jpg_sel|AV_FMT_MJPEG|AV_OP_LOCK, prealarm_t , &av_data ) != RET_SUCCESS )
		{
			prealarm_t += av_data.framerate;
			dbg("Error on LOCK_MJPEG , serial = %d ",prealarm_t);
			continue;
		}
 
		if( GetAVData(jpg_sel|AV_FMT_MJPEG|AV_OP_UNLOCK, prealarm_t, NULL) != RET_SUCCESS)
		{
			DBG("Error on AV_FMT_MJPEG ");
			//return -1;
		}
	
		break;
	}

	//dbg("prealarm_t = %d, av_data.framerate=%4.2f",  prealarm_t, av_data.framerate);
	ret = GetAVData(jpg_sel|AV_FMT_MJPEG|AV_OP_LOCK, prealarm_t, &av_data );
	if(ret != RET_SUCCESS){
		printf("Error : %d\n", ret);
		return ret;
	}
	
	buffer = malloc( av_data.size );
	memcpy( buffer , av_data.ptr , av_data.size );

	if( GetAVData(jpg_sel|AV_FMT_MJPEG|AV_OP_UNLOCK, prealarm_t, &av_data) != RET_SUCCESS) {
		printf("Error on AV_OP_UNLOCK_JPEG\n");
		return -1;
	}
	if( buffer == NULL ) return -1;

	file_size = av_data.size;


	if(ftp_sendline(remoteconnection, "TYPE I\r\n") < 0) {
		SYSLOG tlog;
		
		tlog.opcode = SYSLOG_NETFAIL_FTPC_TYPE;
		tlog.ip = remoteconnection->hostip;
		SetSysLog(&tlog);
	}
	ftp_getrc(remoteconnection, NULL, 0, 0);

	sprintf(line, "ALLO %lu\r\n", file_size);
	ftp_sendline(remoteconnection, line);
	response = ftp_getrc(remoteconnection, NULL, 0, 0);
	switch (response) {
		case 200 :
		case 202 : break;
		case 502 :
		case 504 : log(LOG_ALERT, "ALLO command does not implement parameter\r");
			   break;
		case 500 : log(LOG_ALERT, "Server doesn't understand ALLO command\r");
			   break;
		case 501 : log(LOG_ALERT, "Syntax error in ALLO command\n");
			   break;
		case 421 : log(LOG_ALERT, "Server shutting down\n");
			   ftp_close_data(remoteconnection);
			   ftp_disconnect(remoteconnection);
			   return -1;
		case 530 : log(LOG_ALERT, "Server says not logged in!\n");
			   ftp_close_data(remoteconnection);
			   ftp_disconnect(remoteconnection);
			   return -1;
		default  : log(LOG_ALERT, "ALLO: Unrecognised error %d\n", response);
			   ftp_close_data(remoteconnection);
			   ftp_disconnect(remoteconnection);
			   break;
	}

	if( mode )	//passive mode
		ftp_open_data_pasv( remoteconnection , NULL , 0 );
	else	//active mode
		ftp_open_data(remoteconnection);		

	sprintf(line, "STOR %s\r\n", filename);
	ftp_sendline(remoteconnection, line);
	response = ftp_getrc(remoteconnection, NULL, 0, 0);
	switch (response) {
		case 125 :
		case 150 : if (remoteconnection->status == STATUS_WAITING)
				   if (!ftp_wait_data(remoteconnection)) {
					   ftp_close_data(remoteconnection);
					   return -response;
				   }
				   break;
		case 532 : log(LOG_ALERT, "Server requires an account.. not supported yet\n");
			   ftp_close_data(remoteconnection);
			   return -response;
			   break;
		case 450 : log(LOG_ALERT, "Remote file is busy\n");
			   ftp_close_data(remoteconnection);
			   return -response;
			   break;
		case 452 : log(LOG_ALERT, "Insufficient storage space\n");
			   ftp_close_data(remoteconnection);
			   return -response;
			   break;
		case 553 : log(LOG_ALERT, "Permission denied to upload \"%s\"\n", filename);
			   ftp_close_data(remoteconnection);
			   return -response;
			   break;
		case 500 : log(LOG_ALERT, "Server doesn't understand STOR command\n");
			   ftp_close_data(remoteconnection);
			   return -response;
			   break;
		case 501 : log(LOG_ALERT, "Syntax error in STOR command\n");
			   ftp_close_data(remoteconnection);
			   return -response;
			   break;
		case 421 : log(LOG_ALERT, "Server shutting down\n");
			   ftp_close_data(remoteconnection);
			   ftp_disconnect(remoteconnection);
			   return -response;
			   break;
		case 530 : log(LOG_ALERT, "Server says not logged in!\n");
			   ftp_close_data(remoteconnection);
			   ftp_disconnect(remoteconnection);
			   return -response;
			   break;
		default  : log(LOG_ALERT, "Unrecognised error %d\n", response);
			   ftp_close_data(remoteconnection);
			   ftp_disconnect(remoteconnection);
			   break;
	}

	ftp_wait_data(remoteconnection);
	ftp_write_data(remoteconnection, buffer, av_data.size );

	ftp_close_data(remoteconnection);
	ftp_getrc(remoteconnection, NULL, 0, 0);

	if( buffer ) free( buffer );
	return 1;
}

int cmd_test(char *command_line) {

	int response;
	char line[MAX_LINE_SIZE];
	char str[] = "The Result of Server Test of Your IP Camera.";
	struct ftpconnection *remoteconnection;

	snprintf(line, sizeof(line), "open %s\n", serverIP);
	if(cmd_open(line) == 0)
		return 0;
	
	snprintf(line, sizeof(line), "user %s %s", user, password);
	if(cmd_user(line) == 0)
		return 0;
	
	if(strlen(folderName) > 0) {
		snprintf(line, sizeof(line), "cd \"%s\"", folderName);
		if(!cmd_cd(line)) {
			snprintf(line, sizeof(line), "mkdir \"%s\"", folderName);
			if(!cmd_mkdir(line)) {
				return 0;
			}
			else {
				snprintf(line, sizeof(line), "cd \"%s\"", folderName);
				if(!cmd_cd(line))
					return 0;
			}
		}
	}
	
	if (!getconnected(CURRENT_CONNECTION))
		return 0;
	remoteconnection = CURRENT_CONNECTION;

	if( mode )	//passive mode
		ftp_open_data_pasv( remoteconnection , NULL , 0 );
	else		//active mode
		ftp_open_data(remoteconnection);
	
	sprintf(line, "STOR %s\r\n", "test.txt");
	ftp_sendline(remoteconnection, line);
	response = ftp_getrc(remoteconnection, NULL, 0, 0);
	if(response == 150 || response == 125) {
		if (remoteconnection->status == STATUS_WAITING) {
			if (!ftp_wait_data(remoteconnection)) {
				ftp_close_data(remoteconnection);
				return -response;
			}
		}
	}
	else {
		dbg("responce = %d", response);
		return 0;
	}
	
	ftp_write_data(remoteconnection, str, strlen(str));
	ftp_close_data(remoteconnection);

	pSysInfo->testserver = TEST_SERVER_SUCESS;
	dbg("TEST FTP SERVER %s DONE", serverIP);
	return 1;
}

int cmd_alarm(char *command_line) {  //add by mikewang
	char line[MAX_LINE_SIZE];
	//char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
	int x = postalarm;
	//unsigned int count = 0;
	time_t timep;
	struct timeval tv;
	struct tm *p;

	if(test_mode == 1) {
		if(!cmd_test(""))
			pSysInfo->testserver = TEST_SERVER_FAIL;
		return 1;
	}

	sprintf(line,"open %s\n",serverIP);
	cmd_open(line);

	sprintf(line,"user %s %s",user,password);
	if(cmd_user(line) == 0)
		return 0;

	if(strlen(folderName) > 0) {
		sprintf(line,"cd \"%s\"",folderName);
		if( cmd_cd(line) == 0 ){
			sprintf(line,"mkdir \"%s\"",folderName);
			if(cmd_mkdir(line) == 0) {
				cmd_quit("");
				return 0;
			}
			else {
				sprintf(line,"cd \"%s\"",folderName);
				if( cmd_cd(line) == 0 ){
					cmd_quit("");
					return 0;
				}
			}
		}
	}

	sprintf(line,"lcd %s", ufilepath);
	if( cmd_lcd(line) == 0 )
		cmd_quit("");
	//if( prealarm ) x++;
	
	if( strlen(ufileName)== 0 ) {

		//dbg("profile_codec_fmt[0] = %d, [1] = %d, [2] = %d", pSysInfo->profile_codec_fmt[0], pSysInfo->profile_codec_fmt[1], pSysInfo->profile_codec_fmt[2]);
		//dbg("profile_codec_idx[0] = %d, [1] = %d, [2] = %d", pSysInfo->profile_codec_idx[0], pSysInfo->profile_codec_idx[1], pSysInfo->profile_codec_idx[2]);
		//dbg("profile_jpeg_idx[0] = %d, [1] = %d, [2] = %d", pSysInfo->profile_jpeg_idx[0], pSysInfo->profile_jpeg_idx[1], pSysInfo->profile_jpeg_idx[2]);

		x = prealarm + postalarm;
		jpg_sel = pSysInfo->profile_config.profile_jpeg_idx[jpg_sel - 1];
		//dbg("jpg_sel = %d", jpg_sel);

		int fps, err = 0;
		int last_serial = -1, SerialBook = -1;
		AV_DATA tAVData;
		
		/*if( (fps = getCurrentFrameRate(jpg_sel)) < 0) {
		    DBG("ERROR ON GET PROFILE %d FPS", jpg_sel + 1);
		    fps = 1;
		}*/
	
		if( (err = GetAVData(jpg_sel|AV_FMT_MJPEG|AV_OP_GET_SERIAL, -1 , &tAVData)) != RET_SUCCESS ) {
		    DBG("Error on GET_MJPEG_SERIAL , jpg_sel = %d, ERRNO = %d", jpg_sel, err);
		    cmd_quit("");
		    return -1;
		}
		#if 1	//add by ryan0988	20110527
		fps = 1;
		#else
		fps = tAVData.framerate;
		#endif
		SerialBook = tAVData.serial;
		 if(SerialBook - prealarm * fps > 0 )
		     SerialBook -= prealarm;
		
		while( x > 0 ){
#ifdef SUPPORT_EVENT_2G
			//char name[64];
			//static int i = 0;
			time( &timep );    
			gettimeofday(&tv,NULL);
			p = localtime( &timep );
			//dbg ("Jpg File count = %d \n",count);

			usleep(10000);
			err = GetAVData(jpg_sel|AV_FMT_MJPEG|AV_OP_GET_SERIAL, -1, &tAVData);
			if(err == RET_NO_VALID_DATA) {
			    DBG("GET LATEST MJPEG GET_MJPEG_SERIAL , jpg_sel = %d, RET_NO_VALID_DATA", jpg_sel);
			    continue;
			}
			else if(err < 0) {
			    DBG("Error on GET LATEST MJPEG GET_MJPEG_SERIAL , jpg_sel = %d, ERRNO = %d", jpg_sel, err);
			    break;
			}
			
			if(SerialBook > tAVData.serial)
			    continue;

			last_serial = SerialBook;
			SerialBook += fps;

			if(name_with_date) {
				if(strlen(jpeg_name) > 0)
					sprintf(line,"ujpeg %d \"%s%d%02d%02d_%02d%02d%02d_%.4ld.jpg\"", last_serial , jpeg_name, (1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour,p->tm_min,p->tm_sec,tv.tv_usec >> 7);
				else
					sprintf(line,"ujpeg %d %d%02d%02d_%02d%02d%02d_%.4ld.jpg", last_serial , (1900+p->tm_year),(1+p->tm_mon),p->tm_mday,p->tm_hour,p->tm_min,p->tm_sec,tv.tv_usec >> 7);
			}
			else {
				if(strlen(jpeg_name) > 0)
					sprintf(line,"ujpeg %d %s%ld_%.4ld.jpg", last_serial , jpeg_name, pSysInfo->second, tv.tv_usec >> 7);
				else
					sprintf(line,"ujpeg %d %ld_%.4ld.jpg", last_serial , pSysInfo->second, tv.tv_usec >> 7);
			}
			
#else
			time( &timep );
			gettimeofday(&tv,NULL);
			p = localtime( &timep );
			sprintf(line,"ujpeg %d dms_%d_%02d_%02d_%s_%02d_%02d_%02d_%02ld.jpg", last_serial ,(1900+p->tm_year),(1+p->tm_mon),p->tm_mday,wday[p->tm_wday],p->tm_hour,p->tm_min,p->tm_sec,tv.tv_usec/10000);
#endif
			if( cmd_ujpeg(line) != 1 ){
				cmd_quit("");
				break;
			}

			x--;

			/*
			if( prealarm > 0)	prealarm--;
			if( prealarm == 0 ) x--;
			if( pSysInfo->osd_alarm_motion || pSysInfo->osd_alarm_ext ){
				dbg("------------------------------\n");
				x += postalarm;
			}
			*/ // Disable by Alex. 2010.01.09
		}
	}
	else{
		dbg(" ufileName = %s", ufileName);
		time( &timep );    
		gettimeofday(&tv,NULL);
		p = localtime( &timep );

		if(strstr(ufileName, ".avi")) {
			//./Appro_avi_save -S 1 -d 6 -I 13 -i 100 -p "/mnt/smb/fs2" -A 1 -m 5000 -t 1 -f AAAA &
			char cmd[128] = "";
			char new_name[64] = "";
			
			snprintf(cmd, sizeof(cmd), "/opt/ipnc/Appro_avi_save -S %d -d 0 -I %d -i %d -p '/tmp' -A 1 -m %u -t %s",
				//pSysInfo->config.e2g_media[m_id].video.source + 1, 
				pSysInfo->ipcam[E2G_MEDIA0_VIDEO_SOURCE+m_id*MEDIA_STRUCT_SIZE].config.u8 + 1, 
				msgid,
				//pSysInfo->config.e2g_media[m_id].video.duration,
				pSysInfo->ipcam[E2G_MEDIA0_VIDEO_DURATION+m_id*MEDIA_STRUCT_SIZE].config.u8,
				//pSysInfo->config.e2g_media[m_id].video.maxsize);
				pSysInfo->ipcam[E2G_MEDIA0_VIDEO_MAXSIZE+m_id*MEDIA_STRUCT_SIZE].config.u32,
				ufileName);

			dbg("cmd = %s", cmd);
			system(cmd);

			//if(pSysInfo->config.e2g_media[m_id].video.filename)
			if(conf_string_read(E2G_MEDIA0_VIDEO_FILENAME+m_id*MEDIA_STRUCT_SIZE))
				snprintf(new_name, sizeof(new_name), "%s%ld_%.4ld.avi",
					//pSysInfo->config.e2g_media[m_id].video.filename, pSysInfo->second, tv.tv_usec >> 7);
					conf_string_read(E2G_MEDIA0_VIDEO_FILENAME+m_id*MEDIA_STRUCT_SIZE), pSysInfo->second, tv.tv_usec >> 7);
			else
				snprintf(new_name, sizeof(new_name), "%ld_%.4ld.avi", pSysInfo->second, tv.tv_usec >> 7);
			
			rename(ufileName, new_name);
			strcpy(ufileName, new_name);
		}
		
		sprintf(line,"put %s", ufileName);
		cmd_put(line);
		cmd_go("go");
	}

	return 1;
}
