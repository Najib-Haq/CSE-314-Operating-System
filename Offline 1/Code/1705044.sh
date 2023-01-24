#!/bin/bash
# 1st argument is input file name, 2nd argument is working directory
# REMEMBER NO SPACE -_-
filename="$1"
directory="$2"
output_dir="output_dir"

ALL_FILES=()
NOT_TAKEN=()

# 1st argument is directory, 2nd argument is file extensions to ignore
get_files() {
    # echo "here : $1"
    for file in "$1"/*; do
        # echo "file : $file"
        if [ -f "$file" ]; then
            # echo "file $file"
            # https://stackoverflow.com/questions/9587725/how-do-i-iterate-over-each-line-in-a-file-with-bash
            # "file" to handle white space in names
            take_file=$(find "$file" -regextype posix-egrep -not -regex ".*/*.($2)$")
            donot_take_file=$(find "$file" -regextype posix-egrep -regex ".*/*.($2)$")
            # handle no result
            if [ ${#take_file} != 0 ]; then
                ALL_FILES+=("$take_file")
            fi
            if [ ${#donot_take_file} != 0 ]; then
                NOT_TAKEN+=("$donot_take_file")
            fi
        elif [ -d "$file" ]; then
            # echo "folder $file"
            get_files "$file" "$2"
        fi
    done
}

# 1st argument is directory name, 2nd argument is filename
move_files() {
    file=$2
    # transfer file
    # -p : no error if existing, make parent directories as needed
    mkdir -p "$output_dir/$1"
    cp "$file" "$output_dir/$1"

    # create description txt if doesnt exist
    path_file=$output_dir/$1/desc_$1.txt
    if [ ! -e $path_file ]; then
        touch $path_file
    fi
    # write to file
    echo "$file" >> $path_file
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
            files_in_dir=($(find $folder/ -type f -not -name "*.*"))
        else
            files_in_dir=($(find $folder/ -type f -name "*.$type"))
        fi
        unset IFS
        length=${#files_in_dir[@]}
        # handle .txt type
        if [ $type = "txt" ]; then
            ((length-=1))
        fi
        # length=$(wc -l < $output_dir/$type/desc_$type.txt)
        echo "$type $length"
        echo "$type,$length" >> output.csv
    done

    echo "ignored,${#NOT_TAKEN[@]}" >> output.csv
}

# handle missing argument
if [ $# -eq 0 ]; then
    echo "Input format : 1st argument is input file name, 2nd argument is working directory"
    exit
# handle directory name
elif [ $# -eq 1 ]; then
    directory="."
fi


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

# read file 
dont_match_types=$(tr -s '\r\n' '|' < "$filename")
# handle empty new line? - done
if [ ${dont_match_types: -1} = "|" ]; then
    dont_match_types="${dont_match_types%|}"
fi
echo "DONOT MATCH TYPES: $dont_match_types"

# get files and folders inside current directory
get_files "$directory" "$dont_match_types"
echo "ALL FILES FOUND: ${ALL_FILES[*]}, ${#ALL_FILES[@]}"

## create output_dir
rm -r $output_dir # for testing
mkdir $output_dir

for ((i = 0; i < ${#ALL_FILES[@]}; i++)); do
    file=${ALL_FILES[$i]} #$(echo "${ALL_FILES[$i]}"| cut -d $"./" -f 1)

    name=${file##*/} # get last '/' part
    type=${name##*.} # get extension
    # echo "$name $type"

    # if name and type same, that means no type found
    if [ "$name" = "$type" ]; then
        move_files others "$file"
    else
        move_files "$type" "$file"
    fi
done

create_csv
