#ifndef  _Appro_JPEG_STREAM_SOURCE
#define _Appro_JPEG_STREAM_SOURCE

#ifndef _APPRO_INPUT_HH
#include "ApproInput.hh"
#endif

#ifndef _JPEG_VIDEO_SOURCE_HH
#include <JPEGVideoSource.hh>
#endif

class ApproJPEGStreamSource: public JPEGVideoSource
{
public:
	static ApproJPEGStreamSource *createNew( FramedSource *inputSource );
private:
	ApproJPEGStreamSource(FramedSource *inputSource);
	virtual ~ApproJPEGStreamSource();
	virtual void doGetNextFrame();
	virtual u_int8_t type();
	virtual u_int8_t qFactor();
	virtual u_int8_t width();
	virtual u_int8_t height();
	virtual u_int8_t const *quantizationTables( u_int8_t &precision, u_int16_t &length );
	virtual u_int8_t const *restartIntervalTables( u_int8_t &precision, u_int16_t &length );
	static void afterGettingFrame( void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds );
	void afterGettingFrame1( unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds );
	FramedSource *fSource;
	u_int8_t fLastWidth, fLastHeight; // actual dimensions /8
	u_int8_t fLastQuantizationTable[128];
	u_int16_t fLastQuantizationTableSize;
	u_int8_t fLastrestartIntervalTable[4];
	u_int16_t fLastrestartIntervalTableSize;
	unsigned char fBuffer[VIDEO_MAX_FRAME_SIZE];
};

#endif
