 #ifndef BANK_H
#define BANK_H

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <unordered_map>
#include <utility>
#include <deque>
#include <vector>
#include "account.hpp"

#define SUCCESS 0
#define ACCOUNT_EXIST -1
#define ACCOUNT_NOT_EXIST -2
#define WRONG_PASSWORD -3
#define NOT_ENOUGH_MONEY -4
#define TARGET_ACCOUNT_NOT_EXIST -5

using namespace std;

class Command {
private:
   std::string command_line;
   bool persistent_flag = false;
   bool vip_flag = false; 
   
public:
   string command;
   int vip_priority;
   bool is_persistent; 

   //Command(string cmd, int vip = -1, bool persistent = false);
   bool operator<(const Command& other) const;
   void parse_command(); 
   std::string get_command() const{
	   return this->command;
   }


   Command(const std::string& operation_line) : command_line(operation_line) {}
  
   bool has_persistent() const {
       return command_line.find("PERSISTENT") != std::string::npos;
   }

   bool is_vip() const {
       return command_line.find("VIP") != std::string::npos;
   }

};

struct Bank_Status {
	std::vector<Account> accounts;
};

class Bank {
private:
    int bank_balance;
    vector<Account> accounts;
    pthread_mutex_t read_lock_mutex;
    pthread_mutex_t write_lock_mutex;
    int read_count;
    vector<string> atm_files;
    vector<bool> atm_closed;
    vector<Bank_Status> status_history;
    static Bank* bank_instance;
    std::deque<Bank_Status> statuses;
	pthread_mutex_t status_mutex;

    //std::vector<std::string> atm_files;
    std::vector<std::string> operations;
    pthread_mutex_t vip_queue_mutex;
    std::vector<std::pair<std::string, int>> vip_queue;

    // Readers-writers mechanism
    void bank_read_lock();
    void bank_read_unlock();
    void bank_write_lock();
    void bank_write_unlock();

public:
    Bank();
    ~Bank();

    void set_atm_file (const string file_name);// add file name to atm_files
    void set_atm_closed_to_false(int size);  
    int create_account(int account_id, string password, int initial_amount, int atm_id);
    int delete_account(int account_id, string password, int* balance, int atm_id);
    int deposit(int account_id, string password, int amount, int* new_balance, int atm_id);
    int withdraw(int account_id, string password, int amount, int* new_balance, int atm_id);
    int get_balance(int account_id, string password, int* new_balance, int atm_id);
    int transfer_money(int account_id, string password, int target_account, int amount, int* new_balance, int* new_target_balance, int atm_id);
    void commission();
    void print_status();
    int is_account_exist(int account_id);
    bool close_atm(int source_atm_id, int target_atm_id);
    void rollback(int atm_id, int iterations);
    void save_current_status();
    void load_atms(std::string& atm_file_path);
    void* atm_thread(void* arg);
    void* vip_thread(void* arg);
    void process_command(const Command& cmd, pthread_t atm_id);
    void process_persistent(const Command& cmd, pthread_t atm_id);
    void process_vip(const Command& cmd, pthread_t atm_id);
    void process_regular(const Command& cmd, pthread_t atm_id);
    bool process_operation(const std::string& operation_line);
};


#endif