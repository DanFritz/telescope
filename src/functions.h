/*
 Copyright (c) 2012-2014  Kirill Belyaev
 * kirillbelyaev@yahoo.com
 * kirill@cs.colostate.edu
 * TeleScope - XML Message Stream Broker/Replicator Platform
 * This work is licensed under the Creative Commons Attribution-NonCommercial 3.0 Unported License. 
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/ or send 
 * a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
 */

/*
 "DION
 The violent carriage of it
 Will clear or end the business: when the oracle,
 Thus by Apollo's great divine seal'd up,
 Shall the contents discover, something rare
 Even then will rush to knowledge. Go: fresh horses!
 And gracious be the issue!"

 Winter's Tale, Act 3, Scene 1. William Shakespeare
 */

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "globals.h"

extern const xmlChar msg_start[];
extern int start_len_pos;

//
//functions' declarations
//
int prepareServerSocket( void );

void setTerminateFlag( int sigtype );
int setSignals( void );
int establishPeerConnection( struct hostent *hp , char * ipptr, int port );

///termination signals handling functions to close TeleScope gracefully
void terminate_( int sigtype );

///returns IP for the connection
int returnip( struct hostent *hp , char * ipptr );

void processStream_static_buffer( char * filename );
int receive_xml_static_buffer( char *filename );

void *Reader( void *param );

void launchClientsThreadPool( void );

//save the peer address from the incoming client connection
int getPeerAddress( const struct sockaddr *addr, char *ip );

void initialize_terminateLock( void );
int update_terminateCounter( void );
int get_terminateCounter( void );

int resetXmlBuff( void );
int reset_key( void );
int godaemon( char *rundir, char *pidfile, int noclose );

void setupServer( void );
int update_threadIDCounter( void );
int get_threadIDCounter( void );

void print_service_ports( void );

/**@brief Remove extra white space before and after str
 * @return 0 on success
 */
int trim( char *str );

long long int getTotalMessagesReceived();
long long int getMatchingMessages();
void incMatchingMessages();
QueueTable getQueueTable();

void logMessage( FILE *stream, const char *format, ... );
#endif
