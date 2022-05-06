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
pthread_cond_t buff1_cons_condv, buff2_cons_condv;

int suspend_f = 0;
pthread_mutex_t suspend_f_mutex;
pthread_cond_t suspend_f_condv;


void* producer(void *arg)
{
    unsigned int push_value;
    while (1) {
        push_value = (rand() % 1000); //random [0,999]
        if ( pthread_mutex_lock(&buff1_mutex) == 0 ) {

            while (circ_buff_isfull(&buffer1))
                pthread_cond_wait(&buff1_cons_condv, &buff1_mutex);
            
            if (circ_buff_push(&buffer1, push_value ) == 0){
                printf("Producer: %u\n", push_value);

                //Send a notification every time data is updated
                //The procucer doesn't care if anyone listens...
                pthread_cond_signal(&buff1_prod_condv);
            }
            else
                printf("Producer: buffer is full. THIS REALLY SHOULD NOT HAPPEN\n"); 

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
                
                //Test Buff2 and wait on condvar
                //Be aware that this strategy will eventually stall buff1. 
                //We're holding the lock on Buff1 and no thread is able to push data in
                while (circ_buff_isfull(&buffer2))
                    pthread_cond_wait(&buff2_cons_condv, &buff2_mutex);

                if (circ_buff_pop(&buffer1, &pop_value)==0) {
                    //Success popping from 1
                    pthread_cond_signal(&buff1_cons_condv);

                    if (circ_buff_push(&buffer2, pop_value) == 0){
                        pthread_cond_signal(&buff2_prod_condv);
                    } else {
                        //Push to Buff2 failed - Full
                        //Will drop the packet (Solution2)
                        fprintf(stderr, "Buffer 2 is Full - Droped value: %d THIS REALLY SHOULD NOT HAPPEN\n", pop_value);
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

        //When suspending let the process halt and wait on the signal of the conditional var.
        pthread_mutex_lock(&suspend_f_mutex);
        while (suspend_f){
            pthread_cond_wait(&suspend_f_condv, &suspend_f_mutex);
        }
        pthread_mutex_unlock(&suspend_f_mutex);


        if ( pthread_mutex_lock(&buff2_mutex) == 0 ) {

            while ( circ_buff_isempty(&buffer2) )
                pthread_cond_wait(&buff2_prod_condv, &buff2_mutex);
            
            if (circ_buff_pop(&buffer2, &pop_value)==0){
                printf("                              Consumer %ld: returned %u\n", (long)pthread_self(), pop_value);
                pthread_cond_signal(&buff2_cons_condv);
            }
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
        getchar(); //Dont care about the char - Pressing Enter is fine to toggle

        if ( pthread_mutex_lock(&suspend_f_mutex) == 0 ) {
            suspend_f = !suspend_f;
            pthread_cond_signal(&suspend_f_condv);
            pthread_mutex_unlock(&suspend_f_mutex);
        }
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

    if (pthread_cond_init(&buff1_cons_condv, NULL) !=0 ){
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

    if (pthread_cond_init(&buff2_cons_condv, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_cond_init ");
        return 1;
    }

    if (pthread_mutex_init(&suspend_f_mutex, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_mutex_init ");
        return 1;
    }

    if (pthread_cond_init(&suspend_f_condv, NULL) !=0 ){
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
    pthread_cond_destroy(&buff1_cons_condv);
    pthread_mutex_destroy(&buff1_mutex);

    pthread_cond_destroy(&buff2_prod_condv);
    pthread_cond_destroy(&buff2_cons_condv);
    pthread_mutex_destroy(&buff2_mutex);

    pthread_cond_destroy(&suspend_f_condv);
    pthread_mutex_destroy(&suspend_f_mutex);

    return 0;
}