#!/bin/bash
count=100
visualize=0
list="list.txt"

while read -r line
do
    domain=$(echo "$line" | awk -F/ '{print $3}' | sed 's/\./ /g' | awk '{print $2}')
    echo "Collecting $domain ($line)"
    ./collect.sh "$line" "$domain" $count $visualize
done < "$list"
