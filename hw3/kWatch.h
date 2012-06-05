/*
 * @file:			kWatch.c
 *
 * @Description:	Provides a kernel level mechanism to:
 *					1. set watch on the directories
 *					2. get latest modified files
 *					3. Flush out the info about latest
 *					modified files
 *
 * @author:			Himanshu Jindal, Piyush Kansal
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/crypto.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/namei.h>
#include <linux/scatterlist.h>
#include <linux/uaccess.h>
#include <linux/fdtable.h>
#include <linux/spinlock.h>

/*
 * Include appropriate Module information
 */
MODULE_AUTHOR("Piyush Kansal, Himanshu Jindal");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Encryption/Decryption Module");


/*
 * Declare required constants
 */
/*
 * Type of Watch
 */
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

/*
 * Constant for getting watched directories list
 */
#define _DIR_LIST_				"@#"

/*
 * Module Log File Name
 */
#define _LOG_FILE_				"kWatch.log"
#define _NULL_BIT_				{0, 0, 0, 0}

/*
 * Block Size and Bit Array Length, to block certain paths
 */
#define _BLOCK_ARR_LEN_			14
#define _BLOCK_ARRAY_			{"/proc", "/dev", "/ptmx", "ptmx", "/var", \
								"/tmp", "/bin", "/sbin", "/boot", \
								"/lib", "/mnt", "/etc", "/opt", \
								"/srv"}

/*
 * Max Pathlen supported
 */
#define _MAX_PATH_LEN_			300
#define _NULL_CHAR_				'0'

/*
 * The data node which will maintain a list of inodes
 * of all the changes inside a watched folder
 */
struct data_node {
	unsigned long inode;
	int bMap[4];
	struct data_node *next;
};

/*
 * The head node which will mantain a list of all watched folder
 */
struct head_node {
	unsigned long inode;
	struct data_node *data;
	struct head_node *next;
	int num_data;
} *main_obj;

/*	Declare pointers to the original system calls.
	-	The reason we keep them, rather than call the original function
		(sys_XXXX), is because somebody else might have replaced the
		system call before us. Note that this is not 100% safe, because
		if another module replaced sys_open before us, then when we're
		inserted we'll call the function in that module and it might be
		removed before we are
	-	Another reason for this is that we can't get sys_XXXX as they
		are static variables, so they are not exported
 */
asmlinkage int
(*orig_sys_watch)	(const char * const file,
					int option);

asmlinkage int
(*orig_get_watch)	(const char * const file,
					void *user_buf,
					int buf_len);

asmlinkage long
(*orig_sys_utimensat)	(int dfd,
						const char __user *filename,
						struct timespec __user *utimes,
						int flags);

asmlinkage long
(*orig_sys_utimes)	(char __user *filename,
					struct timeval __user *utimes);

asmlinkage long
(*orig_sys_write)	(unsigned int fd,
					const char __user *buf,
					size_t count);

asmlinkage long
(*orig_sys_unlink)	(const char __user *pathname);

asmlinkage long
(*orig_sys_truncate)(const char __user *path,
					long length);

asmlinkage long
(*orig_sys_ftruncate)	(unsigned int fd,
						unsigned long length);

asmlinkage long
(*orig_sys_rename)	(const char __user *oldname,
					const char __user *newname);

asmlinkage long
(*orig_sys_fchmod)	(unsigned int fd,
					mode_t mode);

asmlinkage long
(*orig_sys_fchown)	(unsigned int fd,
					uid_t user,
					gid_t group);

asmlinkage long
(*orig_sys_rmdir)	(const char __user *pathname);

asmlinkage long
(*orig_sys_chmod)	(const char __user *filename,
					mode_t mode);

asmlinkage long
(*orig_sys_chown)	(const char __user *filename,
					uid_t user,
					gid_t group);

asmlinkage long
(*orig_sys_open)	(const char __user *filename,
					int flags,
					int mode);

asmlinkage long
(*orig_sys_access)	(const char __user *filename,
					int mode);

asmlinkage long
(*orig_sys_mkdir)	(const char __user *pathname,
					umode_t mode);

/*
 * System calls created for module
 */
asmlinkage int
my_sys_watch(const char * const dirname,
				int option);

asmlinkage int
my_get_watch(const char * const file,
				void *user_buf,
				int buf_len);

/*
 * System calls related to file creation, time modification
 */
asmlinkage long
my_sys_open(const char __user *filename,
			int flags,
			int mode);

asmlinkage long
my_sys_utimensat(int dfd,
				const char __user *filename,
				struct timespec __user *utimes,
				int flags);

asmlinkage long
my_sys_utimes(char __user *filename,
			struct timeval __user *times);

asmlinkage long
my_sys_chmod(const char __user *filename,
			mode_t mode);

asmlinkage long
my_sys_mkdir(const char __user *pathname,
			umode_t mode);

/*
 * System calls related to date written to file
 */
asmlinkage long
my_sys_write(unsigned int fd,
			const char __user *buf,
			size_t count);

/*
 * System calls related to file unlinking
 */
asmlinkage long
my_sys_unlink(const char __user *pathname);

asmlinkage long
my_sys_rename(const char __user *oldname,
				const char __user *newname);

asmlinkage long
my_sys_rmdir(const char __user *pathname);

/*
 * System calls related to file truncation
 */
asmlinkage long
my_sys_truncate(const char __user *path,
				long length);

asmlinkage long
my_sys_ftruncate(unsigned int fd,
					unsigned long length);

/*
 * System calls related to file permissions and ownership
 */
asmlinkage long
my_sys_fchmod(unsigned int fd,
				mode_t mode);

asmlinkage long
my_sys_fchown(unsigned int fd,
				uid_t user,
				gid_t group);

asmlinkage long
my_sys_chmod(const char __user *filename,
			mode_t mode);

asmlinkage long
my_sys_chown(const char __user *filename,
			uid_t user,
			gid_t group);

/*
 * User defined functions
 */
int is_stat(unsigned long inode);
int get_inode(const char * const filename, unsigned long *inodeNo);
int set_watch(const char * const dirname);
int num_watch(void);
int rem_watch(const char * const filename);
int flush_watch(const char * const filename);
int num_changes(const char * const filename);
void join_bits(int *obj1, int *obj2);
char *my_get_from_user(const char * const dirname);
/* void myprintf(char *frmt, ...); */
unsigned long check_if_any_parent_is_watched(char *fileName);
unsigned long check_if_any_parent_is_watched_filp(struct file *filePtr);
void process_file_desc(unsigned int fd, int *bMap);
void process_file_name(const char __user *filename, int *bMap);
void add_data_to_obj(unsigned long head, unsigned long data, int *bMap);
void cleanup_obj(struct data_node *head);
void cleanup_head(void);
