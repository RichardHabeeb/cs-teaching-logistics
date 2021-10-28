import csv
import sys


if __name__ == "__main__":

    with open(sys.argv[1], 'r') as f1:
        with open(sys.argv[2], 'r') as f2:
            r1 = csv.DictReader(f1)
            r2 = csv.DictReader(f2)

            rows1 = dict((row['SIS Login ID'], row) for row in r1)

            writer = csv.DictWriter(sys.stdout, r1.fieldnames)
            writer.writeheader()

            for row in r2:

                row[sys.argv[3]] = str(float(row[sys.argv[3]]) +
                        float(rows1[row['SIS Login ID']][sys.argv[3]]))

                writer.writerow(row)






