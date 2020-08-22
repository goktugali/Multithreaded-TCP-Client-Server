#include "utils.h"

char* time_as_string()
{
	time_t t = time(NULL);
  	struct tm tm = *localtime(&t);
  	char* as_string = (char*)calloc(sizeof(char),200);
  	sprintf(as_string,"%02d:%02d:%02d",tm.tm_hour, tm.tm_min, tm.tm_sec);
  	return as_string;
}

char ** read_file(int input_fd,int* lines)
{	
	int fd = input_fd;
	
	size_t size = lseek(fd,0,SEEK_END);
	if(size <= 0){
		close(fd);
		exit(EXIT_FAILURE);
	}

	lseek(fd,0,SEEK_SET);

	char * readed_file = (char*)calloc(sizeof(char),(size+1));
	read(fd,readed_file,size);
	readed_file[size] = '\0';
	close(fd);

	int line_size = char_count(readed_file,'\n',size) + 1;

	char** parsed_file = (char**)calloc(sizeof(char*),line_size);
	
	int index = 0;
	char * token = strtok(readed_file,"\n");
	while(token != NULL)
	{	
		parsed_file[index] = (char*)calloc(sizeof(char),strlen(token)+1);
		strcpy(parsed_file[index],token);
		
		++index;
		token = strtok(NULL,"\n");
	}
	free(readed_file);
	*lines = index;
	return parsed_file;	
}

int char_count(const char* string, char c,size_t size)
{	
	int ctr = 0;
	for (int i = 0; i < size; ++i)
		if(string[i] == c)
			++ctr;
	return ctr;
}
/**
* Reverse the given string.
*/
void str_reverse(char* string)
{
	int len = strlen(string);
	int len_idx = len-1;
	for (int i = 0; i < len; ++i)
		string[len_idx--] = string[i];
	
}
char* remove_whitespace(const char* token)
{
	char* token_cpy = (char*)calloc(sizeof(char),strlen(token) + 1);
	
	int index = 0;
	for (int i = 0; i < strlen(token); ++i)
	{
		if(token[i] != ' '){
			token_cpy[index] = token[i];
			++index;
		}
	}

	return token_cpy;
}

void _exit_invalid_file(const char* filepath)
{
	char errmsg[300];
	sprintf(errmsg,"File '%s' is not a valid file !\n",filepath);
	write(STDERR_FILENO,errmsg,strlen(errmsg));
	exit(EXIT_FAILURE);
}

/******** LINKED LIST FUNCTIONS ********/
LinkedList* LinkedList_create()
{	
	LinkedList* list = (LinkedList*)calloc(sizeof(LinkedList),1);
	list->size = 0;
	list->head = NULL;
	list->tail = NULL;
	return list;
}
void* LinkedList_at(const LinkedList* list, int index)
{	
	node_t* head = list->head;
	for (int i = 0; i < index; ++i)
		head = head->next;
	return head->item;
}
void LinkedList_add(LinkedList* list,void* item)
{
	if(list->size == 0)
	{
		node_t* newnode = (node_t*)calloc(sizeof(node_t),1);
		newnode->item = item;
		newnode->next = NULL;
		list->size = 1;
		list->head = newnode;
		list->tail = newnode;
	}
	else
	{
		node_t* newnode = (node_t*)calloc(sizeof(node_t),1);
		newnode->item = item;
		newnode->next = NULL;
		list->tail->next = newnode;
		list->tail = list->tail->next;
		list->size = list->size + 1;
	}
}

void* LinkedList_get_tail(LinkedList* list)
{
	return list->tail->item;
}

void LinkedList_clear(LinkedList* list)
{
	if(list == NULL)
		return;
	
	node_t* head = list->head;
	node_t* tmp = head;

	while(head != NULL)
	{
		tmp = head;
		head = head->next;

		if(tmp->item != NULL)
			free(tmp->item);
		free(tmp);
	}
	free(list);
}

void* LinkedList_pop(LinkedList* list)
{
	void* head_item = list->head->item;
	void* head = list->head;

	/* skip head pointer to the next item */
	list->head = list->head->next;
	list->size = list->size - 1;

	free(head);
	return head_item;
}

/******** GRAPH FUNCTIONS ********/
Graph* graph_create(int vertex)
{
	Graph* res_graph = (Graph*)calloc(sizeof(Graph),1);
	res_graph->adj = (LinkedList**)calloc(sizeof(LinkedList*),vertex);

	for (int i = 0; i < vertex; ++i)
		res_graph->adj[i] = LinkedList_create();
	
	res_graph->vertex = vertex;
	return res_graph;
}

void graph_addEdge(Graph* graph,int from, int to)
{
	LinkedList** adjs = graph->adj;
	int* to_val = (int*)calloc(sizeof(int),1);
	*to_val = to;
	LinkedList_add(adjs[from],to_val);
	graph->edge = graph->edge + 1;
}

void graph_clear(Graph* graph)
{
	for (int i = 0; i < graph->vertex; ++i)
		LinkedList_clear(graph->adj[i]);
	free(graph->adj);
	free(graph);
}

Graph* create_graph_from_file(int input_fd)
{	
	int lines;
	char** file_contents = read_file(input_fd,&lines);

	/* find start line of the graph entries */

	int start_line=0;
	for (int i = 0; i < lines; ++i)
	{
		if(file_contents[i][0] != '#'){
			start_line = i;
			break;
		}
	}

	/* find vertex number */
	int max = -1;
	for (int i = start_line; i < lines; ++i)
	{
		char* graph_entry = (char*)calloc(sizeof(char),strlen(file_contents[i])+1);
		char* bckp_addr = graph_entry;
		strcpy(graph_entry,file_contents[i]);

		char* from = strtok_r(graph_entry,"\t",&graph_entry);
		int from_node = atoi(from);
		int to_node = atoi(graph_entry);

		if(from_node > to_node){
			if(from_node > max)
				max = from_node;
		}
		else
		{
			if(to_node > max)
				max = to_node;
		}
		free(bckp_addr);
	}
	max = max+1;
	
	/* allocate for graph with given vertex number */
	Graph* res_graph = graph_create(max);
	res_graph->edge = 0;
	
	/* fill graph */
	for (int i = start_line; i < lines; ++i)
	{
		char* graph_entry = (char*)calloc(sizeof(char),strlen(file_contents[i])+1);
		char* bckp_addr = graph_entry;
		strcpy(graph_entry,file_contents[i]);

		char* from = strtok_r(graph_entry,"\t",&graph_entry);
		int from_node = atoi(from);
		int to_node = atoi(graph_entry);

		graph_addEdge(res_graph,from_node,to_node);
		free(bckp_addr);
	}

	/* initialize done, clear file contents */
	for (int i = 0; i < lines; ++i)
		free(file_contents[i]);
	free(file_contents);

	return res_graph;
}

char* path_as_string(int* parent,int from,int to,int vertex)
{	
	
	int need_byte = BUFFER_SIZE;
	
	char* nodestr = (char*)calloc(sizeof(char),vertex+1);
	char* res_path = (char*)calloc(sizeof(char),need_byte);

	/* use linked list as stack */

	LinkedList* reverse_path = LinkedList_create();
	int* item1 = (int*)calloc(sizeof(int),1);
	*item1 = to;
	LinkedList_add(reverse_path,item1);

	int index = parent[to];

	if(index == -1)
	{
		sprintf(res_path,"NO PATH !");
		free(nodestr);
		free(parent);
		LinkedList_clear(reverse_path);
		return res_path;
	}

	int test_path = 0;
	while(index != from && index != -1)
	{		
		int* item = (int*)calloc(sizeof(int),1);
		*item = index;
		LinkedList_add(reverse_path,item);
		index = parent[index];
		++test_path;
	}	

	int* item = (int*)calloc(sizeof(int),1);
	*item = from;
	LinkedList_add(reverse_path,item);

	int node = *(int*)LinkedList_at(reverse_path,reverse_path->size - 1);
	sprintf(nodestr,"%d",node);
	strcpy(res_path,nodestr);
	strcat(res_path,"->");

	for (int i = reverse_path->size-2; i > 0; i--)
	{
		int node = *(int*)LinkedList_at(reverse_path,i);
		sprintf(nodestr,"%d",node);
		strcat(res_path,nodestr);
		strcat(res_path,"->");
	}
	int node1 = *(int*)LinkedList_at(reverse_path,0);
	sprintf(nodestr,"%d",node1);
	strcat(res_path,nodestr);

	LinkedList_clear(reverse_path);
	free(nodestr);
	free(parent);
	return res_path;
}

int* parse_request(char* request_str)
{
	char* cpy = (char*)calloc(sizeof(char),strlen(request_str) + 1);
	char* bckp = cpy;
	strcpy(cpy,request_str);

	int* request = (int*)calloc(sizeof(int),2);

	request[0] = atoi(strtok_r(cpy,"|",&cpy));
	request[1] = atoi(cpy);

	free(bckp);

	return request;
}

char* graph_BFS(Graph* graph, int from, int to)
{

	if(from >= graph->vertex || to >= graph->vertex){
		char* res_str = (char*)calloc(sizeof(char),50);
		sprintf(res_str,"NO PATH !");
		return res_str;
	}

	LinkedList* queue = LinkedList_create();

	int* parent = (int*)calloc(sizeof(int),graph->vertex);
	for (int i = 0; i < graph->vertex; ++i)
		parent[i] = -1;

	int* visited = (int*)calloc(sizeof(int),graph->vertex);
	visited[from] = 1;

	int* start = (int*)calloc(sizeof(int),1);
	*start = from;

	LinkedList_add(queue,start);

	LinkedList** adjs = graph->adj;

	while(queue->size != 0)
	{
		int* current_ptr = (int*)LinkedList_pop(queue);
		int current = *current_ptr;
		free(current_ptr);

		node_t* head = adjs[current]->head;
		for (int i = 0; i < adjs[current]->size; ++i)
		{	
			int neighbour = *((int*)head->item);
			if(!visited[neighbour])
			{
				visited[neighbour] = 1;
				int* item = (int*)calloc(sizeof(int),1);
				*item = neighbour;
				LinkedList_add(queue,item);
				parent[neighbour] = current;
			}
			head=head->next;
		}	
	}

	free(visited);
	LinkedList_clear(queue);

	return path_as_string(parent,from,to,graph->vertex);
}
