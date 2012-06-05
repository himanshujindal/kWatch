#include <linux/linkage.h>
asmlinkage int sys_get_watch(const char * const file, 
		void *user_buf, int buf_len) 
{
	return 0;
}
asmlinkage int sys_watch(const char * const file, 
		int option) 
{
	return 0;
}
