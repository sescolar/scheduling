/*Deve essere multiplo di 24!*/
#define K 24
#define N_TASKS 10
/*Consumo Arduino:
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
#define MAX_QUALITY_LVL 100
/*Massima sovraproduzione,sottoproduzione di energy harvesting*/
#define MAX_OVERPRODUCTION 20
#define MAX_UNDERPRODUCTION 40
/*Sunset e sunrise relativi a metà Ottobre*/
#define SUNSET 17
#define SUNRISE 7
/*Sunset e sunrise relativi a metà Giugno
#define SUNSET 19
#define SUNRISE 5
/*Sunset e sunrise relativi a metà Dicembre
#define SUNSET 16
#define SUNRISE 8
/*Valori in mAh*/
#define BMAX 2600
/*10% della capacità della batteria*/
#define BMIN 260
/*circa [Bmax − Bmin]/2)*/
#define B_INIT 1170
#define N_ITERATION 100
#define SEED 123
/*1 sse si ammettono variazioni nella curva di energy harvesting*/
#define VARIATION 1

struct Task {
    uint8_t q_perc;
    uint8_t q_lvl;
    unsigned int c_mAh;
};

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
/*Varie versione dell'applicazione*/
struct Task tasks[N_TASKS];
/*Energy harvested oraria*/
unsigned int E_h[24];
/*Energy harvested oraria successiva a variazioni percentuali*/
unsigned int E_h_v[24];
/*Energy harvested finale per slot in mAh*/
unsigned int E_s_mAh[K];
/*Tabella per prog din. dello scheduling*/
uint8_t* S[K];
/*Tabella per prog. din. livelli di batteria*/
unsigned int B[2][K*MAX_QUALITY_LVL+1];
/*Scheduling contenente ID dei task schedulati*/
uint8_t NS [K];
/*Iteratori*/
uint8_t i,j,l;
/*Durata slot in % rispetto alla durata di 1 ora*/
const float slotDurationPercentage = 24 / (float) K;

void setup() {
  Serial.begin(9600);
  srand(SEED);
  /*Alloco matrice prog dinamica*/
  for (int i = 0; i< K; i++)
    S[i] = (uint8_t*) malloc(sizeof(uint8_t)*(((i+1)*MAX_QUALITY_LVL+1)));
  tasks[0].c_mAh=0;
  for(i = 1; i<N_TASKS; i++){
      /*Costo in mAh dei task*/
      tasks[i].c_mAh = ceil(((((float)(i-1) / 10) * ACTIVE_SYSTEM_CONSUMPTION) + ((1 - (((float) (i-1)) / 10)) * IDLE_SYSTEM_CONSUMPTION)) * slotDurationPercentage);
  }

  /*Qualità massima e minima che si può assegnare al task dell'iterazione corrente*/
  unsigned int minQuality,maxQuality;
  tasks[0].q_perc=0;
  tasks[0].q_lvl=0;
  tasks[1].q_perc=7;
  tasks[1].q_lvl=floor(((float)(tasks[1].q_perc)) / ((float)100 / MAX_QUALITY_LVL));
  for(i = 2; i<N_TASKS-1; i++){
      /*Devono essere distanziata almeno di 5, per non collidere*/
      minQuality=max((i - 1) * 12, tasks[i - 1].q_perc + 7);
      maxQuality= i * 12;
      /*Qualità in livelli di qualità*/
      tasks[i].q_perc = rand() % (maxQuality + 1 - minQuality) + minQuality;
      tasks[i].q_lvl = floor(((float)(tasks[i].q_perc)) / ((float)100 / MAX_QUALITY_LVL));
  }
  tasks[N_TASKS-1].q_lvl=MAX_QUALITY_LVL;
  tasks[N_TASKS-1].q_perc=100;


  for(i = 0; i<N_TASKS; i++)
      printf("Task %d costo x slot(mAh) : %d , qualita : %d %% qualita (lvl) :%d\n", i,tasks[i].c_mAh, tasks[i].q_perc, tasks[i].q_lvl);

  /*Azzero le ore notturne*/
  for(i=0; i<24 ;i++)
    if((0<=i && i<SUNRISE) || (SUNSET<i && i<=23)) E_h[i] = 0;
    
  for(i=0;i<K;i++)
    E_s_mAh[i] = 0;

  /*Inserisco manualmente i valori presi dal foglio di calcolo, valori in mAh*/
  /*I valori fanno riferimenti ad energy harvesting per metà del mese di Ottobre*/
  E_h[7] = 38; E_h[8] = 118; E_h[9] = 186; E_h[10] = 239; E_h[11] = 272; E_h[12] = 283;
  E_h[13] = 272; E_h[14] = 239; E_h[15] = 186; E_h[16] = 118; E_h[17] = 38;
  /*Inserisco manualmente i valori presi dal foglio di calcolo, valori in mAh
  I valori fanno riferimenti ad energy harvesting per metà del mese di Giugno limitati alla massima produzione del pannello
  E_h[5]=75;E_h[6] = 256;
  E_h[7] = 436; E_h[8] = 520; E_h[9] = 520; E_h[10] = 520; E_h[11] = 520; E_h[12] = 520;
  E_h[13] = 520; E_h[14] = 520; E_h[15] = 520; E_h[16] = 520; E_h[17] = 436;
  E_h[18] = 256;E_h[19] = 75;
  /*Inserisco manualmente i valori presi dal foglio di calcolo, valori in mAh
  I valori fanno riferimenti ad energy harvesting per metà del mese di Dicembre
  E_h[8] = 22; E_h[9] = 57; E_h[10] = 84; E_h[11] = 100; E_h[12] = 106;
  E_h[13] = 100; E_h[14] = 84; E_h[15] = 57; E_h[16] = 22;*/
  
  printf("Ordine di stampa:\nQoS(lvl) , QoS (%%), QoS Test, B_res(i) (lvl), Test amm. (lvl), Test B_res(K), B_res(i) (mAh), Test amm. (mAh), Scheduling(i)\n");
  printf("Parametri:\nK: %d N_TASKS: %d IDLE_SYSTEM_CONSUMPTION: %d ACTIVE_SYSTEM_CONSUMPTION: %d MAX_OVERPRODUCTION: %d \n"
         "MAX_UNDERPRODUCTION: %d SUNSET: %d SUNRISE: %d MAX_QUALITY_LVL: %d  SEED: %d N_ITERATION: %d VARIATION:%d \n",
         K,N_TASKS,IDLE_SYSTEM_CONSUMPTION,ACTIVE_SYSTEM_CONSUMPTION,MAX_OVERPRODUCTION,MAX_UNDERPRODUCTION,
         SUNSET,SUNRISE,MAX_QUALITY_LVL,SEED,N_ITERATION,VARIATION);
  printf("BMAX: %d (mAh) , BMIN: %d (mAh) , BINIT: %d (mAh) \n", BMAX,BMIN,B_INIT);
  printf("num_it,TotalHarvestedEnergy (mAh),QoS(lvl),QoS(%%),QoS Test,B_K+1 Test");
  for(i=0;i<=K;i++)
    printf("B_(%d)(mAh),",i);
  printf("Amm. Test (mAh),");
  for(i=1;i<=K;i++)
    printf("S(%d),",i);
  printf("T exec\n");

}
/*Valore ottimo di qualità trovato*/
int optQ;
/*Valore ottimo di qualità rispetto al solo vincolo di B_min senza pareggio di bilancio*/
int relaxedOptQ;
/*Somma delle qualità percentuali dello scheduling trovato*/
int totalQualityPerc;
/*Energia di harvesting totale in mAh*/
int totalEnergyHarvested;

uint8_t coin,pos;
/*Variazione percentuale di energy harvesting oraria*/
int variation;
/*Contatore numero iterazione corrente*/
int counter=0;
/*Timer per t exec*/
unsigned long t1,t2;

void loop() {
  if(counter<N_ITERATION){
    for(i=0;i<24;i++){
            if(SUNRISE<= i && i <=SUNSET){
                /*Utilizzato per avere una curva senza variazioni*/
                if(!VARIATION) coin = 2;
                else coin = rand() % 3;
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
                /*520 è la corrente massima in mAh fornita dal pannello solare*/
                E_h_v[i] = min((int)(E_h[i] * (float)variation / 100),520);

                for(j=0;j<(K/24);j++) {
                    pos = (i * K / 24) + j;
                    E_s_mAh[pos] = (int)(E_h_v[i] * slotDurationPercentage);
                }
            }
        }
        printf("%d,",counter);
        totalEnergyHarvested=0;
        for(i=0;i<K;i++)
            totalEnergyHarvested+=E_s_mAh[i];
        printf("%d,", totalEnergyHarvested);
        t1=millis();
        relaxedOptQ = schedule(N_TASKS, K, MAX_QUALITY_LVL, E_s_mAh, S, B, tasks);
        optQ = solution(B[(K-1)%2],S,K,NS,tasks,relaxedOptQ);
        t2=millis();
        if(optQ != -1) {
            totalQualityPerc=0;
            printf("%d,",optQ);
            for(i=0;i<K;i++){
                totalQualityPerc+= tasks[NS[i]-1].q_perc;
            }
            printf("%f,", (float)totalQualityPerc/K);
            if(checkQuality(NS,optQ,tasks)) printf("OK,");
            else printf("X,");
            if(checkBres(optQ, B[(K-1)%2], K, NS, tasks, E_s_mAh)) printf("OK,");
            else printf("X,");
            if(checkFeasibility(K, NS, tasks, E_s_mAh)) printf("OK,");
            else printf("X,");
            for(i=0;i<K-1;i++)
                printf("%d,",NS[i]-1);
            printf("%d,",NS[K-1]-1);
        }
        else {
          printf("Nessuna soluzione ammissibile");
        }
        printf("%li\n",t2-t1);
        counter++;
  }
}
