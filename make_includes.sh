for i in $(grep "#include" main/*.c | sed 's/#include //' | sed 's/"//g')
do
        b=$(basename $i)
	for d in $(pwd)/components $(pwd)/managed_components $IDF_PATH/components; do
		f=""
		f=$(find $d -type f -name $b 2> /dev/null)
		if [ "X$f" != "X" ]; then
			echo $(echo $f | sed 's#.*\/\([^/]*\/.*components\/[^/]*\)\/.*#\1#') $i
		fi
	done
done | sort | awk '$1!=p{p=$1;print "\n// "$1} {print "#include \""$2"\""}'
