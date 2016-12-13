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

#include "functions.h"

volatile sig_atomic_t terminateFlag = 0;
static int threadIDCounter = 0; //counter to keep track of Reader's ID


//socket stuff

//sockets
int server_sock;
int peer_sock;

struct sockaddr_in server_sin;

struct sigaction handler; //signal handler mainly for SIGPIPE blocking
struct sigaction term_handler; //signal handler for TERM like signals
pthread_mutex_t terminateLock;
pthread_mutex_t threadIDLock;

pthread_t subscriberThread;
pthread_t *tidR;
pthread_attr_t attr;

struct sockaddr_in peer_sin;
socklen_t len;
int rcv;

int syslog;
int loglevel;
int logfacility;

long long int TotalMessagesReceived = 0;
long long int MatchingMessages = 0;

//XML object structure: in - incoming raw data stream; out - refined xml data object

const xmlChar msg_start[] = "<XML_MESSAGE length=\"00002275";
int start_len_pos = 22;
const xmlChar client_intro[] =
        "<xml><XML_MESSAGE length=\"00000128\" version=\"0.2\" xmlns=\"urn:ietf:params:xml:ns:xfb-0.2\" type_value=\"0\" type=\"MESSAGE\"></XML_MESSAGE>";

int logging_sock = 0;

// This is not declared in the function to keep it off the stack since maximum
// stack size is usually much less than 16MB
char buf[MAX_LINE * MAX_LINE];
char filename[MIN_LINE];

void setupServer( void );
int prepareServerSocket( void );
void *Reader( void *param );

int receive_xml_static_buffer( const char *filename );
void *processStream_static_buffer( void * voidptr_filename );

//save the peer address from the incoming client connection
int getPeerAddress( const struct sockaddr *addr, char *ip );
int get_threadIDCounter( void );

void setTerminateFlag( int sigtype )
{
    terminateFlag = 1;
}

int setSignals( void )
{
    //lets install process-wide signal handler to avoid exiting on SIGPIPE signal
    memset( &handler, 0, sizeof( handler ) );
    if ( sigaction( SIGPIPE, NULL, &handler ) < 0 )
    {
        perror( "sigaction() failed for SIGPIPE!" );
    }
    else if ( handler.sa_handler == SIG_DFL )
    {
        handler.sa_handler = SIG_IGN;
        if ( sigaction( SIGPIPE, &handler, NULL ) < 0 )
            perror( "sigaction() failed for SIGPIPE!" );

    }
    //Set term handler for SIGTERM like signals arrival
    memset( &term_handler, 0, sizeof( term_handler ) );
    //term_handler.sa_handler = terminate_;//set the corresponding termination routine
    term_handler.sa_handler = setTerminateFlag; //set the corresponding termination routine
    sigemptyset( &term_handler.sa_mask );
    //now add a bunch of signals...
    if ( sigaddset( &term_handler.sa_mask, SIGTERM ) < 0 )
        perror( "sigaddset() failed!" );
    if ( sigaddset( &term_handler.sa_mask, SIGINT ) < 0 )
        perror( "sigaddset() failed!" );
    if ( sigaddset( &term_handler.sa_mask, SIGQUIT ) < 0 )
        perror( "sigaddset() failed!" );

    term_handler.sa_flags = 0;

    if ( sigaction( SIGTERM, &term_handler, 0 ) < 0 )
        perror( "sigaction() failed for SIGTERM!" );
    if ( sigaction( SIGINT, &term_handler, 0 ) < 0 )
        perror( "sigaction() failed for SIGINT!" );
    if ( sigaction( SIGQUIT, &term_handler, 0 ) < 0 )
        perror( "sigaction() failed for SIGQUIT!" );
    //block signals in threads
    //pthread_sigmask ( SIG_BLOCK, &term_handler.sa_mask, NULL );
    return 0;
}

int receive_xml_static_buffer( const char *filename )
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    int i, j;

    rcv = recv( peer_sock, buf, xmlStrlen( msg_start ), MSG_WAITALL );
    if ( rcv != xmlStrlen( msg_start ) )
    {
        if ( rcv < 0 )
        {
            return -1;
        }
        else if ( rcv == 0 )
        {
            logMessage( stdout, "Connection to current socket closed. Attempting to open new connection.\n" );
            connectToNextBroker( &peer_sock );
            return -1;

        }
        else
        {
            logMessage( stderr, "Failed to read %d bytes from socket.\n", xmlStrlen( msg_start));
            return -1;
        }
    }
    buf[rcv] = '\0'; // Terminate the string

    // If we established a new connection, we can get a second message with the initial <xml> flag.
    // If that is the case, we shift over what we have read so far, and read 5 more bytes from
    // the socket connection.
    if ( strncmp(buf, "<xml>", 5) == 0 )
    {
        for( i = 0, j = 5; j < xmlStrlen(msg_start); i++, j++ )
            buf[i] = buf[j];
        recv( peer_sock, buf + xmlStrlen(msg_start) - 5, 5, MSG_WAITALL );
    }

    long mesg_len = strtol( buf + start_len_pos, NULL, 10 );
    if ( ( errno == ERANGE && ( mesg_len == LONG_MAX || mesg_len == LONG_MIN ) )
            || ( errno != 0 && mesg_len == 0 ) || mesg_len > sizeof( buf ) )
    {
        logMessage( stdout, "Failed to get message length\n" );
        return -1;
    }

    rcv = recv( peer_sock, buf + xmlStrlen( msg_start ),
        mesg_len - xmlStrlen( msg_start ), MSG_WAITALL );
    if ( rcv != mesg_len - xmlStrlen( msg_start ) )
    {
        perror( "Failed to read the remainder of the message" );
        return -1;
    }
    buf[mesg_len] = '\0'; // Terminate the string

    // XML Stuff
    doc = xmlParseMemory( buf, mesg_len );
    if ( doc == NULL )
    {
        perror( "receive_xml_static_buffer(): Failed to parse document!" );
        return ( -1 );
    }
    root_element = xmlDocGetRootElement( doc );
    if ( root_element == NULL )
    {
        perror( "receive_xml_static_buffer(): root element of the XML doc is NULL!\n" );
        xmlFreeDoc( doc );
        doc = NULL;
        return ( -1 );
    }

    // Process the message
    analyze( filename, root_element, buf );

    // Cleanup
    xmlFreeDoc( doc );
    doc = NULL; /*Fri Jun  6 15:27:46 MDT 2014 */
    root_element = NULL; /* Wed Nov 19 17:16:58 MST 2014 */
    TotalMessagesReceived++;        //count the number of BGP messages received
    return 0;
}

int getPeerAddress( const struct sockaddr *addr, char *ip )
{
    const unsigned char *rawaddr = NULL;
    char printable[80];
    const char *peer = NULL;

    if ( addr->sa_family == AF_INET6 )
    {
        const struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        rawaddr = (const unsigned char *)&addr6->sin6_addr;
    }
    else if ( addr->sa_family == AF_INET )
    {
        const struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        rawaddr = (const unsigned char *)&addr4->sin_addr.s_addr;
    }
    if ( rawaddr != NULL )
        peer = inet_ntop( addr->sa_family, rawaddr, printable,
            sizeof( printable ) );
    if ( peer == NULL )
        peer = "(unknown)";

    strcpy( ip, peer );
    return 0;
}

int launchSubscriberThread( const char * host, const char * port, const char * file )
{
    setupBrokerList( host, port );

    connectToNextBroker( &peer_sock );

    strncpy(filename, file, MIN_LINE);

    if ( pthread_create( &subscriberThread, NULL, processStream_static_buffer, (void *)filename )
            != 0 )
    {
        perror( "failed to create subscriber Thread!" );
        exit( -1 );
    }
    return 0;
}

int prepareServerSocket( void )
{
    //build server address data structure
    server_sin.sin_family = AF_INET;
    server_sin.sin_addr.s_addr = INADDR_ANY;
    server_sin.sin_port = htons( startingPort + SERVER_PORT );

    // memset(&server_sin.sin_zero, '\0', sizeof(&server_sin.sin_zero)); /* Wed May 21 11:33:42 PDT 2014 */

    /* suppress the [-Wsizeof-pointer-memaccess] error in gcc 4.8+ by dereferencing a value */
    memset( &server_sin.sin_zero, '\0', sizeof( *server_sin.sin_zero ) ); /* Wed May 21 11:33:42 PDT 2014 */

    if ( ( server_sock = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
        perror( "can't open a socket!" );
        exit( 1 );
    }

    //lets reuse the socket!
    int v = 1;
    setsockopt( server_sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof( v ) );

    if ( ( bind( server_sock, (struct sockaddr *)&server_sin,
        sizeof( server_sin ) ) ) < 0 )
    {
        perror( "can't bind to a socket!" );
        exit( 1 );
    }

    //lets put the server socket into
    //listening mode to accept client requests
    if ( listen( server_sock, R_NUM ) < 0 )
    {
        perror( "can't start listening on a socket!" );
        exit( 1 );
    }
    return 0;
}

void launchClientsThreadPool( void )
{
    int tindex;

    setupServer( );

    prepareServerSocket();

    // Get the default attributes
    pthread_attr_init( &attr );
    //pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);//does not really help
    //lets create  Readers thread pool
    if ( ( tidR = (pthread_t *)calloc( R_NUM, sizeof(pthread_t) ) ) == NULL )
    {
        perror( "failed to allocate space for Reader thread IDs!" );
        free( tidR );
        exit( -1 );
    }

    /* the last argument to pthread_create must be passed by reference as a pointer cast of type void. NULL may be used if no argument is to be passed. */
    for ( tindex = 0; tindex < R_NUM; tindex++ )
    {
        if ( pthread_create( &tidR[tindex], &attr, Reader, (void *)&server_sock ) != 0 )
        {
            perror( "failed to create a client handling Reader thread!" );
            free( tidR );
            exit( -1 );
        }
        if ( pthread_detach( tidR[tindex] ) != 0 )
        {
            perror( "Reader thread failed to detach!" );
            free( tidR );
            exit( -1 );
        }
    }
    fprintf( stdout, "Clients Thread Pool started. Listening on port %ld \n",
    startingPort + SERVER_PORT );
    fprintf( logfile, "Clients Thread Pool started. Listening on port %ld \n",
    startingPort + SERVER_PORT );
}

void *Reader( void *threadid )
{
    int lsock, sock;
    lsock = *( (long*)threadid );
    char *xmlR = NULL;
    char ipstring[256];
    struct sockaddr_in clientName = { 0 };
    socklen_t clientNameLen = sizeof( clientName );

    int myID;
    Queue XMLQ = NULL;

    char filter[8];
    char refilter[9];

    // detach the thread so the resources may be returned when the thread exits
    pthread_detach( pthread_self() );

    myID = get_threadIDCounter(); /* obtain the ID of the queue to read from by brute force */
    XMLQ = getQueueTable()->qtable[myID];
    //XMLQ = XMLQS[myID];

    memset( ipstring, '\0', sizeof( ipstring ) );
    memset( filter, '\0', sizeof( filter ) );
    memset( refilter, '\0', sizeof( refilter ) );

    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 1000;

    while ( terminateFlag == 0 )
    {            //outer loop starts
        if ( ( sock = accept( lsock, (struct sockaddr *)&clientName, &clientNameLen ) ) < 0 )
        { //need this version to get info about subscriber's IP
            perror( "Reader: can't accept on a socket!" );
            close( sock );
            continue;
        }
        else
        {
            getPeerAddress( (struct sockaddr *)&clientName, ipstring );
            logMessage( stdout, "Reader: Accepting incoming client from IP: %s\n",
                ipstring );
            //updateClientsTable(XMLQ, ipstring);
            updateQueueTableClientsTable( getQueueTable(), ipstring, 1 );

            //write introductory bgp message with <xml> tag to order the reading correctly in the client
            write( sock, client_intro, strlen( (char *)client_intro ) );
        }
        //inner processing loop
        while ( true )
        {
            if ( isQueueEmpty( XMLQ ) == true )
            {
                // Wait 1ms to prevent 100% CPU utilization.
                usleep(1000);
                continue;
            }
            else
            {
                // Sleep for 1 micro second
                nanosleep( &sleeptime, NULL );
                readQueue( XMLQ, &xmlR );

                if ( xmlR == NULL )
                    continue;

                size_t xlen = strlen( xmlR );

                if ( ( write( sock, xmlR, xlen ) == -1 ) )
                {
                    logMessage( logfile,
                            "Reader: client closed the connection\n" );
                    break; //break out of the inner while loop
                }
                free(xmlR);

            } //end of else when queue is not empty
        } //end of inner while loop
        fprintf( logfile, "Reader: exited the inner while loop!\n" );
        updateQueueTableClientsTable( getQueueTable(), ipstring, 2 );
        close( sock );
    } //end of outer while loop

    pthread_exit( (void *)1 );
} //end of Reader

void setupServer( void )
{
    createQueueTable();
    if ( pthread_mutex_init( &threadIDLock, NULL ) )
    {
        perror( "unable to init mutex ID lock" );
        exit( 1 );
    }
}

void lockMutex( pthread_mutex_t * m )
{
    if ( pthread_mutex_lock( m ) )
    {
        perror( "Unable to lock pthread_mutex_t" );
        exit( 1 );
    }
}

void unlockMutex( pthread_mutex_t * m )
{
    if ( pthread_mutex_unlock( m ) )
    {
        perror( "Unable to unlock pthread_mutex_t" );
        exit( 1 );
    }
}


int get_threadIDCounter( void )
{
    int t = 0;

    lockMutex( &threadIDLock );

    t = threadIDCounter;
    threadIDCounter++; //JUST update the counter /* Thu May 22 15:36:29 PDT 2014 */

    unlockMutex( &threadIDLock );
    return t;
}

int resetMatch( void )
{
    MatchingMessages = 0;
    return 0;
}

void terminate_( int sigtype )
{
    sigset_t OldMask;
    sigprocmask( SIG_BLOCK, &term_handler.sa_mask, &OldMask );
    setTerminateFlag( 1 );
    usleep( 100000 );

    deleteConnectionManager();

    if ( serverflag == 1 )
    {
        pthread_attr_destroy( &attr );
        free( tidR );                //free the client serving thread pool
        destroyQueueTable( getQueueTable() );
        free( getQueueTable() );
    }

    xmlCleanupParser(); //Free the global libxml variables

    sigemptyset( &term_handler.sa_mask );
    memset( &term_handler, 0, sizeof( term_handler ) );
    sigprocmask( SIG_SETMASK, &OldMask, NULL );

    logMessage( stdout,
        "TeleScope received TERM signal. Freed all memory, canceled all threads and now exiting gracefully.\n" );
    fclose( logfile );
    exit( 0 );
}

void *processStream_static_buffer( void * voidptr_filename )
{
    char * file = (char *)voidptr_filename;

    while ( terminateFlag != 1 )
    {
        receive_xml_static_buffer( file );
    }      //end of main loop
    pthread_exit( (void *)1 );
}

void print_service_ports( void ) /* Mon May 26 17:15:55 MDT 2014 */
{
    fprintf( stdout,
        "When in server mode TeleScope is listening on the following ports: \n" );
    fprintf( stdout, "Clients Thread Pool - port %ld \n", startingPort + SERVER_PORT );
    fprintf( stdout, "Status Thread - port %ld \n", startingPort + STATUS_PORT );
    fprintf( stdout, "CLI Thread - port %ld \n", startingPort + CLI_PORT );
}

/* trims the string from both sides removing trailing spaces */
int trim( char *str )
{
    int i, l;
    int size = strlen( str );

    if ( size == 0 )
        return -1;

    //remove blanks from the beginning
    for ( i = 0; isspace( str[i] ); i++ );
    if (i)
        for ( l = 0; i <= size; i++, l++ )
            str[l] = str[i];
    else
        l = size + 1;

    //remove blanks from the end
    for ( i = l - 2; i >= 0 && isspace( str[i] ); i-- )
        str[i] = '\0';

    return 0;
}

long long int getTotalMessagesReceived()
{
    return TotalMessagesReceived;
}

long long int getMatchingMessages()
{
    return MatchingMessages;
}

void incMatchingMessages()
{
    MatchingMessages++;
}

void showUptime( int sock )
{
    int uptime = difftime(time(NULL), serverStartTime) + 0.5;

    int hours = uptime / 3600;
    int minutes = ( uptime % 3600 ) / 60;
    int seconds = uptime % 60;

    char tmp[MAX_LINE];
    snprintf(tmp, MAX_LINE, "Server uptime: %02d:%02d:%02d\n", hours, minutes, seconds );
    write( sock, tmp, strlen( tmp ) );
}

void logMessage( FILE *stream, const char *format, ... )
{
    char buffer[MAX_LINE];

    va_list args;
    va_start(args, format);
    vsnprintf( buffer, MAX_LINE, format, args );
    va_end( args );
    fprintf( stream, "%s", buffer );
    fprintf( logfile, "%s", buffer );
    if ( logging_sock )
        write( logging_sock, buffer, strlen(buffer) );
}
