#script to check changing metadata (filestamps owner etc) of file
#cleanup pre existing folders
rm -Rf dir1
rm -Rf dir2
rm -Rf dir3
mkdir dir1
mkdir dir2
mkdir dir3
rmmod kWatch
insmod ../kWatch.ko
.././uWatch -s dir1
.././uWatch -s dir2
.././uWatch -s dir3
echo "Test for changing metadata (timestamp, owner etc) of files inside watched directory"
echo "Created 3 directories and set watch on it: SUCCESSFULLY"
#create files in 3 directories
cd dir1
touch file11
touch file12
touch file13
touch file14
touch file15
touch file16
touch file17
touch file18
touch file19
cd ../dir2
touch file11
touch file12
touch file13
touch file14
touch file15
touch file16
touch file17
touch file18
touch file19
cd ../dir3
touch file11
touch file12
touch file13
touch file14
touch file15
touch file16
touch file17
touch file18
touch file19
cd ..
echo "Created files in all 3 directories under watch"
echo "Will flush changes now"
.././uWatch -f dir1
.././uWatch -f dir2
.././uWatch -f dir3
useradd dummy1
cd dir1
chown dummy1 file12
chown dummy1 file14
chown dummy1 file16
chown dummy1 file18
cd ../dir2
chown dummy1 file12
chown dummy1 file14
chown dummy1 file16
chown dummy1 file18
cd ../dir3
chown dummy1 file12
chown dummy1 file14
chown dummy1 file16
chown dummy1 file18
cd ..
.././uWatch -c
.././uWatch -l
.././uWatch -g dir1
.././uWatch -g dir2
.././uWatch -g dir3
rmmod kWatch.ko
