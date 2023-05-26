# Options
You can modify `CFLAGS` in `Makefile`.
  - -Wall -Werror: More warnings, and see warnings as errors
  - -fsanitize=address: Check for errors like use-after-free, segment fault, memory leak
  - -g: Enable debugging
  - -DDEBUG: Define DEBUG, the log will contains more detailed information

In step 2 and step 3, -DDEBUG is removed to get clearer logs.
# Step 1
Use the following command to build:
```
make
```

Start with
```
./disk <cylinders> <sector per cylinder> <track-to-track delay> <disk-storage filename>
```

For example, use
```
./disk 3 4 20 diskfile > a.out
```
A file named `diskfile` will be created. Then use command
```
I
R 0 0
W 0 0 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
R 0 1
R 2 0
W 1 0 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
R 0 0
R 1 0
E
```
`a.out` will be:
```
3 4
Yes 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
Yes
Yes 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
Yes 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
Yes
Yes aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
Yes ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
Goodbye!
```
`disk.log` will be:
```
[INFO] use command: I
[INFO] 3 Cylinders, 4 Sectors per cylinder
[INFO] use command: R 0 0
[INFO] Delay 0 ms, Read data: 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
[INFO] use command: W 0 0 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
[INFO] Delay 0 ms, Write successfully
[INFO] use command: R 0 1
[INFO] Delay 0 ms, Read data: 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
[INFO] use command: R 2 0
[INFO] Delay 40 ms, Read data: 00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
[INFO] use command: W 1 0 ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
[INFO] Delay 20 ms, Write successfully
[INFO] use command: R 0 0
[INFO] Delay 20 ms, Read data: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
[INFO] use command: R 1 0
[INFO] Delay 20 ms, Read data: ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
[INFO] use command: E
[INFO] Exit
```
# Step 1
```
make
./fs > a.out
```
Use commands:
```
login 1
f
mk b
mkdir a
mk c
mkdir d
mkdir e
rmdir d
rm c
ls
cd a
mkdir aa
cd aa
mk aaa
cd ../..
ls
cd /a/aa/./
ls
w aaa 13 Hello, World!
cat aaa
i aaa 5 2 XX
cat aaa
d aaa 7 3
cat aaa
cd /
ls
e
```
I use some ASCII control character, so I advise not to redirect the output to a file.
`a.out` will be:
```
Hello, uid=1!
Done
Yes
Yes
Yes
Yes
Yes
Yes
Yes
[1mType 	Owner	Update time	Size	Name[0m
drwr-	1	05-26 22:03	32	[34m[1ma[0m
drwr-	1	05-26 22:03	32	[34m[1me[0m
-rwr-	1	05-26 22:03	0	b
Yes
Yes
Yes
Yes
Yes
[1mType 	Owner	Update time	Size	Name[0m
drwr-	1	05-26 22:03	48	[34m[1ma[0m
drwr-	1	05-26 22:03	32	[34m[1me[0m
-rwr-	1	05-26 22:03	0	b
Yes
[1mType 	Owner	Update time	Size	Name[0m
-rwr-	1	05-26 22:03	0	aaa
Yes
Hello, World!

Yes
HelloXX, World!

Yes
HelloXXorld!

Yes
[1mType 	Owner	Update time	Size	Name[0m
drwr-	1	05-26 22:03	48	[34m[1ma[0m
drwr-	1	05-26 22:03	32	[34m[1me[0m
-rwr-	1	05-26 22:03	0	b
Goodbye!
```
`fs.log` will be:
```
[INFO] ncyl=1024, nsec=63
[INFO] Superblock initialized, not formatted
[INFO] size=0, nblocks=0, ninodes=0
[INFO] use command: login 1
[INFO] use command: f
[INFO] ncyl=1024 nsec=63 fsize=64512
[INFO] ninodeblocks=64512 nbitmap=290 nblocks=64222
[INFO] sb: magic=0x5346594d size=64512 nblocks=64222 ninodes=1024 inodestart=1 bmapstart=258
[INFO] Create dir inode 0, inside directory inode 0
[INFO] Success
[INFO] use command: mk b
[INFO] Create file inode 1, inside directory inode 0
[INFO] Success
[INFO] use command: mkdir a
[INFO] Create dir inode 2, inside directory inode 0
[INFO] Success
[INFO] use command: mk c
[INFO] Create file inode 3, inside directory inode 0
[INFO] Success
[INFO] use command: mkdir d
[INFO] Create dir inode 4, inside directory inode 0
[INFO] Success
[INFO] use command: mkdir e
[INFO] Create dir inode 5, inside directory inode 0
[INFO] Success
[INFO] use command: rmdir d
[INFO] Success
[INFO] use command: rm c
[INFO] Success
[INFO] use command: ls
[INFO] List files
Type 	Owner	Update time	Size	Name
drwr-	1	05-26 22:03	32	a
drwr-	1	05-26 22:03	32	e
-rwr-	1	05-26 22:03	0	b

[INFO] use command: cd a
[INFO] Success
[INFO] use command: mkdir aa
[INFO] Create dir inode 6, inside directory inode 2
[INFO] Success
[INFO] use command: cd aa
[INFO] Success
[INFO] use command: mk aaa
[INFO] Create file inode 7, inside directory inode 6
[INFO] Success
[INFO] use command: cd ../..
[INFO] Success
[INFO] use command: ls
[INFO] List files
Type 	Owner	Update time	Size	Name
drwr-	1	05-26 22:03	48	a
drwr-	1	05-26 22:03	32	e
-rwr-	1	05-26 22:03	0	b

[INFO] use command: cd /a/aa/./
[INFO] Success
[INFO] use command: ls
[INFO] List files
Type 	Owner	Update time	Size	Name
-rwr-	1	05-26 22:03	0	aaa

[INFO] use command: w aaa 13 Hello, World!
[INFO] Success
[INFO] use command: cat aaa
[INFO] use command: i aaa 5 2 XX
[INFO] Success
[INFO] use command: cat aaa
[INFO] use command: d aaa 7 3
[INFO] Success
[INFO] use command: cat aaa
[INFO] use command: cd /
[INFO] Success
[INFO] use command: ls
[INFO] List files
Type 	Owner	Update time	Size	Name
drwr-	1	05-26 22:03	48	a
drwr-	1	05-26 22:03	32	e
-rwr-	1	05-26 22:03	0	b

[INFO] use command: e
[INFO] Exit
```
# Step 3
```
make
./disk 1024 63 5 diskfile 1234
./fs 1234 12345
```
Then you can start many clients:
```
./client 12345
```
By default, a user can only read files from other users and cannot write to them. You can try it by yourself.

In step3, all logs are printed in the shell.
