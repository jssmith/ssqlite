#!/bin/bash

set -e

: ${SSQL_CODE_BUCKET?"Must set SSQL_CODE_BUCKET"}
: ${AWS_DEFAULT_REGION?"Must set AWS_DEFAULT_REGION"}

while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    --s3-only)
    S3_ONLY=YES
    ;;
    *)
    echo "Usage: lambdadeploy.sh [ --s3-only ]"
    exit 1
esac
shift
done

rm -rf build-dist
cp -R dist build-dist

TPCC=py-tpcc/pytpcc
cp $TPCC/tpcc.py build-dist
cp $TPCC/constants.py build-dist
cp $TPCC/tpcc.sql build-dist
mkdir build-dist/drivers
cp $TPCC/drivers/sqlitedriver.py build-dist/drivers/
cp -R $TPCC/runtime build-dist/tpccrt
cp -vR $TPCC/util build-dist

pushd .
cd build-dist
rm -f ../ssqlite-fn.zip
zip -r ../ssqlite-fn.zip ./*
popd # dist directory

echo "uploading to S3"
aws s3 cp ssqlite-fn.zip s3://$SSQL_CODE_BUCKET/ssqlite-fn.zip

echo "updating Lambda"
if [ ! $S3_ONLY ]; then
    : ${SSQL_STACK_NAME?"Must set SSQL_STACK_NAME"}
    aws lambda update-function-code \
        --function-name SQLiteDemo-$SSQL_STACK_NAME \
        --s3-bucket "$SSQL_CODE_BUCKET" \
        --s3-key "ssqlite-fn.zip"
fi
