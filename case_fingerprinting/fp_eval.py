import json
import numpy as np

from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sktime.classification.interval_based import TimeSeriesForestClassifier
import matplotlib.pyplot as plt
import seaborn as sns
def eval(dataset_name):
    y_pred_full, y_test_full = [], []
    traces=json.load(open(dataset_name+'.json'))["traces"]
    labels=json.load(open(dataset_name+'.json'))["labels"]
    traces = np.array(traces)
    labels = np.array(labels)
    print("Number of traces: ", len(traces))
    # Re-train 10 times in order to reduce effects of randomness
    for _ in range(10):
        X_train, X_test, y_train, y_test = train_test_split(traces, labels, test_size=0.2)
        clf = TimeSeriesForestClassifier(n_estimators=500, min_interval=3, n_jobs=32)
        clf.fit(X_train, y_train)
        y_pred = clf.predict(X_test)
        y_test_full.extend(y_test)
        y_pred_full.extend(y_pred) 
    report=classification_report(y_test_full, y_pred_full)
    print(report)
    with open("classification_report.txt", "w") as f:
        f.write(report)
    cm = confusion_matrix(y_test_full, y_pred_full)
    print("Confusion Matrix:")
    print(cm)
    with open("confusion_matrix.txt", "w") as f:
        f.write(str(cm))

    plt.figure(figsize=(8,6))
    sns.heatmap(cm, annot=True, fmt='d', cmap="Blues", xticklabels=clf.classes_, yticklabels=clf.classes_)
    plt.savefig("confusion_matrix.png")
if __name__ == "__main__":
    eval("trace")
