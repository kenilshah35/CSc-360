#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

// Node structure for linked list
struct node {
	pid_t pid;
	char *process;
	struct node *next;
};

// Static head of the linkedlist to keep track of processes
struct node *root = NULL;

// Helper method to add a process started by the client to the list
void store_process(pid_t pid, char* process){
	
	// Creating a process node to be added to the linked list
	struct node *new_process = (struct node *)malloc(sizeof(struct node));
	new_process->pid = pid;
	new_process->process = process;
	new_process->next = NULL;
	
	// If it's the first process, set as root
	if(root == NULL){
		root = new_process;
		return;
	}

	// Adding process node to the linked list
	struct node *conductor = root;
	while(conductor->next != NULL){
		conductor = conductor->next;
	}
	conductor->next = new_process;
}

// Helper method to remove a process node which has stopped from the list
void remove_process(pid_t pid){
	
	//If there are no process to remove
	if(root == NULL){
		return;
	}
	
	// Removing process from the linked list
	struct node *conductor = root;
	struct node *prev = NULL;

	if(root->pid == pid){
		root = root->next;
		free(conductor);
		return;
	}

	while(conductor != NULL && conductor->pid != pid){
		prev = conductor;
		conductor = conductor->next;
	}

	if(prev != NULL){
		return;
	}

	prev->next = conductor->next;
	free(conductor);

}


// Start running the desired process in the background
void bg_entry(char **input){

	pid_t pid;
	pid = fork();
	if(pid == 0){
		if(execvp(input[1], &input[1]) < 0){
			perror("Error on execvp");
		}
		exit(EXIT_SUCCESS);
	}
	else if(pid > 0) {
		// store information of the background child process in your data structures
		store_process(pid, input[1]);
	}
	else {
		perror("fork failed");
		exit(EXIT_FAILURE);
	}
}

// List all the processes running
void bglist_entry(){

	struct node *conductor = root;
	int job_count = 0;

	while (conductor != NULL){
		printf("%d:\t%s\n", conductor->pid, conductor->process);
		conductor = conductor->next;
		job_count++;
	}

	printf("Total background jobs:\t%d\n", job_count);
}


// Method to execute bgkill, bgstop, bgstart
void bgsig_entry(pid_t pid, char *cmd){
	
	// Check if any process with the given pid exists
	struct node *conductor = root;
	int flag = 0;
	while(conductor != NULL){
		if(conductor->pid == pid){
			flag = 1;
		}
		conductor = conductor->next;
	}

	if(flag == 0){
		printf("\nError:\tProcess %d does not exist.\n", pid);
		return;
	}

	if(strcmp(cmd, "bgkill") == 0){
		int ret_val = kill(pid, SIGTERM);
		if(ret_val == -1){
			printf("\nerror executing bgkill\n");
		}
	}
	else if(strcmp(cmd, "bgstop") == 0){
		int ret_val = kill(pid, SIGSTOP);
		if(ret_val == -1){
			printf("\nerror executing bgstop\n");
		}
	}
	else {
		int ret_val = kill(pid, SIGCONT);
		if(ret_val == -1){
			printf("\nerror executing bgstart\n");
		}
	}

}

// Helper method for the pstat method used to print out information related to a certain process
void pstat_entry(pid_t pid){
	
	// Check if any process with the given pid exists
	struct node *conductor = root;
	int flag = 0;
	while(conductor != NULL){
		if(conductor->pid == pid){
			flag = 1;
		}
		conductor = conductor->next;
	}

	if(flag == 0){
		printf("\nError:\tProcess %d does not exist.\n", pid);
		return;
	}

	// Check /proc/pid/stat file
	char stat[128];
	char *input_stat[128];
	sprintf(stat, "/proc/%d/stat", pid);
	
	// Opening file
	FILE *file_stat = fopen(stat, "r");
	if(!file_stat){
		perror("failed to open stat file\n");
		exit(EXIT_FAILURE);
	}
	
	// Reading file contents and tokenizing it
	char stat_line[1024];
	int i = 0;
	while(fgets(stat_line, 1023, file_stat) != NULL){
		char* token;
		token = strtok(stat_line, " ");
		input_stat[i] = token;
		while(token != NULL){
			input_stat[i] = token;
			token = strtok(NULL, " ");
			i++;
		}
	}
	fclose(file_stat);
	
	// Retrieve relevant requiered info
	char* comm = input_stat[1];
	char* state = input_stat[2];
	char* ptr;
	unsigned int utime = strtoul(input_stat[13], &ptr, 10)/(float)sysconf(_SC_CLK_TCK);
	unsigned int stime = strtoul(input_stat[14], &ptr, 10)/(float)sysconf(_SC_CLK_TCK);
	char* rss = input_stat[23];
	
	// Check /proc/pid/status
	char status[128];
	char input_status[128][128];
	sprintf(status, "/proc/%d/status", pid);
	
	// Opening file
	FILE *file_status = fopen(status, "r");
	if(!file_status){
		perror("failed to open stat file\n");
		exit(EXIT_FAILURE);
	}
	
	// Reading file contents
	int j = 0;
	while(fgets(input_status[j], 128, file_status) != NULL){
		j++;
	}
	fclose(file_status);
	
	// Retrive relevant required info
	char* voluntary_ctxt_switches = input_status[55];
	char* nonvoluntary_ctxt_switches = input_status[56];

	printf("comm: %s\n", comm);
	printf("state: %s\n", state);
	printf("utime: %lu\n", utime);
	printf("stime: %lu\n", stime);
	printf("rss: %s\n", rss);
	printf("volunatary context switches: %s\n", voluntary_ctxt_switches);
	printf("nonvoluntary context switches: %s\n", nonvoluntary_ctxt_switches);

}

void check_zombieProcess(void){
	int status;
	int retVal = 0;

	while(1) {
		usleep(1000);
		if(root == NULL){
			return ;
		}
		retVal = waitpid(-1, &status, WNOHANG);
		if(retVal > 0) {
			//remove the background process from your data structure

			if(WIFSIGNALED(status)){
				printf("Process %d was killed\n", retVal);
				remove_process(retVal);
			}

			if(WIFEXITED(status)){
				printf("Process %d exits\n", retVal);
				remove_process(retVal);
			}
		}
		else if(retVal == 0){
			break;
		}
		else{
			perror("waitpid failed");
			exit(EXIT_FAILURE);
		}
	}
	return ;
}


int main(){

	while(1){
		
		//Take command in from 
		char* cmd = readline("PMan: > ");

		// parse the input cmd (e.g., with strtok)
		char *input[256];
		char *token = strtok(cmd, " ");
		int i = 0;
		while (token != NULL){
			input[i] = token;
			i++;
			token = strtok(NULL, " ");
		}

		//
		if (strcmp(input[0], "bg") == 0){
			bg_entry(input);
		}
		else if(strcmp(input[0], "bglist") == 0){
			bglist_entry();
		}
		else if((strcmp(input[0], "bgkill") == 0) || (strcmp(input[0], "bgstop") == 0) || (strcmp(input[0], "bgstart") == 0)){
			pid_t pid;
			pid = atoi(input[1]);
			bgsig_entry(pid, input[0]);
		}
		else if(strcmp(input[0], "pstat") == 0){
			pid_t pid;
			pid = atoi(input[1]);
			pstat_entry(pid);
		}
		else{
			printf("\nCommand not found\n");
		}

		check_zombieProcess();

	}
	return 0;
}
