import pandas as pd

column_names = ['Date' , 'Production'] 


print("Reading datafile..")

df = pd.read_csv('solar_0001_solar_current_sose.csv', parse_dates=['Date'], header=0, names=column_names, dayfirst=True)
df.set_index("Date",inplace=True)

print("Computing hourly averages..")

# resample with 1 hour interval. (mean of all minutes)
print(df['Production'])
averages = df.resample("1H").mean()
print("after averages")

# group by each month and compute the mean for each hour of the month across day 

#result = averages.groupby([averages.index.month,averages.index.day,averages.index.hour]).mean().rename_axis(["month", "day", "hour"])
result = averages.groupby([averages.index.month,averages.index.hour]).mean().rename_axis(["month", "hour"])

result['Production'].to_csv('dataset_avg.csv')

print("done")
