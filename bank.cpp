#include "bank.hpp"

pthread_mutex_t log_file_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
Bank* Bank::bank_instance = nullptr;

Bank::Bank() : bank_balance(0), read_count(0) {
    // Initializing mutexes for read and write locks
    if (pthread_mutex_init(&(this->read_lock_mutex), nullptr)) {
        perror("Bank error: pthread_mutex_init failed");
    }

    if (pthread_mutex_init(&(this->write_lock_mutex), nullptr)) {
        perror("Bank error: pthread_mutex_init failed");
    }
    pthread_mutex_init(&status_mutex, nullptr);
    pthread_mutex_init(&vip_queue_mutex, nullptr);
}

Bank::~Bank() {
    // Destroying mutexes for read and write locks
    if (pthread_mutex_destroy(&(this->read_lock_mutex))) {
        perror("Bank error: pthread_mutex_destroy failed");
    }

    if (pthread_mutex_destroy(&(this->write_lock_mutex))) {
        perror("Bank error: pthread_mutex_destroy failed");
    }
    pthread_mutex_destroy(&status_mutex);
    pthread_mutex_destroy(&vip_queue_mutex);
}

void Bank::bank_read_lock() {
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

void Bank::bank_read_unlock() {
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

void Bank::bank_write_lock() {
    if (pthread_mutex_lock(&(this->write_lock_mutex))) {
        perror("Bank error: pthread_mutex_lock failed");
        exit(1);
    }
}

void Bank::bank_write_unlock() {
    if (pthread_mutex_unlock(&(this->write_lock_mutex))) {
        perror("Bank error: pthread_mutex_unlock failed");
        exit(1);
    }
}

int Bank::create_account(int account_id, string password, int initial_amount, int atm_id) {
    bank_write_lock();
    stringstream log_line;

    if (is_account_exist(account_id) != ACCOUNT_NOT_EXIST) {
        sleep(1);
        log_line << "Error " << atm_id << ": Your transaction failed - account with the same id exists";
        write_to_log_file(log_line.str());

        bank_write_unlock();
        return ACCOUNT_EXIST;
    }

    Account a(account_id, password, initial_amount);
    this->accounts.push_back(a);
    sort(accounts.begin(), accounts.end());

    sleep(1);
    log_line << atm_id << ": New account id is " << account_id << " with password " << password << " and initial balance " << initial_amount;
    write_to_log_file(log_line.str());

    bank_write_unlock();
    return SUCCESS;
}

int Bank::delete_account(int account_id, string password, int* balance, int atm_id) {
    bank_write_lock();
    stringstream log_line;

    int index = is_account_exist(account_id);
    if (index == ACCOUNT_NOT_EXIST) {
        sleep(1);
        log_line << "Error " << atm_id << ": Your transaction failed - account id " << account_id << " does not exist";
        write_to_log_file(log_line.str());

        bank_write_unlock();
        return ACCOUNT_NOT_EXIST;
    }

    int status = accounts[index].get_balance_no_print(password, balance, atm_id);
    if (status == SUCCESS) {
        accounts.erase(accounts.begin() + index);
        log_line << atm_id << ": Account " << account_id << " is now closed. Balance was " << *balance;
        write_to_log_file(log_line.str());

        bank_write_unlock();
        return status;
    } else {
        log_line << "Error " << atm_id << ": Your transaction failed - password for account id " << account_id << " is incorrect";
        write_to_log_file(log_line.str());

        bank_write_unlock();
        return status;
    }
}

int Bank::deposit(int account_id, string password, int amount, int* new_balance, int atm_id) {
    bank_read_lock();
    int index = is_account_exist(account_id);

    if (index == ACCOUNT_NOT_EXIST) {
        sleep(1);
        stringstream log_line;
        log_line << "Error " << atm_id << ": Your transaction failed - account id " << account_id << " does not exist";
        write_to_log_file(log_line.str());

        bank_read_unlock();
        return ACCOUNT_NOT_EXIST;
    }

    int status = accounts[index].deposit(amount, password, new_balance, atm_id);

    bank_read_unlock();
    return status;
}

int Bank::withdraw(int account_id, string password, int amount, int* new_balance, int atm_id) {
    bank_read_lock();
    int index = is_account_exist(account_id);

    if (index == ACCOUNT_NOT_EXIST) {
        sleep(1);
        stringstream log_line;
        log_line << "Error " << atm_id << ": Your transaction failed - account id " << account_id << " does not exist";
        write_to_log_file(log_line.str());

        bank_read_unlock();
        return ACCOUNT_NOT_EXIST;
    }

    int status = accounts[index].withdraw(amount, password, new_balance, atm_id);

    bank_read_unlock();
    return status;
}

int Bank::get_balance(int account_id, string password, int* new_balance, int atm_id) {
    bank_read_lock();
    int index = is_account_exist(account_id);

    if (index == ACCOUNT_NOT_EXIST) {
        sleep(1);
        stringstream log_line;
        log_line << "Error " << atm_id << ": Your transaction failed - account id " << account_id << " does not exist";
        write_to_log_file(log_line.str());

        bank_read_unlock();
        return ACCOUNT_NOT_EXIST;
    }

    int status = accounts[index].get_balance(password, new_balance, atm_id);

    bank_read_unlock();
    return status;
}

int Bank::transfer_money(int account_id, string password, int target_account, int amount, int* new_balance, int* new_target_balance, int atm_id) {
    bank_read_lock();
    stringstream log_line;

    int index = is_account_exist(account_id);
    if (index == ACCOUNT_NOT_EXIST) {
        sleep(1);
        log_line << "Error " << atm_id << ": Your transaction failed - account id " << account_id << " does not exist";
        write_to_log_file(log_line.str());

        bank_read_unlock();
        return ACCOUNT_NOT_EXIST;
    }
    int target_index = is_account_exist(target_account);
    if (target_index == ACCOUNT_NOT_EXIST) {
        sleep(1);
        log_line << "Error " << atm_id << ": Your transaction failed - account id " << target_account << " does not exist";
        write_to_log_file(log_line.str());

        bank_read_unlock();
        return TARGET_ACCOUNT_NOT_EXIST;
    }

    if (!accounts[index].check_password(password)) {
        sleep(1);
        log_line << "Error " << atm_id << ": Your transaction failed - password for account id " << account_id << " is incorrect";
        write_to_log_file(log_line.str());

        bank_read_unlock();
        return WRONG_PASSWORD;
    }

    if (index < target_index) {
        accounts[index].account_write_lock();
        accounts[target_index].account_write_lock();
    }
    else {
        accounts[target_index].account_write_lock();
        accounts[index].account_write_lock();
    }

    sleep(1);
    int status = accounts[index].withdraw_without_lock(amount, new_balance);
    accounts[target_index].deposit_without_lock(amount, new_target_balance);

    if (status == NOT_ENOUGH_MONEY) {
        log_line << "Error " << atm_id << ": Your transaction failed - account id " << account_id << " balance is lower than " << amount;
    }
    else { // SUCCESS
        log_line << atm_id << ": Transfer " << amount << " from account " << account_id << " to account " << target_account << " new account balance is " << *new_balance << " new target account balance is " << *new_target_balance;
    }
    write_to_log_file(log_line.str());

    accounts[index].account_write_unlock();
    accounts[target_index].account_write_unlock();

    bank_read_unlock();
    return status;
}

void Bank::commission() {
    srand(static_cast<unsigned int>(time(nullptr)));
    int commission_percentage = (rand() % 5) + 1;

    bank_read_lock();

    for (unsigned i = 0; i < accounts.size(); i++) {
        int commission = accounts[i].commission(commission_percentage);
        this->bank_balance += commission;
    }

    bank_read_unlock();
}

void Bank::print_status() {
    bank_read_lock();
    string print_out;

    for (unsigned i = 0; i < accounts.size(); i++) {
        print_out.append(accounts[i].print_status());
        print_out.append("\n");
    }

    cout << "\033[2J";
    cout << "\033[1;1H";
    cout << "Current Bank Status" << endl;
    cout << print_out;

    bank_read_unlock();
}

int Bank::is_account_exist(int account_id) {
    for (unsigned i = 0; i < accounts.size(); i++) {
        if (accounts[i].get_id() == account_id) {
            return i;
        }
    }
    return ACCOUNT_NOT_EXIST;
}

bool Bank::close_atm(int source_atm_id, int target_atm_id) {
    sleep(1);
   if (target_atm_id > static_cast<int>(atm_files.size())) {
       log_file << "Error " << source_atm_id << ": Your transaction failed – ATM ID " << target_atm_id << " does not exist" << endl;
       return false;
   }

   if (atm_closed[target_atm_id - 1]) {
       log_file << "Error " << source_atm_id << ": Your close operation failed – ATM ID " << target_atm_id << " is already in a closed state" << endl;
       return false;
   }

   atm_closed[target_atm_id - 1] = true;
    log_file << "Bank: ATM " << source_atm_id << " closed " << target_atm_id << " successfully" << endl;
   return true;
}


void Bank::save_current_status() {
    pthread_mutex_lock(&status_mutex);

    Bank_Status current_status;
    current_status.accounts = accounts; // accounts הוא וקטור החשבונות

    statuses.push_back(current_status);

    if (statuses.size() > 120) {
        statuses.pop_front();
    }

    pthread_mutex_unlock(&status_mutex);
}

void Bank::rollback(int atm_id, int iterations) {
    sleep(1);
    pthread_mutex_lock(&status_mutex);

    if (static_cast<size_t>(iterations) > statuses.size()) {
        pthread_mutex_unlock(&status_mutex);
        return;
    }

    Bank_Status target_status = statuses[statuses.size() - iterations];
    accounts = target_status.accounts;

    pthread_mutex_unlock(&status_mutex);

    pthread_mutex_lock(&log_mutex);
    log_file << atm_id << ": Rollback to " << iterations << " bank iterations ago was completed successfully" << endl;
    pthread_mutex_unlock(&log_mutex);
}


void Bank::load_atms(std::string& atm_file_path) {
    ifstream atm_file(atm_file_path);
    string line;
    while (getline(atm_file, line)) {
        atm_files.push_back(line);
    }
}

void Bank::set_atm_file (const string file_name) {
    atm_files.push_back(file_name);
}

 void Bank::set_atm_closed_to_false(int size){
    for(int i =0; i<size; i++)
    atm_closed.push_back (false);
 }

void* Bank::vip_thread(void* arg) {
    while (true) {
        pthread_mutex_lock(&vip_queue_mutex);
        if (!vip_queue.empty()) {
            auto vip_command = vip_queue.back();
            vip_queue.pop_back();
            pthread_mutex_unlock(&vip_queue_mutex);

            // Process the VIP command
            // Use a function to execute the command (similar to the ATM threads)
            // e.g., execute_command(vip_command.first);
        } else {
            pthread_mutex_unlock(&vip_queue_mutex);
            usleep(500000);  // Sleep before checking the queue again
        }
    }
    pthread_exit(nullptr);
}

void* Bank::atm_thread(void* arg) {
    string operation_line = "some_operation"; // This should come from the ATM operations file
    bool is_persistent = false;
    int retry_count = 0;

    while (true) {
        if (is_persistent) {
            bool success = false;
            while (!success && retry_count < 2) {
                // Try to execute the operation
                // If failed, retry after waiting
                success = true;  // Assume the command succeeds here (add actual logic)
                if (!success) {
                    retry_count++;
                    usleep(100000);  // Wait before retrying
                }
            }
            if (!success) {
                cout << "Persistent command failed: " << operation_line << endl;
            }
        }

        // Process normal ATM operations
        // If command is VIP, add to VIP queue
        if (operation_line.find("VIP") != string::npos) {
            int priority = 90;  // Extract priority from operation_line, e.g., "VIP=90"
            pthread_mutex_lock(&vip_queue_mutex);
            vip_queue.push_back({operation_line, priority});
            sort(vip_queue.begin(), vip_queue.end(), [](std::pair<std::string, int>& a, std::pair<std::string, int>& b) {
               return a.second > b.second;
           });
            pthread_mutex_unlock(&vip_queue_mutex);
        }
    }
    pthread_exit(nullptr);
}


void Bank::process_command(const Command& cmd, pthread_t atm_id) {
    Command modified_cmd = cmd;
    modified_cmd.parse_command();
   
    if (modified_cmd.is_persistent) {
        process_persistent(modified_cmd, atm_id);
    } else if (modified_cmd.vip_priority != -1) {
        process_vip(modified_cmd, atm_id);
    } else {
        process_regular(modified_cmd, atm_id);
    }
}

void Command::parse_command() {
   if (command.find("VIP=") != string::npos) {
       size_t pos = command.find("VIP=") + 4;
       vip_priority = stoi(command.substr(pos));
       command = command.substr(0, pos - 4);
   }

   if (command.find("PERSISTENT") != string::npos) {
       is_persistent = true;
       command = command.substr(0, command.find("PERSISTENT") - 1);
   }
}


void Bank::process_persistent(const Command& cmd, pthread_t atm_id) {
   bool success = false; 
   int max_retries = 2;
   int retry_count = 0;
   while (!success && retry_count < max_retries) {
       process_regular(cmd, atm_id);
       retry_count++;
   }
}

void Bank::process_vip(const Command& cmd, pthread_t atm_id) {
    std::cout << "VIP command processing for " << cmd.get_command() << std::endl;
    process_regular(cmd, atm_id);
}

void Bank::process_regular(const Command& cmd, pthread_t atm_id) {
    std::cout << "Processing regular command: " << cmd.get_command() << std::endl;
    // return success
}

bool Bank::process_operation(const std::string& operation_line) {
    Command cmd(operation_line);
    bool success = false;

    if (cmd.has_persistent()) {
        process_persistent(cmd, pthread_self());
        success = true;
    } else if (cmd.is_vip()) {
        process_vip(cmd, pthread_self());
        success = true;
    } else {
        process_regular(cmd, pthread_self());
        success = true;
    }

    return success;
}

