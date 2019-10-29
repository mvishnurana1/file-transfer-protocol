#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
//#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "daemon.h"	//procedures to create daemon process and remove zombies
#include "mydefs.h"	//definitions of constants



void serveClient(int sd);

//setup server and wait for clients
int main()
{
	int sd, nsd, cliAddrLen;
	pid_t pid;
	struct sockaddr_in serAddr, cliAddr;

	//turn server into daemon
	daemonInit();
	//setup server to remove zombie children from system
	slayZombies();

	//setup listening socket
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Server:Socket");
		exit(1);
	}

	//build server internet socket address
	bzero((char *)&serAddr, sizeof(serAddr));	//set serAddr to NULLs
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(MY_SERV_TCP_PORT);
	serAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	//bind server address to socket sd
	if (bind(sd, (struct sockaddr *)&serAddr, sizeof(serAddr)) < 0)
	{
		perror("server:bind");
		exit(1);
	}

	//become a listening socket 
	listen(sd, LISTEN_BUF);

	for (;;)
	{
		//wait to accept a client request for connection
		cliAddrLen = sizeof(cliAddr);
		nsd = accept(sd, (struct sockaddr *)&cliAddr, &cliAddrLen);

		if (nsd < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else
			{
				perror("Server:Accept");
				exit(1);
			}
		}

		//create child proccess to handle client
		if ((pid = fork()) < 0)		//fork failed
		{
			perror("fork");
			exit(2);
		}
		else if (pid > 0) //parent process listen for next client
		{
			close(nsd);
		}
		else		//child serve client
		{
			close (sd);
			serveClient(nsd);
			exit(0);
		}
	}
}


void disCurDir(int sd);
void chCurDir(int sd);
void disDirCont(int sd);
void filetoclient(int sd);
void filefromclient(int sd);

void serveClient(int sd)
{
	char opcode, ackcode;

	for (;;)
	{
		if (read(sd, &opcode, 1) <= 0)
		{
			return;		//connection broken
		}

		switch (opcode)
		{
			case 'L'	:	disCurDir(sd);
						break;
			case 'C'	:	chCurDir(sd);
						break;
			case 'F'	:	disDirCont(sd);
						break;
			case 'D'	:	filetoclient(sd);
						break;
			case 'U'	:	filefromclient(sd);
						break;
		}
	}
}

//once client send a request for current server directory
void disCurDir(int sd)
{
	char dir[MAX_DIR_LENGTH];
	uint16_t dirlen, loop;
	int numread;

	//respond with one byte opcode
	if (write(sd, "L", 1) != 1) return;
	if (getcwd(dir, sizeof(dir)))	//check if directory is accessable
	{
		if (write(sd, "0", 1) != 1) return;	//if so send directory
		dirlen = htons(strlen(dir));
		if (write(sd, &dirlen, sizeof(dirlen)) != sizeof(dirlen)) return;

		dirlen = ntohs(dirlen);
		for (loop = 0; loop < dirlen; loop += numread)
		{
			if ((numread = write(sd, dir+loop, dirlen-loop)) <= 0)
			{
				return;
			}
		}
	}
	else	//send error message
	{
		dirlen = 0;
		if (errno == EACCES)
		{
			if (write(sd, "1", 1) != 1) return;
		}
		else
		{
			if (write(sd, "2", 1) != 1) return;
		}
		if (write(sd, &dirlen, sizeof(dirlen)) != sizeof(dirlen)) return;
	}
	return;
}

//once client sends a request to change server directory
void chCurDir(int sd)
{
	uint16_t dirlen, loop;
	int numread;
	char dir[MAX_DIR_LENGTH];

	//receive directory length and name from client
	if (read(sd, &dirlen, 2) != 2) return;
	dirlen = ntohs(dirlen);
	for (loop = 0; loop < dirlen; loop += numread)
	{
		if ((numread = read(sd, dir, dirlen)) <= 0)
		{
			return;
		}
	}
	dir[dirlen] = '\0';	//terminate end of directory string
	//check if directory is accessable
	if (chdir(dir) == 0)
	{
		write(sd, "C0", 2);	//if so send success message
	}
	else	//send error message to client
	{
		if (errno == EACCES) write(sd, "C2", 2);
		else if (errno == ENOENT) write(sd, "C1", 2);
		else write(sd, "C3", 2);
	}
	return;
}

//once client sends a request for server directory contence
void disDirCont(int sd)
{
	char dir[MAX_DIR_LENGTH];
	char allfilenames[MAX_FILE_LENGTH];
	DIR *dp;
	struct dirent *direntp;
	uint32_t len, loop;
	int numwrite;

	bzero(allfilenames, sizeof(allfilenames)); //blank allfilenames string
	//get current directory amd attempt to read directory contence
	if (getcwd(dir, sizeof(dir)))
	{
		if ((dp = opendir(dir)) == NULL)
		{
			return;
		}
		//format directory contence for transmission
		while (direntp = readdir(dp))
		{
			sprintf(allfilenames, "%s%s\n", allfilenames, direntp->d_name);
		}
		//send directory contence message to client
		if (write(sd, "F0", 2) != 2) return;
		len = htonl(strlen(allfilenames));
		if (write(sd, &len, sizeof(len)) != sizeof(len)) return;
		len = ntohl(len);
		for (loop = 0; loop < len; loop += numwrite)
		{
			if((numwrite = write(sd, allfilenames+loop, len-loop)) <= 0)
			{
				return;
			}
		}
	}
	else	//else send error message
	{
		if (errno == EACCES) write(sd, "1", 1);
		else write(sd, "2", 1);

		len = 0;
		write(sd, &len, sizeof(len));
	}

}

//once client sends a download file request
void filetoclient(int sd)
{
	char filename[MAX_FILENAME_LENGTH];
	char dir[MAX_DIR_LENGTH];
	uint16_t filenamelen;
	uint32_t filelen = 0, loop;
	int numwrite, numread, fd;
	char tmp[MAX_BUF_SIZE];

	//recieve filename message from client
	if (read(sd, &filenamelen, 2) != 2) return;
	filenamelen = ntohs(filenamelen);
	bzero(filename, sizeof(filename));
	for (loop = 0; loop < filenamelen; loop += numread)
	{
		if ((numread = read(sd, filename+loop, filenamelen-loop)) <= 0)
		{
			return;
		}
	}
	//attempt to open client requested file
	if ((fd = open(filename, O_RDONLY)) > 0)
	{
		filelen = lseek(fd, 0, SEEK_END);	//check length of file
		if (filelen) //if file length not 0 send file message to client
		{
			filelen = htonl(filelen);
			if (write(sd, "D0", 2) != 2) return;
			if (write(sd, &filelen, 4) != 4) return;
			lseek(fd, 0, SEEK_SET);
			filelen = ntohl(filelen);
			for (loop = 0; loop < filelen; loop++)
			{
				numread = read(fd, tmp, MAX_BUF_SIZE);
				numwrite = 0;
				while (numwrite < numread)
				{
					numwrite += write(sd, tmp + numwrite, numread - numwrite);
				}
			}
		}
		else	//else send error message to client
		{
			if (write (sd, "D1", 2) != 2) return;
		}
	}
	else
	{
		if (errno == EACCES)
		{
			if (write(sd, "D2", 2) != 2) return;
		}
		else
		{
			if (write(sd, "D3", 2) != 2) return;
		}
		if (write(sd, &filelen, 4) != 4) return;
	}
	close (fd);
}

//once client sends a request to upload a file to server
void filefromclient(int sd)
{
	int fd, numread, numwrite;
	char opcode, filename[MAX_FILENAME_LENGTH];
	char tmp[MAX_BUF_SIZE];
	uint16_t filenamelen;
	uint32_t filelen, loop;

	//receive filename message from client
	bzero(filename, sizeof(filename));
	if (read (sd, &filenamelen, 2) != 2) return;
	filenamelen = ntohs(filenamelen);
	for (loop = 0; loop < filenamelen; loop += numread)
	{
		if((numread = read(sd, filename+loop, filenamelen-loop)) <= 0)
		{
			return;
		}
	}
	//attempt to create client file checking that ir soesnt already exist
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0644)) > 0)
	{
		if(write(sd, "U0", 2) != 2) return; //if good send request to receive file
	}
	else					//else send error message to client
	{
		if (errno == EEXIST)
		{
			if(write(sd, "U1", 2) != 2) return;
		}
		else if (errno == EACCES)
		{
			if(write(sd, "U2", 2) != 2) return;
		}
		else
		{
			if(write(sd, "U3", 2) != 2) return;
		}
	}

	//once client sends confirmation that file is on it way recieve file
	if (read(sd, &opcode, 1) != 1) return;
	if (read(sd, &filelen, 4) != 4) return;

	filelen = ntohl(filelen);
	for (loop = 0; loop < filelen; loop++)
	{
		numread = read(sd, tmp, MAX_BUF_SIZE);
		numwrite = 0;
		while (numwrite < numread)
		{
			numwrite += write(fd, tmp + numwrite, numread - numwrite);
		}
	}

	close(fd);

}
