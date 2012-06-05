/*
 * @file:			uWatch.c
 *
 * @Description:	User level program. Provides a mechanism to:
 *			1. set watch on the directories
 *			2. remove watch of directories
 *			3. get latest modified files
 *			4. flush out the info about latest modified
 *				files
 *			5. get watched directories
 *
 * @author:		Himanshu Jindal, Piyush Kansal
 */


/*
 * Include necessary header files
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>


/*
 * Include necessary constant variables
 */
#define _SET_WATCH_				0
#define _REM_WATCH_				1
#define _NUM_CHANGES_			2
#define _NUM_WATCH_				3
#define _FLUSH_WATCH_			4


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


/*
 * Constant for getting watched directories list
 */
#define _DIR_LIST_				"@#"


/*
 * Function to denote the usage of this program
 */
void usage(char *prg)
{
	printf("Usage: %s [Option]", prg);
	printf("\n\t-s ARG: set watch point. ARG denotes the watch-point"
			"\n\t-r ARG: remove watch point. ARG denotes the"
				" watch-point"
			"\n\t-n ARG: get count of latest added/modified"
				" files under a watch point."
				"ARG denotes the watch-point"
			"\n\t-f ARG: flush the changes for a watch point."
				" ARG denotes the watch-point"
			"\n\t-g ARG: get modified files under a watch point."
				" ARG denotes the watch-point"
			"\n\t-c: get the count of folders being watched"
			"\n\t-l: get the list of folders being watched\n");
}

struct data_node {
	unsigned long inode;
	int bMap[4];
	struct data_node *next;
};

int check_bit(int *bMap, int position)
{
	return bMap[position/32] & (1 << (position%32));
}

/*
 * main function
 * TODO add switch for error function and display proper error
 */
int main(int argc, char **argv)
{
	int ret;
	void *buf = NULL;
	struct data_node *temp = NULL;
	struct data_node *temp_node = NULL;
	int len;
	int i;
	unsigned long inode_num = 0;
	int j;

	/*
	 * Scan i/p parameters from command line
	 */
	switch (getopt(argc, argv, "s:r:n:f:g:cl")) {
	case 's':
		if (NULL == optarg) {
			printf("Missing argument for \"-s\"");
			usage(argv[0]);
			exit(1);
		}

		ret = syscall(349, optarg, _SET_WATCH_);
		if (ret > 0) {
			printf("watch set on %s\n", optarg);
		}
		break;

	case 'r':
		if (NULL == optarg) {
			printf("Missing argument for \"-r\"");
			usage(argv[0]);
			exit(1);
		}

		ret = syscall(349, optarg, _REM_WATCH_);
		if (ret > 0) {
			printf("watch removed from %s\n", optarg);
		}
		break;

	case 'n':
		if (NULL == optarg) {
			printf("Missing argument for \"-n\"");
			usage(argv[0]);
			exit(1);
		}

		ret = syscall(349, optarg, _NUM_CHANGES_);
		if (ret >= 0) {
			printf("Number of changes :%d in %s\n", ret, optarg);
		}
		break;

	case 'f':
		if (NULL == optarg) {
			printf("Missing argument for \"-f\"");
			usage(argv[0]);
			exit(1);
		}

		ret = syscall(349, optarg, _FLUSH_WATCH_);
		if (ret > 0) {
			printf("Watch flushed:%s\n", optarg);
		}
		break;

	case 'g':
		if (NULL == optarg) {
			printf("Missing argument for \"-g\"");
			usage(argv[0]);
			exit(1);
		}

		ret = syscall(349, optarg, _NUM_CHANGES_);

		if (0 == ret) {
			break;
		}

		len = ret * sizeof(struct data_node);
		buf = malloc(len);
		ret = syscall(350, optarg, buf, len);
		printf("Following files modified under watch of %s\n", optarg);
		printf("Inodeno\tInterpretation of the BitMap\n");
		temp = (struct data_node *) buf;
		for (i = 0; i < ret; i++) {
			temp_node = (temp+i);
			inode_num = temp_node->inode;

			if (inode_num != 0) {
				printf("%lu\t", inode_num);

				for (j = 0; j < sizeof(int)*4*8; j++) {
					if (check_bit(temp_node->bMap, j)) {
					switch (j) {
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
						printf(" >119_chunk_changed ");
						break;

					default:
						printf("%d:", j-8);
						break;
					}
					}
				}

				printf("\n");
			}
		}

		if (buf) {
			free(buf);
			buf = NULL;
		}
		break;

	case 'c':
		ret = syscall(349, NULL, _NUM_WATCH_);
		if (ret >= 0) {
			printf("Number of folders "
				"being watched :: %d\n", ret);
		}

		break;

	case 'l':
		ret = syscall(349, NULL, _NUM_WATCH_);

		if (0 == ret) {
			break;
		}

		len = ret * sizeof(unsigned long);
		buf = malloc(len);
		ret = syscall(350, _DIR_LIST_, buf, len);
		printf("Following directories ""under watch\n");
		for (i = 0; i < ret; i++) {
			printf("Directory %d :: "
			"%lu\n", i+1, *((unsigned long *)buf + i));
		}

		break;

	case 'h':
	default:
		usage(argv[0]);
		exit(0);
	}

	if (ret < 0) {
		perror("Error");
		return -1;
	}

	return 0;
}
