import base64
import boto3
import json
import sys

def invoke_lambda(test):
    client = boto3.client('lambda')
    payload = json.dumps({ 'test': test })

    response = client.invoke(
        FunctionName='SQLiteDemo-ssqlite-test-2',
        InvocationType='RequestResponse',
        #InvocationType='Event',
        LogType='Tail',
        Payload=payload,
    )
    status_code = response["StatusCode"]
    function_error = "FunctionError" in response
    print(response["Payload"].read())
    print(base64.b64decode(response["LogResult"]).decode("utf-8"))
    return bool(status_code == 200) and not bool(function_error)

def usage():
    print("Usage: invoke.py [ open | local | write | create | tpcc ] num_iterations")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        usage()
        sys.exit(1)
    test = sys.argv[1]
    ct = int(sys.argv[2])
    if test not in set(["open", "local", "write", "create", "tpcc"]):
        usage()
        sys.exit(1)
    success_ct = 0
    failure_ct = 0
    for _ in range(ct):
        invoke_success = invoke_lambda(test)
        if invoke_success:
            success_ct += 1
        else:
            failure_ct += 1

    print(success_ct, failure_ct)
