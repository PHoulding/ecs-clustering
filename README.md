[![build-ns3](https://github.com/PHoulding/ecs-clustering/actions/workflows/ns3-build.yml/badge.svg)](https://github.com/MPHoulding/ecs-clustering/actions/workflows/ns3-build.yml)
![Lines of code](https://img.shields.io/tokei/lines/github/PHoulding/ecs-clustering?style=plastic)
![NS-3 version](https://img.shields.io/badge/NS--3-3.34-blueviolet?style=plastic)

# ecs-clustering
Welcome to my implementation of the Efficient Clustering Scheme for NS-3.

This is an ongoing project as part of the Gillis Lab to complete my MSc Thesis.
The hope is to implement the Efficient Clustering Scheme in order to simulate
Mobile Ad hoc Networks (MANETs) in order to better understand how density affects
these networks.


## Building the container

```bash
$ docker build \
         --build-arg VCS_REF=$(git rev-parse -q --verify HEAD) \
         --build-arg BUILD_DATE=$(date -u +"%Y-%m-%dT%H:%M:%SZ") \
         --build-arg BUILD_PROFILE=debug \
         -t ecs-clustering:latest .
```

The build variant can be configured using the `BUILD_PROFILE` `build-arg`, it can be set to either `debug`, `release`, or `optimized`.
It is set to `debug` by default.


## Running the container

For convenience this simulation experiment has also been packaged as a prebuilt docker image so that you do not need to install any of the dependencies or compile the simulator yourself.

There are two ways that the docker container can be used:

1. to run ns-3 simulations directly
2. to get a bash shell in a ns-3 installation


### Interactive bash terminal

The interactive shell will put you in the ns-3 root directory.
Depending on which docker image tag is used ns-3 can either be built in debug mode or optimized mode.
Then the ./waf command can be run manually.

```bash
docker run --rm -it ecs-clustering:optimized bash
```


### To run ns-3 simulations

If anything other than `bash` is given as the command to the Docker container then the command will
be passed to `./waf --run "ecs-clustering-example <command>"` to run a simulation using the flags.

```bash
docker run --rm ecs-clustering:optimized --printHelp
```
