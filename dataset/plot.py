import math
import numpy as np
import datetime as dt
import matplotlib.pyplot as plt
import pandas as pd
import time
import sys

import pvlib
from pvlib.location import Location

CSV_FILE = "dataset_avg.csv"

headers = ['Month','Hour','Production']
df = pd.read_csv(CSV_FILE, names=headers)
df.set_index(['Month','Hour'])
#remove the headers of the csv file
df['Production']=df['Production'].astype(float)

ax=df.plot(y='Production')
ax.set_xticks(np.arange(1,288,24))
ax.set_xticklabels(['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'])
#plt.show()
plt.savefig('out.png')

