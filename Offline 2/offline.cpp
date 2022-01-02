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
poisson_distribution<int> distribution(5);

string message;

// global variables defined by input
int no_kiosk;
int no_belts;
int no_belt_passengers;

int kisok_time;
int security_time;
int boarding_time;
int vip_time;

int vip_lr_people_count = 0;
int vip_rl_people_count = 0;

// define mutex and semaphores
sem_t kiosks;
sem_t special_kiosk;
sem_t* belts;
sem_t vip_channel;
sem_t vip_lr_people;
sem_t vip_rl_people;
sem_t boarding;

// keep track of time
chrono::steady_clock::time_point start;

inline int get_time(){
    return chrono::duration_cast<chrono::seconds> (chrono::steady_clock::now() - start).count();
} 

class Passenger{
public:
    int id;
    bool vip = false; // whether vip or not / has vip channel access
    bool pass = false; // whether got pass or not
    Passenger(int id){ this->id = id; }
};


void check_in_kiosk(Passenger* p){
    int kiosk_val;  
    sem_wait(&kiosks); // enter kiosk only if there are kiosks left
    sem_getvalue(&kiosks, &kiosk_val);
    // sleep(1);
    cout<<"Passenger "<<p->id<<" has started self-check in at kiosk "<<kiosk_val<<" at time "<<get_time()<<endl;
    sleep(kisok_time);
    p->pass = true; // got pass
    cout<<"Passenger "<<p->id<<" has finished check in at time "<<get_time()<<endl;
    sem_post(&kiosks);
}

void check_in_special_kiosk(Passenger* p){
    sem_wait(&special_kiosk); // enter kiosk only if there are kiosks left
    cout<<"Passenger "<<p->id<<" has started self-check in at special kiosk at time "<<get_time()<<endl;
    sleep(kisok_time);
    p->pass = true; // got pass
    cout<<"Passenger "<<p->id<<" has finished check in at time "<<get_time()<<endl;
    sem_post(&special_kiosk);
}


void security_check(Passenger* p, int belt_no){
    cout<<"Passenger "<<p->id<<" has started waiting for security check in belt "<<belt_no<<" at time "<<get_time()<<endl;
    sem_wait(&belts[belt_no]);
    cout<<"Passenger "<<p->id<<" has started the security check in belt "<<belt_no<<" at time "<<get_time()<<endl;
    sleep(security_time);
    cout<<"Passenger "<<p->id<<" has crossed security check at time "<<get_time()<<endl;
    sem_post(&belts[belt_no]);
}


void vip_left_right(Passenger* p){
    cout<<"Passenger "<<p->id<<" has started waiting for VIP channel (LR) at time "<<get_time()<<endl;    
    sem_wait(&vip_lr_people); // access to variable vip_lr_people_count
    vip_lr_people_count += 1; 
    if(vip_lr_people_count == 1) sem_wait(&vip_channel); // only 1st person waits for VIP channel to be free
    sem_post(&vip_lr_people); // release access to variable
    
    cout<<"Passenger "<<p->id<<" is moving in VIP channel (LR) at time "<<get_time()<<endl;    
    sleep(vip_time);

    sem_wait(&vip_lr_people); // access to variable vip_channel_people
    vip_lr_people_count -= 1; 
    if(vip_lr_people_count == 0) sem_post(&vip_channel); // signal VIP channel is free now
    sem_post(&vip_lr_people); // release access to variable
    cout<<"Passenger "<<p->id<<" has crossed VIP channel (LR) at time "<<get_time()<<endl;    
}


void vip_right_left(Passenger* p){
    cout<<"Passenger "<<p->id<<" has started waiting for VIP channel (RL) at time "<<get_time()<<endl;    
    sem_wait(&vip_rl_people); // access to variable vip_lr_people_count
    vip_rl_people_count += 1; 
    if(vip_rl_people_count == 1) sem_wait(&vip_channel); // only 1st person waits for VIP channel to be free
    else{
        // this block is inside vip_rl_people so next RL guy cannot enter
        sem_wait(&vip_lr_people);
        if(vip_lr_people_count>0) {
            sem_post(&vip_channel); // release VIP channel for left-right
            sem_wait(&vip_channel); // take back control once VIP channel is free again
        }
        sem_post(&vip_lr_people);
    }
    sem_post(&vip_rl_people); // release access to variable
    
    cout<<"Passenger "<<p->id<<" is moving in VIP channel (RL) at time "<<get_time()<<endl;    
    sleep(vip_time);

    sem_wait(&vip_rl_people); // access to variable vip_channel_people
    vip_rl_people_count -= 1; 
    if(vip_rl_people_count == 0) sem_post(&vip_channel); // signal VIP channel is free now
    sem_post(&vip_rl_people); // release access to variable
    cout<<"Passenger "<<p->id<<" has crossed VIP channel (RL) at time "<<get_time()<<endl;    
}


void board(Passenger *p){
    cout<<"Passenger "<<p->id<<" has started waiting to be boarded at time "<<get_time()<<endl;
    sem_wait(&boarding); // enter kiosk only if there are kiosks left
    cout<<"Passenger "<<p->id<<" has started boarding the plane at time "<<get_time()<<endl;
    sleep(boarding_time);
    p->pass = true; // got pass
    // make passenger lose pass 20% of the time
    if(((double) rand() / (RAND_MAX)) < 0.2){ 
        p->pass = false;
        p->vip = true; // passenger now has vip channel access 
        cout<<"Passenger "<<p->id<<" has lost his boarding pass at time "<<get_time()<<endl;   
    }
    else cout<<"Passenger "<<p->id<<" has board the plane at time "<<get_time()<<endl;
    sem_post(&boarding);
}

void* airport(void *p){
    Passenger* passenger = (Passenger *) p;

    // make passenger vip 20% of the time
    if(((double) rand() / (RAND_MAX)) < 0.2) { passenger->vip = true; }

    message = "Passenger " + to_string(passenger->id); 
    if(passenger->vip) message += " (VIP) ";
    message += "has arrived at the airport at time " + to_string(get_time());
    cout<<message<<endl;

    check_in_kiosk(passenger);


    while(true){
        if(passenger->vip){
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



int main(){
    // initialize variables
    no_kiosk = 10;
    no_belts = 2;
    no_belt_passengers = 1;

    kisok_time = 1;
    security_time = 2;
    boarding_time = 4;
    vip_time = 3;

    int i, j, number, id=1;
    start = chrono::steady_clock::now();
    belts = new sem_t[no_belts];
    
    // initialize semaphores
    sem_init(&kiosks, 0, no_kiosk);
    sem_init(&special_kiosk, 0, 1);
    for(i=0; i<no_belts; i++) sem_init(&belts[i], 0, no_belt_passengers);
    sem_init(&vip_channel, 0, 1);
    sem_init(&vip_lr_people, 0, 1);
    sem_init(&vip_rl_people, 0, 1);
    sem_init(&boarding, 0, 1);

    for(i=0; i<2; i++){
        number = distribution(generator);
        for(j=0; j<number; j++){
            pthread_t thread;
            pthread_create(&thread, NULL, airport, (void*) new Passenger(id));
            id += 1;
        }
        sleep(2);
    }

    pthread_exit(NULL);
}