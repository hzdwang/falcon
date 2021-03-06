#!/bin/bash

# Helper script to run CI test locally without a gitlab runner. This file should be in sync with some pipeline from .gitlab-ci.yml.

docker rmi ubnt20_full:latest --force || true
cd docker/Ubuntu

BUILD_PARAMS=(
	--no-cache
	-t ubnt20_full:latest
	--build-arg OS_VERSION=focal
	--build-arg INCLUDE_CMNALIB=true
	--build-arg INCLUDE_UHD=false
	--build-arg INCLUDE_LIMESDR=false
	--build-arg INCLUDE_SRSLTE=false
	.
)
docker build ${BUILD_PARAMS[@]}

cd ../../

RUN_PARAMS=(
	-v `pwd`/../.:/falcon
	-v `pwd`/../../../falcon-testdata:/falcon/_gitlab-ci/testdata
	-v /tmp:/tmp/tmp-host
	-i
	-t ubnt20_full:latest
)
echo ${RUN_PARAMS[@]}
docker run ${RUN_PARAMS[@]} /bin/bash -c "/falcon/_gitlab-ci/testscripts/./test_all.sh /falcon-build /tmp; bash"
