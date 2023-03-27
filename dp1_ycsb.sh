#!/bin/bash

for i in 80 70 60 50 40;
do
	for j in 1 2 3 4 5;
	do
		cat ycsb/$i/$j/prof* | sort -nk2 > ycsb/$i/$j/r
		awk '{print $1}' ycsb/$i/$j/r > ycsb/$i/$j/raw
	done
	echo "ycsb/$i done"
done

