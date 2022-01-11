/* 
 * Updated: Sole
 * January 2022
 * Rule for code: never use a block comment inside another block comment
 */

#include <Arduino.h>
#define K 48            // multiple of 24 ?? why ??
#define N_TASKS 10


// The discussion below on consumption of Arduino strongly depends on the hardware

/* Consumo Arduino:
 * 40mAh IDLE
 * 90mAh ACTIVE
 *Consumo modulo WIFI ESP8266EX
 * 340mAh TX 802.11b, CCK 11Mbps, POUT=+17 dBm (Siccome nel datasheet ? considerato 50% duty cycle
 * 100mAh Rx 802.11b, 1024 bytes packet length , –80 dBm (Stessa motivazione)
 * ? stato considerato che il modulo WIFI stia acceso il 20% del duty cycle dell'intero sistema alternando
 * in parti uguali ricezione e trasmissione
 * Ossia si ha un costo orario : 22*(1-dc)+95*dc+340*dc/10+100*dc/10 = 22*(1-dc)+90*dc+440/10*dc =
 * 22*(1-dc)+124*dc (mAh)
 */


#define ACTIVE_SYSTEM_CONSUMPTION 124
#define IDLE_SYSTEM_CONSUMPTION 22
#define MAX_QUALITY_LVL 50

// Max. over/underproduction 
#define MAX_OVERPRODUCTION 20
#define MAX_UNDERPRODUCTION 40

// Sunset and sunrise in August
/*#define SUNSET 20
#define SUNRISE 7
*/

// Sunset and sunrise in September
/*#define SUNSET 20
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
    int q_lvl;
    unsigned int c_mAh;
};

struct Task tasks[N_TASKS];
unsigned int E_h[24];       // Hourly Energy harvested 
unsigned int E_h_v[24];     // HOurly Energy harvested uccessiva a variazioni percentuali
unsigned int E_s_mAh[K];    // Final Energy harvested per slot in mAh
const float slotDurationPercentage = 24 / (float) K;    /*Porcentaje horario de la duración de las franjas horarias*/
uint8_t pos;

uint8_t* S[K];                          // DP: Scheduling Table 
unsigned int B[2][K*MAX_QUALITY_LVL+1];           // DP: Quality Table
uint8_t NS[K];                          // Final Scheduling 
int i,j,l;                              // Iterators

int optQ;                   // Optimal Quality of Scheduling
int totalQualityPerc;       // Sum of percentage quality for Scheduling 
int totalEnergyHarvested;   // Total Energy Harvested in mAh 
/*Valore ottimo di qualità rispetto al solo vincolo di B_min senza pareggio di bilancio*/
int relaxedOptQ;
int counter=0;              // iteration counter
unsigned long t1,t2;        // Timers to measure execution time, t exec



/*L'upperBound al valore ottimo: Ossia l'ottimo risolto solo rispetto al vincolo di Bmin*/
int schedule(unsigned int nTasks, unsigned int nSlots, unsigned int maxTaskQualityLevel,
             unsigned int E[K] , uint8_t** S , unsigned int B[2][(K*MAX_QUALITY_LVL)+1], struct Task tasks[N_TASKS]) {

    int currentBmax = 0, maxQualityPreviousSlot, maxQualityCurrentSlot;
    uint8_t idMax = 0; 
    uint8_t t,s;
    int q,Bprec,Br;

    for(s = 0; s<nSlots ; s++){
        /*La qualità massima corrente va resettata ad ogni ciclo, siccome passando da uno slot ad un altro potrebbe
         *non essere possibile schedulare nessun task per nessuna qualità perchè nessun task può essere aggiunto stando sopra Bmin*/
        maxQualityCurrentSlot=-1;
        if(s == 0){
            for(q=0; q <= (int)maxTaskQualityLevel ; q++){
                currentBmax = 0;
                idMax = 0;
                for(t=0; t<nTasks; t++){
                    Br = B_INIT + E[0] - tasks[t].c_mAh;
                    if(tasks[t].q_lvl == q && Br >= BMIN && Br >= currentBmax){
                        currentBmax = Br;
                        idMax = t+1;
                        /*Assegno sempre q_lvl a maxQualityCurrentSlot tanto q_lvl cresce sempre in una singola iterazione esterna*/
                        maxQualityCurrentSlot = q;
                    }
                }
                B[0][q] = min(currentBmax,BMAX);
                S[0][q] = idMax;
            }
        }
        else{
            for(q = 0; q <= maxQualityPreviousSlot + (int)maxTaskQualityLevel ; q++){
                currentBmax = 0;
                idMax = 0;
                for(t=0; t< nTasks; t++){
                    /*Se non accedo in posizioni negative
                     *e se non accedo in posizioni non visitate (Quindi con valori non inizializzati) perchè sicuramente non ammissibili*/
                    if(q>=tasks[t].q_lvl && (q - tasks[t].q_lvl) <= (maxQualityPreviousSlot) &&
                       ((Bprec = B[(s-1)%2][q-tasks[t].q_lvl]) != 0 ) && ((Br = Bprec + E[s] - tasks[t].c_mAh) >= BMIN) && Br > currentBmax){
                        currentBmax = Br;
                        idMax = t+1;
                        /* Devo aggiornare SEMPRE non solo se maggiore perchè magari la qualità dallo slot K allo slot K+1 non è più raggiungibile
                         * per mancata ammissibilità del medesimo scheduling con l'aggiunta del task con qualità nulla, ad esempio allo slot K ho Bres=Bmin
                         * E[K+1] = 0 quindi non posso schedulare nessun task quindi la nuova qualità ottima va cercata per valori inferiori a quella precedente
                         * All'interno dello slot la qualità è scorsa in modo crescente quindi non serve il controllo*/
                        maxQualityCurrentSlot=q;
                    }
                }
                B[s%2][q] = min(currentBmax,BMAX);
                S[s][q] = idMax;
            }
        }

        maxQualityPreviousSlot = maxQualityCurrentSlot;
        /*Se allo slot precedente s non ho nessuna qualità massima (Nessuno scheduling ammissibile)
         *significa che non esistono scheduling lunghi K>s ammissibili quindi posso terminare dicendo che non esistono scheduling ammissibili rispeto a Bmin*/
        if(maxQualityPreviousSlot == -1)
            return -1;
    }
    printf("\tEnd of schedule\n");
    return maxQualityCurrentSlot;
}

/*B ultima colonna della matrice*/
/*Ritorna la qualità ottima e costruisce l'array dello scheduling ottimo*/
int solution(unsigned int B [(K*MAX_QUALITY_LVL)+1], uint8_t** ST, uint8_t nSlots,
             uint8_t S[K], struct Task tasks[N_TASKS], int qUpperBound){

    /*B[qUpperBound] == 0 corrisponde a scheduling non ammissibile, questo è stato fatto per evitare numeri negativi al fine di usare una tabella di uint8_t
     *Il caso potrebbe già essere coperto da B[qUpperBound]>=Binit però nel caso di Binit=0 non lo è*/
    while(qUpperBound >= 0 && !(B[qUpperBound] >= B_INIT && B[qUpperBound] != 0)){
        //printf("Batteria finale: %d\n", B[qUpperBound]);
        qUpperBound--;
    }
    int t = qUpperBound;
    if(qUpperBound != -1){
        for(int s=nSlots-1;s>=0;s--){
            /*Task schedulato con indici che partono da 1, quindi nella stampa partiranno da 1
             *Sotto faccio S[s]-1 per prendere la qualità del task corretto*/
            S[s] = ST[s][t];
            /*Non può uscire dalla memoria perchè se c_lvl'è un task a -1 => qUpperBound sarà -1*/
            t -= tasks[S[s]-1].q_lvl;
        }
    }
    return qUpperBound;
}

int checkQuality(uint8_t NS[K],unsigned int q,struct Task tasks[N_TASKS]){
    int sumQ=0;
    for(int i=0;i<K;i++){
        sumQ+=tasks[NS[i]-1].q_lvl;
    }
    return q == sumQ;
}

int checkBres(int optQ, unsigned int B [(K*MAX_QUALITY_LVL)+1], uint8_t nSlots, uint8_t NS[K], struct Task tasks[N_TASKS]
              ,unsigned int E[K]){

    int Bres = B_INIT;
    for(int i = 0; i<nSlots; i++){
        Bres = min(Bres - tasks[NS[i]-1].c_mAh + E[i], BMAX);
    }
    if(optQ>=0) return (B[optQ]==Bres);
    else return 0;
}
/*Va richiamato con valori in mAh*/
int checkFeasibility(uint8_t nSlots, uint8_t *NS, struct Task *tasks, unsigned int *E){
    int Bres = B_INIT;
    printf("%d,",Bres);
    for(int i = 0; i<nSlots; i++){
        //printf("Bres %f costo: %f E[i]: %f Bmax: %f\n",Bres,tasks[NS[i]-1].c_mAh,E[i],Bmax);
        Bres = min(Bres - tasks[NS[i]-1].c_mAh + E[i], BMAX);
        printf("%d,",Bres);
        /*controllo ammissibilità rispetto a Bmin*/
        if(Bres < BMIN) return 0;
        /*controllo pareggio di bilancio energetico*/
        if(i==nSlots-1 && Bres < B_INIT) return 0;
    }
    return 1;
}


void GenerateTasks()
{
    unsigned int minQuality,maxQuality;
    unsigned int step;
    tasks[0].c_mAh=1; 
    for(i = 1; i<N_TASKS; i++){
        /*Cost in mAh of the task*/
        tasks[i].c_mAh = ceil(((((float)(i-1.0) / 10.0) * ACTIVE_SYSTEM_CONSUMPTION) +
                            ((1 - (((float) (i-1.0)) / 10.0)) * IDLE_SYSTEM_CONSUMPTION)) * slotDurationPercentage);
    }
    
    step= 100/N_TASKS;
    tasks[0].q_perc=1;
    for(i = 1; i<N_TASKS-1; i++){
        minQuality=tasks[i-1].q_perc+1;
        maxQuality= minQuality+step;
        tasks[i].q_perc = random(minQuality+(step/2),maxQuality+1);
        tasks[i].q_lvl = floor(((float)(tasks[i].q_perc)) / ((float)100 / MAX_QUALITY_LVL));
    }
    tasks[N_TASKS-1].q_lvl=MAX_QUALITY_LVL;
    tasks[N_TASKS-1].q_perc=100;

    for(i = 0; i<N_TASKS; i++)
      printf("Task %d cost per slot(mAh) : %d , quality : %d, quality level: %d \n", i,tasks[i].c_mAh, tasks[i].q_perc, tasks[i].q_lvl);

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
         " MAX_UNDERPRODUCTION: %d\n SUNSET: %d\n SUNRISE: %d\n QUALITY_LEVEL: %d\n SEED: %d\n N_ITERATION: %d\n VARIATION:%d\n",K,N_TASKS,IDLE_SYSTEM_CONSUMPTION,ACTIVE_SYSTEM_CONSUMPTION,MAX_OVERPRODUCTION,MAX_UNDERPRODUCTION,SUNSET,SUNRISE,MAX_QUALITY_LVL,SEED,N_ITERATION,VARIATION);
    printf("BMAX: %4d (mAh) , BMIN: %4d (mAh) , BINIT: %d (mAh) \n", (int)BMAX,(int)BMIN,(int)B_INIT);
}


void GetMatrixS(void){
 for (i = 0; i< K; i++)
    S[i] = (uint8_t*) malloc(sizeof(uint8_t)*(((i+1)*MAX_QUALITY_LVL+1)));
}

void setup() 
{
    Serial.begin(9600);
    srand(SEED);
    GetMatrixS();
    GenerateTasks();
    GeneratePanelProduction();
    InitializeEnergyHarvested();
    PrintParameters();
}

void initializeMatrixS(uint8_t** S,int A, int B){
    for(i=0;i<A;i++){
        for(j=0;j<B+1;j++){
            S[i][j] = 0;
        }
    }    
}

void initializeMatrixB(unsigned int B[2][K*MAX_QUALITY_LVL+1],int A, int C){
    for(i=0;i<A;i++){
        for(j=0;j<C+1;j++){
            B[i][j] = 0;
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
       
      initializeMatrixS(S,K,MAX_QUALITY_LVL);
      initializeMatrixB(B,2,K*MAX_QUALITY_LVL);
      
      t1 = millis();
      relaxedOptQ = schedule(N_TASKS, K, MAX_QUALITY_LVL, E_s_mAh, S, B, tasks);
      printf("\tRelaxedOptQ=%d\n",relaxedOptQ);
      optQ = solution(B[(K-1)%2],S,K,NS,tasks,relaxedOptQ);
      t2=millis();
      
      if (optQ != -1) {
          totalQualityPerc = 0;
          printf("%d,",optQ);
          for (i = 0; i < K; i++){
              totalQualityPerc += tasks[NS[i] - 1].q_perc;
              printf("quality level=%d\n",tasks[NS[i] - 1].q_perc);
          } 
          printf("\tTotalQPercentage = %f\n", (float)(totalQualityPerc /(float) K));
          
          if (checkQuality(NS,optQ,tasks)) 
              printf("Schedule OK\n"); 
          else
              printf("Schedule NO\n");
          if(checkBres(optQ, B[(K-1)%2], K, NS, tasks, E_s_mAh)) 
              printf("OK Bres,");
          else 
              printf("KO Bres,");
          if (checkFeasibility(K, NS, tasks, E_s_mAh))
              printf("Feasible OK\n"); 
          else
              printf("Feasible NO\n");
          for(i=0;i<K-1;i++)
                printf("%d,",NS[i]-1);
            printf("%d,",NS[K-1]-1);
      } else {
          printf("No Admissible Solution\n");
      }
      
      printf("Time = %li\n",t2-t1);
      counter++;
  }
}
