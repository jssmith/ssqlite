import base64
import boto3
import json
import sys

from multiprocessing import Pool

import os

def invoke_tpcc_lambda(duration, frac_read, logging=None):
    client = boto3.client("lambda")
    payload = json.dumps({
        "test": "tpcc",
        "duration": duration,
        "frac_read": frac_read
    })

    response = client.invoke(
        FunctionName='SQLiteDemo-ssqlite-test-2',
        InvocationType='RequestResponse',
        #InvocationType='Event',
        LogType='Tail' if logging else 'None',
        Payload=payload,
    )
    status_code = response["StatusCode"]
    function_error = "FunctionError" in response
    success = bool(status_code == 200) and not bool(function_error)
    response_payload = response["Payload"].read()
    if logging:
        print("Payload")
        print(response_payload)
        print("LogResult")
        print(base64.b64decode(response["LogResult"]).decode("utf-8"))
    return success, response["Payload"].read()

def usage():
    print("Usage: tpcc_lambda.py num_invocations duration num_concurrent frac_read log_file")

def invoke(args):
    duration, frac_read = args
    print(duration)
    print(frac_read)
    invoke_success, response_payload = invoke_tpcc_lambda(duration, frac_read)
    print("finished invocation with", "SUCCESS" if invoke_success else "FAILURE")
    return invoke_success, response_payload

if __name__ == "__main__":
    if len(sys.argv) != 6:
        usage()
        sys.exit(1)
    num_invocations = int(sys.argv[1])
    duration = int(sys.argv[2])
    num_concurrent = int(sys.argv[3])
    frac_read = float(sys.argv[4])
    log_file = sys.argv[5]

    if "AWS_DEFAULT_REGION" not in os.environ:
        print("Must set AWS_DEFAULT_REGION")
        sys.exit(1)

    p = Pool(processes=num_concurrent)

    r = p.map(invoke, [(duration, frac_read)] * num_invocations)

    for _, response_payload in r:
        with open(log_file, "ab") as f:
            f.write(response_payload)
            f.write("\n".encode("utf-8"))

    invoke_ct = len(r)
    success_ct = sum([x[0] for x in r])
    failure_ct = invoke_ct - success_ct

    print(success_ct, failure_ct)
