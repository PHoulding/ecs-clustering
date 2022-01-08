#!/bin/bash

if [[ "$1" == "bash" ]]; then
    bash --init-file <(echo "ls; pwd")
    exit 0
else
    echo "run waf and pass through all flags '$@'"
    ./waf --run "ecs-clustering-example $@"
    exit $?
fi


