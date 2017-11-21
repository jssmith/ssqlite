Serverless SQLite Demo Code
========================

## Environment setup

```
export AWS_DEFAULT_REGION=us-west-2
```

Optionally
```
export AWS_DEFAULT_PROFILE=[MY_PROFILE]
```
Replacing `[MY_PROFILE]` with the name of a profile in your `~/.aws/credentials` file.

Select a location for your code on S3

```
export SSQL_CODE_BUCKET=[MY_BUCKET_NAME]
```

Replacing `[MY_BUCKET_NAME]` with the name of your choice.

## Create the S3 bucket

```
aws s3 mb s3://$SSQL_CODE_BUCKET
```

## Deploy using CloudFormation

Copy the the configuration json file to S3:

```
aws s3 cp lambda-efs.json s3://$SSQL_CODE_BUCKET/lambda-efs.json
```



Create the stack

```
aws cloudformation create-stack \
    --stack-name ssqlite-test \
    --parameters "ParameterKey=KeyName,ParameterValue=serverless" \
                 "ParameterKey=S3CodeBucket,ParameterValue=$SSQL_CODE_BUCKET"\
    --template-url https://s3-us-west-2.amazonaws.com/$SSQL_CODE_BUCKET/lambda-efs.json
```

Check the status of stack creation

```
aws cloudformation describe-stacks \
    --stack-name ssqlite-test
```


Remove a stack

```
aws cloudformation delete-stack \
    --stack-name ssqlite-test
```

## Invoke the Lambda function

```
aws lambda invoke \
    --invocation-type RequestResponse \
    --function-name SQLiteDemo \
    --payload '{}' \
    out.txt
```


## mount the efs on the control server
sudo mount -t nfs -o nfsvers=4.1,rsize=1048576,wsize=1048576,hard,timeo=600,retrans=2 fs-7cab1bd5.efs.us-west-2.amazonaws.com:/ /efs


