#include <assert.h>
// SOLEDAD: added to measure time
#include <sys/time.h>


#define K	      24	 
#define N_ITERATION   10
#define slotDurationPercentage  (24.0/K)

#define BMAX              2000
#define BMIN               160

// SOLEDAD change: B_INIT is a percentage of the max. capacity
#define B_INIT_PERC	  (85.0/100)
#define B_INIT            (BMAX*(B_INIT_PERC))		//1800 

// parameter for the precision of the classic algorithm 'with sampling'
// SOLEDAD: Battery Sampling is minimum when BMIN es maximum
#define BATTERY_SAMPLING_MIN	(BMAX-(BMIN*10))		//minimum = BMAX -BMIN*X = 2000-1600=400 
#define BATTERY_SAMPLING_MID	(BMAX-(BMIN*5))			//medium =  BMAX -BMIN*X/Y = 2000-800 = 1200
#define BATTERY_SAMPLING_MAX	(BMAX-BMIN)			//maximum = BMAX -BMIN = 2000-160=1840
#define BATTERY_SAMPLING  	BATTERY_SAMPLING_MAX		//100, 20(BATTERY_SAMPLING_MID)    

// parameter for the precision of the new algorithm
#define MAX_QUALITY_LVL	100              			// maximum = 100 (it is related to the quality as a percentage)

#define mAh_per_lvl      ((float)(BMAX-BMIN)/BATTERY_SAMPLING)
#define level_to_mah(l)  short(ceil((l)*(mAh_per_lvl))+BMIN) 
//Sole: This definition does not work if b=BMIN
#define mah_to_level(b)  short((b-BMIN)/(mAh_per_lvl))
//#define mah_to_level(b)  short(min(BMIN/mAh_per_lvl, b-BMIN/mAh_per_lvl))

#define SEED               124
#define VARIATION            1


/**************************************** TASK MODEL *******************************/

#define N_TASKS   10
#define ACTIVE_SYSTEM_CONSUMPTION   124
#define IDLE_SYSTEM_CONSUMPTION      22 // idle is higher, we must check it



struct Task{
    uint8_t q_perc;    // 1 <= q_perc <= 100%
#ifdef CARFAGNA
    uint8_t q_lvl;
#endif
    uint8_t c_mAh;
};
struct Task tasks[N_TASKS];
uint8_t i;

void GenerateTasks(void)
{
    tasks[0].c_mAh  = 1; 
    tasks[0].q_perc = 1;
    for(i=1; i<N_TASKS; i++)
        tasks[i].c_mAh = ceil(((((float)(i-1.0) / 10.0) * ACTIVE_SYSTEM_CONSUMPTION) +
                        ((1 - (((float) (i-1.0)) / 10.0)) * IDLE_SYSTEM_CONSUMPTION)) * 
                        slotDurationPercentage);
   float scale = 100.0 / tasks[N_TASKS-1].c_mAh;
   for (i=0; i<N_TASKS; i++)
        tasks[i].q_perc = ceil(tasks[i].c_mAh*scale);    
#ifdef CARFAGNA
    const float s = 100.0 / MAX_QUALITY_LVL;      // percentage point of quality for each level
    for (i=0; i<N_TASKS; i++)
      tasks[i].q_lvl = floor( tasks[i].q_perc) / s;
#endif
}

/************************************** END TASK MODEL *******************************/


// SOLEDAD: added to define production
uint16_t E_PROD_OCT[24]={0,0,0,0,0,0,0,0,19,110,224,285,335,350,331,283,134,20,18,8,0,0,0,0 };
uint16_t E_PROD_SEP[24]={0,0,0,0,0,0,0,8,25,108,203,270,313,316,310,250,96,45,37,15,2,0,0,0};
uint16_t E_PROD_AUG[24]={0,0,0,0,0,0,0,4,46,133,216,286,328,339,322,254,60,66,63,23,9,0,0,0};
uint16_t *E_h = E_PROD_AUG;

// SOLEDAD: comment lines
//uint16_t E_h[24]     // Hourly Energy harvested 
//// October 
//= E_PROD_OCT; 

uint16_t E_h_v[24]  = { 0 };    // Hourly Energy harvested varied
uint16_t E_s_mAh[K] = { 0 };    // Final Energy harvested per slot in mAh

void InitializeEnergyHarvested(void)
{
  uint16_t total1 = 0;
  for(i=0; i<24; i++) total1 += E_h[i];
  if (K >= 24) {
    uint16_t s = 0;
    for (i=0; i<24; i++)
      for (uint8_t j=0; j<(K/24); j++)
        s+= E_s_mAh[(i*K/24)+j] = (int)(E_h[i] * slotDurationPercentage);
    uint8_t r = total1 - s;
    for (uint8_t j=K/2; r>0 && j<K; j++) {
      E_s_mAh[j] += 1; r--;
    }
  } else {
    uint8_t scale = 24/K;
    for (i=0; i<K; i++) {
      E_s_mAh[i] = 0;
      for (uint8_t j=i*scale; j<(i+1)*scale; j++) {
        E_s_mAh[i] += E_h[j];
      }
    }
  }
}


#define MAX_OVERPRODUCTION           20
#define MAX_UNDERPRODUCTION          40
#define SUNSET                       19
#define SUNRISE                       8

void update_panel() 
{
  uint8_t coin;
  uint8_t pos;
  uint8_t variation;
  
  // Change the energy production of the panel randomly
  // First it changes the production for each hour. then
  // we adapt the production to the slot size.
  
  
  for (i=0; i<24; i++) {
    if (SUNRISE <= i && i <= SUNSET) {
        if (!VARIATION) coin = 2; else coin = rand() % 3;
        switch (coin) {
        case 0 :
            variation = 100 + (rand() % (MAX_OVERPRODUCTION + 1));
            break;
        case 1 :
            variation = 100 - (rand() % (MAX_UNDERPRODUCTION + 1));
            break;
        case 2 :
            variation = 100;
            break;
        }
        E_h_v[i] = min((int)(E_h[i] * (float)variation / 100),520);   // 520 is the maximum from the panel
    }
    // if K>24 we must spread the production between K/24 slots
    for (int8_t j=0; (K >= 24) && j<(K/24); j++)
      E_s_mAh[(i * K / 24) + j] = (int)(E_h_v[i] * slotDurationPercentage);
  }
  // if K < 24 we must add the production to slot longer than one hour
  if (K < 24) {
    uint8_t scale = 24/K;
    for (i=0; i<K; i++) {
      E_s_mAh[i] = 0;
      for (uint8_t j=i*scale; j<(i+1)*scale; j++) {
        E_s_mAh[i] += E_h_v[j];
      }
    }
  }
  // When we split the production, we loose some watt, this spread them in the
  // center slots. So we run the schedule with the same total production.
  if (K>24) {
    uint16_t total = 0;
    for (i=0; i<24; i++) total += E_h_v[i];
    uint16_t total2 = 0;
    for (i=0; i<K; i++) total2 += E_s_mAh[i];
    uint8_t r = total - total2;
    for (uint8_t j=K/2; r>0 && j<K; j++) {
      E_s_mAh[j] += 1; r--;
    }
  }  
}  

// **************** END PANEL MODEL **********************

char buf[60];
void PrintParameters(void) {
  Serial.flush();
  Serial.print(F("\n#----------------------------------------------\n"));
  Serial.print(F("K = ")); Serial.println(K);
  Serial.print(F("N = ")); Serial.println(N_TASKS);
  
#ifdef CARFAGNA
  Serial.print("BMIN = "); Serial.println(mah_to_level(BMIN));
  Serial.print("BINIT = "); Serial.println(mah_to_level(B_INIT));
  Serial.print("BMAX = "); Serial.println(mah_to_level(BMAX));
  Serial.print(F("MAX_QUALITY_LVL = ")); Serial.println(MAX_QUALITY_LVL);
  Serial.print(F("EnergyForLevel = ")); Serial.println(mAh_per_lvl);  
#else
  Serial.print(F("BMIN = ")); Serial.println(BMIN); 
  Serial.print(F("BINIT = ")); Serial.println(B_INIT);
  Serial.print(F("BMAX = ")); Serial.println(BMAX);
  Serial.print(F("BSAMPLING = ")); Serial.println(BATTERY_SAMPLING);
#endif
  Serial.print(F("\nc_i = ["));
  for (i=0; i<N_TASKS; i++) {
    sprintf(buf,"%3d%c",tasks[i].c_mAh,(i==N_TASKS-1?']':','));
    Serial.print(buf);
  }
  Serial.print("\n");
  Serial.print(F("q_i = ["));
  for (i=0; i<N_TASKS; i++) {
      sprintf(buf,"%3d%c",tasks[i].q_perc,(i==N_TASKS-1?']':','));
      Serial.print(buf);
  }
#ifdef CARFAGNA
  Serial.print("\n");
  Serial.print(F("lvl_i = ["));
  for (i=0; i<N_TASKS; i++) {
      sprintf(buf,"%3d%c",tasks[i].q_lvl,(i==N_TASKS-1?']':','));
      Serial.print(buf);
  }
#endif
  Serial.print(F("\ne_i = ["));
  for (i=0; i<24; i++) {
      sprintf(buf,"%3d%c",E_h[i],(i==24-1?']':','));
      Serial.print(buf);
  }
  Serial.print(F("\nE_i = ["));
  for (i=0; i<K; i++) {
      sprintf(buf,"%3d%c",E_s_mAh[i],(i==K-1?']':','));
      if ((i+1) % 25 == 0) Serial.print("\n");
      Serial.print(buf);
  }
  Serial.print(F("\n#----------------------------------------------\n"));
}


#ifdef CARFAGNA
uint16_t Q[2][K*MAX_QUALITY_LVL+1] = { 0 };         // DP: Quality Table
uint8_t  S[K][K*MAX_QUALITY_LVL+1];                  // DP: Scheduling Table 
#else
uint16_t Q[2][BATTERY_SAMPLING+1] = { 0 };         // DP: Quality Table
uint8_t  S[K][BATTERY_SAMPLING+1];                  // DP: Scheduling Table 
#endif
uint8_t NS[K];                                     // Final Scheduling 



//#define DEBUG 0
//  optQ = solution(B[(K-1)%2],S,K,NS,tasks,relaxedOptQ);
int checkScheduleCarfagna(int qUpperBound){
#if defined CARFAGNA
    while((qUpperBound >= 0) && !((Q[(K-1)%2][qUpperBound] >= B_INIT) && (Q[(K-1)%2][qUpperBound] != 0))){
        qUpperBound--;
    }
    int t = qUpperBound;
    if(qUpperBound != -1){
        for(int s=K-1;s>=0;s--){
            NS[s] = S[s][t];
            t -= tasks[NS[s]-1].q_lvl;
        }
    }
    Serial.println(qUpperBound);
    return qUpperBound;
#endif
}

// schedule(N_TASKS, K, MAX_QUALITY_LVL, E_s_mAh, S, B, tasks);
int scheduleCarfagna(uint16_t E[])
{
  int8_t   t,idmax;
  int16_t  b,Br,Bprec;
  int8_t   k = K-1;       // start in the last slot
  uint16_t qmax = 0,q,l,s;
  uint16_t  maxq_ps = 0;	// maxQualityPreviousSlot
  uint16_t  maxq_cs = 0;	// maxQualityCurrentSlot
  //  B = np.zeros((2,K*maxQ+1),dtype=np.int16) // --> Q
  //  S = np.zeros((K,K*maxQ+1),dtype=np.int8)  // --> S

  #ifdef DEBUG
      sprintf(buf,"------ k=%d ----\n",k);
      Serial.print(buf);
  #endif

  #ifdef CARFAGNA
  for(k=0;k<K;k++){
	  maxq_cs = 0;
	  if (k == 0){
		  for(q=0;q<=MAX_QUALITY_LVL;q++){
			  qmax=0;	//currentBmax
			  idmax=0;
			  for (t=0;t<N_TASKS;t++){
				  Br = B_INIT + E_s_mAh[0] - tasks[t].c_mAh;
				  if ((tasks[t].q_lvl==q)&&(Br>=BMIN)&&(Br>=qmax)){
				  	qmax = Br;
					idmax = t+1;
					maxq_cs = q;
				  }
			  }
			  Q[0][q] = min(qmax,BMAX);
			  S[0][q] = idmax;
	          }
	  }
	  else{
	  	for(q=0;q<=maxq_ps+MAX_QUALITY_LVL;q++){
			qmax=0;
			idmax=0;
			for(t=0;t<N_TASKS;t++){
				if((q>=tasks[t].q_lvl)&&((q-tasks[t].q_lvl)-maxq_ps)&&((Bprec = Q[(k-1)%2][q-tasks[t].q_lvl])!=0)&&((Br=Bprec+E_s_mAh[k] - tasks[t].c_mAh)>=BMIN)&&(Br>qmax)){
					qmax = Br;
					idmax = t+1;
					maxq_cs=q;
				}
			}
			Q[k%2][q] = min(qmax,BMAX);
			S[k][q] = idmax;
	        }
	  }
	  maxq_ps=maxq_cs;
	  // SOLE: Changed: never -1 as maxq_ps is unsigned 
	  //if(maxq_ps==-1) return -1;	// No solution found
	  if(maxq_ps==0) return 0;	// No solution found 
  }
  #endif
  return maxq_cs;
 
}

int schedule(uint16_t E[])
{
  int8_t   t,idmax;
  int16_t  b,Br;
  int8_t   k = K-1;       // start in the last slot
  uint16_t qmax = 0,q=0,l;

  #ifdef DEBUG
      sprintf(buf,"------ k=%d ----\n",k);
      Serial.print(buf);
  #endif

  for (b=BATTERY_SAMPLING; b>=0; b--) {
    uint16_t b_mAh = level_to_mah(b);               // convert level b to mhA
    idmax = 0;
    qmax = 0;
    for (t=N_TASKS-1; t>=0; t--) {
      Br = min(b_mAh - tasks[t].c_mAh + E[k], BMAX);
      if (Br >= B_INIT) {
         qmax = tasks[t].q_perc;
         idmax = t+1;
         break;
      }
    }
    Q[k%2][b] = qmax;
    S[k][b] = idmax;
    #ifdef DEBUG
      sprintf(buf,"qmax=%d, idmax=%d\n",qmax,idmax);
      Serial.print(buf);
    #endif
  }


  for (k=K-2; k>=0; k--) {
    #ifdef DEBUG
      sprintf(buf,"------ k=%d ----\n",k);
      Serial.print(buf);
    #endif
    for (b=BATTERY_SAMPLING; b>=0; b--) {
      uint16_t b_mAh = level_to_mah(b);
      qmax = 0, idmax = 0; 
      for (t=N_TASKS-1; t>=0; t--) {  // iterate from the last
        Br = min(b_mAh - tasks[t].c_mAh + E[k], BMAX);
        if (Br >= BMIN) {
            l = mah_to_level(Br);   
            assert(l <= BATTERY_SAMPLING);
            q = Q[(k + 1)%2][l];
            if (q!=0 && (q + tasks[t].q_perc) > qmax) {
              qmax = q + tasks[t].q_perc;
              idmax = t+1;
            }
        }
      }
      Q[k%2][b] = qmax;
      S[k][b] = idmax;
    } 
  }

  if (K%2 == 0)
    return Q[K%2][mah_to_level(B_INIT)];
  else
    return Q[(K+1)%2][mah_to_level(B_INIT)];
}

// This tasks includes checkQuality, checkBres, checkFeasibility
// checkQuality(NS,optQ,tasks)
// checkBres(optQ, B[(K-1)%2], K, NS, tasks, E_s_mAh)
// checkFeasibility(K, NS, tasks, E_s_mAh)
void checkSolution(uint16_t E[K], uint16_t Q)
{
  uint16_t BresL = mah_to_level(B_INIT);
  uint16_t Bres_mAh, i, q, q_lvl = 0;

#if defined CARFAGNA
  for (i=0; i<K; i++){ 
    q_lvl += tasks[NS[i]-1].q_lvl;  	//checkQuality
  }
#endif    
  q = 0;
  for (i=0; i<K; i++){ 
#ifndef CARFAGNA
    NS[i] = S[i][BresL];	 
#endif    
    q = q + tasks[NS[i]-1].q_perc;  
    assert(NS[i]>0);			//checkFeasibility
#ifndef CARFAGNA
    Bres_mAh = level_to_mah(BresL);
    Bres_mAh = min(Bres_mAh - tasks[ NS[i]-1 ].c_mAh + E[i], BMAX);
    assert(Bres_mAh >= BMIN);		//checkFeasibility
    BresL = mah_to_level(Bres_mAh);
#else
    Bres_mAh = level_to_mah(BresL);
    Bres_mAh = min(Bres_mAh - tasks[ NS[i]-1 ].c_mAh + E[i], BMAX);
    BresL = mah_to_level(Bres_mAh);
    assert(BresL >= mah_to_level(2*BMIN));		//checkFeasibility
#endif
}
  assert(Bres_mAh >= B_INIT);		//checkFeasibility
  assert(q == Q);
}


void ClearQS()
{
  memset(Q,0,sizeof(Q));
  memset(S,0,sizeof(S));
  memset(NS,0,sizeof(NS));
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) ;

  srand(SEED);
  GenerateTasks();
  InitializeEnergyHarvested();
  PrintParameters();   
}

uint8_t counter = 0;              // iteration counter
// SOLEDAD: added to measure start and stop times lower than 1 millisecond
struct timeval stop, start;


void loop() {
  if (counter < N_ITERATION) {

    Serial.print(F("it,Energy = "));
    Serial.print(counter);  
    Serial.print(",");
    if (counter != 0) update_panel();
    uint16_t totalEnergyHarvested = 0;
    for(i=0; i<K; i++) totalEnergyHarvested += E_s_mAh[i];
    Serial.println(totalEnergyHarvested);

    Serial.print(F("E = ["));
    for (i=0; i<K; i++) {
      Serial.print(E_s_mAh[i]);
      Serial.print(i==K-1?"]":",");
    } Serial.println("");

    ClearQS();
    // ***** Scheduling ******
    gettimeofday(&start, NULL);
#ifdef CARFAGNA
    uint16_t Q_carfagna = scheduleCarfagna(E_s_mAh);
    if (Q_carfagna == 0) {
	Serial.println("No solution was found");
    	exit(0);
    }
    uint16_t optQ = checkScheduleCarfagna(Q_carfagna);
#else
    uint16_t optQ = schedule(E_s_mAh);
#endif    
    gettimeofday(&stop, NULL);

    // ***** Check solution ******
    if (optQ != 0) {
      Serial.print("Q = ");
      Serial.println(optQ);
      checkSolution(E_s_mAh,optQ);
      Serial.print("S = [");
      for(i=0; i<K; i++) {
        Serial.print(NS[i]);
        Serial.print((i==K-1)?"]":",");
      }
      Serial.println("");
    }

    Serial.print(F("Time = "));
    Serial.println((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec);
    counter++;
  } else delay(1000);
}
