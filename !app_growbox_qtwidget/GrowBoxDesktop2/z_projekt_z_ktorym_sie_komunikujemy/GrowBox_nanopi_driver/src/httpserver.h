// file: src/httpserver.h

#pragma once
#include <string>
#include "scheduler.h"

class LCD2004;

class HttpServer {
public:
    HttpServer(const std::string& bind_address,
               int port,
               const std::string& www_dir,
               Scheduler& scheduler,
               LCD2004& lcd);

    void run();

private:
    std::string bind_address_;
    int port_;
    std::string www_dir_;
    Scheduler& scheduler_;
    LCD2004& lcd_;
};

// end of file: src/httpserver.h