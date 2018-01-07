import json
import sys
import numpy as np

# Analysis of vfsbench output

def summary(data):
    res = {}
    config = data["experiment_spec"]
    for k in ["mode", "block_size", "blocks_per_thread", "num_threads"]:
        config[k] = data[k]
    res["config"] = config
    # if "detail" in data:
    start_time = min([x["thread_start_time"] for x in data["results"]])
    end_time = max([x["thread_end_time"] for x in data["results"]])
    if (start_time > end_time):
        print("error - possible counter wraparound")
        sys.exit(1)
    res["start_time"] = start_time
    res["end_time"] = end_time
    num_operations = config["num_threads"] * config["blocks_per_thread"]
    res["operations_per_sec"] = num_operations / (end_time - start_time)
    res["throughput"] = res["operations_per_sec"] * config["block_size"]
    if "op_detail" in data["results"][0]:
        latencies = [x["end_time"] - x["start_time"] for thread_res in data["results"] for x in thread_res["op_detail"] ]
        res["avg_latency"] = np.mean(latencies)
        percentiles = [0.1, 1, 10, 20, 50., 80., 90., 95., 99., 99.9]
        res["latency_percentiles"] = dict([("%06.2f"%x[0], x[1]) for x in zip(percentiles, np.percentile(latencies, percentiles))])
    return res


if __name__ == "__main__":
    file_name = sys.argv[1]
    with open(file_name) as f:
        # for data in [json.loads(line) for line in f.readlines()]:
        #     print(summary(data))
        summaries = [summary(json.loads(line)) for line in f.readlines()]
        t = []
        for s in summaries:
            storage_location = s["config"]["storage_location"]
            num_threads = s["config"]["num_threads"]
            block_size = s["config"]["block_size"]
            mode = s["config"]["mode"]
            throughput = s["throughput"]
            avg_latency = s["avg_latency"] if "avg_latency" in s else None
            median_latency = s["latency_percentiles"]["050.00"] if "latency_percentiles" in s else None
            t.append([mode, storage_location, block_size, num_threads, throughput, avg_latency, median_latency])
        t = sorted(t)
        for r in t:
            print(r)

