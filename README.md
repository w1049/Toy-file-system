# A toy file system

This project is part of the OS course.

Part of the design is inspired by xv6 file system and ext2. Details in report.pdf.

This project does have a lot of bugs, especially in multi-user behaviors. For example, I (after several months) don't know what will happen if one user delete all files when the other user is viewing them. And there's a lot of room for performance optimization.