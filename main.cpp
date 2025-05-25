 #include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <csignal>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <cstring>
#include "account.hpp"
#include "bank.hpp"

#define SUCCESS 0
#define ACCOUNT_EXIST -1
#define ACCOUNT_NOT_EXIST -2
#define WRONG_PASSWORD -3
#define NOT_ENOUGH_MONEY -4
#define TARGET_ACCOUNT_NOT_EXIST -5
#define MAX_RETRIES 2
#define RETRY_DELAY 1000000

using namespace std;

/* Global variables */
Bank* bank_instance;
ofstream log_file;
vector<string> atm_files;
bool program_termination_flag;

int completed_atms_count = 0;
pthread_mutex_t completed_atms_mutex = PTHREAD_MUTEX_INITIALIZER;

/* VIP Command Structure and Queue */
struct VipCommand {
    int vip_level;
    string command;

    // Comparator to ensure that commands with higher VIP levels come first in the queue
    bool operator<(const VipCommand& other) const {
        return vip_level < other.vip_level;
    }
};

priority_queue<VipCommand> vip_commands;
pthread_mutex_t vip_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/* --- Changes Begin: Track ATM threads to be closed --- */
vector<bool> atm_threads_to_close;  // To track which ATM threads need to be closed
pthread_mutex_t atm_threads_to_close_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t atm_mutex = PTHREAD_MUTEX_INITIALIZER;
/* --- Changes End --- */


/* Function Declarations for Threads */
void* atm_thread(void* arg);
void* status_printer_thread(void* arg);
void* commission_thread(void* arg);
void* vip_thread(void* arg);

/* Function to add a VIP command to the queue */
void add_vip_command(const string& command, int vip_level) {
    pthread_mutex_lock(&vip_queue_mutex);
    vip_commands.push(VipCommand{vip_level, command});
    pthread_mutex_unlock(&vip_queue_mutex);
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        cerr << "Bank error: illegal arguments" << endl;
        exit(1);
    }

    int num_vip_threads = 0;
    num_vip_threads = atoi(argv[1]);
    if (argc > 1) {
        num_vip_threads = stoi(argv[1]); // First argument is the number of VIP threads
    }

   
    // Create VIP threads
    pthread_t* vip_threads = new pthread_t[num_vip_threads];
    for (int i = 0; i < num_vip_threads; ++i) {
        if (pthread_create(&vip_threads[i], nullptr, vip_thread, nullptr)) {
            perror("Bank error: pthread_create failed for VIP thread");
            exit(1);
        }
    }

    // Initialize Bank instance
    bank_instance = new Bank();

    // Load ATM Files
    for (int i = 2; i < argc; ++i) {
        string file_name = argv[i];
        ifstream file(file_name);
        if (!file.is_open()) {
            cerr << "Bank error: illegal arguments" << endl;
            exit(1);
        }
        file.close();
        atm_files.push_back(file_name);
        bank_instance->set_atm_file(file_name);
    }
    int size = atm_files.size();
      bank_instance->set_atm_closed_to_false(size);

    // Open the log file for output
    log_file.open("log.txt");
    if (!log_file.is_open()) {
        cerr << "Bank error: unable to open log file" << endl;
        exit(1);
    }

   /* --- Changes Begin: Initialize tracking vectors --- */
    // Initialize vector to track ATM threads that need to be closed
    atm_threads_to_close.resize(atm_files.size(), false);
   
    /* --- Changes End --- */


    // Create threads for ATM operations
    pthread_t* atm_threads = new pthread_t[argc - 1];
    int* atm_thread_ids = new int[argc - 1];
    program_termination_flag = false;


    // Start ATM threads
   for (int i = 0; i < argc - 2; ++i) {
       atm_thread_ids[i] = i + 1;
       if (pthread_create(&atm_threads[i], nullptr, atm_thread, static_cast<void*>(&atm_thread_ids[i]))) {
           perror("Bank error: pthread_create failed for ATM thread");
           exit(1);
       }
   }

   // Start commission thread
   pthread_t commission_worker_thread;
   if (pthread_create(&commission_worker_thread, nullptr, commission_thread, nullptr)) {
       perror("Bank error: pthread_create failed for commission thread");
       exit(1);
   }

   // Start status printing thread
   pthread_t status_printer_worker_thread;
   if (pthread_create(&status_printer_worker_thread, nullptr, status_printer_thread, nullptr)) {
       perror("Bank error: pthread_create failed for status printer thread");
       exit(1);
   }

   // Ensure final status is printed after all ATM threads finish
   while (completed_atms_count < (argc - 2)) {
       usleep(100000); // Wait until all ATM threads finish
   }

   program_termination_flag = true;
   usleep(500000); // Wait a bit before final print to allow thread to run

   // Join all threads
   for (int i = 0; i < argc - 2; ++i) {
       int ret = pthread_join(atm_threads[i], nullptr);
       if (ret != 0) {
           cerr << "Bank error: pthread_join failed for ATM thread " << i << ": " << strerror(ret) << endl;
           exit(1);
       }
   }

   // Join commission and print threads
   if (pthread_join(commission_worker_thread, nullptr) || pthread_join(status_printer_worker_thread, nullptr)) {
       perror("Bank error: pthread_join failed for commission or print thread");
       exit(1);
   }

   // Join VIP threads after they finish
   for (int i = 0; i < num_vip_threads; ++i) {
       if (pthread_join(vip_threads[i], nullptr)) {
           perror("Bank error: pthread_join failed for VIP thread");
           exit(1);
       }
   }

   // Close the log file and clean up resources
   log_file.close();
   delete[] atm_threads;
   delete[] atm_thread_ids;
   delete[] vip_threads;
   delete bank_instance;

    return 0;
}

void* atm_thread(void* arg) {
   int atm_id = *(static_cast<int*>(arg));
   string atm_file_name = atm_files[atm_id - 1];

   ifstream atm_file(atm_file_name);
   if (!atm_file.is_open()) {
       cerr << "Bank error: failed to open ATM file" << endl;
       exit(1);
   }

   string operation_line;
   int target_atm_id = 0;
   int iterations = 0;


   while (getline(atm_file, operation_line)) {
       string account, password, amount, target_account;
       int new_balance, new_target_balance;
       stringstream log_file_line;
       vector<string> operation_words;
       bool thread_need_to_term = false;

       usleep(100000); // Simulate delay

       // Tokenize the operation line into words
       string word;
       for (char c : operation_line) {
           if (c != ' ' && c != '\n') {
               word += c;
           } else {
               operation_words.push_back(word);
               word.clear();
           }
       }
       if (!word.empty()) {
           operation_words.push_back(word);
       } else {
           continue;
       }

       // Check if the operation contains VIP or PERSISTENT
       bool is_vip = false;
       bool is_persistent = false;
       int vip_level = 0;

       for (const string& op_word : operation_words) {
           if (op_word.find("VIP=") != string::npos) {
               is_vip = true;
               size_t pos = op_word.find("VIP=");
               vip_level = stoi(op_word.substr(pos + 4));
           } else if (op_word == "PERSISTENT") {
               is_persistent = true;
           }
       }

       // If the operation is VIP, add it to the VIP queue
       if (is_vip) {
           add_vip_command(operation_line, vip_level);
           continue;  // Skip further processing of this command in the ATM thread
       }

       // Handle PERSISTENT commands within the ATM thread
       if (is_persistent) {
           bool success = false;
           int retry_count = 0;
           while (!success && retry_count < MAX_RETRIES) {
               success = bank_instance->process_operation(operation_line);  // Try to process the operation
               if (!success) {
                   ++retry_count;
                   usleep(RETRY_DELAY);  // Wait before retrying
               }
           }
           if (!success) {
               log_file << "Persistent command failed after " << retry_count << " retries: " << operation_line << endl;
           }
           continue;  // Skip further processing of this command in the ATM thread
       }

       // Process the operation (non-VIP, non-PERSISTENT)
       char operation = operation_words[0][0];
       switch (operation) {
           case 'O': // Open account
               account = operation_words[1];
               password = operation_words[2];
               amount = operation_words[3];
               bank_instance->create_account(stoi(account), password, stoi(amount), atm_id);
               break;

           case 'D': // Deposit
               account = operation_words[1];
               password = operation_words[2];
               amount = operation_words[3];
               bank_instance->deposit(stoi(account), password, stoi(amount), &new_balance, atm_id);
               break;

           case 'W': // Withdraw
               account = operation_words[1];
               password = operation_words[2];
               amount = operation_words[3];
               bank_instance->withdraw(stoi(account), password, stoi(amount), &new_balance, atm_id);
               break;

           case 'B': // Balance inquiry
               account = operation_words[1];
               password = operation_words[2];
               bank_instance->get_balance(stoi(account), password, &new_balance, atm_id);
               break;

           case 'Q': // Delete account
               account = operation_words[1];
               password = operation_words[2];
               bank_instance->delete_account(stoi(account), password, &new_balance, atm_id);
               break;

           case 'T': // Transfer money
               account = operation_words[1];
               password = operation_words[2];
               target_account = operation_words[3];
               amount = operation_words[4];
               bank_instance->transfer_money(stoi(account), password, stoi(target_account), stoi(amount), &new_balance, &new_target_balance, atm_id);
               break;

           case 'C': // Close ATM
               target_atm_id = stoi(operation_words[1]);
               if(bank_instance->close_atm(atm_id, target_atm_id)){ // tar_atm need to be closed
                   pthread_mutex_lock(&atm_threads_to_close_mutex);
                   atm_threads_to_close[target_atm_id - 1] = true;  // Mark ATM to close
                   pthread_mutex_unlock(&atm_threads_to_close_mutex);
               }
               break;

           case 'R': // Rollback
               iterations = stoi(operation_words[1]);
               bank_instance->rollback(atm_id, iterations);
               break;
       }

        pthread_mutex_lock(&atm_threads_to_close_mutex);
         if (atm_threads_to_close[atm_id - 1]) {
            thread_need_to_term = true;
         }
         pthread_mutex_unlock(&atm_threads_to_close_mutex);
         if(thread_need_to_term)
            break;  // Exit the loop if the thread is signaled to close
        


   }
   atm_file.close();
   pthread_mutex_lock(&completed_atms_mutex);
   completed_atms_count++;
   pthread_mutex_unlock(&completed_atms_mutex);
    
   pthread_exit(nullptr);
}

void* status_printer_thread(void* arg) {
    while (!program_termination_flag) {
        usleep(500000);
        bank_instance->print_status();
    }
    pthread_exit(nullptr);
}

void* commission_thread(void* arg) {
    while (!program_termination_flag) {
        sleep(3);
        bank_instance->commission();
    }
    pthread_exit(nullptr);
}

void* vip_thread(void* arg) {
    while (!program_termination_flag) {
        pthread_mutex_lock(&vip_queue_mutex);
        if (!vip_commands.empty()) {
            VipCommand vip_command = vip_commands.top();
            vip_commands.pop();
            pthread_mutex_unlock(&vip_queue_mutex);

        } else {
            pthread_mutex_unlock(&vip_queue_mutex);
            usleep(100000); // No command to process, sleep briefly
        }
    }
    pthread_exit(nullptr);
}
