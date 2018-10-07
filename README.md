# Remote File Server

This project is a remote file server that is mostly transparent to the user.

This project provides an interface much like the standard file system calls to allow easy use of files
across the network. It implements 'netopen', 'netread', 'netwrite' and 'netclose' but (instead of files being modified on the client) the client sends the parameters to the file server where the actual file operations will happen. To the client, it  looks like open and netopen, read and netread, write and netwrite and close and netclose work almost identically, except my net commands are working on files on another machine.
