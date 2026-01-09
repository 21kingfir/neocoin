#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <mutex>
#include <cryptopp/rsa.h>
#include <cryptopp/base64.h>
#include <cryptopp/queue.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <handlerlib/handlerlib.hpp>
#include <functionslib/functionslib.hpp>

using namespace std;
using namespace httplib;
using namespace CryptoPP;
using namespace chrono;
using namespace functionslib;
using namespace handlerlib;

using json = nlohmann::json;


atomic<bool> enbloc = false;
atomic<uint> age;

void getendbloc() {
    auto now = system_clock::now();
    auto starting_epoch = duration_cast<milliseconds>(now.time_since_epoch()).count();
    while(true) {
        auto now = system_clock::now();
        auto epoch = duration_cast<milliseconds>(now.time_since_epoch()).count();
        age = epoch - starting_epoch;
        enbloc = (age % 60 == 0);
        this_thread::sleep_for(seconds(1));
    }
}

int main() {
    sqlite3* db;

    const char* addtransaction = "INSERT INTO tx (ids, idr, amount, age) VALUES (?, ?, ?, ?)";
    const char* addrsakeys = "INSERT INTO rsakey (pubkey, privkey, age) VALUES (?, ?, ?)";
    const char* searchrsakeys = "SELECT privkey FROM rsakey WHERE (?) ";
    const char* getsqlsold = "SELECT amount FROM tx WHERE (?)";
    const char* getidlastrsakey = "SELECT id FROM rsakey ORDER BY id DESC LIMIT 1";
    const char* getaccountid = "SELECT id from users WHERE (identifier = ?, password = ?)";

    int rc = sqlite3_open("db/db.db", &db);

    if (rc != SQLITE_OK) {
        cerr << "Impossible d'ouvrir la DB: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

    const char* createtablesql =
    "CREATE TABLE IF NOT EXISTS tx ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "ids TEXT, "
    "idr TEXT, "
    "amount TEXT, "
    "age TEXT"
    ");";

    const char* createstablesql =
    "CREATE TABLE IF NOT EXISTS rsakey ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "pubkey INT, "
    "privkey INT, "
    "age INT "
    ");";

    const char* createuseridsql = 
    "CREATE TABLE IF NOT EXISTS users ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT; "
    "identifier TEXT , "
    "password TEXT "
    ");";

    char* errorMsg = nullptr;

    rc = sqlite3_exec(db, createtablesql, nullptr, nullptr, &errorMsg);


    if (rc != SQLITE_OK) {
        std::cerr << "Erreur SQL: " << errorMsg << std::endl;
        sqlite3_free(errorMsg);
    }

    rc = sqlite3_exec(db, createstablesql, nullptr, nullptr, &errorMsg);


    if (rc != SQLITE_OK) {
        std::cerr << "Erreur SQL: " << errorMsg << std::endl;
        sqlite3_free(errorMsg);
    }

    rc = sqlite3_exec(db, createuseridsql, nullptr, nullptr, &errorMsg);


    if (rc != SQLITE_OK) {
        std::cerr << "Erreur SQL: " << errorMsg << std::endl;
        sqlite3_free(errorMsg);
    }

    vector<tx> transactions;
    mutex m;

    thread background(getendbloc);

    httplib::Server server;

    server.Post("/getaccountid", [&db, &getaccountid](const Request& req, Response& resp) {
        handlerlib::getaccountid(db, getaccountid, req, resp);
    });

    server.Get("/getrsakey", [&db, &addrsakeys, &getidlastrsakey](const Request& req, Response& response) {
        
    });

    server.Put("/addtx", [&transactions, &db, &m, &addtransaction, &searchrsakeys, &getsqlsold ](const Request& req, Response& response) {
        addtx(transactions, db, m, addtransaction, searchrsakeys, getsqlsold, req, response);
    });
    
    cout <<"serveur actually rolling on 127.0.0.1:8080" << endl;
    server.listen("0.0.0.0", 8080);

    sqlite3_close(db);
    background.join();

    return 0;
}