#!/bin/bash
# 1st argument is input file name, 2nd argument is working directory
# REMEMBER NO SPACE -_-
filename=$1
directory=$2
output_dir="output_dir"

ALL_FILES=()
NOT_TAKEN=()

# 1st argument is directory, 2nd argument is file extensions to ignore
get_files() {
    for file in "$1"/*; do
        if [ -f "$file" ]; then
            # echo "file $file"
            # https://stackoverflow.com/questions/9587725/how-do-i-iterate-over-each-line-in-a-file-with-bash
            take_file=$(find $file -regextype posix-egrep -not -regex ".*/*.($2)$")
            donot_take_file=$(find $file -regextype posix-egrep -regex ".*/*.($2)$")
            # echo "TAKE FILE : $take_file"
            ALL_FILES+=($take_file)
            NOT_TAKEN+=($donot_take_file)
        elif [ -d "$file" ]; then
            # echo "folder $file"
            get_files "$file" $2
        fi
    done
}

# 1st argument is directory name, 2nd argument is filename
move_files() {
    # transfer file
    mkdir -p $output_dir/$1
    cp $2 $output_dir/$1

    # create description txt if doesnt exist
    path_file=$output_dir/$1/desc_$1.txt
    if [ ! -e $path_file ]; then
        touch $path_file
    fi
    # write to file
    echo "$2" >> $path_file
}

# create csv
create_csv() {
    # create csv file
    if [ -e output.csv ]; then
        rm output.csv
    fi
    touch output.csv
    echo "file_type,no_of_files" >> output.csv

    # CHANGE THIS. OUTPUT is different
    for folder in $output_dir/*; do
        type=${folder##*/}
        IFS=$'\n' # helps create the array according to newlines from find
        if [ $type = "others" ]; then
            data=($(find $folder/ -type f -not -name "*.*"))
        else
            data=($(find $folder/ -type f -name "*.$type"))
        fi
        unset IFS
        echo "$type ${#data[@]}"
        echo "$type,${#data[@]}" >> output.csv
    done

    echo "ignored,${#NOT_TAKEN[@]}" >> output.csv
}


# TODO: missing input handling
# handle missing argument
if [ $# -eq 0 ]; then
    echo "Input format : 1st argument is input file name, 2nd argument is working directory"
fi

# if [ $# -eq 2 ]; then
#     echo "Changing current directory to $directory..."
#     cd $directory
# fi

# handle file exist
while true; do
    if [ -f "$filename" ]; then
        echo "$filename exists"
        break
    else
        echo "$filename does not exist"
        echo "Enter correct filename"
        read -r filename
    fi
done

# read file content
idx=1
# need to check if anything was read into line too (for last line read)
# while read line || [ -n "$line" ]; do
while read -r fileTypes[$idx]; do
    # echo "In Line : $((idx+1)) : $line"
    # echo "${fileTypes[$idx]}"
    idx=$((idx+1))
    # fileTypes[$idx]=$line
done < "$filename"
# echo "${fileTypes[*]}" # doesnt work for me why 

dont_match_types=$(tr -s '\r\n' '|' < $filename)
echo "DONOT MATCH TYPES: $dont_match_types"


# get files and folders inside current directory
get_files "$directory" "$dont_match_types"
echo "ALL FILES: ${ALL_FILES[*]}, ${#ALL_FILES[@]}"


## create output_dir
rm -r $output_dir # for testing
mkdir $output_dir

for file in "${ALL_FILES[@]}"; do
    IFS='.'
    read -a fileNameArray <<< "$file"
    unset IFS
    if [ "${#fileNameArray[@]}" == 1 ]; then
        move_files others $file
    else
        # transfer file
        move_files ${fileNameArray[1]} $file
    fi
    echo "${fileNameArray[@]}"
done

create_csv
