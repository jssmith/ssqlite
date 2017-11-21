Serverless SQLite Demo Code
===========================

## Environment setup

```
export AWS_DEFAULT_REGION=us-west-2
```

Please note that EFS is not supported in all regions. Check the [Region Table](https://aws.amazon.com/about-aws/global-infrastructure/regional-product-services/) for service availability.
The CloudFormation template used here presently supports the `us-west-2` and `us-east-1` regions.

Optionally set an environment variable for access credentials
```
export AWS_DEFAULT_PROFILE={ MY PROFILE }
```
replacing `{ MY PROFILE }` with the name of a profile in your `~/.aws/credentials` file.


```
export EC2_KEY_PAIR={ MY EC2 KEY }
```

replacing `{ MY EC2 KEY }` with the name of an EC2 key used for ssh access. See [EC2 Key Pair documentation](http://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-key-pairs.html).
This should be a `KeyName` as returned by `aws ec2 describe-key-pairs`.

Select a location for your code on S3

```
export SSQL_CODE_BUCKET= { MY BUCKET NAME }
```

replacing `{ MY BUCKET NAME }` with the name of your choice, e.g., `myusername.ssql.code`.

Set a name for the CloudFormation stack

```
export SSQL_STACK_NAME={ MY STACK NAME }
```

replacing `{ MY STACK NAME }` with a chosen name, e.g., `ssqlite-test`.

## Create the S3 bucket

```
aws s3 mb s3://$SSQL_CODE_BUCKET
```


## Deploy using CloudFormation

Copy the the configuration json file to S3:

```
aws s3 cp lambda-efs.json s3://$SSQL_CODE_BUCKET/lambda-efs.json
```


Build and package the code, upload it to S3:
```
./tools/lambdadeploy.sh --s3-only
```


Create the stack

```
aws cloudformation create-stack \
    --stack-name $SSQL_STACK_NAME \
    --parameters "ParameterKey=KeyName,ParameterValue=$EC2_KEY_PAIR" \
                 "ParameterKey=S3CodeBucket,ParameterValue=$SSQL_CODE_BUCKET"\
    --capabilities CAPABILITY_IAM \
    --template-url https://s3.amazonaws.com/$SSQL_CODE_BUCKET/lambda-efs.json
```

Check the status of stack creation

```
aws cloudformation describe-stacks \
    --stack-name $SSQL_STACK_NAME
```


Later, when ready for cleanup, remove the stack

```
aws cloudformation delete-stack \
    --stack-name { MY_STACK_NAME }
```

replacing `{ MY STACK NAME }` with the name of your stack, e.g., `$SSQL_STACK_NAME`.

## Invoke the Lambda function

```
aws lambda invoke \
    --invocation-type RequestResponse \
    --function-name SQLiteDemo-$SSQL_STACK_NAME \
    --payload '{}' \
    out.txt
```


## Mount the EFS on the control server

Ssh to the instance
```
ssh -i ~/.ssh/$EC2_KEY_PAIR.pem ec2-user@{ EC2 INSTANCE IP }
```

replacing `{ EC2 INSTANCE IP }` with the public IP address of the instance, as found in the control panel (TODO: improve this).

```
sudo mkdir /efs
sudo chown ec2-user.ec2-user /efs
```

TODO: Need to get the name of the EFS onto the EC2 instance, for now need to
look it up in the AWS console

```
sudo mount -t nfs -o nfsvers=4.1,rsize=1048576,wsize=1048576,hard,timeo=600,retrans=2 \
    { EFS NAME }.efs.$AWS_DEFAULT_REGION.amazonaws.com:/ /efs
```

replacing `{ EFS NAME }` with the file system ID of the EFS volume as found in the AWS console (it should look something like `fs-d795b69e`).
