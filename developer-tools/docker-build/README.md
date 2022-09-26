# Docker image to compile the Munt binaries

The build is for test and development purposes, the official binaries are build with guix and not this image.

This image closely matches the [Travis build](https://travis-ci.org/Munt/munt-official/)

## Using this image

Pull the Munt repository and run this image on it, like so:

```
git clone https://github.com/Munt/munt-official
cd munt-official
docker run -it --mount type=bind,src=$PWD,dst=/work -w /work robobit/nlg-build
```

This will pull the image from Docker hub, if you rather build your own:
```
git clone https://github.com/Munt/munt-official
cd munt-official/developer-tools/docker-build
docker build -t nlg-build .
docker run -it --mount type=bind,src=$PWD,dst=/work -w /work nlg-build
```
