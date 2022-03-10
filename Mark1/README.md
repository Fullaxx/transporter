# Transporter Mark 1

## Get or Build Mark 1 docker images
```
docker pull fullaxx/transporter-pad
docker pull fullaxx/transporter-beam
docker pull fullaxx/transporter-absorb

docker build -f Mark1/Dockerfile.tpad   -t="fullaxx/transporter-pad"    github.com/Fullaxx/transporter
docker build -f Mark1/Dockerfile.beam   -t="fullaxx/transporter-beam"   github.com/Fullaxx/transporter
docker build -f Mark1/Dockerfile.absorb -t="fullaxx/transporter-absorb" github.com/Fullaxx/transporter
```

## Docker Usage
Start the transporter pad on a remote machine binding to 76.51.51.84
```
docker run -d -p 76.51.51.84:8480:8480 -p 76.51.51.84:8471:8471 -p 76.51.51.84:8468:8468 -v /tpad:/tpad fullaxx/transporter-pad
```
Beam your files in /local to the remote server hosting the transporter pad
```
docker run -it --rm -e TPADHOST=76.51.51.84 -e TPUTPORT=8480 -v /local:/beam fullaxx/transporter-beam
```
Absorb your files from the remote server to /local
```
docker run -it --rm -e TPADHOST=76.51.51.84 -e TGETPORT=8471 -e TDELPORT=8468 -v /local:/absorb fullaxx/transporter-absorb
```
