
#include <stdlib.h>
#include <fstream>
#include <nrpc/nrpc.h>

class NBoxSession : public nrpc::Session
{
public:
    bool connect(const char *role)
    {
        string path = getenv("TEMP");
        path += "\\VIM-NBOX.txt";

        ifstream fs(path);
        if (!(bool)fs) return false;

        string ser; fs >> ser;

        auto mid = ser.find(':');
        if (mid == string::npos)
            return false;

        auto addr = ser.substr(0, mid++);
        auto port = atoi(ser.substr(mid, ser.size() - mid).c_str());
        if (!this->attach(port))
            return false;

        auto api = api_info();
        // TODO: output as debug string
        cout << "Channel-ID: " << api[0].Int() << endl;
        cout << "ndbg#_start: " << fcall("ndbg#_start", role, api[0]) << endl;
        return true;
    }

    template<typename... Args>
    void print(Args... args)
    {
        //auto l = List::New({args...});
        this->fnotify("ndbg#print", args...);
    }

    void printf(const char *fmt, ...)
    {
        char buf[2000];

        va_list args;
        va_start(args, fmt);
        vsprintf(buf, fmt, args);
        this->print(buf);
    }
};

extern NBoxSession g_ses;