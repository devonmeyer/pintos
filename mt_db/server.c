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

/* The number of client threads. */
int num_client_threads = 0;

void *client_run(void *);
int handle_command(char *, char *, int len);
void *my_create_client_method(void *arg);

client_t *client_create(int ID)
{
    printf("Initializing client thread...\n");
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
    printf("client_destroy\n");
	/* Remove the window */
	window_destroy(client->win);
	free(client);
}

/* Code executed by the client */
void *client_run(void *arg)
{
    printf("client_run\n");
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

void *my_create_client_method(void *arg)
{
    client_t *client = (client_t *) arg;
    
    client = client_create(num_client_threads++); // The title of the client needs to reflect that this is the Nth client made
                                                  // where N = the number of client threads made since the program execution began
    client_run((void *)client);
    client_destroy(client);
    
    return 0;    
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
            pthread_t temp_thread = client_thread.thread; 
            client_t *c = &client_thread;
            
            // Creates a new thread which handles the creation the client thread.
            // We devote this new thread to creating the client thread so that the
            // rest of the program can continue to be available for additional thread-
            // creation calls.  
            if(pthread_create(&temp_thread, NULL, my_create_client_method, &c)) {
                fprintf(stderr, "Error creating thread\n");
                return 1;
            }
            
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
