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
extern int logging_sock;

//
//functions' declarations
//
void setTerminateFlag( int sigtype );

//! @brief termination signals handling functions to close TeleScope gracefully
void terminate_( int sigtype );

int launchSubscriberThread( const char * host, const char * port, const char * file );

void launchClientsThreadPool( void );

void lockMutex( pthread_mutex_t * m );

void unlockMutex( pthread_mutex_t * m );

int setSignals( void );

void print_service_ports( void );

void lockMutex( pthread_mutex_t * m );

void unlockMutex( pthread_mutex_t * m );
//! @brief Remove extra white space before and after str
//! @return 0 on success
int trim( char *str );

long long int getTotalMessagesReceived();

long long int getMatchingMessages();

void incMatchingMessages();


//! @brief report duration telescope has been running
//! @param sock socket to write output to.
void showUptime( int sock );

void logMessage( FILE *stream, const char *format, ... );

#endif
