
import numpy as np
import random

random.seed(123)

ACTIVE_SYSTEM_CONSUMPTION = 124
IDLE_SYSTEM_CONSUMPTION = 22
K = 24
N = 10


def generateTasks(K,N):
	slotDurationPercentage = 24 / K
	x = (np.arange(N)-1)/10
	tasks_cost = np.ceil((x * ACTIVE_SYSTEM_CONSUMPTION + (1 - x)*IDLE_SYSTEM_CONSUMPTION)*slotDurationPercentage)
	tasks_quality = np.linspace(1,100,N,dtype=int)
	
	#for i in range(2,N-1):
	#	minQuality = max(2*(i-1)*step, tasks_quality[i-1]+step)
	#	maxQuality = i * step;
	#	tasks_quality[i] = random.randint(minQuality,maxQuality) 
	#tasks_quality[N-1] = 100;
	return tasks_cost.astype(int), tasks_quality




if __name__ == "__main__":
	tasks_cost, tasks_quality = generateTasks(K,N)
	print("{ ",end="")
	for i,t in enumerate(zip(tasks_cost,tasks_quality)):
		s = "{%d,%d}," % t if i!=N-1 else "{%d,%d}" % t
		print(s,end="")
	print(" }")