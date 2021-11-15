#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "structures.h"

// Number of chairs in the waiting room
#define MAX_CHAIRS 5

// How many customers will be served before barber closes up the shop
#define MAX_CUSTOMERS_SERVED 5

// Change how often customers check in, bigger number means less frequent
#define FREQUENCY 5

// How long a customer can take being served
#define MAX_CUSTOMER_TIME 10

void *barber_thread(void *param); //thread for the barber
void *customer_thread(void *param); //thread for the customers
void *customer_generator_thread(void *param); //thread for generation of customer amount

int customers_served = 0;

Customer *serving; // Customer being served
waitingRoom waiting; // Waiting room

pthread_t barber_t;
pthread_t customer_generator_t;
pthread_attr_t attr;

pthread_mutex_t waiting_mutex;
pthread_mutex_t customers_served_mutex;
pthread_mutex_t barber_mutex;
pthread_cond_t wake_barber;
pthread_cond_t finished_serving_customer;

int main() {
    // Initialize all conditions and mutexes
    pthread_mutex_init(&waiting_mutex, NULL);
    pthread_mutex_init(&customers_served_mutex, NULL);
    pthread_mutex_init(&barber_mutex, NULL);
    pthread_cond_init(&wake_barber, NULL);
    pthread_cond_init(&finished_serving_customer, NULL);

    // Initialize waiting room queue
    waiting = new_waitingRoom(MAX_CHAIRS);

    // Create required threads
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&barber_t, &attr, barber_thread, NULL);
    pthread_create(&customer_generator_t, &attr, customer_generator_thread, NULL);

    // Threads to wait for
    pthread_join(barber_t, NULL);
    pthread_join(customer_generator_t, NULL);

    // Exit
    printf("%d customers have been served. Shop is now closed.\n", customers_served);
    return 0;
}

void *customer_thread(void *param) {
    Customer *c = (Customer *)param;
    pthread_mutex_lock(&c->exec_mutex);
    while (c->elapsed != c->duration) {
        c->elapsed++;
        sleep(1);
        printf("Customer #%d is being served by the barber. Time elapsed: %d\n", c->num, c->elapsed);
    }
    pthread_mutex_unlock(&c->exec_mutex);
    pthread_cond_signal(&finished_serving_customer);
    pthread_exit(NULL);
}

void *barber_thread(void *param) {
    while (1) {
        // Barber is sleeping by default
        if (waiting.num_queued == 0) {
            printf("Barber is sleeping.\n");
            pthread_cond_wait(&wake_barber, &barber_mutex);
            printf("Barber is now awake.\n");
        }

        // Pop a customer from queue and serve
        pthread_mutex_lock(&waiting_mutex);
        serving = WR_pop(&waiting);
        printf("Barber has gotten customer from the waiting room.\n");
        pthread_mutex_unlock(&waiting_mutex);

        printf("Barber is now serving customer #%d for %d seconds long.\n", serving->num, serving->duration);
        pthread_mutex_unlock(&serving->exec_mutex); // Allows the selected customer thread to run

        // Wait for customer thread to finish executing
        pthread_cond_wait(&finished_serving_customer, &barber_mutex);
        printf("Barber has finished serving customer #%d.\n",serving->num);

        // If done serving customer, increment customers served
        pthread_mutex_lock(&customers_served_mutex);
        customers_served++;

        // Terminate Barber thread if MAX_CUSTOMERS_SERVED reached
        if (customers_served == MAX_CUSTOMERS_SERVED) {
            printf("Max customers served: %d, thread terminated.\n", customers_served);
            pthread_mutex_unlock(&customers_served_mutex);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&customers_served_mutex);
    }
}

void *customer_generator_thread(void *param)
{
    int internal_customers_served = 1;

    // command below is to truly do a random amount of time
    // srand(time(NULL));

    sleep(1); // wait for other threads to initialize

    while (1) {
        int random = rand();
        if (random % FREQUENCY == 0) {
            // Customer arrived
            pthread_mutex_lock(&waiting_mutex);

            // If waiting not full...
            if (!WR_isFull(&waiting)) {
                int duration = 1 + random % MAX_CUSTOMER_TIME; // Generate a random time
                duration = duration + 1;
                printf("Customer generation: Customer has arrived, and will take %d seconds.\n", duration);
                Customer *c = new_Customer();
                c->num = internal_customers_served; //Sets customer number for clarification while debugging
                internal_customers_served++;
                c->duration = duration;  //Sets total duration of customer service

                //Customer's mutex is locked to prevent it from happening during initialization
                pthread_mutex_init(&c->exec_mutex, NULL);
                pthread_mutex_lock(&c->exec_mutex);
                pthread_create(&c->tid, &attr, customer_thread, c);
                WR_push(&waiting, c); // Push customer to waiting queue

                // Waking up the barber
                pthread_cond_signal(&wake_barber);
            }
            else {
                printf("Customer generation: Customer has arrived, but waiting room was full so customer left\n");
            }
            pthread_mutex_unlock(&waiting_mutex);
        }

        pthread_mutex_lock(&customers_served_mutex);
        if (customers_served == MAX_CUSTOMERS_SERVED) {
            printf("Customer generation: Max customers served: %d, thread terminated.\n", customers_served);
            pthread_mutex_unlock(&customers_served_mutex);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&customers_served_mutex);
        sleep(1);
    }
}
