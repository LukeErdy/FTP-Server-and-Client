all: myftp myftpserve

myftp: myftp.c
	gcc -o myftp myftp.c

myftpserve: myftpserve.c
	gcc -o myftpserve myftpserve.c