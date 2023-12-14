# FTP-Server-and-Client
A server and client capable of transferring file data between machines on a network, written in C. Makefile builds two executables designed to be executed in a UNIX environment.

Commands: <br />
put <pathname> (transfer a file from the client's machine to the server's machine) <br />
get <pathname> (store a file from the server's machine on the client's machine) <br />
rcd <pathname> (change remote directory) <br />
show <pathname> (view a file remotely) <br />
rls (execute ls -l remotely) <br />
ls (execute ls -l locally) <br />
cd <pathname> (execute cd locally) <br />
exit (terminate the client)
