#!/bin/bash

EXE="../server"

function run_test {

    # parameters
    TAG=$1
    shift
    ARGS=$1
    shift

    # file paths
    OUTPUT=outputs/$TAG.txt
    DIFF=outputs/$TAG.diff
    EXPECT=expected/$TAG.txt
    VALGRND=valgrind/$TAG.txt

    # TODO: Add a timeout parameter to server...need to sleep more than that to let server shut down each test

    # run test and compare output to the expected version
    $EXE -s 2 2>/dev/null > "$OUTPUT" &
    sleep 1
    ./client $ARGS 2>/dev/null > "$OUTPUT.client"
    for ARG in "$@" ; do
        ./client $ARG 2>/dev/null >> "$OUTPUT.client"
    done
    sleep 3

    diff -u "$OUTPUT" "$EXPECT" >"$DIFF"
    PTAG=$(printf '%-30s' "$TAG")
    if [ -s "$DIFF" ]; then

        # try alternative solution (if it exists)
        EXPECT=expected/$TAG-2.txt
        if [ -e "$EXPECT" ]; then
            diff -u "$OUTPUT" "$EXPECT" >"$DIFF"
            if [ -s "$DIFF" ]; then
                echo "$PTAG FAIL (see $DIFF for details)"
            else
                echo "$PTAG pass"
            fi
        else
            echo "$PTAG FAIL (see $DIFF for details)"
        fi
    else
        echo "$PTAG pass"
    fi

    # run valgrind
    #valgrind $EXE 2>/dev/null >"$VALGRND" &
    #sleep 1
    #./client $ARGS >/dev/null 2>&1
}

# initialize output folders
mkdir -p outputs
rm -f outputs/*

# run individual tests
source itests.serv.include
