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
#include <atomic>
#include <httplib.h>

using namespace functionslib;
using namespace std;
using namespace httplib;
using namespace CryptoPP;
using namespace chrono;

using json = nlohmann::json;

void handlerlib::getaccountid(sqlite3* db, const char* getaccountidsql, const httplib::Request& req, httplib::Response& resp) {
    string body = resp.body;
    sqlite3_stmt* stmt;
    auto j = json::parse(body);

    int rc = sqlite3_prepare_v2(db, getaccountidsql, -1, &stmt, nullptr);

    if (rc != SQLITE_DONE) {
        resp.set_content("error while preparing sql request", "text/plain");
        resp.status = 500;
        return;
    }
    string id = j["id"];
    string pwd = j["pwd"];

    sqlite3_bind_text(stmt, 1 , id.c_str(), -1, nullptr);
    sqlite3_bind_text(stmt, 2 , pwd.c_str(), -1, nullptr);

    int user_id;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    }

    string resultid = to_string(user_id);

    resp.set_content(resultid.c_str(), "");
};

void handlerlib::getrsakey(sqlite3* db, atomic<uint> age,const char* sqlrequest, const char* ssqlrequest, const httplib::Request& req, httplib::Response& res) {
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

        int request = sqlite3_prepare_v2(db, sqlrequest, -1, &stmt, nullptr);

        if (request != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            res.set_content("Db error, please retry later", "text/plain");
            res.status = 500;
            return;
        }

        sqlite3_bind_text(stmt, 1, pubkeypem.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, privkeypem.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, age.load());

        int newsqlrequest = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (newsqlrequest != SQLITE_DONE) {
            res.set_content("Db error, please retry later", "text/plain");
            res.status = 500;
            return;
        }
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

        uint fid = getendid(db, ssqlrequest);

        string result = to_string(fid);

        string responses = pubkeypem + " " + result;
        res.set_content(responses.c_str(), "text/plain");

}

void addtx(transactions, db, m, addtransaction, searchrsakeys, getsqlsold , const Request& req, httplib::Response& response) {
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
}
}