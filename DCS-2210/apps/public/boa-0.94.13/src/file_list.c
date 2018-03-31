
#define  LOCAL_DEBUG
#include <debug.h>
#include <sysconf.h>
#include <net_config.h>

#include "file_list.h"
#include "web_translate.h"


char pathname[MAXPATHLEN]; 
char html_header[] = "<HTML><META HTTP-EQUIV=\"CACHE-CONTROL\" CONTENT=\"NO-CACHE\"><script language=\"javascript\" src=\"var.js\" ></script><script language=\"javascript\" src=\"common.js\" ></script><TITLE>Local Storage - SDCard</TITLE><BODY><PRE style='font-family:\"Courier New\";'><H1>Filelist of Memory Card</H1><BR><script>function W(a){document.write(a);}function E(a){return document.getElementById(a);}function FE(a,b){return FixNum(E(a).value,(b==null)?2:b);}function Z(s,a,b){if (s.charAt(a)=='0'){a++;b--;}return parseInt(s.substring(a,(a+b)));}var dd,sY,sM,sD,oY,oM,oD,z,sH,sN,sS,oH,oN,oS,oDT,sB;dd=new Date();oDT=location.href.split(\"time=\");if (oDT.length>1){var t=oDT[1];oY=Z(t,0,4);oM=Z(t,4,2);oD=Z(t,6,2);oH=Z(t,8,2);oN=Z(t,10,2);oS=Z(t,12,2);}sY=GetSelectNumberHtml(\"YY\",2000,2025,1,(oY==null)?dd.getYear():oY ,null,null,4);sM=GetSelectNumberHtml(\"MM\",1,12,1,(oM==null)?dd.getMonth()+1:oM,null,null,2);sD=GetSelectNumberHtml(\"DD\",1,31,1,(oD==null)?dd.getDate():oD,null,null,2);sH=GetSelectNumberHtml(\"HH\",0,23,1,(oH==null)?dd.getHours():oH ,null,null,2);sN=GetSelectNumberHtml(\"NN\",0,59,1,(oN==null)?dd.getMinutes():oN,null,null,2);sS=GetSelectNumberHtml(\"SS\",0,59,1,(oS==null)?dd.getSeconds():oS,null,null,2);sB=\"<input type=button value='Refresh' onclick='SM()'>\";W(\"Filename         Date       Time   Size  Start from:\"+sY+\"/\"+sM+\"/\"+sD+\" \"+sH+\":\"+sN+\":\"+sS+\" \"+sB);W(\"<BR><HR><BR>\"); z=\"sddel.htm\";function SM(){location.href=\"/sdget.htm?sdlisttime=\"+FE(\"YY\",4)+FE(\"MM\")+FE(\"DD\")+FE(\"HH\")+FE(\"NN\")+FE(\"SS\");}function F(b){var o=\"\";var i;for(i=0;i<b;i++){o+=' ';}return o;}function A(a,b,c,d){var o='';o+='<A HREF=\"?FILE='+a+'\">'+a+'</A>';o+=F(15-a.length)+b+' '+c+' '+d;o+=F(9-d.length)+'<A HREF=\"'+z+'?FILE='+a+'\">Delete</A>\\n';W(o);}\n";
char html_content[100];
char html_end[] = "<HR>HTTP Server</A><BR></PRE></BODY></HTML>";
char html_empty_avi[] = "NG NO FILE MATCH *.AVI";
char html_empty_jpg[] = "NG NO FILE MATCH *.JPG";

#define BUF_LEN 64

extern SysInfo *sys_info;

#if 0
main(int argc,char **argv) 
{ 
	int count = 0;
	long freespace = 0;
	char *pInputDir = NULL;
	char *pOutDir = NULL;
	char *pOutFile = NULL;
	char *pOutDisk = NULL;
	
	if (argc < 5)
	{  
		printf("Usage: %s <pathname> <disk_pathname> <ouput_dir> <ouput_file>\n",argv[0]);
		exit(1);
	}
	pInputDir	= argv[1];
	pOutDisk 	= argv[2];
	pOutDir 	= argv[3];
	pOutFile 	= argv[4];
	
	if (chdir(pInputDir) != 0)
	{ 
		printf("Error in chdir \n");
		exit(1);
	}
#if 1
	List_files_To_html(pInputDir,pOutDir,pOutFile,pOutDisk);
#else
	
	count = List_files();
	if( count > 0 )
	{
		printf("Number of files = %d\n",count);
	}
	
	freespace = GetDiskfreeSpace(pOutDisk);
	printf("Free Spces = %dKBytes \n",freespace);
	
	printf("\n");

#endif

}
#endif

//uri_sdlist_htm add by jiahung, 20090522
int CheckDlinkFileExist(char *pFilename)
{
	struct stat finfo;
	int res = -1;

	res = stat( pFilename, &finfo );
	chdir("/opt/ipnc");

	return  res;
}
int	DeleteDlinkFile(char *pFilename)
{
	int res = -1;
	dbg("delfile name = %s\n", pFilename);
	res = remove(pFilename);
	chdir("/opt/ipnc");
	return res;
}
char* GetWorkDir(char* cwd)
{
	//char cwd[BUF_LEN];
	if(!getcwd(cwd, BUF_LEN)){
		perror("getcwd");
		exit(EXIT_FAILURE);
	}
	//dbg("cwd = %s\n", cwd);
	return cwd;
}
/*
0	:file exist
-1	:file not exist
*/
int CheckFileExist(char *pDir, char *pFilename)
{
	struct stat finfo;
	int res = -1;

	if( pDir != NULL )
	{
		if (chdir(pDir) != 0)
		{
			return -1;
		}
	}

	res = stat( pFilename, &finfo );
	chdir("/opt/ipnc");
	
	return  res;
}

 int DeleteFile(char *pDir, char *pFilename)
{
	int res = -1;
	
	if( pDir != NULL )
	{
		if (chdir(pDir) != 0)
		{
			return -1;
		}
	}

	res = remove(pFilename);
	chdir("/opt/ipnc");
	
	return res;
}
/*
int Ouput_html_empty(char *pOutDir,char *pOutFile)
{
	FILE *pfd = NULL;
	int error_code = 0;
	//char *pempty = sys_info->config.sdcard.fileformat ? html_empty_avi : html_empty_jpg;
	char *pempty = sys_info->ipcam[SDCARD_FILEFORMAT].config.u8 ? html_empty_avi : html_empty_jpg;
	
	if (chdir(pOutDir) != 0)
	{
		DBGERR("Error in chdir \n");
		error_code = -1;
		goto QUIT_EMPTY;
	}
	
	remove(pOutFile);

	pfd = fopen(pOutFile,"wb");
	if( pfd == NULL )
	{
		DBGERR("Error in fopen \n");
		error_code = -1;
		goto QUIT_EMPTY;
	}
	if( fseek( pfd, 0, SEEK_SET ) < 0)
	{
		DBGERR("Error in fseek \n");
		error_code = -1;
		goto QUIT_EMPTY;
	}
	
	if( fwrite( pempty , strlen(pempty)+1, 1, pfd ) != strlen(pempty)+1)
	{
		DBGERR("Error in fwrite \n");
		error_code = -1;
		goto QUIT_EMPTY;
	}
QUIT_EMPTY:
	if( pfd != NULL )
	{
		fflush(pfd);
		fclose(pfd);
	}

	return error_code;
  
}

int OuputMem_html_empty(void *pMem, int maxsize)
{
	char tempbuff[100];
	char http_ip_addr[100];
	//char *pempty = sys_info->config.sdcard.fileformat ? html_empty_avi : html_empty_jpg;
	char *pempty = sys_info->ipcam[SDCARD_FILEFORMAT].config.u8 ? html_empty_avi : html_empty_jpg;

	GetIP_Addr(http_ip_addr);

	strncpy( pMem, pempty, (maxsize) );
	sprintf(tempbuff,"<HR>HTTP Server at <A HREF=\"http://%s\">ipnc</A><BR></PRE></BODY></HTML>", http_ip_addr);
	strncat(pMem, tempbuff, maxsize);

	return 0;
  
}
*/


//static FILE_INFO *pLIST_MEM = NULL;
/*
static int timesort(const void* a, const void* b)
{
    return ( ((FILE_INFO*)a)->time - ((FILE_INFO*)b)->time);
}
*/
/*
FILE_INFO *Get_File_List( char *pDir, int *pCnt, time_t start_time)
{
	int count = 0;
	struct stat finfo;
	struct dirent *current;
	
	DIR *d = opendir(pDir);
	if(d == NULL) {
		DBGERR("opendir error");
		goto FAIL_GET_LIST;
	}
	
	//files = malloc(sizeof (struct dirent *) * sys_info->config.sdcard_config.sdlistnum);
	//pLIST_MEM = malloc(sizeof(FILE_INFO) * sys_info->config.sdcard.listnum);
	pLIST_MEM = malloc(sizeof(FILE_INFO) * sys_info->ipcam[SDCARD_LISTNUM].config.u32);
	if( pLIST_MEM == NULL )
	{
		DBGERR("No memory \n");
		goto FAIL_GET_LIST;
	}	

	//while ( NULL != (current = readdir(d)) && count < sys_info->config.sdcard.listnum ) {
	while ( NULL != (current = readdir(d)) && count < sys_info->ipcam[SDCARD_LISTNUM].config.u32 ) {
		
		if(!stat(current->d_name, &finfo)) {
			
			if(finfo.st_ctime >= start_time && file_select(current)) {
				strcpy(pLIST_MEM[count].name, current->d_name);
				pLIST_MEM[count].time = finfo.st_ctime;
				sprintf(pLIST_MEM[count].size,"%lldK", ((long long)finfo.st_size/(long long)1024));
				count++;
			}
		}
	}
	
	closedir(d);

	//if (count != sys_info->config.sdcard.listnum)
	if (count != sys_info->ipcam[SDCARD_LISTNUM].config.u32)
		pLIST_MEM = realloc(pLIST_MEM, sizeof(FILE_INFO) * count);
	
	//count = xx_scandir( pDir, &files, file_select, alphasort, start_time);
	// If no files found, make a non-selectable menu item 
	if(count == 0)
	{
		DBGERR("No files in this directory \n");
		goto FAIL_GET_LIST;
	}

	qsort(pLIST_MEM, count, sizeof(FILE_INFO), &timesort);

	
	//pLIST_MEM = malloc(sizeof(FILE_INFO)*count);
	//if( pLIST_MEM == NULL )
	//{
	//	DBGERR("No memory \n");
	//	goto FAIL_GET_LIST;
	//}
	

	#if 0  // Disable by Alex, 2009.2.16 
	cnt = 0;
	for (i=0; i<count; i++)
	{
		if(!stat(files[i]->d_name,&finfo))
		{
			pTime = localtime(&finfo.st_ctime);
			sprintf(pLIST_MEM[cnt].name,"%s",files[i]->d_name);
			sprintf(pLIST_MEM[cnt].date,"%d/%02d/%02d",(1900+pTime->tm_year),( 1+pTime->tm_mon), pTime->tm_mday);
			sprintf(pLIST_MEM[cnt].time,"%02d:%02d:%02d",pTime->tm_hour, pTime->tm_min, pTime->tm_sec);
			sprintf(pLIST_MEM[cnt].size,"%lldK",((long long)finfo.st_size/(long long)1024));
			//fprintf(stderr,"%s	%s	%s	%s \n",	pLIST_MEM[i].name,
			//							pLIST_MEM[i].date,
			//							pLIST_MEM[i].time,
			//							pLIST_MEM[i].size);
		
		}else{
			sprintf(pLIST_MEM[cnt].name,"%s",files[i]->d_name);
			sprintf(pLIST_MEM[cnt].date,"%d/%02d/%02d",(1900+0),(1+0), 0);
			sprintf(pLIST_MEM[cnt].time,"%02d:%02d:%02d",0, 0, 0);
			sprintf(pLIST_MEM[cnt].size,"%dK",(int)(0/1024));
		}
			
		cnt++;
	}
	#endif

	*pCnt = count; 
	return pLIST_MEM;
	
FAIL_GET_LIST:
	*pCnt = 0;
	pLIST_MEM = NULL;
	return pLIST_MEM;
}

void Clean_File_List(void)
{
	if( pLIST_MEM!=NULL )
	{
		free((void *)pLIST_MEM);
	}

}
*/

#if 0 /*  Disable By Alex, 2009.2.16  */
int List_files_To_html(char *pInputDir,char *pOutDir,char *pOutFile,char *pOutDisk)
{

	int 	count = 0;
	int 	i = 0;
	FILE 	*pfd = NULL;
	int 	error_code = 0;
	FILE_INFO *pFilelist = NULL;
	time_t  now;	/* add by Alex, 2009.01.23	*/
		
	if (chdir(pInputDir) != 0)
	{ 
		DBGERR("Error in chdir \n");
		error_code = -1;
		goto QUIT_SAVE;
	}

	time(&now);
	pFilelist = Get_File_List( pInputDir, &count,  now);
		
	if( pFilelist == NULL )
	{
		Ouput_html_empty( pOutDir, pOutFile);
		
		DBGERR("Error get file list \n");
		error_code = -1;
		goto QUIT_SAVE;
	}
		
	if (chdir(pOutDir) != 0)
	{
		DBGERR("Error in chdir \n");
		error_code = -1;
		goto QUIT_SAVE;
	}
		
	remove(pOutFile);
	
	pfd = fopen(pOutFile,"wb");
	if( pfd == NULL )
	{
		DBGERR("Error in fopen \n");
		error_code = -1;
		goto QUIT_SAVE;
	}
	if( fseek( pfd, 0, SEEK_SET ) < 0)
	{
		DBGERR("Error in fseek \n");
		error_code = -1;
		goto QUIT_SAVE;
	}
	
	if( fwrite( html_header , sizeof(html_header)-1, 1, pfd ) != 1)
	{
		DBGERR("Error in fwrite \n");
		error_code = -1;
		goto QUIT_SAVE;
	}

	for (i=0;i<count;i++)
	{
		sprintf(html_content,"A(\"%s\",\"%s\",\"%s\",\"%s\");",pFilelist[i].name,
													pFilelist[i].date,
													pFilelist[i].time,
													pFilelist[i].size);
		fprintf(pfd,"%s",html_content);
	}
	
	fprintf(pfd,"</SCRIPT>\n");
	fprintf(pfd,"%d file and %d KBytes free\n",count, (int)GetDiskfreeSpace(pOutDisk));
	
	if( fwrite( html_end , sizeof(html_end), 1, pfd ) != 1)
	{
		DBGERR("Error in fwrite \n");
		error_code = -1;
		goto QUIT_SAVE;
	}
	
QUIT_SAVE:
	if( pfd != NULL )
	{
		fflush(pfd);
		fclose(pfd);
	}
	
	Clean_File_List();
	
	if( error_code < 0)
		return error_code;
	else
		return count;

}
#endif

void GetIP_Addr(char *pMem )
{

	NET_IPV4 ip;

	//ip.int32 = net_get_ifaddr("eth0");  //by kevinlee eth0 or ppp0
	ip.int32 = net_get_ifaddr(sys_info->ifname);

	sprintf(pMem, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]);
	
	return ;
}
/*
int MEM_List_files_To_html(char *pInputDir,char *pOutDisk, void *pOutMem, int MaxSize, time_t start_time)
{

	char http_ip_addr[100];
	int 	count = 0;
	int 	i = 0;
	int 	error_code = 0;
	FILE_INFO *pFilelist = NULL;
	char	tempbuff[100];
	struct tm *pTime;
	
	
	GetIP_Addr(http_ip_addr);
	fprintf(stderr,"para_netip %s \n",http_ip_addr);
	
	if (chdir(pInputDir) != 0)
	{ 
		DBGERR("Error in chdir \n");
		error_code = -1;
		goto QUIT_MEM_SAVE;
	}
	
	pFilelist = Get_File_List( pInputDir, &count, start_time);

	if( pFilelist == NULL )
	{	
		DBGERR("Error get file list \n");
		error_code = -1;
		goto QUIT_MEM_SAVE;
	}
	
	strncpy( pOutMem, html_header, MaxSize );
	
	
	for (i=0;i<count;i++)
	{
		pTime = localtime(&pFilelist[i].time);
		sprintf(html_content,"A(\"%s\",\"%d/%02d/%02d\",\"%02d:%02d:%02d\",\"%s\");", pFilelist[i].name,
													(1900+pTime->tm_year), (1+pTime->tm_mon), pTime->tm_mday,
													pTime->tm_hour, pTime->tm_min, pTime->tm_sec,
													pFilelist[i].size);
													
		if( MaxSize > strlen(pOutMem) )
			strncat(pOutMem, html_content, MaxSize-strlen(pOutMem));
	}
	if( MaxSize > strlen(pOutMem) )
		strncat(pOutMem, "</SCRIPT>\n", MaxSize-strlen(pOutMem));
	else
		fprintf(stderr,"MaxSize is not enough!\n");
	
	sprintf(tempbuff, "%d file and %d KBytes free\n",count, (int)GetDiskfreeSpace(pOutDisk));
	
	if( MaxSize > strlen(pOutMem) )
		strncat(pOutMem, tempbuff, MaxSize-strlen(pOutMem));
	else
		fprintf(stderr,"MaxSize is not enough!\n");
	
	sprintf(tempbuff,"<HR>HTTP Server at <A HREF=\"http://%s\">ipnc</A><BR></PRE></BODY></HTML>", http_ip_addr);
	
	if( MaxSize > strlen(pOutMem) )
		strncat(pOutMem, tempbuff, MaxSize-strlen(pOutMem));
	else
		fprintf(stderr,"MaxSize is not enough!\n");
	
	//strncat(pOutMem, html_end, MaxSize);
	
	
QUIT_MEM_SAVE:
	
	Clean_File_List();
	chdir("/opt/ipnc");
	
	if( error_code < 0)
	{
		OuputMem_html_empty( pOutMem,MaxSize);
		return error_code;
	}
	else
	{
		return count;
	}

}


int List_files(void)
{

	int count,i;
	struct direct **files;
	struct stat finfo;
	struct tm *pTime;
	char	datebuff[30];
	char	timebuff[30];

	
	if (getcwd(pathname,sizeof(pathname)) == NULL )
	{ 
		DBGERR("Error getting path \n");
		return -1;
	}
	printf("Current Working Directory = %s \n",pathname);
	
	count = scandir( pathname, &files, file_select, alphasort);
	// If no files found, make a non-selectable menu item 
	if(count <= 0)
	{
		DBGERR("No files in this directory \n");
		return -1;
	}

	
	printf("\n"); 
	for (i=0;i<count;i++)
	{
		
		if(!stat(files[i]->d_name,&finfo))
		{
			pTime = localtime(&finfo.st_ctime);
			sprintf(datebuff,"%d/%02d/%02d",(1900+pTime->tm_year),( 1+pTime->tm_mon), pTime->tm_mday);
			sprintf(timebuff,"%02d:%02d:%02d",pTime->tm_hour, pTime->tm_min, pTime->tm_sec);
			printf("%s	%s	%s	%dK \n",files[i]->d_name,datebuff,timebuff,(int)(finfo.st_size/1024));
		}else{
			printf("%s  \n",files[i]->d_name);
		}

		
	}
	
	 
	return count;

}
*/
long long GetDiskfreeSpace(char *pDisk)
{
	long long freespace = 0;	
	struct statfs disk_statfs;
	
	if( statfs(pDisk, &disk_statfs) >= 0 )
	{
		freespace = (long long)disk_statfs.f_bsize * (long long)disk_statfs.f_bfree >> 10 ;
	}
	/*
	fprintf(stderr,"GetDiskfreeSpace %lli \n",freespace);	
	fprintf(stderr,"f_type: 0x%X\n", disk_statfs.f_type);
	fprintf(stderr,"f_bsize: %d\n", disk_statfs.f_bsize);
	fprintf(stderr,"f_blocks: %li\n", disk_statfs.f_blocks);
	fprintf(stderr,"f_bfree: %li\n", disk_statfs.f_bfree);
	fprintf(stderr,"f_bavail: %li\n", disk_statfs.f_bavail);
	fprintf(stderr,"f_files: %li\n", disk_statfs.f_files);
	fprintf(stderr,"f_ffree: %li\n", disk_statfs.f_ffree);
	*/

	return freespace;
}

long long GetDiskusedSpace(char *pDisk)
{
	long long usedspace = 0;
	struct statfs disk_statfs;
	
	if( statfs(pDisk, &disk_statfs) >= 0 )
	{
		usedspace = ((long long)disk_statfs.f_bsize * ((long long)disk_statfs.f_blocks - (long long)disk_statfs.f_bfree)) >> 10 ;
	}
	return usedspace;
}
char *Compare_List[] = 
{
	".jpg",
	".avi"
};
/*
int file_select(const struct dirent   *entry) 
{	
	char *ptr;
	int cmp_num = sizeof(Compare_List)/sizeof(Compare_List[0]);
	int cnt = 0;
	int	IsFind = 0;

	if ((strcmp(entry->d_name, ".")== 0) ||
			(strcmp(entry->d_name, "..") == 0))
			 return (FALSE);

	// Check for filename extensions 
	ptr = rindex(entry->d_name, '.');
	if( ptr != NULL )
	{
		for( cnt = 0; cnt < cmp_num; cnt++ )
		{
			//if( strcmp(ptr,Compare_List[sys_info->config.sdcard.fileformat]) == 0 )
			if( strcmp(ptr,Compare_List[sys_info->ipcam[SDCARD_FILEFORMAT].config.u8]) == 0 )
			{
				IsFind = 1;
				break;
			}
		}
		if( IsFind == 1 )
		{
			return (TRUE);
		}else{
			return (FALSE);
		}
	}
	else
	{
		return(FALSE);
	}
}
*/
int fileformat_select(const struct dirent   *entry) 
{	
	char *ptr;
	int cmp_num = sizeof(Compare_List)/sizeof(Compare_List[0]);
	int cnt = 0;
	int	IsFind = 0;
	int fileformat;
	char cwd[64] = "";
	if ((strcmp(entry->d_name, ".")== 0) ||
			(strcmp(entry->d_name, "..") == 0))
			 return (FALSE);

	/* Check for filename extensions */
	ptr = rindex(entry->d_name, '.');
	
	if(!strcmp("/mnt/mmc/Picture", GetWorkDir(cwd)))
		fileformat = 1;
	else
		fileformat = 0;
	//dbg("###GetWorkDir(cwd)=%d\s\n", GetWorkDir(cwd));	
	//dbg("###fileformat=%d\n", fileformat);
	if( ptr != NULL )
	{
		for( cnt = 0; cnt < cmp_num; cnt++ )
		{
			if( strcmp(ptr,Compare_List[fileformat]) == 0 )
			{
				IsFind = 1;
				break;
			}
		}
		if( IsFind == 1 )
		{
			return (TRUE);
		}else{
			return (FALSE);
		}
	}
	else
	{
		return(FALSE);
	}
}

