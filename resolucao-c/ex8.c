#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "p5_helper.h"

#define BUFFER_MAX_SIZE 4

unsigned int data_space[BUFFER_MAX_SIZE];
circ_buff_t buffer = { 
    data_space,
    BUFFER_MAX_SIZE,
    0,
    0
};

pthread_mutex_t buff_mutex;

//pthread_cond_t buff_prod_condv = PTHREAD_COND_INITIALIZER;
pthread_cond_t buff_prod_condv;

void* producer(void *arg)
{
    unsigned int push_value;
    while (1) {
        push_value = (rand() % 1000); //random [0,999]
        if ( pthread_mutex_lock(&buff_mutex) == 0 ) {
            
            if (circ_buff_push(&buffer, push_value ) == 0){
                printf("Producer: %u\n", push_value);

                //Send a notification every time data is updated
                //The procucer doesn't care if anyone listens...
                pthread_cond_signal(&buff_prod_condv);
            }
            else
                printf("Producer: buffer is full\n"); 

            pthread_mutex_unlock(&buff_mutex);
        }
        
        usleep(100*1000); //100 ms
    }
    
    return NULL;
}

void* consumer(void *arg)
{
    unsigned int pop_value;
    while (1) {
        if ( pthread_mutex_lock(&buff_mutex) == 0 ) {

            while ( circ_buff_isempty(&buffer) )
                pthread_cond_wait(&buff_prod_condv, &buff_mutex);
            
            if (circ_buff_pop(&buffer, &pop_value)==0)
                printf("                              Consumer: returned %u\n", pop_value);
            else
                printf("                              Consumer: buffer is empty\n");

            pthread_mutex_unlock(&buff_mutex);
        }  

        //No longer needed
        //usleep(150*1000); //150 ms
    }
    
    return NULL;
}


int main(void)
{
    //Seeding...
	srand(time(NULL));

    pthread_t tid[2];

    if (pthread_mutex_init(&buff_mutex, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_mutex_init ");
        return 1;
    }

    if (pthread_cond_init(&buff_prod_condv, NULL) !=0 ){
        fprintf(stderr, "Error in pthread_cond_init ");
        return 1;
    }
    
	pthread_create(&(tid[0]), NULL, &producer, NULL);
	pthread_create(&(tid[1]), NULL, &consumer, NULL);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    pthread_cond_destroy(&buff_prod_condv);
    pthread_mutex_destroy(&buff_mutex);

    return 0;
}