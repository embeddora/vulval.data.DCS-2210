#include "ApproServerMediaSubsession.hh"

ApproServerMediaSubsession::ApproServerMediaSubsession( UsageEnvironment &env, ApproInput &Input, unsigned estimatedBitrate )
	: OnDemandServerMediaSubsession( env, True /*reuse the first source*/ ), fApproInput( Input )
{
	fEstimatedKbps = ( estimatedBitrate + 500 ) / 1000;
}
