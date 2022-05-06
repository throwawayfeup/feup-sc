#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "buff_helper.h"

#define BUFFER_MAX_SIZE 4

unsigned int data_space1[BUFFER_MAX_SIZE];
circ_buff_t buffer1 = { 
    data_space1,
    BUFFER_MAX_SIZE,
    0,
    0
};

unsigned int data_space2[BUFFER_MAX_SIZE];
circ_buff_t buffer2 = { 
    data_space2,
    BUFFER_MAX_SIZE,
    0,
    0
};

pthread_mutex_t buff1_mutex, buff2_mutex;
pthread_cond_t buff1_prod_condv, buff2_prod_condv;

int suspend_f = 0;

void* producer(void *arg)
{
    unsigned int push_value;
    while (1) {
        push_value = (rand() % 1000); //random [0,999]
        if ( pthread_mutex_lock(&buff1_mutex) == 0 ) {
            
            if (circ_buff_push(&buffer1, push_value ) == 0){
                printf("Producer: %u\n", push_value);

                //Send a notification every time data is updated
                //The procucer doesn't care if anyone listens...
                pthread_cond_signal(&buff1_prod_condv);
            }
            else
                printf("Producer: buffer is full\n"); 

            pthread_mutex_unlock(&buff1_mutex);
        }
        
        usleep(100*1000); //100 ms
    }
    
    return NULL;
}

void* relay(void *arg)
{
    unsigned int pop_value;

    while (1) {
        if ( pthread_mutex_lock(&buff1_mutex) == 0 ) {

            while ( circ_buff_isempty(&buffer1) )
                pthread_cond_wait(&buff1_prod_condv, &buff1_mutex);
            
            if ( pthread_mutex_lock(&buff2_mutex) == 0 ) {
                
                //We need to pop from Buff1 (we know it's possible)
                //We want to push into Buff2 (we DONT know it's possible)
                //Busy-wait??
                //while (circ_buff_isfull(&buffer2));
                //Not a solution because I'm holding 2 locks
                //Solution1: Check if (circ_buff_isfull(&buffer2)), do nothing, unlock and let the while re-enter
                //Solution2: Consume from Buff1 and drop when Pushing
                //... going for the latter.

                if (circ_buff_pop(&buffer1, &pop_value)==0) {
                    //Success popping from 1

                    if (circ_buff_push(&buffer2, pop_value ) == 0){
                        pthread_cond_signal(&buff2_prod_condv);
                    } else {
                        //Push to Buff2 failed - Full
                        //Will drop the packet (Solution2)
                        fprintf(stderr, "Buffer 2 is Full - Droped value: %d\n", pop_value);
                    }
                }
                
                pthread_mutex_unlock(&buff2_mutex);
            }            

            pthread_mutex_unlock(&buff1_mutex);
        }  

        //No longer needed
        //usleep(150*1000); //150 ms
    }
    
    return NULL;
}


void* consumer(void *arg)
{
    unsigned int pop_value;
    while (1) {
        
        if (suspend_f)
            continue; //This will busy-wait (CPU-100%)

        if ( pthread_mutex_lock(&buff2_mutex) == 0 ) {

            while ( circ_buff_isempty(&buffer2) )
                pthread_cond_wait(&buff2_prod_condv, &buff2_mutex);
            
            if (circ_buff_pop(&buffer2, &pop_value)==0)
                printf("                              Consumer %ld: returned %u\n", pthread_self(), pop_value);
            else
                printf("                              Consumer: buffer is empty\n");

            pthread_mutex_unlock(&buff2_mutex);
        }  
        
    }
    
    return NULL;
}

void* cli(void *arg)
{
    while (1) {
        getchar(); //Dont care about the char
        suspend_f = !suspend_f;
        printf ("Suspend: %d\n", suspend_f);
    }
    
    return NULL;
}


int main(void)
{
    //Seeding...
	srand(time(NULL));

    pthread_t tid[4];

    if (pthread_mutex_init(&buff1_mutex, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_mutex_init ");
        return 1;
    }

    if (pthread_cond_init(&buff1_prod_condv, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_cond_init ");
        return 1;
    }

    if (pthread_mutex_init(&buff2_mutex, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_mutex_init ");
        return 1;
    }

    if (pthread_cond_init(&buff2_prod_condv, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_cond_init ");
        return 1;
    }
    
	pthread_create(&(tid[0]), NULL, &producer, NULL);
	pthread_create(&(tid[1]), NULL, &relay, NULL);
    pthread_create(&(tid[2]), NULL, &consumer, NULL);
    pthread_create(&(tid[3]), NULL, &cli, NULL);

    for (int i = 0; i < 4; i++)
        pthread_join(tid[i], NULL);

    pthread_cond_destroy(&buff1_prod_condv);
    pthread_mutex_destroy(&buff1_mutex);

    pthread_cond_destroy(&buff2_prod_condv);
    pthread_mutex_destroy(&buff2_mutex);

    return 0;
}