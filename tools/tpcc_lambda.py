import base64
import boto3
import json
import sys

from multiprocessing import Pool

def invoke_tpcc_lambda(duration, logging=None):
    client = boto3.client("lambda")
    payload = json.dumps({
        "test": "tpcc",
        "duration": duration
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
    if logging:
        print("Payload")
        print(response["Payload"].read())
        print("LogResult")
        print(base64.b64decode(response["LogResult"]).decode("utf-8"))
    return success, response["Payload"].read()

def usage():
    print("Usage: tpcc_lambda.py num_invocations duration num_concurrent log_file")

def invoke(x):
    invoke_success, response_payload = invoke_tpcc_lambda(duration)
    print("finished invocation with", "SUCCESS" if invoke_success else "FAILURE")
    return invoke_success, response_payload

if __name__ == "__main__":
    if len(sys.argv) != 5:
        usage()
        sys.exit(1)
    num_invocations = int(sys.argv[1])
    duration = int(sys.argv[2])
    num_concurrent = int(sys.argv[3])
    log_file = sys.argv[4]

    p = Pool(processes=num_concurrent)

    r = p.map(invoke, range(num_invocations))

    for _, response_payload in r:
        with open(log_file, "ab") as f:
            f.write(response_payload)
            f.write("\n".encode("utf-8"))

    invoke_ct = len(r)
    success_ct = sum([x[0] for x in r])
    failure_ct = invoke_ct - success_ct

    print(success_ct, failure_ct)
