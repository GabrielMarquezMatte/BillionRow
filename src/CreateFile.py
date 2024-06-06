#!/usr/bin/env python
#
#  Copyright 2023 The original authors
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

# Based on https://github.com/gunnarmorling/1brc/blob/main/src/main/java/dev/morling/onebrc/CreateMeasurements.java

import os
import sys
from tqdm import tqdm
import time
import numpy as np
from concurrent.futures import ProcessPoolExecutor, as_completed

def check_args(file_args):
    """
    Sanity checks out input and prints out usage if input is not a positive integer
    """
    try:
        if len(file_args) != 2 or int(file_args[1]) <= 0:
            raise Exception()
    except:
        print("Usage:  create_measurements.sh <positive integer number of records to create>")
        print("        You can use underscore notation for large number of records.")
        print("        For example:  1_000_000_000 for one billion")
        exit()


def build_weather_station_name_list():
    """
    Grabs the weather station names from example data provided in repo and dedups
    """
    station_names = []
    with open('weather_stations.csv', 'r', encoding="utf-8") as file:
        file_contents = file.read()
    for station in file_contents.splitlines():
        if "#" in station:
            next
        else:
            station_names.append(station.split(';')[0])
    return list(set(station_names))


def convert_bytes(num):
    """
    Convert bytes to a human-readable format (e.g., KiB, MiB, GiB)
    """
    for x in ['bytes', 'KiB', 'MiB', 'GiB']:
        if num < 1024.0:
            return "%3.1f %s" % (num, x)
        num /= 1024.0


def format_elapsed_time(seconds):
    """
    Format elapsed time in a human-readable format
    """
    if seconds < 60:
        return f"{seconds:.3f} seconds"
    if seconds < 3600:
        minutes, seconds = divmod(seconds, 60)
        return f"{int(minutes)} minutes {seconds:.3f} seconds"
    hours, remainder = divmod(seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if minutes == 0:
        return f"{int(hours)} hours {int(seconds)} seconds"
    return f"{int(hours)} hours {int(minutes)} minutes {int(seconds)} seconds"


def estimate_file_size(weather_station_names, num_rows_to_create):
    """
    Tries to estimate how large a file the test data will be
    """
    total_name_bytes = sum(len(s.encode("utf-8")) for s in weather_station_names)
    avg_name_bytes = total_name_bytes / float(len(weather_station_names))

    # avg_temp_bytes = sum(len(str(n / 10.0)) for n in range(-999, 1000)) / 1999
    avg_temp_bytes = 4.400200100050025

    # add 2 for separator and newline
    avg_line_length = avg_name_bytes + avg_temp_bytes + 2

    human_file_size = convert_bytes(num_rows_to_create * avg_line_length)

    return f"Estimated max file size is:  {human_file_size}."

def generate_batch(weather_station_names, batch_size, coldest_temp, hottest_temp):
    batch = np.random.choice(weather_station_names, size=batch_size)
    temperatures = np.random.uniform(coldest_temp, hottest_temp, size=batch_size)
    return [b.encode() + f';{t:.1f}'.encode() for b, t in zip(batch, temperatures)]


def build_test_data(weather_station_names: list[str], num_rows_to_create: int):
    """
    Generates and writes to file the requested length of test data
    """
    start_time = time.time()
    coldest_temp = -99.9
    hottest_temp = 99.9
    station_names_10k_max = np.random.choice(weather_station_names, size=10_000)
    batch_size = min(100000, num_rows_to_create) # instead of writing line by line to file, process a batch of stations and put it to disk
    chunks = num_rows_to_create // batch_size
    print('Building test data...')
    try:
        with ProcessPoolExecutor(max_workers=6) as executor:
            futures = (executor.submit(generate_batch, station_names_10k_max, batch_size, coldest_temp, hottest_temp) for _ in range(chunks))
            with open("data/data.csv", 'wb') as file:
                for future in tqdm(as_completed(futures), total=chunks, desc="Writing to file", unit="chunks"):
                    batch = future.result()
                    file.write(b'\n'.join(batch) + b'\n')
        sys.stdout.write('\n')
    except Exception as e:
        print("Something went wrong. Printing error info and exiting...")
        print(e)
        exit()
    end_time = time.time()
    elapsed_time = end_time - start_time
    file_size = os.path.getsize("data/data.csv")
    human_file_size = convert_bytes(file_size)
 
    print("Test data successfully written to data/data.csv")
    print(f"Actual file size:  {human_file_size}")
    print(f"Elapsed time: {format_elapsed_time(elapsed_time)}")


def main():
    """
    main program function
    """
    check_args(sys.argv)
    num_rows_to_create = int(sys.argv[1])
    weather_station_names = []
    weather_station_names = build_weather_station_name_list()
    print(estimate_file_size(weather_station_names, num_rows_to_create))
    build_test_data(weather_station_names, num_rows_to_create)
    print("Test data build complete.")


if __name__ == "__main__":
    main()