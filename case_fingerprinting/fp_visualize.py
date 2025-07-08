import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", help="file path")
    parser.add_argument("-bg", "--bg", help="background file path", default='data/result_bg.txt')
    parser.add_argument("-nc", "--noise_cancel", help="whether to cancel noise using the background file", action='store_true', default=False)
    args = parser.parse_args()
    file_path = args.file
    bg_path = args.bg
    # file_path = 'data/taobao/taobao_2.txt'
    # file_path = 'donothing.txt'
    noise_cancel = args.noise_cancel


    data = np.loadtxt(file_path)
    if noise_cancel:
        data_bg = np.loadtxt(bg_path)
        data = data - data_bg
        data[data < 0] = 0


    plt.figure(figsize=(12, 8))
    sns.heatmap(data, cmap='viridis', cbar=True)
    plt.title(file_path.split('.')[0])
    plt.xlabel("Features (256)")
    plt.ylabel("Samples (5000)")


    # plt.show()

    if noise_cancel:
        plt.savefig(file_path.split('.')[0]+'_nc.png')
    else:
        plt.savefig(file_path.split('.')[0]+'.png')
