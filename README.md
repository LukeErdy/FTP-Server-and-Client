# FTP-Server-and-Client
A server and client capable of transferring file data between machines on a network, written in C. Makefile builds two executables designed to be executed in a UNIX environment.

Commands:
put <pathname> (transfer a file from the client's machine to the server's machine)
get <pathname> (store a file from the server's machine on the client's machine)
rcd <pathname> (change remote directory)
show <pathname> (view a file remotely)
rls (execute ls -l remotely)
ls (execute ls -l locally)
cd <pathname> (execute cd locally)
exit (terminate the client)
