#!/bin/bash

trap cleanup INT

cleanup() {
    echo ""
    echo "Interrupted. Killing convert process (PID: $pid)..."
    
    if [[ -n "$pid" ]]; then

        ps -o pid= --ppid "$pid" | xargs -r kill -TERM
        
        kill -TERM -"$pid" 2>/dev/null
    fi
    echo "Exiting cleanly."
    exit 0
}


while true; do

    lastfile1_line=$(ls -tri ridf/*.ridf | tail -n 1)
    
    lastfile1_name=$(echo "$lastfile1_line" | awk '{print $2}')
    
    echo "$lastfile1_name"

    ./deco.mpv_online "$lastfile1_name" root/online.root -port 8001 &
    
    pid=$!

    while true; do
        lastfile2_line=$(ls -tri ridf/*.ridf | tail -n 1)
        
        if [[ "$lastfile2_line" == "$lastfile1_line" ]]; then
            sleep 1
        else
            echo "New file detected."
            
            ps -o pid= --ppid "$pid" | xargs -r kill -TERM
            kill -TERM -"$pid" 2>/dev/null

            echo "Waiting for 30 seconds before next analysis..."
            sleep 30
            
            break
        fi
    done
done
