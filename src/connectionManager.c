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

#include "connectionManager.h"

struct Broker {
  char ip[MAX_LINE];
  long port;
  char name[MAX_LINE];
  long priority;
};

struct Broker * broker;
int numBrokers = 0;
int currentBroker = 0;

int addBrokerImpl( char * buffer, int source );
void sortBrokers();

void setupConnectionManager()
{
    long readSize;
    char * readBuffer = NULL;
    size_t len = 0;
    broker = NULL;
    FILE * fd = fopen( TOPOLOGY_CNF, "r" );

    if ( ! fd ) {
        logMessage(stderr, "Unable to read topology file. Only host from commandline will be used.\n" );
        return;
    }

    // Read TOPOLOGY_CNF and populate broker list.
    int lineNum = 1;
    while ( ( readSize = getline( &readBuffer, &len, fd ) ) > 0 )
    {
        addBrokerImpl( readBuffer, lineNum );
        lineNum++;
    }

    sortBrokers();

    // Cleanup
    free( (void*)readBuffer );
}

void deleteConnectionManager()
{
    free( broker );
}

int failBrokerAdd( const char * attribute, int source )
{
    if ( source > 0 )
        logMessage( stderr, "Invalid %s in topology config file. line %d\n", attribute, source );
    else
        logMessage( stderr, "Invalid %s in broker specification.\n", attribute );
    broker = realloc( broker, sizeof( struct Broker ) * ( numBrokers ) );
    return 1;
}

int addBrokerImpl( char * buffer, int source )
{
    int i;
    char *str, * token, *endNum;
    broker = realloc( broker, sizeof( struct Broker ) * ( numBrokers + 1 ) );
    memset(broker + numBrokers, '\0', sizeof( struct Broker) );

    int error = 0;
    for ( i = 0, str = buffer; error == 0 ; i++, str = NULL )
    {
        token = strtok( str, ";");
        if ( token == NULL )
            break;
        trim(token);
        switch (i)
        {
            case 0:
                if ( strlen( token ) > MAX_LINE )
                    error = failBrokerAdd( "ip", source );
                else
                    strncpy( broker[numBrokers].ip, token, MAX_LINE );
                break;
            case 1:
                broker[numBrokers].port = strtol( token, &endNum, 10 );
                if ( ( errno == ERANGE && ( broker[numBrokers].port == LONG_MAX || broker[numBrokers].port == LONG_MIN ) )
                        || ( errno != 0 && broker[numBrokers].port == 0 ) || *endNum != '\0' )
                    error = failBrokerAdd( "port", source );

                break;
            case 2:
                if ( strlen( token ) > MAX_LINE )
                {
                    error = failBrokerAdd( "server name", source );
                }
                else
                    strncpy( broker[numBrokers].name, token, MAX_LINE );
                break;
            case 3:
                broker[numBrokers].priority = strtol( token, &endNum, 10 );
                if ( ( errno == ERANGE && ( broker[numBrokers].priority == LONG_MAX || broker[numBrokers].priority == LONG_MIN ) )
                        || ( errno != 0 && broker[numBrokers].priority == 0 )
                        || *endNum != '\0'
                                || broker[numBrokers].priority > 100
                                || broker[numBrokers].priority < 0  )
                    error = failBrokerAdd( "priority", source );
                break;
        }
    }
    if ( ! error && i != 4 )
        failBrokerAdd("specification", source);

    else
    {
        // Check for duplicate broker names
        for ( i=0; i < numBrokers; i++ )
            if ( strcmp( broker[i].name, broker[numBrokers].name ) == 0 )
                error = failBrokerAdd("duplicate name", source);

        if ( ! error )
        {
            numBrokers++;
            return 1;
        }
    }
    return 0;
}

int addBroker( char * buffer )
{
    int result = addBrokerImpl( buffer, -1 );
    if (result)
        sortBrokers();
    return result;
}

void sortBrokers()
{
    // Sort broker list by priority
    // Simple in-place selection sort since these lists will not be too big,
    // and it is easy to implement, and clear
    struct Broker tmp;
    int pos, searchIndex, emptySlot;
    for ( pos = 0; pos < numBrokers; pos++ )
    {
        memcpy( &tmp, broker + pos, sizeof( struct Broker) );
        emptySlot = 0;
        for ( searchIndex = pos + 1; searchIndex < numBrokers; searchIndex++ )
            if ( broker[searchIndex].priority > broker[pos].priority )
            {
                // Copy the higher priority broker into pos
                memcpy( broker + pos, broker + searchIndex, sizeof( struct Broker) );
                emptySlot = searchIndex;
            }
        // If a higher priority broker was found, we complete
        // the swap by writing tmp into the now empty slot.
        if ( emptySlot )
            memcpy( broker + emptySlot, &tmp, sizeof( struct Broker) );
    }
}

int removeBroker( char * buffer )
{
    int pos, found;
    for ( pos = 0, found = 0; pos < numBrokers; pos++ )
    {
        if (!found )
        {
            if ( strcmp( broker[pos].name, buffer ) == 0 )
                found = 1;
        }
        else
        {
            broker[pos-1] = broker[pos];
        }
    }
    if (found)
        broker = realloc( broker, sizeof( struct Broker ) * ( --numBrokers ) );
    return found;
}

int changeBrokerPriority( char * buffer )
{
    int pos;
    char * priorityStr = strchr( buffer, ';' );

    if ( priorityStr == NULL )
    {
        logMessage( stdout, "Failed to change Broker priority due to missing ';' delimiter.\n" );
        return 0;
    }

    // Break buffer int to 2 strings. buffer now is the broker name, and priority is the new priority
    *priorityStr = '\0';
    priorityStr++;

    long priority = strtol( priorityStr, NULL, 10 );
    if ( ( errno == ERANGE && ( priority == LONG_MAX || priority == LONG_MIN ) )
            || ( errno != 0 && priority == 0 )
            || priority < 0 || priority > 100 )
    {
        logMessage( stdout, "Failed to change Broker priority due to bad priority value.\n" );
        return 0;
    }

    // Find the broker that matches the name, and set the new priority
    for ( pos = 0; pos < numBrokers; pos++ )
    {
        if ( strcmp( broker[pos].name, buffer ) == 0 )
        {
            broker[pos].priority = priority;
            sortBrokers();
            return 1;
        }
    }
    logMessage( stdout, "Failed to change Broker priority due to bad broker name.\n" );
    return 0;

}

void printBroker( struct Broker * b, int sock ) {
    char tmp[MAX_LINE];
    snprintf(tmp, MAX_LINE, "%s; %ld; %s; %ld\n", b->ip, b->port, b->name, b->priority );
    write( sock, tmp, strlen( tmp ) );
}

void showBrokerMap( int sock )
{
    int i;
    for ( i = 0; i < numBrokers; i++ )
        printBroker( &broker[i], sock );
}

void connectToNextBroker( int * sock )
{
    int v = 1;
    struct hostent *he;
    struct sockaddr_in sin;

    if ( currentBroker < numBrokers )
    {
        logMessage( stdout, "Connecting to %s\n", broker[currentBroker].name );
        // Get IP address
        if ( (he = gethostbyname( broker[currentBroker].ip ) ) == NULL)
        {
            // get the host info
            herror("gethostbyname");
        }
        else
        {

            // copy the network address to sockaddr_in structure
            memcpy(&sin.sin_addr, he->h_addr_list[0], he->h_length);
            sin.sin_family = AF_INET;
            sin.sin_port = htons(broker[currentBroker].port);

            // Create the socket
            if ( ( *sock = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 )
            {
                perror( "Cannot open a socket to broker.");
            }
            else
            {
                //lets reuse the socket!
                setsockopt( *sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof( v ) );

                // Establish the connection
                if ( connect( *sock, (struct sockaddr*)&sin, sizeof( sin ) )
                        < 0 )
                {
                    perror( "can't connect to the XML data stream publisher" );
                    close( *sock );
                }
                else
                {
                    logMessage( stdout, "connected to the XML data stream publisher %s.\n", broker[currentBroker].name );
                }
            }
        }
        currentBroker++;
    }
    else
    {
        logMessage( stdout, "Unable to connect to a broker. Terminating\n" );
        setTerminateFlag(1);
    }


}
