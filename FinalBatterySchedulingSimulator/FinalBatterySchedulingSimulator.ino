
/* 
 * Rule for code: never use a block comment inside another block comment
 */


#define K 24            // multiple of 24 ?? why ??
#define N_TASKS 20

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


// Added for GenerateSolarProduction
#define HOURS         24
#define MONTHS        12
#define LATITUDE      40.42    // latitude in Madrid
#define PINUMBER      3.1415926535897932384626433832795
#define DEGREES       23.45
#define SLOTSPERHOUR  (K/24)
#define STEPS         (SLOTSPERHOUR * HOURS)
#define ETA           0.128        // energy ETA of the solar cell
#define VLD           5.82         // specific voltage of load
#define SURFACE       0.034875     // surface of the solar cell (m2)

uint8_t daysPerMonth[MONTHS] = {31,28,31,30,31,30,31,31,30,31,30,31}; //days per month
float H[STEPS];
float cos_z[STEPS];

struct MonthProduction { 
    unsigned int sunrise;
    unsigned int sunset;
    uint8_t days;
    unsigned int irradiance;   // KWh/m2/day
    double prod[STEPS];  // wh
};

struct MonthProduction Mproduction; // Sunrise, sunset, irradiance, and energy production for a specific month


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
float deg2rad(float degrees){
  return ((degrees * 71) / 4068);
}

void zenit(uint8_t day, uint8_t month){
  float delta;
  float c1;
  unsigned int accumDays = 0;
  float next = 0.0;  
  unsigned int n;

  accumDays = day;
  for (i=0;i<month;i++){
      accumDays = accumDays + daysPerMonth[i];
  }
  
  delta = deg2rad(DEGREES) * sin(2*PI*((accumDays+284)/366));
  c1 = ((((4*-deg2rad(3.7038))*100)/60)/100);

  for (i=0;i<STEPS;i++){
      H[i] =  deg2rad(15 * (next + c1 - 12.0));
      
      printf("%2.2f\t",H[i]);
      next = next + (1/SLOTSPERHOUR);
  }
  printf("\n");
  next = 0.0;
  for (i=0;i<STEPS;i++){
      cos_z[i] = sin(deg2rad(LATITUDE)) * sin(delta) + cos(deg2rad(LATITUDE)) * cos(delta) * cos(H[i]);
      printf("%2.2f\t",cos[i]);
      next = next + (1/SLOTSPERHOUR);
  }
  printf("\n");
}

void GeneratePanelProduction(uint8_t month)
{
//DIrradiance = (2030.0,2960.0,4290.0,5110.0,5950.0,7090.0,7200.0,6340.0,4870.0,3130.0,2130.0,1700.0)
  int accumDays = 0;
  switch(month){
    case 0: Mproduction.sunrise = 8;
            Mproduction.sunset = 16;
            Mproduction.irradiance = 2030;
            
    case 1: Mproduction.sunrise = 8;
            Mproduction.sunset = 16;
            Mproduction.irradiance = 2960;
            
    case 2: Mproduction.sunrise = 8;
            Mproduction.sunset = 16;
            Mproduction.irradiance = 4290;
            
    case 3: Mproduction.sunrise = 8;
            Mproduction.sunset = 18;
            Mproduction.irradiance = 5110;
            
    case 4: Mproduction.sunrise = 7;
            Mproduction.sunset = 18;
            Mproduction.irradiance = 5950;

    case 5: Mproduction.sunrise = 7;
            Mproduction.sunset = 20;
            Mproduction.irradiance = 7090;

    case 6: Mproduction.sunrise = 7;
            Mproduction.sunset = 20;
            Mproduction.irradiance = 7200;
            
    case 7: Mproduction.sunrise = 7;
            Mproduction.sunset = 20;
            Mproduction.irradiance = 6340;
            
    case 8: Mproduction.sunrise = 7;
            Mproduction.sunset = 19;
            Mproduction.irradiance = 4870;
            
    case 9: Mproduction.sunrise = 8;
            Mproduction.sunset = 17;
            Mproduction.irradiance = 3130;
            
    case 10: Mproduction.sunrise = 8;
            Mproduction.sunset = 16;
            Mproduction.irradiance = 2130;             
    case 11:  
            Mproduction.sunrise = 8;
            Mproduction.sunset = 16;
            Mproduction.irradiance = 1700; 
             
    default: break;
  }
  Mproduction.days = daysPerMonth[i]; 
  zenit(1, month);
  for(i=0;i<STEPS;i++){
        Mproduction.prod[i] = cos_z[i] *  Mproduction.irradiance * ETA * SURFACE; 
  }
}
void PrintParameters(void)
{
  
  printf("Ordine di stampa:\nQoS(lvl) , QoS (%%), QoS Test, B_res(i) (mAh), Test amm. (mAh), Scheduling(i)\n");
  printf("Parametri:\nK: %d\n N_TASKS: %d\n IDLE_SYSTEM_CONSUMPTION: %d\n ACTIVE_SYSTEM_CONSUMPTION: %d\n MAX_OVERPRODUCTION: %d\n"
         " MAX_UNDERPRODUCTION: %d\n SUNSET: %d\n SUNRISE: %d\n BATTERY_SAMPLING: %d\n SEED: %d\n N_ITERATION: %d\n VARIATION:%d\n",K,N_TASKS,IDLE_SYSTEM_CONSUMPTION,ACTIVE_SYSTEM_CONSUMPTION,MAX_OVERPRODUCTION,MAX_UNDERPRODUCTION,Mproduction.sunset,Mproduction.sunrise ,BATTERY_SAMPLING,SEED,N_ITERATION,VARIATION);
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
  GeneratePanelProduction(0);
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
    if (Mproduction.sunrise <= i && i <= Mproduction.sunset) {
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
