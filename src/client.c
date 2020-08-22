#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/time.h>
#include <stdlib.h>
#include "utils.h"

/**
* parse the command line arguments
* returns 1 if there is an error, returns 0 on succes.
*/
int parse_args(char* ip_adress,int* port,int* source,int* dest,int argc,char *argv[]);

/**
* returns 0 if all arguments are given, returns 1 on error.
*/
int test_all_args_given(int param_a_test,int param_p_test,int param_s_test,int param_d_test);

/**
* Print help msg.
*/
void print_usage();

/**
* Test the input string.
*/
int test_valid_input(const char* input);

   
int main(int argc, char *argv[]) 
{   

    char IP_ADRESS[100];
    int port,source,dest;
   
    if(parse_args(IP_ADRESS,&port,&source,&dest,argc,argv))
        exit(EXIT_FAILURE);
    
    if(source < 0 || dest < 0)
    {
        char errmsg[250];
        sprintf(errmsg,"Invalid source / destination param. Must be >= 0\n");
        write(STDERR_FILENO,errmsg,strlen(errmsg));
        exit(EXIT_FAILURE);
    }

    char port_str[20];
    sprintf(port_str,"%d",port);
    
    /* create string to send to the server */
    char source_str[200];
    char dest_str[200];
    sprintf(source_str,"%d",source);
    sprintf(dest_str,"%d",dest);

    char str[BUFFER_SIZE];
    strcpy(str,source_str);
    strcat(str,"|");
    strcat(str,dest_str);

    int sock = 0; 
    struct sockaddr_in serv_addr; 
    char buffer[BUFFER_SIZE] = {0}; 

    char* client_start_msg = (char*)calloc(sizeof(char),400);
    sprintf(client_start_msg,"%s | Client (%d) connecting to %s:%s\n",time_as_string(),getpid(),IP_ADRESS,port_str);
    write(STDOUT_FILENO,client_start_msg,strlen(client_start_msg));
    free(client_start_msg);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        char errmsg[200];
        sprintf(errmsg,"Unable the create socket : %s",strerror(errno));
        write(STDERR_FILENO,errmsg,strlen(errmsg));
        return -1; 
    } 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(port); 
       
    if(inet_pton(AF_INET, IP_ADRESS, &serv_addr.sin_addr)<=0)  
    {   
        char errmsg[200];
        sprintf(errmsg,"Invalid address / Address not supported : %s\n",strerror(errno));
        write(STDERR_FILENO,errmsg,strlen(errmsg)); 
        return -1; 
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {   
        char errmsg[200];
        printf("Connection Failed : %s\n",strerror(errno));
        write(STDERR_FILENO,errmsg,strlen(errmsg));  
        return -1; 
    }

    char* client_send_msg = (char*)calloc(sizeof(char),400);
    sprintf(client_send_msg,"%s | Client (%d) connected to requesting a path from node %s to %s\n",time_as_string(),getpid(),source_str,dest_str);
    write(STDOUT_FILENO,client_send_msg,strlen(client_send_msg));
    free(client_send_msg);

    struct timeval start,end;

    /* send request to the server */
    write(sock,str,strlen(str));
    
    gettimeofday(&start,NULL); // get start time

    /* wait response and read */
    read(sock,buffer,BUFFER_SIZE);

    gettimeofday(&end,NULL); // get end time

    /* calculate response time */
    double arrive_time = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6; // in seconds

    char* client_res_msg = (char*)calloc(sizeof(char),strlen(buffer) + 300);
    sprintf(client_res_msg,"%s | Server's response to (%d) : ",time_as_string(),getpid());
    strcat(client_res_msg,buffer);
    sprintf(client_res_msg,"%s, arrived in %.3f seconds\n",client_res_msg,arrive_time);
    write(STDOUT_FILENO,client_res_msg,strlen(client_res_msg));
    free(client_res_msg);

    close(sock);
    return 0; 
}

int parse_args(char* ip_adress,int* port,int* source,int* dest,int argc,char *argv[])
{
    int param_a_test=0,param_p_test=0,param_s_test=0,param_d_test=0;
    int opt;
   
    while ((opt = getopt(argc, argv,"a:p:s:d:")) != -1)
    {   
        switch(opt)
        {   
            case 'a':   
            sprintf(ip_adress,"%s",optarg);
            param_a_test = 1;
            break;

            case 'p':
            if(!test_valid_input(optarg)){
                char errmsg[250];
                sprintf(errmsg,"[-p] option needs numeric value!\n");
                write(STDERR_FILENO,errmsg,strlen(errmsg));
                return 1;
            }
            *port = atoi(optarg);
            param_p_test = 1;
            break;

            case 's':
            if(!test_valid_input(optarg)){
                char errmsg[250];
                sprintf(errmsg,"[-s] option needs numeric value!\n");
                write(STDERR_FILENO,errmsg,strlen(errmsg));
                return 1;
            }
            *source = atoi(optarg);
            param_s_test = 1;
            break;

            case 'd':
            if(!test_valid_input(optarg)){
                char errmsg[250];
                sprintf(errmsg,"[-s] option needs numeric value!\n");
                write(STDERR_FILENO,errmsg,strlen(errmsg));
                return 1;
            }
            *dest = atoi(optarg);
            param_d_test = 1;
            break;

            default:
            print_usage();
            printf("Program terminated !\n");
            return 1; // error
            break;
                
        }
    }   
    return test_all_args_given(param_a_test,param_p_test,param_s_test,param_d_test);
}

/**
* returns 0 if all arguments are given, returns 1 on error.
*/
int test_all_args_given(int param_a_test,int param_p_test,int param_s_test,int param_d_test)
{
    if(!param_a_test){
        char info_msg[60];
        sprintf(info_msg,"[-a] argument not given !\nProgram Terminated\n\n");
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
    if(!param_s_test){
        char info_msg[60];
        sprintf(info_msg,"[-s] argument not given !\nProgram Terminated\n\n");
        write(STDERR_FILENO,info_msg,strlen(info_msg));
        print_usage();
        return 1; // error
    }
    if(!param_d_test){
        char info_msg[60];
        sprintf(info_msg,"[-d] argument not given !\nProgram Terminated\n\n");
        write(STDERR_FILENO,info_msg,strlen(info_msg));
        print_usage();
        return 1; // error
    }

    return 0;
}
void print_usage()
{
    char info_msg[100];
    sprintf(info_msg,"Usage : ../client -a IP ADRESS -p PORT -s SOURCE -d DEST\n");
    write(STDERR_FILENO,info_msg,strlen(info_msg));
}
int test_valid_input(const char* input)
{
    for (int i = 0; i < strlen(input); ++i)
        if(input[i]<48 || input[i]>57)
            return 0;
    return 1;
}