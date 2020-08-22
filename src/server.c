/**
* GOKTUG AKIN
* CSE344 SYSTEM PROGRAMMING
* FINAL PROJECT | MULTI-THREADED SERVER
*/

#include "utils.h"
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>

#define SERVER_BACKLOG 100 

/* take measures against double instantiation */
#define SERVER_SEMAPHORE_NAME "SERVER-SEMAPHORE"

/**
* Global variables
*/
Graph* graph=NULL;
LinkedList* client_queue=NULL;
LinkedList* thread_pool=NULL;
LinkedList* thread_ids=NULL;
LinkedList* cache_database=NULL;
int busy_threads=0;
int init_thread_size;
int max_thread_size;
int log_file_fd;
int input_fd;

/**
* Synchronization tools
*/
pthread_mutex_t mutex_queue;
pthread_mutex_t mutex_th_pool;
pthread_cond_t cond_queue_empty;
pthread_cond_t cond_thread_busy;
pthread_cond_t cond_resize_need;

/* rezier thread */
pthread_t resizer_thread;

/* below are for readers / writes paradigm */
pthread_mutex_t mutex_db;
pthread_cond_t cond_db_okToWrite;
pthread_cond_t cond_db_okToRead;
int AR = 0;
int WR = 0;
int AW = 0;
int WW = 0;

/* lock mutex for output logfile */ 
pthread_mutex_t logfile_mutex;

/* take measures against double instantiation */
sem_t* test_sem;

/* cancel requested by SIGINT flag */
volatile sig_atomic_t cancel_request = 0;

/**
* Becomes daemon process.
*/
int startDaemon(int logfile_fd,int inputfile_fd);

/**
* parse the command line arguments
* returns 1 if there is an error, returns 0 on succes.
*/
int parse_args(char* filepath,int* port,char* pathlogfile,int* thnum_starup,int* max_th,int argc,char *argv[]);

/**
* returns 0 if all arguments are given, returns 1 on error.
*/
int test_all_args_given(int param_i_test,int param_p_test,int param_o_test,int param_s_test,int param_x_test);

/**
* Print help msg.
*/
void print_usage();

/**
* Stars the server for incoming connections.
*/
void start_server(int PORT);
/**
* Loads graph from file
*/
void load_graph_from_file(int input_fd);

/**
* Initialize the server.
*/
void init_server(int input_fd,int init_thread_size);

/**
* Worker thread for handling the incoming connections.
*/
void* worker_thread(void* arg);

/**
* Thread pool resizer thread.
*/
void* resizer_function(void* arg);

/**
* Check if thread pool resizing is mandatory.
*/
int is_resize_need();

/**
* Search path from database.
*/
char* search_database(int from,int to);

/**
* Add new path to the database.
*/
void add_database(path_t* path);

/**
* Terminate the process gracefully.
*/
void handler_sigint(int signo);

int main(int argc, char *argv[])
{	
	/* setup the SIGINT handler */
	struct sigaction sigint_action;
	memset(&sigint_action,0,sizeof(sigint_action));
	sigint_action.sa_handler = &handler_sigint;
	sigaction(SIGINT,&sigint_action,NULL);

	sigset_t set_sigint_blocked;
	sigemptyset(&set_sigint_blocked);
	sigaddset(&set_sigint_blocked,SIGINT);

	/* block SIGINT during initialization */
	sigprocmask(SIG_BLOCK,&set_sigint_blocked,NULL);

	int port;
	char inputfile_name[400];
	char logfile_name[400];

	if(parse_args(inputfile_name,&port,logfile_name,&init_thread_size,&max_thread_size,argc,argv))
		exit(EXIT_FAILURE);

	/* create test semaphore for avoid multiple instances of program running
    * at the same time */
    test_sem = sem_open(SERVER_SEMAPHORE_NAME,O_CREAT | O_EXCL,0644,0);

    if(errno == EEXIST)
    {
    	char errmsg[250];
    	sprintf(errmsg,"Another instance of Server running...!\n");
    	write(STDERR_FILENO,errmsg,strlen(errmsg));
    	exit(EXIT_FAILURE);
    }

	/* open or create the log file */
	log_file_fd = open(logfile_name,O_CREAT | O_TRUNC | O_RDWR , S_IRWXU);
	if(log_file_fd == -1)
	{	
		char errmsg[200];
		sprintf(errmsg,"Error at opening log file : %s",strerror(errno));
		write(STDERR_FILENO,errmsg,strlen(errmsg));
		sem_close(test_sem);
		sem_unlink(SERVER_SEMAPHORE_NAME);
		exit(EXIT_FAILURE);
	}

	/* check input file */
	input_fd = open(inputfile_name,O_RDONLY);

	if(input_fd == -1){
		char errmsg[200];
		sprintf(errmsg,"Error at opening input file : %s",strerror(errno));
		write(log_file_fd,errmsg,strlen(errmsg));
		sem_close(test_sem);
		sem_unlink(SERVER_SEMAPHORE_NAME);
		exit(EXIT_FAILURE);
	}

	/* Now, Become a daemon process..*/
	if(-1 == startDaemon(log_file_fd,input_fd)){
		char errmsg[200];
		sprintf(errmsg,"Daemon cannot be created!\n");
		write(log_file_fd,errmsg,strlen(errmsg));
		sem_close(test_sem);
		sem_unlink(SERVER_SEMAPHORE_NAME);
		exit(EXIT_FAILURE);
	}
	
	/* check init thread size */
	if(init_thread_size < 2 || init_thread_size > max_thread_size)
	{
		char errmsg[200];
		sprintf(errmsg,"Invalid init thread size given : %d\n",init_thread_size);
		write(log_file_fd,errmsg,strlen(errmsg));
		sem_close(test_sem);
		sem_unlink(SERVER_SEMAPHORE_NAME);
		exit(EXIT_FAILURE);
	}

	/* check max thread size */
	if(max_thread_size < 0)
	{
		char errmsg[100];
		sprintf(errmsg,"Max thread size must be positive..\n");
		write(log_file_fd,errmsg,strlen(errmsg));
		sem_close(test_sem);
		sem_unlink(SERVER_SEMAPHORE_NAME);
		exit(EXIT_FAILURE);
	}

	char* start_msg = (char*)calloc(sizeof(char),1000);
	sprintf(start_msg,"Executing with parameters :\n-i %s\n-p %d\n-o %s\n-s %d\n-x %d\n\n",inputfile_name,port,logfile_name,init_thread_size,max_thread_size);
	write(log_file_fd,start_msg,strlen(start_msg));
	free(start_msg);

	/* Initialize server components */
	init_server(input_fd,init_thread_size);

	/* initialization done, unblock SIGINT */
	sigprocmask(SIG_UNBLOCK,&set_sigint_blocked,NULL);

	/* Start listening connections */
	start_server(port);
	return 0;
}

void start_server(int PORT)
{
	/* CREATE SERVER */
	int server_fd, new_socket; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    
    /* Create TCP socket */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        char errmsg[250];
        sprintf(errmsg,"Socket creation failed : %s\n",strerror(errno));
        write(log_file_fd,errmsg,strlen(errmsg));
        kill(getpid(),SIGINT); // terminate the process by SIGINT
    } 
    
    /* Set socket options */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))) 
    { 	
    	char errmsg[250];
        sprintf(errmsg,"%s\n",strerror(errno));
        write(log_file_fd,errmsg,strlen(errmsg)); 
        kill(getpid(),SIGINT); // terminate the process by SIGINT
    }

    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(PORT); 
       
    if (bind(server_fd,(struct sockaddr *)&address,sizeof(address)) < 0) 
    { 	
    	char errmsg[250];
        sprintf(errmsg,"bind failed : %s\n",strerror(errno));
        write(log_file_fd,errmsg,strlen(errmsg));  
        kill(getpid(),SIGINT); // terminate the process by SIGINT
    } 
    if (listen(server_fd, SERVER_BACKLOG) < 0) 
    { 	
    	char errmsg[250];
        sprintf(errmsg,"listen : %s\n",strerror(errno));
        write(log_file_fd,errmsg,strlen(errmsg)); 
        kill(getpid(),SIGINT); // terminate the process by SIGINT
    }

    sigset_t set_sigint_blocked;
	sigemptyset(&set_sigint_blocked);
	sigaddset(&set_sigint_blocked,SIGINT);

	pthread_sigmask(SIG_BLOCK,&set_sigint_blocked,NULL);

    /**
    * Start accepting the connections.
    */
    while(!cancel_request)
    { 	
    	pthread_sigmask(SIG_UNBLOCK,&set_sigint_blocked,NULL); 
	    new_socket = accept(server_fd, (struct sockaddr *)&address,(socklen_t*)&addrlen);
	    pthread_sigmask(SIG_BLOCK,&set_sigint_blocked,NULL);

	    /* If SIGINT arrives, break */
	    if(cancel_request)
	    	break;

	    int* client_fd = (int*)calloc(sizeof(int),1);
	    *client_fd = new_socket;

	    /* if no thread is available, wait */
		pthread_mutex_lock(&mutex_th_pool);
	    while(busy_threads == thread_pool->size)
	    {
	    	pthread_mutex_lock(&logfile_mutex);
	    	char* wait_msg = (char*)calloc(sizeof(char),200);
	    	char* time_str = time_as_string();
	    	sprintf(wait_msg,"%s | No thread is available! Waiting for one.\n",time_str);
	    	write(log_file_fd,wait_msg,strlen(wait_msg));
	    	free(time_str);
	    	free(wait_msg);
	    	pthread_mutex_unlock(&logfile_mutex);
	    	pthread_cond_wait(&cond_thread_busy,&mutex_th_pool);
	    }
	    pthread_mutex_unlock(&mutex_th_pool);

	    /* lock shared job queue */
	    pthread_mutex_lock(&mutex_queue);

	    /* push connection fd to job queue */
	    LinkedList_add(client_queue,client_fd);

	    /* wake up a suspended thread to handle the connection */
	    pthread_cond_signal(&cond_queue_empty);

	    /* unlock the mutex */
	    pthread_mutex_unlock(&mutex_queue);  
	}

	/********* clear resources *********/

	char sigmsg[100];
	sprintf(sigmsg,"Termination signal received, waiting for ongoing threads to complete.\n");
	write(log_file_fd,sigmsg,strlen(sigmsg));

	/* wait all busy threads to finish their jobs, clear the resources */
	node_t* head = thread_pool->head;
	for (int i = 0; i < thread_pool->size; ++i)
	{
		pthread_join(*(pthread_t*)head->item,NULL);
		head = head->next;
	}

	graph_clear(graph);
	LinkedList_clear(client_queue);
	LinkedList_clear(thread_pool);
	LinkedList_clear(thread_ids);
		
	head = cache_database->head;
	node_t* tmp = head;
	while (head != NULL)
	{	
		tmp = head;
		head = head->next;
		path_t* pt = (path_t*)tmp->item;
		free(pt->str);
		free(pt);
		free(tmp);
	}
	free(cache_database);

	/* clear synchronization tools */
	pthread_mutex_destroy(&mutex_th_pool);
	pthread_mutex_destroy(&mutex_queue);
	pthread_mutex_destroy(&mutex_db);
	pthread_mutex_destroy(&logfile_mutex);

	pthread_cond_destroy(&cond_thread_busy);
	pthread_cond_destroy(&cond_queue_empty);
	pthread_cond_destroy(&cond_resize_need);
	pthread_cond_destroy(&cond_db_okToWrite);
	pthread_cond_destroy(&cond_db_okToRead);

	/********* clear resources *********/

	char sigmsg2[100];
	sprintf(sigmsg2,"All threads have terminated, server shutting down.\n");
	write(log_file_fd,sigmsg2,strlen(sigmsg2));

	close(log_file_fd);

	sem_close(test_sem);
	sem_unlink(SERVER_SEMAPHORE_NAME);

	/* clearing done, terminate the server */
	exit(1);
}
void* worker_thread(void* arg)
{	
	sigset_t set_sigint_blocked;
	sigemptyset(&set_sigint_blocked);
	sigaddset(&set_sigint_blocked,SIGINT);

	/** Only main thread catch the SIGINT signal **/
	pthread_sigmask(SIG_BLOCK,&set_sigint_blocked,NULL);

	/* set cancel state and type */
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
	
	int thread_id = (*(int*)arg);
	char buffer[BUFFER_SIZE] = {0};

	while(TRUE)
	{ 	
		char* thread_msg = (char*)calloc(sizeof(char),200);
		char* time_str = time_as_string();
		sprintf(thread_msg,"%s | Thread #%d: waiting for connection...\n",time_str,thread_id);
		write(log_file_fd,thread_msg,strlen(thread_msg));
		free(thread_msg);
		free(time_str);

		/* wait client to be handed by the server */
		pthread_mutex_lock(&mutex_queue);
		while(client_queue->size == 0 && !cancel_request)
			pthread_cond_wait(&cond_queue_empty,&mutex_queue);

		/* If SIGINT arrived, terminate the thread */
		if(cancel_request){
			pthread_mutex_unlock(&mutex_queue);
			return NULL;
		}

		/* get socked fd for communication with client */
		int* cfd_ptr = (int*)LinkedList_pop(client_queue);
		int client_fd = *cfd_ptr;
		free(cfd_ptr);

		/* unlock the request queue */
		pthread_mutex_unlock(&mutex_queue);

		/* increment busy thread, signal the resizer thread */
		pthread_mutex_lock(&mutex_th_pool);
		++busy_threads;
		if(thread_pool->size != max_thread_size)
			pthread_cond_signal(&cond_resize_need);

		double sys_load = (100*busy_threads) / thread_pool->size;
		pthread_mutex_unlock(&mutex_th_pool);

		pthread_mutex_lock(&logfile_mutex);
		char* th_connect_msg = (char*)calloc(sizeof(char),300);
		time_str = time_as_string();
		sprintf(th_connect_msg,"%s | A connection has been delegated to thread id #%d, system load %.1f%%\n",time_str,thread_id,sys_load);
		write(log_file_fd,th_connect_msg,strlen(th_connect_msg));
		free(th_connect_msg);
		free(time_str);
		pthread_mutex_unlock(&logfile_mutex);

		/* Start process the request */
		/* read client request as string, parse it into array */
		read(client_fd,buffer,BUFFER_SIZE);

		int* request = parse_request(buffer);
		int from_node = request[0];
		int to_node = request[1];
		free(request);

		/* first, look cache data structure (DB), (reader) */
		pthread_mutex_lock(&mutex_db);
		
		/* If there are writers, wait them to finish */
		while((AW + WW) > 0)
		{
			WR++;
			pthread_cond_wait(&cond_db_okToRead,&mutex_db);
			WR--;
		}
		AR++;
		pthread_mutex_unlock(&mutex_db);

		pthread_mutex_lock(&logfile_mutex);
		char* search_msg = (char*)calloc(sizeof(char),300);
		time_str = time_as_string();
		sprintf(search_msg,"%s | Thread #%d: searching database for a path from node %d to node %d\n",time_str,thread_id,from_node,to_node);
		write(log_file_fd,search_msg,strlen(search_msg));
		free(search_msg);
		free(time_str);
		pthread_mutex_unlock(&logfile_mutex);

		/* search the database */
		char* test_path = search_database(from_node,to_node);

		/* reading from DB is done */
		pthread_mutex_lock(&mutex_db);
		AR--;
		if(AR == 0 && WW > 0)
			pthread_cond_signal(&cond_db_okToWrite);
		pthread_mutex_unlock(&mutex_db);

		if(test_path != NULL)
		{	
			pthread_mutex_lock(&logfile_mutex);
			char* db_found = (char*)calloc(sizeof(char),strlen(test_path)+200);
			time_str = time_as_string();
			sprintf(db_found,"%s | Thread #%d: path found in database: %s\n",time_str,thread_id,test_path);
			write(log_file_fd,db_found,strlen(db_found));
			free(db_found);
			free(time_str);
			pthread_mutex_unlock(&logfile_mutex);

			/* send response to the client */
			write(client_fd,test_path,strlen(test_path));
		}
		else
		{	
			pthread_mutex_lock(&logfile_mutex);
			char* db_not_found = (char*)calloc(sizeof(char),300);
			time_str = time_as_string();
			sprintf(db_not_found,"%s | Thread #%d: no path in database, calculating %d->%d\n",time_str,thread_id,from_node,to_node);
			write(log_file_fd,db_not_found,strlen(db_not_found));
			free(db_not_found);
			free(time_str);
			pthread_mutex_unlock(&logfile_mutex);

			/* path not found in the database, BFS on graph */
			char* response_path = graph_BFS(graph,from_node,to_node);

			if(strcmp(response_path,"NO PATH !") == 0)
			{	
				pthread_mutex_lock(&logfile_mutex);
				char* p_not_found = (char*)calloc(sizeof(char),300);
				time_str = time_as_string();
				sprintf(p_not_found,"%s | Thread #%d: path not possible from node %d to %d\n",time_str,thread_id,from_node,to_node);
				write(log_file_fd,p_not_found,strlen(p_not_found));
				free(p_not_found);
				free(time_str);
				pthread_mutex_unlock(&logfile_mutex);
			}
			else
			{	
				pthread_mutex_lock(&logfile_mutex);
				char* p_found = (char*)calloc(sizeof(char),strlen(response_path)+200);
				time_str = time_as_string();
				sprintf(p_found,"%s | Thread #%d: path calculated : %s\n",time_str,thread_id,response_path);
				write(log_file_fd,p_found,strlen(p_found));
				free(p_found);
				free(time_str);
				pthread_mutex_unlock(&logfile_mutex); 
			}

			pthread_mutex_lock(&logfile_mutex);
			char* send_msg = (char*)calloc(sizeof(char),300);
			time_str = time_as_string();
			sprintf(send_msg,"%s | Thread #%d: responding to client and adding path to database\n",time_str,thread_id);
			write(log_file_fd,send_msg,strlen(send_msg));
			free(send_msg);
			free(time_str);
			pthread_mutex_unlock(&logfile_mutex); 

			/* send response to the client */
			write(client_fd,response_path,strlen(response_path));

			/* update the database, now thread becomes (writer) */

			pthread_mutex_lock(&mutex_db);
			while((AW + AR) > 0)
			{
				WW++;
				pthread_cond_wait(&cond_db_okToWrite,&mutex_db);
				WW--;
			}
			AW++;
			pthread_mutex_unlock(&mutex_db);

			/* update the database,add new calculated path */

			path_t* path = (path_t*)calloc(sizeof(path_t),1);
			path->from = from_node;
			path->to = to_node;
			path->str = (char*)calloc(sizeof(char),strlen(response_path)+1);
			strcpy(path->str,response_path);
			free(response_path);

			/* add new path */
			add_database(path);

			/* updating the DB is done */

			pthread_mutex_lock(&mutex_db);
			AW--;
			if(WW > 0)
				pthread_cond_signal(&cond_db_okToWrite);
			else if(WR > 0)
				pthread_cond_broadcast(&cond_db_okToRead);

			pthread_mutex_unlock(&mutex_db);
		}
		
		/* clear the buffer */
		memset(buffer,0,sizeof(buffer));

		close(client_fd);

		/* Decrement busy threads */
		pthread_mutex_lock(&mutex_th_pool);
		--busy_threads;
		pthread_cond_broadcast(&cond_thread_busy);
		pthread_mutex_unlock(&mutex_th_pool);

		/* If SIGINT arrived, terminate the thread */
		if(cancel_request)
			return NULL;
	}
}

void* resizer_function(void* arg)
{	
	/* No need for the join the rezier thread, so detach it */
	if(-1 == pthread_detach(pthread_self())){
		char errmsg[350];
		sprintf(errmsg,"%s | Error occured at resizer thread : %s",time_as_string(),strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	int reach_max_limit = 0;

	while(1)
	{	
		pthread_mutex_lock(&mutex_th_pool);
		while(!is_resize_need() && !cancel_request)
			pthread_cond_wait(&cond_resize_need,&mutex_th_pool);

		if(cancel_request){
			pthread_mutex_unlock(&mutex_th_pool);
			return NULL;
		}

		/* create new threads into the thread pool */
		int needed_thread_num = thread_pool->size / 4;
		
		if(needed_thread_num <= 0)
			needed_thread_num = 1;

		if(needed_thread_num + thread_pool->size >= max_thread_size)
			needed_thread_num = max_thread_size - thread_pool->size;
		
		int new_pool_size = thread_pool->size + needed_thread_num;

		int th_id_start = thread_pool->size;
		for (int i = 0; i < needed_thread_num; ++i)
		{
			int* id = (int*)calloc(sizeof(int),1);
			*id = th_id_start;
			++th_id_start;
			LinkedList_add(thread_ids,id);

			pthread_t* new_thread = (pthread_t*)calloc(sizeof(pthread_t),1);
			pthread_create(new_thread,NULL,worker_thread,LinkedList_get_tail(thread_ids));
			LinkedList_add(thread_pool,new_thread);
		}

		if(thread_pool->size == max_thread_size)
			reach_max_limit = 1;
		
		pthread_mutex_unlock(&mutex_th_pool);

		pthread_mutex_lock(&logfile_mutex);
		char* resize_msg = (char*)calloc(sizeof(char),300);
		char* time_str = time_as_string();
		sprintf(resize_msg,"%s | System load reached 75%%, pool extended to %d threads...\n",time_str,new_pool_size);
		write(log_file_fd,resize_msg,strlen(resize_msg));
		free(resize_msg);
		free(time_str);
		pthread_mutex_unlock(&logfile_mutex);

		/* If max thread limit is reached, terminate the resizer thread */
		if(reach_max_limit)
			return NULL;
	}
}
int is_resize_need()
{	
	/* If load reaches the 75%, return true */
	return (busy_threads >= (((thread_pool->size)*3) / 4));
}

void load_graph_from_file(int input_fd)
{	
	char* info_str = (char*)calloc(sizeof(char),400);
	char* time_str = time_as_string();
	sprintf(info_str,"%s | Loading graph..\n",time_str);
	write(log_file_fd,info_str,strlen(info_str));
	free(time_str);

	clock_t begin = clock();
	graph = create_graph_from_file(input_fd);
	clock_t end = clock();
	double load_time = (double)(end - begin) / CLOCKS_PER_SEC;

	time_str = time_as_string();
	sprintf(info_str,"%s | Graph loaded in %.5f seconds with %d nodes and %d edges.\n",time_str,load_time,graph->vertex,graph->edge);
	write(log_file_fd,info_str,strlen(info_str));
	free(info_str);
	free(time_str);
}
void init_server(int input_fd,int init_thread_size)
{
	char* init_msg = (char*)calloc(sizeof(char),300);
	char* time_str = time_as_string();
	sprintf(init_msg,"%s | A pool of %d threads creating...\n",time_str,init_thread_size);
	write(log_file_fd,init_msg,strlen(init_msg));
	free(init_msg);
	free(time_str);

	load_graph_from_file(input_fd);

	client_queue = LinkedList_create();
	thread_pool = LinkedList_create();
	thread_ids = LinkedList_create();
	cache_database = LinkedList_create();

	/* initialize synchronization tools */
	pthread_mutex_init(&mutex_queue,NULL);
	pthread_mutex_init(&mutex_db,NULL);
	pthread_mutex_init(&mutex_th_pool,NULL);
	pthread_mutex_init(&logfile_mutex,NULL);
	pthread_cond_init(&cond_queue_empty,NULL);
	pthread_cond_init(&cond_thread_busy,NULL);
	pthread_cond_init(&cond_db_okToWrite,NULL);
	pthread_cond_init(&cond_db_okToRead,NULL);
	pthread_cond_init(&cond_resize_need,NULL);

	for (int i = 0; i < init_thread_size; ++i)
	{	
		/* set thread id */
		int* id = (int*)calloc(sizeof(int),1);
		*id = i;
		LinkedList_add(thread_ids,id);

		pthread_t* thread = (pthread_t*)calloc(sizeof(pthread_t),1);
		pthread_create(thread,NULL,worker_thread,LinkedList_get_tail(thread_ids));
		LinkedList_add(thread_pool,thread);
	}

	pthread_create(&resizer_thread,NULL,resizer_function,NULL);
}

char* search_database(int from,int to)
{
	node_t* head = cache_database->head;
	for (int i = 0; i < cache_database->size; ++i)
	{
		path_t* path = (path_t*)head->item;
		if(path->from == from && path->to == to)
			return path->str; // path found in database
	
		head = head->next;
	}

	/* path not found in database */
	return NULL;
}
void add_database(path_t* path)
{	
	/* check contains */
	node_t* head = cache_database->head;
	for (int i = 0; i < cache_database->size; ++i)
	{
		path_t* tpath = (path_t*)head->item;
		if(tpath->to == path->to && tpath->from == path->from){
			free(path->str);
			free(path);
			return;
		}
		head = head->next;
	}

	/* if not contains, add the path */
	LinkedList_add(cache_database,path);
}
int startDaemon(int logfile_fd,int inputfile_fd)
{	
	pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {	
    	/* do not close the input file or log file */
    	if(x != logfile_fd && x != input_fd)
        	close(x);
    }

    int fd = open("/dev/null",O_RDWR);

    if (fd != STDIN_FILENO)
		return -1;
	if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
		return -1;
	if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
		return -1;

	return 0;
}

void handler_sigint(int signo)
{	
	cancel_request = 1;
	pthread_cond_broadcast(&cond_queue_empty);
	pthread_cond_signal(&cond_resize_need);
}

/**
* parse the command line arguments
* returns 1 if there is an error, returns 0 on succes.
*/
int parse_args(char* filepath,int* port,char* pathlogfile,int* thnum_starup,int* max_th,int argc,char *argv[])
{
	int param_i_test=0,param_p_test=0,param_o_test=0,param_s_test=0,param_x_test=0;
    int opt;
   
    while ((opt = getopt(argc, argv,"i:p:o:s:x:")) != -1)
    {   
        switch(opt)
        {   
            case 'i':   
            sprintf(filepath,"%s",optarg);
            param_i_test = 1;
            break;

            case 'p':
            *port = atoi(optarg);
            param_p_test = 1;
            break;

            case 'o':
            sprintf(pathlogfile,"%s",optarg);
            param_o_test = 1;
            break;

            case 's':
            *thnum_starup = atoi(optarg);
            param_s_test = 1;
            break;

            case 'x':
            *max_th = atoi(optarg);
            param_x_test = 1;
            break;

            default:
            print_usage();
            printf("Program terminated !\n");
            return 1; // error
            break;
                
        }
    }   
    return test_all_args_given(param_i_test,param_p_test,param_o_test,param_s_test,param_x_test);
}


/**
* returns 0 if all arguments are given, returns 1 on error.
*/
int test_all_args_given(int param_i_test,int param_p_test,int param_o_test,int param_s_test,int param_x_test)
{
	if(!param_i_test){
        char info_msg[60];
        sprintf(info_msg,"[-i] argument not given !\nProgram Terminated\n\n");
        write(STDERR_FILENO,info_msg,strlen(info_msg));
        print_usage();
        return 1; // error
    }
    if(!param_p_test){
        char info_msg[60];
        sprintf(info_msg,"[-p] argument not given !\nProgram Terminated\n\n");
        write(STDERR_FILENO,info_msg,strlen(info_msg));
        print_usage();
        return 1; // error
    }
    if(!param_o_test){
        char info_msg[60];
        sprintf(info_msg,"[-o] argument not given !\nProgram Terminated\n\n");
        write(STDERR_FILENO,info_msg,strlen(info_msg));
        print_usage();
        return 1; // error
    }
    if(!param_s_test){
        char info_msg[60];
        sprintf(info_msg,"[-s] argument not given !\nProgram Terminated\n\n");
        write(STDERR_FILENO,info_msg,strlen(info_msg));
        print_usage();
        return 1; // error
    }
    if(!param_x_test){
        char info_msg[60];
        sprintf(info_msg,"[-x] argument not given !\nProgram Terminated\n\n");
        write(STDERR_FILENO,info_msg,strlen(info_msg));
        print_usage();
        return 1; // error
    }

    return 0;
}
void print_usage()
{
	char info_msg[100];
    sprintf(info_msg,"Usage : ../server -i inputfile -p PORT -o logfile -s poolSize -x maxPoolSize\n");
    write(STDERR_FILENO,info_msg,strlen(info_msg));
}