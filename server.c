#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include "comm.h"
#include "util.h"

/* -----------Functions that implement server functionality -------------------------*/

/*
 * Returns the empty slot on success, or -1 on failure
 */
int find_empty_slot(USER * user_list) {
	// iterate through the user_list and check m_status to see if any slot is EMPTY
	// return the index of the empty slot
    int i = 0;
	for(i=0;i<MAX_USER;i++) {
    	if(user_list[i].m_status == SLOT_EMPTY) {
			return i;
		}
	}
	return -1;
}

/*
 * list the existing users on the server shell
 */
int list_users(int idx, USER * user_list)
{
	// iterate through the user list
	// if you find any slot which is not empty, print that m_user_id
	// if every slot is empty, print "<no users>""
	// If the function is called by the server (that is, idx is -1), then printf the list
	// If the function is called by the user, then send the list to the user using write() and passing m_fd_to_user
	// return 0 on success
	int i, flag = 0;
	char buf[MAX_MSG] = {}, *s = NULL;

	/* construct a list of user names */
	s = buf;
	strncpy(s, "---connecetd user list---\n", strlen("---connecetd user list---\n"));
	s += strlen("---connecetd user list---\n");
	for (i = 0; i < MAX_USER; i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		flag = 1;
		strncpy(s, user_list[i].m_user_id, strlen(user_list[i].m_user_id));
		s = s + strlen(user_list[i].m_user_id);
		strncpy(s, "\n", 1);
		s++;
	}
	if (flag == 0) {
		strcpy(buf, "<no users>\n");
	} else {
		s--;
		strncpy(s, "\0", 1);
	}

	if(idx < 0) {
		printf("%s", buf);
		printf("\n");
	} else {
		/* write to the given pipe fd */
		if (write(user_list[idx].m_fd_to_user, buf, strlen(buf) + 1) < 0)
			perror("writing to server shell");
	}

	return 0;
}

/*
 * add a new user
 */
int add_user(int idx, USER * user_list, int pid, char * user_id, int pipe_to_child, int pipe_to_parent)
// add_user(empty_slot, user_list, getpid(), user_id, pipe_user_to_child[1], pipe_child_to_user[0]);
{
	// populate the user_list structure with the arguments passed to this function
	// return the index of user added
  USER temp;
  temp.m_pid = pid;
  temp.m_fd_to_server = pipe_to_child; // user write child read
  temp.m_fd_to_user = pipe_to_parent; // child write user read
  temp.m_status = SLOT_FULL;
  strcpy(temp.m_user_id, user_id);
  user_list[idx] = temp;
	return idx;
}

/*
 * Kill a user
 */
void kill_user(int idx, USER * user_list) {
	// kill a user (specified by idx) by using the systemcall kill()
	// then call waitpid on the user
  int status;
  int kill_signal = kill(user_list[idx].m_pid, SIGKILL);
  if (kill_signal == -1) {
    perror("Fail to kill a user.");
  } else {
    waitpid(user_list[idx].m_pid, &status, 0);
  }
}

/*
 * Perform cleanup actions after the used has been killed
 */
void cleanup_user(int idx, USER * user_list)
{
	// m_pid should be set back to -1
	// m_user_id should be set to zero, using memset()
	// close all the fd
	// set the value of all fd back to -1
	// set the status back to empty

  user_list[idx].m_pid = -1;
  memset(user_list[idx].m_user_id, '\0', MAX_USER_ID);
  close(user_list[idx].m_fd_to_user);
  user_list[idx].m_fd_to_user = -1;
  close(user_list[idx].m_fd_to_server);
  user_list[idx].m_fd_to_server = -1;
  user_list[idx].m_status = SLOT_EMPTY;
}

/*
 * Kills the user and performs cleanup
 */
void kick_user(int idx, USER * user_list, char *name) {
	// should kill_user()
	// then perform cleanup_user()
  close(user_list[idx].m_fd_to_user);
  close(user_list[idx].m_fd_to_server);

  if (idx != -1) {
    kill_user(idx, user_list);
    printf("%s has been left!\n", name);
    cleanup_user(idx, user_list);
  }
}

/*
 * broadcast message to all users
 */
int broadcast_msg(USER * user_list, char *buf, char *sender)
{
	//iterate over the user_list and if a slot is full, and the user is not the sender itself,
	//then send the message to that user
	//return zero on success
  for (int i = 0; i < MAX_USER; i++) {
    if (user_list[i].m_status == SLOT_FULL
        && strcmp (user_list[i].m_user_id, sender)!=0) {
      if ((write(user_list[i].m_fd_to_user, buf, strlen(buf))) == -1) {
        perror("Fail to send message to user.");
      }
    }
  }
	return 0;
}

/*
 * Cleanup user chat boxes
 */
void cleanup_users(USER * user_list)
{
	// go over the user list and check for any empty slots
	// call cleanup user for each of those users.
  for (int i = 0; i < MAX_USER; i++) {
    if (user_list[i].m_status == SLOT_EMPTY){
      cleanup_user(i, user_list);
    }
  }
}

/*
 * find user index for given user name
 */
int find_user_index(USER * user_list, char * user_id)
{
	// go over the  user list to return the index of the user which matches the argument user_id
	// return -1 if not found
	int i, user_idx = -1;
	if (user_id == NULL) {
		fprintf(stderr, "NULL name passed.\n");
		return user_idx;
	}
	for (i=0;i<MAX_USER;i++) {
		if (user_list[i].m_status == SLOT_EMPTY)
			continue;
		if (strcmp(user_list[i].m_user_id, user_id) == 0) {
			return i;
		}
	}

	return -1;
}

/*
 * given a command's input buffer, extract name
 */
int extract_name(char * buf, char * user_name)
{
	char inbuf[MAX_MSG];
    char * tokens[16];
    strcpy(inbuf, buf);

    int token_cnt = parse_line(inbuf, tokens, " ");

    if(token_cnt >= 2) {
        strcpy(user_name, tokens[1]);
        return 0;
    }

    return -1;
}

int extract_text(char *buf, char * text)
{
    char inbuf[MAX_MSG];
    char * tokens[16];
    strcpy(inbuf, buf);

    int token_cnt = parse_line(buf, tokens, " ");

    if(token_cnt >= 3) {
        strcpy(text, tokens[2]);
        return 0;
    }

    return -1;
}

/*
 * send personal message
 */
void send_p2p_msg(int idx, USER * user_list, char *buf)
{

	// get the target user by name using extract_name() function
	// find the user id using find_user_index()
	// if user not found, write back to the original user "User not found", using the write()function on pipes.
	// if the user is found then write the message that the user wants to send to that user.
  if ((write(user_list[idx].m_fd_to_user, buf, MAX_MSG)) == -1) {
    perror("Fail to write message to specific user.");
  };
}

//takes in the filename of the file being executed, and prints an error message stating the commands and their usage
void show_error_message(char *filename)
{
}


/*
 * Populates the user list initially
 */
void init_user_list(USER * user_list) {

	//iterate over the MAX_USER
	//memset() all m_user_id to zero
	//set all fd to -1
	//set the status to be EMPTY
	int i=0;
	for(i=0;i<MAX_USER;i++) {
		user_list[i].m_pid = -1;
		memset(user_list[i].m_user_id, '\0', MAX_USER_ID);
		user_list[i].m_fd_to_user = -1;
		user_list[i].m_fd_to_server = -1;
		user_list[i].m_status = SLOT_EMPTY;
	}
}

void server_exit(USER * user_list){
  for (int i=0; i < MAX_USER; i++) {
    if (user_list[i].m_status == SLOT_FULL) {
      kick_user(i, user_list, user_list[i].m_user_id);
    }
  }
}

/* ---------------------End of the functions that implementServer functionality -----------------*/


/* ---------------------Start of the Main function ----------------------------------------------*/
int main(int argc, char * argv[])
{
	int nbytes;
	setup_connection("GROUP-68"); // Specifies the connection point as argument.

	USER user_list[MAX_USER];
	init_user_list(user_list);   // Initialize user list

	fcntl(0, F_SETFL, fcntl(0, F_GETFL)| O_NONBLOCK);
	print_prompt("admin");

  char buf[MAX_MSG];
  char child_buf[MAX_MSG];
  char user_id[MAX_USER_ID];
  char user_child_buf[MAX_MSG];
  char server_child_buf[MAX_MSG];
  // all the buffers in different layer
  int user;

  int pipe_child_to_user[2];
  int pipe_user_to_child[2];
  int pipe_child_to_server[2];
  int pipe_server_to_child[2];
  // pipe_write_to_read/ all the pipes in different layers.

	while(1) {
		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/

		// Handling a new connection using get_connection


    /* -----------------------Warm Up--------------------------------------------*/
    //
    int empty_slot = find_empty_slot(user_list);

    if(empty_slot == -1) {
      fprintf(stderr, "There's no more rooms for new users!");
    }

    user = get_connection(user_id, pipe_child_to_user, pipe_user_to_child);

    if (user > -1) {
      pid_t pid;
      if (find_user_index(user_list, user_id)!=-1) {
          write(pipe_child_to_user[1], "ERROR: current user has already joined.", MAX_MSG); // 35 is the byte for the error message
          close(pipe_user_to_child[0]);
          close(pipe_child_to_user[1]);
          // activly broke the pipe to teminate the exit user rejoin.

      }

      else if (empty_slot > -1 && find_user_index(user_list, user_id) == -1) {
        close(pipe_user_to_child[1]);
        close(pipe_child_to_user[0]);

        /******************
        Changed!!!
        */
        add_user(empty_slot, user_list, getpid(), user_id, pipe_user_to_child[1], pipe_child_to_user[0]);
        printf("%s joined the chat, slot: %d\n",user_id,empty_slot);
        if((pipe(pipe_server_to_child)) == -1 || (pipe(pipe_child_to_server)) == -1){
          perror("Failed to create pipe");
        }
        /*
        Changed!!!
        *******************/

        int uflag = fcntl(user_list[empty_slot].m_fd_to_server, F_GETFL, 0);
        int sflag = fcntl(pipe_server_to_child[0], F_GETFL, 0);
        int cflag = fcntl(pipe_user_to_child[0], F_GETFL, 0);
        // get all the fd flags.
        fcntl (user_list[empty_slot].m_fd_to_server, F_SETFL, uflag | O_NONBLOCK);
        fcntl (pipe_server_to_child[0], F_SETFL, sflag | O_NONBLOCK);
        fcntl (pipe_user_to_child[0], F_SETFL, cflag | O_NONBLOCK);
        // set non_block pipes.

        print_prompt("admin");
        pid = fork();
        if (pid == -1) {
          perror("Fork Error");
          exit(0);
        }

        else if (pid == 0) { // handling all the child processes and it's pipe with message inside.

          close(pipe_child_to_server[0]);
          close(pipe_server_to_child[1]);

          while(1) {
            usleep(500);
            memset(user_child_buf, '\0', MAX_MSG);
            memset(server_child_buf,'\0', MAX_MSG);

            int read_flag1,read_flag2;
            read_flag1 = read(pipe_user_to_child[0], user_child_buf, MAX_MSG); // child read from user
            read_flag2 = read(pipe_server_to_child[0], server_child_buf, MAX_MSG); // child read from server

            if (read_flag2 == 0){
              printf("Pipe Broken between %s's client and its server, %s quit\n", user_id, user_id);
              print_prompt("admin");
              close(pipe_child_to_server[1]);
              close(pipe_server_to_child[0]);
              close(pipe_user_to_child[0]);
              close(pipe_child_to_user[1]);
              close(pipe_child_to_user[0]);
              close(pipe_user_to_child[1]);
              exit(1);
            }

            if (read_flag1 == 0) {
              strcpy(user_child_buf, "\\exit");
              write(pipe_child_to_server[1], "\\exitt", 10);

              close(pipe_child_to_server[1]);
              close(pipe_server_to_child[0]);
              close(pipe_child_to_user[1]);
              close(pipe_child_to_user[0]);
              close(pipe_user_to_child[1]);
              close(pipe_user_to_child[0]);
              exit(1);
            }

            if (read_flag2 > 1){
              // write message to user pipe, it will be read in client.c
              if ((write(pipe_child_to_user[1], server_child_buf, MAX_MSG)) == -1) {
                perror("Fail to write message to user pipe.");
              }
            }

            if (read_flag1 > 1) {
              // write the user message to child pipe
              if ((write(pipe_child_to_server[1], user_child_buf, MAX_MSG)) == -1) {
                perror("Fail to write message to child pipe.");
              }
            }
          }
        } //  child process
        else {
            user_list[empty_slot].m_pid = pid; // add the user with it's process id.
            close(pipe_user_to_child[0]);
            close(pipe_child_to_user[1]);
        } // parent process
      }
    }

    memset(buf, '\0', MAX_MSG);
    fgets(buf, MAX_MSG, stdin); // get command from stdin from server
    buf[strlen(buf)-1] = '\0'; // make the buff as string, so thatcan be recognized.
    int command_type, command_type1;
    int p2p_idx, n_idx;
    char *tokens[MAX_USER];
    char *tokens1[MAX_USER];
    char temp[MAX_MSG],temp1[MAX_MSG],bmsg[MAX_MSG+MAX_USER_ID+10]; // +10 is because some extra worning messages
    strcpy(temp, buf);

    if (strlen(temp) > 0) {
      parse_line(temp, tokens, " ");
      command_type = get_command_type(temp);

      switch(command_type) {
        case LIST:
          list_users(-1, user_list);
          print_prompt("admin");
          break;

        case KICK:
          if (tokens[1] == NULL) {
            printf("Usage:\\kick <User ID>\n");
          }
          else {
            n_idx = find_user_index(user_list, tokens[1]);
            if (n_idx > -1) {
              kick_user(n_idx, user_list, tokens[1]);
            } else {
              printf("%s: is not in the chat-room\n", tokens[1]);
            }
          }
          print_prompt("admin");
          break;
/******************************** EXTRA CREDIT *********************************/

        case SEG:
          if (tokens[1] == NULL) {
            printf("Usage:\\seg <User ID 1> <User ID 2> <User I...\n");
          }
          else {
            for (int i = 1; tokens[i] != NULL; i++) {
              int j = find_user_index(user_list, tokens[i]);
              if (j != -1 ){
                write(user_list[j].m_fd_to_user, "\\seg", 6);
                cleanup_user(j, user_list);
              }
              else {
                printf("%s is not in the user list\n", tokens[i]);
              }
            }
          }
          print_prompt("admin");
        break;

/***************************** END OF EXTRA CREDIT *****************************/

        case EXIT:
          server_exit(user_list);
          cleanup_users(user_list);
          close(pipe_child_to_user[1]);
          close(pipe_user_to_child[0]);
          exit(1);
          break;

        default: // default to broadcast the messages to all users
          memset(bmsg, '\0', MAX_MSG + 7);
          sprintf(bmsg, "Admin: %s", buf);
          broadcast_msg(user_list, bmsg, "");
          print_prompt("admin");
      }
    }

    for (int i=0; i < MAX_USER; i++) { // check all the users and it's pipe

      if (user_list[i].m_status == SLOT_FULL) {
        memset (child_buf, '\0', MAX_MSG);
        read(user_list[i].m_fd_to_server, child_buf, MAX_MSG);
        child_buf[strlen(child_buf)-1] = '\0';
        strcpy(temp1, child_buf);
        parse_line(child_buf,tokens1," ");

        if (strlen(temp1) > 0) { // if somthing read into temp1 (this is a buffer)
          command_type1 = get_command_type(temp1);
          switch(command_type1) {
            case LIST:
              list_users(i, user_list);
              break;

            case  P2P:
              if (tokens1[1] == NULL) {
                memset(bmsg, '\0', MAX_MSG);
                strcpy(bmsg, "\nUsage:\\p2p <User ID> <Message>");
                if ((write(user_list[i].m_fd_to_user, bmsg, MAX_MSG)) == -1) {
                  perror("Fail to write to user.");
                }
              }
              else {
                p2p_idx = find_user_index(user_list, tokens1[1]);
                if(p2p_idx == -1) {
                  memset(bmsg, '\0', MAX_MSG);
                  sprintf (bmsg, "ERROR: user %s not found.", tokens1[1]);
                  if ((write(user_list[i].m_fd_to_user, bmsg, MAX_MSG)) == -1) {
                    perror("Fail to write to user.");
                  }
                } else if (p2p_idx == i) {
                  memset(bmsg, '\0', MAX_MSG);
                  strcpy(bmsg, "ERROR: I don't talk to myself.");
                  if ((write(user_list[i].m_fd_to_user, bmsg, MAX_MSG)) == -1) {
                    perror("Fail to write to user.");
                  }
                } else {
                  // Send p2p message successfully
                  memset(bmsg, '\0', MAX_MSG + MAX_USER_ID);

                  strcpy(bmsg, user_list[i].m_user_id);
                  strcat(bmsg, ":");
                  for (int i = 2 ; tokens1[i] != '\0'; i++){
                  strcat(bmsg, " ");
                  strcat(bmsg, tokens1[i]);
                  }
                  send_p2p_msg(p2p_idx, user_list, bmsg);
                }
              }
              break;

/******************************* EXTRA CREDIT ************************************/
            case SEG:
              if ((write(user_list[i].m_fd_to_user, "\\seg", 6)) == -1) {
                perror("Fail to write to user.");
              };
              cleanup_user(i, user_list);
              break;
/****************************** END OF SEG **************************************/

            case EXIT:
              // printf("%s has quit\n", user_list[i].m_user_id);
              kick_user(i, user_list, user_list[i].m_user_id);
              print_prompt("admin");
            break;

            default:
              memset(bmsg, '\0', MAX_MSG + 7);
              sprintf(bmsg, "%s: %s", user_list[i].m_user_id, child_buf);
              printf("%s\n", bmsg);
              broadcast_msg(user_list, bmsg, user_list[i].m_user_id);
              print_prompt("admin");
          }
        }
      }
    }

		// Check max user and same user id

		// Child process: poli users and SERVER
		// Server process: Add a new user information into an empty slot

		// poll child processes and handle user commands
		// Poll stdin (input from the terminal) and handle admnistrative command

		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/
	}


}

/* --------------------End of the main function ----------------------------------------*/
