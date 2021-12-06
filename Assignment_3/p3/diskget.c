#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int getFATentry(int n, char* ptr){
	int low, high, entry;
	ptr = ptr + 512;

	if(n%2 == 0){
		low = ptr[1+(3*n)/2] & 0x0F;
		high = ptr[(3*n)/2] & 0xFF;
		entry = (low<<8) + high;
	}
	else{
		low = ptr[(3*n)/2] & 0xF0;
		high = ptr[1+(3*n)/2] & 0xFF;
		entry = (low>>4) + (high<<4);
	}

	return entry;
}



int sizeOfFile(char* originalFilename, char* ptr){
	int fileSize = -1;

	while(ptr[0] != 0x00){
		if( ptr[11] != 0x0F  &&(ptr[11] & 0x02) == 0x00 && (ptr[11]&0x08) == 0x00 ) {
			char* filename = malloc(sizeof(char));
			int i;
			for(i=0;i<8;i++){
				if(ptr[i] == ' '){
					break;
				}
				filename[i] = ptr[i];
			}
			
			char* ext = malloc(sizeof(char));
			int j;
			for(j=0;j<3;j++){
				if(ptr[j] == ' '){
					break;
				}
				ext[j] = ptr[j+8];
			}

			strcat(filename, ".");
			strcat(filename, ext);

			if(strcmp(originalFilename, filename) == 0){
				fileSize = (ptr[28] & 0xFF) + ((ptr[29] & 0xFF) << 8) + ((ptr[30] & 0xFF) << 16) + ((ptr[31] & 0xFF) << 24);
				break;
			}
		}
		ptr = ptr + 32;
	}
	
	return fileSize;
}

int firstLogicalSectorAddress(char* originalFilename, char* ptr){
	int address = -1;

	while(ptr[0] != 0x00){
		if(ptr[11] != 0x0F  &&(ptr[11] & 0x02) == 0x00 && (ptr[11]&0x08) == 0x00){
			char* filename = malloc(sizeof(char));
			int i;
			for(i=0;i<8;i++){
				if(ptr[i] == ' '){
					break;
				}
				filename[i] = ptr[i];
			}

			char* ext = malloc(sizeof(char));
			int j;
			for(j=0;j<3;j++){
				if(ptr[j] == ' '){
					break;
				}
				ext[j] = ptr[j+8];
			}
			
			strcat(filename, ".");
			strcat(filename, ext);

			if(strcmp(originalFilename, filename) == 0){
				address = ptr[26] + (ptr[27] << 8);
			}
		}
		ptr = ptr + 32;
	}

	return address;
}

void makeCopy(char* ptr1, char* ptr2, char* filename, int size){
	int logicalSectorAddr = firstLogicalSectorAddress(filename, ptr1 + 512*19);
	if(logicalSectorAddr == -1) {
		printf("Cant get first logical sector address");
	}

	int bytesRem = size;
	int physAddr;

	do{
		physAddr = 512*(31 + logicalSectorAddr);
		int i;
		for(i=0;i<512;i++){
			if(bytesRem == 0){
				break;
			}

			ptr2[size - bytesRem] = ptr1[physAddr + i];
			bytesRem = bytesRem - 1;
		}

		logicalSectorAddr = getFATentry(logicalSectorAddr, ptr1);
	}while(logicalSectorAddr != 0x00 && logicalSectorAddr < 0xFF0);
}


int main(int argc, char* argv[]){
	if(argc != 3){
		printf("Invalid input in diskget");
		exit(1);
	}

	int fd1, fd2;
	struct stat sb1;

	fd1 = open(argv[1], O_RDWR);
	fstat(fd1, &sb1);

	char * p1 = mmap(NULL, sb1.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
	if (p1 == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}
	//printf("\nafter p1\n");
	
	//char * p1_temp1 = mmap(NULL, sb1.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);

	int size = sizeOfFile(argv[2], p1 + 512*19);
	if(size == -1){
		printf("File not found");
		exit(1);
	}

	//printf("\nAfter size %d\n", size);

	fd2 = open(argv[2], O_RDWR | O_CREAT, 0666);
	if(fd2 < 0){
		printf("\nError in fd2\n");
	}
	int lastByte = lseek(fd2, size-1, SEEK_SET);
	//printf("\nbefore lastbyte %d\n", lastByte);
	lastByte = write(fd2, "", 1);
	//printf("\nafter lastbyte %d\n", lastByte);

	char * p2 = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
	if (p1 == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);	
	}

	makeCopy(p1, p2, argv[2], size);

	munmap(p2, size);
	close(fd2);

	munmap(p1, sb1.st_size);
	close(fd1);
	return 0;
}















