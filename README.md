# Remote File Server

This project is a remote file server that is mostly transparent to the user.

Assignment instructions:
You will be providing an interface much like the standard file system calls to allow easy use of files
across the network. You should write 'netopen', 'netread', 'netwrite' and 'netclose'. All of these calls should use the same syntax and have the same overall functionality as their local counterparts (except where expressly exempted), but they will ship their parameters your file server where the actual file operations will happen. To your client code, it will look like open and netopen, read and netread, write and netwrite and close and netclose work almost identically, except your net commands are working on files on another machine.

