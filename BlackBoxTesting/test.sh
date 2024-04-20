#!/bin/sh
# This is a comment

rm *.txt
make clean
make

./fsa ext2_10mb.img -traverse | sort > sout_ext2_10mb_traverse.txt

if diff -w sout_ext2_10mb_traverse.txt results/correct_ext2_10mb_traverse.txt; then
    echo Test 1 - Success--------------------EXT2_10MB-TRAVERSE-----------------Success
else
    echo Test 1 - Fail-----------------------EXT2_10MB-TRAVERSE-----------------Fail
fi

./fsa ext2_100mb.img -traverse | sort > sout_ext2_100mb_traverse.txt

if diff -w sout_ext2_100mb_traverse.txt results/correct_ext2_100mb_traverse.txt; then
    echo Test 2 - Success--------------------EXT2_100MB-TRAVERSE----------------Success
else
    echo Test 2 - Fail-----------------------EXT2_100MB-TRAVERSE----------------Fail
fi

./fsa ext2_10mb.img -file /searchdir/B/file3.txt > sout_ext2_10mb_file3.txt

if diff -w sout_ext2_10mb_file3.txt results/correct_ext2_10mb_file3.txt; then
    echo Test 3 - Success--------------------EXT2_10MB-FILE-3-------------------Success
else
    echo Test 3 - Fail-----------------------EXT2_10MB-FILE-3-------------------Fail
fi

./fsa ext2_100mb.img -file /poems/alone/AlonePoemsAlonePoembyEdgarAllanPoe.txt > sout_ext2_100mb_poe.txt

if diff -w sout_ext2_100mb_poe.txt results/correct_ext2_100mb_poe.txt; then
    echo Test 4 - Success--------------------EXT2_100MB-FILE-POE----------------Success
else
    echo Test 4 - Fail-----------------------EXT2_100MB-FILE-POE----------------Fail
fi

./fsa ext2_100mb.img -file /poems/house/HousePoemsTheHouseOfFamePoembyGeoffreyChaucer.txt > sout_ext2_100mb_chaucer.txt

if diff -w sout_ext2_100mb_chaucer.txt results/correct_ext2_100mb_chaucer.txt; then
    echo Test 5 - Success--------------------EXT2_100MB-FILE-CHAUCER------------Success
else
    echo Test 5 - Fail-----------------------EXT2_100MB-FILE-CHAUCER------------Fail
fi
