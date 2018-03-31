#ifndef _ACS_H_
#define _ACS_H_

typedef struct _ACS_VideoHeader 
{ 
unsigned long ulHdrID; //Header ID 
unsigned long ulHdrLength; 
unsigned long ulDataLength; 
unsigned long ulSequenceNumber; 
unsigned long ulTimeSec; 
unsigned long ulTimeUSec; 
unsigned long ulDataCheckSum; 
unsigned short usCodingType; 
unsigned short usFrameRate; 
unsigned short usWidth; 
unsigned short usHeight; 
unsigned char ucMDBitmap; 
unsigned char ucMDPowers[3]; 
}ACS_VideoHeader, *PACS_VideoHeader;

typedef struct _ACS_AudioHeader 
{ 
unsigned long ulHdrID; //Header ID 
unsigned long ulHdrLength; 
unsigned long ulDataLength; 
unsigned long ulSequenceNumber; 
unsigned long ulTimeSec; 
unsigned long ulTimeUSec; 
unsigned long ulDataCheckSum; 
unsigned short usFormat; 
unsigned short usChannels; 
unsigned short usSampleRate; 
unsigned short usSampleBits; 
unsigned long ulReserved; 
}ACS_AudioHeader, *PACS_AudioHeader; 

/* DLINK */
void print_dlink_mpeg4_headers(request * req);
void print_dlink_audio_headers(request *req);
void print_dlink_http_headers(request * req);
void http_run_dlink_command(request *req, COMMAND_ARGUMENT *arg, int num);
void send_request_dlink_ok_mpeg4(request * req);
void send_request_dlink_ok(request * req, int size);
void send_request_uni_xml_ok(request * req);
void send_request_dlink_ok_audio(request * req);
void send_request_dlink_ok_mjpg(request * req);
#endif
