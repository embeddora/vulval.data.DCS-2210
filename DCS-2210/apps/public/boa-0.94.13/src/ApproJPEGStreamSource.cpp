#include "ApproJPEGStreamSource.hh"

ApproJPEGStreamSource *ApproJPEGStreamSource::createNew( FramedSource *inputSource )
{
	return new ApproJPEGStreamSource(inputSource);
}

ApproJPEGStreamSource::ApproJPEGStreamSource( FramedSource *inputSource )
	: JPEGVideoSource( inputSource->envir() ),
#ifdef PLAT_DM355
	  fLastWidth( 0 ), fLastHeight( 0 )
#else
	  fLastWidth( 0 ), fLastHeight( 0 ), fLastQuantizationTableSize( 128 ),fLastrestartIntervalTableSize( 4 )
#endif
{
	fSource = inputSource;
}

ApproJPEGStreamSource::~ApproJPEGStreamSource()
{
	Medium::close( fSource );
}

void ApproJPEGStreamSource::doGetNextFrame()
{
	fSource->getNextFrame( fBuffer, sizeof( fBuffer ), ApproJPEGStreamSource::afterGettingFrame, this, FramedSource::handleClosure, this );
}

u_int8_t ApproJPEGStreamSource::type()
{
#ifdef PLAT_DM355
	return 0;
#else
	return 65;
#endif
};

u_int8_t ApproJPEGStreamSource::qFactor()
{
	return 255; // indicates that quantization tables are dynamic
};

u_int8_t ApproJPEGStreamSource::width()
{
	return fLastWidth;
}

u_int8_t ApproJPEGStreamSource::height()
{
	return fLastHeight;
}

u_int8_t const *ApproJPEGStreamSource::quantizationTables( u_int8_t &precision, u_int16_t &length )
{
	precision = 0;
	length = fLastQuantizationTableSize;
	return fLastQuantizationTable;
}

#ifndef PLAT_DM355
u_int8_t const *ApproJPEGStreamSource::restartIntervalTables( u_int8_t &precision, u_int16_t &length )
{
	precision = 0;
	length = fLastrestartIntervalTableSize;
	return fLastrestartIntervalTable;
}
#endif
 
void ApproJPEGStreamSource::afterGettingFrame( void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds )
{
	ApproJPEGStreamSource *source = (ApproJPEGStreamSource *)clientData;
	source->afterGettingFrame1( frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds );
}

void ApproJPEGStreamSource::afterGettingFrame1( unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds )
{
	fFrameSize = frameSize < fMaxSize ? frameSize : fMaxSize;
#ifdef PLAT_DM355
	unsigned const JPEGHeaderSize = 607;
#else
	unsigned const JPEGHeaderSize = 0x265;
#endif
	Boolean foundSOF0 = False;
	fLastQuantizationTableSize = 0;
#ifdef PLAT_DM355
	for ( unsigned i = 0; i < JPEGHeaderSize - 8; ++i )
	{
		if ( fBuffer[i] == 0xFF )
		{
			if ( fBuffer[i + 1] == 0xDB )	// DQT
			{
				u_int16_t length = ( fBuffer[i + 2] << 8 ) | fBuffer[i + 3];
				if ( i + 2 + length < JPEGHeaderSize )	// sanity check
				{
					u_int16_t tableSize = length - 3;
					if ( fLastQuantizationTableSize + tableSize > 128 )	// sanity check
						tableSize = 128 - fLastQuantizationTableSize;
					memmove( &fLastQuantizationTable[fLastQuantizationTableSize], &fBuffer[i + 5], 64 );
					memmove( &fLastQuantizationTable[fLastQuantizationTableSize + 64], &fBuffer[i + 5 + 1 + 64], 64 );
					fLastQuantizationTableSize += tableSize;
					if ( ( fLastQuantizationTableSize == 128 ) && foundSOF0 )
						break;
					i += length;
				}
			}
			else if ( fBuffer[i + 1] == 0xC0 )	// SOF0
			{
				fLastHeight = ( fBuffer[i + 5] << 5 ) | ( fBuffer[i + 6] >> 3 );
				fLastWidth = ( fBuffer[i + 7] << 5) | ( fBuffer[i + 8] >> 3 );
				foundSOF0 = True;
				if ( fLastQuantizationTableSize == 128 )
					break;
				i += 8;
			}
		}
	}
#else
	for ( unsigned i = 0; i < JPEGHeaderSize - 8; ++i )
	{
		if ( fLastQuantizationTableSize == 0 )
			i = 0x14;
		else if ( foundSOF0 != True )
			i = 0x23E;
		if ( fBuffer[i] == 0xFF )
		{
			if ( fBuffer[i + 1] == 0xDB )	// DQT
			{
				u_int16_t length = ( fBuffer[i + 2] << 8 ) | fBuffer[i + 3];
				if ( i + 2 + length < JPEGHeaderSize )	// sanity check
				{
					u_int16_t tableSize = length - 3;
					if ( fLastQuantizationTableSize + tableSize > 128 )	// sanity check
						tableSize = 128 - fLastQuantizationTableSize;
					memmove( &fLastQuantizationTable[fLastQuantizationTableSize], &fBuffer[i + 5], 64 );
					memmove( &fLastQuantizationTable[fLastQuantizationTableSize + 64], &fBuffer[i + 5 + 1 + 64], 64 );
					fLastQuantizationTableSize += tableSize;
					if ( ( fLastQuantizationTableSize == 128 ) && foundSOF0 )
						break;
					i += length;
				}
			} 
			else if ( fBuffer[i + 1] == 0xC0 )	// SOF0
			{
				fLastHeight = ( fBuffer[i + 5] << 5 ) | ( fBuffer[i + 6] >> 3 );
				fLastWidth = ( fBuffer[i + 7] << 5) | ( fBuffer[i + 8] >> 3);
				foundSOF0 = True;
				if ( fLastQuantizationTableSize == 128 )
					break;
				i += 8;
			}
		}
	}
	for ( unsigned i = 0x251; i < JPEGHeaderSize - 8; ++i ) 
	{
		if ( fBuffer[i] == 0xFF ) 
		{
			if ( fBuffer[i + 1] == 0xDD ) 
			{
				fLastrestartIntervalTable[0] = fBuffer[i + 4];
				fLastrestartIntervalTable[1] = fBuffer[i + 5];
				fLastrestartIntervalTable[2] = 0xFF;
				fLastrestartIntervalTable[3] = 0xFF;
				break;
			}
		}
	}
#endif
	if ( fLastQuantizationTableSize == 64 )
	{
		memmove( &fLastQuantizationTable[64], fLastQuantizationTable, 64 );
		fLastQuantizationTableSize = 128;
	}
	if ( ! foundSOF0 )
		envir() << "Failed to find SOF0 marker in JPEG header!\n";
	fFrameSize = fFrameSize > JPEGHeaderSize ? fFrameSize - JPEGHeaderSize : 0;
	memmove( fTo, &fBuffer[JPEGHeaderSize], fFrameSize );
	fNumTruncatedBytes = numTruncatedBytes;
	fPresentationTime = presentationTime;
	fDurationInMicroseconds = fDurationInMicroseconds;
	FramedSource::afterGetting( this );
}
