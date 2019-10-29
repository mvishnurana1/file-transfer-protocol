#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>


#include "daemon.h"

void daemonInit(void)
{
	pid_t pid;

	if ((pid = fork()) < 0)
	{
		perror("fork");
		exit(1);
	}
	else if (pid > 0)
	{
		printf("Daemon PID = %d\n", pid);
		exit (0);	//close parient
	}
	//in child
	setsid();	//become session leader
	chdir("/");	//change working dir to root dir
	umask(0);	//clear file creation mask
}


void clameChildren(int i)
{
	pid_t pid;

	do
	{
		pid = waitpid(0, (int *)0, WNOHANG);
	}while (pid > 0);
}

void slayZombies(void)
{
	struct sigaction act;
	//catch SIGCHILD and remove zombies from system
	act.sa_handler = clameChildren;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, (struct sigaction *)&act, (struct sigaction *)0);
}
