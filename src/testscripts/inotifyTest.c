#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <fcntl.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 3 * 5000 * ( EVENT_SIZE + 16 ) )

/*
 * Function to denote the usage of this program
 */

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

int main( int argc, char **argv ) 
{
	int length;
	int fd;
	int wd;
	int num;
	int ret = 0;
	char buffer[BUF_LEN];
	int i = 0;

	switch( getopt( argc, argv, "n:h" ) ) {
	case 'n':
		if( NULL == optarg ) {
			printf( "Missing argument for \"-s\"" );
			usage( argv[0] );
			exit(1);
		}

		num = atoi(optarg);
		if(num == 0) {
			printf("Please enter a legitimate number as arg to n\n");
			return -1;
		}
		break;

	case 'h':
	default:
		usage( argv[0] );
		exit(0);
	}

	ret = mkdir("inotify_test", S_IRWXU | S_IRWXG | S_IRWXO);

	if(ret !=0) {
		perror("Error");
		return -1;
	}

	ret = mkdir("inotify_test/dir1", S_IRWXU | S_IRWXG | S_IRWXO);

	fd = inotify_init();
	if ( fd < 0 ) {
		perror( "inotify_init\n" );
		return -1;
	}

	wd = inotify_add_watch( fd, "inotify_test/dir1", IN_MODIFY | IN_CREATE | IN_DELETE );
	
	if( wd < 0) {
		perror( "inotify_add_watch");
	}

	ret = do_file_func("inotify_test/dir1", num);
	if(ret != 0) {
		perror( "do_file_func");
		return -1;
	}

	length = read( fd, buffer, BUF_LEN );
	
	if ( length < 0 ) {
		perror( "read" );
		return -1;
	}
	i = 0;
	while ( i < length ) {
		struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
		if ( event->len ) {
			if ( event->mask & IN_CREATE ) {
				if ( event->mask & IN_ISDIR ) {
					printf( "The directory %s was created.\n", event->name );
				}
				else {
					printf( "The file %s was created.\n", event->name );
				}
			}
			else if ( event->mask & IN_DELETE ) {
				if ( event->mask & IN_ISDIR ) {
					printf( "The directory %s was deleted.\n", event->name );
				}
				else {
					printf( "The file %s was deleted.\n", event->name );
				}
			}
			else if ( event->mask & IN_MODIFY ) {
				if ( event->mask & IN_ISDIR ) {
					printf( "The directory %s was modified.\n", event->name );
				}
				else {
					printf( "The file %s was modified.\n", event->name );
				}
			}
		}
		i += EVENT_SIZE + event->len;
	}
	( void ) inotify_rm_watch( fd, wd );
	( void ) close( fd );

	exit( 0 );
}
