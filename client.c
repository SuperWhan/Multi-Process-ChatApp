#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include "comm.h"
#include "util.h"

/* -------------------------Main function for the client ----------------------*/
int main(int argc, char * argv[]) {

	int pipe_server_to_user[2], pipe_user_to_server[2];
	// pipe_write_to_read[2]
	// You will need to get user name as a parameter, argv[1].

	if(connect_to_server("YOUR_UNIQUE_ID", argv[1], pipe_server_to_user, pipe_user_to_server) == -1) {
		exit(-1);
	}

	/* -------------- YOUR CODE STARTS HERE -----------------------------------*/


	// char write_buf[MAX_MSG];
	// char read_buf[MAX_MSG];
	char buf[MAX_MSG];
	int pip_flag = fcntl (pipe_server_to_user[0], F_GETFL);
	fcntl (pipe_server_to_user[0], F_SETFL, pip_flag | O_NONBLOCK);
	printf("pipe to read: %d\n", pipe_server_to_user[0]);
	printf("pipe tp write: %d\n", pipe_user_to_server[1]);
	// write(pipe_user_to_server[1], "Hello Server!", 13);

	while (1) {
		print_prompt(argv[1]);
		fgets(buf, MAX_MSG, stdin);
		// printf("%s", buf);
		write(pipe_user_to_server[1], buf, MAX_MSG);
	}
	// poll pipe retrieved and print it to sdiout

	// Poll stdin (input from the terminal) and send it to server (child process) via pipe

	usleep(500);
	/* -------------- YOUR CODE ENDS HERE -----------------------------------*/
}

/*--------------------------End of main for the client --------------------------*/
