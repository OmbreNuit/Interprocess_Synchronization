#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <utility> // std::pair
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <pthread.h>
#include <mutex>

struct ArgStruct{
    int cpu_number;
    std::string input;
    int *turn;
    pthread_mutex_t *mutex;
    pthread_mutex_t *mutex2;
    pthread_cond_t *condition;
    
    ArgStruct(){}
};

//----------- entropy function proposed by Dr.Rincon | my code from pa2 --------
std::vector<double> calculate_entropy(std::string task, std::vector<int> cpuCount)
{
    // use of unordered_set to keep track of task char already seen before
    std::unordered_set<char> seen; 
    std::vector<char> unique;
    
    // removing duplicate Task char without changing the order
    for( char c: task) {
        if(seen.insert(c).second){
            unique.push_back(c);
        }
    }  

    // creating and initalizing FreqArray with no duplicate Tasks char and 0 frequency
    std::vector<std::pair<char, int>> NewFreqArray;  
    for(auto c: unique){
        NewFreqArray.push_back(std::make_pair(c,0));
    }

    std::vector<double> H;
    int FreqOfSelectedTask = 0;
    double h = 0.0;
    int currFreq = 0;
    double currH = 0.0;
    double currentTerm = 0;
    int size = task.size();

// --- entropy calculation -----
    for(int i = 0; i < size; i++){
        char selectedTask = task.at(i);
        int extraFreq = cpuCount[i];
        int NFreq = currFreq + extraFreq;

        if(NFreq == extraFreq){
            h = 0.00;
        }
        else{
            for(int i = 0; i < NewFreqArray.size(); i++){
                if(selectedTask == NewFreqArray.at(i).first){
                    FreqOfSelectedTask = NewFreqArray.at(i).second;
                }
            }
            if(FreqOfSelectedTask == 0){
                currentTerm = 0;
            }else{
                currentTerm = FreqOfSelectedTask * std::log2(FreqOfSelectedTask);
            }
            double newTerm = (FreqOfSelectedTask + extraFreq) * log2(FreqOfSelectedTask + extraFreq);
            if(NFreq != 0){
                h = log2(NFreq) - ((log2(currFreq) - currH) * currFreq - currentTerm + newTerm) / NFreq;
            }
            else{
                h = 0.00;
            }
        }
        
        //Updating the New Frequency array 
        for(int i = 0; i < NewFreqArray.size(); i++){
            if(NewFreqArray.at(i).first == selectedTask){
                NewFreqArray.at(i).second += extraFreq;
            }
        }
        
        //updating h and frequency variables
        currH = h;
        H.push_back(h); //adding the new entropy value to vector H
        currFreq = NFreq;
    }
    return H; // H is a vector<double> that has the entropy values
} 

// -------------------------- parse the input string | my code from pa2 --------
std::vector<double> parse(std::string inStr){
    
    std::vector<int> cpuCount; 
    std::istringstream iss(inStr); 
    std::string task;
    int count;
    std::string tasks = "";
    while (iss >> task >> count) {
        tasks += task;
        cpuCount.push_back(count);
    }

    // Call your entropy calculation function here
    std::vector<double> entropyValues = calculate_entropy(tasks, cpuCount);

    return entropyValues;
}

// -------------------- formating the output | my code from pa2 ----------------
std::string create_output(const std::vector<double>& entropyValues, std::string inputStr, int iterations) {
    std::stringstream output_sstream;
    std::stringstream input_sstream(inputStr);

    std::vector<std::pair<char, int>> taskInfo;
    char task;
    int count;

    while (input_sstream >> task >> count) {
        taskInfo.push_back(std::make_pair(task, count));
    }

    output_sstream << "CPU " << iterations << std::endl;

    output_sstream << "Task scheduling information: ";
    for(int i = 0; i < taskInfo.size()-1; i++){
        output_sstream << taskInfo[i].first << "(" << taskInfo[i].second << "), ";
    }
    output_sstream << taskInfo[taskInfo.size()-1].first << "(" << taskInfo[taskInfo.size()-1].second << ")";
    output_sstream << std::endl;

    output_sstream << "Entropy for CPU " << iterations << std::endl;
    for (double entropy : entropyValues) {
        output_sstream << std::fixed << std::setprecision(2) << entropy << " ";
    }
    output_sstream << std::endl;

    return output_sstream.str();
}

//------------------------- thread function ------------------------------
void *thread_function(void *arguments){
    ArgStruct *threadData = (ArgStruct*) arguments;
    
    // copy to local variables
    std::string in = threadData->input;
    int cpuNum = threadData->cpu_number;
    
    pthread_mutex_unlock(threadData->mutex);
    //---------------- End of Critical Section 1 -------------------------
    
    std::vector<double> entropyValues;
    entropyValues = parse(in); // call my entropyFunction
    
    //------------------- Critical Section 2 -----------------------------
    pthread_mutex_lock(threadData->mutex2);          
    while(cpuNum != (*threadData->turn)){
        pthread_cond_wait(threadData->condition, threadData->mutex2);
    }
    pthread_mutex_unlock(threadData->mutex2);        
    //-------------------End of Critical Section 2 -----------------------
    
    //                        Output 
    std::string output;
    output = create_output(entropyValues, in, cpuNum);
    std::cout << output << std::endl;
    
    //------------------- Critical Section 3 -----------------------------
    pthread_mutex_lock(threadData->mutex2);
    (*threadData->turn)++;
    pthread_cond_broadcast(threadData->condition);
    pthread_mutex_unlock(threadData->mutex2);
    //------------------- End of Critical Section 3 ----------------------
    return nullptr;
}

int main(){
    std::string input;
    std::vector<std::string> inputs;
    int nStrings = 0;
    
    //local mutex semaphores and conditional variables
    static pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, nullptr);
    static pthread_mutex_t mutex2;
    pthread_mutex_init(&mutex2, nullptr);
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    static int turn = 1;
    
    // read input 
    while(getline(std::cin, input)){
        inputs.push_back(input);
        nStrings++;
    }
    
    std::vector<pthread_t> threads(nStrings);
    
    ArgStruct args;
    args.turn = &turn;
    args.mutex = &mutex;
    args.mutex2 = &mutex2;
    args.condition = &condition;
    
     for (int i = 0; i < nStrings; i++){
        //------------------- Critical Section 1 -----------------------------
        pthread_mutex_lock(&mutex);
        args.input = inputs[i];
        args.cpu_number = i+1;
        
        pthread_create(&threads[i], nullptr, thread_function, &args);
    }

    for(int i = 0; i < nStrings; i++){
        pthread_join(threads[i], nullptr);
    }
    
    return 0;
}

