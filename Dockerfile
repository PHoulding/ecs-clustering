ARG BUILD_PROFILE=debug


FROM marshallasch/ns3:3.34-${BUILD_PROFILE}

LABEL org.opencontainers.version="v1.0.0"

LABEL org.opencontainers.image.authors="Patrick Houlding <phouldin@uoguelph.ca>"
LABEL org.opencontainers.image.url="https://github.com/PHoulding/ecs-clustering.git"
LABEL org.opencontainers.image.source="https://github.com/PHoulding/ecs-clustering.git"
LABEL org.opencontainers.image.vendor="Patrick Houlding"
LABEL org.opencontainers.image.title="Efficient Clustering Scheme for ns-3"
LABEL org.opencontainers.image.description="ns-3 simulation of the Efficient Clustering Scheme for Patrick Houlding's Masters research"

ENV BUILD_PROFILE=$BUILD_PROFILE

ENTRYPOINT [ "contrib/ecs-clustering/entrypoint.sh" ]

# install protobuf 
RUN apt-get update && apt-get install -y --no-install-recommends \ 
    protobuf-compiler \
    libprotobuf-dev

COPY . contrib/ecs-clustering/

RUN ./waf configure --enable-examples --enable-tests --build-profile=${BUILD_PROFILE} && ./waf build

# these two labels will change every time the container is built
# put them at the end because of layer caching
ARG VCS_REF
LABEL org.opencontainers.image.revision="${VCS_REF}"

ARG BUILD_DATE
LABEL org.opencontainers.image.created="${BUILD_DATE}"
