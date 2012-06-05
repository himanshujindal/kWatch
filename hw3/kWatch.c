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

#include "kWatch.h"


/*
 * Declare extern/global variables
 */
extern void *sys_call_table[];
struct file *filePtr;
uid_t userEuid;
int Block_Size;


/*
 * Bit operations to handle bitmap
 * http://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit-in-c
 *
 */
void join_bits(int *bMap1, int *bMap2)
{
	int i = 0;
	for (i = 0; i < 4; i++)
		bMap1[i] |= bMap2[i];
}


/*
 * http://tldp.org/LDP/Linux-Filesystem-Hierarchy/html/the-root-directory.html
 * Check if the pathname passed is in
 * the other file paths, which we do not track
 * 1 - has to be filtered
 * 0 - Do not filter
 */
int path_filter(const char * const pathname)
{
	int ret = 0;
	int i = 0;
	int len1, len2, len;
	char *block_arr[_BLOCK_ARR_LEN_] = _BLOCK_ARRAY_;
	if (pathname == NULL) {
		ret = 1;
		goto OUT;
	}

	len2 = strlen(pathname);
	if (len2 < 0) {
		ret = 1;
		goto OUT;
	}
	if (len2 == 1) {
		if (0 == strncmp(pathname, "/", 1)) {
			ret = 1;
			goto OUT;
		}
	} else if (len2 >= 4) {
		while (i < _BLOCK_ARR_LEN_) {
			len1 = strlen(block_arr[i]);
			if (len2 < len1) {
				len = len2;
			} else {
				len = len1;
			}

			if (strncmp(pathname, block_arr[i], len) == 0) {
				ret = 1;
				goto OUT;
			}

			i++;
		}
	}

OUT:
	return ret;
}


/*
 * Function that takes a user path and
 * converts it to a kernel space string
 * Remember to free the memory allocated here
 * in the function that calls this function
 */
char *my_get_from_user(const char * const dirname)
{
	int ret = 0;
	char *ret_val = NULL;
	int ipLen = 0;

	/*
	 * Verify if we have access to
	 * the pointers
	 */
	ret = access_ok(VERIFY_READ, dirname, 0);
	if (!ret) {
		ret = -EFAULT;
		goto OUT;
	}

	ipLen = strlen_user(dirname);
	ret_val = kmalloc(ipLen, GFP_KERNEL);
	if (NULL == ret_val) {
		ret = -ENOMEM;
		goto OUT;
	} else {
		ret = strncpy_from_user(ret_val, dirname, ipLen);
		if (ret < 0) {
			goto OUT;
		}
	}

OUT:
	if (ret < 0) {
		if (ret_val) {
			kfree(ret_val);
			ret_val = NULL;
		}
	}

	return ret_val;
}

/*
 * Function definition of our own printf()
 * for logging purposes
 */
/*
void myprintf(char *frmt, ...)
{
	char string[300];
	char *buff;
	int len = 0;
	mm_segment_t oldfs;
	va_list argp;

	va_start(argp, frmt);

	len = vscnprintf(string, 300, frmt, argp);
	buff = kmalloc(len+1, GFP_KERNEL);
	memset(buff, len, '\0');
	memcpy(buff, string, len);
	buff[len] = '\n';

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	len = filePtr->f_op->write(filePtr, buff, len+1, &filePtr->f_pos);
	set_fs(oldfs);

	va_end(argp);

	kfree(buff);
	buff = NULL;
}
*/

/*
 * Function that takes in a file desc
 * And checks recursively if any of its parent
 * is under watch
 * Also takes the int bMap which specifies what changes
 * were made
 */
void process_file_desc(unsigned int fd, int *bMap)
{
	unsigned long parentInodeNo;
	struct file *curFile = NULL;
	char *path = NULL;
	char *path_buf = NULL;

	if (fd < 3) {
		goto OUT;
	}

	curFile = fget(fd);
	if (!curFile) {
		curFile = NULL;
		goto OUT;
	}

	path_buf = kmalloc(_MAX_PATH_LEN_, GFP_KERNEL);
	if (path_buf == NULL) {
		goto OUT;
	}
	path = dentry_path_raw(curFile->f_path.dentry, path_buf,
							_MAX_PATH_LEN_);
	if (NULL != path) {
		if (path_filter(path)) {
			goto OUT;
		}
	}

	parentInodeNo = check_if_any_parent_is_watched_filp(curFile);
	if (parentInodeNo > 0) {
		add_data_to_obj(parentInodeNo,
					curFile->f_path.dentry->d_inode->i_ino,
					bMap);
	}

OUT:
	if (curFile) {
		fput(curFile);
		curFile = NULL;
	}

	if (path_buf) {
		kfree(path_buf);
		path_buf = NULL;
	}
}

/*
 * Function similar to process_file_desc
 * but takes a filename (kernel space)
 * as arg
 */
void process_file_name(const char __user *filename, int *bMap)
{
	int ret;
	unsigned long fileInodeNo = 0, parentInodeNo = 0;
	char *kern_filename = NULL;

	kern_filename = my_get_from_user(filename);
	if (NULL == kern_filename) {
		goto OUT;
	}

	if (path_filter(kern_filename)) {
		goto OUT;
	}
	ret = get_inode(kern_filename, &fileInodeNo);
	if (ret < 0) {
		goto OUT;
	}

	parentInodeNo = check_if_any_parent_is_watched(kern_filename);
	if (parentInodeNo > 0) {
		add_data_to_obj(parentInodeNo, fileInodeNo, bMap);
	}

OUT:
	if (kern_filename) {
		kfree(kern_filename);
		kern_filename = NULL;
	}
}

/*
 * Function to check if an inode is currently being tracked
 * Returns 1 if true, 0 if false
 */
int is_stat(unsigned long inode)
{
	struct head_node *temp;
	int found = 0;
	temp = main_obj->next;

	while (temp != NULL) {
		if (temp->inode == inode) {
			found = 1;
			break;
		}

		temp = temp->next;
	}

	return found;
}

/*
 * Function to add inode info under a watched directory's inode
 * This is to be added to the chain of data connected to a head
 * Also takes a bitmap. If the data is already present,
 * ORs the bitmap to represent the full changes
 */
void add_data_to_obj(unsigned long head, unsigned long data, int *bMap)
{
	struct head_node *temp1;
	struct data_node *temp2, *temp3, *new_node;
	int ret = -1;
	int i = 0;
	temp1 = main_obj->next;

	new_node = kmalloc(sizeof(struct data_node), GFP_KERNEL);
	if (new_node == NULL) {
		ret = -ENOMEM;
		goto OUT;
	}

	new_node->inode = data;
	new_node->next = NULL;

	for (i = 0; i < 4; i++)
		(new_node->bMap)[i] = bMap[i];

	while (temp1 != NULL) {
		if (temp1->inode == head) {
			if (temp1->data == NULL) {
				temp1->data = new_node;
				temp1->num_data = 1;
				ret = 0;
				goto OUT;
			} else {
				temp2 = temp1->data;
				temp3 = temp2;

				while (temp2 != NULL) {
					if (temp2->inode == data) {
						join_bits(temp2->bMap, bMap);
						goto OUT;
					}

					temp3 = temp2;
					temp2 = temp2->next;
				}

				temp3->next = new_node;
				temp1->num_data++;
				ret = 0;
				goto OUT;
			}
		} else {
			temp1 = temp1->next;
		}
	}

OUT:
	if (ret < 0) {
		if (new_node) {
			kfree(new_node);
			new_node = NULL;
		}
	}

	return;
}


/*
 * Insert a new head object in the watch structure
 * This just adds an entry to the main ds
 * Changes will be added later to this heads data nodes
 */
int insert_obj(struct head_node *head)
{
	struct head_node *temp1, *temp2;
	int ret = -1;
	temp1 = main_obj->next;
	temp2 = main_obj;

	while (temp1 != NULL) {
		if (temp1->inode == head->inode) {
			goto OUT;
		}

		temp2 = temp1;
		temp1 = temp1->next;
	}

	temp2->next = head;
	ret = 0;

OUT:
	return ret;
}

/*
 * Function to get the memory for a particular head
 */
int set_handle(int inode_num)
{
	struct head_node *head;
	int ret = -1;

	head = kmalloc(sizeof(struct head_node), GFP_KERNEL);
	if (NULL == head) {
		goto OUT;
	}

	/* Fill up the temporary head */
	head->inode = inode_num;
	head->next = NULL;
	head->data = NULL;
	head->num_data = 0;

	/* Insert the head in the correct place */
	ret = insert_obj(head);

OUT:
	if (ret < 0) {
		if (head) {
			kfree(head);
			head = NULL;
		}
	}

	return ret;
}

/*
 * Return the inode for a particular filename and checks if it is a file or
 * directory depending on the second parameters value return -errno if
 * failure returns 0 if success
 *
 * Security feature:
 * Also checks if the process opening the file is the owner of the file
 */
int get_inode(const char *const filename, unsigned long *inodeNo)
{
	int errno = -EINVAL;
	struct file *dirPtr = NULL;
	struct inode *dir_inode = NULL;

	*inodeNo = 0;
	/* Open I/P file. It will report error in case of non-existing file
	and bad permissions but not bad owner */
	dirPtr = filp_open(filename, O_RDONLY, 0);
	if (!dirPtr || IS_ERR(dirPtr)) {
		errno = (int)PTR_ERR(dirPtr);
		dirPtr = NULL;
		goto OUT;
	}

	/*Get the inode of the open file */
	dir_inode = dirPtr->f_path.dentry->d_inode;

	/* Check if the file ptr belongs to a directory */
	if (!(S_ISDIR(dir_inode->i_mode) || S_ISREG(dir_inode->i_mode))) {
		errno = -EBADF;
		goto OUT;
	}

	/* Check if the I/p file and the process owner match */
	if ((current->real_cred->uid != dir_inode->i_uid)
			&& (current->real_cred->uid != 0)) {
		errno = -EACCES;
		goto OUT;
	}

	/*Take inode number and store */
	*inodeNo = dir_inode->i_ino;

	errno = 0;

OUT:
	if (dirPtr) {
		filp_close(dirPtr, NULL);
		dirPtr = NULL;
	}

	return errno;
}


/*
 * Takes a file name and checks recursively if any parent of this
 * file/folder is under watch
 */
unsigned long check_if_any_parent_is_watched(char *fileName)
{
	int errno;
	unsigned long parentInodeNo = 0;
	struct file *filePtr = NULL;

	filePtr = filp_open(fileName, O_RDONLY, 0);
	if (!filePtr || IS_ERR(filePtr)) {
		errno = (int)PTR_ERR(filePtr);
		filePtr = NULL;
		goto OUT;
	}

	parentInodeNo = check_if_any_parent_is_watched_filp(filePtr);

OUT:
	if (filePtr) {
		filp_close(filePtr, NULL);
		filePtr = NULL;
	}
	return parentInodeNo;
}

/*
 * Takes a file ptr and checks if any parent is under watch
 * fileptr can be obtained from an fd and from a file name too and then
 * passed to this function
 * Assumes filePtr is a valid file pointer
 */
unsigned long check_if_any_parent_is_watched_filp(struct file *filePtr)
{
	unsigned long parentInodeNo = 0, tempno = 0;
	struct dentry *parentPtr = NULL;

	/* To Prevent the parent dentry loop from going in an infinite loop */
	int i = 0;

	/* Get the dentry of the parent of the open file */
	parentPtr = filePtr->f_path.dentry->d_parent;
	while (parentPtr != NULL && i < 20) {
		tempno = parentPtr->d_inode->i_ino;
		if (is_stat(tempno)) {
			parentInodeNo = tempno;
			break;
		} else if (IS_ROOT(parentPtr)) {
			break;
		} else {
			i++;
			parentPtr = parentPtr->d_parent;
		}
	}

	return parentInodeNo;
}

/*
 * System call to get the buffer of files changed
 * in a directory
 */
asmlinkage int my_get_watch(const char * const file, void *user_buf,
							int buf_len)
{
	int errno = 0;
	struct head_node *temp_head = main_obj->next;
	struct data_node *temp_data = NULL;
	unsigned long inode_num;
	void *temp_ptr = NULL;
	int found = 0;
	void *kern_buf = NULL;
	int num = 0;
	int size = 0;
	int i = 0, ret;
	char getWatDirs = 'N';

	errno = access_ok(VERIFY_WRITE, user_buf, buf_len);
	if (!errno) {
		errno = -EACCES;
		goto OUT;
	}

	/* Get the list of directories being watched */
	if (0 == strncmp(file, _DIR_LIST_, strlen(_DIR_LIST_))) {
		getWatDirs = 'Y';
		size = sizeof(unsigned long);
	} else {
		errno = get_inode(file, &inode_num);
		if (errno < 0) {
			goto OUT;
		}

		while (NULL != temp_head) {
			if (inode_num == temp_head->inode) {
				found = 1;
				break;
			}

			temp_head = temp_head->next;
		}

		if (!found) {
			goto OUT;
		}

		size = sizeof(struct data_node);
	}

	num = buf_len/size;
	kern_buf = kmalloc(buf_len, GFP_KERNEL);
	if (!kern_buf) {
		errno = -ENOMEM;
		goto OUT;
	}

	memset(kern_buf, _NULL_CHAR_, buf_len);

	if ('N' == getWatDirs) {
		temp_data = temp_head->data;
		if (temp_data == NULL) {
			ret = copy_to_user(user_buf, kern_buf, buf_len);
			goto OUT;
		}

		while ((i < num) && (temp_data != NULL)) {
			temp_ptr = temp_data;
			memcpy(kern_buf + size*i, temp_ptr, size);
			i++;
			temp_data = temp_data->next;
		}
	} else {
		while (NULL != temp_head) {
			*((unsigned long *)kern_buf + i) = temp_head->inode;
			temp_head = temp_head->next;
			++i;
		}
	}

	ret = copy_to_user(user_buf, kern_buf, buf_len);
	errno = i;

OUT:
	if (kern_buf) {
		kfree(kern_buf);
		kern_buf = NULL;
	}

	return errno;
}

/*
 * System call for the user to interact with the module
 */
asmlinkage int my_sys_watch(const char * const dirname, int option)
{
	int errno = -EINVAL;
	char *kern_dirname = NULL;

	if (_NUM_WATCH_ == option) {
		errno = num_watch();
		goto OUT;
	}

	/* Check for NULL pointers or invalid values */
	if (NULL == dirname) {
		errno = -EINVAL;
		goto OUT;
	}

	kern_dirname = my_get_from_user(dirname);
	if (NULL == kern_dirname) {
		goto OUT;
	}

	if (path_filter(kern_dirname)) {
		errno = -EINVAL;
		goto OUT;
	}

	switch (option) {
	case _SET_WATCH_:
		errno = set_watch(kern_dirname);
		if (errno < 0) {
			goto OUT;
		}

		if (sys_call_table[__NR_write] != my_sys_write) {
			orig_sys_write = sys_call_table[__NR_write];
			sys_call_table[__NR_write] = my_sys_write;
		}

		break;

	case _REM_WATCH_:
		errno = rem_watch(kern_dirname);
		break;

	case _NUM_CHANGES_:
		errno = num_changes(kern_dirname);
		break;

	case _FLUSH_WATCH_:
		errno = flush_watch(kern_dirname);
		break;
	}

OUT:
	if (kern_dirname) {
		kfree(kern_dirname);
		kern_dirname = NULL;
	}

	return errno;
}

/*
 * Function to set a watch
 * This will add a handle with the specified inode
 */
int set_watch(const char * const dirname)
{
	int errno = -EINVAL;
	unsigned long inode_num, parent_inode;

	errno = get_inode(dirname, &inode_num);
	if (errno < 0) {
		goto OUT;
	}

	parent_inode = check_if_any_parent_is_watched((char *)dirname);
	if (parent_inode > 0) {
		errno = -EINVAL;
		goto OUT;
	}

	errno = set_handle(inode_num);

OUT:
	return errno;
}

/*
 * Function returning the number of watched folder
 */
int num_watch(void)
{
	int i = 0;
	struct head_node *temp1 = main_obj->next;

	while (temp1 != NULL) {
		temp1 = temp1->next;
		i++;
	}

	return i;
}

/*
 * Gets the number of changes made inside a watched directory
 */
int num_changes(const char * const filename)
{
	unsigned long inode_num = 0;
	int errno = 0;
	struct head_node *temp1 = main_obj->next;

	errno = get_inode(filename, &inode_num);

	if (errno < 0) {
		goto OUT;
	}

	while (temp1 != NULL) {
		if (temp1->inode == inode_num) {
			errno = temp1->num_data;
			break;
		}

		temp1 = temp1->next;
	}

OUT:
	return errno;
}

/*
 * Function to remove the watch from a specified handle
 */
int rem_watch(const char * const filename)
{
	unsigned long inode_num = 0;
	int errno = 0;
	struct head_node *temp1 = NULL;
	struct head_node *temp2 = NULL;
	/* Get inode number */

	errno = get_inode(filename, &inode_num);

	if (errno < 0) {
		goto OUT;
	}

	/* Check if the inode is under watch*/
	temp1 = main_obj;
	temp2 = main_obj->next;
	while (temp2 != NULL) {
		if (temp2->inode == inode_num) {
			if (temp2->data != NULL) {
				cleanup_obj(temp2->data);
			}

			temp1->next = temp2->next;
			kfree(temp2);
			temp2 = NULL;
			errno = 1;
			break;
		} else {
			temp1 = temp2;
			temp2 = temp2->next;
		}
	}

OUT:
	return errno;
}

/*
 * Function to flush the watched files for a specified handle
 */
int flush_watch(const char * const filename)
{
	unsigned long inode_num = 0;
	int errno = 0;
	struct head_node *temp2 = NULL;
	/* Get inode number */

	errno = get_inode(filename, &inode_num);

	if (errno < 0) {
		goto OUT;
	}

	/* Check if the inode is under watch*/
	temp2 = main_obj->next;
	while (temp2 != NULL) {
		if (temp2->inode == inode_num) {
			if (temp2->data != NULL) {
				cleanup_obj(temp2->data);
			}

			temp2->data = NULL;
			temp2->num_data = 0;
			errno = 1;
			break;
		} else {
			temp2 = temp2->next;
		}
	}

OUT:
	return errno;
}


/*
 * Intercept utimensat()
 */
asmlinkage long my_sys_utimensat(int dfd, const char __user *filename,
					struct timespec __user *utimes,
					int flags)
{
	long errno = -EINVAL;
	int temp[4] = _NULL_BIT_;

	errno = (*orig_sys_utimensat)(dfd, filename, utimes, flags);

	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_TIME_BIT_, (void *)&temp);
	process_file_name(filename, temp);

OUT:
	return errno;
}


/*
 * Intercept utimes()
 */
asmlinkage long my_sys_utimes(char __user *filename,
					struct timeval __user *times)
{
	long errno;
	int temp[4] = _NULL_BIT_;

	errno = orig_sys_utimes(filename, times);
	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_TIME_BIT_, (void *)&temp);
	process_file_name(filename, temp);

OUT:
	return errno;
}

/*
 * Intercept open()
 */
asmlinkage long my_sys_open(const char __user *filename,
					int flags, int mode)
{
	long errno;
	int ret = 0;
	int temp[4] = _NULL_BIT_;
	ret = orig_sys_access(filename, _MY_MODE_);

	errno = orig_sys_open(filename, flags, mode);

	if (errno < 0) {
		goto OUT;
	}

	if (mode || O_EXCL) {
		set_bit(_FILE_CREATE_BIT_, (void *)&temp);
		process_file_name(filename, temp);
		goto OUT;
	}

	if (mode || O_CREAT) {
		if (ret < 0) {
			set_bit(_FILE_CREATE_BIT_, (void *)&temp);
			process_file_name(filename, temp);
			goto OUT;
		}
	}

	if (mode || O_TRUNC) {
		set_bit(_FILE_CREATE_BIT_, (void *)&temp);
		process_file_name(filename, temp);
		goto OUT;
	}

OUT:
	return errno;
}

/*
 * Intercept write()
 */
asmlinkage long my_sys_write(unsigned int fd, const char __user *buf,
					size_t count)
{
	long errno = 0;
	struct file *filp = NULL;
	int change_start = 0, change_end = 0;
	int temp[4] = _NULL_BIT_;
	int i;
	filp = fget(fd);

	if (!filp) {
		filp = NULL;
	} else {
		change_start = filp->f_pos;
		change_start /= Block_Size;
		change_end = filp->f_pos + count;
		change_end /= Block_Size;
	}

	errno = orig_sys_write(fd, buf, count);
	if (errno < 0) {
		goto OUT;
	}

	if (!filp) {
		goto OUT;
	} else {
		fput(filp);
		filp = NULL;
		if (userEuid == current->real_cred->euid) {
			set_bit(_FILE_MODIFY_BIT_, (void *)&temp);
			if (change_start + _CHANGE_OFFSET_ > _CHANGE_MAX_) {
				change_start = _FILE_REST_;
			} else {
				change_start = change_start + _CHANGE_OFFSET_;
			}

			if (change_end + _CHANGE_OFFSET_ > _CHANGE_MAX_) {
				change_end = _FILE_REST_;
			} else {
				change_end = change_end + _CHANGE_OFFSET_;
			}

			for (i = change_start; i <= change_end; i++)
				set_bit(i, (void *)&temp);

			process_file_desc(fd, temp);
		}
	}

OUT:
	if (filp) {
		fput(filp);
		filp = NULL;
	}

	return errno;
}

/*
 * Intercept rmdir()
 */
asmlinkage long my_sys_rmdir(const char __user *pathname)
{
	int errno = -EINVAL;
	unsigned long inodeNum = 0, inodeParent = 0;
	char *kern_pathname = NULL;
	int ret;
	int temp[4] = _NULL_BIT_;

	if (NULL == pathname) {
		goto OUT;
	}

	kern_pathname = my_get_from_user(pathname);
	if (kern_pathname != NULL) {
		if (path_filter(kern_pathname) == 0) {
			ret = get_inode(kern_pathname, &inodeNum);
			if (ret == 0) {
				inodeParent = check_if_any_parent_is_watched(kern_pathname);
			}
		}
	}

	errno = orig_sys_rmdir(pathname);
	if (errno < 0) {
		goto OUT;
	}

	if (inodeParent > 0) {
		set_bit(_FILE_DELETE_BIT_, (void *)&temp);
		add_data_to_obj(inodeParent, inodeNum, temp);
	}

OUT:
	if (kern_pathname) {
		kfree(kern_pathname);
		kern_pathname = NULL;
	}

	return errno;
}

/*
 * Intercept mkdir()
 */
asmlinkage long my_sys_mkdir(const char __user *pathname, umode_t mode)
{
	long errno = 0;
	int temp[4] = _NULL_BIT_;
	errno = orig_sys_mkdir(pathname, mode);

	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_CREATE_BIT_, (void *)&temp);
	process_file_name(pathname, temp);

OUT:
	return errno;
}

/*
 * Intercepted unlink
 */
asmlinkage long my_sys_unlink(const char __user *pathname)
{
	int ret = 0;
	long errno = -EINVAL;
	unsigned long inodeNum = 0, inodeParent = 0;
	int temp[4] = _NULL_BIT_;
	char *kern_pathname = NULL;

	if (NULL == pathname) {
		goto OUT;
	}

	kern_pathname = my_get_from_user(pathname);
	if (kern_pathname != NULL) {
		if (path_filter(kern_pathname) == 0) {
			ret = get_inode(kern_pathname, &inodeNum);
			if (ret == 0) {
				inodeParent = check_if_any_parent_is_watched(kern_pathname);
			}
		}
	}

	errno = orig_sys_unlink(pathname);
	if (errno < 0) {
		goto OUT;
	}

	if (inodeParent > 0) {
		if (0 <= ret) {
			set_bit(_FILE_DELETE_BIT_, (void *)&temp);
			add_data_to_obj(inodeParent, inodeNum, temp);
		}
	}

OUT:
	if (kern_pathname) {
		kfree(kern_pathname);
		kern_pathname = NULL;
	}
	return errno;
}

/*
 * Intercept truncate()
 */
asmlinkage long my_sys_truncate(const char __user *path, long length)
{
	long  errno;
	int temp[4] = _NULL_BIT_;
	int len = length;
	int change_start;
	int change_end;
	int i;

	errno = orig_sys_truncate(path, length);
	if (errno < 0) {
		goto OUT;
	}

	change_start = len/Block_Size;
	if (change_start + _CHANGE_OFFSET_ > _CHANGE_MAX_) {
		change_start = _FILE_REST_;
	} else {
		change_start = change_start + _CHANGE_OFFSET_;
	}

	change_end = _FILE_REST_;
	for (i = change_start; i < change_end; i++) {
		set_bit(i, (void *)&temp);
	}

	set_bit(_FILE_MODIFY_BIT_ , (void *)&temp);
	process_file_name(path, temp);

OUT:
	return errno;
}

/*
 * Intercept ftruncate()
 */
asmlinkage long my_sys_ftruncate(unsigned int fd, unsigned long length)
{
	long errno;
	int change_start, change_end;
	int i;
	int temp[4] = _NULL_BIT_;
	int len = length;

	errno = orig_sys_ftruncate(fd, length);
	if (errno < 0) {
		goto OUT;
	}

	change_start = len/Block_Size;
	if (change_start + _CHANGE_OFFSET_ > _CHANGE_MAX_) {
		change_start = _FILE_REST_;
	} else {
		change_start = change_start + _CHANGE_OFFSET_;
	}

	change_end = _FILE_REST_;
	for (i = change_start; i < change_end; i++) {
		set_bit(i, (void *)&temp);
	}

	set_bit(_FILE_MODIFY_BIT_, (void *)&temp);
	process_file_desc(fd, temp);

OUT:
	return errno;
}

/*
 * Intercept rename()
 */
asmlinkage long my_sys_rename(const char __user *oldname,
					const char __user *newname)
{
	long errno;
	int temp[4] = _NULL_BIT_;

	errno = orig_sys_rename(oldname, newname);
	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_RENAME_BIT_, (void *)&temp);
	process_file_name(newname, temp);

OUT:
	return errno;
}

/*
 * Intercept chmod()
 */
asmlinkage long my_sys_chmod(const char __user *filename, mode_t mode)
{
	long errno;
	int temp[4] = _NULL_BIT_;

	errno = orig_sys_chmod(filename, mode);
	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_MODE_BIT_, (void *)&temp);
	process_file_name(filename, temp);

OUT:
	return errno;
}

/*
 * Intercept fchmod()
 */
asmlinkage long my_sys_fchmod(unsigned int fd, mode_t mode)
{
	long errno;
	int temp[4] = _NULL_BIT_;

	errno = orig_sys_fchmod(fd, mode);
	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_MODE_BIT_, (void *)&temp);
	process_file_desc(fd, temp);

OUT:
	return errno;
}

/*
 * Intercept fchown()
 */
asmlinkage long my_sys_fchown(unsigned int fd, uid_t user, gid_t group)
{
	long errno;
	int temp[4] = _NULL_BIT_;

	errno = orig_sys_fchown(fd, user, group);
	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_OWNER_BIT_, (void *)&temp);
	process_file_desc(fd, temp);

OUT:
	return errno;
}

/*
 * Have overwritten chown32 as that is the one which is called
 * for this architecture
 */
asmlinkage long my_sys_chown(const char __user *filename,
				uid_t user, gid_t group)
{
	long errno;
	int temp[4] = _NULL_BIT_;

	errno = orig_sys_chown(filename, user, group);
	if (errno < 0) {
		goto OUT;
	}

	set_bit(_FILE_OWNER_BIT_, (void *)&temp);
	process_file_name(filename, temp);

OUT:
	return errno;
}

/*
 * Init module
 * Open log file, if in debug mode
 * Intercept system calls
 * Initialise main obj
 */
int init_module(void)
{
	int errno = 0;
/*
	filePtr = filp_open(_LOG_FILE_, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	if (!filePtr || IS_ERR(filePtr)) {
		errno = (int)PTR_ERR(filePtr);
		filePtr = NULL;
		goto OUT;
	}

	filePtr->f_pos = 0;
*/
	/* Store the pointer to the original system calls */
	orig_sys_access = sys_call_table[__NR_access];
	orig_sys_write = NULL;

	orig_sys_watch = sys_call_table[__NR_sys_watch];
	sys_call_table[__NR_sys_watch] = my_sys_watch;

	orig_get_watch = sys_call_table[__NR_sys_get_watch];
	sys_call_table[__NR_sys_get_watch] = my_get_watch;

	orig_sys_utimensat = sys_call_table[__NR_utimensat];
	sys_call_table[__NR_utimensat] = my_sys_utimensat;

	orig_sys_utimes = sys_call_table[__NR_utimes];
	sys_call_table[__NR_utimes] = my_sys_utimes;

	orig_sys_unlink = sys_call_table[__NR_unlink];
	sys_call_table[__NR_unlink] = my_sys_unlink;

	orig_sys_rmdir = sys_call_table[__NR_rmdir];
	sys_call_table[__NR_rmdir] = my_sys_rmdir;

	orig_sys_truncate = sys_call_table[__NR_truncate];
	sys_call_table[__NR_truncate] = my_sys_truncate;

	orig_sys_mkdir = sys_call_table[__NR_mkdir];
	sys_call_table[__NR_mkdir] = my_sys_mkdir;

	orig_sys_ftruncate = sys_call_table[__NR_ftruncate];
	sys_call_table[__NR_ftruncate] = my_sys_ftruncate;

	orig_sys_rename = sys_call_table[__NR_rename];
	sys_call_table[__NR_rename] = my_sys_rename;

	orig_sys_fchown = sys_call_table[__NR_fchown];
	sys_call_table[__NR_fchown] = my_sys_fchown;

	orig_sys_fchmod = sys_call_table[__NR_fchmod];
	sys_call_table[__NR_fchmod] = my_sys_fchmod;

	orig_sys_chmod = sys_call_table[__NR_chmod];
	sys_call_table[__NR_chmod] = my_sys_chmod;

	orig_sys_chown = sys_call_table[__NR_chown32];
	sys_call_table[__NR_chown32] = my_sys_chown;

	orig_sys_open = sys_call_table[__NR_open];
	sys_call_table[__NR_open] = my_sys_open;

	/*
	 * We are now initialising from blank
	 * Later take the values from a saved file
	 * and fill up our data structures
	 */
	main_obj = kmalloc(sizeof(struct head_node), GFP_KERNEL);
	if (main_obj == NULL) {
		errno = -ENOMEM;
		goto OUT;
	}

	main_obj->inode = -1;
	main_obj->next = NULL;
	main_obj->data = NULL;

	userEuid = 0;
	Block_Size = 16*1024;

OUT:
	if (errno < 0) {
		if (filePtr) {
			filp_close(filePtr, NULL);
			filePtr = NULL;
		}
	}

	return errno;
}

/*
 * Clean entries under 1 watch
 */
void cleanup_obj(struct data_node *head)
{
	struct data_node *temp1, *temp2;

	temp1 = head;
	while (temp1 != NULL) {
		temp2 = temp1;
		temp1 = temp1->next;
		kfree(temp2);
		temp2 = NULL;
	}
}

/*
 * Clean memory for all the head nodes
 */
void cleanup_head(void)
{
	struct head_node *temp1, *temp2;

	temp1 = main_obj;
	while (temp1 != NULL) {
		if (temp1->data != NULL)
			cleanup_obj(temp1->data);

		temp2 = temp1;
		temp1 = temp1->next;

		kfree(temp2);
		temp2 = NULL;
	}
}

/*	Cleanup the module
 * 	- free up the used memory
 *	- check if the currently installed system call matches with ours
 *	- if yes, then replace it else report it
 */
void cleanup_module(void)
{
	/*
	 * Free up the memory used so far
	 */
	cleanup_head();
	main_obj = NULL;

	/*
	 * Reset the system call pointers
	 */
	if (sys_call_table[__NR_utimensat] != my_sys_utimensat)
		printk(KERN_ALERT "sys_utimensat() overwritten.");
	else
		sys_call_table[__NR_utimensat] = orig_sys_utimensat;

	if (sys_call_table[__NR_utimes] != my_sys_utimes)
		printk(KERN_ALERT "sys_utimes() overwritten.");
	else
		sys_call_table[__NR_utimes] = orig_sys_utimes;

	if (sys_call_table[__NR_sys_watch] != my_sys_watch)
		printk(KERN_ALERT "sys_watch() system call overwritten.");
	else
		sys_call_table[__NR_sys_watch] = orig_sys_watch;

	if (sys_call_table[__NR_sys_get_watch] != my_get_watch)
		printk(KERN_ALERT "get_watch() system call overwritten.");
	else
		sys_call_table[__NR_sys_get_watch] = orig_get_watch;

	if (sys_call_table[__NR_unlink] != my_sys_unlink)
		printk(KERN_ALERT "sys_unlink() overwritten.");
	else
		sys_call_table[__NR_unlink] = orig_sys_unlink;

	if (sys_call_table[__NR_rmdir] != my_sys_rmdir)
		printk(KERN_ALERT "sys_rmdir() overwritten.");
	else
		sys_call_table[__NR_rmdir] = orig_sys_rmdir;

	if (orig_sys_write) {
		if ((sys_call_table[__NR_write] != my_sys_write))
			printk(KERN_ALERT "sys_write() overwritten.");
		else
			sys_call_table[__NR_write] = orig_sys_write;
	}

	if (sys_call_table[__NR_truncate] != my_sys_truncate)
		printk(KERN_ALERT "sys_truncate() overwritten.");
	else
		sys_call_table[__NR_truncate] = orig_sys_truncate;

	if (sys_call_table[__NR_ftruncate] != my_sys_ftruncate)
		printk(KERN_ALERT "sys_ftruncate() overwritten.");
	else
		sys_call_table[__NR_ftruncate] = orig_sys_ftruncate;

	if (sys_call_table[__NR_rename] != my_sys_rename)
		printk(KERN_ALERT "sys_rename() overwritten.");
	else
		sys_call_table[__NR_rename] = orig_sys_rename;

	if (sys_call_table[__NR_fchmod] != my_sys_fchmod)
		printk(KERN_ALERT "sys_fchmod() overwritten.");
	else
		sys_call_table[__NR_fchmod] = orig_sys_fchmod;

	if (sys_call_table[__NR_chmod] != my_sys_chmod)
		printk(KERN_ALERT "sys_chmod() overwritten.");
	else
		sys_call_table[__NR_chmod] = orig_sys_chmod;

	if (sys_call_table[__NR_mkdir] != my_sys_mkdir)
		printk(KERN_ALERT "sys_mkdir() overwritten.");
	else
		sys_call_table[__NR_mkdir] = orig_sys_mkdir;

	if (sys_call_table[__NR_fchown] != my_sys_fchown)
		printk(KERN_ALERT "sys_fchown() overwritten.");
	else
		sys_call_table[__NR_fchown] = orig_sys_fchown;

	if (sys_call_table[__NR_chown32] != my_sys_chown)
		printk(KERN_ALERT "sys_chown() overwritten.");
	else
		sys_call_table[__NR_chown32] = orig_sys_chown;

	if (sys_call_table[__NR_open] != my_sys_open)
		printk(KERN_ALERT "sys_open() overwritten.");
	else
		sys_call_table[__NR_open] = orig_sys_open;
/*
	if (filePtr) {
		filp_close(filePtr, NULL);
		filePtr = NULL;
	}
*/
	userEuid = 0;
}
