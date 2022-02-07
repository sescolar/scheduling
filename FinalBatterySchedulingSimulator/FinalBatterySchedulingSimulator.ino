
/* 
 * Updated: Sole
 * December 2021
 * Rule for code: never use a block comment inside another block comment
 */

#include <Arduino.h>
#include <assert.h>
#define K 48            // multiple of 24 ?? why ??
#define N_TASKS 10


// The discussion below on consumption of Arduino strongly depends on the hardware

/* Consumo Arduino:
 * 40mAh IDLE
 * 90mAh ACTIVE
 *Consumo modulo WIFI ESP8266EX
 * 340mAh TX 802.11b, CCK 11Mbps, POUT=+17 dBm (Siccome nel datasheet ? considerato 50% duty cycle
 * 100mAh Rx 802.11b, 1024 bytes packet length , �80 dBm (Stessa motivazione)
 * ? stato considerato che il modulo WIFI stia acceso il 20% del duty cycle dell'intero sistema alternando
 * in parti uguali ricezione e trasmissione
 * Ossia si ha un costo orario : 22*(1-dc)+95*dc+340*dc/10+100*dc/10 = 22*(1-dc)+90*dc+440/10*dc =
 * 22*(1-dc)+124*dc (mAh)
 */


#define ACTIVE_SYSTEM_CONSUMPTION 124
#define IDLE_SYSTEM_CONSUMPTION 16
#define BATTERY_SAMPLING 1540


// Max. over/underproduction 
#define MAX_OVERPRODUCTION 20
#define MAX_UNDERPRODUCTION 40

// Sunset and sunrise in August
/*#define SUNSET 20
#define SUNRISE 7



// Sunset and sunrise in September
#define SUNSET 20
#define SUNRISE 8
*/

// Sunset and sunrise in October
#define SUNSET 19
#define SUNRISE 8


#define BMAX 2600               // Values in mAh
#define BMIN (BMAX*0.1)         // 10% of BMAX 

#define B_INIT 1170             // why circa [Bmax ? Bmin]/2) so low?? we can start with 90% of BMAX

#define N_ITERATION 10  //100
#define SEED 124

#define VARIATION 0     // 1 sse we change the harvesting energy curve

struct Task {
    int q_perc;
    unsigned int c_mAh;
};

struct Task tasks[N_TASKS];

unsigned int E_h[24];       // Hourly Energy harvested 
unsigned int E_h_v[24];     // HOurly Energy harvested uccessiva a variazioni percentuali
unsigned int E_s_mAh[K];    // Final Energy harvested per slot in mAh
const float slotDurationPercentage = 24 / (float) K;    /*Porcentaje horario de la duraci�n de las franjas horarias*/
uint8_t pos;

uint8_t S[K][BATTERY_SAMPLING+1];     // DP: Scheduling Table 
int Q[2][BATTERY_SAMPLING+1];         // DP: Quality Table
uint8_t NS[K];                        // Final Scheduling 
int i,j,l;                              // Iterators


/*mAh correspondiente a un nivel de muestreo*/
const float mAh_per_lvl = (float)BMAX/BATTERY_SAMPLING;
#define level_to_mah(l)  short((l)*(mAh_per_lvl)) 
#define mah_to_level(b)  short((b)/(mAh_per_lvl)) 

/*Initial battery level*/
const unsigned int BinitL = floor((float)B_INIT/mAh_per_lvl);
const int bmaxINT = mAh_per_lvl*BATTERY_SAMPLING;

int schedule(unsigned int BinitL, unsigned int E[K])
{
    int qmax,q;
    int b,Br,b_mAh;
    int level;
    uint8_t idmax,t;
    
    for(int i=K-1; i>=0; i--) {
        for(b=0; b<BATTERY_SAMPLING+1; b++) {
            qmax = -1;
            idmax = 0;
            /* mAh corrispondente al livello di batteria corrente approssimato per difetto */
            //b_mAh = b * mAh_per_lvl;
            b_mAh = level_to_mah(b); 
            
            for (t=0; t<N_TASKS; t++) {
                Br = min(b_mAh - tasks[t].c_mAh + E[i], bmaxINT);
                
                if (i == K-1 && (Br >= B_INIT) && qmax < tasks[t].q_perc) {
                    qmax = tasks[t].q_perc;
                    idmax = t+1;
                } else if (i != K-1 && Br >= BMIN) {
                    /* Considero allo slot successivo un livello di batteria approssimato 
                       per difetto per garantire l'ammissibilit? */
                    //level = Br/mAh_per_lvl;
                    level = mah_to_level(Br);
                    assert(level<BATTERY_SAMPLING+1);
                    q = Q[(i + 1)%2][level];                    
                    if (q != -1 && (q + tasks[t].q_perc) > qmax) {
                        qmax = q + tasks[t].q_perc;
                        idmax = t+1;
                    }
                }
            }
            Q[i%2][b] = qmax;
            /* Contiene l'indice del task +1 per usare unsigned int*/
            S[i][b] = idmax;
            //printf("(%d,%d) ",qmax,idmax);
        }
        
    }
    if (K%2 == 0)
        return Q[K%2][BinitL];
    else
        return Q[(K+1)%2][BinitL];
}

/*Chiamare solo se opt!=-1, prima era uint8_t S[K][BMAX+1]*/
void scheduleTasks(uint8_t S[K][BATTERY_SAMPLING+1], uint8_t NS[K], uint8_t nSlots, unsigned int BinitL, struct Task tasks[K], unsigned int E[K], int mAh_per_lvl){

    int BresL = BinitL;
    int Bres_mAh;
    for(int i = 0; i<nSlots; i++){
        Bres_mAh = BresL*mAh_per_lvl;
        NS[i] = S[i][BresL];
        Bres_mAh = min(Bres_mAh - tasks[NS[i] - 1].c_mAh + E[i], BMAX);
        BresL = Bres_mAh / mAh_per_lvl;
    }
}

int checkQuality(uint8_t NS[K], int optQ, struct Task tasks[N_TASKS]){
    int sumQ=0;
    for(int i=0;i<K;i++){
        sumQ+=tasks[NS[i]-1].q_perc;
    }
    return optQ == sumQ;
}

/*Controlla l'ammissibilit? della soluzione trovata usando batteria in mAh*/
int checkFeasibility(uint8_t nSlots, uint8_t *NS, struct Task *tasks, unsigned int *E){
    int Bres = B_INIT;
    //printf("%d,",Bres);
    for(int i = 0; i<nSlots; i++){
        Bres = min(Bres - tasks[NS[i]-1].c_mAh + E[i], BMAX);
        //printf("%d,",Bres);
        /*controllo ammissibilit? rispetto a Bmin*/
        if(Bres < BMIN) return 0;
        /*controllo pareggio di bilancio energetico*/
        if(i==nSlots-1 && Bres < B_INIT) return 0;
    }
    return 1;
}

void GenerateTasks()
{
 
    unsigned int minQuality,maxQuality;
    tasks[0].c_mAh=1; 
    for(i = 1; i<N_TASKS; i++){
        /*Cost in mAh of the task*/
        tasks[i].c_mAh = ceil(((((float)(i-1.0) / 10.0) * ACTIVE_SYSTEM_CONSUMPTION) +
                            ((1 - (((float) (i-1.0)) / 10.0)) * IDLE_SYSTEM_CONSUMPTION)) * slotDurationPercentage);
    }
    
    tasks[0].q_perc=1;
    tasks[1].q_perc=7;
    for(i = 2; i<N_TASKS-1; i++){
        minQuality=max((i - 1) * 12, tasks[i - 1].q_perc + 7);
        maxQuality= i * 12;
        /*Qualità in livelli di qualità*/
        tasks[i].q_perc = rand() % (maxQuality + 1 - minQuality) + minQuality;
    }
    tasks[N_TASKS-1].q_perc=100;

    for(i = 0; i<N_TASKS; i++)
      printf("Task %d cost per slot(mAh) : %d , qualita : %3d %% \n", i,tasks[i].c_mAh, tasks[i].q_perc);

}

void GeneratePanelProduction(void)
{
  /*Initialize 0 hours with no solar production */
  for (i=0; i<24; i++)
      if((0<=i && i<SUNRISE) || (SUNSET<i && i<=23)) {
          E_h[i] = 0;
      }
  for (i=0; i<K; i++) E_s_mAh[i] = 0;

  /*Energy daily solar production in mAh*/
  // August
  //E_h[7] = 3; E_h[8] = 45; E_h[9] = 133; E_h[10] = 215; E_h[11] = 285; E_h[12] = 327; E_h[13] = 339; E_h[14] = 322; E_h[15] = 255; E_h[16] = 60; E_h[17] = 66; E_h[18] = 63; E_h[19] = 23; E_h[20] = 9;

  // September
  //E_h[8] = 24; E_h[9] = 107; E_h[10] = 202; E_h[11] = 270; E_h[12] = 313; E_h[13] = 316; E_h[14] = 310; E_h[15] = 251; E_h[16] = 98; E_h[17] = 45; E_h[18] = 37; E_h[19] = 15; E_h[20] = 2; E_h[20] = 9;

  // October
  E_h[8] = 19; E_h[9] = 110; E_h[10] = 224; E_h[11] = 285; E_h[12] = 335; E_h[13] = 350; E_h[14] = 331; E_h[15] = 283; E_h[16] = 134; E_h[17] = 20; E_h[18] = 18; E_h[19] = 8; 
}

void InitializeEnergyHarvested(void)
{
    for(i=0;i<24;i++){
            /* 520 is the maximum current in mAh supplied by the solar panel */
            E_h_v[i] = min((int)E_h[i] ,520);
            for (j=0; j<(K/24); j++) {
                pos = (i * K / 24) + j;
                E_s_mAh[pos] = (int)(E_h_v[i] * slotDurationPercentage);
            }
        }
}

void PrintParameters(void)
{
  
  printf("Ordine di stampa:\nQoS(lvl) , QoS (%%), QoS Test, B_res(i) (mAh), Test amm. (mAh), Scheduling(i)\n");
  printf("Parametri:\nK: %d\n N_TASKS: %d\n IDLE_SYSTEM_CONSUMPTION: %d\n ACTIVE_SYSTEM_CONSUMPTION: %d\n MAX_OVERPRODUCTION: %d\n"
         " MAX_UNDERPRODUCTION: %d\n SUNSET: %d\n SUNRISE: %d\n BATTERY_SAMPLING: %d\n SEED: %d\n N_ITERATION: %d\n VARIATION:%d\n",K,N_TASKS,IDLE_SYSTEM_CONSUMPTION,ACTIVE_SYSTEM_CONSUMPTION,MAX_OVERPRODUCTION,MAX_UNDERPRODUCTION,SUNSET,SUNRISE,BATTERY_SAMPLING,SEED,N_ITERATION,VARIATION);
  printf("BMAX: %4d (mAh) , BMIN: %4d (mAh) , BINIT: %d (mAh) \n", (int)BMAX,(int)BMIN,(int)B_INIT);
 
  //printf("num_it,TotalHarvestedEnergy (mAh),QoS,QoS(%%),QoS Test,");
  //for(i=0; i<=K; i++) printf("B(%d)(mAh),",i);
  //printf("Amm. Test (mAh),");
  //for(i=1; i<=K; i++) printf("S(%d),",i);
}




void setup() 
{
    Serial.begin(9600);
    srand(SEED);
    GenerateTasks();
    GeneratePanelProduction();
    InitializeEnergyHarvested();
    PrintParameters();
}

int optQ;                   // Optimal Quality of Scheduling
int totalQualityPerc;       // Sum of percentage quality for Scheduling 
int totalEnergyHarvested;   // Total Energy Harvested in mAh 

int counter=0;              // iteration counter
unsigned long t1,t2;        // Timers to measure execution time, t exec

void initializeMatrixS(uint8_t S[K][BATTERY_SAMPLING+1],int A, int B){
    for(i=0;i<A;i++){
        for(j=0;j<B+1;j++){
            S[i][j] = 0;
        }
    }    
}

void initializeMatrixQ(int Q[2][BATTERY_SAMPLING+1],int A, int B){
    for(i=0;i<A;i++){
        for(j=0;j<B+1;j++){
            Q[i][j] = 0;
        }
    }    
}

void loop() {
    
  if (counter < N_ITERATION)
  {
      //PredictEnergy();        /* This function is commented since we are using the real production data */
      printf("Iteration %d:\n",counter);
      
      totalEnergyHarvested = 0;
      for(i=0; i<K; i++) totalEnergyHarvested += E_s_mAh[i];
      printf("\tEnergyHarvested = %4d\n", totalEnergyHarvested);
       
      
      initializeMatrixS(S,K,BATTERY_SAMPLING);
      initializeMatrixQ(Q,2,BATTERY_SAMPLING);

      t1 = millis();
      //optQ = schedule(N_TASKS, K, BinitL, E_s_mAh, S, Q, tasks, mAh_per_lvl);
      optQ = schedule(BinitL, E_s_mAh);   
      scheduleTasks(S, NS, K, BinitL, tasks, E_s_mAh, mAh_per_lvl);

      t2 = millis();
      
      if (optQ != -1) {
          printf("\tOptimalQuality = %4d\n", optQ);
          for(i=0; i<K-1; i++) printf("%d,",NS[i]-1);
          printf("%d]\n",NS[K-1]-1);
      
          totalQualityPerc = 0;
          for (i = 0; i < K; i++) totalQualityPerc += tasks[NS[i] - 1].q_perc;
          printf("\tTotalQPercentage = %3f\n", (float)totalQualityPerc / K);
          
          if (checkQuality(NS,optQ,tasks)) 
              printf("Schedule OK\n"); 
          else
              printf("Schedule NO\n");
          
          if (checkFeasibility(K, NS, tasks, E_s_mAh))
              printf("Feasible OK\n"); 
          else
              printf("Feasible NO\n");
          
      } else {
          t2 = millis();
          printf("No Admissible Solution\n");
      }
      
      printf("Time = %li\n",t2-t1);
      counter++;
  }
}
