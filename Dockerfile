# split into three distinct stages:
# 1) prereqs which installs whatever is needed to build
# 2) build which builds ns3
# 3) exec which runs the built ns3. This way less needs to be built each time
#    and the size of the images are reduced:
#    https://docs.docker.com/develop/develop-images/multistage-build/
# Related: highly recommend turning on docker buildkit locally when building
# to take advantage of multistage builds even more: https://www.docker.com/blog/advanced-dockerfiles-faster-builds-and-smaller-images-using-buildkit-and-multistage-builds/
# export COMPOSE_DOCKER_CLI_BUILD=1                                               
# export DOCKER_BUILDKIT=1
ARG BUILD_PROFILE=debug
FROM marshallasch/ns3:3.34-${BUILD_PROFILE} as prereqs
ENV BUILD_PROFILE=$BUILD_PROFILE

# install protobuf
RUN apt-get update && apt-get install -y --no-install-recommends \
    protobuf-compiler \
    libprotobuf-dev

FROM prereqs as build

COPY . contrib/ecs-clustering/

RUN ./waf configure --enable-examples --enable-tests --build-profile=${BUILD_PROFILE} && ./waf build

FROM build as exec

LABEL org.opencontainers.version="v1.0.0"

LABEL org.opencontainers.image.authors="Patrick Houlding <phouldin@uoguelph.ca>"
LABEL org.opencontainers.image.url="https://github.com/PHoulding/ecs-clustering.git"
LABEL org.opencontainers.image.source="https://github.com/PHoulding/ecs-clustering.git"
LABEL org.opencontainers.image.vendor="Patrick Houlding"
LABEL org.opencontainers.image.title="Efficient Clustering Scheme for ns-3"
LABEL org.opencontainers.image.description="ns-3 simulation of the Efficient Clustering Scheme for Patrick Houlding's Masters research"

ENTRYPOINT [ "contrib/ecs-clustering/entrypoint.sh" ]

# these two labels will change every time the container is built
# put them at the end because of layer caching
ARG VCS_REF
LABEL org.opencontainers.image.revision="${VCS_REF}"

ARG BUILD_DATE
LABEL org.opencontainers.image.created="${BUILD_DATE}"
