# Image Preprocessing

Image Preprocessing allows users to preprocess images in parallel.

## Metrics 

`to be a nice graph comparison`

## Installation

Install Pillow to this folder

```
pip install --target . Pillow
```

Create a lambda function with a name `sfs-image-preprocess`
Configure execution role.
Configure VPC, subnets, and security groups.
Configure memory and timeout.
Configure environment variables
        NFS4_SERVER
        LOGLEVEL

Create a s3 bucket

Upload the code with `deploy.sh`. Make sure you have the necessary permissions 
and environment variables.

In the console, change the handler function to `handler.lambda_handler`

Create a test event

Test your function

Example test json
```
{
  "input_files": [
    "/cat.jpg"
  ],
  "output_files": [
    "/c_cat.jpg"
  ]
}
```

## Usage

Run main.py
