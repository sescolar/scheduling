#include <Arduino.h>
#include <assert.h>


#define K         24
#define N_TASKS   10
#define slotDurationPercentage  (24/(float)K)

#define ACTIVE_SYSTEM_CONSUMPTION   124
#define IDLE_SYSTEM_CONSUMPTION      16
#define MAX_OVERPRODUCTION           20
#define MAX_UNDERPRODUCTION          40
#define SUNSET                       19
#define SUNRISE                       8


#define BATTERY_SAMPLING  2000
#define BMAX              2000
#define BMIN               160
#define B_INIT            1800
#define N_ITERATION        25
#define SEED               124
#define VARIATION            1

struct Task {
    uint8_t q_perc;
    uint8_t c_mAh;
};
struct Task tasks[N_TASKS];

uint8_t i; // Iterators

//------- Panel Production

uint16_t E_h[24]       // Hourly Energy harvested 
// August
= { 0,0,0,0,0,0,0,3,45,133,215,285,327,339,322,255,60,66,63,23,9,0,0,0  };
// September
//= { 0,0,0,0,0,0,0,0,24,107,202,270,313,316,310,251,98,45,37,15,2,9,0,0 };
// October 
//= { 0,0,0,0,0,0,0,0,19,110,224,285,335,350,331,283,134,20,18,8,0,0,0,0 };

uint16_t E_h_v[24];     // Hourly Energy harvested successiva a variazioni percentuali
uint16_t E_s_mAh[K] = { 0 };    // Final Energy harvested per slot in mAh

//------------------------------------

void InitializeEnergyHarvested(void)
{
  for (i=0; i<24; i++) {
    // 520 is the maximum current in mAh supplied by the solar panel
    // E_h_v[i] = min(E_h[i],520);
    for (uint8_t j=0; j<(K/24); j++) {
      uint8_t pos = (i * K / 24) + j;
      E_s_mAh[pos] = E_h[i] * slotDurationPercentage;
    }
  }
}

void GenerateTasks(void)
{
    tasks[0].c_mAh=1; 
    tasks[0].q_perc=1;
    for(i=1; i<N_TASKS; i++)
        tasks[i].c_mAh = ceil(((((float)(i-1.0) / 10.0) * ACTIVE_SYSTEM_CONSUMPTION) +
                        ((1 - (((float) (i-1.0)) / 10.0)) * IDLE_SYSTEM_CONSUMPTION)) * 
                        slotDurationPercentage);
   float scale = 100.0 / tasks[N_TASKS-1].c_mAh;
   for (i=0; i<N_TASKS; i++)
        tasks[i].q_perc = ceil(tasks[i].c_mAh*scale);    
}

uint16_t Q[2][BATTERY_SAMPLING+1] = { 0 };         // DP: Quality Table
uint8_t S[K][BATTERY_SAMPLING+1];                  // DP: Scheduling Table 
uint8_t NS[K];                                     // Final Scheduling 

/*mAh correspondiente a un nivel de muestreo*/
#define mAh_per_lvl      (((float)BMAX-BMIN)/BATTERY_SAMPLING)
#define level_to_mah(l)  short(ceil((l)*(mAh_per_lvl))+BMIN) 
#define mah_to_level(b)  short((b-BMIN)/(mAh_per_lvl))


char buf[30];
void PrintParameters(void)
{
  Serial.flush();
  Serial.print(F("\n#----------------------------------------------\n"));
  Serial.print(F("K = ")); Serial.println(K);
  Serial.print(F("N = ")); Serial.println(N_TASKS);
  
  Serial.print(F("BMIN = ")); Serial.println(BMIN); 
  //Serial.print(","); Serial.println(mah_to_level(BMIN));
  
  Serial.print(F("BINIT = ")); Serial.println(B_INIT);
  //Serial.print(","); Serial.println(mah_to_level(B_INIT));
  
  Serial.print(F("BMAX = ")); Serial.println(BMAX);
  //Serial.print(","); Serial.println(mah_to_level(BMAX));
  
  Serial.print(F("BSAMPLING = ")); Serial.println(BATTERY_SAMPLING);
  Serial.print(F("EnergyForLevel = ")); Serial.println(mAh_per_lvl);  
  Serial.print(F("#Tasks: "));
  for (i=0; i<N_TASKS; i++) { Serial.print(F("   "));Serial.print(i);  }
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

//#define DEBUG 0

void PrintVector(uint16_t v[])
{
  for (i=0; i<=BATTERY_SAMPLING; i++) {
    Serial.print(v[i]);
    if ((i+1) % 42 == 0) 
      Serial.print("\n");
    else 
      Serial.print(" ");
  }
  Serial.println("");
}

uint16_t schedule_compact(uint16_t E[])
{
  int8_t k = K-1, t = N_TASKS-1, idmax = 0; 
  uint16_t qmax = 0, b_mAh = 0, Br = 0, l = 0, q = 0;

  for (int16_t b=BATTERY_SAMPLING; b>=0; b--) {
    b_mAh = level_to_mah(b);
    qmax = 0; idmax = 0;
    for (; t>=0; t--) {
      Br = min(b_mAh - tasks[t].c_mAh + E[k], BMAX);
      if (Br >= B_INIT) {
         qmax = tasks[t].q_perc;
         idmax = t+1;
         break;
      }
    }
    assert(k-1>=0 && k-1<K);
    assert(b>=0 && b<BATTERY_SAMPLING+1);
    assert(qmax>=0 && idmax>=0);
    Q[k%2][b] = qmax;
    S[k-1][b] = idmax;
  }
  
  for (k=K-2; k>=0; k--) {
    if (k==0) {                       // when k==0 only B_INIT matter
      b_mAh = B_INIT;
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
    } else {
      for (int16_t b=BATTERY_SAMPLING; b>=0; b--) {
        b_mAh = level_to_mah(b);
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
        S[k-1][b] = idmax;
      }
    }
  }
  if (K%2 == 0)
    q = qmax;
  else
    q = Q[(K+1)%2][mah_to_level(B_INIT)];
  return q;
}

int schedule(uint16_t E[])
{
  int8_t k,idmax,t;
  uint16_t b_mAh,Br,l;
  uint16_t qmax,q;
  int16_t b;

  k = K-1;
#ifdef DEBUG
  sprintf(buf,"------ k=%d -------\n",k);
  Serial.print(buf);
#endif
  t = N_TASKS-1;
  for (b=BATTERY_SAMPLING; b>=0; b--) {
      b_mAh = level_to_mah(b);
      qmax = 0, idmax = 0; 
#ifdef DEBUG
      sprintf(buf,"b=%2d, B=%4d ", b, b_mAh);
      Serial.print(buf);
#endif
      for (; t>=0; t--) {
        Br = min(b_mAh - tasks[t].c_mAh + E[k], BMAX);
#ifdef DEBUG
        sprintf(buf,"(%d,%4d),",t,Br);
        Serial.print(buf);
#endif
        if (Br >= B_INIT) {
         qmax = tasks[t].q_perc;
         idmax = t+1;
         break;
        }
      }
      Q[k%2][b] = qmax;
      S[k][b] = idmax;
#ifdef DEBUG      
      sprintf(buf,"qmax=%d, idmax=%d ",qmax,idmax);
      Serial.print(buf); 
      Serial.println();
#endif
  }
  
  for (k=K-2; k>=0; k--) {
#ifdef DEBUG
    sprintf(buf,"------ k=%d ----\n",k);
    Serial.print(buf);
#endif
    for (b=BATTERY_SAMPLING; b>=0; b--) {
      b_mAh = level_to_mah(b);
      qmax = 0, idmax = 0; 
#ifdef DEBUG
      sprintf(buf,"b=%2d, B=%4d ", b, b_mAh);
      Serial.print(buf);
#endif
      for (t=N_TASKS-1; t>=0; t--) {  // iterate from the last
        Br = min(b_mAh - tasks[t].c_mAh + E[k], BMAX);
#ifdef DEBUG
        sprintf(buf," (%d,%4d),",t,Br);
        Serial.print(buf);
#endif
        if (Br >= BMIN) {
             l = mah_to_level(Br);   assert(l <= BATTERY_SAMPLING);
             q = Q[(k + 1)%2][l];
             if (q!=0 && (q + tasks[t].q_perc) > qmax) {
               qmax = q + tasks[t].q_perc;
               idmax = t+1;
#ifdef DEBUG
        sprintf(buf,"l=%d,q=%d,",l,q);
        Serial.print(buf);
#endif
             }
        }
      }
      Q[k%2][b] = qmax;
      S[k][b] = idmax;  
#ifdef DEBUG
      sprintf(buf,"qmax=%d, idmax=%d.",qmax,idmax);
      Serial.print(buf); 
      Serial.println("");
#endif
    }
  }
#ifdef DEBUG
  for(b=0; b<BATTERY_SAMPLING+1; b++) {
    Serial.print(Q[0][b]);
    Serial.print(" ");
  }      Serial.println("");
  for(b=0; b<BATTERY_SAMPLING+1; b++) 
   {
    Serial.print(Q[1][b]);
    Serial.print(" ");
  }
#endif
  if (K%2 == 0)
    return Q[K%2][mah_to_level(B_INIT)];
  else
    return Q[(K+1)%2][mah_to_level(B_INIT)];
}

void scheduleTasks(uint16_t E[K], uint16_t Q)
{
  if (Q==0) return;
    uint16_t BresL = mah_to_level(B_INIT);
    uint16_t Bres_mAh, q = 0;
    for (i=0; i<K; i++) {
        Bres_mAh = level_to_mah(BresL);
        NS[i] = S[i][BresL];
        Bres_mAh = min(Bres_mAh - tasks[ NS[i]-1 ].c_mAh + E[i], BMAX);
        q += tasks[NS[i]-1].q_perc;
        assert(Bres_mAh >= BMIN);
        BresL = mah_to_level(Bres_mAh);
    }
    assert(Bres_mAh >= B_INIT);
    assert(q == Q);
}

void update_panel() 
{
    uint8_t coin;
    uint8_t pos;
    /*Variazione percentuale di energy harvesting oraria*/
    uint8_t variation;
    
    for (i=0; i<24; i++) {
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

            for (int8_t j=0; j<(K/24); j++) {
                pos = (i * K / 24) + j;
                E_s_mAh[pos] = (int)(E_h_v[i] * slotDurationPercentage);
            }
        }
    }
}


void ClearQS()
{
  memset(Q,0,sizeof(Q));
  memset(S,0,sizeof(S));
}

void setup() 
{
#ifdef ARDUINO
  Serial.begin(9600);
  while (!Serial) ; 
#endif
  srand(SEED);
  GenerateTasks();
  InitializeEnergyHarvested();
  PrintParameters();  
}

uint8_t counter = 0;              // iteration counter

void loop() {
  if (counter < N_ITERATION)
  {
      Serial.print(F("it,Energy = "));
      Serial.print(counter); Serial.print(",");

      update_panel();
      
      uint16_t totalEnergyHarvested = 0;
      for(i=0; i<K; i++) totalEnergyHarvested += E_s_mAh[i];
      Serial.println(totalEnergyHarvested);

      Serial.print(F("E= ["));
      for (i=0; i<K; i++) {
         Serial.print(E_s_mAh[i]);
         Serial.print(i==K-1?"]":",");
      } Serial.println("");

      ClearQS();
      unsigned long t1 = millis();
      uint16_t optQ = schedule(E_s_mAh);
      unsigned long t2 = millis();

      if (optQ != 0) {
        scheduleTasks(E_s_mAh,optQ);
        ClearQS();
        
        uint16_t optQ2 = schedule_compact(E_s_mAh);
        assert(optQ2 == optQ);
        Serial.print("Q = ");
        Serial.println(optQ);
        //Serial.print(", ");
        //Serial.println(optQ2);
        Serial.print("S=[");
        for(i=0; i<K; i++) {
          Serial.print(NS[i]);
          Serial.print((i==K-1)?"]":",");
        }
        Serial.print("\n");
      }
      Serial.print(F("Time="));
      Serial.println(t2-t1);
      counter++;
  } else delay(1000);
}
