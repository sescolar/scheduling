
/* 
 * Rule for code: never use a block comment inside another block comment
 */


#define K 264            // multiple of 24 ?? why ??
#define N_TASKS 20
#define MONTH   5        // [0,11] Select the month         
#define HOURS  24
// The discussion below on consumption of Arduino strongly depends on the hardware

/* Consumo Arduino:
 * 40mAh IDLE
 * 90mAh ACTIVE
 *Consumo modulo WIFI ESP8266EX
 * 340mAh TX 802.11b, CCK 11Mbps, POUT=+17 dBm (Siccome nel datasheet è considerato 50% duty cycle
 * 100mAh Rx 802.11b, 1024 bytes packet length , –80 dBm (Stessa motivazione)
 * è stato considerato che il modulo WIFI stia acceso il 20% del duty cycle dell'intero sistema alternando
 * in parti uguali ricezione e trasmissione
 * Ossia si ha un costo orario : 22*(1-dc)+95*dc+340*dc/10+100*dc/10 = 22*(1-dc)+90*dc+440/10*dc =
 * 22*(1-dc)+124*dc (mAh)
 */


#define ACTIVE_SYSTEM_CONSUMPTION 124
#define IDLE_SYSTEM_CONSUMPTION 22
#define BATTERY_SAMPLING 512

// Massima sovraproduzione,sottoproduzione di energy harvesting

#define MAX_OVERPRODUCTION 20
#define MAX_UNDERPRODUCTION 40

#define BMAX 2600       // Values in mAh
#define BMIN 260        // 10% of BMAX 

#define B_INIT 1170     // why circa [Bmax − Bmin]/2) so low?? we can start with 90% of BMAX

#define N_ITERATION 100
#define SEED 123

#define VARIATION 0     // 1 sse we change the harvesting energy curve

struct Task {
    int q_perc;
    unsigned int c_mAh;
};

struct Task tasks[N_TASKS];

unsigned int E_h[24];       // Energy harvested oraria
unsigned int E_h_v[24];     // Energy harvested oraria successiva a variazioni percentuali
unsigned int E_s_mAh[K];    // Energy harvested finale per slot in mAh

uint8_t S[K][BATTERY_SAMPLING+1];     // DP: Scheduling Table 
int Q[2][BATTERY_SAMPLING+1];         // DP: Quality Table
uint8_t NS[K];                        // Final Scheduling 
uint8_t i,j,l;                        // Iterators

struct MonthProduction { 
    int sunrise;
    int sunset;
    int prod[HOURS];  // mAh
};

struct MonthProduction Mproduction; // Sunrise, sunset and energy production for a specific month


/*Percentuale oraria di durata di uno slot*/
const float slotDurationPercentage = 24 / (float) K;

/*mAh corrispondenti a un livello di campionamento*/
const int mAh_per_lvl = (float)BMAX/BATTERY_SAMPLING;

/*Livello iniziale di batteria*/
const unsigned int BinitL = (float)B_INIT / ((float)BMAX/BATTERY_SAMPLING);

int schedule(uint8_t nTasks, 
             uint8_t nSlots,
             unsigned int BinitL, 
             unsigned int E[K],
             uint8_t S[K][BATTERY_SAMPLING + 1],
             int Q[2][BATTERY_SAMPLING + 1], 
             struct Task tasks[N_TASKS], 
             int mAh_per_lvl)
{
    int qmax,q;
    int b,Br,b_mAh;
    uint8_t idmax,t;

    for(int i = nSlots-1; i>=0 ;i--){
        for(b=0 ;b<BATTERY_SAMPLING+1; b++){
            qmax=-1;
            idmax=0;
            /*mAh corrispondente al livello di batteria corrente approssimato per difetto*/
            b_mAh = b * mAh_per_lvl;
            for(t=0;t<nTasks;t++){
                Br = min(b_mAh - tasks[t].c_mAh + E[i], BMAX);
                if(i == nSlots-1 && (Br>=B_INIT) && qmax < tasks[t].q_perc){
                    qmax = tasks[t].q_perc;
                    idmax = t+1;
                }
                else{
                    if(i!=(int)nSlots-1) {
                        if (Br >= BMIN) {
                            /*Considero allo slot successivo un livello di batteria approssimato per difetto per garantire l'ammissibilità*/
                            
                            
                            q = Q[(i + 1)%2][Br/mAh_per_lvl];
                        //  printf("Q[%d][%d]=%d \n",(i+1)%2,Br/mAh_per_lvl,q);
                            
                            if (q != -1 && (q + tasks[t].q_perc) > qmax) {
                                qmax = q + tasks[t].q_perc;
                                idmax = t+1;
                            }
                        }
                    }
                }
            }
            Q[i%2][b] = qmax;
            /*Contiene l'indice del task +1 per usare unsigned int*/
            S[i][b] = idmax;
        }
    }
    if (nSlots%2==0)
        return Q[nSlots%2][BinitL];
    else
        return Q[(nSlots+1)%2][BinitL];
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

/*Controlla l'ammissibilità della soluzione trovata usando batteria in mAh*/
int checkFeasibility(uint8_t nSlots, uint8_t *NS, struct Task *tasks, unsigned int *E){
    int Bres = B_INIT;
    //printf("%d,",Bres);
    for(int i = 0; i<nSlots; i++){
        Bres = min(Bres - tasks[NS[i]-1].c_mAh + E[i], BMAX);
        //printf("%d,",Bres);
        /*controllo ammissibilità rispetto a Bmin*/
        if(Bres < BMIN) return 0;
        /*controllo pareggio di bilancio energetico*/
        if(i==nSlots-1 && Bres < B_INIT) return 0;
    }
    return 1;
}

void GenerateTasks(void)
{
  tasks[0].c_mAh  = 0;   // <- wrong for me.
  tasks[0].q_perc = 0;
  
  for (i=1; i<N_TASKS; i++){
      /*Costo in mAh dei task*/
      tasks[i].c_mAh = ceil(((((float)(i-1) / 10) * ACTIVE_SYSTEM_CONSUMPTION) + 
                       ((1 - (((float)(i-1))/ 10)) * IDLE_SYSTEM_CONSUMPTION)) * slotDurationPercentage);
  }
  /*Qualità massima e minima che si può assegnare al task dell'iterazione corrente*/
  unsigned int minQuality,maxQuality;
  
  tasks[1].q_perc = 7;
  for(i = 2; i<N_TASKS-1; i++){
      /*Devono essere distanziata almeno di 5, per non collidere*/
      minQuality = max((i-1)*12, tasks[i-1].q_perc + 7);
      maxQuality = i * 12;
      /* Qualità in livelli di qualità */
      tasks[i].q_perc = rand() % (maxQuality + 1 - minQuality) + minQuality;
  }
  tasks[N_TASKS-1].q_perc = 100;
  for(i = 0; i<N_TASKS; i++)
      printf("Task %d costo x slot(mAh) : %2d , qualita : %3d %% \n",
              i,tasks[i].c_mAh, tasks[i].q_perc);
}

// Soledad, this function is a real mess, please remove unused data

void GeneratePanelProduction(void)
{
  switch(MONTH){
    case 5: Mproduction.sunrise = 5;
            Mproduction.sunset = 19;
            // review the numbers
            Mproduction.prod[5] = 75;
            Mproduction.prod[6] = 256; 
            Mproduction.prod[7] = 436; 
            Mproduction.prod[8] = 520; 
            Mproduction.prod[9] = 520;
            Mproduction.prod[10] = 520; 
            Mproduction.prod[11] = 520; 
            Mproduction.prod[12] = 520; 
            Mproduction.prod[13] = 520; 
            Mproduction.prod[14] = 520; 
            Mproduction.prod[15] = 520; 
            Mproduction.prod[16] = 520; 
            Mproduction.prod[17] = 436; 
            Mproduction.prod[18] = 256;
            Mproduction.prod[19] = 75;

    case 9: Mproduction.sunrise = 7;
            Mproduction.sunset = 17;
            Mproduction.prod[7] = 38; 
            Mproduction.prod[8] = 118; 
            Mproduction.prod[9] = 186;
            Mproduction.prod[10] = 239; 
            Mproduction.prod[11] = 272; 
            Mproduction.prod[12] = 283; 
            Mproduction.prod[13] = 272; 
            Mproduction.prod[14] = 239; 
            Mproduction.prod[15] = 186; 
            Mproduction.prod[16] = 118; 
            Mproduction.prod[17] = 38; 
                
    case 11:  Mproduction.sunrise = 8;
              Mproduction.sunset = 16;
              Mproduction.prod[8] = 22; 
              Mproduction.prod[9] = 57;
              Mproduction.prod[10] = 84; 
              Mproduction.prod[11] = 100; 
              Mproduction.prod[12] = 106; 
              Mproduction.prod[13] = 100; 
              Mproduction.prod[14] = 84; 
              Mproduction.prod[15] = 57; 
              Mproduction.prod[16] = 22; 
             
    default: break;
  }  
  /*Azzero le ore notturne*/
  for (i=0; i<HOURS; i++)
      if((0<=i && i<SUNRISE) || (SUNSET<i && i<=23)) {
          Mproduction.prod[i] = 0;
      }
}
void PrintParameters(void)
{
  
  printf("Ordine di stampa:\nQoS(lvl) , QoS (%%), QoS Test, B_res(i) (mAh), Test amm. (mAh), Scheduling(i)\n");
  printf("Parametri:\nK: %d\n N_TASKS: %d\n IDLE_SYSTEM_CONSUMPTION: %d\n ACTIVE_SYSTEM_CONSUMPTION: %d\n MAX_OVERPRODUCTION: %d\n"
         " MAX_UNDERPRODUCTION: %d\n SUNSET: %d\n SUNRISE: %d\n BATTERY_SAMPLING: %d\n SEED: %d\n N_ITERATION: %d\n VARIATION:%d\n",K,N_TASKS,IDLE_SYSTEM_CONSUMPTION,ACTIVE_SYSTEM_CONSUMPTION,MAX_OVERPRODUCTION,MAX_UNDERPRODUCTION,SUNSET,SUNRISE,BATTERY_SAMPLING,SEED,N_ITERATION,VARIATION);
  printf("BMAX: %4d (mAh) , BMIN: %4d (mAh) , BINIT: %4d (mAh) \n", BMAX,BMIN,B_INIT);
 
//  printf("num_it,TotalHarvestedEnergy (mAh),QoS,QoS(%%),QoS Test,");
//  for(i=0; i<=K; i++) printf("B(%d)(mAh),",i);
//  printf("Amm. Test (mAh),");
//  for(i=1; i<=K; i++) printf("S(%d),",i);
}

void setup() 
{
#ifndef DESKTOP
  Serial.begin(9600);
#endif
  srand(SEED);
  GenerateTasks();
  GeneratePanelProduction();
  PrintParameters();
}

int optQ;                   // Optimal Quality of Scheduling
int totalQualityPerc;       // Sum of percentage quality for Scheduling 
int totalEnergyHarvested;   // Total Energy Harvested in mAh 

uint8_t coin,pos;
int variation;              /*Variazione percentuale di energy harvesting oraria*/
int counter=0;              // iteration counter
unsigned long t1,t2;        // Timers to measure execution time, t exec


void PredictEnergy()
{
  for(i=0; i<24; i++) {
    if (SUNRISE <= i && i <= SUNSET) {
        /*Utilizzato per avere una curva senza variazioni*/
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
        /* 520 è la corrente massima in mAh fornita dal pannello solare */
        E_h_v[i] = min((int)(E_h[i] * (float)variation / 100),520);
        for (j=0; j<(K/24); j++) {
            pos = (i * K / 24) + j;
            E_s_mAh[pos] = (int)(E_h_v[i] * slotDurationPercentage);
        }
    }
  }
}

void loop() {
  if (counter < N_ITERATION)
  {
      PredictEnergy();
      
      printf("Iteration %d:\n",counter);
      
      totalEnergyHarvested = 0;
      for(i=0; i<K; i++) totalEnergyHarvested += E_s_mAh[i];
      printf("\tEnergyHarvested = %4d\n", totalEnergyHarvested);
      
      t1 = millis();
      optQ = schedule(N_TASKS, K, BinitL, E_s_mAh, S, Q, tasks, mAh_per_lvl);
      printf("\tOptimalQuality = %d\n", optQ);
      
      if (optQ != -1) {
          scheduleTasks(S, NS, K, BinitL, tasks, E_s_mAh, mAh_per_lvl);
          printf("\tScheduling: [");
          for(i=0; i<K-1; i++) printf("%d,",NS[i]-1);
          printf("%d]\n",NS[K-1]-1);
      
          t2 = millis();
          totalQualityPerc = 0;
          for (i = 0; i < K; i++) totalQualityPerc += tasks[NS[i] - 1].q_perc;
          printf("\tTotalQPercentage = %f\n", (float)totalQualityPerc / K);
          
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
          printf("No Solutions\n");
      }
      
      printf("Time = %li\n",t2-t1);
      counter++;
  }
}
