#ifndef RIGOL_DRIVER_HPP
#define RIGOL_DRIVER_HPP

#include <string>
#include <stdexcept>
#include <fstream>
#include <memory>
#include <thread>
#include <chrono>

class SignalConnectionError : public std::runtime_error {
public:
    explicit SignalConnectionError(const std::string& msg) : std::runtime_error(msg) {}
};

class SignalParameterError : public std::runtime_error {
public:
    explicit SignalParameterError(const std::string& msg) : std::runtime_error(msg) {}
};

class RigolDriver {
public:
    explicit RigolDriver(const std::string& device_path = "/dev/usbtmc2");

    void setup_rabi(int gain, double freq);
    void shutdown_rabi();
    void set_freq(int gain, double freq);
    double get_freq();
    void setup_sweep(int gain, double start_freq, double stop_freq, double step_freq);
    void shutdown_sweep();
    void setup_sweep_for_imp_odmr(int gain, double start_freq, double stop_freq, double step_freq);
    void setup_sweep_test(int gain, double start_freq, double stop_freq, double step_freq); 
private:
    std::string device_path_;
    int fd_ = -1;

    void open_device();
    void close_device();
    void write_cmd(const std::string& command);
    std::string query_cmd(const std::string& command);
    void check_freq_range(double freq);
    void check_gain_range(int gain);
};

#endif