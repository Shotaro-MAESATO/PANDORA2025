#!/bin/bash

while :
do
    lastfile1=(`ls -tri ridf/*.ridf | tail -n 1`)
    echo ${lastfile1[1]}

    ./ana_sakra_multi ${lastfile1[1]} rootfile/online.root -port 5931 &
    pid=$!
    echo $pid

# Compare inode to detect a new file
    while :
    do
        lastfile2=(`ls -tri ridf/*.ridf | tail -n 1`)
        if [ ${lastfile2[0]} = ${lastfile1[0]} ]; then
            sleep 1
        else
            echo "New file detected."
            kill -2 $pid
            break
        fi
    done
done
