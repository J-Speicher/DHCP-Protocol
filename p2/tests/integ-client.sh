#!/bin/bash

EXE="../client"

function run_test {

    # parameters
    TAG=$1
    ARGS=$2

    # file paths
    OUTPUT=outputs/$TAG.txt
    DIFF=outputs/$TAG.diff
    EXPECT=expected/$TAG.txt
    VALGRND=valgrind/$TAG.txt

    # run test and compare output to the expected version
    if [[ $ARGS == *"-p"* ]] ; then
    	./server 2>/dev/null >"$OUTPUT.server" &
	    sleep 1
        $EXE $ARGS 2>/dev/null >"$OUTPUT"
	    cat "$OUTPUT.server" >> "$OUTPUT"
    else
        $EXE $ARGS 2>/dev/null >"$OUTPUT"
    fi

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

}

# initialize output folders
mkdir -p outputs
rm -f outputs/*

# run individual tests
source itests.cl.include
