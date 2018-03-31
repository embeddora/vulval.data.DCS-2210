#ifndef _FILE_LIST_H_
#define _FILE_LIST_H_

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <dirent.h>
#include <time.h>   
#include <ifaddrs.h>

typedef struct _FILE_INFO{
	char name[30];
	//char date[15];
	//char time[10];
	time_t time;	/* modified by Alex, 2009.2.16  */
	char size[20];
} FILE_INFO;

//sdlist add by jiahung, 20090522
typedef struct _FILELIST{
	char name[50];
	__u8 filetype;
	__u8 eventtype;
	size_t size;
	__u32 numoffiles;
	__u8 property;
	__u8 authority;
	time_t time;	
	int reserve;
} FILELIST;

//int file_select(const struct dirent   *entry);
int List_files(void);
int List_files_To_html(char *pInputDir,char *pOutDir,char *pOutFile,char *pOutDisk);
//int MEM_List_files_To_html(char *pInputDir,char *pOutDisk, void *pOutMem, int MaxSize, time_t start_time);

//int OuputMem_html_empty(void *pMem, int maxsize);
//int Ouput_html_empty(char *pOutDir,char *pOutFile);

int DeleteFile(char *pDir,char *pFilename);
int CheckFileExist(char *pDir,char *pFilename);
long long GetDiskfreeSpace(char *pDisk);
long long GetDiskusedSpace(char *pDisk);
void GetIP_Addr(char *pMem );

int CheckDlinkFileExist(char *pFilename);
int	DeleteDlinkFile(char *pFilename);
char* GetWorkDir(char* cwd);
int fileformat_select(const struct dirent *entry); 

#endif
