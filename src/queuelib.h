/*
 Copyright (c) 2012-2014  Kirill Belyaev
 * kirillbelyaev@yahoo.com
 * kirill@cs.colostate.edu
 * TeleScope - XML Message Stream Broker/Replicator Platform
 * This work is licensed under the Creative Commons Attribution-NonCommercial 3.0 Unported License. 
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/ or send 
 * a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
 * Queue Library support for TeleScope
 */

#ifndef QUEUELIB_H
#define	QUEUELIB_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>

#include <sys/types.h>

#include <pthread.h>

#define QUEUE_MAX_ITEMS 1000
#define R_NUM 128

//keeps track of connecting clients
struct clients {
        char ip[256][256];
        int refcount;
        int size;
};

/* Entries that are stored in the queue 
 * -------------------------------------------------------------------------------------*/
typedef struct QueueEntryStruct {
        // a reference count    
        int count;
        // a pointer to the actual message buffer
        void *messageBuf;
} QueueEntry;

/*----------------------------------------------------------------------------------------
 * Queue Structure Definition
 * -------------------------------------------------------------------------------------*/
struct QueueStruct {
        // locking for multithreaded environment
        pthread_mutex_t queueLock;
        pthread_cond_t queueCond;

        //   the queue data
        // the index of oldest item
        long head;
        // the index of next available slot
        long tail;
        int count;
        // an array of messages that are stored in the queue
        QueueEntry items[QUEUE_MAX_ITEMS];
        // the data copy function for items in this queue
        void (*copy)( void **copy, void *original );
        int (*sizeOf)( void *msg );
        // the sizeof function for items in this queue
        struct clients clients_tbl;

};

typedef struct QueueStruct *Queue;

//Queues Table - enables dedicated queue per subscriber
struct QueueTableStruct {
        pthread_mutex_t queueTableLock;
        Queue qtable[R_NUM];
        struct clients clients_tbl;
};

typedef struct QueueTableStruct *QueueTable;

Queue createQueue( void );
void destroyQueue( Queue q );
void clearQueue( Queue q );
bool isQueueEmpty( Queue q );
int writeQueue( Queue q, void *item );
int readQueue( Queue q, void **item );
int sizeQueue( Queue q );
long getItemsUsed( Queue q );
int getItemsTotal( Queue q );
int updateClientsTable( Queue q, const char *peer );
QueueTable createQueueTable( void );
void destroyQueueTable( QueueTable qt );
int updateQueueTableClientsTable( QueueTable qt, const char *peer, int flag );
int decrementQueueTableRefcount( QueueTable qt );
int getRefcount( Queue q );
int getQueueTableRefcount( QueueTable qt );
int getClientAddress( Queue q, int index, char * buff );
int getQueueTableClientAddress( QueueTable qt, int index, char * buff );
int writeQueueTable( QueueTable qt, void * item );



#endif
