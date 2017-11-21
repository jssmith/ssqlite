#!/bin/bash

set -e

while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    --s3-only)
    BUILD_ONLY=YES
    ;;
    *)
    echo "Usage: lambdadeploy.sh [ --s3-only ]"
    exit 1
esac
shift
done

pushd .
cd dist
rm -f ../ssqlite-fn.zip.zip
zip -r ../ssqlite-fn.zip ./*
popd # dist directory

aws s3 cp ssqlite-fn.zip s3://$SSQL_CODE_BUCKET/ssqlite-fn.zip

if [ ! $BUILD_ONLY ]; then
    aws lambda update-function-code \
        --function-name SQLiteDemo \
        --s3-bucket "$SSQL_CODE_BUCKET" \
        --s3-key "ssqlite-fn.zip"
fi
