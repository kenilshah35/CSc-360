#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct customer_info{
	int user_id;
	int class_type;
	int service_time;
	int arrival_time;
}customer;

typedef struct clerk_info{
	int id;
	int avail; // avail = 0 if available, avail = 1 if busy
}clerk;

#define NQUEUES 2
#define NCLERKS 5

/*Global Variables*/

//record the simulation start time
static struct timeval init_time;

//Waiting time for economy - 0 and business - 1
double class_waiting_time[NQUEUES];

//list of all customers
customer* customer_list;
// Main FIFO queue for customer
customer* queue[NQUEUES];
// Keep Track of queue length
int queue_length[NQUEUES];
// variable to record the status of a queue, the value could be idle=-1 (not using by any clerk) or the clerk id (1 ~ 5), indicating that the corresponding clerk is now signaling this queue.
int queue_status[NQUEUES];
//list of all clerks
clerk clerk_list[NCLERKS];
// variable to keep track of how many customers left to track
int customers_remaining;

// Declare requiered mutexes
pthread_mutex_t queue_mutex;
pthread_mutex_t clerk1_mutex;
pthread_mutex_t clerk2_mutex;
pthread_mutex_t clerk3_mutex;
pthread_mutex_t clerk4_mutex;
pthread_mutex_t clerk5_mutex;

// Declare requiered convars
pthread_cond_t economy_con;
pthread_cond_t business_con;
pthread_cond_t clerk1_con;
pthread_cond_t clerk2_con;
pthread_cond_t clerk3_con;
pthread_cond_t clerk4_con;
pthread_cond_t clerk5_con;

void enqueue(customer* cus, int qid){
	queue[qid][queue_length[qid]] = *cus;
	queue_length[qid] = queue_length[qid] + 1;
}

void dequeue(int qid){
	if(queue_length[qid] == 0){
		printf("\nCant dequeue\n");
		exit(1);
	}
	for(int i=0;i<queue_length[qid]-1;i++){
		queue[qid][i] = queue[qid][i+1];
	}
	queue_length[qid] = queue_length[qid] - 1;
}

double getCurrentSimulationTime(){
	struct timeval cur_time;
	double cur_secs, init_secs;

	init_secs = (init_time.tv_sec + (double) init_time.tv_usec / 1000000);
	gettimeofday(&cur_time, NULL);
	cur_secs = (cur_time.tv_sec + (double) cur_time.tv_usec / 1000000);

	return cur_secs - init_secs;
}

// Thread function used for customer thread
void* customer_entry(void* cus_info){
	
	// store the given param in a temp customer
	customer* p_myInfo = (customer*) cus_info;
	int ctype = p_myInfo->class_type;
	
	// Simulate arrival time
	usleep(p_myInfo->arrival_time * 100000);

	printf("A customer arrives: customer ID %2d.\n", p_myInfo->user_id);
	
	// Lock mutex while enqueing the customer into relevant queue - business or economy based on class type or ctype
	pthread_mutex_lock(&queue_mutex);

	enqueue(p_myInfo, ctype);
	printf("A customer enters a queue: the queue ID %1d, and length of the queue %2d.\n", ctype, queue_length[ctype]);
	double queue_enter_time = getCurrentSimulationTime();
	while(queue_status[ctype] == -1){
		if(ctype == 0){
			pthread_cond_wait(&economy_con, &queue_mutex);
		}
		else if(ctype == 1){
			pthread_cond_wait(&business_con, &queue_mutex);
		}
		else{
			printf("\nError while cond wait in customer thread\n");
		}
	}
	double queue_exit_time = getCurrentSimulationTime();
	//printf("\n%.2f\n", (queue_exit_time - queue_enter_time));
	
	//Calculate waiting time for this customer to be served by a clerk
	class_waiting_time[ctype] = class_waiting_time[ctype] + (queue_exit_time - queue_enter_time);
	int clerk_id = queue_status[ctype];
	printf("A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d.\n", getCurrentSimulationTime(), p_myInfo->user_id, clerk_id);
	
	pthread_mutex_unlock(&queue_mutex);
	
	//Simulate the time taken to serve the customer
	usleep(p_myInfo->service_time * 100000);
	printf("A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d.\n",getCurrentSimulationTime(), p_myInfo->user_id, clerk_id);
	pthread_mutex_lock(&queue_mutex);
	customers_remaining = customers_remaining - 1;
	pthread_mutex_unlock(&queue_mutex);
	
	//Signal the relevant clerk that the customer has been served 
	if(clerk_id == 1){
		pthread_mutex_lock(&clerk1_mutex);
		clerk_list[0].avail = 0;
		pthread_cond_signal(&clerk1_con);
		pthread_mutex_unlock(&clerk1_mutex);
	}
	else if(clerk_id == 2){
		pthread_mutex_lock(&clerk2_mutex);
                clerk_list[1].avail = 0;
                pthread_cond_signal(&clerk2_con);
                pthread_mutex_unlock(&clerk2_mutex);	
	}
	else if(clerk_id == 3){
		pthread_mutex_lock(&clerk3_mutex);
                clerk_list[2].avail = 0;
                pthread_cond_signal(&clerk3_con);
                pthread_mutex_unlock(&clerk3_mutex);
	}else if(clerk_id == 4){
		pthread_mutex_lock(&clerk4_mutex);
                clerk_list[3].avail = 0;
                pthread_cond_signal(&clerk4_con);
                pthread_mutex_unlock(&clerk4_mutex);
	}else if(clerk_id == 5){
		pthread_mutex_lock(&clerk5_mutex);
                clerk_list[4].avail = 0;
                pthread_cond_signal(&clerk5_con);
                pthread_mutex_unlock(&clerk5_mutex);
	}else{
		printf("\nInvalid clerk id in customer thread\n");
	}

	pthread_exit(NULL);

	return NULL;
}

// Thread function used for clerk threads
void* clerk_entry(void* clerk_info){
	
	// Store the given param into temp clerk
	clerk* q_myInfo = (clerk*) clerk_info;
	while(1){
		pthread_mutex_lock(&queue_mutex);
		
		// Business class gets priority for sevice
		int clerk_id = q_myInfo->id;
		int prio_index = 1;
		if(queue_length[prio_index] <= 0){
			prio_index = 0;
		}
		// Check if there is any customer in queue
		if(queue_length[prio_index] > 0){
			dequeue(prio_index);
			
			queue_status[prio_index] = clerk_id;
			clerk_list[clerk_id-1].avail = 1;
			
			if(prio_index == 0){
				pthread_cond_broadcast(&economy_con);
			}
			else{
				pthread_cond_broadcast(&business_con);
			}
			pthread_mutex_unlock(&queue_mutex);
		}
		else{
			pthread_mutex_unlock(&queue_mutex);
			usleep(10);
		}
		
		if(clerk_list[clerk_id-1].avail == 1){
			if(clerk_id == 1){
				pthread_mutex_lock(&clerk1_mutex);
				pthread_cond_wait(&clerk1_con, &clerk1_mutex);
				pthread_mutex_unlock(&clerk1_mutex);
			}
			else if(clerk_id == 2){
				pthread_mutex_lock(&clerk2_mutex);
				pthread_cond_wait(&clerk2_con, &clerk2_mutex);
				pthread_mutex_unlock(&clerk2_mutex);
			}
			else if(clerk_id == 3){
				pthread_mutex_lock(&clerk3_mutex);
				pthread_cond_wait(&clerk3_con, &clerk3_mutex);
				pthread_mutex_unlock(&clerk3_mutex);
			}
			else if(clerk_id == 4){
				pthread_mutex_lock(&clerk4_mutex);
				pthread_cond_wait(&clerk4_con, &clerk4_mutex);
				pthread_mutex_unlock(&clerk4_mutex);
			}
			else if(clerk_id == 5){
				pthread_mutex_lock(&clerk5_mutex);
				pthread_cond_wait(&clerk5_con, &clerk5_mutex);
				pthread_mutex_unlock(&clerk5_mutex);
			}
			else{
				printf("\nInvalid clerk id in clerk thread\n");
			}
		}
		
		if(customers_remaining <= 0){
			pthread_exit(NULL);
		}
		

	}
	//printf("\n\nRemaining customers %2d\n\n", customers_remaining);

	pthread_exit(NULL);
	return NULL;
}

int main(int argc, char* argv[]){
	
	// Read customer information from txt file and store them in the customer list 
	FILE* file = fopen(argv[1], "r");
	int total_cus = 0;
	int total_business_cus = 0;
	int total_economy_cus = 0;
	char input[256];
	
	fgets(input, sizeof(input), file);
	total_cus = atoi(input);
	
	customer_list = (customer*) malloc(total_cus * sizeof(customer));
	// Economy queue
	queue[0] = (customer*) malloc(total_cus * sizeof(customer));
	// Business queue
	queue[1] = (customer*) malloc(total_cus * sizeof(customer));
	
	for(int i=0;i<5;i++){
		clerk_list[i].id = i+1;
		clerk_list[i].avail = 0;
	}
	queue_status[0] = -1;
	queue_status[1] = -1;
	class_waiting_time[0] = 0.0;
	class_waiting_time[1] = 0.0;

	customer temp_cus;
	for(int i=0;i<total_cus;i++){
		
		fgets(input, sizeof(input), file);
		temp_cus.user_id = atoi(strtok(input, ":"));
		temp_cus.class_type = atoi(strtok(NULL, ","));
		temp_cus.arrival_time = atoi(strtok(NULL, ","));
		temp_cus.service_time = atoi(strtok(NULL, ","));

		customer_list[i] = temp_cus;
		if(temp_cus.class_type == 1){
			total_business_cus = total_business_cus + 1;
		}
		else{
			total_economy_cus = total_economy_cus + 1;
		}
	}

	fclose(file);
	customers_remaining = total_cus;
	gettimeofday(&init_time, NULL);
	
	// Initialise all mutexes and convars
	if(pthread_mutex_init(&queue_mutex, NULL) != 0){
		printf("\nQueue mutex init failed\n");
		return 1;
	}
	if(pthread_mutex_init(&clerk1_mutex, NULL) != 0){
		printf("\nClerk1 mutex init failed\n");
		return 1;
	}
	if(pthread_mutex_init(&clerk2_mutex, NULL) != 0){
		printf("\nClerk2 mutex init failed\n");
		return 1;
	}
        if(pthread_mutex_init(&clerk3_mutex, NULL) != 0){
		printf("\nClerk3 mutex init failed\n");
		return 1;
	}
        if(pthread_mutex_init(&clerk4_mutex, NULL) != 0){
		printf("\nClerk4 mutex init failed\n");
		return 1;
	}
        if(pthread_mutex_init(&clerk5_mutex, NULL) != 0){
		printf("\nClerk5 mutex init failed\n");
		return 1;
	}

	if(pthread_cond_init(&business_con, NULL) != 0){
		printf("\nBusiness con var init failed\n");
		return 1;
	}
	if(pthread_cond_init(&economy_con, NULL) != 0){
		printf("\nEconomy con var init failed\n");
		return 1;
	}
	if(pthread_cond_init(&clerk1_con, NULL) != 0){
		printf("\nClerk1 con var init failed\n");
		return 1;
	}
	if(pthread_cond_init(&clerk2_con, NULL) != 0){
		printf("\nClerk2 con var init failed\n");
		return 1;
	}
	if(pthread_cond_init(&clerk3_con, NULL) != 0){
		printf("\nClerk3 con var init failed\n");
		return 1;
	}
	if(pthread_cond_init(&clerk4_con, NULL) != 0){
		printf("\nClerk4 con var init failed\n");
	 	return 1;
	}
	if(pthread_cond_init(&clerk5_con, NULL) != 0){
		printf("\nClerk5 con var init failed\n");
		return 1;
	}
	
	//printf("Before creating");
	
	// Create threads for each clerks
	pthread_t clerk_thread[NCLERKS];
	for(int i=0;i<NCLERKS;i++){
		if(pthread_create(&clerk_thread[i], NULL, clerk_entry, (void*)&clerk_list[i]) != 0){
			printf("\nerror while creating clerk thread\n");
			return 1;
		}
	}

	// Create threads for every customer
	pthread_t cus_thread[total_cus];
	for(int i=0;i<total_cus;i++){
		if(pthread_create(&cus_thread[i], NULL, customer_entry, (void*)&customer_list[i]) != 0){
			printf("\nerror while creating customer thread\n");
			return 1;
		}
	}
	//printf("Before joining");
	
	// wait for all customer threads to terminate
	for(int i=0;i<total_cus;i++){
		//printf("\njoining cus: %d\n", i);
		if(pthread_join(cus_thread[i], NULL) != 0){
			printf("\nerror while joining cus threads\n");
			return 1;
		}
	}

	printf("The average waiting time for all customers in the system is: %.2f seconds.\n", (class_waiting_time[0] + class_waiting_time[1]/total_cus));
	printf("The average waiting time for all business-class customers is: %.2f seconds.\n", class_waiting_time[1]/total_business_cus);
        printf("The average waiting time for all economy-class customers is: %.2f seconds.\n", class_waiting_time[0]/total_economy_cus);
	
	// Program did not terminate on its own. For some reason clerk threads don't terminate. After trial and error following was implemented as a stopgap so program would not run forever
	while(1){
		if(getCurrentSimulationTime() > 170.0){
			free(customer_list);
	                free(queue[0]);
	                free(queue[1]);
	                return 0;
		}
	}

	/*
	for(int i=0;i<NCLERKS;i++){
		//printf("\njoining clerk before: %d\n", i);
		if(pthread_join(clerk_thread[i], NULL) != 0){
			printf("\nerror while joining clerk threads\n");
			return 1;
		}
		//printf("\njoining clerk after: %d\n", i);
	}*/

	//printf("Before destruction");

	pthread_mutex_destroy(&queue_mutex);
	pthread_mutex_destroy(&clerk1_mutex);
	pthread_mutex_destroy(&clerk2_mutex);
	pthread_mutex_destroy(&clerk3_mutex);
	pthread_mutex_destroy(&clerk4_mutex);
	pthread_mutex_destroy(&clerk5_mutex);
	
	pthread_cond_destroy(&economy_con);
	pthread_cond_destroy(&business_con);
	pthread_cond_destroy(&clerk1_con);
	pthread_cond_destroy(&clerk2_con);
	pthread_cond_destroy(&clerk3_con);
	pthread_cond_destroy(&clerk4_con);
	pthread_cond_destroy(&clerk5_con);
	
	//printf("The average waiting time for all customers in the system is: %.2f seconds.\n", (class_waiting_time[0] + class_waiting_time[1]/total_cus));
	//printf("The average waiting time for all business-class customers is: %.2f seconds.\n", class_waiting_time[1]/total_business_cus);
	//printf("The average waiting time for all economy-class customers is: %.2f seconds.\n", class_waiting_time[0]/total_economy_cus);

	free(customer_list);
	free(queue[0]);
	free(queue[1]);
	return 0;

}	
