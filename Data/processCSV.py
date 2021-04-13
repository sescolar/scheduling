#!/usr/bin/python3

import pandas
import matplotlib.pyplot as plt
from matplotlib import colors as mcolors
from csv import reader, writer
import sys
import numpy as np
import time

HOURS = 24
DAYS = 31

filename = "test_SOC.csv"
output = "aggregated.csv"
final = "avg_real_production.csv"


headers = ['Date','Hour','Node','Battery Current','PV sc Current','Wind Speed','Temperature','Humidity','Battery Voltage','SOC','SOC2','SOC3','SOC4','SOC4-SOC2']
hour = ""
acum = 0
counter = 0
num = len(headers)
i = 0
myList = []
# open file in read mode

for i in range(1,DAYS+1):
    if i < 10: date = "0"+str(i)+"/08/2018"
    else: date = str(i)+"/08/2018"
    with open(filename, 'r') as read_obj:
        # pass the file object to reader() to get the reader object
        csv_reader = reader(read_obj)
        # Iterate over each row in the csv using reader object
        for row in csv_reader:
            # row variable is a list that represents a row in csv
            if len(row) != num:
                continue
            if date == row[0]:
                if hour == str(row[1])[0:2]:
                    if float(row[4])>= float(0):
                        acum = acum + float(row[4])
                        counter = counter + 1
                else:
                    if counter != 0:
                        #print("Date={0}; Hour={1:2d}; mA={2:.2f}".format(date, int(hour), float(acum/counter)))
                        myList.append([str(date), hour, float(acum/counter)])
                        i = i + 1
                    hour = str(row[1])[0:2]
                    acum = 0
                    counter = 0
            else:
                date = row[0]
    if counter!= 0:
        #print("Date={0}; Hour={1:2d}; mA={2:.2f}".format(date, int(hour), float(acum/counter)))
        myList.append([str(date), hour, float(acum/counter)])

with open(output, mode='w') as output_file:
    output_writer = writer(output_file, delimiter=',')
    for i in range(0,len(myList[:])):
        output_writer.writerow(myList[i][:]);
output_file.close()

fileObject = open(output)
rows = len(fileObject.readlines())
fileObject.close()


myList = []
read_obj.close()
for j in range(7,11):
    if j<10: month="0"+str(j)
    else: month=str(j)
    i = 0
    while i<HOURS:
        if i<10: hour="0"+str(i)
        else: hour=str(i)
        acum=0
        counter=0
        with open(output, 'r') as read_obj:
            # pass the file object to reader() to get the reader object
            csv_reader = reader(read_obj)
            # Iterate over each row in the csv using reader object
            for row in csv_reader:
                if (month!=row[0][3:5]):
                    if counter != 0:
                        #print("Month={0}; Hour={1}; mA={2:.2f}".format(month, hour, float(acum/counter)))
                        myList.append([str(month), hour, float(acum/counter)])
                        read_obj.close()  
                        break
                else: 
                    if hour == row[1]:
                        if float(row[2])>= float(0):
                            acum = acum + float(row[2])
                            counter = counter + 1
                            #print("Month={0}; Hour={1}; mA={2:.2f}".format(month, hour, float(acum/counter)))
        i = int(hour) + 1
    i = 0
print(myList)
with open(final, mode='w') as output_file:
    output_writer = writer(output_file, delimiter=';')
    for i in range(0,len(myList[:])):
        output_writer.writerow(myList[i][:]);
output_file.close()
