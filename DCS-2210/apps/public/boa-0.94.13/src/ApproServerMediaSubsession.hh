#ifndef _Appro_SERVER_MEDIA_SUBSESSION_HH
#define _Appro_SERVER_MEDIA_SUBSESSION_HH

#include <OnDemandServerMediaSubsession.hh>
#ifndef _APPRO_INPUT_HH
#include "ApproInput.hh"
#endif

class ApproServerMediaSubsession: public OnDemandServerMediaSubsession
{
protected:
	ApproServerMediaSubsession( UsageEnvironment &env, ApproInput &Input, unsigned estimatedBitrate );
	virtual ~ApproServerMediaSubsession() {}
protected:
	ApproInput &fApproInput;
	unsigned fEstimatedKbps;
};

#endif
