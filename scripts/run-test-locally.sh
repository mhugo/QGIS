#!/usr/bin/env bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ -z "$1" ]
then
    echo "Argument: string to pass to ctest -R"
    exit 1
fi

if [ "$2" = "gdb" ]; then
    GDB_MODE="1"
else
    GDB_MODE="0"
fi

export TRAVIS_TIMESTAMP=$(date +%s)
export TRAVIS_AVAILABLE_TIME=150 # in minutes
export TRAVIS_UPLOAD_TIME=5 # time considered to start the machine and the container (minutes)
export TRAVIS_CONFIG=linux
export TRAVIS_PULL_REQUEST=false

export TRAVIS_BUILD_DIR=$(readlink -f $(pwd)/..)
CCACHE_DIR="${CCACHE_DIR:-$HOME/.ccache}"

# for master
# TODO: deduce the docker tag from git upstream configuration ?
DOCKER_TAG=latest

echo "GDB_MODE=$GDB_MODE"
echo "TRAVIS_BUILD_DIR=$TRAVIS_BUILD_DIR"
echo "CCACHE_DIR=$CCACHE_DIR"
echo "DOCKER_TAG=$DOCKER_TAG"

# if no qgis_image exists yet
if [ -z "$(docker images | grep ^qgis_image)" ]
then
    echo "==== QGIS compilation"

    docker kill qgis_container && docker rm qgis_container

    docker run -t --name qgis_container \
	   -v ${TRAVIS_BUILD_DIR}:/root/QGIS \
	   -v ${CCACHE_DIR}:/root/.ccache \
	   --env-file ${TRAVIS_BUILD_DIR}/.ci/travis/linux/docker-variables.env \
	   qgis/qgis3-build-deps:${DOCKER_TAG} \
	   /root/QGIS/.ci/travis/linux/scripts/docker-qgis-build.sh

    docker commit qgis_container qgis_image
fi

# if no qgis_debug exists yet
if [ -z "$(docker images | grep ^qgis_debug)" ]
then
    echo "==== Install GDB in docker container"

    docker kill qgis_with_gdb && docker rm qgis_with_gdb
    docker run -it --name qgis_with_gdb qgis_image sh -c "apt -y install gdb"
    docker commit qgis_with_gdb qgis_debug
fi

# FIXME: handle tests with databases (use docker-compose)

if [ "$GDB_MODE" = "0" ]; then
    echo "==== Run test"

    docker run -it --rm \
	   -v ${TRAVIS_BUILD_DIR}:/root/QGIS \
	   -v ${CCACHE_DIR}:/root/.ccache \
	   -v /tmp:/tmp \
	   --env-file ${TRAVIS_BUILD_DIR}/.ci/travis/linux/docker-variables.env \
	   qgis_debug sh -c "sleep 4 && cd /root/QGIS/build && xvfb-run ctest -VV -R $1"

else

    echo "==== Run test in GDB"

    docker run -it --rm \
	   -v ${TRAVIS_BUILD_DIR}:/root/QGIS \
	   -v ${CCACHE_DIR}:/root/.ccache \
	   -v /tmp:/tmp \
	   --env-file ${TRAVIS_BUILD_DIR}/.ci/travis/linux/docker-variables.env \
	   qgis_debug sh -c \
	          "rm /tmp/.X99-lock && Xvfb :99 -screen 0 1024x768x24 -ac +extension GLX +render -noreset -nolisten tcp & \
       sleep 4 && cd /root/QGIS/build && LD_PRELOAD= DISPLAY=:99 gdb -ex 'set fork-follow-mode child' --args ctest -VV -R $1"
fi
