import json
import re
import sys

# Format results of paramsweep.py for viewing or further analysis

def decode_database(database):
    e = re.compile("^(.+)[:/]")
    m = e.search(database)
    if not m:
        return "???"
    else:
        return m.group(1)

def load_file(filename):
    with open(filename, "r") as f:
        data = json.load(f)
    ee = re.compile("TOTAL\s+([0-9]+)\s+([0-9\.]+)\s+([0-9\.]+)\s+txn/s")
    res_table = []
    for res in data:
        config = res["config"]
        output_text = res["results"][0]
        errors = False if output_text.find("Failed to execute Transaction") == -1 else True
        m = ee.search(output_text)
        if not m:
            print("MATCH FAILURE")
        else:
            executed = int(m.group(1))
            time_us = float(m.group(2))
            rate = float(m.group(3))
            calc_rate = executed * 1000000 / time_us
            res_table.append([
                config["locking_mode"],
                config["journal_mode"],
                config["vfs"],
                decode_database(config["database"]),
                config["cache_size"] if "cache_size" in config else 2000,
                errors,
                calc_rate])
    return res_table

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: formatresults result_file ...")
        sys.exit(1)

    res_table = list([r for filename in sys.argv[1:] for r in load_file(filename)])
    res_table.sort()
    for res in res_table:
        print(res)
