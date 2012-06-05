# Compare performance of this system vs Inotify
# Get i/p from user

if [ $# -ne 1 ]
then
	echo "Enter num-of-files"
	echo "Usage: $0 <num-of-files>"
	exit 1
fi

NUM=$1

# Run Inotify
echo "Running Inotify ..."
rm -Rf inotify_test
time ./inotifyTest -n $NUM

# Run our implementation
echo "Running our implementation ..."
rmmod kWatch
insmod ../kWatch.ko

rm -Rf kwatch_test
time ./kWatchTest -n $NUM

rmmod kWatch
