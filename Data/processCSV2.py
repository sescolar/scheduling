import pandas as pd

column_names = ['Date', 'Hour', 'Node', 'BatteryCurrent', 'PVCurrent', 'WindSpeed',
         'Temperature', 'Humidity', 'BatteryVoltage',
         'SOC', 'SOC2', 'SOC3', 'SOC4', 'SOC4-SOC2']


print("Reading datafile..")

df = pd.read_csv('test_SOC.csv', parse_dates=[[0, 1]], header=0, names=column_names, dayfirst=True)
df.set_index("Date_Hour",inplace=True)

print("Computing hourly averages..")

# resample with 1 hour interval. (mean of all minutes)
averages = df.resample("1H").mean()

# group by each month and compute the mean for each hour of the month across day 

result = averages.groupby([averages.index.month,averages.index.hour]).mean().rename_axis(["month", "hour"])

result['PVCurrent'].to_csv('averages.csv')

print("done")
