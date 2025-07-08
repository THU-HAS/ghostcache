#!/bin/bash

function usage {
    echo "collect.sh <url> <label> <count> <visualize>"
    exit 1
}

if [ "$1" == "" ];
then
    usage
fi
if [ "$2" == "" ];
then
    usage
fi
if [ "$3" == "" ];
then
    usage
fi
if [ "$4" == "" ];
then
    usage
fi

for i in $(seq $3);
do
    mkdir -p data/$2
    taskset -c 0 ./out/fullset_web_fp_fullway $1 $2
    mv data/result_$2.txt data/$2/$2_$i.txt
    if [ "$4" == "1" ];
    then
        python3 visualize.py -f data/$2/$2_$i.txt
    fi
done

