#ifndef MY_DAEMON_H
#define MY_DAEMON_H

//make process a daemon
void daemonInit(void);

//remove zombies created by calling process
void slayZombies(void);

#endif //end of MY_DAEMON_H
