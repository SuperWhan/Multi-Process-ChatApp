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
	// You will need to get user name as a parameter, argv[1].
	if (argc != 2) {
		printf("Usage:\\%s <user_name>\n", argv[0]);
		exit(-1);
	}
	if(connect_to_server("GROUP-68", argv[1], pipe_server_to_user, pipe_user_to_server) == -1) {
		perror("Fail to connect to server.");
		exit(-1);
	}

	/* -------------- YOUR CODE STARTS HERE -----------------------------------*/


	char write_buf[MAX_MSG];
	char read_buf[MAX_MSG];
	int pip_flag = fcntl (pipe_server_to_user[0], F_GETFL);
	fcntl (pipe_server_to_user[0], F_SETFL, pip_flag | O_NONBLOCK);
	fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
	close(pipe_server_to_user[1]);
	close(pipe_user_to_server[0]);
	// printf("pipe to read: %d\n", pipe_server_to_user[0]);
	// printf("pipe to write: %d\n", pipe_user_to_server[1]);
	print_prompt(argv[1]);

	while (1) {
		usleep(500);

		memset(read_buf, '\0', MAX_MSG);
		memset(write_buf, '\0', MAX_MSG);

		fgets(write_buf, MAX_MSG, stdin);
		int read_byte;
		if (strlen(write_buf) > 1) {
			print_prompt(argv[1]);
			write(pipe_user_to_server[1], write_buf, MAX_MSG);
		}

		read_byte = read(pipe_server_to_user[0], read_buf, MAX_MSG);

		if (read_byte > -1) {
			if (read_byte == 0){
				fprintf(stderr, "Disconnected\n");
				fflush(stdout);
				exit(1);
			}
			else if (strcmp(read_buf,"\\seg") == 0) {
				char *n = NULL;
				*n = 1;
			}
				printf("%s\n", read_buf);
				fflush(stdout);
				print_prompt(argv[1]);
		}
	}
	// poll pipe retrieved and print it to sdiout

	// Poll stdin (input from the terminal) and send it to server (child process) via pipe

	/* -------------- YOUR CODE ENDS HERE -----------------------------------*/
}

/*--------------------------End of main for the client --------------------------*/
