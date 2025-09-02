#!/bin/sh

filesdir=$1
searchstr=$2

if [ -z "$filesdir" ] || [ -z "$searchstr" ]; then #parameters not specified
    echo "Error: One or both parameters not specified"
    exit 1
fi

if [ -d "$filesdir" ]; then # check if path is a dir 
    num_files=$(find "$filesdir" -name '*.txt' | wc -l) #wc -l stands for word count -lines
    num_matching_lines=$(grep -r "$searchstr" "$filesdir" | wc -l)
    echo "The number of files are $num_files and the number of matching lines are $num_matching_lines"
    else
    echo "Error: $filesdir does not represent a directory on the system"
    exit 1
fi