#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>


#define _SET_WATCH_				0
#define _REM_WATCH_				1
#define _NUM_CHANGES_			2
#define _NUM_WATCH_				3
#define _FLUSH_WATCH_			4

/*
 * File Type
 */
#define _MY_MODE_				0

/*
 * Change Type
 */
#define _FILE_MODIFY_BIT_		0
#define _FILE_RENAME_BIT_		1
#define _FILE_DELETE_BIT_		2
#define _FILE_CREATE_BIT_		3
#define _FILE_OWNER_BIT_		4
#define _FILE_MODE_BIT_			5
#define _FILE_TIME_BIT_			6
#define _FILE_MMAP_BIT			7
#define _CHANGE_MIN_			8
#define _CHANGE_MAX_			126
#define _FILE_REST_				127
#define _CHANGE_OFFSET_			8

#define CHECK_BIT(var,pos) (*(var+pos) & 1)

struct data_node {
	unsigned long inode;
	int bMap[4];
	struct data_node* next;
};

/*
 * Function to denote the usage of this program
 */
#define NODE_SIZE sizeof(struct data_node)

void usage( char *prg ) {
	printf( "Usage: %s [Option]\n", prg );
	printf( "\n\t-n ARG: get number of inodes under a  watch point. ARG denotes the watch-point			\
		 \n\t-h: help\n" );
}

int do_file_func(char *path, int n) {
	int ret = 0;
	int i, j;
	FILE *file_desc;
	char file_path[20];
	void *bit;

	printf("\nDoing file ops on %d files\n", n);

	printf("Creating %d files in directory %s\n", n, path);

	for( i = 0; i < n; i++ ) {
		sprintf(file_path, "%s/%d", path, i);
		file_desc = fopen( file_path, "w");
		if(file_desc) {
			fclose(file_desc);
		}
	}

	printf("Writing data to %d files in directory %s\n", n, path);

	for( i = 0; i < n; i++ ) {
		sprintf(file_path, "%s/%d", path, i);
		file_desc = fopen( file_path, "a");
		if(file_desc) {
			for( j = 0; j < (int)(10000/(i+1)); j++)
				fprintf(file_desc, "LINE %d\n", j);
			fclose(file_desc);
		}
	}

	printf("Deleting alternate files in directory %s\n", path);
	for( i = 0; i < n; i++ ) {
		if ( i%2 == 0 ) {
			sprintf(file_path, "%s/%d", path, i);

			ret = unlink( file_path);
			if(ret < 0) {
				printf("Error in file %s\n", file_path);
			}
		}
	}

	return ret;
}

int check_bit(int *bMap, int position) {
	 return bMap[position/32] & (1 << (position%32));
}

int main( int argc, char **argv )
{
	int length;
	int fd;
	int wd;
	int num;
	int ret = 0;
	int i = 0;
	int num_node;
	void *temp_data = NULL;
	struct data_node *temp_node = NULL;
	unsigned long inode_num;
	int num1;
	int j = 0;
	void* bit;
	
	switch( getopt( argc, argv, "n:h" ) ) {
	case 'n':
		if( NULL == optarg ) {
			printf( "Missing argument for \"-s\"" );
			usage( argv[0] );
			exit(1);
		}

		num = atoi(optarg);
		if(num <= 0) {
			printf("Please enter a legitimate number as arg to n\n");
			return -1;
		}
		break;

	case 'h':
	default:
		usage( argv[0] );
		exit(0);
	}

	ret = mkdir("kwatch_test", S_IRWXU | S_IRWXG | S_IRWXO);

	if(ret !=0) {
		perror("Error");
		return -1;
	}

	ret = mkdir("kwatch_test/dir1", S_IRWXU | S_IRWXG | S_IRWXO);

	/*
	 * Set the watch on the dir1 folder
	 * do file func
	 * get the buffer
	 * display it
	 */
	ret = syscall( 349, "kwatch_test/dir1", _SET_WATCH_ );
	if(ret < 0) {
		perror("error_kwatch\n");
		return -1;
	}

	ret = do_file_func("kwatch_test/dir1", num);
	if(ret != 0) {
		perror( "do_file_func");
	}

	num_node = syscall( 349, "kwatch_test/dir1", _NUM_CHANGES_ );

	temp_data = malloc(NODE_SIZE*num_node);
	if(temp_data == NULL) {
		perror("memory exhausted");
		return -1;
	}

	ret = syscall( 350, "kwatch_test/dir1", temp_data, NODE_SIZE*num_node );
	if(ret < 0) {
		perror("error retreiving buf");
	}

	for( i = 0; i < ret; i++ ) {
		temp_node = ((struct data_node *)temp_data) + i;
		inode_num = temp_node->inode;
		if(inode_num != 0){
			printf("\n%lu\t%u\t%u\t%u\t%u", inode_num, temp_node->bMap[0], temp_node->bMap[1], temp_node->bMap[2], temp_node->bMap[3]);
			
			for( j = 0; j < sizeof(int)*4*8; j++ ) {
				if(check_bit(temp_node->bMap, j)) {
					switch(j) {
					case _FILE_MODIFY_BIT_:
						printf(" Modified ");
						break;

					case _FILE_RENAME_BIT_:
						printf(" Renamed ");
						break;

					case _FILE_DELETE_BIT_:
						printf(" Deleted ");
						break;

					case _FILE_CREATE_BIT_:
						printf(" Created ");
						break;

					case _FILE_OWNER_BIT_:
						printf(" Owner_Changed ");
						break;

					case _FILE_MODE_BIT_:
						printf(" Mode_Changed ");
						break;

					case _FILE_TIME_BIT_:
						printf(" AccessTime_Changed ");
						break;

					case _FILE_MMAP_BIT:
						printf(" Change_via_Mmap ");
						break;

					case _FILE_REST_:
						printf(" >119_block_changed ");
						break;

					default:
						printf("%d:", j-8);
						break;
					}
				}
			}
		}
	}

	ret = syscall( 349, "kwatch_test/dir1", _REM_WATCH_ );

	return 1;
}
