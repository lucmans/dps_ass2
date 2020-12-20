#ifndef __network_listener_h__
#define __network_listener_h__

class NetworkListener {
    public:
        NetworkListener(unsigned short port);
        ~NetworkListener();

        int wait();

    private:
        int m_fd;
};


#endif //#ifndef __network_listener_h__
