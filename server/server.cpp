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

using namespace std;
using namespace httplib;
using namespace CryptoPP;
using namespace chrono;

using json = nlohmann::json;

typedef struct {
    uint id;
    string ids;
    string idr;
    string amount;
    string age;
} tx;

atomic<bool> enbloc = false;
atomic<uint> age;

string rsa_decrypt_b64(const string& cipher_b64, RSA::PrivateKey& priv) {
    AutoSeededRandomPool rng;
    string cipher, plain;

    StringSource ss1(cipher_b64, true, new Base64Decoder(new StringSink(cipher)));

    RSAES_OAEP_SHA256_Decryptor dec(priv);
    StringSource ss2(cipher, true, new PK_DecryptorFilter(rng, dec, new StringSink(plain)));

    return plain;
}

int uncrypttx(tx* encryptedtx, uint keys_id, const char* sqlrequest, sqlite3* database) {
    sqlite3_stmt* stmt;
    int request = sqlite3_prepare_v2(database, sqlrequest, -1, &stmt, nullptr);

    if (request != SQLITE_DONE) {
        return 1;
    };

    sqlite3_bind_int(stmt, 1, keys_id);

    int exec = sqlite3_step(stmt);

    auto rawstring = sqlite3_column_text(stmt, 0);
    string privkey = rawstring ? reinterpret_cast<const char*>(rawstring) : "";
    privkey.erase(0, 28);
    privkey.erase(privkey.end()-26, privkey.end());
    
    string derprivkey;
    StringSource ss(privkey, true, new Base64Decoder(new StringSink(derprivkey)));

    RSA::PrivateKey privatekey;
    StringSource fprivatekey(derprivkey, true);
    privatekey.Load(fprivatekey);

    sqlite3_finalize(stmt);

    if (exec != SQLITE_OK) {
        return 1;
    }

    encryptedtx->ids = rsa_decrypt_b64(encryptedtx->ids, privatekey);
    encryptedtx->idr = rsa_decrypt_b64(encryptedtx->idr, privatekey);
    encryptedtx->amount = rsa_decrypt_b64(encryptedtx->amount, privatekey);
    encryptedtx->age = rsa_decrypt_b64(encryptedtx->age, privatekey);
    return 0;
}

uint getusersold(string id, sqlite3* db, const char* getsoldsql) {
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, getsoldsql, -1, &stmt, nullptr);

    if (rc != SQLITE_DONE) {
        return 0;
    }
    uint sold;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, nullptr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        sold += sqlite3_column_int(stmt, 3);
    }
    return sold;
}

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

uint getendid(sqlite3* database, const char* recherche) {
    sqlite3_stmt* stmt;
    int request = sqlite3_prepare_v2(database, recherche, -1, &stmt, nullptr);

    uint result;

    if (request != SQLITE_DONE) {
        return 1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return result;
}

int main() {
    sqlite3* db;

    const char* addtransaction = "INSERT INTO tx (ids, idr, amount, age) VALUES (?, ?, ?, ?)";
    const char* addrsakeys = "INSERT INTO rsakey (pubkey, privkey, age) VALUES (?, ?, ?)";
    const char* searchrsakeys = "SELECT privkey FROM rsakey WHERE (?) ";
    const char* getsqlsold = "SELECT amount FROM tx WHERE (?)";
    const char* getidlastrsakey = "SELECT id FROM rsakey ORDER BY id DESC LIMIT 1";
    const char* getaccountid = "SELECT "

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

    server.Post("/getaccountsold", [&](const Request& req, Response& resp) {
        string body = resp.body;
        auto j = json::parse(body);

    });

    server.Get("/getrsakey", [&](const Request& req, Response& response) {
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        AutoSeededRandomPool rng;

        RSA::PrivateKey privkey;
        privkey.GenerateRandomWithKeySize(rng, 2048);

        RSA::PublicKey pubKey(privkey);

        string pubkeypem;
        string privkeypem;

        ByteQueue queue;
        pubKey.Save(queue);

        ByteQueue pqueue;
        privkey.Save(pqueue);

        Base64Encoder encoder (new StringSink(pubkeypem), true, 64);
        queue.CopyTo(encoder);
        encoder.MessageEnd();

        Base64Encoder pencoder (new StringSink(privkeypem), true, 64);
        pqueue.CopyTo(pencoder);
        pencoder.MessageEnd();

        sqlite3_stmt* stmt;

        pubkeypem = "-----BEGIN PUBLIC KEY-----\n" + pubkeypem + "-----END PUBLIC KEY-----\n";
        privkeypem = "-----BEGIN PRIVATE KEY-----\n" + privkeypem + "-----END PRIVATE KEY-----\n";

        int request = sqlite3_prepare_v2(db, addrsakeys, -1, &stmt, nullptr);

        if (request != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            response.set_content("Db error, please retry later", "text/plain");
            response.status = 500;
            return;
        }

        sqlite3_bind_text(stmt, 1, pubkeypem.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, privkeypem.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, age.load());

        int newsqlrequest = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (newsqlrequest != SQLITE_DONE) {
            response.set_content("Db error, please retry later", "text/plain");
            response.status = 500;
            return;
        }
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

        uint fid = getendid(db, getidlastrsakey);

        string result = to_string(fid);

        string responses = pubkeypem + " " + result;
        response.set_content(responses.c_str(), "text/plain");
    });

    server.Put("/addtx", [&transactions, &db, &m, &addtransaction, &searchrsakeys, &getsqlsold ](const Request& req, Response& response) {
        string body = req.body;
        auto j = json::parse(body);
        uint sold = getusersold(j["ids"], db, getsqlsold);
        string stramount = j["amount"];
        uint amount = stoul(stramount);

        if (enbloc && sold > 0 && sold >= amount) {
            try {
                sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
                if (!j.contains("ids") || !j.contains("idr") || !j.contains("amount") || !j.contains("age")) {
                    response.set_content("missing fields/invalid json", "text/plain");
                    response.status = 400;
                    return;
                }

                tx transaction;
                uint id_tx = j["id"];
                transaction.ids = j["ids"];
                transaction.idr = j["idr"];
                transaction.amount = j["amount"];
                transaction.age = j["age"];

                tx* ptx = &transaction;

                int result = uncrypttx(ptx, id_tx, searchrsakeys, db);

                if (result == 1) {
                    response.set_content("False request, retry with an another request", "text/plain");
                    response.status = 400;
                }


                lock_guard<mutex> lock(m);
                transactions.push_back(transaction);

                sqlite3_stmt* stmt;
        
                int requestf = sqlite3_prepare_v2(db, addtransaction, -1, &stmt, nullptr);

                if (requestf != SQLITE_DONE) {
                    response.set_content("error while preparing sql request", "text/plain");
                    response.status = 400;
                    return;
                }

                sqlite3_bind_text(stmt, 1, (transaction.ids).c_str(), -1, nullptr);
                sqlite3_bind_text(stmt, 2, (transaction.idr).c_str(), -1, nullptr);
                sqlite3_bind_text(stmt, 3, (transaction.amount).c_str(), -1, nullptr);
                sqlite3_bind_text(stmt, 4, (transaction.age).c_str(), -1, nullptr);
        
                auto request = sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                if (request != SQLITE_DONE) {
                    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                    response.set_content("db error", "text/plain");
                    response.status = 500;
                    return;
                }

                sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
                response.set_content("tx added", "text/plain");
                response.status = 200;
                return;

            } catch (json::exception& e) {
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                response.status = 400;
                response.set_content("Json invalide/erreur de parsing", "text/plain");
                return;
            }
    } else  {
        response.set_content("bloc not avaible, please retry later", "text/plain");
        response.status = 403;
        return;
    }});
    
    cout <<"serveur actually rolling on 127.0.0.1:8080" << endl;
    server.listen("0.0.0.0", 8080);

    sqlite3_close(db);
    background.join();

    return 0;
}