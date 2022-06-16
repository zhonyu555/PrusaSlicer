#ifndef slic3r_InstanceSend_hpp_
#define slic3r_InstanceSend_hpp_

#include <boost/filesystem/path.hpp>
#include <wx/string.h>
#include <wx/event.h>

#include <boost/thread.hpp>
#include <mutex>
#include <condition_variable>


#if __linux__
#include <dbus/dbus.h> /* Pull in all of D-Bus headers. */
#endif //__linux__

namespace Downloader {
class SlicerSend
{
public:
    bool get_instance_exists() const;
    bool send_path(const wxString& path) const;
    bool start_with_path(const wxString& path) const;
    bool start_or_send(const wxString& path) const;
};

class DownloaderSend
{
public:
    bool get_instance_exists() const;
    bool send_url(const wxString& url) const;
};

#if __linux__
    #define BACKGROUND_DOWNLOADER_MESSAGE_LISTENER
#endif // __linux__

class OtherDownloaderMessageHandler
{
public:
    OtherDownloaderMessageHandler() = default;
    OtherDownloaderMessageHandler(OtherDownloaderMessageHandler const&) = delete;
    void operator=(OtherDownloaderMessageHandler const&) = delete;
    ~OtherDownloaderMessageHandler() { assert(!m_initialized); }

    // inits listening, on each platform different. On linux starts background thread
    void    init(wxEvtHandler* callback_evt_handler);
    // stops listening, on linux stops the background thread
    //void    shutdown(MainFrame* main_frame);
    void    shutdown();

    //finds paths to models in message(= command line arguments, first should be prusaSlicer executable)
    //and sends them to plater via LoadFromOtherInstanceEvent
    //security of messages: from message all existing paths are proccesed to load model 
    //                      win32 - anybody who has hwnd can send message.
    //                      mac - anybody who posts notification with name:@"OtherPrusaSlicerTerminating"
    //                      linux - instrospectable on dbus
    void           handle_message(const std::string& message);
#ifdef __APPLE__
    // Messege form other instance, that it deleted its lockfile - first instance to get it will create its own.
    //void           handle_message_other_closed();
#endif //__APPLE__
#ifdef _WIN32
    static void    init_windows_properties(MainFrame* main_frame, size_t instance_hash);
#endif //WIN32
private:
    bool                    m_initialized { false };
    wxEvtHandler*           m_callback_evt_handler { nullptr };

#ifdef BACKGROUND_DOWNLOADER_MESSAGE_LISTENER
    //worker thread to listen incoming dbus communication
    boost::thread           m_thread;
    std::condition_variable m_thread_stop_condition;
    mutable std::mutex      m_thread_stop_mutex;
    bool                    m_stop{ false };
    bool                    m_start{ true };
    
    // background thread method
    void    listen();
#endif //BACKGROUND_DOWNLOADER_MESSAGE_LISTENER

#if __APPLE__
    //implemented at InstanceCheckMac.mm
    //void    register_for_messages(const std::string &version_hash);
    //void    unregister_for_messages();
    // Opaque pointer to RemovableDriveManagerMM
    //void* m_impl_osx;
public: 
    //void    bring_instance_forward();
#endif //__APPLE__

};

wxDECLARE_EVENT(EVT_URL_MSG, wxCommandEvent);

}
#endif