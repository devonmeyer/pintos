#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "window.h"
#include "db.h"
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>


/* the encapsulation of a client thread, i.e., the thread that handles
 * commands from clients */
typedef struct Client {
	pthread_t thread;
	window_t *win;
} client_t;

void *client_run(void *);
int handle_command(char *, char *, int len);

client_t *client_create(int ID)
{
	client_t *new_Client = (client_t *) malloc(sizeof(client_t));
	char title[16];

	if (new_Client == 0)
		return 0;

	sprintf(title, "Client %d", ID);

	/* Creates a window and set up a communication channel with it */
	new_Client->win = window_create(title);

	return new_Client;
}

void client_destroy(client_t *client)
{
	/* Remove the window */
	window_destroy(client->win);
	free(client);
}

/* Code executed by the client */
void *client_run(void *arg)
{
	client_t *client = (client_t *) arg;

	/* main loop of the client: fetch commands from window, interpret
	 * and handle them, return results to window. */
	char command[256];
	char response[256] = { 0 };	/* response must be null for the first call to serve */

	serve(client->win, response, command);
	while (handle_command(command, response, sizeof(response))) {
		serve(client->win, response, command);
	}

	return 0;
}

int handle_command(char *command, char *response, int len)
{
	if (command[0] == EOF) {
		strncpy(response, "all done", len - 1);
		return 0;
	}

	interpret_command(command, response, len);

	return 1;
}

// THIS IS JUST TESTING
void *inc_x(void *x_void_ptr)
{
    
    /* increment x to 100 */
    int *x_ptr = (int *)x_void_ptr;
    while(++(*x_ptr) < 100);
    
    printf("x increment finished\n");
    
    /* the function must return something - NULL will do */
    return NULL;
    
}

int main(int argc, char *argv[])
{
    printf("my new version\n");
    client_t* client_threads[100]; // Array of pointers to client threads
    int num_threads = 0;
    
    char command[20]; // shouldn't be more than 20 characters

    int flag = 1; // true
    
    while (flag == 1){
        fgets (command, 20, stdin); // This isn't a "bulletproof" way of receiving input. Maybe we improve it later.
        if (command[0] == 'e'){

            client_t client_thread;
            pthread_t temp_thread = client_thread.thread; // CAUSES SEG FAULT
            client_t *c = &client_thread;
            
            // THINK WE NEED TO CREATE A NEW THREAD HERE
            if(pthread_create(&temp_thread, NULL, client_run, &c)) {
                
                fprintf(stderr, "Error creating thread\n");
                return 1;
                
            } else {
                client_threads[num_threads++] = c;
                client_destroy(c);
                num_threads--;
                printf("made it, yo \n");
            }
            
            /*else {
                printf("Sure, I'll make a thread for you, champ.\n");
                // client_t *c;
                c = client_create(num_threads);
                client_threads[num_threads] = c;
                num_threads++;
                
                client_run((void *)c);
                client_destroy(c);
                num_threads--;
                
                //create new client
                //show window
            }*/
            
        }
        if (argc != 1) {
            fprintf(stderr, "Usage: server\n");
            exit(1);
        }
    }
    /*
    
    // ------- below this is the old code -------
    client_t *c;

    
	if (argc != 1) {
		fprintf(stderr, "Usage: server\n");
		exit(1);
	}

	c = client_create(0);
	client_run((void *)c);
	client_destroy(c);
    */
    
	return 0;
}
