#include "account.hpp"

Account::Account(int account_id, string password, int initial_amount)
    : account_id(account_id), password(password), balance(initial_amount), read_count(0)
{
    if (pthread_mutex_init(&(this->read_lock_mutex), nullptr)) {
        perror("Bank error: pthread_mutex_init failed");
    }

    if (pthread_mutex_init(&(this->write_lock_mutex), nullptr)) {
        perror("Bank error: pthread_mutex_init failed");
    }
}

Account::~Account()
{
    if (pthread_mutex_destroy(&(this->read_lock_mutex))) {
        perror("Bank error: pthread_mutex_destroy failed");
    }

    if (pthread_mutex_destroy(&(this->write_lock_mutex))) {
        perror("Bank error: pthread_mutex_destroy failed");
    }
}

bool Account::operator<(const Account& other) const
{
    return account_id < other.account_id;
}

void Account::account_read_lock()
{
    if (pthread_mutex_lock(&(this->read_lock_mutex))) {
        perror("Bank error: pthread_mutex_lock failed");
        exit(1);
    }

    read_count++;

    if (read_count == 1) {
        if (pthread_mutex_lock(&(this->write_lock_mutex))) {
            perror("Bank error: pthread_mutex_lock failed");
            exit(1);
        }
    }

    if (pthread_mutex_unlock(&(this->read_lock_mutex))) {
        perror("Bank error: pthread_mutex_unlock failed");
        exit(1);
    }
}

void Account::account_read_unlock()
{
    if (pthread_mutex_lock(&(this->read_lock_mutex))) {
        perror("Bank error: pthread_mutex_lock failed");
        exit(1);
    }

    read_count--;

    if (read_count == 0) {
        if (pthread_mutex_unlock(&(this->write_lock_mutex))) {
            perror("Bank error: pthread_mutex_unlock failed");
            exit(1);
        }
    }

    if (pthread_mutex_unlock(&(this->read_lock_mutex))) {
        perror("Bank error: pthread_mutex_unlock failed");
        exit(1);
    }
}

void Account::account_write_lock()
{
    if (pthread_mutex_lock(&(this->write_lock_mutex))) {
        perror("Bank error: pthread_mutex_lock failed");
        exit(1);
    }
}

void Account::account_write_unlock()
{
    if (pthread_mutex_unlock(&(this->write_lock_mutex))) {
        perror("Bank error: pthread_mutex_unlock failed");
        exit(1);
    }
}

int Account::get_id()
{
    return this->account_id;
}

bool Account::check_password(string password)
{
    return this->password == password;
}

int Account::get_balance(string password, int* balance, int atm_id)
{
    account_read_lock();
    sleep(1);

    stringstream log_line;

    if (!check_password(password)) {
        log_line << "Error " << atm_id << ": Your transaction failed - password for account id " << this->account_id << " is incorrect";
        write_to_log_file(log_line.str());

        account_read_unlock();
        return WRONG_PASSWORD;
    }

    *balance = this->balance;

    log_line << atm_id << ": Account " << this->account_id << " balance is " << this->balance;
    write_to_log_file(log_line.str());

    account_read_unlock();
    return SUCCESS;
}

int Account::get_balance_no_print(string password, int* balance, int atm_id)
{
    account_read_lock();
    sleep(1);

    if (!check_password(password)) {
        account_read_unlock();
        return WRONG_PASSWORD;
    }

    *balance = this->balance;
    account_read_unlock();
    return SUCCESS;
}

int Account::deposit(int amount, string password, int* new_balance, int atm_id)
{
    account_write_lock();
    sleep(1);

    stringstream log_line;

    if (!check_password(password)) {
        log_line << "Error " << atm_id << ": Your transaction failed - password for account id " << this->account_id << " is incorrect";
        write_to_log_file(log_line.str());

        account_write_unlock();
        return WRONG_PASSWORD;
    }

    this->balance += amount;
    *new_balance = this->balance;

    log_line << atm_id << ": Account " << this->account_id << " new balance is " << this->balance << " after " << amount << " $ was deposited";
    write_to_log_file(log_line.str());

    account_write_unlock();
    return SUCCESS;
}

void Account::deposit_without_lock(int amount, int* new_balance)
{
    this->balance += amount;
    *new_balance = this->balance;
}

int Account::withdraw(int amount, string password, int* new_balance, int atm_id)
{
    account_write_lock();
    sleep(1);

    stringstream log_line;

    if (!check_password(password)) {
        log_line << "Error " << atm_id << ": Your transaction failed - password for account id " << this->account_id << " is incorrect";
        write_to_log_file(log_line.str());

        account_write_unlock();
        return WRONG_PASSWORD;
    }

    if (this->balance < amount) {
        *new_balance = this->balance;

        log_line << "Error " << atm_id << ": Your transaction failed - account id " << this->account_id << " balance is lower than " << amount;
        write_to_log_file(log_line.str());

        account_write_unlock();
        return NOT_ENOUGH_MONEY;
    }

    this->balance -= amount;
    *new_balance = this->balance;

    log_line << atm_id << ": Account " << this->account_id << " new balance is " << this->balance << " after " << amount << " $ was withdrew";
    write_to_log_file(log_line.str());

    account_write_unlock();
    return SUCCESS;
}

int Account::withdraw_without_lock(int amount, int* new_balance)
{
    if (this->balance < amount) {
        *new_balance = this->balance;
        return NOT_ENOUGH_MONEY;
    }

    this->balance -= amount;
    *new_balance = this->balance;

    return SUCCESS;
}

int Account::commission(int commission_percentage)
{
    account_write_lock();

    int commision = round(static_cast<double>(this->balance) * (static_cast<double>(commission_percentage) / 100));
    this->balance -= commision;

    stringstream log_line;
    log_line << "Bank: commissions of " << to_string(commission_percentage) << " % were charged, the bank gained " << to_string(commision) << " $ from account " << this->account_id;
    write_to_log_file(log_line.str());

    account_write_unlock();
    return commision;
}

string Account::print_status()
{
    account_read_lock();

    string line = "Account " + to_string(account_id)  + ": Balance - " + to_string(this->balance) + " $, Account Password - " + this->password;

    account_read_unlock();
    return line;
}

void write_to_log_file(string line)
{
    pthread_mutex_lock(&log_file_lock);
    if (log_file.is_open()) {
        log_file << line << endl;
    }
    pthread_mutex_unlock(&log_file_lock);
}