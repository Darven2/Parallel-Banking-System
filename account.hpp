 #ifndef ACCOUNT_H
#define ACCOUNT_H

#include <iostream>
#include <string>
#include <cmath>
#include <pthread.h>
#include <sstream>
#include <fstream>
#include <unistd.h>

#define SUCCESS 0
#define ACCOUNT_EXIST -1
#define ACCOUNT_NOT_EXIST -2
#define WRONG_PASSWORD -3
#define NOT_ENOUGH_MONEY -4
#define TARGET_ACCOUNT_NOT_EXIST -5

using namespace std;

/* Global Variables */
extern std::ofstream log_file;
extern pthread_mutex_t log_file_lock;

void write_to_log_file(string line);

class Account {
private:
    int account_id;
    string password;
    int balance;
    pthread_mutex_t read_lock_mutex;
    pthread_mutex_t write_lock_mutex;
    int read_count;

public:
    Account(int account_id, string password, int initial_amount);
    ~Account();

    bool operator<(const Account& other) const;

    // Readers-writers mechanism
    void account_read_lock();
    void account_read_unlock();
    void account_write_lock();
    void account_write_unlock();

    int get_id();
    bool check_password(string password);
    int get_balance(string password, int* balance, int atm_id);
    int get_balance_no_print(string password, int* balance, int atm_id);
    int deposit(int amount, string password, int* new_balance, int atm_id);
    void deposit_without_lock(int amount, int* new_balance);
    int withdraw(int amount, string password, int* new_balance, int atm_id);
    int withdraw_without_lock(int amount, int* new_balance);
    int commission(int commission_percentage);
    string print_status();
};

#endif
