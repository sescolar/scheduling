import pandas as pd

column_names = ['Date', 'Hour', 'Node', 'BatteryCurrent', 'PVCurrent', 'WindSpeed',
         'Temperature', 'Humidity', 'BatteryVoltage',
         'SOC', 'SOC2', 'SOC3', 'SOC4', 'SOC4-SOC2']


print("Reading datafile..")

df = pd.read_csv('test_SOC.csv', parse_dates=[[0, 1]], header=0, names=column_names, dayfirst=True)

print("Computing hourly averages..")

df.set_index("Date_Hour",inplace=True)
averages = df.resample("1H").mean()
averages['PVCurrent'].to_csv('hours_averages.csv')

print("done")
