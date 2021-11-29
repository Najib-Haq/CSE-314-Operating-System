#!/bin/bash
# 1st argument is input file name, 2nd argument is working directory
# REMEMBER NO SPACE -_-
filename=$1
directory=$2

# TODO: missing input handling
# handle missing argument
if [ $# -eq 0 ]; then
    echo "Input format : 1st argument is input file name, 2nd argument is working directory"
fi

if [ $# -eq 2 ]; then
    echo "Changing current directory to $directory..."
    cd $directory
fi

# handle file exist
while true; do
    if [ -f "$filename" ]; then
        echo "$filename exists"
        break
    else
        echo "$filename does not exist"
        echo "Enter correct filename"
        read filename
    fi
done

# read file content
idx=0
# need to check if anything was read into line too (for last line read)
# while read line || [ -n "$line" ]; do
while read fileTypes[$idx]; do
    # echo "In Line : $((idx+1)) : $line"
    # echo "${fileTypes[$idx]}"
    idx=$((idx+1))
    # fileTypes[$idx]=$line
done < $filename
echo "${fileTypes[*]}" # doesnt work for me why 