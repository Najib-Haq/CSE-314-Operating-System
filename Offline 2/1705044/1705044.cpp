#include <cstdio>
#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <random>
#include <chrono>

using namespace std;
default_random_engine generator;
// poisson_distribution<int> distribution(5);
exponential_distribution<double> distribution (15/60.0); // 1/lambda -> 15 passengers in 60 seconds

string message;

// global variables defined by input
int no_kiosk;
int no_belts;
int no_belt_passengers;

int kisok_time;
int security_time;
int boarding_time;
int vip_time;

// global variables needed in functions
int vip_lr_people_count = 0;
int vip_rl_people_count = 0;
queue<int> kiosk_q;
int kiosk_val; 

// define mutex and semaphores
sem_t kiosks;
sem_t special_kiosk;
sem_t* belts;
sem_t vip_LR_channel;
sem_t vip_RL_channel;
sem_t vip_LR_people;
sem_t vip_RL_people;
sem_t boarding;
sem_t printing;
// pthread_mutex_t printing;


// keep track of time
chrono::steady_clock::time_point start;

inline int get_time(){
    return chrono::duration_cast<chrono::seconds> (chrono::steady_clock::now() - start).count();
} 

class Passenger{
public:
    int id;
    bool vip = false; // whether vip
    bool pass = false; // whether got pass or not
    bool vip_access = false; // whether has vip access
    Passenger(int id){ this->id = id; }
    string get_id() { 
        if (this->vip) return to_string(this->id) + " (VIP)";
        return to_string(this->id);
    }
    void set_vip() { this->vip = true, this->vip_access = true; }
};


void print(string msg){
    sem_wait(&printing);
    // pthread_mutex_lock(&printing);
    cout<<msg<<endl;
    // pthread_mutex_unlock(&printing);
    sem_post(&printing);
}


void check_in_kiosk(Passenger* p){ 
    sem_wait(&kiosks); // enter kiosk only if there are kiosks left
    kiosk_val = kiosk_q.front(); kiosk_q.pop();
    print("Passenger " + p->get_id() + " has started self-check in at kiosk " + to_string(kiosk_val) + " at time " + to_string(get_time()));
    sleep(kisok_time);
    p->pass = true; // got pass
    print("Passenger " + p->get_id() + " has finished check in at time " + to_string(get_time()));
    kiosk_q.push(kiosk_val);
    sem_post(&kiosks);
}


void check_in_special_kiosk(Passenger* p){
    print("Passenger " + p->get_id() + " has started waiting for special kiosk at time " + to_string(get_time()));    
    sem_wait(&special_kiosk); // enter kiosk only if there are kiosks left
    print("Passenger " + p->get_id() + " has started self-check in at special kiosk at time " + to_string(get_time()));
    sleep(kisok_time);
    p->pass = true; // got pass
    print("Passenger " + p->get_id() + " has finished check in at time " + to_string(get_time()));
    sem_post(&special_kiosk);
}


void security_check(Passenger* p, int belt_no){
    print("Passenger " + p->get_id() + " has started waiting for security check in belt " + to_string(belt_no+1) + " at time " + to_string(get_time()));
    sem_wait(&belts[belt_no]);
    print("Passenger " + p->get_id() + " has started the security check in belt " + to_string(belt_no+1) + " at time " + to_string(get_time()));
    sleep(security_time);
    print("Passenger " + p->get_id() + " has crossed security check at time " + to_string(get_time()));
    sem_post(&belts[belt_no]);
}


void vip_left_right(Passenger* p){
    print("Passenger " + p->get_id() + " has started waiting for VIP channel (L->R) at time " + to_string(get_time()));    
    sem_wait(&vip_LR_people); // access to variable vip_lr_people_count
    vip_lr_people_count += 1; 
    if(vip_lr_people_count == 1) {
        sem_wait(&vip_RL_channel); // signal RL to be closed
        sem_wait(&vip_LR_channel); // signal LR to be closed - in use
    }
    sem_post(&vip_LR_people); // release access to variable
    
    print("Passenger " + p->get_id() + " is moving in VIP channel (L->R) at time " + to_string(get_time())); 
    sleep(vip_time);

    sem_wait(&vip_LR_people); // access to variable vip_lr_people_count
    print("Passenger " + p->get_id() + " has crossed VIP channel (L->R) at time " + to_string(get_time()));  
    vip_lr_people_count -= 1; 
    if(vip_lr_people_count == 0) {
        sem_post(&vip_LR_channel); // signal LR cross done
        sem_post(&vip_RL_channel); // signal RL is free for use
    }
    sem_post(&vip_LR_people); // release access to variable
}


void vip_right_left(Passenger* p){
    print("Passenger " + p->get_id() + " has started waiting for VIP channel (R->L) at time " + to_string(get_time()));
    
    sem_wait(&vip_RL_channel); // this gives LR priority over RL : can be stopped by LR any time.
    sem_wait(&vip_RL_people); // access to variable vip_rl_people_count
    vip_rl_people_count += 1; 
    if(vip_rl_people_count == 1) sem_wait(&vip_LR_channel); // LR cannot be accessed now, RL crossing will begin
    sem_post(&vip_RL_people); // release access to variable
    sem_post(&vip_RL_channel);

    print("Passenger " + p->get_id() + " is moving in VIP channel (R->L) at time " + to_string(get_time()));
    sleep(vip_time);

    // this automatically handles the case to wake up LR when current RLs are done if LR present
    sem_wait(&vip_RL_people); // access to variable vip_rl_people_count
    print("Passenger " + p->get_id() + " has crossed VIP channel (R->L) at time " + to_string(get_time())); 
    vip_rl_people_count -= 1; 
    if(vip_rl_people_count == 0) sem_post(&vip_LR_channel); // RL crossing done. signal LR is free now.
    sem_post(&vip_RL_people); // release access to variable
}


void board(Passenger *p){
    print("Passenger " + p->get_id() + " has started waiting to be boarded at time " + to_string(get_time()));
    sem_wait(&boarding); // enter kiosk only if there are kiosks left
    print("Passenger " + p->get_id() + " has started boarding the plane at time " + to_string(get_time()));
    sleep(boarding_time);
    p->pass = true; // got pass
    // make passenger lose pass 30% of the time
    if(((double) rand() / (RAND_MAX)) < 0.3){ 
        p->pass = false;
        p->vip_access = true; // passenger now has vip channel access 
        print("Passenger " + p->get_id() + " has lost his boarding pass at time " + to_string(get_time())); 
    }
    else print("Passenger " + p->get_id() + " has boarded the plane at time " + to_string(get_time()));
    sem_post(&boarding);
}


void* airport(void *p){
    Passenger* passenger = (Passenger *) p;

    // make passenger vip 50% of the time
    if(((double) rand() / (RAND_MAX)) < 0.5) { passenger->set_vip(); }

    print("Passenger " + passenger->get_id() + " has arrived at the airport at time " + to_string(get_time()));
    check_in_kiosk(passenger);

    while(true){
        if(passenger->vip_access){
            // use vip channel
            vip_left_right(passenger);
        }
        else{
            // security check
            // TODO : loop over and check which one is available, else randomly assign?
            security_check(passenger, rand () % no_belts); 
        }
        board(passenger);
        if(passenger->pass) break; // successful boarding
        vip_right_left(passenger);
        check_in_special_kiosk(passenger);

    }
    delete passenger;
}


//////////////// TEST CASES /////////////////////
void* vip_LR(void *p){
    Passenger* passenger = (Passenger *) p;
    vip_left_right(passenger);
}

void* vip_RL(void *p){
    Passenger* passenger = (Passenger *) p;
    vip_right_left(passenger);
}

void test1(){
    /* 
        passengers 1-4:
        2 LR at time 0 
        2 RL at time 1
        result : first 2 LR cross then 2 RL cross

        passengers 5-8:
        2 RL at time 4
        2 LR at time 5
        result : first 2 RL cross then 2 LR cross        
    */
    vip_time=2; 
    pthread_t threads[8];
    // test LR first
    int id=0;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    sleep(1);
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));

    sleep(3);
    
    // test RL first
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    sleep(1);
    id++;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
}

void test2(){
    /* 
        test LR priority:
        1 RL at time 0
        1 RL at time 1
        2 LR at time 2
        2 RL at time 3
        result : first 1,2 RL; 3,4 LR, 5,6 RL
    */

    vip_time=3; 
    pthread_t threads[6];
    // test LR first
    int id=0;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    id++;
    sleep(2);
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    sleep(1);
    id++;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    sleep(1);
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
}

void test3(){
    /* 
        test RL priority:
        1 LR at time 0
        1 LR at time 1
        2 RL at time 2
        2 LR at time 3
        result : first 1,2,5,6 LR, 3,4 RL
    */

    vip_time=3; 
    pthread_t threads[6];
    // test LR first
    int id=0;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    id++;
    sleep(2);
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    sleep(1);
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_RL, (void*) new Passenger(id+1));
    sleep(1);
    id++;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
    id++;
    pthread_create(&threads[id], NULL, vip_LR, (void*) new Passenger(id+1));
}


int main(){
    freopen("input.txt", "r", stdin);
    freopen("1705044_log.txt", "w", stdout);

    // initialize variables
    cin>>no_kiosk>>no_belts>>no_belt_passengers;
    cin>>kisok_time>>security_time>>boarding_time>>vip_time;

    // cout<<no_kiosk<<" "<<no_belts<<" "<<no_belt_passengers<<endl;
    // cout<<kisok_time<<" "<<security_time<<" "<<boarding_time<<" "<<vip_time<<endl;

    int i, j, sleep_time, number, id=1;
    start = chrono::steady_clock::now();
    belts = new sem_t[no_belts];
    // push available kisoks into queue
    for(i=1; i<=no_kiosk; i++) kiosk_q.push(i);
    
    // initialize semaphores
    sem_init(&kiosks, 0, no_kiosk);
    sem_init(&special_kiosk, 0, 1);
    for(i=0; i<no_belts; i++) sem_init(&belts[i], 0, no_belt_passengers);
    sem_init(&vip_LR_channel, 0, 1);
    sem_init(&vip_RL_channel, 0, 1);
    sem_init(&vip_LR_people, 0, 1);
    sem_init(&vip_RL_people, 0, 1);
    sem_init(&boarding, 0, 1);
    sem_init(&printing, 0, 1);

    // test3();

    // poisson distribution
    // for(i=0; i<2; i++){
    //     number = distribution(generator);
    //     for(j=0; j<number; j++){
    //         pthread_t thread;
    //         pthread_create(&thread, NULL, airport, (void*) new Passenger(id));
    //         id += 1;
    //     }
    //     sleep(2);
    // }

    // poisson process
    for(i=0; i<30; i++){
        sleep_time = distribution(generator);
        pthread_t thread;
        pthread_create(&thread, NULL, airport, (void*) new Passenger(id));
        id += 1;
        // print("SLEEP : " + to_string(sleep_time));
        sleep(sleep_time);
    }


    pthread_exit(NULL);
}