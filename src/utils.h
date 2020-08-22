/**
* CSE344 SYSTEMS PROGRAMMING
* FINAL PROJECT
* GOKTUG ALI AKIN | 161044018
*
* THIS FILE CONTAINS UTILITY FUNCTIONS
*/

#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h> 
#include <signal.h>
#include <math.h>
#include <time.h>
/**
* Global named semaphore is used for prevent multiple instances of program running.
*/
#include <semaphore.h>

extern int errno;

#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR);
#define MAX(x,y) ((x>y) ? x:y)

#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 4096

/** Generic, templated linked list **/
typedef struct node_t
{
	void* item;
	struct node_t* next;
}node_t;

typedef struct LinkedList
{
	
	node_t* head;
	node_t* tail;
	int size;

}LinkedList;

typedef struct Graph
{	
	LinkedList** adj;
	int vertex;
	int edge;
}Graph;

/* used for cache database */
typedef struct path_t
{
	int from;
	int to;
	char* str;
}path_t;


/* utility functions */

/**
* Return current time as string.
*/
char* time_as_string();

/**
* read and parse the file into char array.
*/
char** read_file(int input_fd,int* lines);

/**
* return the number of c in the given string.
*/
int char_count(const char* string, char c,size_t size);

/**
* Reverse the given string.
*/
void str_reverse(char* string);

/**
* exit under illegal file conditions.
*/
void _exit_invalid_file(const char* filepath);

/**
* Remove whitespaces from string, returns new string.
*/
char* remove_whitespace(const char* token);

/******** LINKED LIST FUNCTIONS ********/

LinkedList* LinkedList_create();

/**
* Return item at given index.
*/
void* LinkedList_at(const LinkedList* list, int index);

/**
* Add item to the end of the list.
*/
void LinkedList_add(LinkedList* list,void* item);

/**
* Get tail item of the linked list.
*/
void* LinkedList_get_tail(LinkedList* list);

/**
* Clear the linked list from memory.
*/
void LinkedList_clear(LinkedList* list);

/* return adress must be de-allocated */
void* LinkedList_pop(LinkedList* list);

/******** GRAPH FUNCTIONS ********/

/**
* Creates graph with given vertex number.
*/
Graph* graph_create(int vertex);

/**
* Add new edge into the given graph.
*/
void graph_addEdge(Graph* graph,int from, int to);

/**
* Do BFS on the given graph.
* Source : Elliot B. Koffman, Data Structures and Algorithms book.
*/
char* graph_BFS(Graph* graph,int from, int to);

/**
* Clear the given graph from memory.
*/
void graph_clear(Graph* graph);

/**
* Creates and return the graph initialized from the file.
*/
Graph* create_graph_from_file(int input_fd);

/**
* Return the path found by BFS.
* free's the given path pointer.
*/
char* path_as_string(int* path,int from,int to,int vertex);

/***********************************/

/**
* Parse the request sended by the client into int array with size 2.
* arr[0] = from;
* arr[1] = to;
*/
int* parse_request(char* request_str);

#endif