/**
* SERVER TESTER & DEBUGGER
* Goktug Akin.
*
* [!] [NOT PART OF THE HOMEWORK]
*
* This process creates clients argv[3] times wit given argv[1]=(source) and
* argv[2]=(destination) for breadh first search on graph.
*
* You can change the IP adress like 127.0.0.1 or 
* for different machines : 192.168.1.X
*/

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char const *argv[])
{
	char *argvs[] = {"client","-a","127.0.0.1","-p","8080","-s",argv[1],"-d",argv[2],(char*)0};
	
	for (int i = 0; i < atoi(argv[3]); ++i)
		if(fork() == 0)
			execve(argvs[0],argvs,NULL);

	/* wait all clients .. */	
	while(wait(NULL)>0);

	return 0;
}
