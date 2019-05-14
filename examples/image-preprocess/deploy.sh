#!/bin/bash
set -e 

: ${LAMBDA_FUNCTION_NAME?"Must set SSQL_STACK_NAME"}
: ${LAMBDA_CODE_S3_BUCKET?"Must set SSQL_CODE_BUCKET"}
: ${AWS_DEFAULT_REGION?"Must set AWS_DEFAULT_REGION"}

zip -r image-preprocess.zip handler.py process.py sfs PIL
aws s3 cp image-preprocess.zip s3://$LAMBDA_CODE_S3_BUCKET/image-preprocess.zip

aws lambda update-function-code \
        --function-name $LAMBDA_FUNCTION_NAME \
        --s3-bucket "$LAMBDA_CODE_S3_BUCKET" \
        --s3-key "image-preprocess.zip"

rm image-preprocess.zip
