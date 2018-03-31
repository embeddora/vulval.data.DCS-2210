#ifndef _RTSP_O_HTTP_SERVER_HH
#define _RTSP_O_HTTP_SERVER_HH

#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.hh"
#endif
#ifndef _NET_ADDRESS_HH
#include <NetAddress.hh>
#endif
#define RTSP_BUFFER_SIZE 10000 // for incoming requests, and outgoing responses

extern "C"
{
#include "rtsp_o_http.h"
}

class RTSP_O_HTTPServer	: public Medium
{
public:
	static RTSP_O_HTTPServer *createNew( UsageEnvironment &env, UserAuthenticationDatabase *authDatabase = NULL );
	static Boolean lookupByName( UsageEnvironment &env, char const *name, RTSP_O_HTTPServer *&resultServer );
	void addClientConnection( RTSP_DATA *workdata );
	void addServerMediaSession( ServerMediaSession *serverMediaSession );
	virtual ServerMediaSession *lookupServerMediaSession( char const *streamName );
	void removeServerMediaSession( ServerMediaSession* serverMediaSession );
	char *rtspURL( ServerMediaSession const *serverMediaSession ) const;
	static void incomingHandler( void *instance )
	{
		RTSP_O_HTTPServer::RTSPClientSession::incomingHandler( instance );
	}
protected:
	RTSP_O_HTTPServer( UsageEnvironment &env, UserAuthenticationDatabase *authDatabase );
	virtual ~RTSP_O_HTTPServer();
private: // redefined virtual functions
	virtual Boolean isRTSPServer() const;
private:
	// The state of each individual session handled by a RTSP server:
	class RTSPClientSession
	{
	public:
		RTSPClientSession( RTSP_O_HTTPServer &ourServer, unsigned sessionId, RTSP_DATA *workdata );
		virtual ~RTSPClientSession();
		static void incomingHandler( void *instance )
		{
			( (RTSPClientSession *) instance )->incomingRequestHandler1();
		}
	private:
		UsageEnvironment &envir() { return fOurServer.envir(); }
		void reclaimStreamStates();
		void resetRequestBuffer();
		void incomingRequestHandler1();
		void handleCmd_bad( char const *cseq );
		void handleCmd_notSupported( char const *cseq );
		void handleCmd_notFound( char const *cseq );
		void handleCmd_OPTIONS( char const *cseq );
		void handleCmd_DESCRIBE( char const *cseq, char const *urlSuffix, char const *fullRequestStr );
		void handleCmd_SETUP( char const *cseq, char const *urlPreSuffix, char const *urlSuffix, char const *fullRequestStr );
		void handleCmd_withinSession( char const *cmdName, char const *urlPreSuffix, char const *urlSuffix, char const *cseq, char const *fullRequestStr );
		void handleCmd_TEARDOWN( ServerMediaSubsession *subsession, char const *cseq );
		void handleCmd_PLAY( ServerMediaSubsession *subsession, char const *cseq, char const *fullRequestStr );
		void handleCmd_PAUSE( ServerMediaSubsession *subsession, char const *cseq );
		void handleCmd_GET_PARAMETER( ServerMediaSubsession *subsession, char const *cseq, char const *fullRequestStr );
		void handleCmd_SET_PARAMETER( ServerMediaSubsession *subsession, char const *cseq, char const *fullRequestStr );
		Boolean authenticationOK( char const *cmdName, char const *cseq, char const *urlSuffix, char const *fullRequestStr );
		Boolean isMulticast() const { return fIsMulticast; }
	private:
		RTSP_O_HTTPServer &fOurServer;
		unsigned fOurSessionId;
		ServerMediaSession *fOurServerMediaSession;
		unsigned char fRequestBuffer[RTSP_BUFFER_SIZE];
		unsigned fRequestBufferBytesLeft;
		unsigned char fResponseBuffer[RTSP_BUFFER_SIZE];
		Boolean fIsMulticast, fSessionIsActive, fStreamAfterSETUP;
		Authenticator fCurrentAuthenticator; // used if access control is needed
		unsigned char fTCPStreamIdCount; // used for (optional) RTP/TCP
		unsigned fNumStreamStates;
		RTSP_DATA *fClientData;
		struct streamState
		{
			ServerMediaSubsession *subsession;
			void *streamToken;
		}
		*fStreamStates;
	};
private:
	friend class RTSPClientSession;
	UserAuthenticationDatabase *fAuthDB;
	HashTable *fServerMediaSessions;
	unsigned fSessionIdCounter;
};

#endif
