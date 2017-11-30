#!/bin/bash

set -e

: ${SQLITE_EXE?"Must set SQLITE_EXE"}


if [ "$#" -ne 3 ]; then
    echo "Usage: write_tests.sh vfs journal_mode"
    exit
fi

SQLITE_VFS=$1
SQLITE_FILE=$2
JOURNAL_MODE=$3

function run_test {
    INIT_SQL=$1
    echo "init with $INIT_CMD"

    PAGE_SIZE_SQL=$2

    cat $INIT_SQL test/warehouse.sql | $SQLITE_EXE -vfs $SQLITE_VFS $VFSTRACE -cmd "$INIT_CMD" $SQLITE_FILE
    cat $INIT_SQL test/item_sample.sql | $SQLITE_EXE -vfs $SQLITE_VFS $VFSTRACE -cmd "$INIT_CMD" $SQLITE_FILE
    cat $INIT_SQL test/stock_sample.sql | $SQLITE_EXE -vfs $SQLITE_VFS $VFSTRACE -cmd "$INIT_CMD" $SQLITE_FILE

    cat $INIT_SQL test/q1.sql | $SQLITE_EXE -vfs $SQLITE_VFS -line -echo $VFSTRACE -cmd "$INIT_CMD" $SQLITE_FILE
    cat $INIT_SQL test/q2.sql | $SQLITE_EXE -vfs $SQLITE_VFS -line -echo $VFSTRACE -cmd "$INIT_CMD" $SQLITE_FILE

    if [ -z $PAGE_SIZE_SQL ]; then
        echo "Skipping page size"
    else
        echo "Updating page size"
        cat $PAGE_SIZE_SQL | $SQLITE_EXE -vfs $SQLITE_VFS $VFSTRACE -cmd "$INIT_CMD" $SQLITE_FILE
    fi

    cat $INIT_SQL test/q3.sql | $SQLITE_EXE -vfs $SQLITE_VFS -line -echo $VFSTRACE -cmd "$INIT_CMD" $SQLITE_FILE
}


echo "Using database $SQLITE_FILE with vfs $SQLITE_VFS"

if [ "$JOURNAL_MODE" = "delete" ]; then
    run_test "" test/page_size_64k.sql
elif [ "$JOURNAL_MODE" = "wal" ]; then
    run_test test/wal_mode.sql ""
else
    echo "Unsupported journal mode $JOURNAL_MODE"
    exit
fi
