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

replacing `{ MY BUCKET NAME }` with the name of your choice, e.g., `mydomain.myusername.ssql.code`.

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


If you end up editing `lambda-efs.json` while doing development you can update the stack
```
aws s3 cp lambda-efs.json s3://$SSQL_CODE_BUCKET/lambda-efs.json
aws cloudformation update-stack \
    --stack-name $SSQL_STACK_NAME \
    --parameters "ParameterKey=KeyName,ParameterValue=$EC2_KEY_PAIR" \
                 "ParameterKey=S3CodeBucket,ParameterValue=$SSQL_CODE_BUCKET"\
    --capabilities CAPABILITY_IAM \
    --template-url https://s3.amazonaws.com/$SSQL_CODE_BUCKET/lambda-efs.json
```

Later, when ready for cleanup, remove the stack

```
aws cloudformation delete-stack \
    --stack-name { MY_STACK_NAME }
```

replacing `{ MY STACK NAME }` with the name of your stack, e.g., `$SSQL_STACK_NAME`.


## Mount the EFS on the control server

Get the public IP address of the control server
```
export SSQL_CONTROL_SERVER=$(aws ec2 describe-instances \
    --filters \
    "Name=tag:aws:cloudformation:logical-id,Values=ControlServer" \
    "Name=tag:aws:cloudformation:stack-name,Values=$SSQL_STACK_NAME" \
    --query 'Reservations[*].Instances[*].[PublicIpAddress]' \
    --output=text)
```

Ssh to the instance
```
ssh -i ~/.ssh/$EC2_KEY_PAIR.pem ec2-user@$SSQL_CONTROL_SERVER
```

```
sudo mkdir /efs
```


```
export AWS_DEFAULT_REGION=$(curl http://169.254.169.254/latest/dynamic/instance-identity/document | grep region | awk -F \" '{print $4}')
```

Check to make sure that the command succeeded
```
echo $AWS_DEFAULT_REGION
```

Query the name of the file system
```
export SSQL_EFS_NAME=$(aws efs describe-file-systems \
    --query "FileSystems[?Name=='sqlite-fs-ssqlite-test-2'][FileSystemId]" \
    --output=text)
```

Check to make sure that the command succeeded
```
echo $SSQL_EFS_NAME
```

Here you should see something like `fs-d795b69e`.

Mount the EFS volume
```
sudo mount -t nfs -o nfsvers=4.1,rsize=1048576,wsize=1048576,hard,timeo=600,retrans=2 \
    $SSQL_EFS_NAME.efs.$AWS_DEFAULT_REGION.amazonaws.com:/ /efs
```


Configure permissions (TODO: not certain this is necessary)
```
sudo chown nfsnobody.nfsnobody /efs
```

## Generate a test database

Remaining on the control server

```
sudo yum update -y
sudo yum install -y git python35
git clone https://github.com/jssmith/py-tpcc
cd py-tpcc/pytpcc
python3 tpcc.py --config initialization-config --reset --no-execute sqlite
```

Note that generating a test database can be resource consuming and may deplete runtime credits of the T2 instance used as the control server. You may wish to configure this instance as [T2 Unlimited](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/t2-unlimited.html) by using AWS EC2 web console.

Copy the initial TPCC database to EFS
```
sudo -u nfsnobody -g nfsnobody cp /tmp/tpcc-initial /efs/tpcc-nfs
```


## Invoke the Lambda function

You can invoke the Lambda function either from the machine originally used for configuration, or from the control server if you have set  `$SSQL_STACK_NAME` and `$AWS_DEFAULT_REGION`.

```
aws lambda invoke \
    --invocation-type RequestResponse \
    --function-name SQLiteDemo-$SSQL_STACK_NAME \
    --payload '{}' \
    out.txt
```

view the output
```
cat out.txt
```


Building SQLite and Python
==========================

This repository includes a binary version of the SQLite library for Python (`dist/_sqlite3.so`) that encompasses SQLite as opposed to loading it from a shared library.
To build it yourself follow these [instructions](build_sqlite_python.md).
