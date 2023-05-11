
# Please add here the date of last change, or add to a git repo.
# Antonio / 18/04/2023


import sys
import numpy as np
import math
from collections import namedtuple

Task = namedtuple("Task","cost quality")

def check(K,S,Bstart,Bmin,Bmax,E,Tasks,debug=False):
    """ Check that the schedule is valid and energy neutral
        return the overall quality if valid or 0 if not
    """
    b = Bstart
    q = 0
    for i in range(K):
        bnew = min(b+E[i]-Tasks[S[i]].cost,Bmax)
        q += Tasks[S[i]].quality
        if debug: print("%2d :  %4d - %3d + %3d = %4d : %4d" %
        (S[i],b,Tasks[S[i]].cost,E[i],bnew,q))
        b = bnew
        if (b < Bmin): return 0
    if (b < Bstart):
        return 0
    return sum(Tasks[s].quality for s in S)


def schedule(K,Bstart,Bmin,Bmax,E,Tasks):
    #print("run schedule on",K,Bstart,Bmin,Bmax,Tasks,E)
    M = np.zeros( (  2,Bmax+1), dtype=int)
    I = np.zeros( (K-1,Bmax+1), dtype=int)
    k = K-1
    t = len(Tasks)-1
    for b in range(Bmax,-1,-1):
        while (t>=0):
            if (b+E[k]-Tasks[t].cost >= Bstart):
                M[k%2][b] = Tasks[t].quality
                I[k-1][b] = t+1
                break;
            t = t-1
    Mz,Iz = 0,0
    for k in range(K-2,-1,-1):
        if k == 0:
            b = Bstart
            qmax = -100
            idmax = 0
            for t in range(len(Tasks)):
                Bprime = min(b + E[k] -Tasks[t].cost,Bmax);
                if Bprime >= Bmin:
                    q = M[(k+1)%2][Bprime]
                    if (q == 0): continue
                    if (q + Tasks[t].quality > qmax):
                        qmax = q + Tasks[t].quality
                        idmax = t+1
            Mz = qmax if qmax != -100 else 0
            Iz = idmax
        else:
            for b in range(0,Bmax+1):
                qmax = -100
                idmax = 0
                for t in range(len(Tasks)):
                    Bprime = min(b+E[k]-Tasks[t].cost,Bmax);
                    if (Bprime >= Bmin):
                        q = M[(k+1)%2][Bprime]
                        if (q == 0): continue
                        if (q + Tasks[t].quality > qmax):
                            qmax = q + Tasks[t].quality
                            idmax = t+1
                M[k%2][b] = qmax if qmax != -100 else 0
                I[k-1][b] = idmax

    S = [0]*K
    B = Bstart
    S[0] = Iz-1
    if (S[0] < 0): return (S,0) 
    B = min(B + E[0] - Tasks[S[0]].cost,Bmax)
    assert(B >= Bmin)
    for i in range(1,K):
        S[i] = I[i-1][B]-1
        if (S[i] < 0): return (S,0)
        B = min(B + E[i] - Tasks[S[i]].cost,Bmax)
        assert(B >= Bmin)
    assert(B >= Bstart)
    qmax = Mz if K%2==0 else M[(K+1)%2][Bstart]
    assert(qmax == sum(Tasks[s].quality for s in S))
    return (S,qmax)

def ScheduleNoMem(slots,Bstart,Bmin,Bmax,Eprod,Tasks,Bend):
    # basic checks
    assert(len(Eprod) == slots)
    assert(Bmin < Bstart < Bmax)    
    M = np.zeros( (2,Bmax+1), dtype=int)
    index = -1
    idmax = 0
    for i in range(slots-1,-1,-1):
        for B in range(0,Bmax+1):
            qmax = -100
            idmax = 0
            if (i == slots-1):
                for t,task in enumerate(Tasks):
                    if (B-task.cost+Eprod[i] >= Bend and task.quality > qmax):
                        qmax = task.quality
                        idmax = t+1
            else:
                for t,task in enumerate(Tasks):
                    Bprime = min(B-task.cost+Eprod[i],Bmax)
                    if (Bprime >= Bmin):
                        q = M[(i+1)%2][Bprime]
                        if (q == 0): continue
                        if (q + task.quality > qmax):
                            qmax = q + task.quality
                            idmax = t+1
            M[i%2][B] =  qmax if qmax != -100 else 0
            if (i == 0 and B == Bstart): 
                index = idmax    
    return(M[K%2][Bstart],index)

def ScheduleClassic(slots,Bstart,Bmin,Bmax,Eprod,Tasks):
    """
    slots = K = number of slots in a day
    Battery: Bmin < Bstart < Bmax 
    PanelProducton:  Eprod | len(Eprod) == slots
    Tasks: ordered from lowest cost,quality to greatest
    """
    # basic checks
    assert(len(Eprod) == slots)
    assert(Bmin < Bstart < Bmax)    
    M = np.zeros( (slots,Bmax+1), dtype=int)
    I = np.zeros( (slots,Bmax+1), dtype=int)
    for i in range(slots-1,-1,-1):
        for B in range(0,Bmax+1):
            qmax = -100
            idmax = 0
            if (i == slots-1):
                for t,task in enumerate(Tasks):
                    if (B-task.cost+Eprod[i] >= Bstart and task.quality > qmax):
                        qmax = task.quality
                        idmax = t+1
            else:
                for t,task in enumerate(Tasks):
                    Bprime = min(B-task.cost+Eprod[i],Bmax)
                    if (Bprime >= Bmin):
                        q = M[i+1][Bprime]
                        if (q == 0): continue
                        if (q + task.quality > qmax):
                            qmax = q + task.quality
                            idmax = t+1
            M[i][B] =  qmax if qmax != -100 else 0
            I[i][B] = idmax
    S = [0]*K
    B = Bstart
    for i in range(K):
        S[i] = I[i][B]-1
        if (S[i] < 0): return (S,0)
        B = min(B + Eprod[i] - Tasks[ S[i] ].cost,Bmax)
        assert(B >= Bmin)
    assert(B >= Bstart)
    return(S,sum(Tasks[s].quality for s in S))


def find_header(lines):
    """ We start to read, from the first line that print 'it,Energy'
    """
    i = 0
    while ("it,Energy" not in lines[i]): 
        i += 1
    return i

def iterated_test():
    K = 24
    Bmin = 10
    Bmax = 220
    Bstart = 180
    Panel = [0,0,0,0,0,1,3,5,20,50,70,120,120,70,50,20,5,3,1,0,0,0,0,0]
    Tasks = []
    for x in zip([2,10,20,32,50],[1,2,3,4,5]):
        Tasks.append(Task(*x))
    (S,Q) = ScheduleClassic(K,Bstart,Bmin,Bmax,Panel,Tasks)
    print("Q = ",Q)
    print("S = ",S)
    qsum = 0
    Bend = Bstart
    S = []
    for i in range(K):
    #print("run with",(K-i,Bstart))
        (Q,task) = ScheduleNoMem(K-i,Bstart,Bmin,Bmax,Panel[i:],Tasks,Bend)
    qsum += Tasks[task-1].quality
    print("Q = ",Q," t = ",task-1, "qsum = ", qsum);
    Bstart = min( Bstart - Tasks[task-1].cost + Panel[i], Bmax )
    S += [task-1]
    print("S' =",S)
    print("TotalEnergy = ",sum(Panel)," ",sum(Panel)/K)
    sys.exit();

#### main program

if __name__ == "__main__":
    """
    # analyze.py console_output.txt option
    # console_output.txt -> output from Ardino serial, start with #------
    # option:
    # none -> parse input, run schedule exact, compare with schedule Arduino
    # 1.   -> parse input, just ouput the Arduino 'input' for scheduling
    # 2.   -> parse input, run ScheduleClassic, output result (no comparison)
    # 3.   -> parse input, run ScheduleCalssic and schedule, compare.
    # 4.   -> parse input, check if quality is correct
    """
    K,N,BMIN,BMAX,BINIT,BSAMPLING = [0]*6
    c_i, q_i, e_i,it = [0,0,0,0]
    Energy,Q,Time,E,S = 0,0,0,[],[]
    
    if len(sys.argv) == 1:
        print("analyze [serialdump]")
        sys.exit() 
    option = int(sys.argv[2]) if len(sys.argv) == 3 else 0 

    f = open(sys.argv[1])
    lines = f.read().split("\n")
    # parse header
    i = find_header(lines)
    exec("\n".join(lines[0:i]))
    #--print header
    print(K,N)
    print(BMIN,BMAX,BINIT,BSAMPLING)
    print(c_i)
    print(q_i)
    Tasks = []
    for x in zip(c_i,q_i):
        Tasks.append(Task(*x))
    #--
    opt_ratio = []
    execution_time = []
    l = i
    L = len(lines)
    skip = 5
    while (l < L):
        s = "\n".join(lines[l:l+skip])
        if s == "": break
        exec(s)
        if option == 1:
            print("%2d" % (it),"\t",E)
        elif option == 2:
            print("%2d" % (it),"\t",E)
            (s,q) = ScheduleClassic(K,BINIT,BMIN,BMAX,E,Tasks)
            print(" ",q,end=" - ")
            print(f"S = {s}" if q!=0 else "")
        elif option == 3:
            print("%2d" % (it),"\t",E)
            (s,q) = ScheduleClassic(K,BINIT,BMIN,BMAX,E,Tasks)
            print(" ",q,end=" - ")
            print(f"S = {s}" if q!=0 else "")
            (s,q) = schedule(K,BINIT,BMIN,BMAX,E,Tasks)
            print(" ",q,end=" - ")
            print(f"S = {s}" if q!=0 else "")
        elif option == 0:   
            print("%2d " % (it),Energy,Time,end=" - \n")
            (Sn,q) = schedule(K,BINIT,BMIN,BMAX,E,Tasks)
            opt_ratio.append(100*Q/q)
            execution_time.append(Time)
            print("\t",E)
            print("\tS Esatta   ",Sn,q)
            print("\tS Arduino  ",S,Q,"%.1f%%" % (100*Q/q))
        elif option == 4:
            print("%2d" % (it)," ",E)
            (s,q) = ScheduleClassic(K,BINIT,BMIN,BMAX,E,Tasks)
            print("    ",S,Q,check(K,S,BINIT,BMIN,BMAX,E,Tasks),q)
        l += skip
    print("Quality (min,max,avg): %.3f, %.3f, %.3f" % (np.array(opt_ratio).min(),np.array(opt_ratio).max(),np.array(opt_ratio).mean()))
    print("Time (min,max,avg): %.3f, %.3f, %.3f" % (np.array(execution_time).min(),np.array(execution_time).max(),np.array(execution_time).mean()))
