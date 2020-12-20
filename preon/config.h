#ifndef __config_h__
#define __config_h__

#include <string>

class Config {
    public:
        Config(const std::string &config_filename);

        const std::string  &get_tracker_url() const {return m_tracker_url;}
        unsigned short      get_tracker_port() const {return m_tracker_port;}
        unsigned short      get_listen_port() const {return m_listen_port;}
        const std::string  &get_download_folder() const {return m_download_folder;}

        unsigned            get_n_workers() const {return m_n_workers;}
        void                set_n_workers(unsigned n_workers) {m_n_workers = n_workers;}

    private:
        std::string     m_tracker_url;
        unsigned short  m_tracker_port;
        unsigned short  m_listen_port;
        std::string     m_download_folder;
        unsigned        m_n_workers;

        void load_file(const std::string &filename);
};

#endif //#ifndef __config_h__
