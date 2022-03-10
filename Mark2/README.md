# Transporter Mark 2

## Build Mark 2 docker images
```
docker build -f Mark2/Dockerfile.tpad   -t="fullaxx/transporter-pad"    github.com/Fullaxx/transporter
docker build -f Mark2/Dockerfile.beam   -t="fullaxx/transporter-beam"   github.com/Fullaxx/transporter
docker build -f Mark2/Dockerfile.absorb -t="fullaxx/transporter-absorb" github.com/Fullaxx/transporter
```

## Docker Usage
Start the transporter pad on a remote machine binding to 76.51.51.84:8384
```
docker run -d -p 76.51.51.84:8384:8384 -v /tpad:/tpad fullaxx/transporter-pad
```
Beam your files in /local to the remote server hosting the transporter pad
```
docker run -it --rm -e TPAD="76.51.51.84:8384" -v /local:/beam fullaxx/transporter-beam
```
Absorb your files from the remote server to /local in random order
```
docker run -it --rm -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local oldest first
```
docker run -it --rm -e METHOD="oldest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local newest first
```
docker run -it --rm -e METHOD="newest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local largest first
```
docker run -it --rm -e METHOD="largest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
Absorb your files from the remote server to /local smallest first
```
docker run -it --rm -e METHOD="smallest" -e TPAD="76.51.51.84:8384" -v /local:/absorb fullaxx/transporter-absorb
```
