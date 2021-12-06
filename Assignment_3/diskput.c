#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>


int getFATentry(int n, char* p){
	int low, high, entry;
	p = p + 512;

	if(n%2 == 0){
		low = p[1+(3*n)/2] & 0x0F;
		high = p[(3*n)/2] & 0xFF;
		entry = (low<<8) + high;
	}
	else{
		low = p[(3*n)/2] & 0xF0;
		high = p[1+(3*n)/2] & 0xFF;
		entry = (low>>4) + (high<<4);
	}

	return entry;
}


int main(int argc, char* argv[]) {
	
	if(argc !=3){
		printf("invalid input for diskput");
		exit(0);
	}

	int fd1, fd2;
	struct stat sb1;
	struct stat sb2;
	char input[256];

	strncpy(input, argv[2], 256);
	
	fd1 = open(argv[1], O_RDWR);
	fstat(fd1, &sb1);
	char * p1 = mmap(NULL, sb1.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
	if (p1 == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}

	char* tokenisedInput;
	char** path = malloc(256*(sizeof(char*)));
	int cnt = 0;

	tokenisedInput = strtok(input, "/");
	while(NULL != tokenisedInput){
		path[cnt] = tokenisedInput;
		tokenisedInput = strtok(NULL, "/");
		cnt = cnt + 1;
	}
	//printf("%s%s%s", path[0], path[1], path[2]);
	//printf("\n%d\n", cnt);	
	
	char* filename = path[cnt-1];
	char* name = path[cnt-1];
	char* s = name;
	while(*s) {
		*s = toupper((unsigned char)*s);
		s++;
	}
	//printf("\n%s\n", name);	
	
	fd2 = open(filename, O_RDWR);
	fstat(fd2, &sb2);
	char * p2 = mmap(NULL, sb2.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
	if (p2 == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}

	int cntFreeSectors = 0;
	int l;
	for(l=2;l<2848;l++){
		if(getFATentry(l, p1) == 0x000){
			cntFreeSectors++;
		}
	}
	int freeSizeDisk = cntFreeSectors*512;

	if(sb2.st_size > freeSizeDisk){
		printf("No enough free space in the disk image");
		exit(0);
	}
}




