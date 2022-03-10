# Transporter File Transfer Utility

## Base Docker Image
[Ubuntu](https://hub.docker.com/_/ubuntu) 20.04 (x64)

## About this Image
Transporter is very simple [ZeroMQ](https://zeromq.org/) based file transfer utility.
It is primarily a proof-of-concept and a way to learn the ZMQ API.
The goal of the transporter is to move your files automatically from one machine to another, efficiently and correctly.

## Efficient Integrity Checking
During the transfer, as the data is being read/written from/to the disk, a rolling hash digest is being calculated by [libgcrypt](https://gnupg.org/software/libgcrypt/index.html).
Once the transfer is complete, the digest is finalized and hash values are compared to validate successful transfer before removing the source file.
This is much more efficient than hashing the file after transfer is complete, since we only have to read/write the file once.

## Engineering
The transporter pad is a server with a listening socket that will handle connections from beam and absorb. \
Beam will connect to tpad and relocate files from your local machine to the remote server. \
Absorb will connect to tpad and relocate files from the remote server to your local machine.

## Mark 1 vs Mark 2
Mark 1 was create as a proof of concept and has a very simple code base. \
It is bandwidth efficient, but it does have some limitations:
* 3 server ports are required for operation
* There is a 2GB file limit on any transfer
Mark 2 was created to address these limitations, but it does appear less bandwidth efficient during transfer. \
See the instructions for [Mark 1](https://github.com/Fullaxx/transporter/tree/master/Mark1) and [Mark 2](https://github.com/Fullaxx/transporter/tree/master/Mark2)

## Known Issues
Transporter has no built-in data confidentiality or authentication. \
Do not put this on the internet without securing the network traffic. \
There are many ways to do this including a properly configured firewall and other utilities such as [stunnel](https://www.stunnel.org/).
