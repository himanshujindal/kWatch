#test for creation and deletion of directories recursively
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
echo "Test for creation and deletion of directories inside watched directory"
echo "Created 3 directories and set watch on it: SUCCESSFULLY"
#create dirs in 3 directories
cd dir1
mkdir dir11
mkdir dir12
mkdir dir13
cd dir11
mkdir dir11
mkdir dir12
mkdir dir13
cd ../dir12
mkdir dir11
mkdir dir12
mkdir dir13
cd ../dir13
mkdir dir11
mkdir dir12
mkdir dir13
cd ..
cd ../dir2
mkdir dir11
mkdir dir12
mkdir dir13
cd dir11
mkdir dir11
mkdir dir12
mkdir dir13
cd ../dir12
mkdir dir11
mkdir dir12
mkdir dir13
cd ../dir13
mkdir dir11
mkdir dir12
mkdir dir13
cd ..
cd ../dir3
mkdir dir11
mkdir dir12
mkdir dir13
cd dir11
mkdir dir11
mkdir dir12
mkdir dir13
cd ../dir12
mkdir dir11
mkdir dir12
mkdir dir13
cd ../dir13
mkdir dir11
mkdir dir12
mkdir dir13
cd ../..
echo "Created 2 levels of directories in all 3 directories under watch"
.././uWatch -c
.././uWatch -l
.././uWatch -g dir1
.././uWatch -g dir2
.././uWatch -g dir3
#Now delete all the files
cd dir1
rm -Rf *
cd ../dir2
rm -Rf *
cd ../dir3
rm -Rf *
echo "Deleted files in all 3 directories under watch"
cd ..
.././uWatch -c
.././uWatch -l
.././uWatch -g dir1
.././uWatch -g dir2
.././uWatch -g dir3
rmmod kWatch.ko
