#ifndef HANDLERLIB_HPP
#define HANDLERLIB_HPP

#include <httplib.h>
#include <string>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

typedef struct {
    uint id;
    string ids;
    string idr;
    string amount;
    string age;
} tx;


namespace handlerlib {

void getaccountid(sqlite3* db, const char* sqlrequest, const httplib::Request& req, httplib::Response& res);

void getrsakey(sqlite3* db, const char* sqlrequest, const char* ssqlrequest ,const httplib::Request& req, httplib::Response& res);

void addtx(vector<tx>transactions, sqlite3* db, mutex m, const char* addtransaction, const char* searchrsakeys, getsqlsold , const Request& req, httplib::Response& response);

}

#endif