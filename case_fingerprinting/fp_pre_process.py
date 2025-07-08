import os
import json
def process_files(data_folder):
    sums_list = []
    names_list = []
    for folder_name in os.listdir(data_folder):
        folder_path = os.path.join(data_folder, folder_name)
        if os.path.isdir(folder_path):
            for file_name in os.listdir(folder_path):
                file_path = os.path.join(folder_path, file_name)
                if file_name.endswith(".txt"):
                    name = folder_name
                    with open(file_path, 'r') as file:
                        sum_list=[]
                        for line in file:
                            numbers = list(map(int, line.split()))
                            line_sum = sum(numbers)
                            sum_list.append(line_sum)
                        sums_list.append(sum_list)
                        names_list.append(name)    
    return sums_list, names_list

if __name__ == "__main__":
    data_folder = "data"
    sums, names = process_files(data_folder)
    with open("trace.json", "w") as out:
        json.dump({
            "traces": sums,
            "labels": names
        }, out, separators=(",", ":"))