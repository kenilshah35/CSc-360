#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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


int main(int argc, char* argv[]){

	if(argc < 2){
		printf("Diskinfo invalid input");
		exit(1);
	}

	int fd;
	struct stat sb;

	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);
	//printf("Size: %lu\n\n", (uint64_t)sb.st_size);
	
	char * p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		printf("Error: failed to map memory\n");
		exit(1);
	}

	char* osName = malloc(sizeof(char));
	int i;
	for(i=0;i<8;i++){
		osName[i] = p[i+3];
	}

	/*
	char* labelDisk = malloc(sizeof(char));
	//Check if in boot sector
	int j;
	for(j=0;j<8;j++){
		labelDisk[j] = p[j+43];
	}
	//if not, check in root directory
	if(labelDisk[0] == ' '){
		p = p + 512*19;
		while(p[0] != 0x00){
			if(p[11] == 8){
				int k;
				for(k=0;k<8;k++){
					labelDisk[k] = p[k];
				}
			}
			p = p + 32;
		}
	}*/

	//Total size of disk
	int numOfSectors = p[19] + (p[20]<<8);
	printf("\n\n\n num sectors: %d\n\n\n", p[19]);
	int totalDiskSize = numOfSectors*512;

	//Free size of the disk
	int cntFreeSectors = 0;
	int l;
	for(l=2;l<2848;l++){
		if(getFATentry(l, p) == 0x000){
			cntFreeSectors++;
		}
	}
	int freeSizeDisk = cntFreeSectors*512;

	//TBD
	int totalFiles = 1;

	int numOfFATcopies = p[16];

	int sectorsFat = p[22] + (p[23]<<8);
	
	char* labelDisk = malloc(sizeof(char));
	int j;
	for(j=0;j<8;j++){
		labelDisk[j] = p[j+43];	
	}
	if(labelDisk[0] == ' '){
		p = p + 512*19;
		while(p[0] != 0x00){
			if(p[11] == 8){
				int k;
				for(k=0;k<8;k++){
					labelDisk[k] = p[k];
				}
			}
			p = p+32;
		}
	}



	//Prints information
	printf("OS Name: %s\n", osName);
	printf("Label of the disk: %s\n", labelDisk);
	printf("Total size of the disk: %d bytes\n", totalDiskSize);
	printf("Free size of the disk: %d bytes\n", freeSizeDisk);
	printf("\n==============\n");
	printf("The number of files in the disk (including all files in the root directory and files in all subdirectories): %d", totalFiles);
	printf("\n==============\n");
	printf("Number of FAT copies: %d\n", numOfFATcopies);
	printf("Sectors per FAT: %d\n", sectorsFat);

	munmap(p, sb.st_size); // the modifed the memory data would be mapped to the disk image
	close(fd);
	free(osName);
	free(labelDisk);

	return 0;
}


