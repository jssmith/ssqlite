import json
import sys

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: tpcc_lambda_rates input_file")
    input_file = sys.argv[1]
    with open(input_file) as f:
        events = list([x for l in f.readlines() for x in json.loads(l)["Result"]["TxnsDetail"]])
    min_start = min([x["start_time"] for x in events])
    max_end = max([x["end_time"] for x in events])
    rate = float(len(events)) / (max_end - min_start)
    print("overall rate: %f" % rate)
            # print(json.loads(l))
