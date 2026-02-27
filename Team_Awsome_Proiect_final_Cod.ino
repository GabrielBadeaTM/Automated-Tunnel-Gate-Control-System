#include <Arduino_FreeRTOS.h> 
#include <semphr.h> 

// --- DEFINIȚII PINI ---
#define PIN_LED_In1   13
#define PIN_LED_Out1  5
#define PIN_LED_In2   12
#define PIN_LED_Out2  4
#define PIN_LED_Panica  6

#define PIN_GAZE  A14  
#define PIN_FUM   A15 

// Butoane (Intreruperi)
#define PIN_In1   2
#define PIN_Out1  18
#define PIN_In2   3
#define PIN_Out2  19
#define PIN_Panica  20

// --- VARIABILE GLOBALE ---
int nrMSens1 = 0;
int nrMSens2 = 0;

float nivel_gaze = 0;
float nivel_fum = 0;
bool pericol_fum = false;
bool pericol_gaze = false;
bool panica = false;

// Variabile control Manual
bool modManual = false;
bool manualIn1 = false, manualOut1 = false;
bool manualIn2 = false, manualOut2 = false;


// --- SEMAFOARE & MUTEX ---
SemaphoreHandle_t semIn1, semOut1, semIn2, semOut2, semPanica;

// Semafoare pentru bariere
SemaphoreHandle_t semInchideIn1, semDeschideIn1;
SemaphoreHandle_t semInchideOut1, semDeschideOut1;
SemaphoreHandle_t semInchideIn2, semDeschideIn2;
SemaphoreHandle_t semInchideOut2, semDeschideOut2;

SemaphoreHandle_t Mutex_Trafic;
SemaphoreHandle_t Mutex_Gaze, Mutex_Fum;
SemaphoreHandle_t Mutex_Pericol;
SemaphoreHandle_t Mutex_Panica;
SemaphoreHandle_t Mutex_Manual;

const int N = 10; // maxim numar masini pe sens
const int prag_gaz = 60;
const int prag_fum = 70;
const unsigned long debounce_delay = 250; 

// --- ISR-uri (Doar dau semaforul) ---
// Variabilele "last_interrupt" nu mai sunt critice daca facem debounce in Task, 
// dar le pastram pentru siguranță.
volatile unsigned long last_time_In1 = 0;
void ISR_In1() {
  if(millis() - last_time_In1 > debounce_delay){
    xSemaphoreGiveFromISR(semIn1, NULL);
    last_time_In1 = millis();
  }
}

volatile unsigned long last_time_Out1 = 0;
void ISR_Out1() {
  if(millis() - last_time_Out1 > debounce_delay){
    xSemaphoreGiveFromISR(semOut1, NULL);
    last_time_Out1 = millis();
  }
}

volatile unsigned long last_time_In2 = 0;
void ISR_In2() {
  if(millis() - last_time_In2 > debounce_delay){
    xSemaphoreGiveFromISR(semIn2, NULL);
    last_time_In2 = millis();
  }
}

volatile unsigned long last_time_Out2 = 0;
void ISR_Out2() {
  if(millis() - last_time_Out2 > debounce_delay){
    xSemaphoreGiveFromISR(semOut2, NULL);
    last_time_Out2 = millis();
  }
}

volatile unsigned long last_time_Panic = 0;
void ISR_Panica() {
  if(millis() - last_time_Panic > debounce_delay){
    xSemaphoreGiveFromISR(semPanica, NULL);
    last_time_Panic = millis();
  }
}

// --- PROTOTIPURI TASK-URI ---
void TaskCitireGaze( void *pvParameters );
void TaskCitireFum( void *pvParameters );
void TaskVerificareNivelGaze( void *pvParameters );
void TaskVerificareNivelFum( void *pvParameters );
void TaskInSens1( void *pvParameters );
void TaskOutSens1( void *pvParameters );
void TaskInSens2( void *pvParameters );
void TaskOutSens2( void *pvParameters );
void TaskPanica( void *pvParameters );
void TaskManual( void *pvParameters );
void TaskServer( void *pvParameters );
void TaskAfisare( void *pvParameters );
// Cele 8 Task-uri specifice pentru bariere:
void TaskInchideBarieraIn1( void *pvParameters );
void TaskInchideBarieraIn2( void *pvParameters );
void TaskInchideBarieraOut1( void *pvParameters );
void TaskInchideBarieraOut2( void *pvParameters );
void TaskDeschideBarieraIn1( void *pvParameters );
void TaskDeschideBarieraIn2( void *pvParameters );
void TaskDeschideBarieraOut1( void *pvParameters );
void TaskDeschideBarieraOut2( void *pvParameters );


void setup() {
  Serial.begin(115200); 
  while(!Serial);

  // Configurare Pini
  pinMode(PIN_LED_In1, OUTPUT);
  pinMode(PIN_LED_Out1, OUTPUT);
  pinMode(PIN_LED_In2, OUTPUT);
  pinMode(PIN_LED_Out2, OUTPUT);
  pinMode(PIN_LED_Panica, OUTPUT);

  // BUTOANE: INPUT simplu (cu rezistente externe Pull-Down)
  pinMode(PIN_In1, INPUT);
  pinMode(PIN_Out1, INPUT);
  pinMode(PIN_In2, INPUT);
  pinMode(PIN_Out2, INPUT);
  pinMode(PIN_Panica, INPUT);

  // Creare Semafoare
  semIn1 = xSemaphoreCreateBinary(); semOut1 = xSemaphoreCreateBinary();
  semIn2 = xSemaphoreCreateBinary(); semOut2 = xSemaphoreCreateBinary();
  semPanica = xSemaphoreCreateBinary();

  semInchideIn1 = xSemaphoreCreateBinary(); semDeschideIn1 = xSemaphoreCreateBinary();
  semInchideOut1 = xSemaphoreCreateBinary(); semDeschideOut1 = xSemaphoreCreateBinary();
  semInchideIn2 = xSemaphoreCreateBinary(); semDeschideIn2 = xSemaphoreCreateBinary();
  semInchideOut2 = xSemaphoreCreateBinary(); semDeschideOut2 = xSemaphoreCreateBinary();

  Mutex_Trafic = xSemaphoreCreateMutex();
  Mutex_Gaze = xSemaphoreCreateMutex();
  Mutex_Fum = xSemaphoreCreateMutex();
  Mutex_Pericol = xSemaphoreCreateMutex();
  Mutex_Panica = xSemaphoreCreateMutex();
  Mutex_Manual = xSemaphoreCreateMutex();

  // CREARE TASK-URI 
  // Folosim stack minim (100) la task-urile simple pentru a nu umple memoria
  xTaskCreate(TaskCitireGaze, "Gaze", 100, NULL, 1, NULL);
  xTaskCreate(TaskCitireFum, "Fum", 100, NULL, 1, NULL);
  xTaskCreate(TaskVerificareNivelGaze, "VerifG", 100, NULL, 1, NULL);
  xTaskCreate(TaskVerificareNivelFum, "VerifF", 100, NULL, 1, NULL);

  xTaskCreate(TaskInSens1, "In1", 128, NULL, 2, NULL);
  xTaskCreate(TaskOutSens1, "Out1", 128, NULL, 2, NULL);
  xTaskCreate(TaskInSens2, "In2", 128, NULL, 2, NULL);
  xTaskCreate(TaskOutSens2, "Out2", 128, NULL, 2, NULL);

  xTaskCreate(TaskPanica, "Panic", 128, NULL, 3, NULL);
  xTaskCreate(TaskManual, "Man", 128, NULL, 1, NULL);
  xTaskCreate(TaskServer, "Srv", 200, NULL, 2, NULL); // Serverul e complex, are nevoie de stack

  // Cele 8 Task-uri de bariera - Stack MIC (80-100)
  xTaskCreate(TaskInchideBarieraIn1,  "C_In1", 85, (void*)PIN_LED_In1, 2, NULL);
  xTaskCreate(TaskDeschideBarieraIn1, "O_In1", 85, (void*)PIN_LED_In1, 2, NULL);
  xTaskCreate(TaskInchideBarieraOut1,  "C_Out1", 85, (void*)PIN_LED_Out1,2, NULL);
  xTaskCreate(TaskDeschideBarieraOut1, "O_Out1", 85, (void*)PIN_LED_Out1,2, NULL);
  xTaskCreate(TaskInchideBarieraIn2,  "C_In2", 85, (void*)PIN_LED_In2, 2, NULL);
  xTaskCreate(TaskDeschideBarieraIn2, "O_In2", 85, (void*)PIN_LED_In2,2, NULL);
  xTaskCreate(TaskInchideBarieraOut2,  "C_Out2", 85, (void*)PIN_LED_Out2,2, NULL);
  xTaskCreate(TaskDeschideBarieraOut2, "O_Out2", 85, (void*)PIN_LED_Out2,2, NULL);

  xTaskCreate(TaskAfisare, "Disp", 256, NULL, 1, NULL);

  // INTRERUPERI LA FINAL (RISING pentru Pull-Down)
  attachInterrupt(digitalPinToInterrupt(PIN_In1), ISR_In1, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_Out1), ISR_Out1, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_In2), ISR_In2, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_Out2), ISR_Out2, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_Panica), ISR_Panica, RISING);

  Serial.println("Sistem initializat.");
  vTaskStartScheduler();
}

void loop() {}

// --- IMPLEMENTARE TASK-URI ---

// Citire Senzori
void TaskCitireGaze(void *pvParameters) {
    for(;;) {
        int gaze = analogRead(PIN_GAZE);  
        float procent = (gaze / 1023.0) * 100.0;
        if(xSemaphoreTake(Mutex_Gaze, portMAX_DELAY) == pdTRUE){
           nivel_gaze = procent;
           xSemaphoreGive(Mutex_Gaze);
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void TaskCitireFum(void *pvParameters) {
    for(;;) {
        int fum = analogRead(PIN_FUM);  
        float procent = (fum / 1023.0) * 100.0; 
        if(xSemaphoreTake(Mutex_Fum, portMAX_DELAY) == pdTRUE){
          nivel_fum = procent;
          xSemaphoreGive(Mutex_Fum);
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

// Verificare Nivel
void TaskVerificareNivelGaze(void *pvParameters) {
  for(;;) {
    bool local_pericol = false;
    if (xSemaphoreTake(Mutex_Gaze, portMAX_DELAY) == pdTRUE) {
      if (nivel_gaze > prag_gaz) local_pericol = true;
      xSemaphoreGive(Mutex_Gaze);
    }
    
    xSemaphoreTake(Mutex_Pericol, portMAX_DELAY);
    pericol_gaze = local_pericol;
    xSemaphoreGive(Mutex_Pericol);
    
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void TaskVerificareNivelFum(void *pvParameters) {
  for(;;) {
    bool local_pericol = false;
    if (xSemaphoreTake(Mutex_Fum, portMAX_DELAY) == pdTRUE) {
      if (nivel_fum > prag_fum) local_pericol = true;
      xSemaphoreGive(Mutex_Fum);
    }
    
    xSemaphoreTake(Mutex_Pericol, portMAX_DELAY);
    pericol_fum = local_pericol;
    xSemaphoreGive(Mutex_Pericol);
    
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// Gestionare Trafic (CU DEBOUNCE SOFTWARE)
void TaskInSens1(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(semIn1, portMAX_DELAY) == pdTRUE){
      vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce
      if(digitalRead(PIN_In1) == HIGH) { // Verificare fizica
        if(xSemaphoreTake(Mutex_Trafic, portMAX_DELAY) == pdTRUE){
          if (nrMSens1 < N) nrMSens1++;
          xSemaphoreGive(Mutex_Trafic);
        }
      }
    }
  }
}

void TaskOutSens1(void *pvParameters) {
  for(;;) {
    if(xSemaphoreTake(semOut1, portMAX_DELAY) == pdTRUE) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      if(digitalRead(PIN_Out1) == HIGH) {
        if(xSemaphoreTake(Mutex_Trafic, portMAX_DELAY) == pdTRUE){
            if(nrMSens1 > 0) nrMSens1--;
            xSemaphoreGive(Mutex_Trafic);
        }
      }
    }
  }
}

void TaskInSens2(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(semIn2, portMAX_DELAY) == pdTRUE){
      vTaskDelay(50 / portTICK_PERIOD_MS);
      if(digitalRead(PIN_In2) == HIGH) {
        if(xSemaphoreTake(Mutex_Trafic, portMAX_DELAY) == pdTRUE){
            if (nrMSens2 < N) nrMSens2++;
            xSemaphoreGive(Mutex_Trafic);
        }
      }
    }
  }
}

void TaskOutSens2(void *pvParameters) {
  for(;;) {
    if(xSemaphoreTake(semOut2, portMAX_DELAY) == pdTRUE) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      if(digitalRead(PIN_Out2) == HIGH) {
        if(xSemaphoreTake(Mutex_Trafic, portMAX_DELAY) == pdTRUE){
          if(nrMSens2 > 0) nrMSens2--;
          xSemaphoreGive(Mutex_Trafic);
        }
      }
    }
  }
}

void TaskPanica(void *pvParameters) {
  for(;;) {
    if(xSemaphoreTake(semPanica, portMAX_DELAY) == pdTRUE) {
      vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce
      if(digitalRead(PIN_Panica) == HIGH) {
        if(xSemaphoreTake(Mutex_Panica, portMAX_DELAY) == pdTRUE){
          panica = !panica;
          xSemaphoreGive(Mutex_Panica);
          vTaskDelay(500 / portTICK_PERIOD_MS); // Delay anti-repetitie
        }
      }
    }
  }
}

// Cele 8 Task-uri pentru bariere
void TaskInchideBarieraIn1( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semInchideIn1, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, HIGH); 
    }
  }
}
void TaskInchideBarieraOut1( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semInchideOut1, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, HIGH); 
    }
  }
}
void TaskInchideBarieraIn2( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semInchideIn2, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, HIGH); 
    }
  }
}
void TaskInchideBarieraOut2( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semInchideOut2, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, HIGH); 
    }
  }
}

void TaskDeschideBarieraIn1( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semDeschideIn1, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, LOW); 
    }
  }
}
void TaskDeschideBarieraOut1( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semDeschideOut1, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, LOW); 
    }
  }
}
void TaskDeschideBarieraIn2( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semDeschideIn2, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, LOW);
    } 
  }
}
void TaskDeschideBarieraOut2( void *pvParameters ){
  int pin = (int)pvParameters;
  for(;;) {
    if(xSemaphoreTake(semDeschideOut2, portMAX_DELAY) == pdTRUE) {
      digitalWrite(pin, LOW); 
    }
  }
}

void TaskManual(void *pvParameters) {
  for (;;) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if(strchr("01234", c)) {
          xSemaphoreTake(Mutex_Manual, portMAX_DELAY);
          if(c == '0') {
             modManual = false; manualIn1=0; manualOut1=0; manualIn2=0; manualOut2=0;
          } else {
             modManual = true;
             if(c == '1') manualIn1 = !manualIn1;
             if(c == '2') manualOut1 = !manualOut1;
             if(c == '3') manualIn2 = !manualIn2;
             if(c == '4') manualOut2 = !manualOut2;
          }
          xSemaphoreGive(Mutex_Manual);
      }
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// SERVERUL
void TaskServer(void *pvParameters) {
  // Variabile de MEMORIE pentru starea barierelor
  // false = Deschisa, true = Inchisa
  // Le initializam false (presupunem deschise la start)
  bool stareIn1_Inchisa = false;
  bool stareOut1_Inchisa = false;
  bool stareIn2_Inchisa = false;
  bool stareOut2_Inchisa = false;

  for(;;) {
    bool localPanica, localManual, localPericol;
    
    // 1. VERIFICARE PANICA
    xSemaphoreTake(Mutex_Panica, portMAX_DELAY); localPanica = panica; xSemaphoreGive(Mutex_Panica);
    digitalWrite(PIN_LED_Panica, localPanica ? HIGH : LOW);
    if (localPanica) {
        // Vrem TOTUL INCHIS. Verificam daca e deja inchis inainte sa dam comanda.
        if(!stareIn1_Inchisa) { xSemaphoreGive(semInchideIn1); stareIn1_Inchisa = true; }
        if(!stareOut1_Inchisa){ xSemaphoreGive(semInchideOut1); stareOut1_Inchisa = true; }
        if(!stareIn2_Inchisa) { xSemaphoreGive(semInchideIn2); stareIn2_Inchisa = true; }
        if(!stareOut2_Inchisa){ xSemaphoreGive(semInchideOut2); stareOut2_Inchisa = true; }

        vTaskDelay(100 / portTICK_PERIOD_MS);
        continue; // Sarim peste restul logicii
    }

    // 2. VERIFICARE MANUAL
    xSemaphoreTake(Mutex_Manual, portMAX_DELAY); localManual = modManual; xSemaphoreGive(Mutex_Manual);
    if(localManual) {
        xSemaphoreTake(Mutex_Manual, portMAX_DELAY);
        
        // --- Manual IN 1 ---
        if(manualIn1 == true && !stareIn1_Inchisa) {
             xSemaphoreGive(semInchideIn1); stareIn1_Inchisa = true;
        } else if (manualIn1 == false && stareIn1_Inchisa) {
             xSemaphoreGive(semDeschideIn1); stareIn1_Inchisa = false;
        }

        // --- Manual OUT 1 ---
        if(manualOut1 == true && !stareOut1_Inchisa) {
             xSemaphoreGive(semInchideOut1); stareOut1_Inchisa = true;
        } else if (manualOut1 == false && stareOut1_Inchisa) {
             xSemaphoreGive(semDeschideOut1); stareOut1_Inchisa = false;
        }

        // --- Manual IN 2 ---
        if(manualIn2 == true && !stareIn2_Inchisa) {
             xSemaphoreGive(semInchideIn2); stareIn2_Inchisa = true;
        } else if (manualIn2 == false && stareIn2_Inchisa) {
             xSemaphoreGive(semDeschideIn2); stareIn2_Inchisa = false;
        }

        // --- Manual OUT 2 ---
        if(manualOut2 == true && !stareOut2_Inchisa) {
             xSemaphoreGive(semInchideOut2); stareOut2_Inchisa = true;
        } else if (manualOut2 == false && stareOut2_Inchisa) {
             xSemaphoreGive(semDeschideOut2); stareOut2_Inchisa = false;
        }

        xSemaphoreGive(Mutex_Manual);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        continue; 
    }
        
    // 3. VERIFICARE PERICOL
    xSemaphoreTake(Mutex_Pericol, portMAX_DELAY); 
    localPericol = (pericol_gaze || pericol_fum); 
    xSemaphoreGive(Mutex_Pericol);
    if(localPericol) {
       // Inchidem intrarile
       if(!stareIn1_Inchisa) { xSemaphoreGive(semInchideIn1); stareIn1_Inchisa = true; }
       if(!stareIn2_Inchisa) { xSemaphoreGive(semInchideIn2); stareIn2_Inchisa = true; }

       // Deschidem iesirile (Evacuare)
       if(stareOut1_Inchisa) { xSemaphoreGive(semDeschideOut1); stareOut1_Inchisa = false; }
       if(stareOut2_Inchisa) { xSemaphoreGive(semDeschideOut2); stareOut2_Inchisa = false; }

       vTaskDelay(150 / portTICK_PERIOD_MS);
       continue;
    }

    // 4. AUTOMAT (Trafic Normal)
    if(xSemaphoreTake(Mutex_Trafic, portMAX_DELAY) == pdTRUE){
      
      // Sens 1 Logic
      if(nrMSens1 < N) {
          // Vrem deschis
          if(stareIn1_Inchisa) { xSemaphoreGive(semDeschideIn1); stareIn1_Inchisa = false; }
      } else {
          // Vrem inchis
          if(!stareIn1_Inchisa) { xSemaphoreGive(semInchideIn1); stareIn1_Inchisa = true; }
      }

      // Sens 2 Logic
      if(nrMSens2 < N) {
          if(stareIn2_Inchisa) { xSemaphoreGive(semDeschideIn2); stareIn2_Inchisa = false; }
      } else {
          if(!stareIn2_Inchisa) { xSemaphoreGive(semInchideIn2); stareIn2_Inchisa = true; }
      }
      
      xSemaphoreGive(Mutex_Trafic);
    }

    // Iesirile trebuie sa fie deschise in mod normal
    if(stareOut1_Inchisa) { xSemaphoreGive(semDeschideOut1); stareOut1_Inchisa = false; }
    if(stareOut2_Inchisa) { xSemaphoreGive(semDeschideOut2); stareOut2_Inchisa = false; }

    vTaskDelay(150 / portTICK_PERIOD_MS);
  }
}

void TaskAfisare(void *pvParameters) {
  // Variabile locale pentru a afisa rapid
  int localSens1, localSens2; 

  for(;;) {
    Serial.println(" ");
    Serial.println("====================");

    // --- GAZE ---
    if(xSemaphoreTake(Mutex_Gaze, portMAX_DELAY) == pdTRUE) {
      Serial.print("Gaze: ");
      Serial.print(nivel_gaze);
      Serial.print("% | Pericol: ");
      xSemaphoreGive(Mutex_Gaze);
    }

    if(xSemaphoreTake(Mutex_Pericol, portMAX_DELAY) == pdTRUE) {
      if(pericol_gaze == true) Serial.println("DA");
      else Serial.println("NU");
      xSemaphoreGive(Mutex_Pericol);
    }

    // --- FUM ---
    if(xSemaphoreTake(Mutex_Fum, portMAX_DELAY) == pdTRUE) {
      Serial.print("Fum:  ");
      Serial.print(nivel_fum);
      Serial.print("% | Pericol: ");
      xSemaphoreGive(Mutex_Fum);
    }

    if(xSemaphoreTake(Mutex_Pericol, portMAX_DELAY) == pdTRUE) {
      if(pericol_fum == true) Serial.println("DA");
      else Serial.println("NU");
      xSemaphoreGive(Mutex_Pericol);
    }

    // --- TRAFIC ---
    if(xSemaphoreTake(Mutex_Trafic, portMAX_DELAY) == pdTRUE) {
      localSens1 = nrMSens1;
      localSens2 = nrMSens2;
      xSemaphoreGive(Mutex_Trafic);
    }
    Serial.print("Masini sens 1: "); Serial.println(localSens1);
    Serial.print("Masini sens 2: "); Serial.println(localSens2);

    // --- PANICA ---
    if(xSemaphoreTake(Mutex_Panica, portMAX_DELAY) == pdTRUE) {
      Serial.print("Panica: ");
      if(panica == true) Serial.println("ACTIVA");
      else Serial.println("inactiva");
      xSemaphoreGive(Mutex_Panica);
    }

    // --- MOD DE OPERARE (MANUAL / AUTOMAT) ---
    if(xSemaphoreTake(Mutex_Manual, portMAX_DELAY) == pdTRUE) {
      Serial.print("Mod operare: ");
      if(modManual == true) Serial.println("MANUAL");
      else Serial.println("AUTOMAT");

      // Detalii doar daca e Manual
      if(modManual == true) {
        Serial.print("  In1: ");
        if(manualIn1) Serial.println("inchisa"); else Serial.println("deschisa");

        Serial.print("  Out1: ");
        if(manualOut1) Serial.println("inchisa"); else Serial.println("deschisa");

        Serial.print("  In2: ");
        if(manualIn2) Serial.println("inchisa"); else Serial.println("deschisa");

        Serial.print("  Out2: ");
        if(manualOut2) Serial.println("inchisa"); else Serial.println("deschisa");
      }
      xSemaphoreGive(Mutex_Manual);
    }

    Serial.println("=========================");
    
    // Refresh la 0.5 secunde
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}