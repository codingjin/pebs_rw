#!/bin/bash

for i in 80 70 60 50 40;
do
	for j in 1 2 3 4 5;
	do
		sort -n ycsb/$i/$j/raw | uniq -c | sort -nk1 > ycsb/$i/$j/uniq
		awk '{print $1}' ycsb/$i/$j/uniq | uniq -c | awk '{print $2"\t"$1}' > ycsb/$i/$j/dist
	done
	echo "ycsb/$i done"
	echo ""
done

