#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#include "mydefs.h"
#include "token.h"



void userCLI(int sd);
int connecttoserver(int argc, char *argv[]);

int main (int argc, char *argv[])
{
	int sd;

	//connect to server
	sd = connecttoserver(argc, argv);
	//run user command line interface
	userCLI(sd);
	exit(0);
}

//setup server descriptor
int connecttoserver(int argc, char *argv[])
{
	int sd;
	char host[HOST_NAME_LEN];
	struct sockaddr_in serAddr;
	struct hostent *hp;

	//get server host name
	if (argc == 1)	//assume server is running local host
	{
		gethostname(host, HOST_NAME_LEN);
	}
	else if (argc == 2)	//use user specified host name
	{
		strcpy(host, argv[1]);
	}
	else
	{
		printf("Usage: %s [<server_host_name>]\n", argv[0]);
		exit(1);
	}

	//get host address and build a server socket address
	bzero((char *)&serAddr, sizeof(serAddr));
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(MY_SERV_TCP_PORT);

	if ((hp = gethostbyname(host)) == NULL)
	{
		printf("host %s NOT FOUND...\n", host);
		exit(1);
	}
	serAddr.sin_addr.s_addr = * (u_long *) hp->h_addr;

	//create TCP socket and connect to server address
	sd = socket(AF_INET, SOCK_STREAM, 0);

	if (connect(sd, (struct sockaddr *)&serAddr, sizeof(serAddr)) < 0)
	{
		perror("Client:Connect");
		exit(2);
	}
	return(sd);
}

//list of modules
int getSerDir(int sd);
void getCliDir(void);
int dirSer(int sd);
void dirCli(void);
int cdSer(int sd, char *token);
void cdCli(char *token);
int download(int sd, char *filename);
int upload(int sd, char *filename);
void helptxt(void);

//user command line interface
void userCLI(int sd)
{
	char cmd[MAX_BUF_SIZE];		//command string entered by user
	char *token[MAXTOKENS];		//tokens of command string
	int tk;				//number of tokens in command string
	int cont = 1;			//boolean for continue loop

	printf("Welcome to my FTP client, type help for instructions\n");

	//until user enters quit command loop
	while (cont)
	{
		printf("$> ");		//generic user prompt
		fgets(cmd, MAX_BUF_SIZE, stdin);

		//break user command up into string tokens
		tk = tokenise(cmd, token, " \t\n");

		if (tk == 1 || tk == 2)
		{
			//display the current directory of the server
			if (strcmp(token[0], "pwd") == 0 && tk == 1)
			{
				getSerDir(sd);
			}
			else if (strcmp(token[0], "lpwd") == 0 && tk == 1)
			{
				//displat the current directory of the client
				getCliDir();
			}
			else if (strcmp(token[0], "dir") == 0 && tk == 1)
			{
				//display the file's contained in the current directory of the server
				dirSer(sd);
			}
			else if (strcmp(token[0], "ldir") == 0 && tk == 1)
			{
				//display the file's contained in the current sirectory of the client
				dirCli();
			}
			else if (strcmp(token[0], "cd") == 0 && tk == 2)
			{
				//change the current directory of server to token[1]
				cdSer(sd, token[1]);
			}
			else if (strcmp(token[0], "lcd") == 0 && tk == 2)
			{
				//change the current directory of the client to token[1]
				cdCli(token[1]);
			}
			else if (strcmp(token[0], "get") == 0 && tk == 2)
			{
				//down load file token[1] from server to client
				download(sd, token[1]);
			}
			else if (strcmp(token[0], "put") == 0 && tk == 2)
			{
				//upload file token[1] from client to server
				upload(sd, token[1]);
			}
			else if (strcmp(token[0], "quit") == 0)
			{
				//quit simple FTP Client
				cont = 0;
			}
			else if (strcmp(token[0], "help") == 0)
			{
				//display help and commands text
				helptxt();
			}
			else
			{
				printf("type \"help\" for list of commands...\n");
			}
		}
	}
	return;
}


//display current working directory of server
int getSerDir(int sd)
{
	char dir[MAX_DIR_LENGTH];
	char opcode, ackcode;
	uint16_t dirlen, loop;
	int numread;

	//send one byte opcode to server
	if (write(sd, "L", 1) != 1) return(-1);		//-1 error sending message
	//recieve reply message
	if (read(sd, &opcode, 1) != 1) return(-2);	//-2 error recieving message
	if (read(sd, &ackcode, 1) != 1) return(-2);
	if (read(sd, &dirlen, 2) != 2) return(-2);
	//check server sent correct opcode
	if (opcode == 'L')
	{
		if (ackcode == '0')	//if server sends directory read directory
		{
			dirlen = ntohs(dirlen);
			for (loop = 0; loop < dirlen; loop += numread)
			{
				if ((numread = read(sd, dir+loop, dirlen-loop)) <= 0)
				{
					return(-3);	//-3 error recieving directory
				}
			}
			dir[dirlen] = '\0';	//add NULL terminator
			printf(" %s\n", dir);	//display server sirectory to user
		}
		else if (ackcode == '1') //if error display message
		{
			printf("Access denied...\n");
		}
		else
		{
			printf("Server directory error...\n");
		}
	}
	else
	{
		printf("Server client out of sync. error...\n");
		exit(2);
	}
	return(0);
}


//get current working directory from local machine
void getCliDir(void)
{
	char dir[MAX_DIR_LENGTH];

	//get current working directory
	if(getcwd(dir, sizeof(dir)))
	{
		printf(" %s\n", dir);
	}
	else
	{
		printf("bad directory name.\n");
	}
	return;
}

//formats files for display
void displayfiles(char *files)
{
	int tk, i, line = 0, len;
	char *token[MAXTOKENS];

	tk = tokenise(files, token, "\n");

	for (i = 0; i < tk; i++)
	{
		len = strlen(token[i]);
		if ((line += (len +1)) > 80)
		{
			printf("\n");
			line = len;
		}
		printf(" %s", token[i]);
	}
	printf("\n");
}

//display files in servers current working directory
int dirSer(int sd)
{
	char allfilenames[MAX_FILE_LENGTH];
	uint32_t len, loop;
	int numread;
	char opcode, ackcode;
	//send opcode to server
	if (write(sd, "F", 1) != 1) return(-1);
	//read reply message from server
	if (read(sd, &opcode, 1) != 1) return(-2);
	if (read(sd, &ackcode, 1) != 1) return(-2);
	if (read(sd, &len, sizeof(len)) != sizeof(len)) return(-2);
	//check server send correct reply message
	if (opcode == 'F')
	{
		if (ackcode == '0') //if server is sending directory contence
		{
			len = ntohl(len); //convert to host byte order
			//get all filenames preserving frame size
			for (loop = 0; loop < len; loop += numread)
			{
				if((numread = read(sd, allfilenames+loop, len-loop)) <= 0)
				{
					return(-3);
				}
			}
			displayfiles(allfilenames);
		}
		else if (ackcode == '1') //if error display error message
		{
			printf("permission denied...\n");
		}
		else
		{
			printf("other error\n");
		}
	}
	else
	{
		printf("Server client out of sync. error...\n");
		exit(2);
	}
	return;
}


//display list of files in clients working directory
void dirCli(void)
{
	char dir[MAX_DIR_LENGTH];
	char allfilenames[MAX_FILE_LENGTH];
	DIR *dp;
	struct dirent *direntp;
	bzero(allfilenames, sizeof(allfilenames));
	if (getcwd(dir, sizeof(dir)))
	{
		if ((dp = opendir(dir)) == NULL)
		{
			printf("Error reading directory %s\n", dir);
			return;
		}
		while (direntp = readdir(dp))
		{
			sprintf(allfilenames, "%s%s\n", allfilenames, direntp->d_name);
		}
		displayfiles(allfilenames);
	}
	else
	{
		printf("bad directory name...\n");
	}
	return;
}


int cdSer(int sd, char *dir)
{
	uint16_t dirlen, loop;
	int numread;
	char opcode, ackcode;

	//send server change directory message
	if (write(sd, "C", 1) != 1) return(-1);
	dirlen = htons(strlen(dir));
	if (write(sd, &dirlen, 2) != 2) return(-1);
	//send directory name
	dirlen = ntohs(dirlen);
	for (loop = 0; loop < dirlen; loop += numread)
	{
		if ((numread = write(sd, dir+loop, dirlen-loop)) <= 0)
		{
			return(-1);
		}
	}
	//recieve responce message
	if (read(sd, &opcode, 1) != 1) return(-2);
	if (read(sd, &ackcode, 1) != 1) return(-2);
	//check if server sent correct reply message
	if (opcode == 'C')
	{
		if (ackcode == '0') //display server message
		{
			printf("Servers Active directory changed\n");
		}
		else if (ackcode == '1')
		{
			printf("Directory doesn't exist..\n");
		}
		else if (ackcode == '2')
		{
			printf("Permission denied...\n");
		}
		else
		{
			printf("Directory error\n");
		}
	}
	else
	{
		printf("Server client out of sinc. error...\n");
		exit(2);
	}
	return(0);
}


//change working directory in client
void cdCli(char *token)
{
	if (chdir(token) != 0)
	{
		printf("bad directory name\n");
	}
	return;
}


//download file from server current directory to client current directory
int download(int sd, char *filename)
{
	uint16_t filenamelen;
	uint32_t filelen, loop;
	int numwrite, numread, fd;
	char opcode, ackcode;
	char tmp[MAX_BUF_SIZE];
	//send message to server
	if (write(sd, "D", 1) != 1) return(-1);
	filenamelen = strlen(filename);
	filenamelen = htons(filenamelen);
	if (write(sd, &filenamelen, 2) != 2) return(-1);
	//send filename to server
	filenamelen = ntohs(filenamelen);
	for (loop = 0; loop < filenamelen; loop += numwrite)
	{
		if ((numwrite = write(sd, filename+loop, filenamelen-loop)) <=0)
		{
			return(-1);
		}
	}
	//recieve reply from server
	if (read (sd, &opcode, 1) != 1) return(-2);
	if (read (sd, &ackcode, 1) != 1) return(-2);
	if (read (sd, &filelen, 4) != 4) return(-2);
	//check if server sent correct return message
	if (opcode == 'D')
	{
		if (ackcode = '0') //if server sent file
		{
			filelen = ntohl(filelen);
			if ((fd = open(filename, O_WRONLY | O_CREAT, 0644)) > 0)
			{
				for (loop = 0; loop < filelen; loop += numread)
				{
					numread = read(sd, tmp, MAX_BUF_SIZE);
					numwrite = 0;
					while (numwrite < numread)
					{
						numwrite += write(fd, tmp + numwrite, numread - numwrite);
					}
				}
				printf("download complete.\n");
			}
			else
			{
				printf("file creation error\n");
				exit(3);
			}
			close(fd);
		}
		else if (ackcode = '1') //else display error message
		{
			printf("File doesn't exist...\n");
		}
		else if (ackcode = '2')
		{
			printf("Access denied\n");
		}
		else
		{
			printf("other error\n");
		}
	}
	else
	{
		printf("Server Client out of sync. error...\n");
		exit(2);
	}
	return(0);
}


//close file descriptor and return error code
int closeret(int fd, int ret)
{
	close(fd);
	return(ret);
}


//upload file from client current directory to servers current directory.
int upload(int sd, char *filename)
{
	uint16_t filenamelen;
	uint32_t filelen, loop;
	int fd, numread, numwrite;
	char opcode, ackcode;
	char tmp[MAX_BUF_SIZE];
	//check if file exists
	if ((fd = open(filename, O_RDONLY)) > 0)
	{
		filelen = lseek(fd, 0, SEEK_END); //get length of file
		if (!filelen)
		{
			printf("file %s doesn't exist...\n", filename);
			return(closeret(fd, -3));
		}
		lseek(fd, 0, SEEK_SET); //set pointer to beginning of file
	}
	else
	{
		printf("file error: %s\n", filename);
		return(closeret(fd, -3));
	}

	filenamelen = strlen(filename);
	filenamelen = htons(filenamelen);
	//send message to server
	if (write(sd, "U", 1) != 1) return(closeret(fd, -1));
	if (write(sd, &filenamelen, 2) != 2) return(closeret(fd, -1));
	filenamelen = ntohs(filenamelen);
	for (loop = 0; loop < filenamelen; loop += numwrite)
	{
		if((numwrite = write(sd, filename+loop, filenamelen-loop)) <= 0)
		{
			return(closeret(fd, -1));
		}
	}
	//recieve message from server
	if (read(sd, &opcode, 1) != 1) 	return(closeret(fd, -2));
	if (read(sd, &ackcode, 1) != 1) return(closeret(fd, -2));
	//check if server sent correct message
	if (opcode == 'U')
	{
		if (ackcode == '0') //if server ready
		{
			//send file to server
			if(write(sd, "u", 1) != 1) return(closeret(fd, -4));
			filelen = htonl(filelen);
			if(write(sd, &filelen, 4) != 4) return(closeret(fd, -4));
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
			printf("upload complete.\n");
		}
		else if (ackcode == '1') //else display error message
		{
			printf("file already exists\n");
		}
		else if (ackcode == '2')
		{
			printf("cannot create file name\n");
		}
		else
		{
			printf("cannot copy file\n");
		}
	}
	else
	{
		printf("Server Client out of sync error...\n");
		exit(2);
	}
	close(fd);
	return(0);
}


//display command list.
void helptxt(void)
{
	printf("\n List of commands...\n");
	printf("pwd - displays the current directory of the server\n");
	printf("lpwd - displays the current directory of the client\n");
	printf("dir - lists the files in the current directory of the server\n");
	printf("ldir - lists the files in the current directory of the client\n");
	printf("cd directory_pathname - to change the current directory on the server\n");
	printf("lcd directory_pathname - to change thr current directory on the client\n");
	printf("get filename - to download file from server\n");
	printf("put filename - to upload file to server\n");
	printf("help - to display this message\n");
	printf("quit - to quit the Simple FTP client session\n\n");
	return;
}
