# Transporter File Transfer Utility

## Base Docker Image
[Ubuntu](https://hub.docker.com/_/ubuntu) 20.04 (x64)

## Get the images from Docker Hub or build it locally
```
docker pull fullaxx/transporter-pad
docker pull fullaxx/transporter-beam
docker pull fullaxx/transporter-absorb

docker build -f Dockerfile.tpad.mk2   -t="fullaxx/transporter-pad"    github.com/Fullaxx/transporter
docker build -f Dockerfile.beam.mk2   -t="fullaxx/transporter-beam"   github.com/Fullaxx/transporter
docker build -f Dockerfile.absorb.mk2 -t="fullaxx/transporter-absorb" github.com/Fullaxx/transporter
```

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

## Usage
Start the transporter pad on a remote machine binding to 76.51.51.84:8384
```
docker run -d -p 76.51.51.84:8384:8384 -v /tpad:/tpad fullaxx/transporter-pad
```
Beam your files in /local to the remote server hosting the transporter pad
```
docker run -it -e TPAD="76.51.51.84:8384" -v /local:/beam fullaxx/transporter-beam
```
Absorb your files from the remote server to /local in random order
```
docker run -it -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local oldest first
```
docker run -it -e METHOD="oldest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local newest first
```
docker run -it -e METHOD="newest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local largest first
```
docker run -it -e METHOD="largest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local smallest first
```
docker run -it -e METHOD="smallest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```

## Known Issues
Transporter has no built-in data confidentiality or authentication. \
Do not put this on the internet without securing the network traffic. \
There are many ways to do this including a properly configured firewall and other utilities such as [Stunnel](https://www.stunnel.org/).
