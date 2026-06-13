#include "RigolDriver.hpp"
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <chrono>
RigolDriver::RigolDriver(const std::string& device_path) : device_path_(device_path) {
    open_device();
}

void RigolDriver::open_device() {
    fd_ = open(device_path_.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw SignalConnectionError("Не удалось открыть " + device_path_ + 
                                    ". Проверьте права: sudo chmod 666 " + device_path_);
    }
}

void RigolDriver::close_device() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

void RigolDriver::write_cmd(const std::string& command) {
    std::string cmd = command + "\n";
    ssize_t written = write(fd_, cmd.c_str(), cmd.length());
    if (written < 0) {
        throw SignalConnectionError("Ошибка записи в устройство");
    }
}

std::string RigolDriver::query_cmd(const std::string& command) {
    write_cmd(command);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    char buffer[256] = {0};
    ssize_t n = read(fd_, buffer, sizeof(buffer) - 1);
    if (n < 0) {
        throw SignalConnectionError("Ошибка чтения из устройства");
    }
    return std::string(buffer, n);
}

void RigolDriver::check_freq_range(double freq) {
    if (freq < 9e3 || freq > 13.6e9) {
        throw SignalParameterError("Частота должна быть в диапазоне 9 kHz – 13.6 GHz");
    }
}

void RigolDriver::check_gain_range(int gain) {
    if (gain < -130 || gain > 27) {
        throw SignalParameterError("Усиление должно быть в диапазоне [-130, 27] дБм");
    }
}

void RigolDriver::setup_rabi(int gain, double freq) {
    check_gain_range(gain);
    check_freq_range(freq);
    write_cmd(":LEV " + std::to_string(gain) + "dBm");
    write_cmd(":FREQ " + std::to_string(freq));
    write_cmd(":OUTP 1");
    write_cmd(":MOD:STAT 1");
    write_cmd(":PULM:SOUR EXT");
    write_cmd(":PULM:STAT 1");
}

void RigolDriver::shutdown_rabi() {
    write_cmd(":OUTP 0");
    write_cmd(":MOD:STAT 0");
    write_cmd(":PULM:STAT 0");
}

void RigolDriver::set_freq(int gain, double freq) {
    check_gain_range(gain);
    check_freq_range(freq);
    write_cmd(":LEV " + std::to_string(gain) + "dBm");
    write_cmd(":FREQ " + std::to_string(freq));
    write_cmd(":OUTP 1");
}

double RigolDriver::get_freq() {
    std::string response = query_cmd(":FREQ?");
    return std::stod(response);
}

void RigolDriver::setup_sweep(int gain, double start_freq, double stop_freq, double step_freq) {
    check_gain_range(gain);
    check_freq_range(start_freq);
    check_freq_range(stop_freq);

    if (start_freq >= stop_freq) {
        throw SignalParameterError("Начальная частота должна быть меньше конечной");
    }
    if (step_freq <= 0) {
        throw SignalParameterError("Шаг частоты должен быть положительным");
    }

    int points = static_cast<int>(std::round((stop_freq - start_freq) / step_freq)) + 1;
    if (points > 65535) {
        throw SignalParameterError("Слишком много точек: " + std::to_string(points) + " (максимум 65535)");
    }

    write_cmd(":SWE:RES");
    write_cmd(":LEV " + std::to_string(gain) + "dBm");
    write_cmd(":SOUR1:FUNC:MODE SWE");
    write_cmd(":SWE:MODE CONT");
    write_cmd(":SWE:STEP:SHAP RAMP");
    write_cmd(":SWE:TYPE STEP");
    write_cmd(":SWE:STEP:POIN " + std::to_string(points));
    write_cmd(":SWE:STEP:STAR:FREQ " + std::to_string(start_freq));
    write_cmd(":SWE:STEP:STOP:FREQ " + std::to_string(stop_freq));
    write_cmd(":SWE:POIN:TRIG:TYPE EXT");
    write_cmd(":OUTP 1");
}

void RigolDriver::shutdown_sweep() {
    write_cmd(":OUTP 0");
}


void RigolDriver::setup_sweep_test(int gain, double start_freq, double stop_freq, double step_freq) {
    check_gain_range(gain);
    check_freq_range(start_freq);
    check_freq_range(stop_freq);

    if (start_freq >= stop_freq) {
        throw SignalParameterError("Начальная частота должна быть меньше конечной");
    }
    if (step_freq <= 0) {
        throw SignalParameterError("Шаг частоты должен быть положительным");
    }

    int points = static_cast<int>(std::round((stop_freq - start_freq) / step_freq)) + 1;
    if (points > 65535) {
        throw SignalParameterError("Слишком много точек: " + std::to_string(points) + " (максимум 65535)");
    }

    // Сброс sweep
    write_cmd(":SWE:RES");
    
    // Тип sweep и режим
    write_cmd(":SWE:TYPE STEP");           // Ступенчатый sweep
    write_cmd(":SWE:MODE CONT");           // Непрерывный режим (из доки: CONTinuous)
    
    // Направление и форма
    write_cmd(":SWE:DIR FWD");             // Прямое направление
    write_cmd(":SWE:STEP:SHAP RAMP");      // Форма рампы
    
    // Параметры частоты
    write_cmd(":SWE:STEP:STAR:FREQ " + std::to_string(start_freq));
    write_cmd(":SWE:STEP:STOP:FREQ " + std::to_string(stop_freq));
    write_cmd(":SWE:STEP:POIN " + std::to_string(points));
    
    // Уровень мощности
    write_cmd(":SWE:STEP:STAR:LEV " + std::to_string(gain) + "dBm");
    write_cmd(":SWE:STEP:STOP:LEV " + std::to_string(gain) + "dBm");
    
    // Шаг по частоте
    write_cmd(":SWE:STEP:SPAC LIN");       // Линейный шаг
    
    // Dwell time (из доки: 20ms - 100s)
    write_cmd(":SWE:STEP:DWEL 0.1");       // 100ms на точку
    
    // Триггер sweep (из доки: :SWE:SWE:TRIG:TYPE)
    write_cmd(":SWE:SWE:TRIG:TYPE AUTO");  // Авто триггер периода sweep
    
    // Триггер точки (из доки: :SWE:POIN:TRIG:TYPE)
    write_cmd(":SWE:POIN:TRIG:TYPE AUTO"); // Авто триггер каждой точки
    
    // Включаем sweep (из доки: :SWE:STAT FREQ)
    write_cmd(":SWE:STAT FREQ");           // Включаем частотный sweep
    
    // Включаем выход
    write_cmd(":OUTP ON");
}
void RigolDriver::setup_sweep_for_imp_odmr(int gain, double start_freq, double stop_freq, double step_freq) {
    check_gain_range(gain);
    check_freq_range(start_freq);
    check_freq_range(stop_freq);

    if (start_freq >= stop_freq) {
        throw SignalParameterError("Начальная частота должна быть меньше конечной");
    }
    if (step_freq <= 0) {
        throw SignalParameterError("Шаг частоты должен быть положительным");
    }

    int points = static_cast<int>(std::round((stop_freq - start_freq) / step_freq)) + 1;
    if (points > 65535) {
        throw SignalParameterError("Слишком много точек: " + std::to_string(points) + " (максимум 65535)");
    }

    // Только параметры sweep, без переключения режимов
    write_cmd(":SWE:TYPE STEP");
    write_cmd(":SWE:MODE CONT");
    write_cmd(":SWE:STEP:SHAP RAMP");
    write_cmd(":SWE:STEP:STAR:FREQ " + std::to_string(start_freq));
    write_cmd(":SWE:STEP:STOP:FREQ " + std::to_string(stop_freq));
    write_cmd(":SWE:STEP:POIN " + std::to_string(points));
    write_cmd(":SWE:POIN:TRIG:TYPE EXT");
    write_cmd(":MOD:STAT 1");
    write_cmd(":PULM:SOUR EXT");
    write_cmd(":PULM:STAT 1");
    write_cmd(":LEV " + std::to_string(gain) + "dBm");
    write_cmd(":OUTP ON");
}